#include <stdint.h>
#include "peloader.h"
#include "mm.h"
#include "util.h"
#include "serial.h"
#include "cdfs.h"

// DLL list
static LOADED_DLL *dll_list = 0;

static void *PeGetELFProcAddress(void *elf_base, const char *func_name);

// ELF structures
#define ELF_MAGIC 0x464C457F

typedef struct {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) ELF32_HEADER;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed)) ELF32_PHDR;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
} __attribute__((packed)) ELF32_SHDR;

typedef struct {
    uint32_t name;
    uint32_t value;
    uint32_t size;
    uint8_t  info;
    uint8_t  other;
    uint16_t shndx;
} __attribute__((packed)) ELF32_SYM;

// ELF loader
static void *PeLoadELF(void *image_data, uint32_t size) {
    ELF32_HEADER *elf = (ELF32_HEADER*)image_data;
    
    SerialPutString("[ELF] Type=");
    SerialPrintDec(elf->type);
    SerialPutString(" Machine=");
    SerialPrintDec(elf->machine);
    SerialPutString(" Entry=0x");
    SerialPrintHex(elf->entry);
    SerialPutString("\r\n");
    
    // Must be 32-bit x86 shared object or executable
    if (elf->ident[4] != 1) {
        SerialPutString("[ELF] FAIL: not 32-bit (class=");
        SerialPrintDec(elf->ident[4]);
        SerialPutString(")\r\n");
        return 0;
    }
    
    if (elf->machine != 3) {
        SerialPutString("[ELF] FAIL: not x86 (machine=");
        SerialPrintDec(elf->machine);
        SerialPutString(")\r\n");
        return 0;
    }
    
    // Calculate total memory needed
    uint32_t mem_end = 0;
    uint32_t base = 0xFFFFFFFF;
    
    ELF32_PHDR *ph = (ELF32_PHDR*)((uint8_t*)image_data + elf->phoff);
    
    for (int i = 0; i < elf->phnum; i++) {
        if (ph[i].type == 1) { // PT_LOAD
            uint32_t end = ph[i].vaddr + ph[i].memsz;
            if (end > mem_end) mem_end = end;
            if (ph[i].vaddr < base) base = ph[i].vaddr;
        }
    }
    
    SerialPutString("[ELF] Base=0x");
    SerialPrintHex(base);
    SerialPutString(" End=0x");
    SerialPrintHex(mem_end);
    SerialPutString("\r\n");
    
    if (mem_end == 0 || mem_end > 0x1000000) {
        SerialPutString("[ELF] FAIL: bad size\r\n");
        return 0;
    }
    
    uint8_t *image_base = (uint8_t*)kmalloc(mem_end);
    if (!image_base) {
        SerialPutString("[ELF] FAIL: OOM\r\n");
        return 0;
    }
    
    memset(image_base, 0, mem_end);
    
    // Load program headers
    for (int i = 0; i < elf->phnum; i++) {
        if (ph[i].type == 1 && ph[i].filesz > 0) {
            uint32_t dest = ph[i].vaddr;
            uint32_t src = ph[i].offset;
            uint32_t len = ph[i].filesz;
            
            if (src + len <= size && dest + len <= mem_end) {
                memcpy(image_base + dest, (uint8_t*)image_data + src, len);
            }
        }
    }
    
    // Apply relocations from REL sections
    ELF32_SHDR *sh = (ELF32_SHDR*)((uint8_t*)image_data + elf->shoff);
    uint8_t *shstrtab = (uint8_t*)image_data + sh[elf->shstrndx].offset;
    
    for (int i = 0; i < elf->shnum; i++) {
        const char *name = (const char*)(shstrtab + sh[i].name);
        
        if (sh[i].type == 9 && strcmp(name, ".rel.dyn") == 0) {
            // Found relocation section
            uint32_t *rel_data = (uint32_t*)((uint8_t*)image_data + sh[i].offset);
            int count = sh[i].size / 8;
            
            SerialPutString("[ELF] Applying ");
            SerialPrintDec(count);
            SerialPutString(" relocations...\r\n");
            
            for (int j = 0; j < count; j++) {
                uint32_t r_offset = rel_data[j * 2];
                uint32_t r_info = rel_data[j * 2 + 1];
                uint8_t r_type = r_info & 0xFF;
                
                if (r_type == 7) { // R_386_JMP_SLOT or R_386_RELATIVE
                    uint32_t *patch = (uint32_t*)(image_base + r_offset);
                    *patch += (uint32_t)image_base;
                }
            }
        }
    }
    
    SerialPutString("[ELF] Loaded at 0x");
    SerialPrintHex((uint32_t)image_base);
    SerialPutString("\r\n");
    
    return image_base;
}

void *PeLoadImage(void *image_data, uint32_t size) {
    SerialPutString("[PELoad] size=");
    SerialPrintDec(size);
    SerialPutString("\r\n");
    
    if (size < 64) {
        SerialPutString("[PELoad] FAIL: too small\r\n");
        return 0;
    }
    
    // Check for ELF magic first
    uint32_t magic = *(uint32_t*)image_data;
    if (magic == 0x464C457F) {
        SerialPutString("[PELoad] ELF file detected, using ELF loader\r\n");
        return PeLoadELF(image_data, size);
    }
    
    // PE loading
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_data;
    
    SerialPutString("[PELoad] MZ=0x");
    SerialPrintHex(dos->e_magic);
    SerialPutString("\r\n");
    
    if (dos->e_magic != 0x5A4D) {
        SerialPutString("[PELoad] FAIL: bad MZ (0x");
        SerialPrintHex(dos->e_magic);
        SerialPutString("), not PE or ELF\r\n");
        uint8_t *bytes = (uint8_t*)image_data;
        SerialPutString("[PELoad] First bytes: ");
        for (int i = 0; i < 16; i++) {
            SerialPrintHex(bytes[i]);
            SerialPutString(" ");
        }
        SerialPutString("\r\n");
        return 0;
    }
    
    uint32_t pe_off = dos->e_lfanew;
    SerialPutString("[PELoad] PE offset=0x");
    SerialPrintHex(pe_off);
    SerialPutString("\r\n");
    
    if (pe_off + 4 > size) {
        SerialPutString("[PELoad] FAIL: PE offset OOB\r\n");
        return 0;
    }
    
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_data + pe_off);
    
    SerialPutString("[PELoad] PE sig=0x");
    SerialPrintHex(file->Signature);
    SerialPutString(" (need 0x4550)\r\n");
    
    if (file->Signature != 0x00004550) {
        SerialPutString("[PELoad] FAIL: bad PE sig\r\n");
        return 0;
    }
    
    SerialPutString("[PELoad] Machine=0x");
    SerialPrintHex(file->Machine);
    SerialPutString(" (0x14C=i386)\r\n");
    
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    
    SerialPutString("[PELoad] OptMagic=0x");
    SerialPrintHex(opt->Magic);
    SerialPutString(" (0x10B=PE32)\r\n");
    
    if (opt->Magic != 0x10B) {
        SerialPutString("[PELoad] FAIL: not PE32\r\n");
        return 0;
    }
    
    uint32_t image_size = opt->SizeOfImage;
    SerialPutString("[PELoad] ImageSize=0x");
    SerialPrintHex(image_size);
    SerialPutString("\r\n");
    
    if (image_size == 0 || image_size > 0x1000000) {
        SerialPutString("[PELoad] FAIL: bad image size\r\n");
        return 0;
    }
    
    uint8_t *image_base = (uint8_t*)kmalloc(image_size);
    if (!image_base) {
        SerialPutString("[PELoad] FAIL: OOM\r\n");
        return 0;
    }
    
    memset(image_base, 0, image_size);
    
    uint32_t headers_size = opt->SizeOfHeaders;
    if (headers_size > size) headers_size = size;
    memcpy(image_base, image_data, headers_size);
    
    IMAGE_SECTION_HEADER *sections = (IMAGE_SECTION_HEADER*)((uint8_t*)opt + file->SizeOfOptionalHeader);
    
    SerialPutString("[PELoad] Sections=");
    SerialPrintDec(file->NumberOfSections);
    SerialPutString("\r\n");
    
    for (int i = 0; i < file->NumberOfSections; i++) {
        if (sections[i].SizeOfRawData > 0) {
            uint32_t dest = sections[i].VirtualAddress;
            uint32_t src = sections[i].PointerToRawData;
            uint32_t len = sections[i].SizeOfRawData;
            
            SerialPutString("[PELoad] Sec");
            SerialPrintDec(i);
            SerialPutString(": VA=0x");
            SerialPrintHex(dest);
            SerialPutString(" Raw=0x");
            SerialPrintHex(src);
            SerialPutString(" Size=0x");
            SerialPrintHex(len);
            SerialPutString("\r\n");
            
            if (src + len <= size && dest + len <= image_size) {
                memcpy(image_base + dest, (uint8_t*)image_data + src, len);
                SerialPutString("[PELoad]   -> Copied\r\n");
            } else {
                SerialPutString("[PELoad]   -> SKIP bounds (src+len=");
                SerialPrintHex(src + len);
                SerialPutString(" size=");
                SerialPrintHex(size);
                SerialPutString(" dest+len=");
                SerialPrintHex(dest + len);
                SerialPutString(" image=");
                SerialPrintHex(image_size);
                SerialPutString(")\r\n");
            }
        }
    }
    
    SerialPutString("[PELoad] OK, returning 0x");
    SerialPrintHex((uint32_t)image_base);
    SerialPutString("\r\n");
    
    return image_base;
}

void *PeGetEntryPoint(void *image_base) {
    if (!image_base) return 0;
    
    // Check for ELF
    if (*(uint32_t*)image_base == 0x464C457F) {
        ELF32_HEADER *elf = (ELF32_HEADER*)image_base;
        if (elf->entry != 0) {
            return (uint8_t*)image_base + elf->entry;
        }
        return 0;
    }
    
    // PE
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    if (dos->e_magic != 0x5A4D) return 0;
    
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    return (uint8_t*)image_base + opt->AddressOfEntryPoint;
}

void PePerformRelocations(void *image_base) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    
    int32_t delta = (int32_t)((uint32_t)image_base - opt->ImageBase);
    if (delta == 0) return;
    
    if (opt->NumberOfRvaAndSizes <= 5) return;
    
    uint32_t *data_dir = (uint32_t*)((uint8_t*)opt + 96);
    uint32_t reloc_rva = data_dir[10];
    uint32_t reloc_size = data_dir[11];
    
    if (reloc_rva == 0 || reloc_size == 0) return;
    
    uint8_t *reloc = (uint8_t*)image_base + reloc_rva;
    uint8_t *reloc_end = reloc + reloc_size;
    
    while (reloc < reloc_end) {
        uint32_t page_rva = *(uint32_t*)reloc;
        uint32_t block_size = *(uint32_t*)(reloc + 4);
        if (block_size == 0) break;
        
        uint16_t *entries = (uint16_t*)(reloc + 8);
        int count = (block_size - 8) / 2;
        
        for (int i = 0; i < count; i++) {
            if ((entries[i] >> 12) == 3) {
                uint32_t *patch = (uint32_t*)((uint8_t*)image_base + page_rva + (entries[i] & 0xFFF));
                *patch = (uint32_t)((int32_t)(*patch) + delta);
            }
        }
        reloc += block_size;
    }
}

int PeResolveImports(void *image_base) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    
    if (opt->NumberOfRvaAndSizes < 2) return 1;
    
    uint32_t *data_dir = (uint32_t*)((uint8_t*)opt + 96);
    uint32_t import_rva = data_dir[2];
    
    if (import_rva == 0) return 1;
    
    IMAGE_IMPORT_DESCRIPTOR *import = (IMAGE_IMPORT_DESCRIPTOR*)((uint8_t*)image_base + import_rva);
    int missing = 0;
    
    while (import->Name != 0) {
        const char *dll_name = (const char*)((uint8_t*)image_base + import->Name);
        
        SerialPutString("[PE] Import: ");
        SerialPutString(dll_name);
        SerialPutString("\r\n");
        
        // Try to load the DLL
        void *dll_base = PeLoadDll(dll_name);
        
        uint32_t *lookup = (uint32_t*)((uint8_t*)image_base + import->ImportLookupTable);
        if (import->ImportLookupTable == 0)
            lookup = (uint32_t*)((uint8_t*)image_base + import->ImportAddressTable);
        
        uint32_t *iat = (uint32_t*)((uint8_t*)image_base + import->ImportAddressTable);
        
        while (*lookup != 0) {
            const char *func_name = 0;
            
            if (*lookup & 0x80000000) {
                SerialPutString("  Ord: ");
                SerialPrintDec(*lookup & 0xFFFF);
                SerialPutString("\r\n");
            } else {
                uint16_t *hint = (uint16_t*)((uint8_t*)image_base + (*lookup & 0x7FFFFFFF));
                func_name = (const char*)((uint8_t*)hint + 2);
            }
            
            if (func_name && dll_base) {
                void *addr = PeGetProcAddress(dll_base, func_name);
                if (addr) {
                    *iat = (uint32_t)addr;
                    SerialPutString("  ");
                    SerialPutString(func_name);
                    SerialPutString(" -> OK\r\n");
                } else {
                    missing++;
                    *iat = 0;
                    SerialPutString("  ");
                    SerialPutString(func_name);
                    SerialPutString(" -> MISSING\r\n");
                }
            } else if (func_name) {
                missing++;
                *iat = 0;
                SerialPutString("  ");
                SerialPutString(func_name);
                SerialPutString(" -> NO DLL\r\n");
            }
            
            lookup++;
            iat++;
        }
        import++;
    }
    
    return (missing == 0) ? 1 : 0;
}

void *PeGetProcAddress(void *dll_base, const char *func_name) {
    if (!dll_base || !func_name) return 0;
    
    // Check if it's an ELF DLL
    if (*(uint32_t*)dll_base == 0x464C457F) {
        return PeGetELFProcAddress(dll_base, func_name);
    }
    
    // PE DLL
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)dll_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)dll_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    
    if (opt->NumberOfRvaAndSizes < 1) return 0;
    
    uint32_t *data_dir = (uint32_t*)((uint8_t*)opt + 96);
    uint32_t export_rva = data_dir[0];
    if (export_rva == 0) return 0;
    
    IMAGE_EXPORT_DIRECTORY *exp = (IMAGE_EXPORT_DIRECTORY*)((uint8_t*)dll_base + export_rva);
    
    uint32_t *functions = (uint32_t*)((uint8_t*)dll_base + exp->AddressOfFunctions);
    uint32_t *names = (uint32_t*)((uint8_t*)dll_base + exp->AddressOfNames);
    uint16_t *ordinals = (uint16_t*)((uint8_t*)dll_base + exp->AddressOfNameOrdinals);
    
    for (uint32_t i = 0; i < exp->NumberOfNames; i++) {
        const char *name = (const char*)((uint8_t*)dll_base + names[i]);
        if (strcmp(name, func_name) == 0) {
            return (uint8_t*)dll_base + functions[ordinals[i]];
        }
    }
    
    return 0;
}

// Find a function in an ELF DLL
static void *PeGetELFProcAddress(void *elf_base, const char *func_name) {
    ELF32_HEADER *elf = (ELF32_HEADER*)elf_base;
    
    // Get section headers
    ELF32_SHDR *sh = (ELF32_SHDR*)((uint8_t*)elf_base + elf->shoff);
    uint8_t *shstr = (uint8_t*)elf_base + sh[elf->shstrndx].offset;
    
    ELF32_SYM *dynsym = 0;
    uint32_t dynsym_count = 0;
    const char *dynstr = 0;
    
    // Find .dynsym and .dynstr
    for (int i = 0; i < elf->shnum; i++) {
        const char *name = (const char*)(shstr + sh[i].name);
        if (strcmp(name, ".dynsym") == 0 && sh[i].addr != 0) {
            dynsym = (ELF32_SYM*)((uint8_t*)elf_base + sh[i].addr);
            dynsym_count = sh[i].size / sizeof(ELF32_SYM);
        }
        if (strcmp(name, ".dynstr") == 0 && sh[i].addr != 0) {
            dynstr = (const char*)((uint8_t*)elf_base + sh[i].addr);
        }
    }
    
    if (!dynsym || !dynstr) {
        SerialPutString("[ELF] No dynamic symbols found\r\n");
        return 0;
    }
    
    // Search for the symbol
    for (uint32_t i = 0; i < dynsym_count; i++) {
        const char *name = dynstr + dynsym[i].name;
        if (strcmp(name, func_name) == 0 && dynsym[i].value != 0) {
            SerialPutString("[ELF] Found ");
            SerialPutString(func_name);
            SerialPutString(" at 0x");
            SerialPrintHex(dynsym[i].value);
            SerialPutString("\r\n");
            return (uint8_t*)elf_base + dynsym[i].value;
        }
    }
    
    SerialPutString("[ELF] Symbol not found: ");
    SerialPutString(func_name);
    SerialPutString("\r\n");
    return 0;
}

void *PeLoadDll(const char *dll_name) {
    SerialPutString("[PE] PeLoadDll: ");
    SerialPutString(dll_name);
    SerialPutString("\r\n");
    
    // Check if already loaded
    LOADED_DLL *dll = dll_list;
    while (dll) {
        SerialPutString("[PE] Checking against loaded: ");
        SerialPutString(dll->name);
        SerialPutString("\r\n");
        if (strcmp(dll->name, dll_name) == 0) {
            SerialPutString("[PE] DLL already loaded!\r\n");
            return dll->image_base;
        }
        dll = dll->next;
    }
    
    // Build uppercase path
    char path[256];
    char upper_name[128];
    
    // Uppercase the DLL name for ISO 9660
    int i;
    for (i = 0; dll_name[i] && i < 127; i++) {
        upper_name[i] = dll_name[i];
        if (upper_name[i] >= 'a' && upper_name[i] <= 'z')
            upper_name[i] -= 32;
    }
    upper_name[i] = 0;
    
    SerialPutString("[PE] Looking for: ");
    SerialPutString(upper_name);
    SerialPutString("\r\n");
    
    // Try /SYSTEM32/ first
    strcpy(path, "SYSTEM32/");
    strcat(path, upper_name);
    
    SerialPutString("[PE] Trying path: ");
    SerialPutString(path);
    SerialPutString("\r\n");
    
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    
    if (!CdfsReadFile(path, &file_buf, &file_size)) {
        SerialPutString("[PE] Not found in SYSTEM32, trying APPS...\r\n");
        
        // Try /APPS/
        strcpy(path, "APPS/");
        strcat(path, upper_name);
        
        SerialPutString("[PE] Trying path: ");
        SerialPutString(path);
        SerialPutString("\r\n");
        
        if (!CdfsReadFile(path, &file_buf, &file_size)) {
            SerialPutString("[PE] DLL not found: ");
            SerialPutString(upper_name);
            SerialPutString("\r\n");
            return 0;
        }
    }
    
    SerialPutString("[PE] Found! Size: ");
    SerialPrintDec(file_size);
    SerialPutString(" bytes\r\n");
    
    // Load the DLL
    void *image = PeLoadImage(file_buf, file_size);
    kfree(file_buf);
    
    if (!image) {
        SerialPutString("[PE] Failed to load DLL image\r\n");
        return 0;
    }
    
    SerialPutString("[PE] DLL image loaded at 0x");
    SerialPrintHex((uint32_t)image);
    SerialPutString("\r\n");
    
    // Resolve the DLL's own imports
    SerialPutString("[PE] Resolving DLL imports...\r\n");
    PeResolveImports(image);
    
    SerialPutString("[PE] Applying DLL relocations...\r\n");
    PePerformRelocations(image);
    
    // Call DllMain
    void *entry = PeGetEntryPoint(image);
    if (entry) {
        SerialPutString("[PE] Calling DllMain...\r\n");
        typedef int (*DllMain_t)(void*, uint32_t, void*);
        DllMain_t dllmain = (DllMain_t)entry;
        dllmain(image, 1, 0); // DLL_PROCESS_ATTACH
    }
    
    // Add to list
    LOADED_DLL *new_dll = (LOADED_DLL*)kmalloc(sizeof(LOADED_DLL));
    strcpy(new_dll->name, dll_name);
    new_dll->image_base = image;
    new_dll->entry_point = entry;
    new_dll->next = dll_list;
    dll_list = new_dll;
    
    SerialPutString("[PE] DLL loaded successfully!\r\n");
    return image;
}

void PePrintInfo(void *image_base) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    (void)opt;
}