// kernel/scheduler.c
#include <stdint.h>
#include "scheduler.h"
#include "hal.h"
#include "portio.h"
#include "object.h"

THREAD *KiCurrentThread = 0;
static THREAD *readyList = 0;

void KiInitScheduler(void) {
    outb(0x43, 0x36);
    uint16_t div = 1193180 / 100;
    outb(0x40, div & 0xFF);
    outb(0x40, div >> 8);
    HalDisplayString("[Ke] Scheduler (PIT) initialized.\n");
}

void KeStartThread(THREAD *thread, void (*entry)(void)) {
    uint32_t *stack = (uint32_t*)(0x500000 + (thread - (THREAD*)0) * 0x1000);
    stack = (uint32_t*)(((uint32_t)stack & 0xFFFFF000) + 0x1000);
    stack--;
    *stack = 0x202;           // EFLAGS (IF on)
    stack--;
    *stack = 0x08;            // CS (code selector)
    stack--;
    *stack = (uint32_t)entry; // EIP
    thread->KernelStack = (uint32_t)stack;
    thread->State = 0;
    
    *((THREAD**)thread) = readyList;
    readyList = thread;
}

void KiStartScheduler(void) {
    if (!readyList) {
        HalDisplayString("[Ke] No ready threads – halting.\n");
        while(1) __asm__("hlt");
    }
    KiCurrentThread = readyList;
    
    // Use pure assembly to switch to first thread
    __asm__ volatile(
        "movl %0, %%esp\n\t"
        "popal\n\t"
        "iret\n\t"
        :
        : "m"(KiCurrentThread->KernelStack)
        : "memory"
    );
}

void irq0_handler(void) {
    if (!KiCurrentThread) return;
    
    // Save current ESP directly using register variable
    register uint32_t saved_esp asm("esp");
    KiCurrentThread->KernelStack = saved_esp;

    THREAD *next = *((THREAD**)KiCurrentThread);
    if (!next) next = readyList;
    if (next) {
        KiCurrentThread = next;
    }
    
    // Restore new thread's ESP
    __asm__ volatile(
        "movl %0, %%esp\n\t"
        :
        : "m"(KiCurrentThread->KernelStack)
        : "memory"
    );
}