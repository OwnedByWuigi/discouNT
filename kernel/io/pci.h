#ifndef DISCOUNT_IO_PCI_H
#define DISCOUNT_IO_PCI_H

#include <stdint.h>
#include "io/port.h"

/* Architecture-neutral PCI configuration-space access. */
static inline uint32_t PciConfigRead32(uint8_t bus, uint8_t device,
                                       uint8_t function, uint8_t offset) {
#if defined(__loongarch64)
    uintptr_t address = 0x20000000UL + ((uintptr_t)bus << 20) +
                        ((uintptr_t)device << 15) +
                        ((uintptr_t)function << 12) + (offset & 0xfcU);
    return *(volatile uint32_t *)address;
#else
    uint32_t address = 0x80000000U | ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) | (offset & 0xfcU);
    outl(0xcf8, address);
    return inl(0xcfc);
#endif
}

static inline void PciConfigWrite32(uint8_t bus, uint8_t device,
                                    uint8_t function, uint8_t offset,
                                    uint32_t value) {
#if defined(__loongarch64)
    uintptr_t address = 0x20000000UL + ((uintptr_t)bus << 20) +
                        ((uintptr_t)device << 15) +
                        ((uintptr_t)function << 12) + (offset & 0xfcU);
    *(volatile uint32_t *)address = value;
    __asm__ volatile("dbar 0" ::: "memory");
#else
    uint32_t address = 0x80000000U | ((uint32_t)bus << 16) |
                       ((uint32_t)device << 11) |
                       ((uint32_t)function << 8) | (offset & 0xfcU);
    outl(0xcf8, address);
    outl(0xcfc, value);
#endif
}

static inline uint16_t PciConfigRead16(uint8_t bus, uint8_t device,
                                       uint8_t function, uint8_t offset) {
    uint32_t value = PciConfigRead32(bus, device, function, offset);
    return (uint16_t)(value >> ((offset & 2U) * 8U));
}

static inline uint8_t PciConfigRead8(uint8_t bus, uint8_t device,
                                     uint8_t function, uint8_t offset) {
    uint32_t value = PciConfigRead32(bus, device, function, offset);
    return (uint8_t)(value >> ((offset & 3U) * 8U));
}

#endif
