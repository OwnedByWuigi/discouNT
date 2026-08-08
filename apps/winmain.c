// apps/winmain.c
#include <stdint.h>
#include "../kernel/object.h"
#include "../kernel/win32k.h"
#include "../kernel/gdi32.h"
#include "../kernel/user32.h"
#include "../kernel/ke.h"

// Forward declarations
void WndProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
extern void KeYield(void);

void WinMain(void) {
    // Register window class
    Win32kRegisterClass("MyWindowClass", 0, WndProc);
    
    // Create main window
    HANDLE hwnd = CreateWindow("MyWindowClass", "NT-like OS - Win32 Demo", 10, 2, 60, 15);
    
    // Show the window
    ShowWindow(hwnd);
    UpdateWindow(hwnd);
    
    // Show a message box
    MessageBox(0, "Welcome to NT-like OS!\nWin32 Subsystem Active", "Welcome", MB_OK);
    
    // Message loop (simplified)
    while(1) {
        KeYield();
    }
}

void WndProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    if (msg == WM_PAINT) {
        HANDLE dc = GdiGetDC(hwnd);
        GdiDrawText(dc, 2, 2, "This is a Win32 Window!", -1);
        GdiDrawText(dc, 2, 3, "Minimal Win32 Subsystem", -1);
        GdiDrawText(dc, 2, 5, "Features:", -1);
        GdiDrawText(dc, 4, 6, "- Window Manager (Win32k)", -1);
        GdiDrawText(dc, 4, 7, "- GDI (Graphics)", -1);
        GdiDrawText(dc, 4, 8, "- User32 (UI)", -1);
        GdiDrawText(dc, 4, 9, "- Kernel32 (Base API)", -1);
        GdiDrawText(dc, 4, 11, "Process: System", -1);
        GdiDrawText(dc, 4, 12, "Architecture: NT-like", -1);
        GdiReleaseDC(dc);
    }
    
    (void)wParam;
    (void)lParam;
}