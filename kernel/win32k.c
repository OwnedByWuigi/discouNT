// kernel/win32k.c
#include <stdint.h>
#include "win32k.h"
#include "mm.h"
#include "hal.h"
#include "util.h"

void Win32kInit(void) {
    HalPutString("[Win32k] Window Manager initialized\n", 0x0A);
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
    
    char objName[128];
    strcpy(objName, "WndClass_");
    strcat(objName, className);
    
    return ObCreateObject(OBJ_TYPE_WINDOW, objName, wndClass, sizeof(WNDCLASS));
}

HANDLE Win32kCreateWindow(const char *className, const char *title,
                          int x, int y, int w, int h, uint32_t style) {
    // Find the window class
    char objName[128];
    strcpy(objName, "WndClass_");
    strcat(objName, className);
    HANDLE classHandle = ObFindObject(objName, OBJ_TYPE_WINDOW);
    
    WNDCLASS *wndClass = (WNDCLASS*)ObReferenceObject(classHandle);
    if (!wndClass) return INVALID_HANDLE;
    
    // Create window
    WINDOW *window = (WINDOW*)kmalloc(sizeof(WINDOW));
    if (!window) return INVALID_HANDLE;
    
    memset(window, 0, sizeof(WINDOW));
    int len = strlen(title);
    if (len >= 63) len = 63;
    memcpy(window->title, title, len);
    window->x = x;
    window->y = y;
    window->width = w;
    window->height = h;
    window->style = style;
    window->visible = 0;
    window->wndClass = wndClass;
    window->wndProc = wndClass->wndProc;
    
    HANDLE hwnd = ObCreateObject(OBJ_TYPE_WINDOW, title, window, sizeof(WINDOW));
    if (hwnd == INVALID_HANDLE) return INVALID_HANDLE;
    
    // Send WM_CREATE
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
    
    // Draw window frame
    int x = window->x, y = window->y;
    int w = window->width, h = window->height;
    
    // Draw border
    for (int i = 0; i < w; i++) {
        HalSetCursor(x + i, y);
        HalPutChar(0xCD, 0x1F);  // Top border
        
        if (h > 1) {
            HalSetCursor(x + i, y + h - 1);
            HalPutChar(0xCD, 0x1F);  // Bottom border
        }
    }
    
    for (int i = 1; i < h - 1; i++) {
        HalSetCursor(x, y + i);
        HalPutChar(0xBA, 0x1F);  // Left border
        
        HalSetCursor(x + w - 1, y + i);
        HalPutChar(0xBA, 0x1F);  // Right border
    }
    
    // Draw corners
    HalSetCursor(x, y);
    HalPutChar(0xC9, 0x1F);  // Top-left
    HalSetCursor(x + w - 1, y);
    HalPutChar(0xBB, 0x1F);  // Top-right
    if (h > 1) {
        HalSetCursor(x, y + h - 1);
        HalPutChar(0xC8, 0x1F);  // Bottom-left
        HalSetCursor(x + w - 1, y + h - 1);
        HalPutChar(0xBC, 0x1F);  // Bottom-right
    }
    
    // Draw title
    if (w > 2) {
        HalSetCursor(x + 1, y);
        HalPutChar(' ', 0x1F);
        for (int i = 0; i < w - 2 && window->title[i]; i++) {
            HalPutChar(window->title[i], 0x1F);
        }
    }
    
    ObDereferenceObject(hwnd);
}

void Win32kUpdateWindow(HANDLE hwnd) {
    WINDOW *window = (WINDOW*)ObReferenceObject(hwnd);
    if (!window) return;
    
    // Send WM_PAINT
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
    
    // Redraw window
    if (window->visible) {
        Win32kShowWindow(hwnd);
    }
    
    ObDereferenceObject(hwnd);
}

void Win32kGetClientRect(HANDLE hwnd, RECT *rect) {
    WINDOW *window = (WINDOW*)ObReferenceObject(hwnd);
    if (!window || !rect) return;
    
    rect->left = 1;  // Inside borders
    rect->top = 1;
    rect->right = window->width - 1;
    rect->bottom = window->height - 1;
    
    ObDereferenceObject(hwnd);
}

void Win32kDefWindowProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    (void)hwnd;
    (void)msg;
    (void)wParam;
    (void)lParam;
    // Default message handling
}