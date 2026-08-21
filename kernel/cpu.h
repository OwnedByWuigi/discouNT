#ifndef DISCOUNT_CPU_H
#define DISCOUNT_CPU_H
#include <stdint.h>
#include "io/port.h"

static inline void CpuRelax(void) {
#if defined(__loongarch64)
    __asm__ volatile("dbar 0" ::: "memory");
#else
    __asm__ volatile("pause");
#endif
}
static inline void CpuDisableInterrupts(void) {
#if defined(__loongarch64)
    __asm__ volatile("dbar 0" ::: "memory");
#else
    __asm__ volatile("cli" ::: "memory");
#endif
}
__attribute__((noreturn)) static inline void CpuHalt(void) {
    CpuDisableInterrupts();
    for (;;) {
#if defined(__loongarch64)
        __asm__ volatile("idle 0");
#else
        __asm__ volatile("hlt");
#endif
    }
}
__attribute__((noreturn)) static inline void CpuReboot(void) {
#if defined(__loongarch64)
    CpuHalt();
#else
    uint8_t status;
    do { status = inb(0x64); } while (status & 2);
    outb(0x64, 0xfe);
    __asm__ volatile("int $0");
    CpuHalt();
#endif
}
__attribute__((noreturn)) static inline void CpuPowerOff(void) {
#if !defined(__loongarch64)
    outw(0x604,0x2000); outw(0xb004,0x2000); outw(0x4004,0x3400);
#endif
    CpuHalt();
}
#endif
