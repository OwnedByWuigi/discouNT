// kernel/entry.c
#include <stdint.h>
#include "hal.h"
#include "idt.h"
#include "object.h"
#include "scheduler.h"

void threadA(void) {
    while(1) {
        HalDisplayString("Thread A\n");
        for(volatile int i=0; i<10000000; i++);
    }
}

void threadB(void) {
    while(1) {
        HalDisplayString("Thread B\n");
        for(volatile int i=0; i<10000000; i++);
    }
}

// Kernel main - must use __attribute__((section(".text"))) if needed
void kmain(void) {
    HalInitialize();
    HalDisplayString("NT-like OS v0.1\n");
    HalDisplayString("Kernel loaded successfully!\n");

    IdtInitialize();
    HalDisplayString("[Ke] IDT initialized.\n");
    
    ObInit();
    
    PROCESS *system = (PROCESS*)ObCreateObject(OBJ_TYPE_PROCESS, "System");
    if (system) {
        HalDisplayString("[Ex] Created System process.\n");
    }

    THREAD *t1 = (THREAD*)ObCreateObject(OBJ_TYPE_THREAD, "ThreadA");
    THREAD *t2 = (THREAD*)ObCreateObject(OBJ_TYPE_THREAD, "ThreadB");
    if (t1) t1->Process = system;
    if (t2) t2->Process = system;

    HalDisplayString("[Ke] Starting scheduler...\n");
    KiInitScheduler();
    KeStartThread(t1, threadA);
    KeStartThread(t2, threadB);

    KiStartScheduler();
    
    // Should never reach here
    while(1) {
        HalDisplayString("Scheduler returned!\n");
    }
}