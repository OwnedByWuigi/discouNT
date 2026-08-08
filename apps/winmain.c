#include <stdint.h>
#include "../kernel/object.h"
#include "../kernel/win32k.h"
#include "../kernel/vga.h"
#include "../kernel/user32.h"
#include "../kernel/ke.h"
#include "../kernel/util.h"

void WndProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam);

void WinMain(void) {
    // Draw desktop
    VgaClearScreen(COLOR_BLUE);
    
    // Desktop icons
    VgaDrawString(20, 460, "NT-like OS v0.3 - VGA Graphics Mode", COLOR_WHITE, COLOR_BLUE);
    VgaSwapBuffers();
    
    // Register classes
    Win32kRegisterClass("MainWindow", 0, WndProc);
    Win32kRegisterClass("MessageBox", 0, WndProc);
    
    // Create main window
    HANDLE hwnd = CreateWindow("MainWindow", "NT-like OS - Win32 Demo", 50, 50, 540, 400);
    ShowWindow(hwnd);
    
    // Paint the window content
    UpdateWindow(hwnd);
    
    // Show message box
    MessageBox(0, "Welcome to NT-like OS!\n\nVGA Graphics Mode: 640x480x16\nWin32 Subsystem Active", 
               "Welcome", 0);
    
    while(1) {
        KeYield();
    }
}

void WndProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    if (msg == WM_PAINT) {
        WINDOW *window = (WINDOW*)ObReferenceObject(hwnd);
        if (!window) return;
        
        int x = window->x;
        int y = window->y;
        
        // Client area
        int cx = x + 5;
        int cy = y + 25;
        
        VgaDrawString(cx, cy, "Win32 Window in VGA Graphics Mode!", COLOR_BLACK, COLOR_LIGHT_GRAY);
        VgaDrawString(cx, cy + 15, "Resolution: 640x480x16 colors", COLOR_BLACK, COLOR_LIGHT_GRAY);
        VgaDrawString(cx, cy + 35, "==================================", COLOR_BLACK, COLOR_LIGHT_GRAY);
        VgaDrawString(cx, cy + 55, "Features:", COLOR_RED, COLOR_LIGHT_GRAY);
        VgaDrawString(cx + 10, cy + 75, "* Window Manager (Win32k.sys)", COLOR_BLACK, COLOR_LIGHT_GRAY);
        VgaDrawString(cx + 10, cy + 95, "* GDI Graphics Primitives", COLOR_BLACK, COLOR_LIGHT_GRAY);
        VgaDrawString(cx + 10, cy + 115, "* User32 Window API", COLOR_BLACK, COLOR_LIGHT_GRAY);
        VgaDrawString(cx + 10, cy + 135, "* Object Manager", COLOR_BLACK, COLOR_LIGHT_GRAY);
        VgaDrawString(cx + 10, cy + 155, "* Kernel Executive (Threads)", COLOR_BLACK, COLOR_LIGHT_GRAY);
        
        // Draw a demonstration rectangle
        VgaFillRect(cx + 200, cy + 55, 150, 100, COLOR_CYAN);
        VgaDrawRect(cx + 200, cy + 55, 150, 100, COLOR_BLACK);
        VgaDrawString(cx + 210, cy + 90, "GDI Demo", COLOR_BLACK, COLOR_CYAN);
        
        ObDereferenceObject(hwnd);
    }
    
    (void)wParam;
    (void)lParam;
}