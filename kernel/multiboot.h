#ifndef MULTIBOOT_H
#define MULTIBOOT_H
#include <stdint.h>

#define MULTIBOOT_INFO_MEMORY  (1U << 0)
#define MULTIBOOT_INFO_MODULES (1U << 3)
#define MULTIBOOT_INFO_MMAP    (1U << 6)
#define MULTIBOOT_MEMORY_AVAILABLE 1

typedef struct _MULTIBOOT_INFO {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint8_t syms[16];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} __attribute__((packed)) MULTIBOOT_INFO;

typedef struct _MULTIBOOT_MMAP_ENTRY {
    uint32_t size;
    uint64_t base;
    uint64_t length;
    uint32_t type;
} __attribute__((packed)) MULTIBOOT_MMAP_ENTRY;

typedef struct _MULTIBOOT_MODULE {
    uint32_t start;
    uint32_t end;
    uint32_t string;
    uint32_t reserved;
} __attribute__((packed)) MULTIBOOT_MODULE;

#endif
