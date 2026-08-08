// kernel/win32k.h
#ifndef WIN32K_H
#define WIN32K_H
#include <stdint.h>
#include "object.h"

// Window messages
#define WM_CREATE       0x0001
#define WM_DESTROY      0x0002
#define WM_PAINT        0x000F
#define WM_CLOSE        0x0010
#define WM_KEYDOWN      0x0100
#define WM_KEYUP        0x0101
#define WM_LBUTTONDOWN  0x0201
#define WM_LBUTTONUP    0x0202

// Window styles
#define WS_OVERLAPPED   0x00000000
#define WS_VISIBLE      0x10000000
#define WS_CAPTION      0x00C00000
#define WS_SYSMENU      0x00080000
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU)

// Forward declaration
typedef struct _WNDCLASS WNDCLASS;

typedef struct _WINDOW {
    char      title[64];
    int       x, y;
    int       width, height;
    uint32_t  style;
    uint8_t   visible;
    uint8_t   active;
    WNDCLASS  *wndClass;
    void      (*wndProc)(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
} WINDOW;

typedef struct _WNDCLASS {
    char     className[64];
    uint32_t style;
    void     (*wndProc)(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
} WNDCLASS;

typedef struct _RECT {
    int left, top, right, bottom;
} RECT;

// Public API
void Win32kInit(void);
HANDLE Win32kRegisterClass(const char *className, uint32_t style, void (*wndProc)(HANDLE, uint32_t, uint32_t, uint32_t));
HANDLE Win32kCreateWindow(const char *className, const char *title, int x, int y, int w, int h, uint32_t style);
void Win32kShowWindow(HANDLE hwnd);
void Win32kUpdateWindow(HANDLE hwnd);
void Win32kSetWindowText(HANDLE hwnd, const char *text);
void Win32kGetClientRect(HANDLE hwnd, RECT *rect);
void Win32kDefWindowProc(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
#endif