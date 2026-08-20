#ifndef VMM_H
#define VMM_H
#include <stdint.h>

void VmmInitialize(void);
void *VmmAllocatePages(uint32_t count);
void VmmFreePages(void *address, uint32_t count);
uintptr_t VmmGetPhysicalAddress(const void *address);

#endif
