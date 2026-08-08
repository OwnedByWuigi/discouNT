// kernel/idt.c
#include <stdint.h>
#include "idt.h"
#include "portio.h"

struct IDTEntry {
    uint16_t offset_lo;
    uint16_t selector;
    uint8_t zero;
    uint8_t type_attr;
    uint16_t offset_hi;
} __attribute__((packed));

struct IDTPtr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

extern void isr0();
extern void isr32();

static struct IDTEntry idt[256];

static void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_lo = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_hi = (base >> 16) & 0xFFFF;
}

void IdtInitialize(void) {
    for (int i = 0; i < 256; i++)
        idt_set_gate(i, (uint32_t)isr0, 0x08, 0x8E);
    
    idt_set_gate(32, (uint32_t)isr32, 0x08, 0x8E);

    struct IDTPtr ptr = { sizeof(idt)-1, (uint32_t)&idt };
    __asm__ volatile("lidt %0" : : "m"(ptr));
}