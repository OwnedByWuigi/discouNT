#include <stdint.h>
#include "kernel32.h"
#include "hal.h"
#include "vga.h"
#include "util.h"

static int kernel32_initialized = 0;

void Kernel32Init(void) {
    kernel32_initialized = 1;
}

HANDLE GetStdHandle(DWORD nStdHandle) {
    if (nStdHandle == STD_OUTPUT_HANDLE) {
        return (HANDLE)0xB8000; // VGA text mode address as "handle"
    }
    return (HANDLE)0;
}

DWORD WriteConsoleA(HANDLE hConsole, const void *lpBuffer, DWORD nChars, 
                     DWORD *lpCharsWritten, void *lpReserved) {
    (void)hConsole;
    (void)lpReserved;
    
    const char *str = (const char*)lpBuffer;
    for (DWORD i = 0; i < nChars; i++) {
        VgaDrawChar(10 + i * 8, 400, str[i], COLOR_WHITE, COLOR_BLUE);
    }
    
    if (lpCharsWritten) *lpCharsWritten = nChars;
    return nChars;
}

void ExitProcess(DWORD exitCode) {
    (void)exitCode;
    HalPutString("[kernel32] Process exit called\n", 0x0C);
    while(1) __asm__ volatile("hlt");
}

int lstrlenA(LPCSTR lpString) {
    return strlen(lpString);
}

LPSTR lstrcpyA(LPSTR lpDest, LPCSTR lpSrc) {
    strcpy(lpDest, lpSrc);
    return lpDest;
}

int lstrcmpA(LPCSTR lpString1, LPCSTR lpString2) {
    return strcmp(lpString1, lpString2);
}