// kernel/user32.c
#include <stdint.h>
#include "user32.h"
#include "win32k.h"
#include "gdi32.h"
#include "hal.h"
#include "mm.h"
#include "util.h"

HANDLE CreateWindow(const char *className, const char *title, int x, int y, int w, int h) {
    return Win32kCreateWindow(className, title, x, y, w, h, WS_OVERLAPPEDWINDOW);
}

HANDLE CreateWindowEx(uint32_t exStyle, const char *className, const char *title,
                       uint32_t style, int x, int y, int w, int h) {
    (void)exStyle;
    return Win32kCreateWindow(className, title, x, y, w, h, style);
}

void ShowWindow(HANDLE hwnd) {
    Win32kShowWindow(hwnd);
}

void UpdateWindow(HANDLE hwnd) {
    Win32kUpdateWindow(hwnd);
}

void SetWindowText(HANDLE hwnd, const char *text) {
    Win32kSetWindowText(hwnd, text);
}

void GetClientRect(HANDLE hwnd, RECT *rect) {
    Win32kGetClientRect(hwnd, rect);
}

void MessageBox(HANDLE hwnd, const char *text, const char *caption, uint32_t type) {
    (void)type;
    
    int x = 20, y = 5, w = 40, h = 8;
    HANDLE msgBox = Win32kCreateWindow("MessageBox", caption, x, y, w, h, WS_OVERLAPPED | WS_VISIBLE);
    Win32kShowWindow(msgBox);
    
    // Draw text inside
    HANDLE dc = GdiGetDC(msgBox);
    GdiDrawText(dc, 2, 2, text, -1);
    GdiDrawText(dc, (w - 4) / 2 - 2, h - 3, "[OK]", 4);
    GdiReleaseDC(dc);
    
    (void)hwnd;
}