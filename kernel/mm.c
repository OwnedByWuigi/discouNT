#include <stdint.h>
#include "mm.h"

#define KERNEL_HEAP_SIZE 0x100000
static uint8_t kernel_heap[KERNEL_HEAP_SIZE];
static uint32_t heap_offset = 0;

void *kmalloc(uint32_t size) {
    // Align to 4 bytes
    size = (size + 3) & ~3;
    
    if (heap_offset + size > KERNEL_HEAP_SIZE) {
        return 0; // Out of memory
    }
    
    void *ptr = &kernel_heap[heap_offset];
    heap_offset += size;
    return ptr;
}

void kfree(void *ptr) {
    // Simple allocator - no free
    (void)ptr;
}