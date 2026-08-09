// ntdll.c - NT Native API implementation for NT-like OS
#include <stdint.h>
#include <stdarg.h>

// DLL entry
__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule; (void)lpReserved;
    return 1;
}

// Kernel function imports (these exist in our kernel)
extern void HalPutString(const char *str, uint8_t color);
extern void HalPutChar(char c, uint8_t color);
extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void SerialPutString(const char *str);
extern uint32_t strlen(const char *s);
extern void *memset(void *s, int c, uint32_t n);
extern void *memcpy(void *d, const void *s, uint32_t n);

// === NT API Functions ===

__attribute__((stdcall)) int NtDisplayString(const char *text) {
    HalPutString(text, 0x0F);
    return 0;
}

__attribute__((stdcall)) int NtClose(void *handle) {
    (void)handle;
    return 0;
}

__attribute__((stdcall)) int NtWaitForSingleObject(void *handle, int alertable, void *timeout) {
    (void)handle; (void)alertable; (void)timeout;
    return 0;
}

__attribute__((stdcall)) int NtTerminateProcess(void *process, int status) {
    (void)process;
    SerialPutString("[NT] NtTerminateProcess called\r\n");
    while(1) __asm__ volatile("hlt");
    return 0;
}

__attribute__((stdcall)) int NtTerminateThread(void *thread, int status) {
    (void)thread; (void)status;
    return 0;
}

__attribute__((stdcall)) int NtCreateEvent(void **event, uint32_t access, void *attr, int type, int state) {
    if (event) *event = (void*)0x1000; // Fake handle
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

__attribute__((stdcall)) int NtDelayExecution(int alertable, void *interval) {
    (void)alertable;
    // interval is LARGE_INTEGER in 100ns units
    int64_t *delay = (int64_t*)interval;
    if (delay && *delay > 0) {
        uint32_t ms = (uint32_t)(*delay / 10000);
        for (volatile uint32_t i = 0; i < ms * 1000; i++) __asm__ volatile("");
    }
    return 0;
}

__attribute__((stdcall)) int NtGetTickCount(uint64_t *count) {
    if (count) *count = 0;
    return 0;
}

__attribute__((stdcall)) int NtQuerySystemTime(uint64_t *time) {
    if (time) *time = 0;
    return 0;
}

__attribute__((stdcall)) int NtQuerySystemInformation(uint32_t info_class, void *info, uint32_t size, uint32_t *needed) {
    (void)info_class; (void)info; (void)size;
    if (needed) *needed = 0;
    return 0xC0000002; // STATUS_NOT_IMPLEMENTED
}

__attribute__((stdcall)) int NtReadFile(void *file, void *event, void *apc, void *apc_ctx, 
                                          void *io_status, void *buf, uint32_t len, 
                                          void *offset, void *key) {
    (void)file; (void)event; (void)apc; (void)apc_ctx;
    (void)io_status; (void)buf; (void)len; (void)offset; (void)key;
    return 0;
}

__attribute__((stdcall)) int NtWriteFile(void *file, void *event, void *apc, void *apc_ctx,
                                           void *io_status, const void *buf, uint32_t len,
                                           void *offset, void *key) {
    (void)file; (void)event; (void)apc; (void)apc_ctx; (void)key;
    // Write to serial for debugging
    if (buf && len > 0 && len < 1024) {
        char tmp[1024];
        memcpy(tmp, buf, len);
        tmp[len] = 0;
        SerialPutString(tmp);
    }
    if (io_status) {
        // IO_STATUS_BLOCK: status (4), information (4)
        *(uint32_t*)io_status = 0;
        *(uint32_t*)((uint8_t*)io_status + 4) = len;
    }
    return 0;
}

__attribute__((stdcall)) int NtCreateFile(void **handle, uint32_t access, void *attr, 
                                            void *io_status, void *alloc_size, uint32_t attrs,
                                            uint32_t share, uint32_t disposition, uint32_t options,
                                            void *ea, uint32_t ea_len) {
    if (handle) *handle = (void*)0x2000;
    return 0;
}

__attribute__((stdcall)) int NtOpenFile(void **handle, uint32_t access, void *attr, 
                                          void *io_status, uint32_t share, uint32_t options) {
    if (handle) *handle = (void*)0x2001;
    return 0;
}

__attribute__((stdcall)) int NtQueryInformationFile(void *file, void *io_status, 
                                                       void *info, uint32_t len, uint32_t class_) {
    (void)file; (void)io_status; (void)info; (void)len; (void)class_;
    return 0xC0000002;
}

__attribute__((stdcall)) int NtSetInformationFile(void *file, void *io_status, 
                                                    void *info, uint32_t len, uint32_t class_) {
    (void)file; (void)io_status; (void)info; (void)len; (void)class_;
    return 0;
}

__attribute__((stdcall)) int NtDeviceIoControlFile(void *file, void *event, void *apc, void *apc_ctx,
                                                      void *io_status, uint32_t code, void *in, 
                                                      uint32_t in_len, void *out, uint32_t out_len) {
    (void)file; (void)event; (void)apc; (void)apc_ctx;
    (void)io_status; (void)code; (void)in; (void)in_len; (void)out; (void)out_len;
    return 0xC0000002;
}

__attribute__((stdcall)) int NtCreateSection(void **section, uint32_t access, void *attr, 
                                               void *size, uint32_t protect, uint32_t alloc, void *file) {
    if (section) *section = (void*)0x3000;
    return 0;
}

__attribute__((stdcall)) int NtMapViewOfSection(void *section, void *process, void **base, 
                                                   uint32_t zero, uint32_t commit, void *offset, 
                                                   uint32_t *view_size, uint32_t inherit, 
                                                   uint32_t alloc_type, uint32_t protect) {
    if (base) *base = kmalloc(0x10000);
    return 0;
}

__attribute__((stdcall)) int NtUnmapViewOfSection(void *process, void *base) {
    (void)process;
    if (base) kfree(base);
    return 0;
}

__attribute__((stdcall)) int NtCreateMutant(void **mutant, uint32_t access, void *attr, int owned) {
    if (mutant) *mutant = (void*)0x4000;
    return 0;
}

__attribute__((stdcall)) int NtReleaseMutant(void *mutant, int *previous) {
    if (previous) *previous = 0;
    return 0;
}

__attribute__((stdcall)) int NtSuspendThread(void *thread, uint32_t *count) {
    (void)thread;
    if (count) *count = 0;
    return 0;
}

__attribute__((stdcall)) int NtResumeThread(void *thread, uint32_t *count) {
    (void)thread;
    if (count) *count = 0;
    return 0;
}

__attribute__((stdcall)) int NtCancelIoFile(void *file, void *io_status) {
    (void)file; (void)io_status;
    return 0;
}

__attribute__((stdcall)) int NtShutdownSystem(uint32_t action) {
    (void)action;
    SerialPutString("[NT] Shutdown requested\r\n");
    return 0;
}

// Registry stubs
__attribute__((stdcall)) int NtCreateKey(void **key, uint32_t access, void *attr, uint32_t index,
                                           void *name, uint32_t options, uint32_t *disposition) {
    if (key) *key = (void*)0x5000;
    return 0;
}

__attribute__((stdcall)) int NtOpenKey(void **key, uint32_t access, void *attr) {
    if (key) *key = (void*)0x5001;
    return 0;
}

__attribute__((stdcall)) int NtQueryValueKey(void *key, void *name, uint32_t class_, 
                                               void *info, uint32_t len, uint32_t *result_len) {
    (void)key; (void)name; (void)class_; (void)info; (void)len;
    if (result_len) *result_len = 0;
    return 0xC0000034; // STATUS_OBJECT_NAME_NOT_FOUND
}

__attribute__((stdcall)) int NtSetValueKey(void *key, void *name, uint32_t index, 
                                             uint32_t type, void *data, uint32_t len) {
    (void)key; (void)name; (void)index; (void)type; (void)data; (void)len;
    return 0;
}

__attribute__((stdcall)) int NtInitializeRegistry(uint32_t setup) {
    (void)setup;
    return 0;
}

// Plug and Play stubs
__attribute__((stdcall)) int NtPlugPlayControl(uint32_t class_, void *data, uint32_t len) {
    (void)class_; (void)data; (void)len;
    return 0;
}

__attribute__((stdcall)) int NtGetPlugPlayEvent(uint32_t reserved, uint32_t event_type, 
                                                  void *buffer, uint32_t len, uint32_t *actual_len) {
    (void)reserved; (void)event_type; (void)buffer; (void)len;
    if (actual_len) *actual_len = 0;
    return 0xC0000010; // STATUS_NO_MORE_ENTRIES
}

// Error handling
__attribute__((stdcall)) int NtRaiseHardError(uint32_t status, uint32_t num_params, uint32_t flags,
                                                void *params, uint32_t response, uint32_t *resp) {
    (void)status; (void)num_params; (void)flags; (void)params; (void)response;
    if (resp) *resp = 0; // OptionOk
    return 0;
}

// === RTL Functions ===

__attribute__((stdcall)) void *RtlAllocateHeap(void *heap, uint32_t flags, uint32_t size) {
    (void)heap; (void)flags;
    return kmalloc(size);
}

__attribute__((stdcall)) int RtlFreeHeap(void *heap, uint32_t flags, void *ptr) {
    (void)heap; (void)flags;
    kfree(ptr);
    return 1;
}

__attribute__((stdcall)) void *RtlReAllocateHeap(void *heap, uint32_t flags, void *ptr, uint32_t size) {
    (void)heap; (void)flags;
    void *new_ptr = kmalloc(size);
    if (ptr && new_ptr) memcpy(new_ptr, ptr, size);
    if (ptr) kfree(ptr);
    return new_ptr;
}

__attribute__((stdcall)) void *RtlCreateHeap(uint32_t flags, void *base, uint32_t reserve, 
                                               uint32_t commit, void *lock, void *params) {
    (void)flags; (void)base; (void)reserve; (void)commit; (void)lock; (void)params;
    return (void*)1;
}

__attribute__((stdcall)) void *RtlDestroyHeap(void *heap) {
    (void)heap;
    return 0;
}

__attribute__((stdcall)) void RtlInitAnsiString(void *dst, const char *src) {
    if (!dst) return;
    uint16_t len = src ? strlen(src) : 0;
    if (len > 65534) len = 65534;
    *(uint16_t*)dst = len;           // Length
    *(uint16_t*)((uint8_t*)dst + 2) = len; // MaximumLength
    *(uint32_t*)((uint8_t*)dst + 4) = (uint32_t)src; // Buffer
}

__attribute__((stdcall)) void RtlInitUnicodeString(void *dst, const void *src) {
    if (!dst) return;
    // Count wchar_t (16-bit) length
    uint16_t *wstr = (uint16_t*)src;
    uint16_t len = 0;
    if (wstr) {
        while (wstr[len]) len++;
        len *= 2; // Convert to bytes
    }
    if (len > 65534) len = 65534;
    *(uint16_t*)dst = len;
    *(uint16_t*)((uint8_t*)dst + 2) = len;
    *(uint32_t*)((uint8_t*)dst + 4) = (uint32_t)src;
}

__attribute__((stdcall)) void RtlFreeUnicodeString(void *str) {
    (void)str;
}

__attribute__((stdcall)) void *RtlCreateUnicodeString(void *dst, const void *src) {
    // This is simplified
    RtlInitUnicodeString(dst, src);
    return dst;
}

__attribute__((stdcall)) int RtlAnsiStringToUnicodeString(void *dst, void *src, int allocate) {
    (void)dst; (void)src; (void)allocate;
    return 0;
}

__attribute__((stdcall)) int RtlUnicodeStringToOemString(void *dst, void *src, int allocate) {
    (void)dst; (void)src; (void)allocate;
    return 0;
}

__attribute__((stdcall)) uint32_t RtlxUnicodeStringToOemSize(void *str) {
    (void)str;
    return 0;
}

__attribute__((stdcall)) int RtlMultiByteToUnicodeN(void *dst, uint32_t dst_len, uint32_t *written,
                                                       const char *src, uint32_t src_len) {
    (void)dst; (void)dst_len; (void)src; (void)src_len;
    if (written) *written = 0;
    return 0;
}

__attribute__((stdcall)) int RtlPrefixUnicodeString(void *str1, void *str2, int case_insensitive) {
    (void)str1; (void)str2; (void)case_insensitive;
    return 1; // TRUE
}

__attribute__((stdcall)) int RtlIsTextUnicode(void *buf, int len, int *flags) {
    (void)buf; (void)len;
    if (flags) *flags = 0;
    return 0;
}

__attribute__((stdcall)) int RtlAdjustPrivilege(uint32_t privilege, int enable, int current_thread, int *was_enabled) {
    (void)privilege; (void)enable; (void)current_thread;
    if (was_enabled) *was_enabled = 0;
    return 0;
}

__attribute__((stdcall)) void RtlAssert(void *failed_assertion, void *file_name, uint32_t line, void *message) {
    SerialPutString("[RTL] ASSERTION FAILED: ");
    if (message) SerialPutString((const char*)message);
    SerialPutString("\r\n");
}

__attribute__((stdcall)) int RtlTimeFieldsToTime(void *time_fields, void *time) {
    (void)time_fields; (void)time;
    return 0;
}

__attribute__((stdcall)) int RtlNormalizeProcessParams(void *params) {
    (void)params;
    return 0;
}

__attribute__((stdcall)) int RtlCreateUserThread(void *process, void *security, int suspended,
                                                    void *stack_zero, void *stack_commit,
                                                    void *start, void *param, void **thread, 
                                                    void *client_id) {
    (void)process; (void)security; (void)suspended;
    (void)stack_zero; (void)stack_commit;
    (void)start; (void)param;
    if (thread) *thread = (void*)0x6000;
    return 0;
}

// Debug print
__attribute__((stdcall)) void DbgPrint(const char *fmt, ...) {
    (void)fmt;
    // In a real implementation, we'd format the string
    // For now, just print the first argument
    SerialPutString("[DbgPrint] ");
    SerialPutString(fmt);
    SerialPutString("\r\n");
}

// NlsMbOemCodePageTag - usually just returns a constant
__attribute__((stdcall)) int NlsMbOemCodePageTag(void) {
    return 0; // CP_ACP = 0
}