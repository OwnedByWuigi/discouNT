#include <stdint.h>
#include "vmm.h"
#include "pmm.h"

/* The boot environment maintains an identity-mapped kernel address space.
 * This layer owns virtual allocations separately from the physical bitmap;
 * page-table replacement can therefore happen without changing heap users. */
void VmmInitialize(void) { }

void *VmmAllocatePages(uint32_t count) {
    uintptr_t physical = PmmAllocatePages(count);
    return physical ? (void*)physical : 0;
}

void VmmFreePages(void *address, uint32_t count) {
    PmmFreePages((uintptr_t)address, count);
}

uintptr_t VmmGetPhysicalAddress(const void *address) {
    return (uintptr_t)address;
}
