// kernel/gdi32.c
#include <stdint.h>
#include "gdi32.h"
#include "win32k.h"  // Need this for WINDOW struct
#include "mm.h"
#include "hal.h"
#include "util.h"

// Map COLORREF to VGA attributes (simplified)
static uint8_t ColorToVGA(COLORREF color) {
    uint8_t r = color & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = (color >> 16) & 0xFF;
    
    // Simple mapping: bright if any channel > 128
    uint8_t bright = (r > 128 || g > 128 || b > 128) ? 0x08 : 0x00;
    
    if (r > 128 && g > 128 && b > 128) return 0x07 | bright; // White
    if (r > 128 && g > 128) return 0x06 | bright; // Yellow
    if (r > 128 && b > 128) return 0x05 | bright; // Magenta
    if (r > 128) return 0x04 | bright; // Red
    if (g > 128 && b > 128) return 0x03 | bright; // Cyan
    if (g > 128) return 0x02 | bright; // Green
    if (b > 128) return 0x01 | bright; // Blue
    return 0x00; // Black
}

HANDLE GdiCreatePen(uint32_t style, uint32_t width, COLORREF color) {
    PEN *pen = (PEN*)kmalloc(sizeof(PEN));
    pen->style = style;
    pen->width = width;
    pen->color = color;
    return ObCreateObject(OBJ_TYPE_PEN, "Pen", pen, sizeof(PEN));
}

HANDLE GdiCreateSolidBrush(COLORREF color) {
    BRUSH *brush = (BRUSH*)kmalloc(sizeof(BRUSH));
    brush->color = color;
    brush->style = 0;
    return ObCreateObject(OBJ_TYPE_BRUSH, "Brush", brush, sizeof(BRUSH));
}

HANDLE GdiGetDC(HANDLE hwnd) {
    DC *dc = (DC*)kmalloc(sizeof(DC));
    memset(dc, 0, sizeof(DC));
    dc->hwnd = hwnd;
    dc->text_color = RGB(255, 255, 255);
    dc->bk_color = RGB(0, 0, 128);
    return ObCreateObject(OBJ_TYPE_DC, "DC", dc, sizeof(DC));
}

void GdiReleaseDC(HANDLE dc_handle) {
    ObDereferenceObject(dc_handle);
}

void GdiSetPixel(HANDLE dc_handle, int x, int y, COLORREF color) {
    DC *dc = (DC*)ObReferenceObject(dc_handle);
    if (!dc) return;
    
    WINDOW *window = (WINDOW*)ObReferenceObject(dc->hwnd);
    if (!window) {
        ObDereferenceObject(dc_handle);
        return;
    }
    
    int abs_x = window->x + x;
    int abs_y = window->y + y;
    
    if (abs_x >= 0 && abs_x < 80 && abs_y >= 0 && abs_y < 25) {
        HalSetCursor(abs_x, abs_y);
        HalPutChar(' ', ColorToVGA(color) << 4);
    }
    
    ObDereferenceObject(dc->hwnd);
    ObDereferenceObject(dc_handle);
}

void GdiDrawText(HANDLE dc_handle, int x, int y, const char *text, int len) {
    DC *dc = (DC*)ObReferenceObject(dc_handle);
    if (!dc) return;
    
    WINDOW *window = (WINDOW*)ObReferenceObject(dc->hwnd);
    if (!window) {
        ObDereferenceObject(dc_handle);
        return;
    }
    
    if (len == -1) len = strlen(text);
    
    int abs_x = window->x + x;
    int abs_y = window->y + y;
    
    uint8_t attr = (ColorToVGA(dc->bk_color) << 4) | ColorToVGA(dc->text_color);
    
    for (int i = 0; i < len && text[i]; i++) {
        if (abs_x + i < 80 && abs_y < 25 && abs_x + i >= 0 && abs_y >= 0) {
            HalSetCursor(abs_x + i, abs_y);
            HalPutChar(text[i], attr);
        }
    }
    
    ObDereferenceObject(dc->hwnd);
    ObDereferenceObject(dc_handle);
}

void GdiFillRect(HANDLE dc_handle, int x1, int y1, int x2, int y2) {
    DC *dc = (DC*)ObReferenceObject(dc_handle);
    if (!dc) return;
    
    WINDOW *window = (WINDOW*)ObReferenceObject(dc->hwnd);
    if (!window) {
        ObDereferenceObject(dc_handle);
        return;
    }
    
    uint8_t attr = (ColorToVGA(dc->bk_color) << 4) | ColorToVGA(dc->bk_color);
    
    for (int y = y1; y < y2; y++) {
        for (int x = x1; x < x2; x++) {
            int abs_x = window->x + x;
            int abs_y = window->y + y;
            if (abs_x >= 0 && abs_x < 80 && abs_y >= 0 && abs_y < 25) {
                HalSetCursor(abs_x, abs_y);
                HalPutChar(' ', attr);
            }
        }
    }
    
    ObDereferenceObject(dc->hwnd);
    ObDereferenceObject(dc_handle);
}

void GdiDrawRect(HANDLE dc_handle, int x1, int y1, int x2, int y2) {
    GdiFillRect(dc_handle, x1, y1, x2, y1 + 1); // Top
    GdiFillRect(dc_handle, x1, y2 - 1, x2, y2); // Bottom
    GdiFillRect(dc_handle, x1, y1, x1 + 1, y2); // Left
    GdiFillRect(dc_handle, x2 - 1, y1, x2, y2); // Right
}