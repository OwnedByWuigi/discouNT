#ifndef MULTIBOOT_H
#define MULTIBOOT_H
#include <stdint.h>

#define MULTIBOOT_INFO_MEMORY  (1U << 0)
#define MULTIBOOT_INFO_MODULES (1U << 3)
#define MULTIBOOT_INFO_MMAP    (1U << 6)
#define MULTIBOOT_INFO_FRAMEBUFFER (1U << 12)
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
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    union {
        struct {
            uint32_t palette_addr;
            uint16_t palette_num_colors;
        } __attribute__((packed)) indexed;
        struct {
            uint8_t red_field_position;
            uint8_t red_mask_size;
            uint8_t green_field_position;
            uint8_t green_mask_size;
            uint8_t blue_field_position;
            uint8_t blue_mask_size;
        } __attribute__((packed)) rgb;
    } color_info;
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
