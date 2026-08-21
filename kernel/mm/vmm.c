#include <stdint.h>
#include "mm/vmm.h"
#include "mm/pmm.h"

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

void *VmmMapMmioRange(uint64_t physical_address, uint32_t length) {
#if defined(__x86_64__)
    uint64_t start = physical_address & ~0x1FFFFFULL;
    uint64_t end = (physical_address + length + 0x1FFFFFULL) & ~0x1FFFFFULL;
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t *pml4 = (uint64_t *)(uintptr_t)(cr3 & ~0xFFFULL);
    for (uint64_t address = start; address < end; address += 0x200000ULL) {
        uint32_t pml4_index = (uint32_t)((address >> 39) & 0x1FF);
        uint32_t pdpt_index = (uint32_t)((address >> 30) & 0x1FF);
        uint32_t pd_index = (uint32_t)((address >> 21) & 0x1FF);
        uint64_t *pdpt, *pd;
        if (!(pml4[pml4_index] & 1)) {
            uintptr_t page = PmmAllocatePage();
            if (!page) return 0;
            pdpt = (uint64_t *)page;
            for (uint32_t i = 0; i < 512; ++i) pdpt[i] = 0;
            pml4[pml4_index] = (uint64_t)page | 3;
        } else pdpt = (uint64_t *)(uintptr_t)(pml4[pml4_index] & 0x000FFFFFFFFFF000ULL);
        if (!(pdpt[pdpt_index] & 1)) {
            uintptr_t page = PmmAllocatePage();
            if (!page) return 0;
            pd = (uint64_t *)page;
            for (uint32_t i = 0; i < 512; ++i) pd[i] = 0;
            pdpt[pdpt_index] = (uint64_t)page | 3;
        } else pd = (uint64_t *)(uintptr_t)(pdpt[pdpt_index] & 0x000FFFFFFFFFF000ULL);
        pd[pd_index] = address | 0x83;
    }
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3) : "memory");
    return (void *)(uintptr_t)physical_address;
#else
    if (physical_address + length > 0x100000000ULL) return 0;
    return (void *)(uintptr_t)physical_address;
#endif
}
