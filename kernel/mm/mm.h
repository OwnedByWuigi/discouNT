// kernel/mm/mm.h
#ifndef MM_H
#define MM_H
#include <stdint.h>

void MmInitialize(void *boot_info);
void *kmalloc(uint32_t size);
void kfree(void *ptr);
void *malloc(uint32_t size);
void *calloc(uint32_t count, uint32_t size);
void *realloc(void *ptr, uint32_t size);
void free(void *ptr);
uint32_t MmGetHeapUsed(void);
uint32_t MmGetHeapTotal(void);

#endif
