#include <stdint.h>
#include "windows.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void *memset(void *dest, int c, uint32_t n);
extern void *memcpy(void *dest, const void *src, uint32_t n);
extern void Win32kGetWindowRect(void *hwnd, LPRECT lpRect);
extern void Win32kGetClientRect(void *hwnd, LPRECT lpRect);

extern void FbPutPixel(int x, int y, uint8_t color);
extern void FbFillRect(int x, int y, int w, int h, uint8_t color);
extern void FbDrawRect(int x, int y, int w, int h, uint8_t color);
extern void FbDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg);
extern void FbDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg);
extern void FbSwapBuffers(void);
extern void Win32kRefreshCursor(void);
extern void SerialPutString(const char *str);

#define GDI_OBJ_BRUSH  1
#define GDI_OBJ_PEN    2
#define GDI_OBJ_BITMAP 3

typedef struct _GDIHDR {
    uint32_t kind;
} GDIHDR;

typedef struct _GDIBRUSH {
    GDIHDR hdr;
    COLORREF color;
} GDIBRUSH;

typedef struct _GDIPEN {
    GDIHDR hdr;
    int style;
    int width;
    COLORREF color;
} GDIPEN;

typedef struct _GDIBITMAP {
    GDIHDR hdr;
    int width;
    int height;
    COLORREF *pixels;
} GDIBITMAP;

typedef struct _GDISAVEDC {
    int used;
    RECT clip;
    COLORREF text_color;
    COLORREF bk_color;
    int bk_mode;
    int cur_x;
    int cur_y;
    HGDIOBJ selected_pen;
    HGDIOBJ selected_brush;
    HGDIOBJ selected_bitmap;
} GDISAVEDC;

typedef struct _GDIDC {
    int is_screen;
    HWND hwnd;
    int has_custom_origin;
    int origin_x;
    int origin_y;
    RECT clip;
    COLORREF text_color;
    COLORREF bk_color;
    int bk_mode;
    int cur_x;
    int cur_y;
    HGDIOBJ selected_pen;
    HGDIOBJ selected_brush;
    HGDIOBJ selected_bitmap;
    GDISAVEDC saved[8];
} GDIDC;

static GDIBRUSH g_stock_white_brush = {{GDI_OBJ_BRUSH}, RGB(255,255,255)};
static GDIBRUSH g_stock_ltgray_brush = {{GDI_OBJ_BRUSH}, RGB(192,192,192)};
static GDIBRUSH g_stock_black_brush = {{GDI_OBJ_BRUSH}, RGB(0,0,0)};
static GDIPEN g_stock_black_pen = {{GDI_OBJ_PEN}, PS_SOLID, 1, RGB(0,0,0)};

static GDIDC *gdi_alloc_dc(void);

HDC GdiCreateScreenDC(HWND hwnd) {
    GDIDC *dc = gdi_alloc_dc();
    if (!dc) return 0;
    dc->is_screen = 1;
    dc->hwnd = hwnd;
    Win32kGetClientRect((void*)hwnd, &dc->clip);
    return (HDC)dc;
}

HDC GdiCreateScreenDCEx(HWND hwnd, int origin_x, int origin_y, int width, int height) {
    GDIDC *dc = gdi_alloc_dc();
    if (!dc) return 0;
    dc->is_screen = 1;
    dc->hwnd = hwnd;
    dc->has_custom_origin = 1;
    dc->origin_x = origin_x;
    dc->origin_y = origin_y;
    dc->clip.left = 0;
    dc->clip.top = 0;
    dc->clip.right = width;
    dc->clip.bottom = height;
    return (HDC)dc;
}

void GdiDestroyScreenDC(HDC hdc) {
    DeleteDC(hdc);
}

static uint8_t gdi_color_to_index(COLORREF color) {
    int r = color & 0xFF;
    int g = (color >> 8) & 0xFF;
    int b = (color >> 16) & 0xFF;
    if (r > 220 && g > 220 && b > 220) return 15;
    if (r < 40 && g < 40 && b < 40) return 0;
    if (r > 180 && g < 100 && b < 100) return 4;
    if (r < 100 && g > 180 && b < 100) return 2;
    if (r > 180 && g > 180 && b < 100) return 14;
    if (r < 100 && g < 100 && b > 180) return 1;
    if (r > 150 && g > 150 && b > 150) return 7;
    if (g > r && g > b) return 10;
    if (r > g && r > b) return 12;
    if (b > r && b > g) return 9;
    return 8;
}

static GDIDC *gdi_alloc_dc(void) {
    GDIDC *dc = (GDIDC*)kmalloc(sizeof(GDIDC));
    if (!dc) return 0;
    memset(dc, 0, sizeof(*dc));
    dc->text_color = RGB(0,0,0);
    dc->bk_color = RGB(192,192,192);
    dc->bk_mode = OPAQUE;
    dc->selected_pen = (HGDIOBJ)&g_stock_black_pen;
    dc->selected_brush = (HGDIOBJ)&g_stock_ltgray_brush;
    return dc;
}

static void gdi_get_screen_origin(HWND hwnd, int *ox, int *oy) {
    RECT rc;
    if (ox) *ox = 0;
    if (oy) *oy = 0;
    if (!hwnd) return;
    Win32kGetWindowRect((void*)hwnd, &rc);
    if (ox) *ox = rc.left;
    if (oy) *oy = rc.top;
}

static GDIBITMAP *gdi_get_bitmap(HGDIOBJ obj) {
    GDIHDR *hdr = (GDIHDR*)obj;
    if (!hdr || hdr->kind != GDI_OBJ_BITMAP) return 0;
    return (GDIBITMAP*)obj;
}

static GDIPEN *gdi_get_pen(HGDIOBJ obj) {
    GDIHDR *hdr = (GDIHDR*)obj;
    if (!hdr || hdr->kind != GDI_OBJ_PEN) return 0;
    return (GDIPEN*)obj;
}

static GDIBRUSH *gdi_get_brush(HGDIOBJ obj) {
    GDIHDR *hdr = (GDIHDR*)obj;
    if (!hdr || hdr->kind != GDI_OBJ_BRUSH) return 0;
    return (GDIBRUSH*)obj;
}

static void gdi_put_pixel(GDIDC *dc, int x, int y, COLORREF color) {
    GDIBITMAP *bmp;
    if (!dc) return;
    if (x < dc->clip.left || y < dc->clip.top || x >= dc->clip.right || y >= dc->clip.bottom) return;
    if (dc->is_screen) {
        int ox = 0, oy = 0;
        if (dc->has_custom_origin) {
            ox = dc->origin_x;
            oy = dc->origin_y;
        } else {
            gdi_get_screen_origin(dc->hwnd, &ox, &oy);
        }
        FbPutPixel(ox + x, oy + y, gdi_color_to_index(color));
        return;
    }
    bmp = gdi_get_bitmap(dc->selected_bitmap);
    if (!bmp) return;
    if (x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return;
    bmp->pixels[(y * bmp->width) + x] = color;
}

static COLORREF gdi_get_pixel(GDIDC *dc, int x, int y) {
    GDIBITMAP *bmp;
    if (!dc || dc->is_screen) return 0;
    bmp = gdi_get_bitmap(dc->selected_bitmap);
    if (!bmp) return 0;
    if (x < 0 || y < 0 || x >= bmp->width || y >= bmp->height) return 0;
    return bmp->pixels[(y * bmp->width) + x];
}

static void gdi_fill_rect_dc(GDIDC *dc, int left, int top, int right, int bottom, COLORREF color) {
    int x, y;
    if (!dc) return;
    if (left < dc->clip.left) left = dc->clip.left;
    if (top < dc->clip.top) top = dc->clip.top;
    if (right > dc->clip.right) right = dc->clip.right;
    if (bottom > dc->clip.bottom) bottom = dc->clip.bottom;
    if (right <= left || bottom <= top) return;
    if (dc->is_screen) {
        int ox = 0, oy = 0;
        if (dc->has_custom_origin) {
            ox = dc->origin_x;
            oy = dc->origin_y;
        } else {
            gdi_get_screen_origin(dc->hwnd, &ox, &oy);
        }
        FbFillRect(ox + left, oy + top, right - left, bottom - top, gdi_color_to_index(color));
        return;
    }
    for (y = top; y < bottom; y++) {
        for (x = left; x < right; x++) {
            gdi_put_pixel(dc, x, y, color);
        }
    }
}

static void gdi_line_to(GDIDC *dc, int x0, int y0, int x1, int y1, COLORREF color) {
    int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    int guard;
    if (!dc) return;
    if (dx > 8192 || -dy > 8192) {
        if (x0 < -4096) x0 = -4096;
        if (x0 > 4096) x0 = 4096;
        if (x1 < -4096) x1 = -4096;
        if (x1 > 4096) x1 = 4096;
        if (y0 < -4096) y0 = -4096;
        if (y0 > 4096) y0 = 4096;
        if (y1 < -4096) y1 = -4096;
        if (y1 > 4096) y1 = 4096;
        dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
        sx = (x0 < x1) ? 1 : -1;
        dy = -((y1 > y0) ? (y1 - y0) : (y0 - y1));
        sy = (y0 < y1) ? 1 : -1;
        err = dx + dy;
    }
    guard = dx + (-dy) + 4;
    if (guard < 4) guard = 4;
    if (guard > 16384) guard = 16384;
    while (1) {
        gdi_put_pixel(dc, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        if (--guard <= 0) break;
        if ((err * 2) >= dy) {
            err += dy;
            x0 += sx;
        }
        if ((err * 2) <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void gdi_draw_text_screen(GDIDC *dc, int x, int y, LPCSTR str, int len) {
    char buf[256];
    int ox = 0, oy = 0, i;
    if (!dc || !str) return;
    if (len < 0) {
        len = 0;
        while (str[len]) len++;
    }
    if (len > 255) len = 255;
    for (i = 0; i < len; i++) buf[i] = str[i];
    buf[len] = 0;
    if (dc->has_custom_origin) {
        ox = dc->origin_x;
        oy = dc->origin_y;
    } else {
        gdi_get_screen_origin(dc->hwnd, &ox, &oy);
    }
    FbDrawString(ox + x, oy + y, buf, gdi_color_to_index(dc->text_color), gdi_color_to_index(dc->bk_color));
}

static void gdi_draw_text_mem(GDIDC *dc, int x, int y, LPCSTR str, int len) {
    int i, px, py, row, col;
    if (!dc || !str) return;
    if (len < 0) {
        len = 0;
        while (str[len]) len++;
    }
    px = x;
    py = y;
    for (i = 0; i < len; i++) {
        char ch = str[i];
        for (row = 0; row < 8; row++) {
            for (col = 0; col < 6; col++) {
                int border = (row == 0 || row == 7 || col == 0 || col == 5);
                if (ch != ' ' && border) gdi_put_pixel(dc, px + col, py + row, dc->text_color);
                else if (dc->bk_mode == OPAQUE) gdi_put_pixel(dc, px + col, py + row, dc->bk_color);
            }
        }
        px += 8;
    }
}

int DllMain(void *h, uint32_t r, void *l) {
    (void)h; (void)r; (void)l;
    return 1;
}

BOOL TextOutW(HDC hdc, int x, int y, LPCWSTR lpString, int c) {
    char buf[256];
    int i, len = c;
    GDIDC *dc = (GDIDC*)hdc;
    if (!lpString) return FALSE;
    if (len < 0) {
        len = 0;
        while (lpString[len]) len++;
    }
    if (len > 255) len = 255;
    for (i = 0; i < len; i++) buf[i] = (char)((lpString[i] >= 32 && lpString[i] < 127) ? lpString[i] : '?');
    buf[len] = 0;
    if (dc && dc->is_screen) gdi_draw_text_screen(dc, x, y, buf, len);
    else gdi_draw_text_mem(dc, x, y, buf, len);
    return TRUE;
}

BOOL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c) {
    GDIDC *dc = (GDIDC*)hdc;
    if (dc && dc->is_screen) gdi_draw_text_screen(dc, x, y, lpString, c);
    else gdi_draw_text_mem(dc, x, y, lpString, c);
    return TRUE;
}

BOOL ExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT *lprect, LPCWSTR lpString, UINT c, const INT *lpDx) {
    GDIDC *dc = (GDIDC*)hdc;
    (void)lpDx;
    if ((options & ETO_OPAQUE) && lprect) {
        gdi_fill_rect_dc(dc, lprect->left, lprect->top, lprect->right, lprect->bottom, dc ? dc->bk_color : RGB(0,0,0));
    }
    if (lpString) return TextOutW(hdc, x, y, lpString, (int)c);
    return TRUE;
}

COLORREF SetTextColor(HDC hdc, COLORREF color) {
    GDIDC *dc = (GDIDC*)hdc;
    COLORREF old = 0;
    if (!dc) return 0;
    old = dc->text_color;
    dc->text_color = color;
    return old;
}

COLORREF SetBkColor(HDC hdc, COLORREF color) {
    GDIDC *dc = (GDIDC*)hdc;
    COLORREF old = 0;
    if (!dc) return 0;
    old = dc->bk_color;
    dc->bk_color = color;
    return old;
}

int SetBkMode(HDC hdc, int mode) {
    GDIDC *dc = (GDIDC*)hdc;
    int old = OPAQUE;
    if (!dc) return 0;
    old = dc->bk_mode;
    dc->bk_mode = mode;
    return old;
}

HGDIOBJ GetStockObject(int i) {
    switch (i) {
    case 0:
        return (HGDIOBJ)&g_stock_ltgray_brush;
    default:
        return (HGDIOBJ)&g_stock_white_brush;
    }
}

HPEN CreatePen(int fnPenStyle, int nWidth, COLORREF crColor) {
    GDIPEN *pen = (GDIPEN*)kmalloc(sizeof(GDIPEN));
    if (!pen) return 0;
    pen->hdr.kind = GDI_OBJ_PEN;
    pen->style = fnPenStyle;
    pen->width = nWidth;
    pen->color = crColor;
    return (HPEN)pen;
}

HBRUSH CreateSolidBrush(COLORREF color) {
    GDIBRUSH *brush = (GDIBRUSH*)kmalloc(sizeof(GDIBRUSH));
    if (!brush) return 0;
    brush->hdr.kind = GDI_OBJ_BRUSH;
    brush->color = color;
    return (HBRUSH)brush;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ hgdiobj) {
    GDIDC *dc = (GDIDC*)hdc;
    GDIHDR *hdr = (GDIHDR*)hgdiobj;
    HGDIOBJ old = 0;
    if (!dc || !hdr) return 0;
    switch (hdr->kind) {
    case GDI_OBJ_PEN:
        old = dc->selected_pen;
        dc->selected_pen = hgdiobj;
        return old;
    case GDI_OBJ_BRUSH:
        old = dc->selected_brush;
        dc->selected_brush = hgdiobj;
        return old;
    case GDI_OBJ_BITMAP:
        old = dc->selected_bitmap;
        dc->selected_bitmap = hgdiobj;
        return old;
    default:
        return 0;
    }
}

BOOL DeleteObject(HGDIOBJ ho) {
    GDIHDR *hdr = (GDIHDR*)ho;
    if (!hdr) return FALSE;
    if (ho == (HGDIOBJ)&g_stock_white_brush || ho == (HGDIOBJ)&g_stock_ltgray_brush ||
        ho == (HGDIOBJ)&g_stock_black_brush || ho == (HGDIOBJ)&g_stock_black_pen) {
        return TRUE;
    }
    if (hdr->kind == GDI_OBJ_BITMAP) {
        GDIBITMAP *bmp = (GDIBITMAP*)ho;
        if (bmp->pixels) kfree(bmp->pixels);
    }
    kfree(ho);
    return TRUE;
}

BOOL DeleteDC(HDC hdc) {
    GDIDC *dc = (GDIDC*)hdc;
    if (!dc) return FALSE;
    kfree(dc);
    return TRUE;
}

HDC CreateCompatibleDC(HDC hdc) {
    GDIDC *base = (GDIDC*)hdc;
    GDIDC *dc = gdi_alloc_dc();
    if (!dc) return 0;
    if (base) {
        dc->text_color = base->text_color;
        dc->bk_color = base->bk_color;
        dc->bk_mode = base->bk_mode;
    }
    dc->is_screen = 0;
    return (HDC)dc;
}

HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy) {
    GDIBITMAP *bmp;
    (void)hdc;
    if (cx <= 0 || cy <= 0) return 0;
    bmp = (GDIBITMAP*)kmalloc(sizeof(GDIBITMAP));
    if (!bmp) return 0;
    memset(bmp, 0, sizeof(*bmp));
    bmp->hdr.kind = GDI_OBJ_BITMAP;
    bmp->width = cx;
    bmp->height = cy;
    bmp->pixels = (COLORREF*)kmalloc((uint32_t)(cx * cy * sizeof(COLORREF)));
    if (!bmp->pixels) {
        kfree(bmp);
        return 0;
    }
    memset(bmp->pixels, 0, (uint32_t)(cx * cy * sizeof(COLORREF)));
    return (HBITMAP)bmp;
}

HBITMAP LoadBitmapW(HINSTANCE hInstance, LPCWSTR lpBitmapName) {
    (void)hInstance;
    (void)lpBitmapName;
    return CreateCompatibleBitmap(0, 16, 16);
}

BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT lppt) {
    GDIDC *dc = (GDIDC*)hdc;
    if (!dc) return FALSE;
    if (lppt) {
        lppt->x = dc->cur_x;
        lppt->y = dc->cur_y;
    }
    dc->cur_x = x;
    dc->cur_y = y;
    return TRUE;
}

BOOL LineTo(HDC hdc, int x, int y) {
    GDIDC *dc = (GDIDC*)hdc;
    GDIPEN *pen;
    if (!dc) return FALSE;
    pen = gdi_get_pen(dc->selected_pen);
    gdi_line_to(dc, dc->cur_x, dc->cur_y, x, y, pen ? pen->color : RGB(255,255,255));
    dc->cur_x = x;
    dc->cur_y = y;
    return TRUE;
}

COLORREF SetPixel(HDC hdc, int x, int y, COLORREF color) {
    GDIDC *dc = (GDIDC*)hdc;
    if (!dc) return 0;
    gdi_put_pixel(dc, x, y, color);
    return color;
}

int FillRect(HDC hdc, const RECT *lprc, HBRUSH hbr) {
    GDIDC *dc = (GDIDC*)hdc;
    GDIBRUSH *brush = gdi_get_brush((HGDIOBJ)hbr);
    if (!dc || !lprc) return 0;
    gdi_fill_rect_dc(dc, lprc->left, lprc->top, lprc->right, lprc->bottom, brush ? brush->color : RGB(255,255,255));
    return 1;
}

BOOL BitBlt(HDC hdcDest, int xDest, int yDest, int w, int h, HDC hdcSrc, int xSrc, int ySrc, DWORD rop) {
    GDIDC *dst = (GDIDC*)hdcDest;
    GDIDC *src = (GDIDC*)hdcSrc;
    int x, y;
    static int logged_self_blt = 0;
    static int logged_screen_blt = 0;
    if (!dst || !src) return FALSE;
    if (dst == src && w >= 100 && h >= 20 && logged_self_blt < 8) {
        logged_self_blt++;
        SerialPutString("[GDI] BitBlt self begin\r\n");
    }
    if (dst->is_screen && w >= 100 && h >= 20 && logged_screen_blt < 8) {
        logged_screen_blt++;
        SerialPutString("[GDI] BitBlt screen begin\r\n");
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            COLORREF src_pixel = gdi_get_pixel(src, xSrc + x, ySrc + y);
            COLORREF dst_pixel = dst->is_screen ? 0 : gdi_get_pixel(dst, xDest + x, yDest + y);
            COLORREF out = src_pixel;
            if (rop == SRCPAINT) out = src_pixel | dst_pixel;
            gdi_put_pixel(dst, xDest + x, yDest + y, out);
        }
    }
    if (dst == src && w >= 100 && h >= 20 && logged_self_blt <= 8) {
        SerialPutString("[GDI] BitBlt self end\r\n");
    }
    if (dst->is_screen && w >= 100 && h >= 20 && logged_screen_blt <= 8) {
        SerialPutString("[GDI] BitBlt screen end\r\n");
    }
    if (dst->is_screen) Win32kRefreshCursor();
    return TRUE;
}

int SaveDC(HDC hdc) {
    GDIDC *dc = (GDIDC*)hdc;
    int i;
    if (!dc) return 0;
    for (i = 0; i < 8; i++) {
        if (!dc->saved[i].used) {
            dc->saved[i].used = 1;
            dc->saved[i].clip = dc->clip;
            dc->saved[i].text_color = dc->text_color;
            dc->saved[i].bk_color = dc->bk_color;
            dc->saved[i].bk_mode = dc->bk_mode;
            dc->saved[i].cur_x = dc->cur_x;
            dc->saved[i].cur_y = dc->cur_y;
            dc->saved[i].selected_pen = dc->selected_pen;
            dc->saved[i].selected_brush = dc->selected_brush;
            dc->saved[i].selected_bitmap = dc->selected_bitmap;
            return i + 1;
        }
    }
    return 0;
}

BOOL RestoreDC(HDC hdc, int nSavedDC) {
    GDIDC *dc = (GDIDC*)hdc;
    int i = nSavedDC - 1;
    if (!dc || i < 0 || i >= 8 || !dc->saved[i].used) return FALSE;
    dc->clip = dc->saved[i].clip;
    dc->text_color = dc->saved[i].text_color;
    dc->bk_color = dc->saved[i].bk_color;
    dc->bk_mode = dc->saved[i].bk_mode;
    dc->cur_x = dc->saved[i].cur_x;
    dc->cur_y = dc->saved[i].cur_y;
    dc->selected_pen = dc->saved[i].selected_pen;
    dc->selected_brush = dc->saved[i].selected_brush;
    dc->selected_bitmap = dc->saved[i].selected_bitmap;
    dc->saved[i].used = 0;
    return TRUE;
}

int ExcludeClipRect(HDC hdc, int left, int top, int right, int bottom) {
    GDIDC *dc = (GDIDC*)hdc;
    if (!dc) return 0;
    dc->clip.left = left;
    dc->clip.top = top;
    dc->clip.right = right;
    dc->clip.bottom = bottom;
    return 1;
}

HICON CreateIconIndirect(PICONINFO piconinfo) {
    if (!piconinfo) return 0;
    return (HICON)piconinfo->hbmColor;
}

BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom) {
    GDIDC *dc = (GDIDC*)hdc;
    GDIPEN *pen;
    if (!dc) return FALSE;
    pen = gdi_get_pen(dc->selected_pen);
    if (dc->is_screen) {
        int ox = 0, oy = 0;
        if (dc->has_custom_origin) {
            ox = dc->origin_x;
            oy = dc->origin_y;
        } else {
            gdi_get_screen_origin(dc->hwnd, &ox, &oy);
        }
        FbDrawRect(ox + left, oy + top, right - left, bottom - top, gdi_color_to_index(pen ? pen->color : RGB(255,255,255)));
        return TRUE;
    }
    gdi_line_to(dc, left, top, right - 1, top, pen ? pen->color : RGB(255,255,255));
    gdi_line_to(dc, right - 1, top, right - 1, bottom - 1, pen ? pen->color : RGB(255,255,255));
    gdi_line_to(dc, right - 1, bottom - 1, left, bottom - 1, pen ? pen->color : RGB(255,255,255));
    gdi_line_to(dc, left, bottom - 1, left, top, pen ? pen->color : RGB(255,255,255));
    return TRUE;
}

int DrawTextW(HDC hdc, LPCWSTR lpchText, int cchText, LPRECT lprc, UINT format) {
    int x, y, len = cchText;
    if (!lprc) return 0;
    if (len < 0) {
        len = 0;
        while (lpchText && lpchText[len]) len++;
    }
    x = lprc->left;
    y = lprc->top;
    if ((format & DT_CENTER) && len > 0) {
        int width = len * 8;
        int rectw = lprc->right - lprc->left;
        if (rectw > width) x = lprc->left + ((rectw - width) / 2);
    }
    return TextOutW(hdc, x, y, lpchText, len);
}
