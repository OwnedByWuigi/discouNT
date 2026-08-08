#include <stdint.h>
#include "win32k.h"
#include "vga.h"
#include "mm.h"
#include "util.h"
#include "serial.h"

// Window list
#define MAX_WINDOWS 32
static HANDLE window_list[MAX_WINDOWS];
static int window_count = 0;

// Dragging state
static int dragging = 0;
static HANDLE drag_window = INVALID_HANDLE;
static int drag_offset_x = 0;
static int drag_offset_y = 0;

void Win32kInit(void) {
    VgaInit();
    VgaClearScreen(COLOR_BLUE);
    window_count = 0;
    dragging = 0;
    drag_window = INVALID_HANDLE;
}

HANDLE Win32kRegisterClass(const char *className, uint32_t style, void (*wndProc)(HANDLE, uint32_t, uint32_t, uint32_t)) {
    WNDCLASS *wc = (WNDCLASS*)kmalloc(sizeof(WNDCLASS));
    memset(wc, 0, sizeof(WNDCLASS));
    int len = strlen(className);
    if (len > 63) len = 63;
    memcpy(wc->className, className, len);
    wc->style = style;
    wc->wndProc = wndProc;
    return ObCreateObject(OBJ_TYPE_WINDOW, className, wc, sizeof(WNDCLASS));
}

HANDLE Win32kCreateWindow(const char *className, const char *title, int x, int y, int w, int h, uint32_t style) {
    HANDLE hClass = ObFindObject(className, OBJ_TYPE_WINDOW);
    if (hClass == INVALID_HANDLE) return INVALID_HANDLE;
    
    WNDCLASS *wc = (WNDCLASS*)ObReferenceObject(hClass);
    if (!wc) return INVALID_HANDLE;
    
    WINDOW *win = (WINDOW*)kmalloc(sizeof(WINDOW));
    memset(win, 0, sizeof(WINDOW));
    int len = strlen(title);
    if (len > 63) len = 63;
    memcpy(win->title, title, len);
    win->x = x; win->y = y;
    win->width = w; win->height = h;
    win->style = style;
    win->visible = 0;
    win->wndClass = wc;
    win->wndProc = wc->wndProc;
    
    HANDLE hwnd = ObCreateObject(OBJ_TYPE_WINDOW, title, win, sizeof(WINDOW));
    
    // Add to window list
    if (window_count < MAX_WINDOWS) {
        window_list[window_count++] = hwnd;
    }
    
    if (win->wndProc) win->wndProc(hwnd, WM_CREATE, 0, 0);
    ObDereferenceObject(hClass);
    return hwnd;
}

void Win32kDestroyWindow(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    
    // Remove from window list
    for (int i = 0; i < window_count; i++) {
        if (window_list[i] == hwnd) {
            window_list[i] = window_list[window_count - 1];
            window_count--;
            break;
        }
    }
    
    if (win->wndProc) win->wndProc(hwnd, WM_DESTROY, 0, 0);
    ObDereferenceObject(hwnd);
    ObDereferenceObject(hwnd); // Second deref to destroy
}

void Win32kShowWindow(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    win->visible = 1;
    
    int x = win->x, y = win->y, w = win->width, h = win->height;
    
    // Window background
    VgaFillRect(x, y, w, h, COLOR_LIGHT_GRAY);
    
    // Title bar
    if (win->style & WS_CAPTION) {
        VgaFillRect(x, y, w, 18, COLOR_DARK_GRAY);
        VgaDrawString(x + 4, y + 2, win->title, COLOR_WHITE, COLOR_DARK_GRAY);
        
        // Close button (X)
        VgaFillRect(x + w - 18, y + 2, 14, 14, COLOR_RED);
        VgaDrawChar(x + w - 15, y + 3, 'X', COLOR_WHITE, COLOR_RED);
    }
    
    // Border
    VgaDrawRect(x, y, w, h, COLOR_BLACK);
    
    ObDereferenceObject(hwnd);
}

void Win32kUpdateWindow(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (win && win->wndProc) {
        win->wndProc(hwnd, WM_PAINT, 0, 0);
    }
    if (win) ObDereferenceObject(hwnd);
}

void Win32kGetClientRect(HANDLE hwnd, RECT *rect) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (win && rect) {
        rect->left = 2;
        rect->top = (win->style & WS_CAPTION) ? 20 : 2;
        rect->right = win->width - 2;
        rect->bottom = win->height - 2;
    }
    if (win) ObDereferenceObject(hwnd);
}

// Check if point is in a window's close button
static int is_close_button(WINDOW *win, int x, int y) {
    if (!(win->style & WS_CAPTION)) return 0;
    int bx = win->x + win->width - 18;
    int by = win->y + 2;
    return (x >= bx && x < bx + 14 && y >= by && y < by + 14);
}

// Check if point is in a window's title bar
static int is_title_bar(WINDOW *win, int x, int y) {
    if (!(win->style & WS_CAPTION)) return 0;
    int tx = win->x;
    int ty = win->y;
    int tw = win->width;
    int th = 18;
    // Exclude close button area
    if (x >= tx + tw - 20 && y >= ty && y < ty + th) return 0;
    return (x >= tx && x < tx + tw && y >= ty && y < ty + th);
}

// Check if point is in a window
static int is_in_window(WINDOW *win, int x, int y) {
    return (x >= win->x && x < win->x + win->width &&
            y >= win->y && y < win->y + win->height);
}

// Find topmost window at position
static HANDLE find_window_at(int x, int y) {
    // Search backwards (topmost first)
    for (int i = window_count - 1; i >= 0; i--) {
        WINDOW *win = (WINDOW*)ObReferenceObject(window_list[i]);
        if (win && win->visible) {
            int found = is_in_window(win, x, y);
            ObDereferenceObject(window_list[i]);
            if (found) return window_list[i];
        }
    }
    return INVALID_HANDLE;
}

void Win32kHandleMouseDown(int x, int y, int button) {
    if (button != 1) return; // Left button only
    
    HANDLE hwnd = find_window_at(x, y);
    if (hwnd == INVALID_HANDLE) return;
    
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    
    // Check close button first
    if (is_close_button(win, x, y)) {
        SerialPutString("[Win32k] Close button clicked\r\n");
        ObDereferenceObject(hwnd);
        Win32kDestroyWindow(hwnd);
        Win32kRedrawAll();
        return;
    }
    
    // Check title bar for dragging
    if (is_title_bar(win, x, y)) {
        SerialPutString("[Win32k] Start drag\r\n");
        dragging = 1;
        drag_window = hwnd;
        drag_offset_x = x - win->x;
        drag_offset_y = y - win->y;
    }
    
    ObDereferenceObject(hwnd);
}

void Win32kHandleMouseUp(int x, int y, int button) {
    (void)x; (void)y;
    if (button != 1) return;
    
    if (dragging) {
        SerialPutString("[Win32k] End drag\r\n");
        dragging = 0;
        drag_window = INVALID_HANDLE;
    }
}

void Win32kHandleMouseMove(int x, int y) {
    if (!dragging || drag_window == INVALID_HANDLE) return;
    
    WINDOW *win = (WINDOW*)ObReferenceObject(drag_window);
    if (!win) {
        dragging = 0;
        return;
    }
    
    int new_x = x - drag_offset_x;
    int new_y = y - drag_offset_y;
    
    // Clamp to screen
    if (new_x < 0) new_x = 0;
    if (new_y < 0) new_y = 0;
    if (new_x + win->width > 640) new_x = 640 - win->width;
    if (new_y + win->height > 480) new_y = 480 - win->height;
    
    if (new_x != win->x || new_y != win->y) {
        win->x = new_x;
        win->y = new_y;
        Win32kRedrawAll();
    }
    
    ObDereferenceObject(drag_window);
}

void Win32kRedrawAll(void) {
    VgaClearScreen(COLOR_BLUE);
    
    for (int i = 0; i < window_count; i++) {
        WINDOW *win = (WINDOW*)ObReferenceObject(window_list[i]);
        if (win && win->visible) {
            Win32kShowWindow(window_list[i]);
            Win32kUpdateWindow(window_list[i]);
        }
        if (win) ObDereferenceObject(window_list[i]);
    }
}