// kernel/mm.h
#ifndef MM_H
#define MM_H
#include <stdint.h>

void MmInitialize(void *boot_info);
void *kmalloc(uint32_t size);
void kfree(void *ptr);
uint32_t MmGetHeapUsed(void);
uint32_t MmGetHeapTotal(void);

#endif
