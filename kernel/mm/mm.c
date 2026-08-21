#include <stdint.h>
#include "mm/mm.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "core/util.h"

#define MM_ALIGN 8U
#define MM_SPLIT_MIN 16U
#define MM_HEAP_GROW_PAGES 16U

typedef struct _HEAP_BLOCK {
    uint32_t size;
    uint32_t free;
    struct _HEAP_BLOCK *next;
} HEAP_BLOCK;

static HEAP_BLOCK *heap_head;
static uint32_t heap_committed;
static int heap_initialized;

static uint32_t MmAlignUp(uint32_t size) {
    return (size + MM_ALIGN - 1) & ~(MM_ALIGN - 1);
}

void MmInitialize(void *boot_info) {
    PmmInitialize(boot_info);
    VmmInitialize();
    heap_head = 0;
    heap_committed = 0;
    heap_initialized = 1;
}

static HEAP_BLOCK *MmGrowHeap(uint32_t required) {
    uint32_t bytes = required + sizeof(HEAP_BLOCK);
    uint32_t pages = (bytes + PMM_PAGE_SIZE - 1) / PMM_PAGE_SIZE;
    HEAP_BLOCK *block;
    if (pages < MM_HEAP_GROW_PAGES) pages = MM_HEAP_GROW_PAGES;
    block = (HEAP_BLOCK*)VmmAllocatePages(pages);
    if (!block) return 0;
    block->size = pages * PMM_PAGE_SIZE - sizeof(HEAP_BLOCK);
    block->free = 1;
    block->next = heap_head;
    heap_head = block;
    heap_committed += pages * PMM_PAGE_SIZE;
    return block;
}

static void MmSplitBlock(HEAP_BLOCK *block, uint32_t size) {
    HEAP_BLOCK *remainder;
    if (block->size <= size + sizeof(HEAP_BLOCK) + MM_SPLIT_MIN) return;
    remainder = (HEAP_BLOCK*)((uint8_t*)(block + 1) + size);
    remainder->size = block->size - size - sizeof(HEAP_BLOCK);
    remainder->free = 1;
    remainder->next = block->next;
    block->size = size;
    block->next = remainder;
}

static void MmCoalesce(void) {
    HEAP_BLOCK *block = heap_head;
    while (block && block->next) {
        uint8_t *block_end = (uint8_t*)(block + 1) + block->size;
        if (block->free && block->next->free && block_end == (uint8_t*)block->next) {
            block->size += sizeof(HEAP_BLOCK) + block->next->size;
            block->next = block->next->next;
            continue;
        }
        block = block->next;
    }
}

void *kmalloc(uint32_t size) {
    HEAP_BLOCK *block;
    if (!size || !heap_initialized) return 0;
    size = MmAlignUp(size);
retry:
    for (block = heap_head; block; block = block->next) {
        if (block->free && block->size >= size) {
            MmSplitBlock(block, size);
            block->free = 0;
            return block + 1;
        }
    }
    if (MmGrowHeap(size)) goto retry;
    return 0;
}

void kfree(void *pointer) {
    HEAP_BLOCK *block;
    if (!pointer || !heap_initialized) return;
    for (block = heap_head; block; block = block->next) {
        if ((void*)(block + 1) == pointer) {
            block->free = 1;
            MmCoalesce();
            return;
        }
    }
}

void *malloc(uint32_t size) { return kmalloc(size); }

void *calloc(uint32_t count, uint32_t size) {
    uint32_t bytes;
    void *pointer;
    if (count && size > 0xffffffffU / count) return 0;
    bytes = count * size;
    pointer = kmalloc(bytes);
    if (pointer) memset(pointer, 0, bytes);
    return pointer;
}

void *realloc(void *pointer, uint32_t size) {
    HEAP_BLOCK *block;
    void *replacement;
    uint32_t copy_size;
    if (!pointer) return kmalloc(size);
    if (!size) { kfree(pointer); return 0; }
    for (block = heap_head; block; block = block->next)
        if ((void *)(block + 1) == pointer) break;
    if (!block || block->free) return 0;
    if (block->size >= size) return pointer;
    replacement = kmalloc(size);
    if (!replacement) return 0;
    copy_size = block->size < size ? block->size : size;
    memcpy(replacement, pointer, copy_size);
    kfree(pointer);
    return replacement;
}

void free(void *pointer) { kfree(pointer); }

uint32_t MmGetHeapUsed(void) {
    HEAP_BLOCK *block;
    uint32_t used = 0;
    for (block = heap_head; block; block = block->next)
        if (!block->free) used += block->size;
    return used;
}

uint32_t MmGetHeapTotal(void) { return heap_committed; }
