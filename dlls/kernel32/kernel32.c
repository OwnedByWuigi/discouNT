// kernel32.c - Win32 API implementation for NT-like OS
#include <stdint.h>

__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule; (void)lpReserved; (void)reason;
    return 1;
}

// Console functions
__attribute__((stdcall)) void *GetStdHandle(uint32_t handle) {
    (void)handle;
    return (void*)0xB8000; // VGA text buffer as stdout
}

__attribute__((stdcall)) int WriteConsoleA(void *handle, const char *buf, uint32_t len, 
                                              uint32_t *written, void *reserved) {
    (void)handle; (void)reserved;
    extern void HalPutString(const char *str, uint8_t color);
    // Print the buffer
    for (uint32_t i = 0; i < len; i++) {
        extern void HalPutChar(char c, uint8_t color);
        HalPutChar(buf[i], 0x0F);
    }
    if (written) *written = len;
    return 1;
}

__attribute__((stdcall)) void ExitProcess(uint32_t code) {
    (void)code;
    while(1) __asm__ volatile("hlt");
}

// Memory functions
__attribute__((stdcall)) void *HeapAlloc(void *heap, uint32_t flags, uint32_t size) {
    (void)heap; (void)flags;
    extern void *kmalloc(uint32_t size);
    return kmalloc(size);
}

__attribute__((stdcall)) int HeapFree(void *heap, uint32_t flags, void *ptr) {
    (void)heap; (void)flags;
    extern void kfree(void *ptr);
    kfree(ptr);
    return 1;
}

__attribute__((stdcall)) void *GetProcessHeap(void) {
    return (void*)1; // Fake heap handle
}

__attribute__((stdcall)) void *GetModuleHandleA(const char *name) {
    (void)name;
    return (void*)0x400000; // Return image base
}

__attribute__((stdcall)) uint32_t GetVersion(void) {
    return 0x00050001;
}

__attribute__((stdcall)) uint32_t GetTickCount(void) {
    return 0; // No timer yet
}

__attribute__((stdcall)) void Sleep(uint32_t ms) {
    for (volatile uint32_t i = 0; i < ms * 1000; i++) __asm__ volatile("");
}