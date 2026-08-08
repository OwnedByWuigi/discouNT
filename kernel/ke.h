#ifndef KE_H
#define KE_H
#include <stdint.h>
#include "object.h"

typedef enum {
    THREAD_READY = 0,
    THREAD_RUNNING,
    THREAD_WAITING,
    THREAD_TERMINATED
} THREAD_STATE;

typedef struct _THREAD {
    HANDLE handle;
    THREAD_STATE state;
    uint32_t priority;
    void *stack;
    uint32_t stack_size;
    uint32_t entry_point;    // Initial EIP
    uint32_t context_esp;    // Saved ESP
    uint32_t wait_handle;
    struct _THREAD *next;
} THREAD;

typedef struct _PROCESS {
    char name[MAX_NAME_LEN];
    HANDLE handle_table[32]; // Process-specific handle table
    uint32_t handle_count;
    THREAD *main_thread;
    THREAD *thread_list;
} PROCESS;

typedef struct _MUTEX {
    uint32_t locked;
    THREAD *owner;
} MUTEX;

typedef struct _EVENT {
    uint32_t signaled;
    uint32_t manual_reset;
} EVENT;

// Public API
void KeInit(void);
HANDLE KeCreateThread(void (*entry)(void), uint32_t stack_size);
HANDLE KeCreateProcess(const char *name);
void KeTerminateThread(HANDLE thread_handle);
void KeYield(void);
HANDLE KeCreateMutex(void);
void KeWaitMutex(HANDLE mutex_handle);
void KeReleaseMutex(HANDLE mutex_handle);
HANDLE KeCreateEvent(uint32_t manual_reset);
void KeSetEvent(HANDLE event_handle);
void KeWaitEvent(HANDLE event_handle);
void KeStartScheduler(void);
#endif