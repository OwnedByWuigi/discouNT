#ifndef WIN32_GUIAPP_H
#define WIN32_GUIAPP_H

#include <stdint.h>

typedef uint32_t GUI_HANDLE;

typedef struct _GUI_RECT {
    int left, top, right, bottom;
} GUI_RECT;

#define GUI_WM_CREATE   0x0001
#define GUI_WM_PAINT    0x000F
#define GUI_WM_DESTROY  0x0002
#define GUI_WM_CLOSE    0x0010

#define GUI_MOUSE_MOVE      1
#define GUI_MOUSE_LDOWN     2
#define GUI_MOUSE_LUP       3
#define GUI_MOUSE_WHEEL     4

#define GUI_WS_VISIBLE           0x10000000L
#define GUI_WS_CAPTION           0x00C00000L
#define GUI_WS_SYSMENU           0x00080000L
#define GUI_WS_THICKFRAME        0x00040000L
#define GUI_WS_MINIMIZEBOX       0x00020000L
#define GUI_WS_MAXIMIZEBOX       0x00010000L
#define GUI_WS_OVERLAPPEDWINDOW  (GUI_WS_CAPTION | GUI_WS_SYSMENU | GUI_WS_THICKFRAME | GUI_WS_MINIMIZEBOX | GUI_WS_MAXIMIZEBOX)

typedef struct _GUI_APP_API {
    GUI_HANDLE (*RegisterClass)(const char *className, uint32_t style, void (*wndProc)(GUI_HANDLE, uint32_t, uint32_t, uint32_t));
    GUI_HANDLE (*CreateWindow)(const char *className, const char *title, int x, int y, int w, int h, uint32_t style);
    GUI_HANDLE (*CreateWindowByClass)(GUI_HANDLE hClass, const char *title, int x, int y, int w, int h, uint32_t style);
    void (*ShowWindow)(GUI_HANDLE hwnd);
    void (*UpdateWindow)(GUI_HANDLE hwnd);
    void (*GetClientRect)(GUI_HANDLE hwnd, GUI_RECT *rect);
    void (*GetWindowRect)(GUI_HANDLE hwnd, GUI_RECT *rect);
    void (*FillRect)(int x, int y, int w, int h, uint8_t color);
    void (*DrawRect)(int x, int y, int w, int h, uint8_t color);
    void (*DrawString)(int x, int y, const char *str, uint8_t fg, uint8_t bg);
    int (*ReadSector)(uint32_t lba, uint8_t *buffer);
    int (*ExecuteImage)(const char *path);
    uint32_t (*GetProcessId)(void);
    int (*GetScreenWidth)(void);
    int (*GetScreenHeight)(void);
    int (*GetScreenModeCount)(void);
    int (*GetScreenModeInfo)(int index, int *width, int *height, int *bpp);
    int (*SetScreenResolution)(int width, int height);
    int (*Ping)(const char *ip_text, char *out_text, int out_text_len);
    void (*Reboot)(void);
    void (*Shutdown)(void);
} GUI_APP_API;

#endif
