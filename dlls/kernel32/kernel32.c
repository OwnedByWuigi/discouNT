#include <stdint.h>
#include <stdarg.h>
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
extern void *PeLoadDll(const char *dll_name);
extern void *PeGetLoadedModuleHandle(const char *name);
extern uint32_t strlen(const char *s);
extern void *memset(void *dest, int c, uint32_t n);
extern void *memcpy(void *dest, const void *src, uint32_t n);
extern void SerialPutString(const char *str);
extern int CdfsReadFile(const char *path, uint8_t **out_buffer, uint32_t *out_size);

typedef void (*K32_CONSOLE_SINK)(const char *buffer, uint32_t length);
static K32_CONSOLE_SINK g_console_sink;

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

static void k32_wstrncpy(LPWSTR dst, LPCWSTR src, int max_chars) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
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

static void k32_append_dec2(WCHAR *dst, int *pos, int value) {
    dst[(*pos)++] = (WCHAR)(L'0' + ((value / 10) % 10));
    dst[(*pos)++] = (WCHAR)(L'0' + (value % 10));
}

static void k32_append_dec4(WCHAR *dst, int *pos, int value) {
    dst[(*pos)++] = (WCHAR)(L'0' + ((value / 1000) % 10));
    dst[(*pos)++] = (WCHAR)(L'0' + ((value / 100) % 10));
    dst[(*pos)++] = (WCHAR)(L'0' + ((value / 10) % 10));
    dst[(*pos)++] = (WCHAR)(L'0' + (value % 10));
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
static void *g_process_image_base = 0;

static char k32_toupper_char(char ch) {
    return (ch >= 'a' && ch <= 'z') ? (ch - ('a' - 'A')) : ch;
}

static void k32_uppercase_copy(const char *src, char *dst, int max_chars) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        dst[i] = k32_toupper_char(src[i]);
        i++;
    }
    dst[i] = 0;
}

static void k32_wide_to_ansi_name(LPCWSTR src, char *dst, int max_chars) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        dst[i] = k32_toupper_char((char)src[i]);
        i++;
    }
    dst[i] = 0;
}

#define K32_HANDLE_FILE  0x4B333246u

typedef struct _K32_FILE_HANDLE {
    uint32_t magic;
    uint8_t *data;
    uint32_t size;
    uint32_t pos;
    DWORD access;
} K32_FILE_HANDLE;

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

static void k32_wide_to_ansi_path(LPCWSTR src, char *dst, int max_chars) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        WCHAR ch = src[i];
        if (ch == L'\\') ch = L'/';
        dst[i] = (char)((ch >= 1 && ch <= 127) ? ch : '?');
        i++;
    }
    dst[i] = 0;
}

static const char *k32_find_last_path_part(const char *path) {
    const char *last = path;
    if (!path) return "";
    while (*path) {
        if (*path == '/' || *path == '\\') last = path + 1;
        path++;
    }
    return last;
}

static K32_FILE_HANDLE *k32_file_from_handle(HANDLE hFile) {
    K32_FILE_HANDLE *fh = (K32_FILE_HANDLE*)hFile;
    if (!fh || fh->magic != K32_HANDLE_FILE) return 0;
    return fh;
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
    (void)reserved;
    if (!buf) return 0;
    if (g_console_sink && ((uintptr_t)handle == (uintptr_t)GetStdHandle((uint32_t)-11) ||
                          (uintptr_t)handle == (uintptr_t)GetStdHandle((uint32_t)-12)))
        g_console_sink(buf, len);
    if (written) *written = len;
    return 1;
}

void ExitProcess(uint32_t code) {
    (void)code;
    for (;;) KeYield();
}

void *HeapAlloc(void *heap, uint32_t flags, SIZE_T size) {
    (void)heap;
    (void)flags;
    return kmalloc(size);
}

HLOCAL LocalAlloc(UINT flags, SIZE_T bytes) {
    void *memory = kmalloc((uint32_t)bytes);
    if (memory && (flags & 0x0040U)) memset(memory, 0, (uint32_t)bytes);
    return (HLOCAL)memory;
}

HLOCAL LocalFree(HLOCAL memory) {
    if (memory) kfree(memory);
    return (HLOCAL)0;
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
    char upper_name[128];
    if (!name) return g_process_image_base ? g_process_image_base : (void*)0x400000;
    k32_uppercase_copy(name, upper_name, sizeof(upper_name));
    return PeGetLoadedModuleHandle(upper_name);
}

void *GetModuleHandleW(LPCWSTR name) {
    char upper_name[128];
    if (!name) return g_process_image_base ? g_process_image_base : (void*)0x400000;
    k32_wide_to_ansi_name(name, upper_name, sizeof(upper_name));
    return PeGetLoadedModuleHandle(upper_name);
}

HMODULE LoadLibraryW(LPCWSTR name) {
    char ansi_name[128];
    k32_wide_to_ansi_name(name, ansi_name, sizeof(ansi_name));
    if (!ansi_name[0]) return 0;
    return (HMODULE)PeLoadDll(ansi_name);
}

HMODULE LoadLibraryA(const char *name) {
    char upper_name[128];
    k32_uppercase_copy(name, upper_name, sizeof(upper_name));
    if (!upper_name[0]) return 0;
    return (HMODULE)PeLoadDll(upper_name);
}

__attribute__((stdcall)) void Kernel32SetProcessImageBase(void *image_base) {
    g_process_image_base = image_base;
}

__attribute__((stdcall)) void Kernel32SetConsoleSink(K32_CONSOLE_SINK sink) {
    g_console_sink = sink;
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
    K32_FILE_HANDLE *fh = k32_file_from_handle(hObject);
    if (fh) {
        if (fh->data) kfree(fh->data);
        kfree(fh);
        return 1;
    }
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

HANDLE CreateFileW(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    K32_FILE_HANDLE *fh;
    char path[260];
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    (void)dwShareMode;
    (void)lpSecurityAttributes;
    (void)dwCreationDisposition;
    (void)dwFlagsAndAttributes;
    (void)hTemplateFile;
    if (!lpFileName) {
        g_last_error = 2;
        return INVALID_HANDLE_VALUE;
    }
    k32_wide_to_ansi_path(lpFileName, path, sizeof(path));
    if (path[0] && path[0] != '/') {
        char tmp[260];
        int i = 0;
        tmp[i++] = '/';
        while (path[i - 1] && i < (int)sizeof(tmp) - 1) {
            tmp[i] = path[i - 1];
            i++;
        }
        tmp[i - 1] = 0;
        i = 0;
        while (tmp[i] && i < (int)sizeof(path) - 1) {
            path[i] = tmp[i];
            i++;
        }
        path[i] = 0;
    }
    if ((dwDesiredAccess & GENERIC_WRITE) != 0) {
        g_last_error = 5;
        return INVALID_HANDLE_VALUE;
    }
    if (!CdfsReadFile(path, &file_buf, &file_size)) {
        g_last_error = 2;
        return INVALID_HANDLE_VALUE;
    }
    fh = (K32_FILE_HANDLE*)kmalloc(sizeof(K32_FILE_HANDLE));
    if (!fh) {
        if (file_buf) kfree(file_buf);
        g_last_error = 8;
        return INVALID_HANDLE_VALUE;
    }
    fh->magic = K32_HANDLE_FILE;
    fh->data = file_buf;
    fh->size = file_size;
    fh->pos = 0;
    fh->access = dwDesiredAccess;
    return (HANDLE)fh;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              DWORD *lpNumberOfBytesRead, LPVOID lpOverlapped) {
    K32_FILE_HANDLE *fh = k32_file_from_handle(hFile);
    DWORD remaining;
    DWORD amount;
    (void)lpOverlapped;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
    if (!fh || !lpBuffer) {
        g_last_error = 6;
        return 0;
    }
    if (fh->pos >= fh->size) return 1;
    remaining = fh->size - fh->pos;
    amount = (nNumberOfBytesToRead < remaining) ? nNumberOfBytesToRead : remaining;
    memcpy(lpBuffer, fh->data + fh->pos, amount);
    fh->pos += amount;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = amount;
    return 1;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               DWORD *lpNumberOfBytesWritten, LPVOID lpOverlapped) {
    (void)lpOverlapped;
    if (g_console_sink && ((uintptr_t)hFile == (uintptr_t)GetStdHandle((uint32_t)-11) ||
                           (uintptr_t)hFile == (uintptr_t)GetStdHandle((uint32_t)-12))) {
        g_console_sink((const char *)lpBuffer, nNumberOfBytesToWrite);
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = nNumberOfBytesToWrite;
        return 1;
    }
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0;
    g_last_error = 5;
    return 0;
}

DWORD GetFileSize(HANDLE hFile, DWORD *lpFileSizeHigh) {
    K32_FILE_HANDLE *fh = k32_file_from_handle(hFile);
    if (!fh) {
        g_last_error = 6;
        return INVALID_FILE_SIZE;
    }
    if (lpFileSizeHigh) *lpFileSizeHigh = 0;
    return fh->size;
}

BOOL SetEndOfFile(HANDLE hFile) {
    (void)hFile;
    g_last_error = 5;
    return 0;
}

HANDLE FindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData) {
    HANDLE hFile;
    DWORD size;
    if (!lpFindFileData) {
        g_last_error = 87;
        return INVALID_HANDLE_VALUE;
    }
    memset(lpFindFileData, 0, sizeof(*lpFindFileData));
    hFile = CreateFileW(lpFileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (hFile == INVALID_HANDLE_VALUE) return INVALID_HANDLE_VALUE;
    size = GetFileSize(hFile, 0);
    lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    lpFindFileData->nFileSizeLow = size;
    if (lpFileName) {
        int i = 0;
        int start = 0;
        while (lpFileName[i]) {
            if (lpFileName[i] == L'/' || lpFileName[i] == L'\\') start = i + 1;
            i++;
        }
        i = 0;
        while (lpFileName[start + i] && i < (MAX_PATH - 1)) {
            lpFindFileData->cFileName[i] = lpFileName[start + i];
            i++;
        }
        lpFindFileData->cFileName[i] = 0;
    }
    return hFile;
}

BOOL FindClose(HANDLE hFindFile) {
    return CloseHandle(hFindFile);
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

BOOL GetCPInfoExW(UINT CodePage, DWORD dwFlags, LPCPINFOEXW lpCPInfoEx) {
    const WCHAR *name = L"Unicode (UTF-8)";
    int i = 0;
    (void)dwFlags;
    if (!lpCPInfoEx) return 0;
    memset(lpCPInfoEx, 0, sizeof(*lpCPInfoEx));
    lpCPInfoEx->MaxCharSize = (CodePage == CP_UTF8) ? 4 : 1;
    lpCPInfoEx->DefaultChar[0] = '?';
    lpCPInfoEx->DefaultChar[1] = 0;
    lpCPInfoEx->UnicodeDefaultChar = L'?';
    lpCPInfoEx->CodePage = CodePage ? CodePage : CP_ACP;
    if (lpCPInfoEx->CodePage != CP_UTF8) name = L"ANSI";
    while (name[i] && i < (MAX_PATH - 1)) {
        lpCPInfoEx->CodePageName[i] = name[i];
        i++;
    }
    lpCPInfoEx->CodePageName[i] = 0;
    return 1;
}

int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr, int cchWideChar,
                        LPSTR lpMultiByteStr, int cbMultiByte, LPCSTR lpDefaultChar, BOOL *lpUsedDefaultChar) {
    int i = 0;
    int out = 0;
    (void)CodePage;
    (void)dwFlags;
    (void)lpDefaultChar;
    if (lpUsedDefaultChar) *lpUsedDefaultChar = FALSE;
    if (!lpWideCharStr) return 0;
    if (cchWideChar < 0) cchWideChar = k32_wstrlen(lpWideCharStr) + 1;
    if (!lpMultiByteStr || cbMultiByte <= 0) return cchWideChar;
    while (i < cchWideChar && out < cbMultiByte) {
        WCHAR ch = lpWideCharStr[i];
        if (ch > 0x7F && lpUsedDefaultChar) *lpUsedDefaultChar = TRUE;
        lpMultiByteStr[out++] = (char)((ch >= 0x20 && ch <= 0x7F) ? ch : (ch == 0 ? 0 : '?'));
        if (!ch) break;
        i++;
    }
    if (out == cbMultiByte) lpMultiByteStr[cbMultiByte - 1] = 0;
    return out;
}

LPWSTR GetCommandLineW(void) {
    static WCHAR cmdline[] = L"NOTEPAD.EXE";
    return cmdline;
}

int MulDiv(int nNumber, int nNumerator, int nDenominator) {
    long long value;
    if (!nDenominator) return -1;
    value = (long long)nNumber * (long long)nNumerator;
    if ((value >= 0 && nDenominator > 0) || (value <= 0 && nDenominator < 0)) {
        value += nDenominator / 2;
    } else {
        value -= nDenominator / 2;
    }
    value /= nDenominator;
    return (int)value;
}

short GetFileTitleW(LPCWSTR lpFile, LPWSTR lpTitle, WORD cbBuf) {
    LPCWSTR last;
    int len;
    if (!lpTitle || cbBuf == 0) return -1;
    if (!lpFile) {
        lpTitle[0] = 0;
        return 0;
    }
    last = lpFile;
    while (*lpFile) {
        if (*lpFile == L'/' || *lpFile == L'\\') last = lpFile + 1;
        lpFile++;
    }
    len = k32_wstrlen(last);
    if (len >= cbBuf) len = cbBuf - 1;
    k32_wstrncpy(lpTitle, last, len + 1);
    return 0;
}

BOOL IsTextUnicode(const void *buf, int len, int *flags) {
    const BYTE *p = (const BYTE*)buf;
    int zero_odd = 0;
    int zero_even = 0;
    int i;
    if (flags) *flags = 0;
    if (!buf || len < 2) return FALSE;
    if (len >= 2 && p[0] == 0xFF && p[1] == 0xFE) return TRUE;
    if (len >= 2 && p[0] == 0xFE && p[1] == 0xFF) return TRUE;
    for (i = 0; i + 1 < len; i += 2) {
        if (p[i] == 0) zero_even++;
        if (p[i + 1] == 0) zero_odd++;
    }
    return zero_odd > zero_even;
}

WORD RtlUshortByteSwap(WORD s) {
    return (WORD)((s >> 8) | (s << 8));
}

int memcmp(const void *a, const void *b, uint32_t n) {
    const BYTE *pa = (const BYTE*)a;
    const BYTE *pb = (const BYTE*)b;
    uint32_t i;
    for (i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (pa[i] < pb[i]) ? -1 : 1;
    }
    return 0;
}

void GetLocalTime(LPSYSTEMTIME lpSystemTime) {
    if (!lpSystemTime) return;
    lpSystemTime->wYear = 2026;
    lpSystemTime->wMonth = 8;
    lpSystemTime->wDay = 11;
    lpSystemTime->wDayOfWeek = 2;
    lpSystemTime->wHour = 12;
    lpSystemTime->wMinute = 0;
    lpSystemTime->wSecond = 0;
    lpSystemTime->wMilliseconds = 0;
}

int GetTimeFormatW(DWORD Locale, DWORD dwFlags, const SYSTEMTIME *lpTime, LPCWSTR lpFormat, LPWSTR lpTimeStr, int cchTime) {
    SYSTEMTIME local;
    int pos = 0;
    const SYSTEMTIME *st = lpTime;
    (void)Locale;
    (void)lpFormat;
    if (!st) {
        GetLocalTime(&local);
        st = &local;
    }
    if (!lpTimeStr || cchTime < 6) return 0;
    k32_append_dec2(lpTimeStr, &pos, st->wHour);
    lpTimeStr[pos++] = L':';
    k32_append_dec2(lpTimeStr, &pos, st->wMinute);
    if (!(dwFlags & TIME_NOSECONDS)) {
        lpTimeStr[pos++] = L':';
        k32_append_dec2(lpTimeStr, &pos, st->wSecond);
    }
    if (pos >= cchTime) pos = cchTime - 1;
    lpTimeStr[pos] = 0;
    return pos;
}

int GetDateFormatW(DWORD Locale, DWORD dwFlags, const SYSTEMTIME *lpDate, LPCWSTR lpFormat, LPWSTR lpDateStr, int cchDate) {
    SYSTEMTIME local;
    int pos = 0;
    const SYSTEMTIME *st = lpDate;
    (void)Locale;
    (void)dwFlags;
    (void)lpFormat;
    if (!st) {
        GetLocalTime(&local);
        st = &local;
    }
    if (!lpDateStr || cchDate < 11) return 0;
    k32_append_dec2(lpDateStr, &pos, st->wMonth);
    lpDateStr[pos++] = L'/';
    k32_append_dec2(lpDateStr, &pos, st->wDay);
    lpDateStr[pos++] = L'/';
    k32_append_dec4(lpDateStr, &pos, st->wYear);
    if (pos >= cchDate) pos = cchDate - 1;
    lpDateStr[pos] = 0;
    return pos;
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
