#include <stdint.h>
#include <string.h>
#include "windows.h"
#include "icon.h"
#include "../../compat/jpeg/jpeg_image.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void *memset(void *dest, int c, uint32_t n);
extern void *memcpy(void *dest, const void *src, uint32_t n);
extern void Win32kGetWindowRect(void *hwnd, LPRECT lpRect);
extern void Win32kGetClientRect(void *hwnd, LPRECT lpRect);

HBITMAP WINAPI CreateDIBSection(HDC dc,const BITMAPINFO *info,UINT usage,void **bits,HANDLE section,DWORD offset){SIZE_T bytes;(void)dc;(void)usage;(void)section;(void)offset;if(!info||!bits)return 0;bytes=(SIZE_T)(info->bmiHeader.biWidth<0?-info->bmiHeader.biWidth:info->bmiHeader.biWidth)*(SIZE_T)(info->bmiHeader.biHeight<0?-info->bmiHeader.biHeight:info->bmiHeader.biHeight)*4;*bits=kmalloc((uint32_t)bytes);if(!*bits)return 0;memset(*bits,0,(uint32_t)bytes);return (HBITMAP)*bits;}
HBITMAP WINAPI CreateBitmap(int width,int height,UINT planes,UINT bits,const void *data){BITMAPINFO info;void *pixels=0;(void)planes;(void)bits;(void)data;memset(&info,0,sizeof(info));info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=height;return CreateDIBSection(0,&info,0,&pixels,0,0);}
HBITMAP WINAPI CreateDIBitmap(HDC dc,const BITMAPINFOHEADER *header,DWORD init,const void *bits,const BITMAPINFO *info,UINT usage){BITMAPINFO local;void *pixels=0;(void)init;(void)bits;if(!header)return 0;if(info)local=*info;else{memset(&local,0,sizeof(local));local.bmiHeader=*header;}return CreateDIBSection(dc,&local,usage,&pixels,0,0);}
HDC WINAPI CreateEnhMetaFileW(HDC dc,LPCWSTR file,const RECT *rect,LPCWSTR desc){(void)file;(void)rect;(void)desc;return dc;}
HENHMETAFILE WINAPI CloseEnhMetaFile(HDC dc){return (HENHMETAFILE)dc;}
UINT WINAPI GetEnhMetaFileBits(HENHMETAFILE emf,UINT size,BYTE *bits){(void)emf;(void)size;(void)bits;return 0;}
BOOL WINAPI DeleteEnhMetaFile(HENHMETAFILE emf){(void)emf;return TRUE;}
BOOL WINAPI GetTextExtentPointA(HDC dc,LPCSTR text,int count,LPSIZE size){(void)dc;(void)text;if(!size)return FALSE;size->cx=count*8;size->cy=16;return TRUE;}
BOOL WINAPI GetTextExtentPointW(HDC dc,LPCWSTR text,int count,LPSIZE size){return GetTextExtentPoint32W(dc,text,count,size);}
extern void Win32kGetClientScreenRect(void *hwnd, LPRECT lpRect);

extern void FbPutPixel(int x, int y, uint8_t color);
extern void FbPutPixelRGB(int x, int y, uint32_t rgb);
extern void FbFillRect(int x, int y, int w, int h, uint8_t color);
extern void FbDrawRect(int x, int y, int w, int h, uint8_t color);
extern void FbDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg);
extern void FbDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg);
extern void FbSwapBuffers(void);
extern void Win32kRefreshCursor(void);
extern void SerialPutString(const char *str);
extern int CdfsReadFile(const char *path, uint8_t **out_buffer, uint32_t *out_size);

#define GDI_OBJ_BRUSH  1
#define GDI_OBJ_PEN    2
#define GDI_OBJ_BITMAP 3
#define GDI_OBJ_FONT   4

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

typedef struct _GDIFONT {
    GDIHDR hdr;
    LOGFONTW lf;
} GDIFONT;

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
    HGDIOBJ selected_font;
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
    HGDIOBJ selected_font;
    GDISAVEDC saved[8];
} GDIDC;

static GDIBRUSH g_stock_white_brush = {{GDI_OBJ_BRUSH}, RGB(255,255,255)};
static GDIBRUSH g_stock_ltgray_brush = {{GDI_OBJ_BRUSH}, RGB(192,192,192)};
static GDIBRUSH g_stock_black_brush = {{GDI_OBJ_BRUSH}, RGB(0,0,0)};
static GDIPEN g_stock_black_pen = {{GDI_OBJ_PEN}, PS_SOLID, 1, RGB(0,0,0)};

static GDIFONT g_stock_font = {{GDI_OBJ_FONT}, {16, 0, 0, 0, FW_REGULAR, 0, 0, 0, DEFAULT_CHARSET,
                                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                                               FIXED_PITCH | FF_DONTCARE, {L'S',L'y',L's',L't',L'e',L'm',0}}};

static GDIDC *gdi_alloc_dc(void);
static void gdi_put_pixel(GDIDC *dc, int x, int y, COLORREF color);

HBITMAP GdiCreateBitmapFromBmp(const void *data, uint32_t size) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t off, dib, colors, row_bytes;
    int width, height, bpp, y, x;
    GDIBITMAP *bmp;
    if (!p || size < 54 || p[0] != 'B' || p[1] != 'M') return 0;
    off = *(const uint32_t *)(p + 10);
    dib = *(const uint32_t *)(p + 14);
    width = *(const int32_t *)(p + 18);
    height = *(const int32_t *)(p + 22);
    bpp = *(const uint16_t *)(p + 28);
    if (dib < 40 || width <= 0 || height == 0 || (bpp != 24 && bpp != 32)) return 0;
    if (height < 0) height = -height;
    row_bytes = ((uint32_t)width * (uint32_t)bpp + 31u) & ~31u;
    row_bytes /= 8u;
    if (off >= size || row_bytes * (uint32_t)height > size - off) return 0;
    bmp = (GDIBITMAP *)kmalloc(sizeof(*bmp));
    if (!bmp) return 0;
    bmp->hdr.kind = GDI_OBJ_BITMAP;
    bmp->width = width; bmp->height = height;
    bmp->pixels = (COLORREF *)kmalloc((uint32_t)width * (uint32_t)height * sizeof(COLORREF));
    if (!bmp->pixels) { kfree(bmp); return 0; }
    colors = (bpp / 8);
    for (y = 0; y < height; y++) {
        const uint8_t *src = p + off + (uint32_t)(height - 1 - y) * row_bytes;
        for (x = 0; x < width; x++) {
            const uint8_t *q = src + x * colors;
            bmp->pixels[y * width + x] = RGB(q[2], q[1], q[0]);
        }
    }
    return (HBITMAP)bmp;
}

static HBITMAP GdiCreateBitmapFromJpeg(const void *data, uint32_t size) {
    JPEG_IMAGE image;
    GDIBITMAP *bitmap;
    if (!JpegDecodeImage(data, size, &image)) return 0;
    bitmap = (GDIBITMAP *)kmalloc(sizeof(*bitmap));
    if (!bitmap) { JpegFreeImage(&image); return 0; }
    bitmap->hdr.kind = GDI_OBJ_BITMAP;
    bitmap->width = image.width; bitmap->height = image.height;
    bitmap->pixels = image.pixels;
    /* GDI stores COLORREF as 0x00BBGGRR; the shared JPEG/framebuffer API
       returns 0x00RRGGBB. */
    for (uint32_t i = 0; i < (uint32_t)image.width * (uint32_t)image.height; i++) {
        uint32_t rgb = bitmap->pixels[i];
        bitmap->pixels[i] = ((rgb & 0x0000FFU) << 16) |
                            (rgb & 0x00FF00U) |
                            ((rgb & 0xFF0000U) >> 16);
    }
    return (HBITMAP)bitmap;
}

BOOL GdiPaintWallpaper(HDC hdc, const char *path) {
    static char loaded_path[128];
    static GDIBITMAP *wallpaper;
    GDIDC *dc = (GDIDC *)hdc;
    uint8_t *data = 0;
    uint32_t size = 0;
    int x, y, out_w, out_h;
    if (!dc || !path) return FALSE;
    if (!wallpaper || strcmp(loaded_path, path) != 0) {
        if (!CdfsReadFile(path, &data, &size)) return FALSE;
        if (size >= 2 && data[0] == 0xFF && data[1] == 0xD8)
            wallpaper = (GDIBITMAP *)GdiCreateBitmapFromJpeg(data, size);
        else
            wallpaper = (GDIBITMAP *)GdiCreateBitmapFromBmp(data, size);
        kfree(data);
        if (!wallpaper) return FALSE;
        { int i = 0; while (path[i] && i < (int)sizeof(loaded_path) - 1) { loaded_path[i] = path[i]; i++; } loaded_path[i] = 0; }
    }
    out_w = dc->clip.right - dc->clip.left;
    out_h = dc->clip.bottom - dc->clip.top;
    if (out_w <= 0 || out_h <= 0) return FALSE;
    for (y = 0; y < out_h; y++) for (x = 0; x < out_w; x++)
        gdi_put_pixel(dc, x, y, wallpaper->pixels[(y * wallpaper->height / out_h) * wallpaper->width +
                                                   (x * wallpaper->width / out_w)]);
    return TRUE;
}

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
    dc->selected_font = (HGDIOBJ)&g_stock_font;
    return dc;
}

static void gdi_get_screen_origin(HWND hwnd, int *ox, int *oy) {
    RECT rc;
    if (ox) *ox = 0;
    if (oy) *oy = 0;
    if (!hwnd) return;
    Win32kGetClientScreenRect((void*)hwnd, &rc);
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
        /* GDI COLORREF is 0x00BBGGRR; the framebuffer surface is
         * 0x00RRGGBB. */
        FbPutPixelRGB(ox + x, oy + y,
                      ((color & 0x0000FFU) << 16) |
                      (color & 0x00FF00U) |
                      ((color & 0xFF0000U) >> 16));
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
    int ox = 0, oy = 0, i;
    if (!dc || !str) return;
    if (len < 0) {
        len = 0;
        while (str[len]) len++;
    }
    if (dc->has_custom_origin) {
        ox = dc->origin_x;
        oy = dc->origin_y;
    } else {
        gdi_get_screen_origin(dc->hwnd, &ox, &oy);
    }
    for (i = 0; i < len; i++) {
        int char_left = x + (i * 8);
        int char_top = y;
        int char_right = char_left + 8;
        int char_bottom = char_top + 8;
        if (char_right <= dc->clip.left || char_left >= dc->clip.right ||
            char_bottom <= dc->clip.top || char_top >= dc->clip.bottom) {
            continue;
        }
        FbDrawChar(ox + char_left, oy + char_top, str[i],
                   gdi_color_to_index(dc->text_color),
                   gdi_color_to_index(dc->bk_color));
    }
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

BOOL DrawStateW(HDC hdc, HBRUSH brush, void *draw, LPARAM data, WPARAM w,
                int x, int y, int cx, int cy, UINT flags) {
    RECT rc;
    HBRUSH blue;
    HBRUSH gray;
    GDIBITMAP *bmp = gdi_get_bitmap((HGDIOBJ)(uintptr_t)data);
    (void)brush; (void)draw; (void)w; (void)flags;
    if (!hdc || !data) return FALSE;
    if (bmp) {
        GDIDC *dc = (GDIDC *)hdc;
        int dy, dx;
        int out_w = cx > 0 ? cx : bmp->width;
        int out_h = cy > 0 ? cy : bmp->height;
        for (dy = 0; dy < out_h; dy++) for (dx = 0; dx < out_w; dx++) {
            int sx = dx * bmp->width / out_w;
            int sy = dy * bmp->height / out_h;
            gdi_put_pixel(dc, x + dx, y + dy, bmp->pixels[sy * bmp->width + sx]);
        }
        return TRUE;
    }
    rc.left = x; rc.top = y;
    rc.right = x + (cx > 0 ? cx : 275);
    rc.bottom = y + (cy > 0 ? cy : 54);
    blue = CreateSolidBrush(RGB(0, 0, 128));
    if (blue) {
        FillRect(hdc, &rc, blue);
        DeleteObject(blue);
    }
    if (((uint32_t)(uintptr_t)data & 0xFFFFu) == 20001u) {
        gray = CreateSolidBrush(RGB(192, 192, 192));
        if (gray) { FillRect(hdc, &rc, gray); DeleteObject(gray); }
    } else {
        TextOutW(hdc, x + 12, y + 18, L"discouNT", 8);
    }
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

HFONT CreateFontIndirectW(const LOGFONTW *lplf) {
    GDIFONT *font = (GDIFONT*)kmalloc(sizeof(GDIFONT));
    if (!font) return 0;
    font->hdr.kind = GDI_OBJ_FONT;
    if (lplf) memcpy(&font->lf, lplf, sizeof(LOGFONTW));
    else memset(&font->lf, 0, sizeof(LOGFONTW));
    return (HFONT)font;
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
        /* Memory DCs have no window client rectangle to initialize their
         * clip from.  A selected bitmap is their drawable surface, so make
         * the clip follow it just like a real compatible DC does. */
        if (!dc->is_screen) {
            GDIBITMAP *bmp = (GDIBITMAP *)hgdiobj;
            if (bmp->width > 0 && bmp->height > 0) {
                dc->clip.left = 0;
                dc->clip.top = 0;
                dc->clip.right = bmp->width;
                dc->clip.bottom = bmp->height;
            }
        }
        return old;
    case GDI_OBJ_FONT:
        old = dc->selected_font;
        dc->selected_font = hgdiobj;
        return old;
    default:
        return 0;
    }
}

BOOL DeleteObject(HGDIOBJ ho) {
    GDIHDR *hdr = (GDIHDR*)ho;
    if (!hdr) return FALSE;
    if (ho == (HGDIOBJ)&g_stock_white_brush || ho == (HGDIOBJ)&g_stock_ltgray_brush ||
        ho == (HGDIOBJ)&g_stock_black_brush || ho == (HGDIOBJ)&g_stock_black_pen ||
        ho == (HGDIOBJ)&g_stock_font) {
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
    /* Do not present here.  Complex controls (notably Task Manager's graph
     * control) compose several BitBlt operations into one frame.  Presenting
     * after each copy exposes black/grid/intermediate frames and causes
     * visible flicker.  USER32 presents once painting is complete. */
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
    dc->selected_font = dc->saved[i].selected_font;
    dc->saved[i].used = 0;
    return TRUE;
}

BOOL GetTextMetricsW(HDC hdc, LPTEXTMETRICW lptm) {
    GDIDC *dc = (GDIDC*)hdc;
    GDIFONT *font;
    int height = 16;
    if (!dc || !lptm) return FALSE;
    memset(lptm, 0, sizeof(*lptm));
    font = (GDIFONT*)dc->selected_font;
    if (font && ((GDIHDR*)font)->kind == GDI_OBJ_FONT && font->lf.lfHeight != 0) {
        height = font->lf.lfHeight;
        if (height < 0) height = -height;
        if (height < 8) height = 8;
    }
    lptm->tmHeight = height;
    lptm->tmAscent = (height * 3) / 4;
    lptm->tmDescent = height - lptm->tmAscent;
    lptm->tmInternalLeading = 0;
    lptm->tmExternalLeading = 0;
    lptm->tmAveCharWidth = 8;
    lptm->tmMaxCharWidth = 8;
    lptm->tmWeight = FW_REGULAR;
    lptm->tmOverhang = 0;
    lptm->tmDigitizedAspectX = 96;
    lptm->tmDigitizedAspectY = 96;
    lptm->tmFirstChar = 32;
    lptm->tmLastChar = 126;
    lptm->tmDefaultChar = '?';
    lptm->tmBreakChar = ' ';
    lptm->tmItalic = 0;
    lptm->tmUnderlined = 0;
    lptm->tmStruckOut = 0;
    lptm->tmPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
    lptm->tmCharSet = DEFAULT_CHARSET;
    return TRUE;
}

BOOL GetTextExtentPoint32W(HDC hdc, LPCWSTR lpString, int c, LPSIZE psizl) {
    TEXTMETRICW tm;
    int len = c;
    (void)hdc;
    if (!psizl) return FALSE;
    if (len < 0) {
        len = 0;
        if (lpString) while (lpString[len]) len++;
    }
    if (!GetTextMetricsW(hdc, &tm)) return FALSE;
    psizl->cx = len * tm.tmAveCharWidth;
    psizl->cy = tm.tmHeight;
    return TRUE;
}

BOOL GetTextExtentExPointW(HDC hdc, LPCWSTR lpszStr, int cchString, int nMaxExtent,
                           LPINT lpnFit, LPINT lpnDx, LPSIZE lpSize) {
    TEXTMETRICW tm;
    int len = cchString;
    int i;
    (void)lpszStr;
    if (!GetTextMetricsW(hdc, &tm)) return FALSE;
    if (len < 0) {
        len = 0;
        if (lpszStr) while (lpszStr[len]) len++;
    }
    if (lpnDx) {
        for (i = 0; i < len; i++) lpnDx[i] = (i + 1) * tm.tmAveCharWidth;
    }
    if (lpnFit) {
        if (nMaxExtent <= 0) *lpnFit = 0;
        else *lpnFit = nMaxExtent / tm.tmAveCharWidth;
        if (*lpnFit > len) *lpnFit = len;
    }
    if (lpSize) {
        lpSize->cx = len * tm.tmAveCharWidth;
        lpSize->cy = tm.tmHeight;
    }
    return TRUE;
}

int SetMapMode(HDC hdc, int mode) {
    (void)hdc;
    (void)mode;
    return MM_TEXT;
}

int StartDocW(HDC hdc, const DOCINFOW *lpdi) {
    (void)hdc;
    (void)lpdi;
    return 1;
}

int StartPage(HDC hdc) {
    (void)hdc;
    return 1;
}

int EndPage(HDC hdc) {
    (void)hdc;
    return 1;
}

int EndDoc(HDC hdc) {
    (void)hdc;
    return 1;
}

int GetDeviceCaps(HDC hdc, int index) {
    (void)hdc;
    switch (index) {
    case LOGPIXELSX:
    case LOGPIXELSY:
        return 96;
    case PHYSICALWIDTH:
        return 800;
    case PHYSICALHEIGHT:
        return 600;
    case PHYSICALOFFSETX:
    case PHYSICALOFFSETY:
        return 0;
    default:
        return 0;
    }
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
    GDIBITMAP *color_bmp;
    GDIBITMAP *mask_bmp;
    DISCOUNT_ICON *icon;
    int x, y;
    if (!piconinfo) return 0;
    color_bmp = gdi_get_bitmap((HGDIOBJ)piconinfo->hbmColor);
    if (!color_bmp) return 0;
    mask_bmp = gdi_get_bitmap((HGDIOBJ)piconinfo->hbmMask);

    icon = (DISCOUNT_ICON*)kmalloc(sizeof(DISCOUNT_ICON));
    if (!icon) return 0;
    memset(icon, 0, sizeof(*icon));
    icon->magic = DISCOUNT_ICON_MAGIC;
    icon->width = color_bmp->width;
    icon->height = color_bmp->height;
    icon->pixels = (uint32_t*)kmalloc((uint32_t)(icon->width * icon->height * sizeof(uint32_t)));
    if (!icon->pixels) {
        kfree(icon);
        return 0;
    }

    for (y = 0; y < icon->height; y++) {
        for (x = 0; x < icon->width; x++) {
            uint32_t color = color_bmp->pixels[(y * icon->width) + x] & 0x00FFFFFFU;
            uint32_t alpha = 0xFF000000U;
            if (mask_bmp && x < mask_bmp->width && y < mask_bmp->height) {
                if ((mask_bmp->pixels[(y * mask_bmp->width) + x] & 0x00FFFFFFU) != 0) alpha = 0;
            }
            icon->pixels[(y * icon->width) + x] = color | alpha;
        }
    }
    return (HICON)icon;
}

/* CreateIconIndirect returns a GDI-owned icon, while Win32 applications
 * correctly release it through USER32's DestroyIcon.  Keep the ownership
 * boundary explicit so USER32 can dispose of icons created here. */
BOOL GdiDestroyIcon(HICON hIcon) {
    DISCOUNT_ICON *icon = (DISCOUNT_ICON*)hIcon;
    if (!icon || icon->magic != DISCOUNT_ICON_MAGIC) return FALSE;
    if (icon->pixels) kfree(icon->pixels);
    icon->pixels = NULL;
    icon->magic = 0;
    kfree(icon);
    return TRUE;
}

BOOL GetIconInfo(HICON hIcon, PICONINFO info) {
    DISCOUNT_ICON *icon = (DISCOUNT_ICON *)hIcon;
    if (!icon || icon->magic != DISCOUNT_ICON_MAGIC || !info) return FALSE;
    memset(info, 0, sizeof(*info));
    info->fIcon = TRUE;
    return TRUE;
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
