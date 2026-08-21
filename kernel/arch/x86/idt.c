// kernel/arch/x86/idt.c
#include <stdint.h>
#include "arch/x86/idt.h"
#include "arch/x86/portio.h"

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

extern void isr_stub_0();
extern void isr_stub_1();
extern void isr_stub_2();
extern void isr_stub_3();
extern void isr_stub_4();
extern void isr_stub_5();
extern void isr_stub_6();
extern void isr_stub_7();
extern void isr_stub_8();
extern void isr_stub_9();
extern void isr_stub_10();
extern void isr_stub_11();
extern void isr_stub_12();
extern void isr_stub_13();
extern void isr_stub_14();

static struct IDTEntry idt[256];

static void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].offset_lo = base & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
    idt[num].offset_hi = (base >> 16) & 0xFFFF;
}

void IdtInitialize(void) {
    for (int i = 0; i < 256; i++) idt_set_gate(i, 0, 0x08, 0);
    idt_set_gate(0, (uint32_t)isr_stub_0, 0x08, 0x8E);
    idt_set_gate(1, (uint32_t)isr_stub_1, 0x08, 0x8E);
    idt_set_gate(2, (uint32_t)isr_stub_2, 0x08, 0x8E);
    idt_set_gate(3, (uint32_t)isr_stub_3, 0x08, 0x8E);
    idt_set_gate(4, (uint32_t)isr_stub_4, 0x08, 0x8E);
    idt_set_gate(5, (uint32_t)isr_stub_5, 0x08, 0x8E);
    idt_set_gate(6, (uint32_t)isr_stub_6, 0x08, 0x8E);
    idt_set_gate(7, (uint32_t)isr_stub_7, 0x08, 0x8E);
    idt_set_gate(8, (uint32_t)isr_stub_8, 0x08, 0x8E);
    idt_set_gate(9, (uint32_t)isr_stub_9, 0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr_stub_10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr_stub_11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr_stub_12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr_stub_13, 0x08, 0x8E);
    idt_set_gate(14, (uint32_t)isr_stub_14, 0x08, 0x8E);

    struct IDTPtr ptr = { sizeof(idt)-1, (uint32_t)&idt };
    __asm__ volatile("lidt %0" : : "m"(ptr));
}
