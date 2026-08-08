// kernel/ke.c
#include <stdint.h>
#include "ke.h"
#include "mm.h"
#include "hal.h"
#include "util.h"

static THREAD *current_thread = 0;
static THREAD *ready_queue = 0;
static PROCESS *process_list = 0;
static uint32_t thread_count = 0;
static uint32_t scheduler_ticks = 0;

// Simple memory management for kernel objects
#define KERNEL_HEAP_SIZE 0x100000  // 1MB kernel heap
static uint8_t kernel_heap[KERNEL_HEAP_SIZE];
static uint32_t heap_offset = 0;

void *kmalloc(uint32_t size) {
    if (heap_offset + size > KERNEL_HEAP_SIZE) return 0;
    void *ptr = &kernel_heap[heap_offset];
    heap_offset += (size + 3) & ~3; // Align to 4 bytes
    return ptr;
}

void kfree(void *ptr) {
    // Simple bump allocator - no real free
    (void)ptr;
}

void KeInit(void) {
    thread_count = 0;
    scheduler_ticks = 0;
    HalPutString("[Ke] Kernel Executive initialized\n", 0x0A);
}

HANDLE KeCreateProcess(const char *name) {
    PROCESS *proc = (PROCESS*)kmalloc(sizeof(PROCESS));
    if (!proc) return INVALID_HANDLE;
    
    memset(proc, 0, sizeof(PROCESS));
    int len = strlen(name);
    if (len >= MAX_NAME_LEN) len = MAX_NAME_LEN - 1;
    memcpy(proc->name, name, len);
    
    // Insert into process list
    proc->thread_list = process_list ? process_list->thread_list : 0;
    process_list = proc;
    
    return ObCreateObject(OBJ_TYPE_PROCESS, name, proc, sizeof(PROCESS));
}

HANDLE KeCreateThread(void (*entry)(void), uint32_t stack_size) {
    THREAD *thread = (THREAD*)kmalloc(sizeof(THREAD));
    if (!thread) return INVALID_HANDLE;
    
    memset(thread, 0, sizeof(THREAD));
    thread->priority = 8; // Normal priority
    thread->state = THREAD_READY;
    thread->stack_size = stack_size ? stack_size : 4096;
    thread->stack = kmalloc(thread->stack_size);
    thread->entry_point = (uint32_t)entry;
    
    // Set up initial stack for context switch
    uint32_t *stack = (uint32_t*)((uint8_t*)thread->stack + thread->stack_size);
    *(--stack) = 0x202;         // EFLAGS
    *(--stack) = 0x08;          // CS
    *(--stack) = (uint32_t)entry; // EIP
    thread->context_esp = (uint32_t)stack;
    
    // Add to ready queue
    thread->next = ready_queue;
    ready_queue = thread;
    
    char name[32];
    itoa(thread_count++, name, 10);
    char thread_name[64] = "Thread";
    strcat(thread_name, name);
    
    HANDLE handle = ObCreateObject(OBJ_TYPE_THREAD, thread_name, thread, sizeof(THREAD));
    thread->handle = handle;
    return handle;
}

void KeTerminateThread(HANDLE thread_handle) {
    THREAD *thread = (THREAD*)ObReferenceObject(thread_handle);
    if (thread) {
        thread->state = THREAD_TERMINATED;
        ObDereferenceObject(thread_handle);
    }
}

void KeYield(void) {
    if (!current_thread) return;
    
    // Save current thread context
    __asm__ volatile("movl %%esp, %0" : "=m"(current_thread->context_esp));
    
    // Find next ready thread
    THREAD *next = current_thread->next;
    while (next && (next == current_thread || next->state != THREAD_READY)) {
        next = next->next;
    }
    if (!next) next = ready_queue; // Wrap around
    
    if (next && next->state == THREAD_READY) {
        THREAD *prev = current_thread;
        current_thread = next;
        current_thread->state = THREAD_RUNNING;
        
        // Switch to new thread
        __asm__ volatile("movl %0, %%esp\n\t" : : "m"(current_thread->context_esp));
    }
    
    scheduler_ticks++;
}

void KeStartScheduler(void) {
    if (!ready_queue) {
        HalPutString("[Ke] No threads to schedule!\n", 0x0C);
        return;
    }
    
    current_thread = ready_queue;
    current_thread->state = THREAD_RUNNING;
    
    HalPutString("[Ke] Starting scheduler...\n", 0x0A);
    
    // Jump to first thread
    __asm__ volatile(
        "movl %0, %%esp\n\t"
        "iret\n\t"
        : : "m"(current_thread->context_esp)
    );
}

HANDLE KeCreateMutex(void) {
    MUTEX *mutex = (MUTEX*)kmalloc(sizeof(MUTEX));
    memset(mutex, 0, sizeof(MUTEX));
    return ObCreateObject(OBJ_TYPE_MUTEX, "Mutex", mutex, sizeof(MUTEX));
}

void KeWaitMutex(HANDLE mutex_handle) {
    MUTEX *mutex = (MUTEX*)ObReferenceObject(mutex_handle);
    if (!mutex) return;
    
    while (mutex->locked) {
        KeYield();
    }
    mutex->locked = 1;
    mutex->owner = current_thread;
    ObDereferenceObject(mutex_handle);
}

void KeReleaseMutex(HANDLE mutex_handle) {
    MUTEX *mutex = (MUTEX*)ObReferenceObject(mutex_handle);
    if (mutex && mutex->owner == current_thread) {
        mutex->locked = 0;
        mutex->owner = 0;
    }
    if (mutex) ObDereferenceObject(mutex_handle);
}

HANDLE KeCreateEvent(uint32_t manual_reset) {
    EVENT *event = (EVENT*)kmalloc(sizeof(EVENT));
    memset(event, 0, sizeof(EVENT));
    event->manual_reset = manual_reset;
    return ObCreateObject(OBJ_TYPE_EVENT, "Event", event, sizeof(EVENT));
}

void KeSetEvent(HANDLE event_handle) {
    EVENT *event = (EVENT*)ObReferenceObject(event_handle);
    if (event) {
        event->signaled = 1;
        ObDereferenceObject(event_handle);
    }
}

void KeWaitEvent(HANDLE event_handle) {
    EVENT *event = (EVENT*)ObReferenceObject(event_handle);
    if (!event) return;
    
    while (!event->signaled) {
        KeYield();
    }
    
    if (!event->manual_reset) {
        event->signaled = 0;
    }
    ObDereferenceObject(event_handle);
}