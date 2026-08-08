// kernel/user32.h
#ifndef USER32_H
#define USER32_H
#include <stdint.h>
#include "object.h"
#include "win32k.h"  // For WINDOW, RECT

HANDLE CreateWindow(const char *className, const char *title, int x, int y, int w, int h);
HANDLE CreateWindowEx(uint32_t exStyle, const char *className, const char *title, 
                       uint32_t style, int x, int y, int w, int h);
void ShowWindow(HANDLE hwnd);
void UpdateWindow(HANDLE hwnd);
void SetWindowText(HANDLE hwnd, const char *text);
void GetClientRect(HANDLE hwnd, RECT *rect);
void MessageBox(HANDLE hwnd, const char *text, const char *caption, uint32_t type);

#define MB_OK 0x00000000
#endif