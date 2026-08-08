// kernel/gdi32.h
#ifndef GDI32_H
#define GDI32_H
#include <stdint.h>
#include "object.h"

typedef uint32_t COLORREF;
#define RGB(r,g,b) ((COLORREF)(((uint8_t)(r)|((uint16_t)((uint8_t)(g))<<8))|(((uint32_t)(uint8_t)(b))<<16)))

typedef struct _PEN {
    COLORREF color;
    uint32_t width;
    uint32_t style;
} PEN;

typedef struct _BRUSH {
    COLORREF color;
    uint32_t style;
} BRUSH;

typedef struct _DC {
    HANDLE hwnd;
    COLORREF text_color;
    COLORREF bk_color;
    HANDLE pen;
    HANDLE brush;
} DC;

// GDI Objects
HANDLE GdiCreatePen(uint32_t style, uint32_t width, COLORREF color);
HANDLE GdiCreateSolidBrush(COLORREF color);
HANDLE GdiGetDC(HANDLE hwnd);
void GdiReleaseDC(HANDLE dc_handle);

// Drawing operations
void GdiSetPixel(HANDLE dc_handle, int x, int y, COLORREF color);
void GdiDrawText(HANDLE dc_handle, int x, int y, const char *text, int len);
void GdiFillRect(HANDLE dc_handle, int x1, int y1, int x2, int y2);
void GdiDrawRect(HANDLE dc_handle, int x1, int y1, int x2, int y2);
#endif