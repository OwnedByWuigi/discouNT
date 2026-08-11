#include <stdint.h>
#include "peloader.h"
#include "mm.h"
#include "util.h"
#include "serial.h"
#include "cdfs.h"
#include "kexports.h"

// DLL list
static LOADED_DLL *dll_list = 0;
static char pe_last_error[256];
static const char *pe_current_loading_dll = 0;

static void *PeGetELFProcAddress(void *elf_base, const char *func_name);
static void *PeFindLoadedDllSymbol(const char *func_name);

#define PE_RUNTIME_MAGIC 0x544E4C44U

typedef struct {
    uint32_t magic;
    uint32_t flags;
    uint32_t image_size;
    uint32_t elf_base_vaddr;
    void *source_image;
    uint32_t source_size;
} PE_RUNTIME_HEADER;

#define PE_RUNTIME_FLAG_ELF 0x00000001U

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

typedef struct {
    uint32_t offset;
    uint32_t info;
} __attribute__((packed)) ELF32_REL;

#define ELF32_R_SYM(info)  ((info) >> 8)
#define ELF32_R_TYPE(info) ((uint8_t)((info) & 0xFF))

#define R_386_32       1
#define R_386_PC32     2
#define R_386_GLOB_DAT 6
#define R_386_JMP_SLOT 7
#define R_386_RELATIVE 8

static void PeSetLastError(const char *text) {
    if (!text) {
        pe_last_error[0] = 0;
        return;
    }

    strncpy:
    {
        int i = 0;
        while (text[i] && i < (int)sizeof(pe_last_error) - 1) {
            pe_last_error[i] = text[i];
            i++;
        }
        pe_last_error[i] = 0;
    }
}

static void PeSetImportMissingDllError(const char *dll_name) {
    int i = 0;
    const char *prefix = "This application failed to start because ";
    const char *suffix = " was not found.";

    pe_last_error[0] = 0;
    while (*prefix && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *prefix++;
    if (dll_name) {
        while (*dll_name && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *dll_name++;
    }
    while (*suffix && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *suffix++;
    pe_last_error[i] = 0;
}

static void PeSetImportMissingProcError(const char *dll_name, const char *func_name) {
    int i = 0;
    const char *prefix = "The procedure entry point ";
    const char *middle = " could not be located in ";
    const char *suffix = ".";

    pe_last_error[0] = 0;
    while (*prefix && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *prefix++;
    if (func_name) {
        while (*func_name && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *func_name++;
    }
    while (*middle && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *middle++;
    if (dll_name) {
        while (*dll_name && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *dll_name++;
    }
    while (*suffix && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *suffix++;
    pe_last_error[i] = 0;
}

static void PeSetELFImportMissingProcError(const char *func_name) {
    int i = 0;
    const char *prefix = "The procedure entry point ";
    const char *suffix = " could not be located.";

    pe_last_error[0] = 0;
    while (*prefix && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *prefix++;
    if (func_name) {
        while (*func_name && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *func_name++;
    }
    while (*suffix && i < (int)sizeof(pe_last_error) - 1) pe_last_error[i++] = *suffix++;
    pe_last_error[i] = 0;
}

static PE_RUNTIME_HEADER *PeGetRuntimeHeader(void *image_base) {
    PE_RUNTIME_HEADER *hdr;
    if (!image_base) return 0;
    hdr = (PE_RUNTIME_HEADER*)((uint8_t*)image_base - sizeof(PE_RUNTIME_HEADER));
    if (hdr->magic != PE_RUNTIME_MAGIC) return 0;
    return hdr;
}

static uint32_t PeGetELFLoadBase(ELF32_HEADER *elf) {
    ELF32_PHDR *ph = (ELF32_PHDR*)((uint8_t*)elf + elf->phoff);
    uint32_t base = 0xFFFFFFFF;

    for (int i = 0; i < elf->phnum; i++) {
        if (ph[i].type == 1 && ph[i].memsz > 0) {
            if (ph[i].vaddr < base) base = ph[i].vaddr;
        }
    }

    return (base == 0xFFFFFFFF) ? 0 : base;
}

// ELF loader
static void *PeLoadELF(void *image_data, uint32_t size) {
    ELF32_HEADER *elf = (ELF32_HEADER*)image_data;
    PE_RUNTIME_HEADER *runtime;
    uint8_t *alloc_base;
    uint8_t *image_base;
    uint8_t *source_copy;
    
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
    
    // Calculate loaded span
    uint32_t mem_end = 0;
    uint32_t base = 0xFFFFFFFF;
    uint32_t image_size;
    
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
    
    if (base == 0xFFFFFFFF || mem_end <= base) {
        SerialPutString("[ELF] FAIL: bad load range\r\n");
        return 0;
    }

    image_size = mem_end - base;

    SerialPutString("[ELF] Span=0x");
    SerialPrintHex(image_size);
    SerialPutString("\r\n");

    if (image_size == 0 || image_size > 0x1000000) {
        SerialPutString("[ELF] FAIL: bad size\r\n");
        return 0;
    }
    
    alloc_base = (uint8_t*)kmalloc(sizeof(PE_RUNTIME_HEADER) + image_size);
    if (!alloc_base) {
        SerialPutString("[ELF] FAIL: OOM\r\n");
        return 0;
    }

    runtime = (PE_RUNTIME_HEADER*)alloc_base;
    image_base = alloc_base + sizeof(PE_RUNTIME_HEADER);

    source_copy = (uint8_t*)kmalloc(size);
    if (!source_copy) {
        kfree(alloc_base);
        SerialPutString("[ELF] FAIL: OOM source copy\r\n");
        return 0;
    }

    memcpy(source_copy, image_data, size);

    runtime->magic = PE_RUNTIME_MAGIC;
    runtime->flags = PE_RUNTIME_FLAG_ELF;
    runtime->image_size = image_size;
    runtime->elf_base_vaddr = base;
    runtime->source_image = source_copy;
    runtime->source_size = size;

    memset(image_base, 0, image_size);
    
    // Load program headers
    for (int i = 0; i < elf->phnum; i++) {
        if (ph[i].type == 1 && ph[i].filesz > 0) {
            uint32_t dest = ph[i].vaddr - base;
            uint32_t src = ph[i].offset;
            uint32_t len = ph[i].filesz;
            
            if (src + len <= size && dest + len <= image_size) {
                memcpy(image_base + dest, (uint8_t*)image_data + src, len);
            }
        }
    }
    
    // Apply relocations from REL sections in the source image
    ELF32_SHDR *sh = (ELF32_SHDR*)((uint8_t*)image_data + elf->shoff);
    uint8_t *shstrtab = (uint8_t*)image_data + sh[elf->shstrndx].offset;
    
    for (int i = 0; i < elf->shnum; i++) {
        const char *name = (const char*)(shstrtab + sh[i].name);
        
        if (sh[i].type == 9) {
            ELF32_REL *rel_data = (ELF32_REL*)((uint8_t*)image_data + sh[i].offset);
            ELF32_SYM *symtab = 0;
            const char *strtab = 0;
            int count = sh[i].size / sizeof(ELF32_REL);
            
            SerialPutString("[ELF] Applying ");
            SerialPrintDec(count);
            SerialPutString(" relocations from ");
            SerialPutString(name);
            SerialPutString("...\r\n");

            if (sh[i].link < elf->shnum) {
                ELF32_SHDR *symsec = &sh[sh[i].link];
                if (symsec->entsize == sizeof(ELF32_SYM) && symsec->offset < size) {
                    symtab = (ELF32_SYM*)((uint8_t*)image_data + symsec->offset);
                    if (symsec->link < elf->shnum) {
                        ELF32_SHDR *strsec = &sh[symsec->link];
                        if (strsec->offset < size) {
                            strtab = (const char*)((uint8_t*)image_data + strsec->offset);
                        }
                    }
                }
            }
            
            for (int j = 0; j < count; j++) {
                uint32_t r_offset = rel_data[j].offset;
                uint32_t r_info = rel_data[j].info;
                uint32_t r_sym = ELF32_R_SYM(r_info);
                uint8_t r_type = ELF32_R_TYPE(r_info);
                uint32_t patch_off;
                uint32_t *patch;
                uint32_t addend;
                uint32_t sym_value = 0;
                
                if (r_offset < base) continue;
                patch_off = r_offset - base;
                if (patch_off + 4 > image_size) continue;

                patch = (uint32_t*)(image_base + patch_off);
                addend = *patch;

                if (symtab && r_sym != 0) {
                    ELF32_SYM *sym = &symtab[r_sym];
                    if (sym->value >= base) {
                        sym_value = (uint32_t)image_base + (sym->value - base);
                    } else if (sym->value != 0) {
                        sym_value = (uint32_t)image_base + sym->value;
                    }

                    if (sym->value == 0 && strtab && sym->name != 0) {
                        const char *sym_name = strtab + sym->name;
                        void *resolved = KernelResolveSymbol(sym_name);
                        if (resolved) {
                            sym_value = (uint32_t)resolved;
                        } else {
                            PeSetELFImportMissingProcError(sym_name);
                            SerialPutString("[ELF] Unresolved external symbol: ");
                            SerialPutString(sym_name);
                            SerialPutString("\r\n");
                            kfree(source_copy);
                            kfree(alloc_base);
                            return 0;
                        }
                    }
                }

                switch (r_type) {
                    case R_386_RELATIVE:
                        *patch = (uint32_t)image_base + addend;
                        break;
                    case R_386_PC32:
                        if (sym_value != 0) {
                            *patch = sym_value + addend - (uint32_t)patch;
                        }
                        break;
                    case R_386_GLOB_DAT:
                    case R_386_JMP_SLOT:
                        if (sym_value != 0) *patch = sym_value;
                        break;
                    case R_386_32:
                        if (sym_value != 0) *patch = sym_value + addend;
                        break;
                    default:
                        break;
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
    
    PE_RUNTIME_HEADER *runtime;
    uint8_t *alloc_base = (uint8_t*)kmalloc(sizeof(PE_RUNTIME_HEADER) + image_size);
    uint8_t *image_base;

    if (!alloc_base) {
        SerialPutString("[PELoad] FAIL: OOM\r\n");
        return 0;
    }

    runtime = (PE_RUNTIME_HEADER*)alloc_base;
    runtime->magic = PE_RUNTIME_MAGIC;
    runtime->flags = 0;
    runtime->image_size = image_size;
    runtime->elf_base_vaddr = 0;
    runtime->source_image = 0;
    runtime->source_size = 0;

    image_base = alloc_base + sizeof(PE_RUNTIME_HEADER);
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
    PE_RUNTIME_HEADER *runtime = PeGetRuntimeHeader(image_base);

    if (!image_base) return 0;
    
    // Check for ELF
    if (*(uint32_t*)image_base == 0x464C457F) {
        ELF32_HEADER *elf = (ELF32_HEADER*)image_base;
        uint32_t base = runtime && (runtime->flags & PE_RUNTIME_FLAG_ELF)
            ? runtime->elf_base_vaddr
            : PeGetELFLoadBase(elf);
        if (elf->entry != 0) {
            if (base != 0 && elf->entry >= base) {
                return (uint8_t*)image_base + (elf->entry - base);
            }
            if (runtime && elf->entry < runtime->image_size) {
                return (uint8_t*)image_base + elf->entry;
            }
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

void PeFreeImage(void *image_base) {
    PE_RUNTIME_HEADER *runtime;

    if (!image_base) return;

    runtime = PeGetRuntimeHeader(image_base);
    if (!runtime) {
        kfree(image_base);
        return;
    }

    if (runtime->source_image) {
        kfree(runtime->source_image);
        runtime->source_image = 0;
    }

    kfree(runtime);
}

void PePerformRelocations(void *image_base) {
    if (!image_base) return;
    if (*(uint32_t*)image_base == 0x464C457F) return;

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
    if (!image_base) return 0;
    if (*(uint32_t*)image_base == 0x464C457F) return 1;
    PeClearLastError();

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
                    if (!pe_last_error[0]) PeSetImportMissingProcError(dll_name, func_name);
                    SerialPutString("  ");
                    SerialPutString(func_name);
                    SerialPutString(" -> MISSING\r\n");
                }
            } else if (func_name) {
                missing++;
                *iat = 0;
                if (!pe_last_error[0]) PeSetImportMissingDllError(dll_name);
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

static void *PeFindLoadedDllSymbol(const char *func_name) {
    LOADED_DLL *dll = dll_list;
    while (dll) {
        void *addr = PeGetProcAddress(dll->image_base, func_name);
        if (addr) return addr;
        dll = dll->next;
    }
    return 0;
}

void *PeResolveExternalSymbol(const char *func_name) {
    static const char *core_dlls[] = {
        "NTDLL.DLL",
        "KERNEL32.DLL",
        "ADVAPI32.DLL",
        "GDI32.DLL",
        "USER32.DLL",
        "SHELL32.DLL",
        "SHLWAPI.DLL",
        "COMCTL32.DLL",
        "COMDLG32.DLL"
    };
    void *addr;

    if (!func_name || !*func_name) return 0;

    addr = PeFindLoadedDllSymbol(func_name);
    if (addr) return addr;

    if (pe_current_loading_dll) {
        return 0;
    }

    for (uint32_t i = 0; i < (sizeof(core_dlls) / sizeof(core_dlls[0])); i++) {
        PeLoadDll(core_dlls[i]);
    }

    return PeFindLoadedDllSymbol(func_name);
}

const char *PeGetLastError(void) {
    return pe_last_error[0] ? pe_last_error : 0;
}

void PeClearLastError(void) {
    pe_last_error[0] = 0;
}

void *PeGetProcAddress(void *dll_base, const char *func_name) {
    PE_RUNTIME_HEADER *runtime;

    if (!dll_base || !func_name) return 0;

    runtime = PeGetRuntimeHeader(dll_base);
    if (runtime && (runtime->flags & PE_RUNTIME_FLAG_ELF)) {
        return PeGetELFProcAddress(dll_base, func_name);
    }

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
    PE_RUNTIME_HEADER *runtime = PeGetRuntimeHeader(elf_base);
    ELF32_HEADER *elf;
    uint8_t *elf_file_base;
    uint32_t base;
    
    SerialPutString("[ELF] Looking for: ");
    SerialPutString(func_name);
    SerialPutString("\r\n");

    if (runtime && (runtime->flags & PE_RUNTIME_FLAG_ELF) && runtime->source_image) {
        elf_file_base = (uint8_t*)runtime->source_image;
        elf = (ELF32_HEADER*)elf_file_base;
        base = runtime->elf_base_vaddr;
    } else {
        elf_file_base = (uint8_t*)elf_base;
        elf = (ELF32_HEADER*)elf_base;
        base = PeGetELFLoadBase(elf);
    }
    
    // Section headers are at file offset shoff
    // We need to find .dynsym and .dynstr using their file offsets
    ELF32_SHDR *sh = (ELF32_SHDR*)(elf_file_base + elf->shoff);
    
    uint32_t shstr_offset = sh[elf->shstrndx].offset;
    uint8_t *shstr = elf_file_base + shstr_offset;
    
    ELF32_SYM *dynsym = 0;
    uint32_t dynsym_count = 0;
    const char *dynstr = 0;
    uint32_t dynsym_addr = 0;
    uint32_t dynstr_addr = 0;
    ELF32_SYM *symtab = 0;
    uint32_t symtab_count = 0;
    const char *strtab = 0;
    
    for (int i = 0; i < elf->shnum; i++) {
        const char *name = (const char*)(shstr + sh[i].name);
        
        if (strcmp(name, ".dynsym") == 0) {
            dynsym_addr = sh[i].offset;
            dynsym = (ELF32_SYM*)(elf_file_base + dynsym_addr);
            dynsym_count = sh[i].size / sizeof(ELF32_SYM);
            SerialPutString("[ELF] .dynsym at 0x");
            SerialPrintHex(dynsym_addr);
            SerialPutString(" count=");
            SerialPrintDec(dynsym_count);
            SerialPutString("\r\n");
        }
        if (strcmp(name, ".dynstr") == 0) {
            dynstr_addr = sh[i].offset;
            dynstr = (const char*)(elf_file_base + dynstr_addr);
            SerialPutString("[ELF] .dynstr at 0x");
            SerialPrintHex(dynstr_addr);
            SerialPutString("\r\n");
        }
        if (strcmp(name, ".symtab") == 0) {
            symtab = (ELF32_SYM*)(elf_file_base + sh[i].offset);
            symtab_count = sh[i].size / sizeof(ELF32_SYM);
        }
        if (strcmp(name, ".strtab") == 0) {
            strtab = (const char*)(elf_file_base + sh[i].offset);
        }
    }
    
    if (!dynsym || !dynstr) {
        if (symtab && strtab) {
            dynsym = symtab;
            dynsym_count = symtab_count;
            dynstr = strtab;
            SerialPutString("[ELF] Using .symtab/.strtab fallback\r\n");
        }
    }

    if (!dynsym || !dynstr) {
        SerialPutString("[ELF] Dynamic/static symbols not found\r\n");
        
        // Dump sections
        SerialPutString("[ELF] Sections: shnum=");
        SerialPrintDec(elf->shnum);
        SerialPutString("\r\n");
        for (int i = 0; i < elf->shnum && i < 20; i++) {
            SerialPutString("  [");
            SerialPrintDec(i);
            SerialPutString("] ");
            SerialPutString((const char*)(shstr + sh[i].name));
            SerialPutString(" addr=0x");
            SerialPrintHex(sh[i].addr);
            SerialPutString(" offset=0x");
            SerialPrintHex(sh[i].offset);
            SerialPutString(" size=0x");
            SerialPrintHex(sh[i].size);
            SerialPutString("\r\n");
        }
        return 0;
    }
    
    // Search for the symbol
    for (uint32_t i = 0; i < dynsym_count; i++) {
        if (dynsym[i].name == 0) continue;
        const char *name = dynstr + dynsym[i].name;
        if (strcmp(name, func_name) == 0) {
            uint32_t sym_value = dynsym[i].value;
            SerialPutString("[ELF] Found ");
            SerialPutString(func_name);
            SerialPutString(" value=0x");
            SerialPrintHex(sym_value);
            SerialPutString("\r\n");
            
            if (sym_value != 0 && sym_value >= base) {
                return (uint8_t*)elf_base + (sym_value - base);
            }
            // If value is 0, maybe it's an import that needs resolving
            return 0;
        }
    }
    
    // Print first 5 symbols for debugging
    SerialPutString("[ELF] First symbols:\r\n");
    for (uint32_t i = 0; i < 5 && i < dynsym_count; i++) {
        if (dynsym[i].name != 0) {
            SerialPutString("  ");
            SerialPutString(dynstr + dynsym[i].name);
            SerialPutString(" value=0x");
            SerialPrintHex(dynsym[i].value);
            SerialPutString("\r\n");
        }
    }
    
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
    pe_current_loading_dll = dll_name;
    void *image = PeLoadImage(file_buf, file_size);
    pe_current_loading_dll = 0;
    kfree(file_buf);
    
    if (!image) {
        SerialPutString("[PE] Failed to load DLL image\r\n");
        return 0;
    }
    
    SerialPutString("[PE] DLL image loaded at 0x");
    SerialPrintHex((uint32_t)image);
    SerialPutString("\r\n");

    // Register immediately to prevent recursive self-load loops while resolving imports/relocations.
    LOADED_DLL *new_dll = (LOADED_DLL*)kmalloc(sizeof(LOADED_DLL));
    if (!new_dll) {
        PeFreeImage(image);
        SerialPutString("[PE] Failed to allocate LOADED_DLL record\r\n");
        return 0;
    }
    strcpy(new_dll->name, dll_name);
    new_dll->image_base = image;
    new_dll->entry_point = 0;
    new_dll->next = dll_list;
    dll_list = new_dll;
    
    // Resolve the DLL's own imports
    SerialPutString("[PE] Resolving DLL imports...\r\n");
    if (!PeResolveImports(image)) {
        SerialPutString("[PE] DLL import resolution failed\r\n");
        dll_list = new_dll->next;
        kfree(new_dll);
        PeFreeImage(image);
        return 0;
    }
    
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

    new_dll->entry_point = entry;
    
    SerialPutString("[PE] DLL loaded successfully!\r\n");
    return image;
}

void PePrintInfo(void *image_base) {
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER*)image_base;
    IMAGE_FILE_HEADER *file = (IMAGE_FILE_HEADER*)((uint8_t*)image_base + dos->e_lfanew);
    IMAGE_OPTIONAL_HEADER32 *opt = (IMAGE_OPTIONAL_HEADER32*)((uint8_t*)file + sizeof(IMAGE_FILE_HEADER));
    (void)opt;
}
