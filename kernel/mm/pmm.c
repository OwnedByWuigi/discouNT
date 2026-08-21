#include <stdint.h>
#include "mm/pmm.h"
#include "arch/x86/multiboot.h"
#include "core/util.h"

#define PMM_MAX_PAGES 1048576U /* Physical addresses below 4 GiB. */
#define PMM_BITMAP_BYTES (PMM_MAX_PAGES / 8)

extern uint8_t __kernel_start;
extern uint8_t __kernel_end;

static uint8_t page_bitmap[PMM_BITMAP_BYTES];
static uint32_t page_limit;
static uint32_t managed_pages;
static uint32_t free_pages;
static uint32_t search_hint;

static int PmmPageUsed(uint32_t page) {
    return (page_bitmap[page >> 3] & (1U << (page & 7))) != 0;
}

static void PmmSetPage(uint32_t page, int used) {
    uint8_t mask;
    int was_used;
    if (page >= page_limit) return;
    mask = (uint8_t)(1U << (page & 7));
    was_used = (page_bitmap[page >> 3] & mask) != 0;
    if (used) page_bitmap[page >> 3] |= mask;
    else page_bitmap[page >> 3] &= (uint8_t)~mask;
    if (was_used && !used) free_pages++;
    if (!was_used && used && free_pages) free_pages--;
}

static void PmmMarkRange(uint64_t base, uint64_t length, int used) {
    uint64_t first = used ? base / PMM_PAGE_SIZE :
        (base + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    uint64_t last = used ? (base + length + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE :
        (base + length) / PMM_PAGE_SIZE;
    uint64_t page;
    if (last > page_limit) last = page_limit;
    for (page = first; page < last; page++) PmmSetPage((uint32_t)page, used);
}

static void PmmReserveBootData(MULTIBOOT_INFO *info) {
    uint32_t i;
    uintptr_t kernel_end = (uintptr_t)&__kernel_end;
    PmmMarkRange(0, kernel_end, 1);
    if (!info) return;
    PmmMarkRange((uintptr_t)info, sizeof(*info), 1);
    if ((info->flags & MULTIBOOT_INFO_MMAP) && info->mmap_addr)
        PmmMarkRange(info->mmap_addr, info->mmap_length, 1);
    if ((info->flags & MULTIBOOT_INFO_MODULES) && info->mods_addr) {
        MULTIBOOT_MODULE *modules = (MULTIBOOT_MODULE*)(uintptr_t)info->mods_addr;
        PmmMarkRange(info->mods_addr, info->mods_count * sizeof(*modules), 1);
        for (i = 0; i < info->mods_count; i++)
            if (modules[i].end > modules[i].start)
                PmmMarkRange(modules[i].start, modules[i].end - modules[i].start, 1);
    }
}

void PmmInitialize(void *boot_info) {
    MULTIBOOT_INFO *info = (MULTIBOOT_INFO*)boot_info;
    uint64_t highest = 16ULL * 1024 * 1024;
    memset(page_bitmap, 0xFF, sizeof(page_bitmap));
    free_pages = 0;

    if (info && (info->flags & MULTIBOOT_INFO_MMAP)) {
        uint32_t offset = 0;
        while (offset + sizeof(uint32_t) <= info->mmap_length) {
            MULTIBOOT_MMAP_ENTRY *entry = (MULTIBOOT_MMAP_ENTRY*)
                (uintptr_t)(info->mmap_addr + offset);
            uint64_t end = entry->base + entry->length;
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE && end > highest) highest = end;
            offset += entry->size + sizeof(entry->size);
            if (entry->size == 0) break;
        }
    } else if (info && (info->flags & MULTIBOOT_INFO_MEMORY)) {
        highest = 0x100000ULL + ((uint64_t)info->mem_upper * 1024ULL);
    }
    if (highest > 0x100000000ULL) highest = 0x100000000ULL;
    page_limit = (uint32_t)(highest / PMM_PAGE_SIZE);
    if (page_limit > PMM_MAX_PAGES) page_limit = PMM_MAX_PAGES;

    if (info && (info->flags & MULTIBOOT_INFO_MMAP)) {
        uint32_t offset = 0;
        while (offset + sizeof(uint32_t) <= info->mmap_length) {
            MULTIBOOT_MMAP_ENTRY *entry = (MULTIBOOT_MMAP_ENTRY*)
                (uintptr_t)(info->mmap_addr + offset);
            if (entry->type == MULTIBOOT_MEMORY_AVAILABLE)
                PmmMarkRange(entry->base, entry->length, 0);
            offset += entry->size + sizeof(entry->size);
            if (entry->size == 0) break;
        }
    } else {
        PmmMarkRange(0x100000, highest - 0x100000, 0);
    }
    managed_pages = free_pages;
    PmmReserveBootData(info);
    search_hint = ((uintptr_t)&__kernel_end + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
}

uintptr_t PmmAllocatePages(uint32_t count) {
    uint32_t page, run, start, begin, end;
    int pass;
    if (!count || count > free_pages) return 0;
    for (pass = 0; pass < 2; pass++) {
        begin = pass == 0 ? search_hint : 1;
        end = pass == 0 ? page_limit : search_hint;
        run = 0;
        start = 0;
        for (page = begin; page < end; page++) {
            if (!PmmPageUsed(page)) {
                if (!run) start = page;
                if (++run == count) {
                    for (page = start; page < start + count; page++) PmmSetPage(page, 1);
                    search_hint = start + count;
                    return (uintptr_t)start * PMM_PAGE_SIZE;
                }
            } else run = 0;
        }
    }
    return 0;
}

uintptr_t PmmAllocatePage(void) { return PmmAllocatePages(1); }

void PmmFreePages(uintptr_t address, uint32_t count) {
    uint32_t page = (uint32_t)(address / PMM_PAGE_SIZE), i;
    if ((address & (PMM_PAGE_SIZE - 1)) || page == 0) return;
    for (i = 0; i < count && page + i < page_limit; i++) PmmSetPage(page + i, 0);
    if (page < search_hint) search_hint = page;
}

void PmmFreePage(uintptr_t address) { PmmFreePages(address, 1); }
uint32_t PmmGetTotalPages(void) { return managed_pages; }
uint32_t PmmGetFreePages(void) { return free_pages; }
