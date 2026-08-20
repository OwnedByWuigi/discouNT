#include <stdint.h>
#include "w32k.h"
#include "fb.h"
#include "mm.h"
#include "util.h"
#include "mouse.h"
#include "serial.h"
#include "icon.h"

#ifndef WM_MOUSEMOVE
#define WM_MOUSEMOVE 0x0200
#endif
#ifndef WM_LBUTTONDOWN
#define WM_LBUTTONDOWN 0x0201
#endif
#ifndef WM_LBUTTONUP
#define WM_LBUTTONUP 0x0202
#endif
#ifndef MAKELPARAM
#define MAKELPARAM(l, h) ((uint32_t)(((uint16_t)((l) & 0xFFFF)) | (((uint32_t)((uint16_t)((h) & 0xFFFF))) << 16)))
#endif

#define MAX_WINDOWS 32
#define DESKTOP_COLOR        COLOR_BLUE
#define FACE_COLOR           COLOR_LIGHT_GRAY
#define SHADOW_COLOR         COLOR_DARK_GRAY
#define FRAME_COLOR          COLOR_BLACK
#define HILIGHT_COLOR        COLOR_WHITE
#define ACTIVE_CAPTION       COLOR_BLUE
#define INACTIVE_CAPTION     COLOR_DARK_GRAY
#define ACTIVE_CAPTION_TEXT  COLOR_WHITE
#define INACTIVE_CAPTION_TEXT COLOR_LIGHT_GRAY
#define CLIENT_COLOR         COLOR_LIGHT_GRAY
#define TITLEBAR_HEIGHT      18
#define FRAME_THICKNESS      2
#define EDGE_THICKNESS       1
#define BUTTON_SIZE          14
#define BUTTON_MARGIN        2
#define ICON_BOX_SIZE        14
#define MINIMIZED_WIDTH      180

static HANDLE window_list[MAX_WINDOWS];
static int window_count = 0;
static uint32_t window_object_type;
static HANDLE active_window = INVALID_HANDLE;
static int redraw_in_progress;
static int color_preview_overlay;

static int dragging = 0;
static HANDLE drag_window = INVALID_HANDLE;
static int drag_offset_x = 0;
static int drag_offset_y = 0;
static int resizing = 0;
static HANDLE resize_window = INVALID_HANDLE;
static int resize_edge = 0;
static int resize_start_mouse_x = 0;
static int resize_start_mouse_y = 0;
static int resize_start_x = 0;
static int resize_start_y = 0;
static int resize_start_width = 0;
static int resize_start_height = 0;

typedef enum _CAPTION_BUTTON_HIT {
    CAPBTN_NONE = 0,
    CAPBTN_MINIMIZE,
    CAPBTN_MAXIMIZE,
    CAPBTN_CLOSE
} CAPTION_BUTTON_HIT;

enum {
    RESIZE_NONE  = 0,
    RESIZE_LEFT  = 1 << 0,
    RESIZE_RIGHT = 1 << 1,
    RESIZE_TOP   = 1 << 2,
    RESIZE_BOTTOM= 1 << 3
};

static void restore_window_if_needed(HANDLE hwnd, WINDOW *win);
static HANDLE find_topmost_visible_window(void);
static void minimize_window(HANDLE hwnd, WINDOW *win);
static void maximize_window(WINDOW *win);
static void restore_window(WINDOW *win);
static void layout_minimized_window(HANDLE hwnd, WINDOW *win);
static int hit_resize_edge(WINDOW *win, int x, int y);
static void update_cursor_for_point(int x, int y);
static HANDLE find_window_at(int x, int y);

static int minimized_window_height(void) {
    return FRAME_THICKNESS + TITLEBAR_HEIGHT + FRAME_THICKNESS;
}

static int client_top(const WINDOW *win) {
    if (win->minimized) return win->height;
    return (win->style & WS_CAPTION) ? (FRAME_THICKNESS + TITLEBAR_HEIGHT + EDGE_THICKNESS) : FRAME_THICKNESS;
}

static int client_left(const WINDOW *win) {
    return FRAME_THICKNESS + EDGE_THICKNESS;
}

static int client_right(const WINDOW *win) {
    return win->width - FRAME_THICKNESS - EDGE_THICKNESS;
}

static int client_bottom(const WINDOW *win) {
    if (win->minimized) return win->height;
    return win->height - FRAME_THICKNESS - EDGE_THICKNESS;
}

static uint8_t win32k_rgb_to_index(uint32_t color) {
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

static void draw_hline(int x, int y, int w, uint8_t color) {
    FbFillRect(x, y, w, 1, color);
}

static void draw_vline(int x, int y, int h, uint8_t color) {
    FbFillRect(x, y, 1, h, color);
}

static void draw_bevel(int x, int y, int w, int h, uint8_t light, uint8_t dark) {
    if (w <= 1 || h <= 1) return;
    draw_hline(x, y, w - 1, light);
    draw_vline(x, y, h - 1, light);
    draw_hline(x + 1, y + h - 1, w - 1, dark);
    draw_vline(x + w - 1, y + 1, h - 1, dark);
}

static void draw_caption_button(int x, int y, int size, char glyph, int pressed) {
    int ox = pressed ? 1 : 0;
    int oy = pressed ? 1 : 0;
    FbFillRect(x, y, size, size, FACE_COLOR);
    if (pressed) draw_bevel(x, y, size, size, SHADOW_COLOR, HILIGHT_COLOR);
    else draw_bevel(x, y, size, size, HILIGHT_COLOR, SHADOW_COLOR);

    if (glyph == '_') {
        FbFillRect(x + 3 + ox, y + size - 4 + oy, size - 6, 2, FRAME_COLOR);
        return;
    }

    if (glyph == 'O') {
        draw_hline(x + 3 + ox, y + 3 + oy, size - 7, FRAME_COLOR);
        draw_hline(x + 3 + ox, y + size - 5 + oy, size - 7, FRAME_COLOR);
        draw_vline(x + 3 + ox, y + 3 + oy, size - 7, FRAME_COLOR);
        draw_vline(x + size - 5 + ox, y + 3 + oy, size - 7, FRAME_COLOR);
        return;
    }

    if (glyph == 'R') {
        draw_hline(x + 4 + ox, y + 3 + oy, size - 7, FRAME_COLOR);
        draw_vline(x + 4 + ox, y + 3 + oy, size - 7, FRAME_COLOR);
        draw_hline(x + 4 + ox, y + size - 5 + oy, size - 7, FRAME_COLOR);
        draw_vline(x + size - 4 + ox, y + 3 + oy, size - 7, FRAME_COLOR);

        draw_hline(x + 2 + ox, y + 5 + oy, size - 7, FRAME_COLOR);
        draw_vline(x + 2 + ox, y + 5 + oy, size - 7, FRAME_COLOR);
        draw_hline(x + 2 + ox, y + size - 3 + oy, size - 7, FRAME_COLOR);
        draw_vline(x + size - 6 + ox, y + 5 + oy, size - 7, FRAME_COLOR);
        return;
    }

    if (glyph == 'X') {
        for (int i = 0; i < size - 6; i++) {
            FbFillRect(x + 3 + i + ox, y + 3 + i + oy, 1, 1, FRAME_COLOR);
            FbFillRect(x + 3 + i + ox, y + size - 4 - i + oy, 1, 1, FRAME_COLOR);
        }
        return;
    }

    FbDrawChar(x + 3 + ox, y + 3 + oy, glyph, FRAME_COLOR, FACE_COLOR);
}

static int get_caption_button_count(const WINDOW *win) {
    if (!(win->style & WS_CAPTION) || !(win->style & WS_SYSMENU)) return 0;
    return 3;
}

static int get_caption_button_x(const WINDOW *win, int index_from_right) {
    return win->x + win->width - FRAME_THICKNESS - BUTTON_MARGIN - BUTTON_SIZE -
           (index_from_right * (BUTTON_SIZE + 2));
}

static CAPTION_BUTTON_HIT hit_caption_button(WINDOW *win, int x, int y) {
    int by;
    if (!(win->style & WS_CAPTION) || !(win->style & WS_SYSMENU)) return CAPBTN_NONE;

    by = win->y + FRAME_THICKNESS + BUTTON_MARGIN;
    if (!(y >= by && y < by + BUTTON_SIZE)) return CAPBTN_NONE;

    if (x >= get_caption_button_x(win, 0) && x < get_caption_button_x(win, 0) + BUTTON_SIZE) return CAPBTN_CLOSE;
    if (x >= get_caption_button_x(win, 1) && x < get_caption_button_x(win, 1) + BUTTON_SIZE) return CAPBTN_MAXIMIZE;
    if (x >= get_caption_button_x(win, 2) && x < get_caption_button_x(win, 2) + BUTTON_SIZE) return CAPBTN_MINIMIZE;
    return CAPBTN_NONE;
}

static void draw_sys_icon(WINDOW *win, int x, int y) {
    char glyph = 0;
    uint8_t fill = ACTIVE_CAPTION;
    uintptr_t icon_key = (uintptr_t)(win ? (win->small_icon ? win->small_icon : win->big_icon) : 0);
    DISCOUNT_ICON *icon = (DISCOUNT_ICON*)icon_key;

    FbFillRect(x, y, ICON_BOX_SIZE, ICON_BOX_SIZE, FACE_COLOR);
    draw_bevel(x, y, ICON_BOX_SIZE, ICON_BOX_SIZE, HILIGHT_COLOR, SHADOW_COLOR);

    if (icon && icon->magic == DISCOUNT_ICON_MAGIC && icon->pixels && icon->width > 0 && icon->height > 0) {
        int dst_w = ICON_BOX_SIZE - 2;
        int dst_h = ICON_BOX_SIZE - 2;
        for (int dy = 0; dy < dst_h; dy++) {
            int sy = (dy * icon->height) / dst_h;
            for (int dx = 0; dx < dst_w; dx++) {
                int sx = (dx * icon->width) / dst_w;
                uint32_t pixel = icon->pixels[(sy * icon->width) + sx];
                uint8_t alpha = (uint8_t)(pixel >> 24);
                if (!alpha) continue;
                FbPutPixel(x + 1 + dx, y + 1 + dy, win32k_rgb_to_index(pixel & 0x00FFFFFFU));
            }
        }
        return;
    }

    if (icon_key) {
        int i = 0;
        while (win->title[i]) {
            char c = win->title[i];
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
                glyph = (c >= 'a' && c <= 'z') ? (c - 32) : c;
                break;
            }
            i++;
        }
        fill = (uint8_t)(1 + ((icon_key >> 4) % 14));
        if (fill == FACE_COLOR || fill == HILIGHT_COLOR) fill = ACTIVE_CAPTION;
        FbFillRect(x + 2, y + 2, ICON_BOX_SIZE - 4, ICON_BOX_SIZE - 4, fill);
        if (glyph) {
            FbDrawChar(x + 4, y + 3, glyph, COLOR_WHITE, fill);
            return;
        }
    }

    FbFillRect(x + 3, y + 3, 8, 8, ACTIVE_CAPTION);
    FbFillRect(x + 5, y + 5, 4, 1, HILIGHT_COLOR);
}

static void draw_window_frame(WINDOW *win) {
    int x = win->x;
    int y = win->y;
    int w = win->width;
    int h = win->height;
    int title_color = win->active ? ACTIVE_CAPTION : INACTIVE_CAPTION;
    int title_text_color = win->active ? ACTIVE_CAPTION_TEXT : INACTIVE_CAPTION_TEXT;

    FbFillRect(x, y, w, h, FACE_COLOR);

    draw_bevel(x, y, w, h, HILIGHT_COLOR, FRAME_COLOR);
    draw_bevel(x + 1, y + 1, w - 2, h - 2, HILIGHT_COLOR, SHADOW_COLOR);

    if (win->style & WS_CAPTION) {
        int cap_x = x + FRAME_THICKNESS;
        int cap_y = y + FRAME_THICKNESS;
        int cap_w = w - (FRAME_THICKNESS * 2);

        FbFillRect(cap_x, cap_y, cap_w, TITLEBAR_HEIGHT, title_color);

        if (win->style & WS_SYSMENU) {
            draw_sys_icon(win, cap_x + BUTTON_MARGIN, cap_y + BUTTON_MARGIN);
        }

        if (win->style & WS_SYSMENU) {
            int btn_y = cap_y + BUTTON_MARGIN;
            int close_x = get_caption_button_x(win, 0);
            int max_x = get_caption_button_x(win, 1);
            int min_x = get_caption_button_x(win, 2);
            draw_caption_button(min_x, btn_y, BUTTON_SIZE, win->minimized ? 'R' : '_', 0);
            draw_caption_button(max_x, btn_y, BUTTON_SIZE, win->maximized ? 'R' : 'O', 0);
            draw_caption_button(close_x, btn_y, BUTTON_SIZE, 'X', 0);
        }

        {
            int text_x = cap_x + BUTTON_MARGIN + ((win->style & WS_SYSMENU) ? (ICON_BOX_SIZE + 4) : 2);
            int text_right = x + w - FRAME_THICKNESS - BUTTON_MARGIN -
                             ((win->style & WS_SYSMENU) ? ((get_caption_button_count(win) * (BUTTON_SIZE + 2)) + 2) : 2);
            int max_chars = (text_right - text_x) / 8;
            char title_buf[64];
            int len = (int)strlen(win->title);
            if (len > max_chars) len = max_chars;
            if (len < 0) len = 0;
            memcpy(title_buf, win->title, (uint32_t)len);
            title_buf[len] = 0;
            FbDrawString(text_x, cap_y + 4, title_buf, title_text_color, title_color);
        }
    }

    {
        int left = x + client_left(win);
        int top = y + client_top(win);
        int right = x + client_right(win);
        int bottom = y + client_bottom(win);
        int client_w = right - left;
        int client_h = bottom - top;

        if (client_w > 0 && client_h > 0) {
            FbFillRect(left, top, client_w, client_h, CLIENT_COLOR);
            draw_bevel(left - 1, top - 1, client_w + 2, client_h + 2, SHADOW_COLOR, HILIGHT_COLOR);
        }
    }
}

static void set_window_active(HANDLE hwnd) {
    active_window = hwnd;

    for (int i = 0; i < window_count; i++) {
        WINDOW *win = (WINDOW*)ObReferenceObject(window_list[i]);
        if (win) {
            win->active = (window_list[i] == hwnd) ? 1 : 0;
            ObDereferenceObject(window_list[i]);
        }
    }
}

static void raise_window(HANDLE hwnd) {
    int pos = -1;
    for (int i = 0; i < window_count; i++) {
        if (window_list[i] == hwnd) {
            pos = i;
            break;
        }
    }
    if (pos < 0 || pos == window_count - 1) return;

    for (int i = pos; i < window_count - 1; i++) {
        window_list[i] = window_list[i + 1];
    }
    window_list[window_count - 1] = hwnd;
}

static int is_in_window(WINDOW *win, int x, int y) {
    return (x >= win->x && x < win->x + win->width &&
            y >= win->y && y < win->y + win->height);
}

static int is_title_bar(WINDOW *win, int x, int y) {
    if (!(win->style & WS_CAPTION)) return 0;

    {
        int tx = win->x + FRAME_THICKNESS;
        int ty = win->y + FRAME_THICKNESS;
        int tw = win->width - (FRAME_THICKNESS * 2);
        if (!(x >= tx && x < tx + tw && y >= ty && y < ty + TITLEBAR_HEIGHT)) return 0;
    }

    if (hit_caption_button(win, x, y) != CAPBTN_NONE) return 0;
    if ((win->style & WS_SYSMENU) &&
        x < win->x + FRAME_THICKNESS + BUTTON_MARGIN + ICON_BOX_SIZE + 2) return 0;
    return 1;
}

static int hit_resize_edge(WINDOW *win, int x, int y) {
    int edge = RESIZE_NONE;
    int border = 4;

    if (!win || win->minimized || win->maximized) return RESIZE_NONE;
    if (!(win->style & WS_THICKFRAME)) return RESIZE_NONE;
    if (!is_in_window(win, x, y)) return RESIZE_NONE;

    if (x < win->x + border) edge |= RESIZE_LEFT;
    else if (x >= win->x + win->width - border) edge |= RESIZE_RIGHT;

    if (y < win->y + border) edge |= RESIZE_TOP;
    else if (y >= win->y + win->height - border) edge |= RESIZE_BOTTOM;

    return edge;
}

static void update_cursor_for_point(int x, int y) {
    int edge = RESIZE_NONE;
    MOUSE_CURSOR_TYPE type = MOUSE_CURSOR_ARROW;
    HANDLE hwnd;
    WINDOW *win;

    if (resizing) {
        edge = resize_edge;
    } else {
        hwnd = find_window_at(x, y);
        if (hwnd != INVALID_HANDLE) {
            win = (WINDOW*)ObReferenceObject(hwnd);
            if (win) {
                edge = hit_resize_edge(win, x, y);
                ObDereferenceObject(hwnd);
            }
        }
    }

    if ((edge & RESIZE_LEFT) && (edge & RESIZE_TOP)) type = MOUSE_CURSOR_SIZENWSE;
    else if ((edge & RESIZE_RIGHT) && (edge & RESIZE_BOTTOM)) type = MOUSE_CURSOR_SIZENWSE;
    else if ((edge & RESIZE_RIGHT) && (edge & RESIZE_TOP)) type = MOUSE_CURSOR_SIZENESW;
    else if ((edge & RESIZE_LEFT) && (edge & RESIZE_BOTTOM)) type = MOUSE_CURSOR_SIZENESW;
    else if (edge & (RESIZE_LEFT | RESIZE_RIGHT)) type = MOUSE_CURSOR_SIZEWE;
    else if (edge & (RESIZE_TOP | RESIZE_BOTTOM)) type = MOUSE_CURSOR_SIZENS;

    MouseSetCursorType(type);
}

static HANDLE find_topmost_visible_window(void) {
    for (int i = window_count - 1; i >= 0; i--) {
        WINDOW *win = (WINDOW*)ObReferenceObject(window_list[i]);
        if (win && win->visible && !win->minimized) {
            ObDereferenceObject(window_list[i]);
            return window_list[i];
        }
        if (win) ObDereferenceObject(window_list[i]);
    }
    return INVALID_HANDLE;
}

static void restore_window_if_needed(HANDLE hwnd, WINDOW *win) {
    (void)hwnd;
    if (!win || !win->minimized) return;
    win->minimized = 0;
    win->visible = 1;
    if (win->restore_width > 0) win->width = win->restore_width;
    if (win->restore_height > 0) win->height = win->restore_height;
    win->x = win->restore_x;
    win->y = win->restore_y;
}

static void layout_minimized_window(HANDLE hwnd, WINDOW *win) {
    int screen_w;
    int screen_h;
    int slot = 0;

    if (!win) return;

    screen_w = FbGetWidth();
    screen_h = FbGetHeight();
    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 480;

    for (int i = 0; i < window_count; i++) {
        HANDLE other_hwnd = window_list[i];
        WINDOW *other;
        if (other_hwnd == hwnd) continue;
        other = (WINDOW*)ObReferenceObject(other_hwnd);
        if (other) {
            if (other->visible && other->minimized) slot++;
            ObDereferenceObject(other_hwnd);
        }
    }

    win->width = MINIMIZED_WIDTH;
    win->height = minimized_window_height();
    win->x = 8 + (slot * (MINIMIZED_WIDTH + 8));
    if (win->x + win->width > screen_w) {
        int columns = (screen_w - 8) / (MINIMIZED_WIDTH + 8);
        int row = 0;
        if (columns < 1) columns = 1;
        row = slot / columns;
        slot = slot % columns;
        win->x = 8 + (slot * (MINIMIZED_WIDTH + 8));
        win->y = screen_h - 8 - minimized_window_height() - (row * (minimized_window_height() + 8));
    } else {
        win->y = screen_h - 8 - minimized_window_height();
    }
    if (win->y < 0) win->y = 0;
}

static void minimize_window(HANDLE hwnd, WINDOW *win) {
    if (!win || win->minimized) return;
    win->restore_x = win->x;
    win->restore_y = win->y;
    win->restore_width = win->width;
    win->restore_height = win->height;
    win->minimized = 1;
    win->visible = 1;
    layout_minimized_window(hwnd, win);
    if (drag_window == hwnd) {
        dragging = 0;
        drag_window = INVALID_HANDLE;
    }
}

static void maximize_window(WINDOW *win) {
    int screen_w;
    int screen_h;
    if (!win || win->maximized) return;

    screen_w = FbGetWidth();
    screen_h = FbGetHeight();
    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 480;

    win->restore_x = win->x;
    win->restore_y = win->y;
    win->restore_width = win->width;
    win->restore_height = win->height;
    win->x = 0;
    win->y = 0;
    win->width = screen_w;
    win->height = screen_h;
    win->maximized = 1;
}

static void restore_window(WINDOW *win) {
    if (!win || !win->maximized) return;
    if (win->restore_width > 0) win->width = win->restore_width;
    if (win->restore_height > 0) win->height = win->restore_height;
    win->x = win->restore_x;
    win->y = win->restore_y;
    win->maximized = 0;
}

static HANDLE find_window_at(int x, int y) {
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

static int rects_intersect(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
    if (x1 + w1 <= x2) return 0;
    if (x2 + w2 <= x1) return 0;
    if (y1 + h1 <= y2) return 0;
    if (y2 + h2 <= y1) return 0;
    return 1;
}

static void render_scene(HANDLE skip_window) {
    FbClearScreen(DESKTOP_COLOR);

    for (int i = 0; i < window_count; i++) {
        HANDLE hwnd = window_list[i];
        WINDOW *win;
        if (hwnd == skip_window) continue;

        win = (WINDOW*)ObReferenceObject(hwnd);
        if (win && win->visible) {
            draw_window_frame(win);
            if (win->wndProc && !win->minimized) win->wndProc(hwnd, WM_PAINT, 0, 0);
        }
        if (win) ObDereferenceObject(hwnd);
    }
}

static void begin_fast_drag(HANDLE hwnd) {
    (void)hwnd;
    Win32kRedrawAll();
}

static void fast_drag_present(WINDOW *win, int old_x, int old_y) {
    (void)win;
    (void)old_x;
    (void)old_y;
    Win32kRedrawAll();
}

void Win32kInit(void *mb_info) {
    window_object_type = ObRegisterObjectType("Window", 0);
    FbInit(mb_info);
    FbClearScreen(DESKTOP_COLOR);
    window_count = 0;
    active_window = INVALID_HANDLE;
    dragging = 0;
    drag_window = INVALID_HANDLE;
    resizing = 0;
    resize_window = INVALID_HANDLE;
    
    if (FbIsFramebuffer()) {
        SerialPutString("[Win32k] Using linear framebuffer\r\n");
    } else {
        SerialPutString("[Win32k] Using VGA fallback\r\n");
    }
}

HANDLE Win32kRegisterClass(const char *className, uint32_t style, void (*wndProc)(HANDLE, uint32_t, uint32_t, uint32_t)) {
    WNDCLASS *wc = (WNDCLASS*)kmalloc(sizeof(WNDCLASS));
    HANDLE hclass;
    if (!wc) {
        SerialPutString("[Win32k] RegisterClass kmalloc failed\r\n");
        return INVALID_HANDLE;
    }
    memset(wc, 0, sizeof(WNDCLASS));
    int len = strlen(className);
    if (len > 63) len = 63;
    memcpy(wc->className, className, len);
    wc->style = style;
    wc->wndProc = wndProc;
    hclass = ObCreateObject(window_object_type, className, wc, sizeof(WNDCLASS));
    if (hclass == INVALID_HANDLE) {
        SerialPutString("[Win32k] RegisterClass ObCreateObject failed for ");
        SerialPutString(className);
        SerialPutString("\r\n");
        kfree(wc);
    }
    return hclass;
}

HANDLE Win32kCreateWindowByClass(HANDLE hClass, const char *title, int x, int y, int w, int h, uint32_t style) {
    if (hClass == INVALID_HANDLE) {
        SerialPutString("[Win32k] CreateWindowByClass invalid class handle\r\n");
        return INVALID_HANDLE;
    }

    WNDCLASS *wc = (WNDCLASS*)ObReferenceObject(hClass);
    if (!wc) {
        SerialPutString("[Win32k] CreateWindowByClass class reference failed\r\n");
        return INVALID_HANDLE;
    }
    
    WINDOW *win = (WINDOW*)kmalloc(sizeof(WINDOW));
    if (!win) {
        SerialPutString("[Win32k] CreateWindowByClass window kmalloc failed\r\n");
        ObDereferenceObject(hClass);
        return INVALID_HANDLE;
    }
    memset(win, 0, sizeof(WINDOW));
    int len = strlen(title);
    if (len > 63) len = 63;
    memcpy(win->title, title, len);
    if (w < 96) w = 96;
    if (h < 64) h = 64;
    win->x = x; win->y = y;
    win->width = w; win->height = h;
    win->restore_x = x; win->restore_y = y;
    win->restore_width = w; win->restore_height = h;
    win->style = style;
    win->visible = (style & WS_VISIBLE) ? 1 : 0;
    win->active = 0;
    win->minimized = 0;
    win->maximized = 0;
    win->wndClass = wc;
    win->wndProc = wc->wndProc;
    
    HANDLE hwnd = ObCreateObject(window_object_type, title, win, sizeof(WINDOW));
    if (hwnd == INVALID_HANDLE) {
        SerialPutString("[Win32k] CreateWindowByClass ObCreateObject failed for ");
        SerialPutString(title);
        SerialPutString("\r\n");
        kfree(win);
        ObDereferenceObject(hClass);
        return INVALID_HANDLE;
    }
    
    if (window_count < MAX_WINDOWS) {
        window_list[window_count++] = hwnd;
    } else {
        SerialPutString("[Win32k] CreateWindowByClass window list full\r\n");
    }

    raise_window(hwnd);
    set_window_active(hwnd);
    
    if (win->wndProc) win->wndProc(hwnd, WM_CREATE, 0, 0);
    ObDereferenceObject(hClass);
    return hwnd;
}

HANDLE Win32kCreateWindow(const char *className, const char *title, int x, int y, int w, int h, uint32_t style) {
    HANDLE hClass = ObFindObject(className, window_object_type);
    if (hClass == INVALID_HANDLE) return INVALID_HANDLE;
    return Win32kCreateWindowByClass(hClass, title, x, y, w, h, style);
}

void Win32kDestroyWindow(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    
    for (int i = 0; i < window_count; i++) {
        if (window_list[i] == hwnd) {
            for (int j = i; j < window_count - 1; j++) {
                window_list[j] = window_list[j + 1];
            }
            window_count--;
            break;
        }
    }

    if (active_window == hwnd) {
        active_window = (window_count > 0) ? window_list[window_count - 1] : INVALID_HANDLE;
        set_window_active(active_window);
    }
    
    if (win->wndProc) win->wndProc(hwnd, WM_DESTROY, 0, 0);
    ObDereferenceObject(hwnd);
    ObDereferenceObject(hwnd);
}

void Win32kShowWindow(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    win->minimized = 0;
    win->visible = 1;
    draw_window_frame(win);
    ObDereferenceObject(hwnd);
}

void Win32kUpdateWindow(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (win && win->wndProc && !win->minimized) {
        win->wndProc(hwnd, WM_PAINT, 0, 0);
    }
    if (win) ObDereferenceObject(hwnd);
}

void Win32kGetClientRect(HANDLE hwnd, RECT *rect) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (win && rect) {
        rect->left = 0;
        rect->top = 0;
        rect->right = client_right(win) - client_left(win);
        rect->bottom = client_bottom(win) - client_top(win);
    }
    if (win) ObDereferenceObject(hwnd);
}

void Win32kGetClientScreenRect(HANDLE hwnd, RECT *rect) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (win && rect) {
        rect->left = win->x + client_left(win);
        rect->top = win->y + client_top(win);
        rect->right = win->x + client_right(win);
        rect->bottom = win->y + client_bottom(win);
    }
    if (win) ObDereferenceObject(hwnd);
}

void Win32kGetWindowRect(HANDLE hwnd, RECT *rect) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (win && rect) {
        rect->left = win->x;
        rect->top = win->y;
        rect->right = win->x + win->width;
        rect->bottom = win->y + win->height;
    }
    if (win) ObDereferenceObject(hwnd);
}

void Win32kHandleMouseDown(int x, int y, int button) {
    if (button != 1) return;
    
    HANDLE hwnd = find_window_at(x, y);
    if (hwnd == INVALID_HANDLE) return;

    raise_window(hwnd);
    set_window_active(hwnd);
    
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    
    switch (hit_caption_button(win, x, y)) {
    case CAPBTN_CLOSE:
        SerialPutString("[Win32k] Close window\r\n");
        ObDereferenceObject(hwnd);
        /* Give USER32/the application the normal WM_CLOSE first.  Destroying
         * the kernel window directly left USER32's HWND and the app's message
         * loop alive, so closed programs could never really exit. */
        win = (WINDOW*)ObReferenceObject(hwnd);
        if (win && win->wndProc) win->wndProc(hwnd, WM_CLOSE, 0, 0);
        if (win) ObDereferenceObject(hwnd);
        if (ObReferenceObject(hwnd)) {
            ObDereferenceObject(hwnd);
            Win32kDestroyWindow(hwnd);
        }
        Win32kRedrawAll();
        return;
    case CAPBTN_MAXIMIZE:
        if (win->minimized) {
            restore_window_if_needed(hwnd, win);
            maximize_window(win);
        } else if (win->maximized) {
            restore_window(win);
        } else {
            maximize_window(win);
        }
        ObDereferenceObject(hwnd);
        Win32kRedrawAll();
        return;
    case CAPBTN_MINIMIZE:
        if (win->minimized) {
            restore_window_if_needed(hwnd, win);
            ObDereferenceObject(hwnd);
            set_window_active(hwnd);
            Win32kRedrawAll();
            return;
        }
        minimize_window(hwnd, win);
        ObDereferenceObject(hwnd);
        set_window_active(find_topmost_visible_window());
        Win32kRedrawAll();
        return;
    default:
        break;
    }

    {
        int resize_hit = hit_resize_edge(win, x, y);
        if (resize_hit != RESIZE_NONE) {
            resizing = 1;
            resize_window = hwnd;
            resize_edge = resize_hit;
            resize_start_mouse_x = x;
            resize_start_mouse_y = y;
            resize_start_x = win->x;
            resize_start_y = win->y;
            resize_start_width = win->width;
            resize_start_height = win->height;
            ObDereferenceObject(hwnd);
            Win32kRedrawAll();
            return;
        }
    }
    
    if (is_title_bar(win, x, y)) {
        SerialPutString("[Win32k] Drag start\r\n");
        dragging = 1;
        drag_window = hwnd;
        drag_offset_x = x - win->x;
        drag_offset_y = y - win->y;
        begin_fast_drag(hwnd);
        ObDereferenceObject(hwnd);
        Win32kRedrawAll();
        return;
    }

    if (win->wndProc &&
        x >= client_left(win) + win->x &&
        x < client_right(win) + win->x &&
        y >= client_top(win) + win->y &&
        y < client_bottom(win) + win->y) {
        int client_x = x - (win->x + client_left(win));
        int client_y = y - (win->y + client_top(win));
        win->wndProc(hwnd, WM_LBUTTONDOWN, 0, (uint32_t)MAKELPARAM(client_x, client_y));
    }

    ObDereferenceObject(hwnd);
    Win32kRedrawAll();
}

void Win32kHandleMouseUp(int x, int y, int button) {
    if (button != 1) return;
    
    if (dragging) {
        SerialPutString("[Win32k] Drag end\r\n");
        dragging = 0;
        drag_window = INVALID_HANDLE;
        Win32kRedrawAll();
    }
    if (resizing) {
        resizing = 0;
        resize_window = INVALID_HANDLE;
        resize_edge = RESIZE_NONE;
        Win32kRedrawAll();
    }

    {
        HANDLE hwnd = find_window_at(x, y);
        WINDOW *win = hwnd != INVALID_HANDLE ? (WINDOW*)ObReferenceObject(hwnd) : 0;
        if (win) {
            if (win->wndProc &&
                x >= client_left(win) + win->x &&
                x < client_right(win) + win->x &&
                y >= client_top(win) + win->y &&
                y < client_bottom(win) + win->y) {
                int client_x = x - (win->x + client_left(win));
                int client_y = y - (win->y + client_top(win));
                win->wndProc(hwnd, WM_LBUTTONUP, 0, (uint32_t)MAKELPARAM(client_x, client_y));
            }
            ObDereferenceObject(hwnd);
        }
    }
    update_cursor_for_point(x, y);
}

void Win32kHandleMouseMove(int x, int y) {
    if (resizing && resize_window != INVALID_HANDLE) {
        WINDOW *win = (WINDOW*)ObReferenceObject(resize_window);
        if (!win) {
            resizing = 0;
            resize_window = INVALID_HANDLE;
            resize_edge = RESIZE_NONE;
            return;
        }

        {
            int dx = x - resize_start_mouse_x;
            int dy = y - resize_start_mouse_y;
            int new_x = resize_start_x;
            int new_y = resize_start_y;
            int new_w = resize_start_width;
            int new_h = resize_start_height;
            int min_w = 96;
            int min_h = 64;
            int screen_w = FbGetWidth();
            int screen_h = FbGetHeight();

            if (screen_w <= 0) screen_w = 640;
            if (screen_h <= 0) screen_h = 480;

            if (resize_edge & RESIZE_RIGHT) new_w = resize_start_width + dx;
            if (resize_edge & RESIZE_BOTTOM) new_h = resize_start_height + dy;
            if (resize_edge & RESIZE_LEFT) {
                new_x = resize_start_x + dx;
                new_w = resize_start_width - dx;
            }
            if (resize_edge & RESIZE_TOP) {
                new_y = resize_start_y + dy;
                new_h = resize_start_height - dy;
            }

            if (new_w < min_w) {
                if (resize_edge & RESIZE_LEFT) new_x -= (min_w - new_w);
                new_w = min_w;
            }
            if (new_h < min_h) {
                if (resize_edge & RESIZE_TOP) new_y -= (min_h - new_h);
                new_h = min_h;
            }

            if (new_x < 0) {
                if (resize_edge & RESIZE_LEFT) new_w += new_x;
                new_x = 0;
            }
            if (new_y < 0) {
                if (resize_edge & RESIZE_TOP) new_h += new_y;
                new_y = 0;
            }
            if (new_x + new_w > screen_w) {
                if (resize_edge & RESIZE_RIGHT) new_w = screen_w - new_x;
                else new_x = screen_w - new_w;
            }
            if (new_y + new_h > screen_h) {
                if (resize_edge & RESIZE_BOTTOM) new_h = screen_h - new_y;
                else new_y = screen_h - new_h;
            }

            if (new_w < min_w) new_w = min_w;
            if (new_h < min_h) new_h = min_h;
            if (new_x < 0) new_x = 0;
            if (new_y < 0) new_y = 0;

            win->x = new_x;
            win->y = new_y;
            win->width = new_w;
            win->height = new_h;
            if (!win->maximized && !win->minimized) {
                win->restore_x = new_x;
                win->restore_y = new_y;
                win->restore_width = new_w;
                win->restore_height = new_h;
            }
        }

        ObDereferenceObject(resize_window);
        update_cursor_for_point(x, y);
        Win32kRedrawAll();
        return;
    }

    {
        HANDLE hwnd = find_window_at(x, y);
        WINDOW *hover = hwnd != INVALID_HANDLE ? (WINDOW*)ObReferenceObject(hwnd) : 0;
        if (hover) {
            if (hover->wndProc &&
                x >= hover->x + client_left(hover) &&
                x < hover->x + client_right(hover) &&
                y >= hover->y + client_top(hover) &&
                y < hover->y + client_bottom(hover)) {
                int client_x = x - (hover->x + client_left(hover));
                int client_y = y - (hover->y + client_top(hover));
                hover->wndProc(hwnd, WM_MOUSEMOVE, 0, (uint32_t)MAKELPARAM(client_x, client_y));
            }
            ObDereferenceObject(hwnd);
        }
    }

    update_cursor_for_point(x, y);
    if (!dragging || drag_window == INVALID_HANDLE) return;
    
    WINDOW *win = (WINDOW*)ObReferenceObject(drag_window);
    if (!win) { dragging = 0; return; }
    
    {
        int old_x = win->x;
        int old_y = win->y;
    int new_x = x - drag_offset_x;
    int new_y = y - drag_offset_y;
    int screen_w = FbGetWidth();
    int screen_h = FbGetHeight();
    if (screen_w <= 0) screen_w = 640;
    if (screen_h <= 0) screen_h = 480;

    if (new_x < 0) new_x = 0;
    if (new_y < 0) new_y = 0;
    if (new_x + win->width > screen_w) new_x = screen_w - win->width;
    if (new_y + win->height > screen_h) new_y = screen_h - win->height;
    if (new_x < 0) new_x = 0;
    if (new_y < 0) new_y = 0;
    
    if (new_x != win->x || new_y != win->y) {
        if (win->maximized) restore_window(win);
        win->restore_x = new_x;
        win->restore_y = new_y;
        win->x = new_x;
        win->y = new_y;
        MouseEraseCursor();
        fast_drag_present(win, old_x, old_y);
    }
    }
    
    ObDereferenceObject(drag_window);
}

void Win32kRefreshCursor(void) {
    MouseEraseCursor();
    MouseDrawCursor();
    FbSwapBuffers();
}

int Win32kIsDragging(void) {
    return dragging;
}

int Win32kIsResizing(void) {
    return resizing;
}

HANDLE Win32kGetActiveWindow(void) {
    return active_window;
}

void Win32kActivateWindow(HANDLE hwnd) {
    if (hwnd == INVALID_HANDLE) return;
    raise_window(hwnd);
    set_window_active(hwnd);
}

void Win32kSetWindowIcons(HANDLE hwnd, HANDLE big_icon, HANDLE small_icon) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    win->big_icon = big_icon;
    win->small_icon = small_icon ? small_icon : big_icon;
    ObDereferenceObject(hwnd);
}

void Win32kRedrawAll(void) {
    if (redraw_in_progress) return;
    redraw_in_progress = 1;
    MouseEraseCursor();

    FbClearScreen(DESKTOP_COLOR);

    for (int i = 0; i < window_count; i++) {
        WINDOW *win = (WINDOW*)ObReferenceObject(window_list[i]);
        if (win && win->visible) {
            draw_window_frame(win);
            if (win->wndProc && !win->minimized) {
                win->wndProc(window_list[i], WM_PAINT, 0, 0);
            }
        }
        if (win) ObDereferenceObject(window_list[i]);
    }

    if (color_preview_overlay) {
        int sw = FbGetWidth(), sh = FbGetHeight();
        for (int py = 0; py < sh; py += 8) for (int px = 0; px < sw; px += 8) {
            uint32_t r = (uint32_t)(px * 255 / (sw > 1 ? sw - 1 : 1));
            uint32_t g = (uint32_t)(py * 255 / (sh > 1 ? sh - 1 : 1));
            uint32_t b = (uint32_t)(((px / 8 + py / 8) & 31) * 255 / 31);
            FbFillRectRGB(px, py, 8, 8, (r << 16) | (g << 8) | b);
        }
    }

    MouseDrawCursor();
    FbSwapBuffers();
    redraw_in_progress = 0;
}

void Win32kSetColorPreview(int enabled) {
    color_preview_overlay = enabled ? 1 : 0;
    Win32kRedrawAll();
}
