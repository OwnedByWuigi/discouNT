#ifndef DISCOUNT_INVOKE_H
#define DISCOUNT_INVOKE_H
#include <stdint.h>
int KeInvokeMain(void *entry, void *stack, uint32_t stack_size);
int KeInvokeWinMain(void *entry, void *image, void *stack, uint32_t stack_size);
#endif
