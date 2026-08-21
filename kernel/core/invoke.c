#include <stdint.h>
#include "core/invoke.h"

int KeInvokeMain(void *entry, void *stack, uint32_t stack_size) {
    typedef int (*ENTRY)(void);
    (void)stack;
    (void)stack_size;
    return entry ? ((ENTRY)entry)() : -1;
}

int KeInvokeWinMain(void *entry, void *image, void *stack, uint32_t stack_size) {
    typedef int (*ENTRY)(void *, void *, char *, int);
    (void)stack;
    (void)stack_size;
    return entry ? ((ENTRY)entry)(image, 0, 0, 1) : -1;
}
