#include <stdint.h>
#include "win32k.h"
#include "mm.h"
#include "vga.h"
#include "util.h"

void Win32kInit(void) {
    VgaInit();
    VgaClearScreen(COLOR_BLUE);
    VgaSwapBuffers();
}

HANDLE Win32kRegisterClass(const char *className, uint32_t style, 
                            void (*wndProc)(HANDLE, uint32_t, uint32_t, uint32_t)) {
    WNDCLASS *wndClass = (WNDCLASS*)kmalloc(sizeof(WNDCLASS));
    if (!wndClass) return INVALID_HANDLE;
    
    memset(wndClass, 0, sizeof(WNDCLASS));
    int len = strlen(className);
    if (len >= 63) len = 63;
    memcpy(wndClass->className, className, len);
    wndClass->style = style;
    wndClass->wndProc = wndProc;
    
    return ObCreateObject(OBJ_TYPE_WINDOW, className, wndClass, sizeof(WNDCLASS));
}

HANDLE Win32kCreateWindow(const char *className, const char *title,
                           int x, int y, int w, int h, uint32_t style) {
    HANDLE classHandle = ObFindObject(className, OBJ_TYPE_WINDOW);
    if (classHandle == INVALID_HANDLE) return INVALID_HANDLE;
    
    WNDCLASS *wndClass = (WNDCLASS*)ObReferenceObject(classHandle);
    if (!wndClass) return INVALID_HANDLE;
    
    WINDOW *window = (WINDOW*)kmalloc(sizeof(WINDOW));
    if (!window) {
        ObDereferenceObject(classHandle);
        return INVALID_HANDLE;
    }
    
    memset(window, 0, sizeof(WINDOW));
    int len = strlen(title);
    if (len >= 63) len = 63;
    memcpy(window->title, title, len);
    window->x = x;
    window->y = y;
    window->width = w;
    window->height = h;
    window->style = style;
    window->wndClass = wndClass;
    window->wndProc = wndClass->wndProc;
    
    HANDLE hwnd = ObCreateObject(OBJ_TYPE_WINDOW, title, window, sizeof(WINDOW));
    
    if (window->wndProc) {
        window->wndProc(hwnd, WM_CREATE, 0, 0);
    }
    
    ObDereferenceObject(classHandle);
    return hwnd;
}

void Win32kShowWindow(HANDLE hwnd) {
    WINDOW *window = (WINDOW*)ObReferenceObject(hwnd);
    if (!window) return;
    
    window->visible = 1;
    int x = window->x, y = window->y;
    int w = window->width, h = window->height;
    
    // Window background (light gray)
    VgaFillRect(x, y, w, h, COLOR_LIGHT_GRAY);
    
    // Title bar (dark blue)
    if (window->style & WS_CAPTION) {
        VgaFillRect(x, y, w, 20, COLOR_DARK_GRAY);
        VgaFillRect(x, y + 18, w, 2, COLOR_WHITE);
        
        // Title text
        VgaDrawString(x + 5, y + 4, window->title, COLOR_WHITE, COLOR_DARK_GRAY);
        
        // Close button (X)
        VgaFillRect(x + w - 20, y + 2, 16, 16, COLOR_RED);
        VgaDrawString(x + w - 17, y + 3, "X", COLOR_WHITE, COLOR_RED);
        
        // Minimize button
        if (window->style & WS_MINIMIZEBOX) {
            VgaFillRect(x + w - 40, y + 2, 16, 16, COLOR_DARK_GRAY);
            VgaDrawString(x + w - 37, y + 5, "_", COLOR_WHITE, COLOR_DARK_GRAY);
        }
    }
    
    // Window border
    VgaDrawRect(x, y, w, h, COLOR_BLACK);
    
    ObDereferenceObject(hwnd);
}

void Win32kUpdateWindow(HANDLE hwnd) {
    WINDOW *window = (WINDOW*)ObReferenceObject(hwnd);
    if (!window) return;
    
    if (window->wndProc) {
        window->wndProc(hwnd, WM_PAINT, 0, 0);
    }
    
    ObDereferenceObject(hwnd);
}

void Win32kSetWindowText(HANDLE hwnd, const char *text) {
    WINDOW *window = (WINDOW*)ObReferenceObject(hwnd);
    if (!window) return;
    
    int len = strlen(text);
    if (len >= 63) len = 63;
    memcpy(window->title, text, len);
    window->title[len] = 0;
    
    if (window->visible) {
        Win32kShowWindow(hwnd);
    }
    
    ObDereferenceObject(hwnd);
}

void Win32kGetClientRect(HANDLE hwnd, RECT *rect) {
    WINDOW *window = (WINDOW*)ObReferenceObject(hwnd);
    if (!window || !rect) return;
    
    rect->left = 2;
    rect->top = (window->style & WS_CAPTION) ? 22 : 2;
    rect->right = window->width - 2;
    rect->bottom = window->height - 2;
    
    ObDereferenceObject(hwnd);
}