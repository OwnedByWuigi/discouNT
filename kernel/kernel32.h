#ifndef KERNEL32_H
#define KERNEL32_H
#include <stdint.h>

// Minimal kernel32 API
typedef uint32_t DWORD;
typedef uint32_t HANDLE;
typedef const char *LPCSTR;
typedef char *LPSTR;

// Console functions
HANDLE GetStdHandle(DWORD nStdHandle);
DWORD WriteConsoleA(HANDLE hConsole, const void *lpBuffer, DWORD nChars, 
                     DWORD *lpCharsWritten, void *lpReserved);
void ExitProcess(DWORD exitCode);

// String functions (these link to our existing util.c functions)
int lstrlenA(LPCSTR lpString);
LPSTR lstrcpyA(LPSTR lpDest, LPCSTR lpSrc);
int lstrcmpA(LPCSTR lpString1, LPCSTR lpString2);

#define STD_OUTPUT_HANDLE ((DWORD)-11)

void Kernel32Init(void);
#endif