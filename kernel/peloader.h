#ifndef PELOADER_H
#define PELOADER_H
#include <stdint.h>

// DOS Header (first 64 bytes of PE file)
typedef struct {
    uint16_t e_magic;       // "MZ" (0x5A4D)
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;      // Offset to PE header
} __attribute__((packed)) IMAGE_DOS_HEADER;

// PE File Header
typedef struct {
    uint32_t Signature;              // "PE\0\0" (0x00004550)
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
} __attribute__((packed)) IMAGE_FILE_HEADER;

// PE Optional Header (32-bit)
typedef struct {
    uint16_t Magic;                  // 0x10B for PE32
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData;
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    // Data directories follow but we access them by offset
} __attribute__((packed)) IMAGE_OPTIONAL_HEADER32;

// Section Header
typedef struct {
    uint8_t  Name[8];
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} __attribute__((packed)) IMAGE_SECTION_HEADER;

// Import Descriptor
typedef struct {
    uint32_t ImportLookupTable;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t ImportAddressTable;
} __attribute__((packed)) IMAGE_IMPORT_DESCRIPTOR;

// Export Directory
typedef struct {
    uint32_t Characteristics;
    uint32_t TimeDateStamp;
    uint16_t MajorVersion;
    uint16_t MinorVersion;
    uint32_t Name;
    uint32_t Base;
    uint32_t NumberOfFunctions;
    uint32_t NumberOfNames;
    uint32_t AddressOfFunctions;
    uint32_t AddressOfNames;
    uint32_t AddressOfNameOrdinals;
} __attribute__((packed)) IMAGE_EXPORT_DIRECTORY;

// PE Loader functions
void *PeLoadImage(void *image_data, uint32_t size);
void *PeGetEntryPoint(void *image_base);
int PeResolveImports(void *image_base);
const char *PeGetLastError(void);
void PeClearLastError(void);
void PePrintInfo(void *image_base);
void PePrintImports(void *image_base);
void PePerformRelocations(void *image_base);
void PeFreeImage(void *image_base);

// DLL management
typedef struct _LOADED_DLL {
    char name[64];
    char path[128];
    void *image_base;
    void *entry_point;
    struct _LOADED_DLL *next;
} LOADED_DLL;

void PeInit(void);
void *PeLoadDll(const char *dll_name);
void *PeGetProcAddress(void *dll_base, const char *func_name);
void *PeResolveExternalSymbol(const char *func_name);
void PeSetImagePath(void *image_base, const char *path);
const char *PeGetImagePath(void *image_base);
void *PeGetLoadedModuleHandle(const char *name);
void *PeLoadImage(void *image_data, uint32_t size);
void *PeGetEntryPoint(void *image_base);
int PeResolveImports(void *image_base);
void PePerformRelocations(void *image_base);
void PePrintInfo(void *image_base);
void PeFreeImage(void *image_base);
#endif
