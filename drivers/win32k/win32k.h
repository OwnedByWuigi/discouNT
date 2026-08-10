#ifndef WIN32K_H
#define WIN32K_H
#include <stdint.h>
#include "object.h"

#define WM_CREATE   1
#define WM_PAINT    2
#define WM_DESTROY  3
#define WM_CLOSE    4

#define WS_OVERLAPPED   0x00000000
#define WS_VISIBLE      0x00000001
#define WS_CAPTION      0x00000002
#define WS_SYSMENU      0x00000004
#define WS_THICKFRAME   0x00000008

#define WS_OVERLAPPEDWINDOW (WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME)

typedef struct _WNDCLASS {
    char     className[64];
    uint32_t style;
    void     (*wndProc)(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam);
} WNDCLASS;

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

typedef struct _RECT {
    int left, top, right, bottom;
} RECT;

void Win32kInit(void *mb_info);
HANDLE Win32kRegisterClass(const char *className, uint32_t style, void (*wndProc)(HANDLE, uint32_t, uint32_t, uint32_t));
HANDLE Win32kCreateWindow(const char *className, const char *title, int x, int y, int w, int h, uint32_t style);
void Win32kShowWindow(HANDLE hwnd);
void Win32kUpdateWindow(HANDLE hwnd);
void Win32kGetClientRect(HANDLE hwnd, RECT *rect);
void Win32kDestroyWindow(HANDLE hwnd);
void Win32kHandleMouseDown(int x, int y, int button);
void Win32kHandleMouseUp(int x, int y, int button);
void Win32kHandleMouseMove(int x, int y);
void Win32kRedrawAll(void);
void Win32kRefreshCursor(void);
int Win32kIsDragging(void);

#endif
