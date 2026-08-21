#ifndef PMM_H
#define PMM_H
#include <stdint.h>

#define PMM_PAGE_SIZE 4096U

void PmmInitialize(void *boot_info);
uintptr_t PmmAllocatePage(void);
uintptr_t PmmAllocatePages(uint32_t count);
void PmmFreePage(uintptr_t address);
void PmmFreePages(uintptr_t address, uint32_t count);
uint32_t PmmGetTotalPages(void);
uint32_t PmmGetFreePages(void);

#endif
