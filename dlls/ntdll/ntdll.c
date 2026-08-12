// ntdll.c - NT Native API for NT-like OS
#include <stdint.h>
#include "version.h"

// DLL entry
__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule; (void)lpReserved;
    if (reason == 1) { // DLL_PROCESS_ATTACH
        // Initialize
    }
    return 1;
}

// Kernel imports
extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void SerialPutString(const char *str);
extern void SerialPrintHex(uint32_t val);
extern void SerialPrintDec(uint32_t val);
extern uint32_t strlen(const char *s);
extern void *memset(void *s, int c, uint32_t n);
extern void *memcpy(void *d, const void *s, uint32_t n);
extern int strcmp(const char *a, const char *b);
extern uint32_t KeCreateEvent(uint32_t manual_reset);
extern void KeSetEvent(uint32_t event_handle);
extern void KeResetEvent(uint32_t event_handle);
extern void KeWaitEvent(uint32_t event_handle);
extern uint32_t KeCreateThread(void (*entry)(void *), void *arg, uint32_t stack_size);
extern uint32_t KeGetSchedulerTicks(void);
extern void KeYield(void);
extern int FbGetWidth(void);
extern int FbGetHeight(void);

typedef struct {
    uint16_t Length;
    uint16_t MaximumLength;
    uint16_t *Buffer;
} UNICODE_STRING32;

typedef struct {
    uint32_t PeakVirtualSize;
    uint32_t VirtualSize;
    uint32_t PageFaultCount;
    uint32_t PeakWorkingSetSize;
    uint32_t WorkingSetSize;
    uint32_t QuotaPeakPagedPoolUsage;
    uint32_t QuotaPagedPoolUsage;
    uint32_t QuotaPeakNonPagedPoolUsage;
    uint32_t QuotaNonPagedPoolUsage;
    uint32_t PagefileUsage;
    uint32_t PeakPagefileUsage;
} VM_COUNTERS32;

typedef struct {
    uint64_t ReadOperationCount;
    uint64_t WriteOperationCount;
    uint64_t OtherOperationCount;
    uint64_t ReadTransferCount;
    uint64_t WriteTransferCount;
    uint64_t OtherTransferCount;
} IO_COUNTERS32;

typedef struct {
    uint32_t NextEntryOffset;
    uint32_t NumberOfThreads;
    uint64_t Reserved[3];
    uint64_t CreateTime;
    uint64_t UserTime;
    uint64_t KernelTime;
    UNICODE_STRING32 ProcessName;
    int32_t BasePriority;
    void *UniqueProcessId;
    void *InheritedFromUniqueProcessId;
    uint32_t HandleCount;
    uint32_t SessionId;
    uint32_t UniqueProcessKey;
    VM_COUNTERS32 vmCounters;
    uint32_t PrivatePageCount;
    IO_COUNTERS32 IoCounters;
    uint32_t dwThreadCount;
    int32_t dwBasePriority;
} SYSTEM_PROCESS_INFORMATION32;

typedef struct {
    uint32_t Reserved;
    uint32_t TimerResolution;
    uint32_t MmPageSize;
    uint32_t MmNumberOfPhysicalPages;
    uint32_t LowestPhysicalPageNumber;
    uint32_t HighestPhysicalPageNumber;
    uint32_t AllocationGranularity;
    uint32_t MinimumUserModeAddress;
    uint32_t MaximumUserModeAddress;
    uint32_t ActiveProcessorsAffinityMask;
    uint8_t NumberOfProcessors;
} SYSTEM_BASIC_INFORMATION32;

typedef struct {
    uint64_t BootTime;
    uint64_t SystemTime;
    uint64_t TimeZoneBias;
    uint32_t CurrentTimeZoneId;
    uint32_t Reserved;
} SYSTEM_TIMEOFDAY_INFORMATION32;

typedef struct {
    uint64_t IdleProcessTime;
    uint64_t IdleTime;
    uint64_t IoReadTransferCount;
    uint64_t IoWriteTransferCount;
    uint64_t IoOtherTransferCount;
    uint32_t AvailablePages;
    uint32_t TotalCommittedPages;
    uint32_t TotalCommitLimit;
    uint32_t PeakCommitment;
    uint32_t PagedPoolUsage;
    uint32_t NonPagedPoolUsage;
} SYSTEM_PERFORMANCE_INFORMATION32;

typedef struct {
    uint64_t IdleTime;
    uint64_t KernelTime;
    uint64_t UserTime;
    uint64_t DpcTime;
    uint64_t InterruptTime;
    uint32_t InterruptCount;
    uint64_t Reserved1[2];
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION32;

typedef struct {
    uint32_t Count;
    struct {
        uint16_t UniqueProcessId;
        uint16_t CreatorBackTraceIndex;
        uint8_t ObjectTypeIndex;
        uint8_t HandleAttributes;
        uint16_t HandleValue;
        void *Object;
        uint32_t GrantedAccess;
    } Handles[1];
} SYSTEM_HANDLE_INFORMATION32;

typedef struct {
    uint32_t CurrentSize;
    uint32_t PeakSize;
    uint32_t PageFaultCount;
} SYSTEM_FILECACHE_INFORMATION32;

typedef struct {
    int32_t ExitStatus;
    void *PebBaseAddress;
    uint32_t AffinityMask;
    int32_t BasePriority;
    uint32_t UniqueProcessId;
    uint32_t InheritedFromUniqueProcessId;
} PROCESS_BASIC_INFORMATION32;

typedef struct {
    uint8_t Reserved1[2];
    uint8_t BeingDebugged;
    uint8_t Reserved2[1];
    void *Reserved3[2];
    void *Ldr;
    void *ProcessParameters;
    uint8_t Reserved4[104];
    void *Reserved5[52];
    void *PostProcessInitRoutine;
    uint8_t Reserved6[128];
    uint32_t SessionId;
    uint8_t NumberOfProcessors;
} PEB32;

typedef struct {
    void *Reserved1[12];
    PEB32 *Peb;
} TEB32;

typedef struct {
    uint32_t dwOSVersionInfoSize;
    uint32_t dwMajorVersion;
    uint32_t dwMinorVersion;
    uint32_t dwBuildNumber;
    uint32_t dwPlatformId;
    uint16_t szCSDVersion[128];
} RTL_OSVERSIONINFOW32;

static PEB32 g_peb = { {0}, 0, {0}, {0}, 0, 0, {0}, {0}, 0, {0}, 0, 1 };
static TEB32 g_teb = { {0}, &g_peb };
static uint64_t g_fake_ticks = 0;
static uint16_t g_proc_idle_name[] = {0};
static uint16_t g_proc_taskmgr_name[] = {'T','A','S','K','M','G','R','.','E','X','E',0};
static uint16_t g_proc_cmd_name[] = {'C','M','D','.','E','X','E',0};

extern uint32_t KeGetSchedulerTicks(void);
extern uint32_t MmGetHeapUsed(void);
extern uint32_t MmGetHeapTotal(void);
extern uint32_t KeGetProcessorCount(void);
extern uint32_t KeGetPhysicalMemoryPages(void);

static void ntdll_thread_boot(void *arg) {
    uint32_t (*fn)(void*) = ((uint32_t (**)(void*))arg)[0];
    void *param = ((void**)arg)[1];
    if (fn) fn(param);
    kfree(arg);
    for (;;) KeYield();
}

// Forward declarations
__attribute__((stdcall)) int NtDisplayString(const char *text);
__attribute__((stdcall)) int NtClose(void *handle);
__attribute__((stdcall)) int NtTerminateProcess(void *process, int status);

// === NT Object Manager API ===

__attribute__((stdcall)) int NtCreateEvent(void **event, uint32_t access, void *attr, int type, int state) {
    uint32_t h;
    (void)access;
    (void)attr;
    h = KeCreateEvent(type ? 1 : 0);
    if (state) KeSetEvent(h);
    else KeResetEvent(h);
    if (event) *event = (void*)(uintptr_t)h;
    return 0; // STATUS_SUCCESS
}

__attribute__((stdcall)) int NtOpenEvent(void **event, uint32_t access, void *attr) {
    if (event) *event = (void*)0x1001;
    return 0;
}

__attribute__((stdcall)) int NtSetEvent(void *event, int *previous) {
    if (event) KeSetEvent((uint32_t)(uintptr_t)event);
    if (previous) *previous = 0;
    return 0;
}

__attribute__((stdcall)) int NtResetEvent(void *event, int *previous) {
    if (event) KeResetEvent((uint32_t)(uintptr_t)event);
    if (previous) *previous = 0;
    return 0;
}

__attribute__((stdcall)) int NtPulseEvent(void *event, int *previous) {
    if (previous) *previous = 0;
    return 0;
}

__attribute__((stdcall)) int NtCreateMutant(void **mutant, uint32_t access, void *attr, int owned) {
    if (mutant) *mutant = (void*)0x4000;
    return 0;
}

__attribute__((stdcall)) int NtOpenMutant(void **mutant, uint32_t access, void *attr) {
    if (mutant) *mutant = (void*)0x4001;
    return 0;
}

__attribute__((stdcall)) int NtReleaseMutant(void *mutant, int *previous) {
    if (previous) *previous = 0;
    return 0;
}

__attribute__((stdcall)) int NtCreateSemaphore(void **sem, uint32_t access, void *attr, int initial, int max) {
    if (sem) *sem = (void*)0x3000;
    return 0;
}

__attribute__((stdcall)) int NtReleaseSemaphore(void *sem, int count, int *previous) {
    if (previous) *previous = 0;
    return 0;
}

__attribute__((stdcall)) int NtCreateTimer(void **timer, uint32_t access, void *attr, int type) {
    if (timer) *timer = (void*)0x5000;
    return 0;
}

__attribute__((stdcall)) int NtSetTimer(void *timer, void *due_time, void *timer_apc, 
                                          void *timer_context, int resume, int period, void *previous) {
    return 0;
}

__attribute__((stdcall)) int NtCancelTimer(void *timer, int *previous) {
    if (previous) *previous = 0;
    return 0;
}

// === NT Thread/Process API ===

__attribute__((stdcall)) int NtCreateThread(void **thread, uint32_t access, void *attr, 
                                              void *process, void *client_id, void *context,
                                              void *stack, int suspended) {
    void **ctx;
    uint32_t h;
    (void)access; (void)attr; (void)process; (void)client_id; (void)suspended;
    ctx = (void**)kmalloc(sizeof(void*) * 2);
    if (!ctx) return 0xC0000017;
    ctx[0] = context;
    ctx[1] = stack;
    h = KeCreateThread(ntdll_thread_boot, ctx, 16384);
    if (thread) *thread = (void*)(uintptr_t)h;
    return 0;
}

__attribute__((stdcall)) int NtOpenThread(void **thread, uint32_t access, void *attr, void *client_id) {
    if (thread) *thread = (void*)0x7001;
    return 0;
}

__attribute__((stdcall)) int NtTerminateThread(void *thread, int status) {
    (void)thread; (void)status;
    return 0;
}

__attribute__((stdcall)) int NtSuspendThread(void *thread, uint32_t *count) {
    if (count) *count = 0;
    return 0;
}

__attribute__((stdcall)) int NtResumeThread(void *thread, uint32_t *count) {
    if (count) *count = 0;
    return 0;
}

__attribute__((stdcall)) int NtGetContextThread(void *thread, void *context) {
    (void)thread; (void)context;
    return 0;
}

__attribute__((stdcall)) int NtSetContextThread(void *thread, void *context) {
    (void)thread; (void)context;
    return 0;
}

__attribute__((stdcall)) int NtQueryInformationThread(void *thread, uint32_t class_, void *info, 
                                                         uint32_t len, uint32_t *needed) {
    if (needed) *needed = len;
    return 0;
}

__attribute__((stdcall)) int NtOpenProcess(void **process, uint32_t access, void *attr, void *client_id) {
    if (process) *process = (void*)0x8000;
    return 0;
}

__attribute__((stdcall)) int NtTerminateProcess(void *process, int status) {
    (void)process; (void)status;
    SerialPutString("[NT] Process terminated, halting\r\n");
    while(1) __asm__ volatile("hlt");
    return 0;
}

__attribute__((stdcall)) int NtQueryInformationProcess(void *process, uint32_t class_, void *info,
                                                          uint32_t len, uint32_t *needed) {
    PROCESS_BASIC_INFORMATION32 *pbi = (PROCESS_BASIC_INFORMATION32*)info;
    (void)process;
    if (needed) *needed = sizeof(PROCESS_BASIC_INFORMATION32);
    if (class_ != 0 || !info || len < sizeof(PROCESS_BASIC_INFORMATION32)) return 0xC0000004;
    memset(pbi, 0, sizeof(*pbi));
    pbi->PebBaseAddress = &g_peb;
    pbi->AffinityMask = 1;
    pbi->BasePriority = 8;
    pbi->UniqueProcessId = 1;
    if (needed) *needed = len;
    return 0;
}

__attribute__((stdcall)) int NtYieldExecution(void) {
    __asm__ volatile("pause");
    return 0;
}

__attribute__((stdcall)) int NtDelayExecution(int alertable, void *interval) {
    (void)alertable;
    if (interval) {
        int64_t *delay = (int64_t*)interval; // 100ns units
        if (*delay > 0) {
            uint32_t ms = (uint32_t)(*delay / 10000);
            for (volatile uint32_t i = 0; i < ms * 500; i++) __asm__ volatile("pause");
        }
    }
    return 0;
}

// === NT Wait API ===

__attribute__((stdcall)) int NtWaitForSingleObject(void *handle, int alertable, void *timeout) {
    (void)alertable; (void)timeout;
    if (handle) KeWaitEvent((uint32_t)(uintptr_t)handle);
    return 0;
}

__attribute__((stdcall)) int NtWaitForMultipleObjects(uint32_t count, void **handles, int wait_type,
                                                         int alertable, void *timeout) {
    (void)count; (void)handles; (void)wait_type; (void)alertable; (void)timeout;
    return 0;
}

__attribute__((stdcall)) int NtSignalAndWaitForSingleObject(void *signal, void *wait, 
                                                               int alertable, void *timeout) {
    return 0;
}

// === NT File API ===

__attribute__((stdcall)) int NtCreateFile(void **handle, uint32_t access, void *attr, 
                                            void *io_status, void *alloc_size, uint32_t attrs,
                                            uint32_t share, uint32_t disposition, uint32_t options,
                                            void *ea, uint32_t ea_len) {
    if (handle) *handle = (void*)0x2000;
    if (io_status) {
        *(uint32_t*)io_status = 0;
        *(uint32_t*)((uint8_t*)io_status + 4) = 0;
    }
    return 0;
}

__attribute__((stdcall)) int NtOpenFile(void **handle, uint32_t access, void *attr, 
                                          void *io_status, uint32_t share, uint32_t options) {
    if (handle) *handle = (void*)0x2001;
    if (io_status) {
        *(uint32_t*)io_status = 0;
        *(uint32_t*)((uint8_t*)io_status + 4) = 0;
    }
    return 0;
}

__attribute__((stdcall)) int NtReadFile(void *file, void *event, void *apc, void *apc_ctx, 
                                          void *io_status, void *buf, uint32_t len, 
                                          void *offset, void *key) {
    (void)file; (void)event; (void)apc; (void)apc_ctx; (void)key;
    // Zero the buffer
    if (buf && len > 0) memset(buf, 0, len);
    if (io_status) {
        *(uint32_t*)io_status = 0;
        *(uint32_t*)((uint8_t*)io_status + 4) = 0; // 0 bytes read
    }
    return 0;
}

__attribute__((stdcall)) int NtWriteFile(void *file, void *event, void *apc, void *apc_ctx,
                                           void *io_status, const void *buf, uint32_t len,
                                           void *offset, void *key) {
    (void)file; (void)event; (void)apc; (void)apc_ctx; (void)key;
    if (buf && len > 0) {
        SerialPutString("[NtWriteFile] ");
        char tmp[256];
        uint32_t l = len > 255 ? 255 : len;
        memcpy(tmp, buf, l);
        tmp[l] = 0;
        SerialPutString(tmp);
        SerialPutString("\r\n");
    }
    if (io_status) {
        *(uint32_t*)io_status = 0;
        *(uint32_t*)((uint8_t*)io_status + 4) = len;
    }
    return 0;
}

__attribute__((stdcall)) int NtQueryInformationFile(void *file, void *io_status, 
                                                       void *info, uint32_t len, uint32_t class_) {
    if (io_status) {
        *(uint32_t*)io_status = 0;
        *(uint32_t*)((uint8_t*)io_status + 4) = 0;
    }
    return 0xC0000002; // STATUS_NOT_IMPLEMENTED
}

__attribute__((stdcall)) int NtSetInformationFile(void *file, void *io_status, 
                                                    void *info, uint32_t len, uint32_t class_) {
    return 0;
}

__attribute__((stdcall)) int NtQueryVolumeInformationFile(void *file, void *io_status,
                                                             void *info, uint32_t len, uint32_t class_) {
    return 0xC0000002;
}

__attribute__((stdcall)) int NtQueryDirectoryFile(void *file, void *event, void *apc, void *apc_ctx,
                                                     void *io_status, void *buf, uint32_t len,
                                                     uint32_t class_, int single, void *filter,
                                                     int restart) {
    if (io_status) {
        *(uint32_t*)io_status = 0;
        *(uint32_t*)((uint8_t*)io_status + 4) = 0;
    }
    return 0xC0000002;
}

__attribute__((stdcall)) int NtCancelIoFile(void *file, void *io_status) {
    return 0;
}

__attribute__((stdcall)) int NtFlushBuffersFile(void *file, void *io_status) {
    return 0;
}

__attribute__((stdcall)) int NtLockFile(void *file, void *event, void *apc, void *apc_ctx,
                                          void *io_status, void *offset, uint64_t len,
                                          uint32_t key, int fail, int exclusive) {
    return 0;
}

__attribute__((stdcall)) int NtUnlockFile(void *file, void *io_status, void *offset, 
                                            uint64_t len, uint32_t key) {
    return 0;
}

__attribute__((stdcall)) int NtDeviceIoControlFile(void *file, void *event, void *apc, void *apc_ctx,
                                                      void *io_status, uint32_t code, void *in, 
                                                      uint32_t in_len, void *out, uint32_t out_len) {
    return 0xC0000002;
}

__attribute__((stdcall)) int NtFsControlFile(void *file, void *event, void *apc, void *apc_ctx,
                                               void *io_status, uint32_t code, void *in, 
                                               uint32_t in_len, void *out, uint32_t out_len) {
    return 0xC0000002;
}

// === NT Section/Memory API ===

__attribute__((stdcall)) int NtCreateSection(void **section, uint32_t access, void *attr, 
                                               void *size, uint32_t protect, uint32_t alloc, void *file) {
    if (section) *section = (void*)0x6000;
    return 0;
}

__attribute__((stdcall)) int NtOpenSection(void **section, uint32_t access, void *attr) {
    if (section) *section = (void*)0x6001;
    return 0;
}

__attribute__((stdcall)) int NtMapViewOfSection(void *section, void *process, void **base, 
                                                   uint32_t zero, uint32_t commit, void *offset, 
                                                   uint32_t *view_size, uint32_t inherit, 
                                                   uint32_t alloc_type, uint32_t protect) {
    if (base) *base = kmalloc(view_size ? *view_size : 0x10000);
    return 0;
}

__attribute__((stdcall)) int NtUnmapViewOfSection(void *process, void *base) {
    (void)process;
    if (base) kfree(base);
    return 0;
}

__attribute__((stdcall)) int NtAllocateVirtualMemory(void *process, void **base, uint32_t zero,
                                                        uint32_t *size, uint32_t alloc_type, uint32_t protect) {
    if (base && size) *base = kmalloc(*size);
    return 0;
}

__attribute__((stdcall)) int NtFreeVirtualMemory(void *process, void **base, uint32_t *size, uint32_t free_type) {
    if (base && *base) kfree(*base);
    return 0;
}

__attribute__((stdcall)) int NtProtectVirtualMemory(void *process, void **base, uint32_t *size,
                                                       uint32_t new_protect, uint32_t *old_protect) {
    if (old_protect) *old_protect = 0x04; // PAGE_READWRITE
    return 0;
}

__attribute__((stdcall)) int NtQueryVirtualMemory(void *process, void *base, uint32_t class_,
                                                     void *info, uint32_t len, uint32_t *needed) {
    if (needed) *needed = len;
    return 0;
}

// === NT Registry API ===

__attribute__((stdcall)) int NtCreateKey(void **key, uint32_t access, void *attr, uint32_t index,
                                           void *name, uint32_t options, uint32_t *disposition) {
    if (key) *key = (void*)0x9000;
    if (disposition) *disposition = 1; // REG_CREATED_NEW_KEY
    return 0;
}

__attribute__((stdcall)) int NtOpenKey(void **key, uint32_t access, void *attr) {
    if (key) *key = (void*)0x9001;
    return 0;
}

__attribute__((stdcall)) int NtDeleteKey(void *key) {
    return 0;
}

__attribute__((stdcall)) int NtDeleteValueKey(void *key, void *name) {
    return 0;
}

__attribute__((stdcall)) int NtQueryKey(void *key, uint32_t class_, void *info, uint32_t len, uint32_t *needed) {
    if (needed) *needed = len;
    return 0;
}

__attribute__((stdcall)) int NtSetValueKey(void *key, void *name, uint32_t index, 
                                             uint32_t type, void *data, uint32_t len) {
    return 0;
}

__attribute__((stdcall)) int NtQueryValueKey(void *key, void *name, uint32_t class_, 
                                               void *info, uint32_t len, uint32_t *result_len) {
    if (result_len) *result_len = 0;
    return 0xC0000034; // STATUS_OBJECT_NAME_NOT_FOUND
}

__attribute__((stdcall)) int NtEnumerateKey(void *key, uint32_t index, uint32_t class_,
                                              void *name, uint32_t len, uint32_t *needed) {
    if (needed) *needed = 0;
    return 0x8000001A; // STATUS_NO_MORE_ENTRIES
}

__attribute__((stdcall)) int NtEnumerateValueKey(void *key, uint32_t index, uint32_t class_,
                                                    void *name, uint32_t len, uint32_t *needed) {
    if (needed) *needed = 0;
    return 0x8000001A;
}

__attribute__((stdcall)) int NtInitializeRegistry(uint32_t setup) {
    return 0;
}

__attribute__((stdcall)) int NtNotifyChangeKey(void *key, void *event, void *apc, void *apc_ctx,
                                                  void *io_status, uint32_t filter, int watch,
                                                  void *buf, uint32_t len, int async) {
    return 0;
}

// === NT System API ===

__attribute__((stdcall)) int NtQuerySystemInformation(uint32_t class_, void *info, uint32_t len, uint32_t *needed) {
    uint32_t heap_used = MmGetHeapUsed();
    uint32_t heap_total = MmGetHeapTotal();
    uint32_t committed_pages;
    uint32_t available_pages;
    uint32_t scheduler_ticks = KeGetSchedulerTicks();
    uint32_t physical_pages = KeGetPhysicalMemoryPages();
    uint32_t processors = KeGetProcessorCount();
    uint32_t phase;
    SerialPutString("[NTDLL] NtQuerySystemInformation class=");
    SerialPrintDec(class_);
    SerialPutString("\r\n");
    /* Use the scheduler clock as the time base.  This remains monotonic in
     * the cooperative kernel, unlike the old fixed 50/50 synthetic sample. */
    g_fake_ticks += 100000 + (uint64_t)(scheduler_ticks & 0x3FFu) * 16u;
    phase = (scheduler_ticks / 8u) % 20u;
    committed_pages = 256u + (heap_used / 4096u);
    if (!physical_pages) physical_pages = 16384u;
    if (committed_pages > physical_pages) committed_pages = physical_pages;
    available_pages = physical_pages - committed_pages;
    if (class_ == 0 && info && len >= sizeof(SYSTEM_BASIC_INFORMATION32)) {
        SYSTEM_BASIC_INFORMATION32 *sbi = (SYSTEM_BASIC_INFORMATION32*)info;
        memset(sbi, 0, sizeof(*sbi));
        sbi->TimerResolution = 10000;
        sbi->MmPageSize = 4096;
        sbi->MmNumberOfPhysicalPages = physical_pages;
        sbi->HighestPhysicalPageNumber = physical_pages - 1;
        sbi->AllocationGranularity = 4096;
        sbi->MinimumUserModeAddress = 0x1000;
        sbi->MaximumUserModeAddress = 0x7FFFFFFF;
        sbi->ActiveProcessorsAffinityMask = processors >= 32 ? 0xFFFFFFFFU : ((1U << (processors ? processors : 1)) - 1U);
        sbi->NumberOfProcessors = (uint8_t)(processors ? processors : 1);
        if (needed) *needed = sizeof(*sbi);
        SerialPutString("[NTDLL] NtQuerySystemInformation basic ok\r\n");
        return 0;
    }
    if (class_ == 3 && info && len >= sizeof(SYSTEM_TIMEOFDAY_INFORMATION32)) {
        SYSTEM_TIMEOFDAY_INFORMATION32 *tod = (SYSTEM_TIMEOFDAY_INFORMATION32*)info;
        memset(tod, 0, sizeof(*tod));
        tod->SystemTime = g_fake_ticks;
        if (needed) *needed = sizeof(*tod);
        return 0;
    }
    if (class_ == 2 && info && len >= sizeof(SYSTEM_PERFORMANCE_INFORMATION32)) {
        SYSTEM_PERFORMANCE_INFORMATION32 *spi = (SYSTEM_PERFORMANCE_INFORMATION32*)info;
        memset(spi, 0, sizeof(*spi));
        spi->IdleTime = g_fake_ticks / 2;
        spi->IdleProcessTime = g_fake_ticks / 2;
        spi->AvailablePages = available_pages;
        spi->TotalCommittedPages = committed_pages;
        spi->TotalCommitLimit = 16384;
        spi->PeakCommitment = committed_pages + 128;
        spi->PagedPoolUsage = heap_used / 4096u;
        spi->NonPagedPoolUsage = (heap_used / 8192u) + 1;
        if (needed) *needed = sizeof(*spi);
        return 0;
    }
    if (class_ == 21 && info && len >= sizeof(SYSTEM_FILECACHE_INFORMATION32)) {
        SYSTEM_FILECACHE_INFORMATION32 *fc = (SYSTEM_FILECACHE_INFORMATION32*)info;
        memset(fc, 0, sizeof(*fc));
        fc->CurrentSize = 256;
        fc->PeakSize = 512;
        if (needed) *needed = sizeof(*fc);
        return 0;
    }
    if (class_ == 8 && info && len >= sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION32)) {
        SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION32 *pp = (SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION32*)info;
        memset(pp, 0, sizeof(*pp));
        /* No hardware idle counter is exposed by the current scheduler yet;
         * use scheduler activity to produce a bounded live sample rather
         * than reporting a permanently fixed 50 percent. */
        pp->IdleTime = (g_fake_ticks * (80u + phase)) / 100u;
        pp->KernelTime = (g_fake_ticks * (10u + phase / 4u)) / 100u;
        pp->UserTime = g_fake_ticks - pp->IdleTime - pp->KernelTime;
        if (needed) *needed = sizeof(*pp);
        return 0;
    }
    if (class_ == 16 && info && len >= sizeof(SYSTEM_HANDLE_INFORMATION32)) {
        SYSTEM_HANDLE_INFORMATION32 *hi = (SYSTEM_HANDLE_INFORMATION32*)info;
        memset(hi, 0, sizeof(*hi));
        hi->Count = 1;
        hi->Handles[0].UniqueProcessId = 1;
        hi->Handles[0].HandleValue = 1;
        if (needed) *needed = sizeof(*hi);
        return 0;
    }
    if (class_ == 5 && info && len >= (sizeof(SYSTEM_PROCESS_INFORMATION32) * 3)) {
        SYSTEM_PROCESS_INFORMATION32 *p = (SYSTEM_PROCESS_INFORMATION32*)info;
        SYSTEM_PROCESS_INFORMATION32 *p2 = (SYSTEM_PROCESS_INFORMATION32*)((uint8_t*)info + sizeof(SYSTEM_PROCESS_INFORMATION32));
        SYSTEM_PROCESS_INFORMATION32 *p3 = (SYSTEM_PROCESS_INFORMATION32*)((uint8_t*)info + (sizeof(SYSTEM_PROCESS_INFORMATION32) * 2));
        memset(info, 0, sizeof(SYSTEM_PROCESS_INFORMATION32) * 3);
        p->NextEntryOffset = sizeof(SYSTEM_PROCESS_INFORMATION32);
        p->ProcessName.Buffer = g_proc_idle_name;
        p->ProcessName.Length = 0;
        p->ProcessName.MaximumLength = 0;
        p->UniqueProcessId = 0;
        p->HandleCount = 0;
        p->dwThreadCount = p->NumberOfThreads = 1;
        p->dwBasePriority = p->BasePriority = 0;
        p->KernelTime = g_fake_ticks / 2;

        p2->NextEntryOffset = sizeof(SYSTEM_PROCESS_INFORMATION32);
        p2->ProcessName.Buffer = g_proc_cmd_name;
        p2->ProcessName.Length = 7 * 2;
        p2->ProcessName.MaximumLength = 8 * 2;
        p2->UniqueProcessId = (void*)2;
        p2->HandleCount = 8;
        p2->dwThreadCount = p2->NumberOfThreads = 1;
        p2->dwBasePriority = p2->BasePriority = 8;
        p2->vmCounters.WorkingSetSize = 256 * 1024;
        p2->vmCounters.PeakWorkingSetSize = 512 * 1024;
        p2->vmCounters.VirtualSize = 1024 * 1024;
        p2->KernelTime = g_fake_ticks / 4;
        p2->UserTime = g_fake_ticks / 4;

        p3->NextEntryOffset = 0;
        p3->ProcessName.Buffer = g_proc_taskmgr_name;
        p3->ProcessName.Length = 11 * 2;
        p3->ProcessName.MaximumLength = 12 * 2;
        p3->UniqueProcessId = (void*)3;
        p3->HandleCount = 12;
        p3->dwThreadCount = p3->NumberOfThreads = 4;
        p3->dwBasePriority = p3->BasePriority = 8;
        p3->vmCounters.WorkingSetSize = 1024 * 1024;
        p3->vmCounters.PeakWorkingSetSize = 2 * 1024 * 1024;
        p3->vmCounters.VirtualSize = 4 * 1024 * 1024;
        p3->KernelTime = g_fake_ticks / 3;
        p3->UserTime = g_fake_ticks / 3;
        if (needed) *needed = sizeof(SYSTEM_PROCESS_INFORMATION32) * 3;
        return 0;
    }
    if (needed) *needed = 0;
    if (info && len) memset(info, 0, len);
    return 0xC0000002;
}

__attribute__((stdcall)) int RtlGetVersion(void *info) {
    RTL_OSVERSIONINFOW32 *ver = (RTL_OSVERSIONINFOW32*)info;
    uint32_t i;

    if (!ver) return 0xC000000D;
    ver->dwMajorVersion = DISCOUNT_WIN32_MAJOR;
    ver->dwMinorVersion = DISCOUNT_WIN32_MINOR;
    ver->dwBuildNumber = DISCOUNT_WIN32_BUILD;
    ver->dwPlatformId = 2;
    for (i = 0; i < (sizeof(ver->szCSDVersion) / sizeof(ver->szCSDVersion[0])); i++) {
        ver->szCSDVersion[i] = 0;
    }
    return 0;
}

__attribute__((stdcall)) int NtSetSystemInformation(uint32_t class_, void *info, uint32_t len) {
    return 0;
}

__attribute__((stdcall)) int NtQuerySystemTime(uint64_t *time) {
    if (time) *time = 0;
    return 0;
}

__attribute__((stdcall)) int NtSetSystemTime(void *time, void *previous) {
    return 0;
}

__attribute__((stdcall)) int NtQueryPerformanceCounter(uint64_t *counter, uint64_t *frequency) {
    if (counter) *counter = 0;
    if (frequency) *frequency = 1000000;
    return 0;
}

__attribute__((stdcall)) int NtGetTickCount(uint64_t *count) {
    if (count) *count = (uint64_t)KeGetSchedulerTicks() * 16ULL;
    return 0;
}

__attribute__((stdcall)) int NtShutdownSystem(uint32_t action) {
    SerialPutString("[NT] System shutdown requested\r\n");
    return 0;
}

__attribute__((stdcall)) int NtDisplayString(const char *text) {
    if (text) SerialPutString(text);
    return 0;
}

__attribute__((stdcall)) int NtRaiseHardError(uint32_t status, uint32_t num_params, uint32_t flags,
                                                void *params, uint32_t response, uint32_t *resp) {
    SerialPutString("[NT] Hard error: 0x");
    SerialPrintHex(status);
    SerialPutString("\r\n");
    if (resp) *resp = 0; // OptionOk
    return 0;
}

__attribute__((stdcall)) int NtRaiseException(void *exception_record, void *context, int first_chance) {
    SerialPutString("[NT] Exception raised\r\n");
    return 0;
}

__attribute__((stdcall)) int NtContinue(void *context, int test_alert) {
    return 0;
}

// === NT Security API ===

__attribute__((stdcall)) int NtAccessCheck(void *security_descriptor, void *token, uint32_t access,
                                             void *generic_mapping, void *privileges, uint32_t *priv_len,
                                             uint32_t *granted, int *status) {
    if (granted) *granted = access;
    if (status) *status = 0;
    return 0;
}

__attribute__((stdcall)) int NtOpenProcessToken(void *process, uint32_t access, void **token) {
    if (token) *token = (void*)0xA000;
    return 0;
}

__attribute__((stdcall)) int NtOpenThreadToken(void *thread, uint32_t access, int open_as_self, void **token) {
    if (token) *token = (void*)0xA001;
    return 0;
}

__attribute__((stdcall)) int NtAdjustPrivilegesToken(void *token, int disable_all, void *new_state,
                                                        uint32_t len, void *previous, uint32_t *prev_len) {
    return 0;
}

__attribute__((stdcall)) int NtQuerySecurityObject(void *handle, uint32_t info_class, void *info,
                                                      uint32_t len, uint32_t *needed) {
    if (needed) *needed = len;
    return 0;
}

// === NT Misc API ===

__attribute__((stdcall)) int NtClose(void *handle) {
    (void)handle;
    return 0;
}

__attribute__((stdcall)) void *NtCurrentTeb(void) {
    return &g_teb;
}

__attribute__((stdcall)) int NtDuplicateObject(void *source_process, void *source_handle,
                                                  void *target_process, void **target_handle,
                                                  uint32_t access, uint32_t attrs, uint32_t options) {
    if (target_handle) *target_handle = source_handle;
    return 0;
}

__attribute__((stdcall)) int NtQueryObject(void *handle, uint32_t class_, void *info, uint32_t len,
                                             uint32_t *needed) {
    if (needed) *needed = len;
    return 0;
}

__attribute__((stdcall)) int NtPlugPlayControl(uint32_t class_, void *data, uint32_t len) {
    return 0;
}

__attribute__((stdcall)) int NtGetPlugPlayEvent(uint32_t reserved, uint32_t event_type, 
                                                  void *buffer, uint32_t len, uint32_t *actual_len) {
    if (actual_len) *actual_len = 0;
    return 0xC0000010;
}

__attribute__((stdcall)) int NtPowerInformation(uint32_t class_, void *in, uint32_t in_len,
                                                   void *out, uint32_t out_len) {
    return 0;
}

// === RTL Functions ===

__attribute__((stdcall)) void *RtlAllocateHeap(void *heap, uint32_t flags, uint32_t size) {
    (void)heap; (void)flags;
    return kmalloc(size);
}

__attribute__((stdcall)) int RtlFreeHeap(void *heap, uint32_t flags, void *ptr) {
    (void)heap; (void)flags;
    if (ptr) kfree(ptr);
    return 1;
}

__attribute__((stdcall)) void *RtlReAllocateHeap(void *heap, uint32_t flags, void *ptr, uint32_t size) {
    (void)heap; (void)flags;
    void *new_ptr = kmalloc(size);
    if (ptr && new_ptr) {
        memcpy(new_ptr, ptr, size);
        kfree(ptr);
    }
    return new_ptr;
}

__attribute__((stdcall)) uint32_t RtlSizeHeap(void *heap, uint32_t flags, void *ptr) {
    (void)heap; (void)flags; (void)ptr;
    return 0; // Can't determine size with simple allocator
}

__attribute__((stdcall)) void *RtlCreateHeap(uint32_t flags, void *base, uint32_t reserve, 
                                               uint32_t commit, void *lock, void *params) {
    return (void*)1;
}

__attribute__((stdcall)) void *RtlDestroyHeap(void *heap) {
    return 0;
}

__attribute__((stdcall)) int RtlLockHeap(void *heap) { return 1; }
__attribute__((stdcall)) int RtlUnlockHeap(void *heap) { return 1; }
__attribute__((stdcall)) int RtlCompactHeap(void *heap, uint32_t flags) { return 0; }
__attribute__((stdcall)) int RtlValidateHeap(void *heap, uint32_t flags, void *ptr) { return 1; }

// String functions
__attribute__((stdcall)) void RtlInitAnsiString(void *dst, const char *src) {
    if (!dst) return;
    uint16_t len = src ? strlen(src) : 0;
    if (len > 65534) len = 65534;
    *(uint16_t*)dst = len;
    *(uint16_t*)((uint8_t*)dst + 2) = len;
    *(uint32_t*)((uint8_t*)dst + 4) = (uint32_t)src;
}

__attribute__((stdcall)) void RtlInitUnicodeString(void *dst, const void *src) {
    if (!dst) return;
    uint16_t *wstr = (uint16_t*)src;
    uint16_t len = 0;
    if (wstr) while (wstr[len]) len++;
    len *= 2;
    if (len > 65534) len = 65534;
    *(uint16_t*)dst = len;
    *(uint16_t*)((uint8_t*)dst + 2) = len;
    *(uint32_t*)((uint8_t*)dst + 4) = (uint32_t)src;
}

__attribute__((stdcall)) void RtlFreeUnicodeString(void *str) { (void)str; }
__attribute__((stdcall)) int RtlCreateUnicodeStringFromAsciiz(void *dst, const char *src) {
    RtlInitAnsiString(dst, src);
    return 0;
}

__attribute__((stdcall)) int RtlAnsiStringToUnicodeString(void *dst, void *src, int allocate) {
    (void)dst; (void)src; (void)allocate;
    return 0;
}

__attribute__((stdcall)) int RtlUnicodeStringToAnsiString(void *dst, void *src, int allocate) {
    (void)dst; (void)src; (void)allocate;
    return 0;
}

__attribute__((stdcall)) int RtlUnicodeStringToOemString(void *dst, void *src, int allocate) {
    return 0;
}

__attribute__((stdcall)) uint32_t RtlxUnicodeStringToOemSize(void *str) { return 0; }
__attribute__((stdcall)) uint32_t RtlxAnsiStringToUnicodeSize(void *str) { return 0; }

__attribute__((stdcall)) int RtlMultiByteToUnicodeN(void *dst, uint32_t dst_len, uint32_t *written,
                                                       const char *src, uint32_t src_len) {
    if (written) *written = 0;
    return 0;
}

__attribute__((stdcall)) int RtlUnicodeToMultiByteN(void *dst, uint32_t dst_len, uint32_t *written,
                                                       const void *src, uint32_t src_len) {
    if (written) *written = 0;
    return 0;
}

__attribute__((stdcall)) int RtlCompareString(void *locale, const void *str1, const void *str2, int case_insensitive) {
    return 0; // Equal
}

__attribute__((stdcall)) int RtlCompareUnicodeString(void *str1, void *str2, int case_insensitive) {
    return 0; // Equal
}

__attribute__((stdcall)) int RtlEqualUnicodeString(void *str1, void *str2, int case_insensitive) {
    return 1; // TRUE
}

__attribute__((stdcall)) int RtlPrefixUnicodeString(void *str1, void *str2, int case_insensitive) {
    return 1; // TRUE
}

__attribute__((stdcall)) int RtlIsTextUnicode(void *buf, int len, int *flags) {
    if (flags) *flags = 0;
    return 0;
}

__attribute__((stdcall)) void RtlCopyUnicodeString(void *dst, void *src) {
    if (dst && src) {
        uint16_t len = *(uint16_t*)src;
        *(uint16_t*)dst = len;
        *(uint16_t*)((uint8_t*)dst + 2) = len;
        *(uint32_t*)((uint8_t*)dst + 4) = *(uint32_t*)((uint8_t*)src + 4);
    }
}

__attribute__((stdcall)) int RtlAppendUnicodeStringToString(void *dst, void *src) { return 0; }
__attribute__((stdcall)) int RtlAppendUnicodeToString(void *dst, const void *src) { return 0; }
__attribute__((stdcall)) int RtlIntegerToUnicodeString(uint32_t val, int base, void *str) { return 0; }
__attribute__((stdcall)) int RtlUnicodeStringToInteger(void *str, int base, uint32_t *val) { 
    if (val) *val = 0;
    return 0;
}

__attribute__((stdcall)) void RtlMoveMemory(void *dst, const void *src, uint32_t len) {
    memcpy(dst, src, len);
}

__attribute__((stdcall)) void RtlFillMemory(void *dst, uint32_t len, uint8_t val) {
    memset(dst, val, len);
}

__attribute__((stdcall)) void RtlZeroMemory(void *dst, uint32_t len) {
    memset(dst, 0, len);
}

__attribute__((stdcall)) int RtlCompareMemory(const void *a, const void *b, uint32_t len) {
    for (uint32_t i = 0; i < len; i++)
        if (((uint8_t*)a)[i] != ((uint8_t*)b)[i]) return i;
    return len;
}

__attribute__((stdcall)) void RtlAssert(void *failed, void *file, uint32_t line, void *msg) {
    SerialPutString("[RTL] ASSERT: ");
    if (msg) SerialPutString((const char*)msg);
    SerialPutString("\r\n");
}

__attribute__((stdcall)) int RtlAdjustPrivilege(uint32_t priv, int enable, int current, int *was) {
    if (was) *was = 0;
    return 0;
}

__attribute__((stdcall)) int RtlTimeToTimeFields(void *time, void *fields) { return 0; }
__attribute__((stdcall)) int RtlTimeFieldsToTime(void *fields, void *time) { return 0; }
__attribute__((stdcall)) int RtlNormalizeProcessParams(void *params) { return 0; }

__attribute__((stdcall)) int RtlCreateUserThread(void *process, void *security, int suspended,
                                                    void *stack_zero, void *stack_commit,
                                                    void *start, void *param, void **thread, 
                                                    void *client_id) {
    if (thread) *thread = (void*)0x6000;
    return 0;
}

__attribute__((stdcall)) void DbgPrint(const char *fmt, ...) {
    SerialPutString("[Dbg] ");
    SerialPutString(fmt);
    SerialPutString("\r\n");
}

__attribute__((stdcall)) void DbgBreakPoint(void) {
    SerialPutString("[Dbg] Breakpoint\r\n");
}

__attribute__((stdcall)) int NlsMbOemCodePageTag(void) { return 0; }
__attribute__((stdcall)) int NlsMbCodePageTag(void) { return 0; }

__attribute__((stdcall)) void KiUserExceptionDispatcher(void) {
    SerialPutString("[NT] Exception dispatch\r\n");
}

__attribute__((stdcall)) void KiUserCallbackDispatcher(void) {
    SerialPutString("[NT] Callback dispatch\r\n");
}

__attribute__((stdcall)) void KiUserApcDispatcher(void) {
    SerialPutString("[NT] APC dispatch\r\n");
}
