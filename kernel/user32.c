#include <stdint.h>
#include "user32.h"
#include "win32k.h"
#include "vga.h"
#include "mm.h"
#include "util.h"

HANDLE CreateWindow(const char *className, const char *title, int x, int y, int w, int h) {
    return Win32kCreateWindow(className, title, x, y, w, h, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
}

HANDLE CreateWindowEx(uint32_t exStyle, const char *className, const char *title,
                       uint32_t style, int x, int y, int w, int h) {
    (void)exStyle;
    return Win32kCreateWindow(className, title, x, y, w, h, style | WS_VISIBLE);
}

void ShowWindow(HANDLE hwnd) {
    Win32kShowWindow(hwnd);
    VgaSwapBuffers();
}

void UpdateWindow(HANDLE hwnd) {
    Win32kUpdateWindow(hwnd);
    VgaSwapBuffers();
}

void SetWindowText(HANDLE hwnd, const char *text) {
    Win32kSetWindowText(hwnd, text);
    VgaSwapBuffers();
}

void GetClientRect(HANDLE hwnd, RECT *rect) {
    Win32kGetClientRect(hwnd, rect);
}

void MessageBox(HANDLE hwnd, const char *text, const char *caption, uint32_t type) {
    (void)type;
    
    int w = 300;
    int h = 120;
    int x = (VGA_WIDTH - w) / 2;
    int y = (VGA_HEIGHT - h) / 2;
    
    HANDLE msgBox = Win32kCreateWindow("MessageBox", caption, x, y, w, h, 
                                        WS_OVERLAPPED | WS_VISIBLE | WS_CAPTION | WS_SYSMENU);
    Win32kShowWindow(msgBox);
    
    VgaDrawString(x + 10, y + 30, text, COLOR_BLACK, COLOR_LIGHT_GRAY);
    
    // OK button
    int bx = x + (w - 60) / 2;
    int by = y + h - 40;
    VgaFillRect(bx, by, 60, 24, COLOR_DARK_GRAY);
    VgaDrawRect(bx, by, 60, 24, COLOR_BLACK);
    VgaDrawString(bx + 15, by + 5, "OK", COLOR_WHITE, COLOR_DARK_GRAY);
    
    VgaSwapBuffers();
    ObDereferenceObject(msgBox);
    
    (void)hwnd;
}