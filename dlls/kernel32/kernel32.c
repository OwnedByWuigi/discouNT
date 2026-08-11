#include <stdint.h>
#include "windows.h"
#include "version.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void KeYield(void);
extern uint32_t KeGetSchedulerTicks(void);
extern uint32_t KeCreateEvent(uint32_t manual_reset);
extern void KeSetEvent(uint32_t event_handle);
extern void KeResetEvent(uint32_t event_handle);
extern void KeWaitEvent(uint32_t event_handle);
extern uint32_t KeCreateThread(void (*entry)(void *), void *arg, uint32_t stack_size);
extern void *PeGetProcAddress(void *dll_base, const char *func_name);
extern uint32_t strlen(const char *s);
extern void *memset(void *dest, int c, uint32_t n);
extern void *memcpy(void *dest, const void *src, uint32_t n);
extern void SerialPutString(const char *str);

static int k32_wstrlen(LPCWSTR s) {
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static LPWSTR k32_wstrcpy(LPWSTR dst, LPCWSTR src) {
    int i = 0;
    if (!dst) return dst;
    if (!src) {
        dst[0] = 0;
        return dst;
    }
    while (src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
    return dst;
}

static int k32_wstrcmp(LPCWSTR a, LPCWSTR b) {
    int i = 0;
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return (a[i] < b[i]) ? -1 : 1;
        i++;
    }
    if (a[i] == b[i]) return 0;
    return a[i] ? 1 : -1;
}

static ULONGLONG k32_udivmod64(ULONGLONG num, ULONGLONG den, ULONGLONG *rem_out) {
    ULONGLONG q = 0;
    ULONGLONG r = 0;
    int i;
    if (den == 0) {
        if (rem_out) *rem_out = 0;
        return 0;
    }
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((num >> i) & 1ULL);
        if (r >= den) {
            r -= den;
            q |= (1ULL << i);
        }
    }
    if (rem_out) *rem_out = r;
    return q;
}

static DWORD g_last_error = 0;

typedef struct _K32_THREAD_START {
    LPTHREAD_START_ROUTINE start;
    LPVOID arg;
} K32_THREAD_START;

static void k32_thread_boot(void *arg) {
    K32_THREAD_START *ctx = (K32_THREAD_START*)arg;
    SerialPutString("[KERNEL32] CreateThread worker start\r\n");
    if (ctx && ctx->start) {
        ctx->start(ctx->arg);
    }
    if (ctx) kfree(ctx);
    SerialPutString("[KERNEL32] CreateThread worker exit\r\n");
    for (;;) KeYield();
}

int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

DWORD GetLastError(void) {
    return g_last_error;
}

void SetLastError(DWORD dwErrCode) {
    g_last_error = dwErrCode;
}

void *GetStdHandle(uint32_t handle) {
    return (void*)(uintptr_t)(0x10000u + handle);
}

int WriteConsoleA(void *handle, const char *buf, uint32_t len, uint32_t *written, void *reserved) {
    (void)handle;
    (void)reserved;
    if (!buf) return 0;
    if (written) *written = len;
    return 1;
}

void ExitProcess(uint32_t code) {
    (void)code;
    for (;;) KeYield();
}

void *HeapAlloc(void *heap, uint32_t flags, uint32_t size) {
    (void)heap;
    (void)flags;
    return kmalloc(size);
}

int HeapFree(void *heap, uint32_t flags, void *ptr) {
    (void)heap;
    (void)flags;
    if (ptr) kfree(ptr);
    return 1;
}

void *GetProcessHeap(void) {
    return (void*)1;
}

void *GetModuleHandleA(const char *name) {
    (void)name;
    return (void*)0x400000;
}

void *GetModuleHandleW(LPCWSTR name) {
    (void)name;
    return (void*)0x400000;
}

void *GetProcAddress(void *hModule, const char *name) {
    if (!hModule || !name) return 0;
    return PeGetProcAddress(hModule, name);
}

uint32_t GetVersion(void) {
    return ((DISCOUNT_WIN32_BUILD & 0xFFFF) << 16) |
           ((DISCOUNT_WIN32_MINOR & 0xFF) << 8) |
           (DISCOUNT_WIN32_MAJOR & 0xFF);
}

uint32_t GetTickCount(void) {
    return KeGetSchedulerTicks() * 16;
}

void Sleep(uint32_t ms) {
    uint32_t i;
    for (i = 0; i < (ms ? ms : 1); i++) KeYield();
}

HANDLE CreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName) {
    uint32_t h;
    (void)lpEventAttributes;
    (void)lpName;
    h = KeCreateEvent(bManualReset ? 1 : 0);
    if (h == 0xFFFFFFFFU) {
        g_last_error = 8;
        return 0;
    }
    if (bInitialState) KeSetEvent(h);
    else KeResetEvent(h);
    return (HANDLE)(uintptr_t)h;
}

BOOL SetEvent(HANDLE hEvent) {
    if (!hEvent) return 0;
    KeSetEvent((uint32_t)(uintptr_t)hEvent);
    return 1;
}

BOOL ResetEvent(HANDLE hEvent) {
    if (!hEvent) return 0;
    KeResetEvent((uint32_t)(uintptr_t)hEvent);
    return 1;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    (void)dwMilliseconds;
    if (!hHandle) return WAIT_FAILED;
    KeWaitEvent((uint32_t)(uintptr_t)hHandle);
    return WAIT_OBJECT_0;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize,
                                             LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter,
                                             DWORD dwCreationFlags, DWORD *lpThreadId) {
    K32_THREAD_START *ctx;
    uint32_t h;
    (void)lpThreadAttributes;
    (void)dwCreationFlags;
    if (!lpStartAddress) return 0;
    ctx = (K32_THREAD_START*)kmalloc(sizeof(K32_THREAD_START));
    if (!ctx) return 0;
    ctx->start = lpStartAddress;
    ctx->arg = lpParameter;
    h = KeCreateThread(k32_thread_boot, ctx, (uint32_t)(dwStackSize ? dwStackSize : 16384));
    if (h == 0xFFFFFFFFU) {
        kfree(ctx);
        return 0;
    }
    if (lpThreadId) *lpThreadId = (DWORD)h;
    return (HANDLE)(uintptr_t)h;
}

BOOL CloseHandle(HANDLE hObject) {
    (void)hObject;
    return 1;
}

DWORD GetCurrentProcessId(void) {
    return 1;
}

HANDLE GetCurrentProcess(void) {
    return (HANDLE)1;
}

HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
    (void)dwDesiredAccess;
    (void)bInheritHandle;
    return (HANDLE)(uintptr_t)(dwProcessId ? dwProcessId : 1);
}

BOOL TerminateProcess(HANDLE hProcess, UINT uExitCode) {
    (void)hProcess;
    (void)uExitCode;
    return 1;
}

BOOL GetProcessAffinityMask(HANDLE hProcess, DWORD_PTR *lpProcessAffinityMask, DWORD_PTR *lpSystemAffinityMask) {
    (void)hProcess;
    if (lpProcessAffinityMask) *lpProcessAffinityMask = 1;
    if (lpSystemAffinityMask) *lpSystemAffinityMask = 1;
    return 1;
}

BOOL SetProcessAffinityMask(HANDLE hProcess, DWORD_PTR dwProcessAffinityMask) {
    (void)hProcess;
    (void)dwProcessAffinityMask;
    return 1;
}

BOOL SetPriorityClass(HANDLE hProcess, DWORD dwPriorityClass) {
    (void)hProcess;
    (void)dwPriorityClass;
    return 1;
}

DWORD GetPriorityClass(HANDLE hProcess) {
    (void)hProcess;
    return NORMAL_PRIORITY_CLASS;
}

void InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    if (lpCriticalSection) {
        lpCriticalSection->LockCount = 0;
        lpCriticalSection->RecursionCount = 0;
        lpCriticalSection->OwningThread = 0;
        lpCriticalSection->LockSemaphore = 0;
    }
}

void EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    (void)lpCriticalSection;
}

void LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection) {
    (void)lpCriticalSection;
}

void GetSystemInfo(LPSYSTEM_INFO lpSystemInfo) {
    if (!lpSystemInfo) return;
    lpSystemInfo->dwPageSize = 4096;
    lpSystemInfo->lpMinimumApplicationAddress = (LPVOID)0x1000;
    lpSystemInfo->lpMaximumApplicationAddress = (LPVOID)0x7FFFFFFF;
    lpSystemInfo->dwActiveProcessorMask = 1;
    lpSystemInfo->dwNumberOfProcessors = 1;
    lpSystemInfo->dwProcessorType = 386;
    lpSystemInfo->dwAllocationGranularity = 4096;
    lpSystemInfo->wProcessorLevel = 3;
    lpSystemInfo->wProcessorRevision = 0;
}

BOOL GetVersionExW(LPOSVERSIONINFOW lpVersionInformation) {
    uint32_t i;
    if (!lpVersionInformation) return 0;
    lpVersionInformation->dwMajorVersion = DISCOUNT_WIN32_MAJOR;
    lpVersionInformation->dwMinorVersion = DISCOUNT_WIN32_MINOR;
    lpVersionInformation->dwBuildNumber = DISCOUNT_WIN32_BUILD;
    lpVersionInformation->dwPlatformId = VER_PLATFORM_WIN32_NT;
    for (i = 0; i < (sizeof(lpVersionInformation->szCSDVersion) / sizeof(lpVersionInformation->szCSDVersion[0])); i++) {
        lpVersionInformation->szCSDVersion[i] = 0;
    }
    return 1;
}

HANDLE LoadImageA(HINSTANCE hinst, LPCSTR name, UINT type, int cx, int cy, UINT fuLoad) {
    (void)hinst; (void)name; (void)type; (void)cx; (void)cy; (void)fuLoad;
    return (HANDLE)1;
}

BOOL CreateProcessW(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                    LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
                    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
                    LPCWSTR lpCurrentDirectory, STARTUPINFOW *lpStartupInfo,
                    PROCESS_INFORMATION *lpProcessInformation) {
    (void)lpApplicationName; (void)lpCommandLine; (void)lpProcessAttributes; (void)lpThreadAttributes;
    (void)bInheritHandles; (void)dwCreationFlags; (void)lpEnvironment; (void)lpCurrentDirectory; (void)lpStartupInfo;
    if (lpProcessInformation) {
        lpProcessInformation->hProcess = 0;
        lpProcessInformation->hThread = 0;
        lpProcessInformation->dwProcessId = 0;
        lpProcessInformation->dwThreadId = 0;
    }
    g_last_error = 120;
    return 0;
}

BOOL ReadProcessMemory(HANDLE hProcess, LPCVOID lpBaseAddress, LPVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesRead) {
    (void)hProcess;
    if (!lpBuffer || !lpBaseAddress) return 0;
    memcpy(lpBuffer, lpBaseAddress, (uint32_t)nSize);
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = nSize;
    return 1;
}

BOOL WriteProcessMemory(HANDLE hProcess, LPVOID lpBaseAddress, LPCVOID lpBuffer, SIZE_T nSize, SIZE_T *lpNumberOfBytesWritten) {
    (void)hProcess;
    if (!lpBuffer || !lpBaseAddress) return 0;
    memcpy(lpBaseAddress, lpBuffer, (uint32_t)nSize);
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = nSize;
    return 1;
}

DWORD GetGuiResources(HANDLE hProcess, DWORD uiFlags) {
    (void)hProcess; (void)uiFlags;
    return 0;
}

BOOL GetProcessIoCounters(HANDLE hProcess, PIO_COUNTERS lpIoCounters) {
    (void)hProcess;
    if (!lpIoCounters) return 0;
    memset(lpIoCounters, 0, sizeof(*lpIoCounters));
    return 1;
}

BOOL IsWow64Process(HANDLE hProcess, BOOL *Wow64Process) {
    (void)hProcess;
    if (Wow64Process) *Wow64Process = FALSE;
    return 1;
}

BOOL ImpersonateLoggedOnUser(HANDLE hToken) {
    (void)hToken;
    return 1;
}

BOOL RevertToSelf(void) {
    return 1;
}

int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte,
                        LPWSTR lpWideCharStr, int cchWideChar) {
    int i = 0;
    (void)CodePage; (void)dwFlags;
    if (!lpMultiByteStr) return 0;
    if (cbMultiByte < 0) cbMultiByte = (int)strlen(lpMultiByteStr) + 1;
    if (!lpWideCharStr || cchWideChar <= 0) return cbMultiByte;
    while (i < cbMultiByte && i < cchWideChar) {
        lpWideCharStr[i] = (unsigned char)lpMultiByteStr[i];
        if (!lpMultiByteStr[i]) return i;
        i++;
    }
    if (i == cchWideChar) lpWideCharStr[cchWideChar - 1] = 0;
    return i;
}

int lstrlenW(LPCWSTR lpString) {
    return k32_wstrlen(lpString);
}

WCHAR *wcsupr(WCHAR *str) {
    int i = 0;
    if (!str) return str;
    while (str[i]) {
        if (str[i] >= L'a' && str[i] <= L'z') str[i] -= (L'a' - L'A');
        i++;
    }
    return str;
}

WCHAR *_ui64tow(ULONGLONG value, WCHAR *buffer, int radix) {
    WCHAR temp[65];
    int i = 0, j;
    if (!buffer) return 0;
    if (radix != 10 && radix != 16) radix = 10;
    if (value == 0) {
        buffer[0] = L'0';
        buffer[1] = 0;
        return buffer;
    }
    while (value && i < 64) {
        unsigned digit = (unsigned)(value % (ULONGLONG)radix);
        temp[i++] = (WCHAR)(digit < 10 ? (L'0' + digit) : (L'A' + (digit - 10)));
        value /= (ULONGLONG)radix;
    }
    for (j = 0; j < i; j++) buffer[j] = temp[i - 1 - j];
    buffer[i] = 0;
    return buffer;
}

LPWSTR lstrcpyW(LPWSTR lpString1, LPCWSTR lpString2) {
    return k32_wstrcpy(lpString1, lpString2);
}

LPWSTR lstrcpynW(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength) {
    int i = 0;
    if (!lpString1 || iMaxLength <= 0) return lpString1;
    if (!lpString2) {
        lpString1[0] = 0;
        return lpString1;
    }
    while (lpString2[i] && i < iMaxLength - 1) {
        lpString1[i] = lpString2[i];
        i++;
    }
    lpString1[i] = 0;
    return lpString1;
}

LPWSTR lstrcatW(LPWSTR lpString1, LPCWSTR lpString2) {
    int i = k32_wstrlen(lpString1);
    int j = 0;
    if (!lpString1) return lpString1;
    if (!lpString2) return lpString1;
    while (lpString2[j]) {
        lpString1[i + j] = lpString2[j];
        j++;
    }
    lpString1[i + j] = 0;
    return lpString1;
}

int lstrcmpW(LPCWSTR lpString1, LPCWSTR lpString2) {
    return k32_wstrcmp(lpString1, lpString2);
}

ULONGLONG __udivdi3(ULONGLONG num, ULONGLONG den) {
    return k32_udivmod64(num, den, 0);
}

ULONGLONG __umoddi3(ULONGLONG num, ULONGLONG den) {
    ULONGLONG rem = 0;
    k32_udivmod64(num, den, &rem);
    return rem;
}
