// ntdll.c - NT Native API for NT-like OS
#include <stdint.h>

// DLL entry
__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule; (void)lpReserved;
    if (reason == 1) { // DLL_PROCESS_ATTACH
        // Initialize
    }
    return 1;
}

// Kernel imports
extern void HalPutString(const char *str, uint8_t color);
extern void HalPutChar(char c, uint8_t color);
extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void SerialPutString(const char *str);
extern void SerialPrintHex(uint32_t val);
extern void SerialPrintDec(uint32_t val);
extern uint32_t strlen(const char *s);
extern void *memset(void *s, int c, uint32_t n);
extern void *memcpy(void *d, const void *s, uint32_t n);
extern int strcmp(const char *a, const char *b);

// Forward declarations
__attribute__((stdcall)) int NtDisplayString(const char *text);
__attribute__((stdcall)) int NtClose(void *handle);
__attribute__((stdcall)) int NtTerminateProcess(void *process, int status);

// === NT Object Manager API ===

__attribute__((stdcall)) int NtCreateEvent(void **event, uint32_t access, void *attr, int type, int state) {
    if (event) *event = (void*)0x1000;
    return 0; // STATUS_SUCCESS
}

__attribute__((stdcall)) int NtOpenEvent(void **event, uint32_t access, void *attr) {
    if (event) *event = (void*)0x1001;
    return 0;
}

__attribute__((stdcall)) int NtSetEvent(void *event, int *previous) {
    if (previous) *previous = 0;
    return 0;
}

__attribute__((stdcall)) int NtResetEvent(void *event, int *previous) {
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
    if (thread) *thread = (void*)0x7000;
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
    (void)handle; (void)alertable; (void)timeout;
    return 0; // STATUS_SUCCESS (pretend object is signaled)
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
    if (needed) *needed = 0;
    if (info && len > 0) memset(info, 0, len);
    return 0xC0000002; // STATUS_NOT_IMPLEMENTED
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
    if (count) *count = 0;
    return 0;
}

__attribute__((stdcall)) int NtShutdownSystem(uint32_t action) {
    SerialPutString("[NT] System shutdown requested\r\n");
    return 0;
}

__attribute__((stdcall)) int NtDisplayString(const char *text) {
    HalPutString(text, 0x0F);
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