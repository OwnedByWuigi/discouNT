// kernel/core/ke.c
#include <stdint.h>
#include "core/ke.h"
#include "mm/mm.h"
#include "arch/x86/hal.h"
#include "core/util.h"

static THREAD *current_thread = 0;
static THREAD *ready_queue = 0;
static PROCESS *process_list = 0;
static uint32_t thread_count = 0;
static uint32_t scheduler_ticks = 0;
static uint32_t process_object_type;
static uint32_t thread_object_type;
static uint32_t event_object_type;
static uint32_t mutex_object_type;

static void KeDeleteThreadObject(void *body) {
    THREAD *thread = (THREAD*)body;
    if (thread && thread->stack) {
        kfree(thread->stack);
        thread->stack = 0;
    }
}

// Note: kmalloc and kfree are now in mm.c - include mm.h instead

static void KeThreadBootstrap(void) {
    if (current_thread) {
        void (*entry)(void*) = (void (*)(void*))current_thread->entry_point;
        void *arg = (void*)current_thread->entry_arg;
        if (entry) entry(arg);
        current_thread->state = THREAD_TERMINATED;
    }
    for (;;) KeYield();
}

static void KeAppendReadyThread(THREAD *thread) {
    THREAD *tail;
    if (!thread) return;
    thread->next = 0;
    if (!ready_queue) {
        ready_queue = thread;
        return;
    }
    tail = ready_queue;
    while (tail->next) tail = tail->next;
    tail->next = thread;
}

void KeInit(void) {
    thread_count = 0;
    scheduler_ticks = 0;
    process_object_type = ObRegisterObjectType("Process", 0);
    thread_object_type = ObRegisterObjectType("Thread", KeDeleteThreadObject);
    event_object_type = ObRegisterObjectType("Event", 0);
    mutex_object_type = ObRegisterObjectType("Mutex", 0);
    // Don't print anything here - HAL may not be initialized
}

void KeAttachCurrentThread(const char *name) {
    THREAD *thread = (THREAD*)kmalloc(sizeof(THREAD));
    HANDLE handle;

    if (!thread || current_thread) return;
    memset(thread, 0, sizeof(THREAD));
    thread->priority = 8;
    thread->state = THREAD_RUNNING;
    thread->stack = 0;
    thread->stack_size = 0;
    thread->entry_point = 0;
    thread->entry_arg = 0;
    thread->context_esp = 0;
    thread->context_ebx = 0;
    thread->context_esi = 0;
    thread->context_edi = 0;
    thread->context_ebp = 0;
    handle = ObCreateObject(thread_object_type, name ? name : "MainThread", thread, sizeof(THREAD));
    thread->handle = handle;
    current_thread = thread;
    ready_queue = thread;
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
    
    return ObCreateObject(process_object_type, name, proc, sizeof(PROCESS));
}

HANDLE KeCreateThread(void (*entry)(void *), void *arg, uint32_t stack_size) {
    THREAD *thread = (THREAD*)kmalloc(sizeof(THREAD));
    if (!thread) return INVALID_HANDLE;
    
    memset(thread, 0, sizeof(THREAD));
    thread->priority = 8; // Normal priority
    thread->state = THREAD_READY;
    thread->stack_size = stack_size ? stack_size : 4096;
    thread->stack = kmalloc(thread->stack_size);
    thread->entry_point = (uintptr_t)entry;
    thread->entry_arg = (uintptr_t)arg;
    thread->context_ebx = 0;
    thread->context_esi = 0;
    thread->context_edi = 0;
    thread->context_ebp = 0;
    
    // Cooperative switch: after KeYield restores ESP and returns, execution enters bootstrap.
#if defined(__x86_64__)
    /* ret enters a SysV AMD64 function with RSP % 16 == 8. */
    uintptr_t stack_top = ((uintptr_t)thread->stack + thread->stack_size) & ~(uintptr_t)15;
    uintptr_t *stack = (uintptr_t*)(stack_top - 16);
    stack[0] = (uintptr_t)KeThreadBootstrap;
    stack[1] = 0;
    thread->context_esp = (uintptr_t)stack;
#else
    uint32_t *stack = (uint32_t*)((uint8_t*)thread->stack + thread->stack_size);
    *(--stack) = (uint32_t)KeThreadBootstrap;
    thread->context_esp = (uintptr_t)stack;
#endif
    
    // Add to ready queue
    KeAppendReadyThread(thread);
    
    char name[32];
    // Simple number to string
    char *p = name + 30;
    *p = 0;
    uint32_t n = thread_count++;
    do {
        *--p = '0' + (n % 10);
        n /= 10;
    } while (n > 0);
    
    char thread_name[64] = "Thread";
    strcat(thread_name, p);
    
    HANDLE handle = ObCreateObject(thread_object_type, thread_name, thread, sizeof(THREAD));
    if (handle == INVALID_HANDLE) {
        kfree(thread->stack);
        kfree(thread);
        return INVALID_HANDLE;
    }
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

static THREAD *KeSelectNextThread(void) {
    THREAD *next;
    THREAD *scan;

    if (!current_thread) return 0;

    next = 0;
    scan = current_thread->next ? current_thread->next : ready_queue;
    while (scan) {
        if (scan != current_thread && scan->state == THREAD_READY) {
            next = scan;
            break;
        }
        scan = scan->next;
        if (!scan && current_thread != ready_queue) scan = ready_queue;
        if (scan == current_thread) break;
    }

    if (!next) return 0;

    if (current_thread->state == THREAD_RUNNING) current_thread->state = THREAD_READY;
    current_thread = next;
    current_thread->state = THREAD_RUNNING;

    scheduler_ticks++;
    return current_thread;
}

__attribute__((naked)) void KeYield(void) {
#if defined(__x86_64__)
    __asm__ volatile(
        "movq current_thread(%rip), %rax\n\t"
        "testq %rax, %rax\n\t"
        "jz 2f\n\t"
        "movq %rsp, 48(%rax)\n\t"
        "movq %rbx, 56(%rax)\n\t"
        "movq %rsi, 64(%rax)\n\t"
        "movq %rdi, 72(%rax)\n\t"
        "movq %rbp, 80(%rax)\n\t"
        "movq %r12, 88(%rax)\n\t"
        "movq %r13, 96(%rax)\n\t"
        "movq %r14, 104(%rax)\n\t"
        "movq %r15, 112(%rax)\n\t"
        "subq $8, %rsp\n\t"
        "call KeSelectNextThread\n\t"
        "addq $8, %rsp\n\t"
        "testq %rax, %rax\n\t"
        "jz 2f\n\t"
        "movq 56(%rax), %rbx\n\t"
        "movq 64(%rax), %rsi\n\t"
        "movq 72(%rax), %rdi\n\t"
        "movq 80(%rax), %rbp\n\t"
        "movq 88(%rax), %r12\n\t"
        "movq 96(%rax), %r13\n\t"
        "movq 104(%rax), %r14\n\t"
        "movq 112(%rax), %r15\n\t"
        "movq 48(%rax), %rsp\n\t"
        "ret\n\t"
        "2:\n\t"
        "ret\n\t"
    );
#else
    __asm__ volatile(
        "movl current_thread, %eax\n\t"
        "test %eax, %eax\n\t"
        "jz 2f\n\t"
        "movl %esp, 28(%eax)\n\t"
        "movl %ebx, 32(%eax)\n\t"
        "movl %esi, 36(%eax)\n\t"
        "movl %edi, 40(%eax)\n\t"
        "movl %ebp, 44(%eax)\n\t"
        "call KeSelectNextThread\n\t"
        "test %eax, %eax\n\t"
        "jz 2f\n\t"
        "movl 32(%eax), %ebx\n\t"
        "movl 36(%eax), %esi\n\t"
        "movl 40(%eax), %edi\n\t"
        "movl 44(%eax), %ebp\n\t"
        "movl 28(%eax), %esp\n\t"
        "ret\n\t"
        "2:\n\t"
        "ret\n\t"
    );
#endif
}

void KeStartScheduler(void) {
    if (!ready_queue) {
        HalPutString("[Ke] No threads to schedule!\n", 0x0C);
        return;
    }
    
    current_thread = ready_queue;
    current_thread->state = THREAD_RUNNING;
    
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
    return ObCreateObject(mutex_object_type, "Mutex", mutex, sizeof(MUTEX));
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
    return ObCreateObject(event_object_type, "Event", event, sizeof(EVENT));
}

void KeSetEvent(HANDLE event_handle) {
    EVENT *event = (EVENT*)ObReferenceObject(event_handle);
    if (event) {
        event->signaled = 1;
        ObDereferenceObject(event_handle);
    }
}

void KeResetEvent(HANDLE event_handle) {
    EVENT *event = (EVENT*)ObReferenceObject(event_handle);
    if (event) {
        event->signaled = 0;
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

uint32_t KeGetSchedulerTicks(void) {
    return scheduler_ticks;
}
