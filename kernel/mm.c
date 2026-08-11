#include <stdint.h>
#include "mm.h"

/* USER32 list views reserve sizeable backing tables for native Win32 apps
 * (Task Manager has several).  Eight MiB was enough for the shell alone but
 * left no room to start a second GUI process. */
#define KERNEL_HEAP_SIZE 0x1000000
#define MM_ALIGN         8
#define MM_SPLIT_MIN     16

typedef struct _HEAP_BLOCK {
    uint32_t size;
    uint32_t free;
    struct _HEAP_BLOCK *next;
} HEAP_BLOCK;

static uint8_t kernel_heap[KERNEL_HEAP_SIZE];
static HEAP_BLOCK *heap_head = 0;
static int heap_initialized = 0;

static uint32_t mm_align_up(uint32_t size) {
    return (size + (MM_ALIGN - 1)) & ~(MM_ALIGN - 1);
}

static void mm_init(void) {
    if (heap_initialized) return;
    heap_head = (HEAP_BLOCK*)kernel_heap;
    heap_head->size = KERNEL_HEAP_SIZE - sizeof(HEAP_BLOCK);
    heap_head->free = 1;
    heap_head->next = 0;
    heap_initialized = 1;
}

static void mm_split_block(HEAP_BLOCK *block, uint32_t size) {
    HEAP_BLOCK *next;
    uint32_t remaining;

    if (!block) return;
    if (block->size <= size + sizeof(HEAP_BLOCK) + MM_SPLIT_MIN) return;

    remaining = block->size - size - sizeof(HEAP_BLOCK);
    next = (HEAP_BLOCK*)((uint8_t*)(block + 1) + size);
    next->size = remaining;
    next->free = 1;
    next->next = block->next;

    block->size = size;
    block->next = next;
}

static void mm_coalesce(void) {
    HEAP_BLOCK *block = heap_head;
    while (block && block->next) {
        if (block->free && block->next->free) {
            block->size += sizeof(HEAP_BLOCK) + block->next->size;
            block->next = block->next->next;
            continue;
        }
        block = block->next;
    }
}

void *kmalloc(uint32_t size) {
    HEAP_BLOCK *block;

    if (size == 0) return 0;
    mm_init();

    size = mm_align_up(size);
    block = heap_head;
    while (block) {
        if (block->free && block->size >= size) {
            mm_split_block(block, size);
            block->free = 0;
            return (void*)(block + 1);
        }
        block = block->next;
    }

    return 0;
}

void kfree(void *ptr) {
    HEAP_BLOCK *block;

    if (!ptr) return;
    if ((uint8_t*)ptr < kernel_heap || (uint8_t*)ptr >= kernel_heap + KERNEL_HEAP_SIZE) return;

    block = ((HEAP_BLOCK*)ptr) - 1;
    block->free = 1;
    mm_coalesce();
}
