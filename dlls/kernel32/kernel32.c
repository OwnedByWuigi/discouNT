#include <stdint.h>
#include <stdarg.h>
#include "windows.h"
#include "core/version.h"

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
extern void *realloc(void *memory, SIZE_T size);
extern void SerialPutString(const char *str);
extern int CdfsReadFile(const char *path, uint8_t **out_buffer, uint32_t *out_size);
extern int CsrssExecuteImage(const char *path);
extern HFILE _lopen(LPCSTR path, int read_write);
extern uint32_t KeGetPhysicalMemoryPages(void);

UINT WINAPI GetSystemDirectoryW(LPWSTR b, UINT n) { static const WCHAR s[] = L"/DISCOUNT/SYSTEM32"; UINT i=0; if(!b||!n)return 0; while(i+1<n&&s[i]){b[i]=s[i];i++;} b[i]=0; return i; }
BOOL WINAPI GlobalMemoryStatusEx(void *status) {
    uint64_t total = (uint64_t)KeGetPhysicalMemoryPages() * 4096;
    uint64_t *values = (uint64_t *)((uint8_t *)status + 8);
    if (!status) return FALSE;
    memset(status, 0, 64);
    values[0] = total;
    values[1] = total;
    values[2] = total;
    values[3] = total;
    values[4] = total;
    values[5] = total;
    return TRUE;
}
int WINAPI GetLocaleInfoW(LCID locale,DWORD type,LPWSTR b,int n) { static const WCHAR s[] = L"English"; int i=0; (void)locale;(void)type; if(!b||n<=0)return 0; while(i+1<n&&s[i]){b[i]=s[i];i++;}b[i]=0;return i+1; }
static const DWORD k32_process_ids[] = {0, 2, 3};
static DWORD k32_copy_process_name(LPWSTR dst, DWORD capacity, DWORD pid) {
    const char *name;
    DWORD i = 0;
    if (!dst || !capacity) return 0;
    if (pid == 0) name = "System";
    else if (pid == 2) name = "CMD.EXE";
    else if (pid == 3) name = "TASKMGR.EXE";
    else return 0;
    while (name[i] && i + 1 < capacity) { dst[i] = (WCHAR)name[i]; i++; }
    dst[i] = 0;
    return i;
}
BOOL WINAPI EnumProcesses(DWORD *p,DWORD bytes,DWORD *needed) {
    DWORD count = sizeof(k32_process_ids) / sizeof(k32_process_ids[0]);
    DWORD copy = bytes / sizeof(DWORD), i;
    if (needed) *needed = count * sizeof(DWORD);
    if (!p && bytes) return FALSE;
    if (copy > count) copy = count;
    for (i = 0; i < copy; i++) p[i] = k32_process_ids[i];
    return TRUE;
}
BOOL WINAPI EnumProcessModules(HANDLE p,HMODULE *m,DWORD bytes,DWORD *needed) {
    DWORD pid = (DWORD)(uintptr_t)p;
    if (needed) *needed = sizeof(HMODULE);
    if (!m || bytes < sizeof(HMODULE) || (pid != 0 && pid != 2 && pid != 3)) return FALSE;
    m[0] = (HMODULE)(uintptr_t)0x00400000;
    return TRUE;
}
DWORD WINAPI GetModuleBaseNameW(HANDLE p,HMODULE m,LPWSTR b,DWORD n) {
    (void)m;
    return k32_copy_process_name(b, n, (DWORD)(uintptr_t)p);
}
DWORD WINAPI GetFileVersionInfoSizeW(LPCWSTR file, DWORD *handle) { (void)file; if(handle)*handle=0; return 0; }
BOOL WINAPI GetFileVersionInfoW(LPCWSTR file,DWORD handle,DWORD len,LPVOID data) { (void)file;(void)handle;(void)len;(void)data; return FALSE; }
BOOL WINAPI VerQueryValueW(LPCVOID block,LPCWSTR sub,LPVOID *value,UINT *len) { (void)block;(void)sub;if(value)*value=0;if(len)*len=0;return FALSE; }
extern HFILE _lclose(HFILE file);
extern char *strrchr(const char *s, int c);
#ifndef EOF
#define EOF (-1)
#endif

typedef void (*K32_CONSOLE_SINK)(const char *buffer, uint32_t length);
static K32_CONSOLE_SINK g_console_sink;
int errno;

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

/* Windows resolves an extensionless module name as a DLL.  Preserve paths
   and explicit extensions, and only add the suffix to the final component. */
static void k32_normalize_module_name(char *name, uint32_t capacity) {
    uint32_t i = 0, base = 0;
    int has_extension = 0;
    if (!name || capacity < 5) return;
    while (name[i]) {
        if (name[i] == '/' || name[i] == '\\') {
            base = i + 1;
            has_extension = 0;
        } else if (name[i] == '.' && i > base) {
            has_extension = 1;
        }
        i++;
    }
    if (!has_extension && i + 4 < capacity) {
        name[i++] = '.';
        name[i++] = 'D';
        name[i++] = 'L';
        name[i++] = 'L';
        name[i] = 0;
    }
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

void GetStartupInfoW(LPSTARTUPINFOW startup_info) {
    if (!startup_info) return;
    memset(startup_info, 0, sizeof(*startup_info));
    startup_info->cb = sizeof(*startup_info);
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

void exit(int status) { ExitProcess((uint32_t)status); }

int _wcsicmp(LPCWSTR a, LPCWSTR b) {
    int i = 0;
    while (a && b && a[i] && b[i]) {
        WCHAR ca = a[i], cb = b[i];
        if (ca >= L'A' && ca <= L'Z') ca += L'a' - L'A';
        if (cb >= L'A' && cb <= L'Z') cb += L'a' - L'A';
        if (ca != cb) return ca < cb ? -1 : 1;
        i++;
    }
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    return a[i] == b[i] ? 0 : (a[i] ? 1 : -1);
}

WCHAR *wcsdup(LPCWSTR source) {
    const uint8_t *raw = (const uint8_t *)source;
    if (source && raw[1] == 0 && raw[2] != 0) {
        const uint16_t *short_source = (const uint16_t *)source;
        uint16_t *short_copy;
        SIZE_T length = 0;
        while (short_source[length]) length++;
        short_copy = (uint16_t *)kmalloc((uint32_t)((length + 1) * sizeof(uint16_t)));
        if (!short_copy) return 0;
        memcpy(short_copy, short_source, (uint32_t)((length + 1) * sizeof(uint16_t)));
        return (WCHAR *)short_copy;
    }
    int length = k32_wstrlen(source) + 1;
    WCHAR *copy = (WCHAR*)kmalloc((uint32_t)length * sizeof(WCHAR));
    int i;
    if (!copy) return 0;
    for (i = 0; i < length; i++) copy[i] = source[i];
    return copy;
}

long wcstol(LPCWSTR source, LPWSTR *end, int radix) {
    long value = 0;
    int sign = 1;
    (void)radix;
    while (*source == L' ' || *source == L'\t') source++;
    if (*source == L'-') { sign = -1; source++; }
    while (*source >= L'0' && *source <= L'9') value = value * 10 + (*source++ - L'0');
    if (end) *end = (LPWSTR)source;
    return value * sign;
}

void *HeapAlloc(void *heap, uint32_t flags, SIZE_T size) {
    (void)heap;
    void *memory = kmalloc(size);
    if (memory && (flags & HEAP_ZERO_MEMORY)) memset(memory, 0, size);
    return memory;
}

void *HeapReAlloc(void *heap, uint32_t flags, void *memory, SIZE_T size) {
    (void)heap;
    (void)flags;
    return realloc(memory, size);
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

HLOCAL LocalLock(HLOCAL memory) {
    return memory;
}

HLOCAL LocalReAlloc(HLOCAL memory, SIZE_T bytes, UINT flags) {
    (void)flags;
    return (HLOCAL)realloc(memory, bytes);
}

LPSTR lstrcpyA(LPSTR dst, LPCSTR src) {
    int i = 0;
    if (!dst) return dst;
    if (!src) { dst[0] = 0; return dst; }
    do { dst[i] = src[i]; } while (src[i++]);
    return dst;
}

LPSTR lstrcpynA(LPSTR dst, LPCSTR src, int count) {
    int i = 0;
    if (!dst || count <= 0) return dst;
    if (src) while (src[i] && i < count - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
    return dst;
}

int lstrcmpA(LPCSTR a, LPCSTR b) {
    int i = 0;
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (a[i] && a[i] == b[i]) i++;
    return (unsigned char)a[i] == (unsigned char)b[i] ? 0 :
           ((unsigned char)a[i] < (unsigned char)b[i] ? -1 : 1);
}

int lstrcmpiA(LPCSTR a, LPCSTR b) {
    int i = 0;
    char ca, cb;
    if (!a || !b) return a == b ? 0 : (a ? 1 : -1);
    do {
        ca = a[i]; cb = b[i++];
        if (ca >= 'A' && ca <= 'Z') ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z') cb += 'a' - 'A';
        if (ca != cb) return ca < cb ? -1 : 1;
    } while (ca);
    return 0;
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

void *calloc(SIZE_T count,SIZE_T size) {
    SIZE_T total;
    void *memory;
    if (count && size > (SIZE_T)-1 / count) return 0;
    total = count * size;
    memory = kmalloc((uint32_t)total);
    if (memory) memset(memory,0,(uint32_t)total);
    return memory;
}
void *malloc(SIZE_T size) { return kmalloc((uint32_t)size); }
void free(void *memory) { if (memory) kfree(memory); }
void *realloc(void *memory, SIZE_T size) {
    void *replacement;
    if (!memory) return kmalloc((uint32_t)size);
    replacement = kmalloc((uint32_t)size);
    if (replacement) kfree(memory);
    return replacement;
}

LONG InterlockedIncrement(volatile LONG *value) { return __atomic_add_fetch(value,1,__ATOMIC_SEQ_CST); }
LONG InterlockedDecrement(volatile LONG *value) { return __atomic_sub_fetch(value,1,__ATOMIC_SEQ_CST); }
PVOID InterlockedCompareExchangePointer(PVOID volatile *destination,PVOID exchange,PVOID compare){__atomic_compare_exchange_n(destination,&compare,exchange,FALSE,__ATOMIC_SEQ_CST,__ATOMIC_SEQ_CST);return compare;}

DWORD GetFullPathNameW(LPCWSTR path,DWORD size,LPWSTR buffer,LPWSTR *file_part) {
    DWORD needed=(DWORD)k32_wstrlen(path)+1,i;
    if(file_part)*file_part=0;
    if(!path)return 0;
    if(!buffer||size<needed)return needed;
    k32_wstrcpy(buffer,path);
    if(file_part)for(i=0;i+1<needed;i++)if(buffer[i]==L'/'||buffer[i]==L'\\')*file_part=&buffer[i+1];
    return needed-1;
}

void *GetModuleHandleA(const char *name) {
    char upper_name[128];
    if (!name) return g_process_image_base ? g_process_image_base : (void*)0x400000;
    k32_uppercase_copy(name, upper_name, sizeof(upper_name));
    k32_normalize_module_name(upper_name, sizeof(upper_name));
    return PeGetLoadedModuleHandle(upper_name);
}

void *GetModuleHandleW(LPCWSTR name) {
    char upper_name[128];
    if (!name) return g_process_image_base ? g_process_image_base : (void*)0x400000;
    k32_wide_to_ansi_name(name, upper_name, sizeof(upper_name));
    k32_normalize_module_name(upper_name, sizeof(upper_name));
    return PeGetLoadedModuleHandle(upper_name);
}
BOOL FreeLibrary(HMODULE module){(void)module;return TRUE;}
LPVOID MapViewOfFile(HANDLE mapping,DWORD access,DWORD high,DWORD low,SIZE_T bytes){(void)access;(void)high;(void)low;(void)bytes;return mapping;}
BOOL UnmapViewOfFile(LPCVOID address){(void)address;return TRUE;}
BOOL ReadDirectoryChangesW(HANDLE d,LPVOID b,DWORD l,BOOL s,DWORD f,DWORD*r,LPOVERLAPPED o,LPVOID c){(void)d;(void)b;(void)l;(void)s;(void)f;(void)c;if(r)*r=0;if(o&&o->hEvent)ResetEvent(o->hEvent);SetLastError(ERROR_FILE_NOT_FOUND);return FALSE;}
BOOL GetOverlappedResult(HANDLE f,LPOVERLAPPED o,DWORD*t,BOOL w){(void)f;(void)o;(void)w;if(t)*t=0;return FALSE;}
DWORD WaitForMultipleObjects(DWORD count,const HANDLE*h,BOOL all,DWORD ms){(void)all;if(!count)return WAIT_FAILED;return WaitForSingleObject(h[0],ms);}
BOOL FindNextFileW(HANDLE find,LPWIN32_FIND_DATAW data){(void)find;(void)data;SetLastError(ERROR_FILE_NOT_FOUND);return FALSE;}
HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES a,BOOL owner,LPCWSTR name){(void)a;(void)owner;(void)name;return CreateEventW(0,FALSE,TRUE,0);}
int CompareStringW(LCID locale,DWORD flags,LPCWSTR a,int na,LPCWSTR b,int nb){int i=0;(void)locale;while((na<0?a[i]:i<na)&&(nb<0?b[i]:i<nb)){WCHAR x=a[i],y=b[i];if(flags&NORM_IGNORECASE){if(x>=L'a'&&x<=L'z')x-=32;if(y>=L'a'&&y<=L'z')y-=32;}if(x!=y)return x<y?CSTR_LESS_THAN:3;if(!x)break;i++;}return CSTR_EQUAL;}

HMODULE LoadLibraryW(LPCWSTR name) {
    char ansi_name[128];
    HMODULE loaded;
    k32_wide_to_ansi_name(name, ansi_name, sizeof(ansi_name));
    if (!ansi_name[0]) return 0;
    k32_normalize_module_name(ansi_name, sizeof(ansi_name));
    loaded = (HMODULE)PeGetLoadedModuleHandle(ansi_name);
    if (loaded) return loaded;
    return (HMODULE)PeLoadDll(ansi_name);
}

HMODULE LoadLibraryA(const char *name) {
    char upper_name[128];
    HMODULE loaded;
    k32_uppercase_copy(name, upper_name, sizeof(upper_name));
    if (!upper_name[0]) return 0;
    k32_normalize_module_name(upper_name, sizeof(upper_name));
    loaded = (HMODULE)PeGetLoadedModuleHandle(upper_name);
    if (loaded) return loaded;
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
    return 3;
}

DWORD GetCurrentThreadId(void) { return 1; }
HANDLE GetCurrentThread(void) { return (HANDLE)(ULONG_PTR)1; }

HANDLE GetCurrentProcess(void) {
    return (HANDLE)1;
}

HANDLE OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
    (void)dwDesiredAccess;
    (void)bInheritHandle;
    if (dwProcessId == 0 || dwProcessId == 2 || dwProcessId == 3)
        return (HANDLE)(uintptr_t)dwProcessId;
    return NULL;
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
        lpCriticalSection->DebugInfo = 0;
        lpCriticalSection->LockCount = 0;
        lpCriticalSection->RecursionCount = 0;
        lpCriticalSection->OwningThread = 0;
        lpCriticalSection->LockSemaphore = 0;
    }
}

BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION section, DWORD spin, DWORD flags) {
    InitializeCriticalSection(section);
    if (!section) return FALSE;
    section->SpinCount = spin;
    if (flags & RTL_CRITICAL_SECTION_FLAG_FORCE_DEBUG_INFO) {
        section->DebugInfo = (PRTL_CRITICAL_SECTION_DEBUG)kmalloc(sizeof(*section->DebugInfo));
        if (!section->DebugInfo) return FALSE;
        memset(section->DebugInfo, 0, sizeof(*section->DebugInfo));
        section->DebugInfo->CriticalSection = section;
    }
    return TRUE;
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
    char command[512];
    int pos = 0;
    LPCWSTR source = lpCommandLine && lpCommandLine[0] ? lpCommandLine : lpApplicationName;
    (void)lpProcessAttributes; (void)lpThreadAttributes;
    (void)bInheritHandles; (void)dwCreationFlags; (void)lpEnvironment; (void)lpCurrentDirectory; (void)lpStartupInfo;
    if (lpProcessInformation) {
        lpProcessInformation->hProcess = 0;
        lpProcessInformation->hThread = 0;
        lpProcessInformation->dwProcessId = 0;
        lpProcessInformation->dwThreadId = 0;
    }
    if (!source) {
        g_last_error = ERROR_FILE_NOT_FOUND;
        return FALSE;
    }
    while (source[pos] && pos < (int)sizeof(command) - 1) {
        WCHAR ch = source[pos];
        command[pos] = ch < 128 ? (char)ch : '?';
        pos++;
    }
    command[pos] = 0;
    if (CsrssExecuteImage(command) < 0) {
        g_last_error = ERROR_FILE_NOT_FOUND;
        return FALSE;
    }
    /* The launcher is asynchronous, but per-process handles are not exposed
       yet; leave the already-zeroed output handles safely closable. */
    g_last_error = 0;
    return TRUE;
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

HANDLE CreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa,
                   DWORD disposition, DWORD flags, HANDLE template_file) {
    WCHAR wide[260]; int i = 0;
    if (!name) return INVALID_HANDLE_VALUE;
    while (name[i] && i < 259) { wide[i] = (WCHAR)(unsigned char)name[i]; i++; }
    wide[i] = 0;
    return CreateFileW(wide, access, share, sa, disposition, flags, template_file);
}

DWORD GetFileAttributesA(LPCSTR name) {
    HFILE f = _lopen(name, OF_READ);
    if (f == HFILE_ERROR) return INVALID_FILE_ATTRIBUTES;
    _lclose(f);
    return FILE_ATTRIBUTE_NORMAL;
}

DWORD SearchPathA(LPCSTR path, LPCSTR file, LPCSTR extension, DWORD length,
                  LPSTR buffer, LPSTR *part) {
    char candidate[260]; uint32_t n, i = 0;
    (void)path;
    if (!file || !buffer || !length) return 0;
    while (file[i] && i < sizeof(candidate)-1) { candidate[i] = file[i]; i++; }
    candidate[i] = 0;
    if (extension && !strrchr(candidate, '.')) {
        n = strlen(candidate); i = 0;
        while (extension[i] && n + i < sizeof(candidate)-1) { candidate[n+i] = extension[i]; i++; }
        candidate[n+i] = 0;
    }
    if (GetFileAttributesA(candidate) == INVALID_FILE_ATTRIBUTES) return 0;
    n = strlen(candidate);
    if (n + 1 > length) return n + 1;
    for (i=0; i<=n; i++) buffer[i] = candidate[i];
    if (part) *part = (LPSTR)k32_find_last_path_part(buffer);
    return n;
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

int lstrcmpiW(LPCWSTR a, LPCWSTR b) {
    WCHAR ca, cb;
    if (!a) return b ? -1 : 0;
    if (!b) return 1;
    do {
        ca = *a++;
        cb = *b++;
        if (ca >= L'A' && ca <= L'Z') ca += L'a' - L'A';
        if (cb >= L'A' && cb <= L'Z') cb += L'a' - L'A';
        if (ca != cb) return ca < cb ? -1 : 1;
    } while (ca);
    return 0;
}

static int k32_emit_char(char *buffer, size_t count, int pos, char ch) {
    if (buffer && count && (uint32_t)pos < count - 1) buffer[pos] = ch;
    return pos + 1;
}

static int k32_emit_unsigned(char *buffer, size_t count, int pos, ULONGLONG value,
                             unsigned base, int width, char pad) {
    char digits[32];
    int used = 0;
    do {
        unsigned digit = (unsigned)(value % base);
        digits[used++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
        value /= base;
    } while (value && used < (int)sizeof(digits));
    while (width-- > used) pos = k32_emit_char(buffer, count, pos, pad);
    while (used) pos = k32_emit_char(buffer, count, pos, digits[--used]);
    return pos;
}

int vsnprintf(char *buffer, size_t count, const char *format, va_list args) {
    int pos = 0;
    while (format && *format) {
        int width = 0, is_long = 0;
        char pad = ' ';
        if (*format != '%') {
            pos = k32_emit_char(buffer, count, pos, *format++);
            continue;
        }
        format++;
        if (*format == '%') {
            pos = k32_emit_char(buffer, count, pos, *format++);
            continue;
        }
        if (*format == '0') { pad = '0'; format++; }
        while (*format >= '0' && *format <= '9') width = width * 10 + *format++ - '0';
        while (*format == 'l') { is_long++; format++; }
        switch (*format ? *format++ : 0) {
        case 's': {
            if (is_long) {
                const WCHAR *str = va_arg(args, const WCHAR *);
                if (!str) str = L"(null)";
                while (*str) {
                    WCHAR ch = *str++;
                    pos = k32_emit_char(buffer, count, pos, ch < 128 ? (char)ch : '?');
                }
            } else {
                const char *str = va_arg(args, const char *);
                if (!str) str = "(null)";
                while (*str) pos = k32_emit_char(buffer, count, pos, *str++);
            }
            break;
        }
        case 'c': pos = k32_emit_char(buffer, count, pos, (char)va_arg(args, int)); break;
        case 'd':
        case 'i': {
            LONGLONG value = is_long ? va_arg(args, long) : va_arg(args, int);
            ULONGLONG magnitude;
            if (value < 0) {
                pos = k32_emit_char(buffer, count, pos, '-');
                magnitude = (ULONGLONG)(-(value + 1)) + 1;
            } else magnitude = (ULONGLONG)value;
            pos = k32_emit_unsigned(buffer, count, pos, magnitude, 10, width, pad);
            break;
        }
        case 'p':
            pos = k32_emit_char(buffer, count, pos, '0');
            pos = k32_emit_char(buffer, count, pos, 'x');
            pos = k32_emit_unsigned(buffer, count, pos, (ULONG_PTR)va_arg(args, void *), 16,
                                    sizeof(void *) * 2, '0');
            break;
        case 'x':
        case 'X':
            pos = k32_emit_unsigned(buffer, count, pos,
                                    is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int),
                                    16, width, pad);
            break;
        case 'u':
            pos = k32_emit_unsigned(buffer, count, pos,
                                    is_long ? va_arg(args, unsigned long) : va_arg(args, unsigned int),
                                    10, width, pad);
            break;
        default: break;
        }
    }
    if (buffer && count) buffer[(uint32_t)pos < count ? pos : count - 1] = 0;
    return pos;
}

int snprintf(char *buffer, size_t count, const char *format, ...) {
    int result;
    va_list args;
    va_start(args, format);
    result = vsnprintf(buffer, count, format, args);
    va_end(args);
    return result;
}

int sprintf(char *buffer, const char *format, ...) {
    int result;
    va_list args;
    va_start(args, format);
    result = vsnprintf(buffer, 0xffffffffu, format, args);
    va_end(args);
    return result;
}

int printf(const char *format, ...) {
    char buffer[512];
    uint32_t written;
    va_list args;
    int result;
    va_start(args, format);
    result = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    WriteConsoleA(GetStdHandle((uint32_t)-11), buffer, (uint32_t)result, &written, 0);
    return result;
}

DWORD GetCurrentDirectoryA(DWORD size, LPSTR buffer) {
    const char *directory = "/";
    if (!buffer || size == 0) return 1;
    if (size < 2) { buffer[0] = 0; return 1; }
    buffer[0] = directory[0];
    buffer[1] = 0;
    return 1;
}

DWORD GetPrivateProfileStringA(LPCSTR app, LPCSTR key, LPCSTR def,
                               LPSTR buffer, DWORD size, LPCSTR file) {
    (void)app; (void)key; (void)file;
    if (!buffer || !size) return 0;
    lstrcpynA(buffer, def ? def : "", (int)size);
    return strlen(buffer);
}

int GetPrivateProfileIntA(LPCSTR app, LPCSTR key, int def, LPCSTR file) {
    char value[32];
    int result = 0, sign = 1, i = 0;
    (void)GetPrivateProfileStringA(app, key, "", value, sizeof(value), file);
    if (!value[0]) return def;
    if (value[0] == '-') { sign = -1; i++; }
    while (value[i] >= '0' && value[i] <= '9') result = result * 10 + value[i++] - '0';
    return sign * result;
}

BOOL WritePrivateProfileStringA(LPCSTR app, LPCSTR key, LPCSTR value, LPCSTR file) {
    (void)app; (void)key; (void)value; (void)file;
    return TRUE;
}

UINT WinExec(LPCSTR command, UINT show) {
    (void)show;
    return command && CsrssExecuteImage(command) >= 0 ? 33 : 0;
}

static HFILE k32_open_legacy_file(LPCSTR path, int write) {
    WCHAR wide[260];
    int i = 0;
    HANDLE handle;
    if (!path) return HFILE_ERROR;
    while (path[i] && i < (int)(sizeof(wide) / sizeof(wide[0])) - 1) { wide[i] = (WCHAR)(unsigned char)path[i]; i++; }
    wide[i] = 0;
    handle = CreateFileW(wide, write ? GENERIC_WRITE : GENERIC_READ, FILE_SHARE_READ,
                         0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    return handle == INVALID_HANDLE_VALUE ? HFILE_ERROR : (HFILE)(LONG_PTR)handle;
}

HFILE OpenFile(LPCSTR path, LPOFSTRUCT result, UINT style) {
    HFILE file;
    (void)style;
    file = k32_open_legacy_file(path, 0);
    if (result) { result->cBytes = sizeof(*result); result->fFixedDisk = 1; }
    return file;
}
HFILE _lopen(LPCSTR path, int read_write) { return k32_open_legacy_file(path, read_write != 0); }
UINT _lread(HFILE file, LPVOID buffer, UINT bytes) { DWORD done = 0; return ReadFile((HANDLE)(LONG_PTR)file, buffer, bytes, &done, 0) ? done : (UINT)-1; }
UINT _lwrite(HFILE file, LPCVOID buffer, UINT bytes) { DWORD done = 0; return WriteFile((HANDLE)(LONG_PTR)file, (LPVOID)buffer, bytes, &done, 0) ? done : (UINT)-1; }
HFILE _lcreat(LPCSTR path, int attributes) { (void)attributes; return k32_open_legacy_file(path, 1); }
HFILE _lclose(HFILE file) { return CloseHandle((HANDLE)(LONG_PTR)file) ? 0 : HFILE_ERROR; }
LONG _hread(HFILE file, LPVOID buffer, LONG count) { return (LONG)_lread(file, buffer, (UINT)count); }
LONG _llseek(HFILE file, LONG offset, int origin) {
    K32_FILE_HANDLE *handle = k32_file_from_handle((HANDLE)(LONG_PTR)file);
    uint32_t position;
    if (!handle) return HFILE_ERROR;
    if (origin == 0) position = (uint32_t)offset;
    else if (origin == 1) position = handle->pos + (uint32_t)offset;
    else position = handle->size + (uint32_t)offset;
    if (position > handle->size) position = handle->size;
    handle->pos = position;
    return (LONG)position;
}

int sscanf(const char *buffer, const char *format, ...) {
    va_list args;
    int assigned = 0, consumed = 0;
    va_start(args, format);
    while (buffer && format && *format) {
        if (*format != '%') { if (*format == ' ' || *format == '\t') { while (*buffer == ' ' || *buffer == '\t') { buffer++; consumed++; } format++; continue; } if (*buffer++ != *format++) break; consumed++; continue; }
        format++;
        if (*format == 'n') { *va_arg(args, int *) = consumed; format++; continue; }
        if (*format == 'd') { int sign = 1, value = 0, digits = 0; int *out = va_arg(args, int *); format++; while (*buffer == ' ' || *buffer == '\t') { buffer++; consumed++; } if (*buffer == '-') { sign = -1; buffer++; consumed++; } while (*buffer >= '0' && *buffer <= '9') { value = value * 10 + (*buffer++ - '0'); consumed++; digits++; } if (!digits) break; *out = sign * value; assigned++; continue; }
        break;
    }
    va_end(args);
    return assigned;
}

void *memcpy(void *dest, const void *src, uint32_t bytes) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    uint32_t i;
    for (i = 0; i < bytes; i++) d[i] = s[i];
    return dest;
}

void *memset(void *dest, int value, uint32_t bytes) {
    uint8_t *d = (uint8_t *)dest;
    uint32_t i;
    for (i = 0; i < bytes; i++) d[i] = (uint8_t)value;
    return dest;
}

uint32_t strlen(const char *text) {
    uint32_t length = 0;
    while (text && text[length]) length++;
    return length;
}

int strcmp(const char *a, const char *b) { return lstrcmpA(a, b); }
char *strcpy(char *dst, const char *src) { return lstrcpyA(dst, src); }
char *strcat(char *dst, const char *src) { char *p = dst; while (p && *p) p++; if (p) lstrcpyA(p, src); return dst; }
void *memmove(void *dst, const void *src, uint32_t n) { uint8_t *d=dst; const uint8_t *s=src; uint32_t i; if (d==s)return dst; if(d<s){for(i=0;i<n;i++)d[i]=s[i];}else{for(i=n;i;i--)d[i-1]=s[i-1];}return dst; }
char *strchr(const char *s, int c) { if (!s) return 0; while (*s) { if ((unsigned char)*s == (unsigned char)c) return (char *)s; s++; } return c == 0 ? (char *)s : 0; }
char *strrchr(const char *s, int c) { const char *last=0; if(!s)return 0; do {if((unsigned char)*s==(unsigned char)c)last=s;} while(*s++); return (char *)last; }
int strcasecmp(const char *a, const char *b) { unsigned char ca,cb; if(!a||!b)return a==b?0:(a?1:-1); do {ca=(unsigned char)*a++;cb=(unsigned char)*b++;if(ca>='A'&&ca<='Z')ca+=32;if(cb>='A'&&cb<='Z')cb+=32;if(ca!=cb)return ca<cb?-1:1;} while(ca);return 0; }
char *strdup(const char *s) { uint32_t n; char *p; if(!s)return 0; n=strlen(s)+1;p=kmalloc(n);if(p)memcpy(p,s,n);return p; }
long strtol(const char *s, char **end, int base) { long v=0; int neg=0; if(!s){if(end)*end=(char*)s;return 0;} while(*s==' '||*s=='\t')s++;if(*s=='-'){neg=1;s++;}if(base==0)base=(*s=='0'&&(s[1]=='x'||s[1]=='X'))?16:10;if(base==16&&s[0]=='0'&&(s[1]=='x'||s[1]=='X'))s+=2;while(*s){int d=(*s>='0'&&*s<='9')?*s-'0':((*s>='a'&&*s<='f')?*s-'a'+10:(*s>='A'&&*s<='F'?*s-'A'+10:-1));if(d<0||d>=base)break;v=v*base+d;s++;}if(end)*end=(char*)s;return neg?-v:v; }
int _snprintf(char *buffer, size_t count, const char *format, ...) { int r; va_list ap; va_start(ap,format); r=vsnprintf(buffer,count,format,ap); va_end(ap); return r; }

/* Flex emits references to the hosted C stream ABI even though winhlp32
 * supplies YY_INPUT itself.  Export the small stream surface it needs. */
typedef struct _DISCOUNT_FILE FILE;
FILE *stdin = (FILE *)(uintptr_t)1, *stdout = (FILE *)(uintptr_t)2, *stderr = (FILE *)(uintptr_t)3;
int ferror(FILE *stream) { (void)stream; return 0; }
int getc(FILE *stream) { (void)stream; return EOF; }
int clearerr(FILE *stream) { (void)stream; return 0; }
size_t fread(void *buffer, size_t size, size_t count, FILE *stream) { (void)buffer;(void)size;(void)count;(void)stream;return 0; }
size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream) { (void)stream; if(buffer&&size&&count&&stream==stdout) WriteConsoleA(GetStdHandle((uint32_t)-11),buffer,(uint32_t)(size*count),0,0); return count; }
int fprintf(FILE *stream, const char *format, ...) { char buffer[512]; int r; va_list ap; va_start(ap,format); r=vsnprintf(buffer,sizeof(buffer),format,ap); va_end(ap); if(stream==stdout||stream==stderr) fwrite(buffer,1,(size_t)r,stream); return r; }

HRESULT SetThreadDescription(HANDLE thread, LPCWSTR description) {
    (void)thread; (void)description;
    return S_OK;
}

ULONGLONG __udivdi3(ULONGLONG num, ULONGLONG den) {
    return k32_udivmod64(num, den, 0);
}

ULONGLONG __umoddi3(ULONGLONG num, ULONGLONG den) {
    ULONGLONG rem = 0;
    k32_udivmod64(num, den, &rem);
    return rem;
}
