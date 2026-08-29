#include <stdint.h>
#include <string.h>
#include "wininet.h"

typedef struct {
    uint32_t kind;
    uint32_t position;
    uint32_t length;
    const uint8_t *body;
} WININET_NATIVE_HANDLE;

static WININET_NATIVE_HANDLE *wininet_alloc(uint32_t kind) {
    extern void *kmalloc(uint32_t);
    WININET_NATIVE_HANDLE *handle = (WININET_NATIVE_HANDLE *)kmalloc(sizeof(*handle));
    if (!handle) return 0;
    memset(handle, 0, sizeof(*handle));
    handle->kind = kind;
    return handle;
}

int DllMain(void *module, uint32_t reason, void *reserved) {
    (void)module; (void)reason; (void)reserved;
    return TRUE;
}

HINTERNET InternetOpenA(LPCSTR agent, DWORD access, LPCSTR proxy, LPCSTR bypass, DWORD flags) {
    (void)agent; (void)access; (void)proxy; (void)bypass; (void)flags;
    return (HINTERNET)wininet_alloc(1);
}

HINTERNET InternetOpenW(LPCWSTR agent, DWORD access, LPCWSTR proxy, LPCWSTR bypass, DWORD flags) {
    (void)agent; (void)access; (void)proxy; (void)bypass; (void)flags;
    return (HINTERNET)wininet_alloc(1);
}

HINTERNET InternetOpenUrlA(HINTERNET root, LPCSTR url, LPCSTR headers, DWORD length, DWORD flags, uintptr_t context) {
    (void)root; (void)url; (void)headers; (void)length; (void)flags; (void)context;
    return (HINTERNET)wininet_alloc(2);
}

HINTERNET InternetOpenUrlW(HINTERNET root, LPCWSTR url, LPCWSTR headers, DWORD length, DWORD flags, uintptr_t context) {
    (void)root; (void)url; (void)headers; (void)length; (void)flags; (void)context;
    return (HINTERNET)wininet_alloc(2);
}

BOOL InternetReadFile(HINTERNET internet, void *buffer, DWORD length, DWORD *read) {
    WININET_NATIVE_HANDLE *handle = (WININET_NATIVE_HANDLE *)internet;
    DWORD available;
    if (read) *read = 0;
    if (!handle || !buffer || !read) return FALSE;
    available = handle->length - handle->position;
    if (available > length) available = length;
    if (available && handle->body) memcpy(buffer, handle->body + handle->position, available);
    handle->position += available;
    *read = available;
    return TRUE;
}

BOOL InternetCloseHandle(HINTERNET internet) {
    extern void kfree(void *);
    if (!internet) return FALSE;
    kfree(internet);
    return TRUE;
}

HINTERNET InternetConnectA(HINTERNET root, LPCSTR server, uint16_t port, LPCSTR user, LPCSTR password, DWORD service, DWORD flags, uintptr_t context) {
    (void)root; (void)server; (void)port; (void)user; (void)password; (void)service; (void)flags; (void)context;
    return (HINTERNET)wininet_alloc(3);
}

HINTERNET InternetConnectW(HINTERNET root, LPCWSTR server, uint16_t port, LPCWSTR user, LPCWSTR password, DWORD service, DWORD flags, uintptr_t context) {
    (void)root; (void)server; (void)port; (void)user; (void)password; (void)service; (void)flags; (void)context;
    return (HINTERNET)wininet_alloc(3);
}

HINTERNET HttpOpenRequestA(HINTERNET connect, LPCSTR verb, LPCSTR object, LPCSTR version, LPCSTR referer, const LPCSTR *accept, DWORD flags, uintptr_t context) {
    (void)connect; (void)verb; (void)object; (void)version; (void)referer; (void)accept; (void)flags; (void)context;
    return (HINTERNET)wininet_alloc(4);
}

HINTERNET HttpOpenRequestW(HINTERNET connect, LPCWSTR verb, LPCWSTR object, LPCWSTR version, LPCWSTR referer, const LPCWSTR *accept, DWORD flags, uintptr_t context) {
    (void)connect; (void)verb; (void)object; (void)version; (void)referer; (void)accept; (void)flags; (void)context;
    return (HINTERNET)wininet_alloc(4);
}

BOOL HttpSendRequestA(HINTERNET request, LPCSTR headers, DWORD length, void *optional, DWORD optional_length) {
    (void)request; (void)headers; (void)length; (void)optional; (void)optional_length;
    return FALSE;
}

BOOL HttpSendRequestW(HINTERNET request, LPCWSTR headers, DWORD length, void *optional, DWORD optional_length) {
    (void)request; (void)headers; (void)length; (void)optional; (void)optional_length;
    return FALSE;
}

BOOL InternetQueryInfoA(HINTERNET internet, DWORD level, void *buffer, DWORD *length, DWORD *index) {
    (void)internet; (void)level; (void)buffer; (void)index;
    if (length) *length = 0;
    return FALSE;
}

BOOL InternetQueryInfoW(HINTERNET internet, DWORD level, void *buffer, DWORD *length, DWORD *index) {
    return InternetQueryInfoA(internet, level, buffer, length, index);
}
