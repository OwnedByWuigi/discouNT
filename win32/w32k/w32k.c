#include <stdint.h>
#include "w32k.h"
#include "fb.h"
#include "mm/mm.h"
#include "core/util.h"
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
#define W32K_SW_HIDE           0
#define W32K_SW_SHOWNORMAL     1
#define W32K_SW_SHOWMINIMIZED  2
#define W32K_SW_MINIMIZE       6
#define W32K_SW_RESTORE        9

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
static uint32_t *drag_pixels = 0;
static int drag_pixels_width = 0;
static int drag_pixels_height = 0;
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

/* A caption button that has been pushed down but not yet released.  Like
 * WINE's track_min_max_box, the window only triggers after the button is
 * released over it, so a click that is dragged elsewhere is cancelled. */
static int caption_press_hit = CAPBTN_NONE;
static WINDOW *caption_press_win = 0;
static HANDLE caption_press_hwnd = INVALID_HANDLE;

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
static void set_window_active(HANDLE hwnd);

static void paint_desktop_area(int x, int y, int w, int h) {
    if (!FbPaintWallpaper(x, y, w, h, "WEB/IMG0.BMP")) FbFillRect(x, y, w, h, DESKTOP_COLOR);
}

static int window_is_topmost(const WINDOW *win) {
    if (!win || !win->wndClass) return 0;
    return strcmp(win->wndClass->className, "Shell_TrayWnd") == 0 ||
           strcmp(win->wndClass->className, "MenuPopup") == 0;
}

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

/* The classic Windows caption symbols are Marlett font glyphs (0x30..0x32,
 * 0x72).  The OS uses a fixed 16-color palette with no such font, so the
 * glyphs are hand rasterized here to match those symbols rather than the
 * literal ASCII letters used before.  Each glyph is drawn inside a button of
 * the given size, centered on the face between its bevel and edge. */
static void draw_caption_glyph_min(int x, int y, int size, int o) {
    int w = size - 6; /* ~8 px wide underline bar */
    FbFillRect(x + (size - w) / 2 + o, y + size - 6 + o, w, 1, FRAME_COLOR);
}

static void draw_caption_glyph_max(int x, int y, int size, int o) {
    int l = x + 3 + o;
    int t = y + 3 + o;
    int w = size - 6;
    draw_hline(l, t, w, FRAME_COLOR);
    draw_vline(l, t, w, FRAME_COLOR);
    draw_hline(l, t + w, w, FRAME_COLOR);
    draw_vline(l + w, t, w, FRAME_COLOR);
}

static void draw_caption_glyph_restore(int x, int y, int size, int o) {
    int w = size - 6;        /* 8 px wide back window */
    /* Back window, upper-left */
    draw_hline(x + 1 + o, y + 1 + o, w, FRAME_COLOR);
    draw_vline(x + 1 + o, y + 1 + o, w, FRAME_COLOR);
    draw_hline(x + 1 + o, y + 1 + w + o, w, FRAME_COLOR);
    draw_vline(x + 1 + w + o, y + 1 + o, w, FRAME_COLOR);
    /* Front window, lower-right overlapping */
    draw_hline(x + 3 + o, y + 3 + o, w - 2, FRAME_COLOR);
    draw_vline(x + 3 + o, y + 3 + o, w - 2, FRAME_COLOR);
    draw_hline(x + 3 + o, y + 3 + w - 2 + o, w - 2, FRAME_COLOR);
    draw_vline(x + 3 + w - 2 + o, y + 3 + o, w - 2, FRAME_COLOR);
}

static void draw_caption_glyph_close(int x, int y, int size, int o) {
    /* A thick X: the Marlett close glyph is two crossing strokes. */
    for (int i = 0; i + 3 < size; i++) {
        FbFillRect(x + 3 + i + o, y + 2 + i + o, 2, 1, FRAME_COLOR);
        FbFillRect(x + size - 3 - i + o, y + 2 + i + o, 2, 1, FRAME_COLOR);
    }
}

static void draw_caption_button(int x, int y, int size, char glyph, int pressed) {
    int ox = pressed ? 1 : 0;
    int oy = pressed ? 1 : 0;
    FbFillRect(x, y, size, size, FACE_COLOR);
    if (pressed) draw_bevel(x, y, size, size, SHADOW_COLOR, HILIGHT_COLOR);
    else draw_bevel(x, y, size, size, HILIGHT_COLOR, SHADOW_COLOR);

    switch (glyph) {
    case '_': draw_caption_glyph_min(x, y, size, o);      break;
    case 'O': draw_caption_glyph_max(x, y, size, o);      break;
    case 'R': draw_caption_glyph_restore(x, y, size, o);  break;
    case 'X': draw_caption_glyph_close(x, y, size, o);    break;
    default: FbDrawChar(x + 3 + o, y + 3 + o, glyph, FRAME_COLOR, FACE_COLOR); break;
    }
}

/* True when the caption button is currently shown depressed (mouse is held
 * down on it).  Mirrors the depressed state WINE tracks while a caption
 * button is captured. */
static int caption_button_pressed(WINDOW *win, int hit) {
    return caption_press_win == win && caption_press_hit == hit;
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

/* Execute the action of a caption button after it has been pressed and
 * released.  Mirrors the SC_CLOSE/SC_MINIMIZE/SC_MAXIMIZE handling WINE
 * performs for HTCLOSE/HTMINBUTTON/HTMAXBUTTON in its DefWindowProc. */
static void do_caption_button_action(HANDLE hwnd, WINDOW *win, int hit) {
    switch (hit) {
    case CAPBTN_CLOSE:
        /* Give the application a normal WM_CLOSE first; destroying the kernel
         * window directly would leave the app's message loop alive. */
        if (win->wndProc) win->wndProc(hwnd, WM_CLOSE, 0, 0);
        if (ObReferenceObject(hwnd)) {
            ObDereferenceObject(hwnd);
            Win32kDestroyWindow(hwnd);
        }
        break;
    case CAPBTN_MAXIMIZE:
        if (win->minimized) {
            restore_window_if_needed(hwnd, win);
            maximize_window(win);
        } else if (win->maximized) {
            restore_window(win);
        } else {
            maximize_window(win);
        }
        break;
    case CAPBTN_MINIMIZE:
        if (win->minimized) {
            restore_window_if_needed(hwnd, win);
            set_window_active(hwnd);
        } else {
            minimize_window(hwnd, win);
            set_window_active(find_topmost_visible_window());
        }
        break;
    default:
        break;
    }
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
    WINDOW *candidate;

    if (hwnd != INVALID_HANDLE) {
        candidate = (WINDOW*)ObReferenceObject(hwnd);
        if (!candidate) return;
        if (candidate->desktop) {
            ObDereferenceObject(hwnd);
            return;
        }
        ObDereferenceObject(hwnd);
    }
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
    int target;
    WINDOW *candidate = (WINDOW*)ObReferenceObject(hwnd);
    if (!candidate) return;
    if (candidate->desktop) {
        ObDereferenceObject(hwnd);
        return;
    }
    target = window_is_topmost(candidate) ? window_count - 1 : 0;
    ObDereferenceObject(hwnd);
    for (int i = 0; i < window_count; i++) {
        if (window_list[i] == hwnd) {
            pos = i;
            break;
        }
    }
    if (pos < 0) return;
    if (target == 0) {
        target = window_count - 1;
        for (int i = 0; i < window_count; i++) {
            WINDOW *win = (WINDOW*)ObReferenceObject(window_list[i]);
            int topmost = window_is_topmost(win);
            if (win) ObDereferenceObject(window_list[i]);
            if (topmost) { target = i - (pos < i ? 1 : 0); break; }
        }
        if (target < 0) target = 0;
    }
    if (pos == target) return;
    if (pos < target) for (int i = pos; i < target; i++) window_list[i] = window_list[i + 1];
    else for (int i = pos; i > target; i--) window_list[i] = window_list[i - 1];
    window_list[target] = hwnd;
}

static int is_in_window(WINDOW *win, int x, int y) {
    return (x >= win->x && x < win->x + win->width &&
            y >= win->y && y < win->y + win->height);
}

static int is_menu_popup_window(const WINDOW *win) {
    return win && win->wndClass && strcmp(win->wndClass->className, "MenuPopup") == 0;
}

static int has_visible_menu_popup(void) {
    for (int i = 0; i < window_count; i++) {
        WINDOW *win = (WINDOW*)ObReferenceObject(window_list[i]);
        int visible = win && win->visible && is_menu_popup_window(win);
        if (win) ObDereferenceObject(window_list[i]);
        if (visible) return 1;
    }
    return 0;
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
        if (win && win->visible && !win->minimized && !win->desktop) {
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
    paint_desktop_area(0, 0, FbGetWidth(), FbGetHeight());

    for (int pass = 0; pass < 2; pass++) for (int i = 0; i < window_count; i++) {
        HANDLE hwnd = window_list[i];
        WINDOW *win;
        if (hwnd == skip_window) continue;

        win = (WINDOW*)ObReferenceObject(hwnd);
        if (win && win->visible && (is_menu_popup_window(win) == (pass == 1))) {
            draw_window_frame(win);
            if (win->wndProc && !win->minimized) win->wndProc(hwnd, WM_PAINT, 0, 0);
        }
        if (win) ObDereferenceObject(hwnd);
    }
}

static void begin_fast_drag(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    /* Activation may have changed the caption state. Establish it only in
       this window's damage rectangle before taking the snapshot. */
    MouseEraseCursor();
    FbSetClipRect(win->x, win->y, win->width, win->height);
    paint_desktop_area(win->x, win->y, win->width, win->height);
    draw_window_frame(win);
    if (win->wndProc && !win->minimized) win->wndProc(hwnd, WM_PAINT, 0, 0);
    draw_window_caption(win);
    FbResetClipRect();
    if (drag_pixels) kfree(drag_pixels);
    drag_pixels_width = win->width;
    drag_pixels_height = win->height;
    drag_pixels = (uint32_t*)kmalloc((uint32_t)win->width * (uint32_t)win->height * sizeof(uint32_t));
    if (drag_pixels) FbCaptureRGB(win->x, win->y, win->width, win->height, drag_pixels, win->width);
    MouseDrawCursor();
    FbSwapBuffers();
    ObDereferenceObject(hwnd);
}

static void fast_drag_present(WINDOW *win, int old_x, int old_y) {
    int left = old_x < win->x ? old_x : win->x;
    int top = old_y < win->y ? old_y : win->y;
    int right = old_x + win->width > win->x + win->width ? old_x + win->width : win->x + win->width;
    int bottom = old_y + win->height > win->y + win->height ? old_y + win->height : win->y + win->height;

    MouseEraseCursor();
    FbSetClipRect(left, top, right - left, bottom - top);
    paint_desktop_area(left, top, right - left, bottom - top);
    for (int pass = 0; pass < 2; pass++) for (int i = 0; i < window_count; i++) {
        WINDOW *other = (WINDOW*)ObReferenceObject(window_list[i]);
        if (other && other->visible && (is_menu_popup_window(other) == (pass == 1)) &&
            (window_list[i] == drag_window ||
             rects_intersect(other->x, other->y, other->width, other->height,
                             left, top, right - left, bottom - top))) {
            if (window_list[i] != drag_window || !drag_pixels) {
                draw_window_frame(other);
                if (other->wndProc && !other->minimized) other->wndProc(window_list[i], WM_PAINT, 0, 0);
            }
        }
        if (other) ObDereferenceObject(window_list[i]);
    }
    for (int i = 0; i < window_count; i++) {
        WINDOW *other = (WINDOW*)ObReferenceObject(window_list[i]);
        if (other && other->visible && (window_list[i] != drag_window || !drag_pixels) &&
            rects_intersect(other->x, other->y, other->width, other->height,
                            left, top, right - left, bottom - top))
            draw_window_caption(other);
        if (other) ObDereferenceObject(window_list[i]);
    }
    if (drag_pixels && drag_pixels_width == win->width && drag_pixels_height == win->height)
        FbBlitRGB(win->x, win->y, win->width, win->height, drag_pixels, drag_pixels_width);
    FbResetClipRect();
    MouseDrawCursor();
    FbSwapBuffers();
}

void Win32kInit(void *mb_info) {
    window_object_type = ObRegisterObjectType("Window", 0);
    FbInit(mb_info);
    paint_desktop_area(0, 0, FbGetWidth(), FbGetHeight());
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
    if (strcmp(wc->className, "Desktop") != 0 &&
        strcmp(wc->className, "Shell_TrayWnd") != 0) {
        if (w < 96) w = 96;
        if (h < 64) h = 64;
    }
    win->x = x; win->y = y;
    win->width = w; win->height = h;
    win->restore_x = x; win->restore_y = y;
    win->restore_width = w; win->restore_height = h;
    win->style = style;
    win->visible = (style & WS_VISIBLE) ? 1 : 0;
    win->active = 0;
    win->minimized = 0;
    win->maximized = 0;
    win->desktop = strcmp(wc->className, "Desktop") == 0;
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
        if (win->desktop) {
            int i;
            for (i = window_count; i > 0; i--) window_list[i] = window_list[i - 1];
            window_list[0] = hwnd;
            window_count++;
        } else {
            window_list[window_count++] = hwnd;
        }
    } else {
        SerialPutString("[Win32k] CreateWindowByClass window list full\r\n");
    }

    if (!win->desktop) {
        raise_window(hwnd);
        set_window_active(hwnd);
    }
    
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
        active_window = find_topmost_visible_window();
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
    if (win->desktop) Win32kRedrawAll();
    else draw_window_frame(win);
    ObDereferenceObject(hwnd);
}

void Win32kSetWindowShowState(HANDLE hwnd, int command) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    if (command == W32K_SW_HIDE) {
        win->visible = 0;
    } else if (command == W32K_SW_MINIMIZE || command == W32K_SW_SHOWMINIMIZED) {
        win->visible = 1;
        minimize_window(hwnd, win);
        ObDereferenceObject(hwnd);
        set_window_active(find_topmost_visible_window());
        Win32kRedrawAll();
        return;
    } else {
        win->visible = 1;
        if (command == W32K_SW_RESTORE || command == W32K_SW_SHOWNORMAL) restore_window_if_needed(hwnd, win);
        ObDereferenceObject(hwnd);
        Win32kActivateWindow(hwnd);
        Win32kRedrawAll();
        return;
    }
    ObDereferenceObject(hwnd);
    Win32kRedrawAll();
}

int Win32kIsWindowMinimized(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    int minimized = win ? win->minimized : 0;
    if (win) ObDereferenceObject(hwnd);
    return minimized;
}

void Win32kUpdateWindow(HANDLE hwnd) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    int top_level = win && (win->desktop || win->owner == INVALID_HANDLE || win->owner == 0);
    if (top_level && has_visible_menu_popup()) {
        /* USER32 may invalidate an owner immediately after opening a popup.
           Painting that owner directly writes over the shared framebuffer
           and leaves the popup invisible even though it remains hittable.
           Recompose the complete scene so front-layer windows are restored. */
        if (win) ObDereferenceObject(hwnd);
        Win32kRedrawAll();
        return;
    }
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
    WINDOW *win;
    if (hwnd == INVALID_HANDLE) return;

    win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;

    /* The desktop is an input surface, but never a foreground window.  It
       must remain at the bottom of the compositor and background clicks must
       not replace the active application. */
    if (!win->desktop && !(win->exstyle & WS_EX_NOACTIVATE)) {
        raise_window(hwnd);
        set_window_active(hwnd);
    }
    
    {
        int caption_hit = hit_caption_button(win, x, y);
        if (caption_hit != CAPBTN_NONE) {
            /* Register the press and act only once the button is released over
             * itself.  This gives the button a depressed visual state and
             * aborts the click when the pointer is dragged off, matching the
             * press/release tracking WINE does with track_min_max_box. */
            caption_press_hit = caption_hit;
            caption_press_win = win;
            caption_press_hwnd = hwnd;
            ObDereferenceObject(hwnd);
            Win32kRedrawAll();
            return;
        }
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
}

void Win32kHandleMouseUp(int x, int y, int button) {
    if (button != 1) return;

    /* Resolve a caption button press.  It fires only if the pointer is
     * released over the same button, mirroring WINE's track_min_max_box. */
    if (caption_press_hwnd != INVALID_HANDLE) {
        HANDLE hw = caption_press_hwnd;
        int hit = caption_press_hit;
        WINDOW *win = (WINDOW*)ObReferenceObject(hw);
        int fire = win && win->visible && hit_caption_button(win, x, y) == hit;

        caption_press_hit = CAPBTN_NONE;
        caption_press_win = 0;
        caption_press_hwnd = INVALID_HANDLE;

        if (fire) do_caption_button_action(hw, win, hit);
        if (win) ObDereferenceObject(hw);

        Win32kRedrawAll();
        return;
    }

    if (dragging) {
        SerialPutString("[Win32k] Drag end\r\n");
        dragging = 0;
        drag_window = INVALID_HANDLE;
        if (drag_pixels) kfree(drag_pixels);
        drag_pixels = 0;
        drag_pixels_width = 0;
        drag_pixels_height = 0;
    }
    if (resizing) {
        resizing = 0;
        resize_window = INVALID_HANDLE;
        resize_edge = RESIZE_NONE;
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

        update_cursor_for_point(x, y);
        /* Resizing already has a bounded damage path; never repaint the
           desktop for each resize sample. */
        {
            int left = resize_start_x < win->x ? resize_start_x : win->x;
            int top = resize_start_y < win->y ? resize_start_y : win->y;
            int right = resize_start_x + resize_start_width > win->x + win->width ?
                        resize_start_x + resize_start_width : win->x + win->width;
            int bottom = resize_start_y + resize_start_height > win->y + win->height ?
                         resize_start_y + resize_start_height : win->y + win->height;
            MouseEraseCursor();
            FbSetClipRect(left, top, right - left, bottom - top);
            paint_desktop_area(left, top, right - left, bottom - top);
            for (int pass = 0; pass < 2; pass++) for (int i = 0; i < window_count; i++) {
                WINDOW *other = (WINDOW*)ObReferenceObject(window_list[i]);
                if (other && other->visible && (is_menu_popup_window(other) == (pass == 1)) &&
                    rects_intersect(other->x, other->y,
                    other->width, other->height, left, top, right - left, bottom - top)) {
                    draw_window_frame(other);
                    if (other->wndProc && !other->minimized) other->wndProc(window_list[i], WM_PAINT, 0, 0);
                }
                if (other) ObDereferenceObject(window_list[i]);
            }
            for (int i = 0; i < window_count; i++) {
                WINDOW *other = (WINDOW*)ObReferenceObject(window_list[i]);
                if (other && other->visible && rects_intersect(other->x, other->y,
                    other->width, other->height, left, top, right - left, bottom - top))
                    draw_window_caption(other);
                if (other) ObDereferenceObject(window_list[i]);
            }
            FbResetClipRect();
            MouseDrawCursor();
            FbSwapBuffers();
        }
        ObDereferenceObject(resize_window);
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

int Win32kGetScreenWidth(void) {
    int width = FbGetWidth();
    return width > 0 ? width : 800;
}

int Win32kGetScreenHeight(void) {
    int height = FbGetHeight();
    return height > 0 ? height : 600;
}

void Win32kActivateWindow(HANDLE hwnd) {
    WINDOW *win;
    if (hwnd == INVALID_HANDLE) return;
    win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    if (win->desktop && active_window != INVALID_HANDLE && active_window != hwnd) {
        ObDereferenceObject(hwnd);
        return;
    }
    ObDereferenceObject(hwnd);
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

void Win32kSetWindowRect(HANDLE hwnd, int x, int y, int width, int height) {
    WINDOW *win = (WINDOW*)ObReferenceObject(hwnd);
    if (!win) return;
    if (width > 0 && height > 0) {
        win->x = x;
        win->y = y;
        win->width = width;
        win->height = height;
        if (!win->maximized) {
            win->restore_x = x;
            win->restore_y = y;
            win->restore_width = width;
            win->restore_height = height;
        }
    }
    ObDereferenceObject(hwnd);
}

void Win32kRedrawAll(void) {
    if (redraw_in_progress) return;
    redraw_in_progress = 1;
    MouseEraseCursor();

    paint_desktop_area(0, 0, FbGetWidth(), FbGetHeight());

    for (int pass = 0; pass < 2; pass++) for (int i = 0; i < window_count; i++) {
        WINDOW *win = (WINDOW*)ObReferenceObject(window_list[i]);
        if (win && win->visible && (is_menu_popup_window(win) == (pass == 1))) {
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
