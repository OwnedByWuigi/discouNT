#ifndef WIN32K_H
#define WIN32K_H
#include <stdint.h>
#include "ob/object.h"

#define WM_CREATE   0x0001
#define WM_PAINT    0x000F
#define WM_DESTROY  0x0002
#define WM_CLOSE    0x0010

#define WS_OVERLAPPED       0x00000000L
#define WS_VISIBLE          0x10000000L
#define WS_CAPTION          0x00C00000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L

#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

typedef struct _WNDCLASS {
    char     className[64];
    uint32_t style;
    void     (*wndProc)(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
} WNDCLASS;

typedef struct _WINDOW {
    char      title[64];
    int       x, y;
    int       width, height;
    int       restore_x, restore_y;
    int       restore_width, restore_height;
    uint32_t  style;
    uint8_t   visible;
    uint8_t   active;
    uint8_t   minimized;
    uint8_t   maximized;
    uint8_t   desktop;
    HANDLE    big_icon;
    HANDLE    small_icon;
    WNDCLASS  *wndClass;
    void      (*wndProc)(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
} WINDOW;

typedef struct _RECT {
    int left, top, right, bottom;
} RECT;

void Win32kInit(void *mb_info);
HANDLE Win32kRegisterClass(const char *className, uint32_t style, void (*wndProc)(HANDLE, uint32_t, uint32_t, uint32_t));
HANDLE Win32kCreateWindow(const char *className, const char *title, int x, int y, int w, int h, uint32_t style);
HANDLE Win32kCreateWindowByClass(HANDLE hClass, const char *title, int x, int y, int w, int h, uint32_t style);
void Win32kShowWindow(HANDLE hwnd);
void Win32kSetWindowShowState(HANDLE hwnd, int command);
int Win32kIsWindowMinimized(HANDLE hwnd);
void Win32kUpdateWindow(HANDLE hwnd);
void Win32kGetClientRect(HANDLE hwnd, RECT *rect);
void Win32kGetClientScreenRect(HANDLE hwnd, RECT *rect);
void Win32kGetWindowRect(HANDLE hwnd, RECT *rect);
void Win32kDestroyWindow(HANDLE hwnd);
void Win32kHandleMouseDown(int x, int y, int button);
void Win32kHandleMouseUp(int x, int y, int button);
void Win32kHandleMouseMove(int x, int y);
void Win32kRedrawAll(void);
void Win32kSetColorPreview(int enabled);
void Win32kRefreshCursor(void);
int Win32kIsDragging(void);
int Win32kIsResizing(void);
HANDLE Win32kGetActiveWindow(void);
void Win32kActivateWindow(HANDLE hwnd);
void Win32kSetWindowIcons(HANDLE hwnd, HANDLE big_icon, HANDLE small_icon);
void Win32kSetWindowRect(HANDLE hwnd, int x, int y, int width, int height);
int Win32kGetScreenWidth(void);
int Win32kGetScreenHeight(void);

#endif
