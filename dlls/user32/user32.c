#include <stdint.h>
#include <stdarg.h>
#include "windows.h"
#include "ntuser.h"
#include "commctrl.h"
#include "icon.h"
#include "discount_dialog.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void *memset(void *dest, int c, uint32_t n);
extern void *memcpy(void *dest, const void *src, uint32_t n);
extern uint32_t strlen(const char *s);
extern void KeYield(void);
extern uint32_t GetTickCount(void);
extern uint32_t GetCurrentProcessId(void);
extern void SerialPutString(const char *str);
extern void CsrssShutdownSystem(void);
extern int CdfsReadFile(const char *path, uint8_t **out_buffer, uint32_t *out_size);
extern const char *PeGetImagePath(void *image_base);
extern void *Win32kRegisterClass(const char *className, uint32_t style, void (*wndProc)(void *, uint32_t, uint32_t, uint32_t));
extern void *Win32kCreateWindow(const char *className, const char *title, int x, int y, int w, int h, uint32_t style);
extern void Win32kShowWindow(void *hwnd);
extern void Win32kSetWindowShowState(void *hwnd, int command);
extern int Win32kIsWindowMinimized(void *hwnd);
extern void Win32kUpdateWindow(void *hwnd);
extern void Win32kGetWindowRect(void *hwnd, LPRECT lpRect);
extern void Win32kDestroyWindow(void *hwnd);
extern void Win32kActivateWindow(void *hwnd);
extern int Win32kGetScreenWidth(void);
extern int Win32kGetScreenHeight(void);
extern void Win32kSetWindowRect(void *hwnd, int x, int y, int width, int height);
extern void *Win32kGetActiveWindow(void);
extern void Win32kSetWindowIcons(void *hwnd, HANDLE big_icon, HANDLE small_icon);
extern void Win32kRedrawAll(void);
extern void Win32kRefreshCursor(void);
extern HDC GdiCreateScreenDC(HWND hwnd);
extern HDC GdiCreateScreenDCEx(HWND hwnd, int origin_x, int origin_y, int width, int height);
extern void GdiDestroyScreenDC(HDC hdc);
extern BOOL GdiDestroyIcon(HICON hIcon);

static const uint8_t *u32_get_embedded_blob(HINSTANCE hInstance, const char *section_name, uint32_t *out_size);

#define MAX_U32_CLASSES 64
#define MAX_U32_WINDOWS 256
#define MAX_U32_TIMERS 64
#define MAX_U32_MENUS 64
#define MAX_U32_MESSAGES 256
#define MAX_U32_ICONS 64
#define MAX_U32_QUIT_STATES 32
#define MAX_LV_COLUMNS 32
#define MAX_LV_ITEMS 256
#define MAX_TAB_ITEMS 16
#define U32_FRAME_THICKNESS 2
#define U32_EDGE_THICKNESS 1
#define U32_TITLEBAR_HEIGHT 18
#define U32_MENU_HEIGHT 18

typedef struct _U32_CLASS {
    int used;
    WCHAR name[64];
    WCHAR menu_name[64];
    UINT menu_res_id;
    WNDPROC proc;
    UINT style;
    HICON hIcon;
    HICON hIconSm;
    HCURSOR hCursor;
    HBRUSH hbrBackground;
    HINSTANCE hInstance;
    HANDLE win32k_class;
} U32_CLASS;

typedef struct _U32_MENU_ITEM {
    UINT id;
    UINT flags;
    WCHAR text[64];
    HMENU submenu;
    ULONG_PTR item_data;
    HBITMAP bitmap;
} U32_MENU_ITEM;

typedef struct _U32_MENU {
    int used;
    HMENU handle;
    int count;
    int default_item;
    DWORD style;
    ULONG_PTR menu_data;
    U32_MENU_ITEM items[32];
} U32_MENU;

typedef struct _U32_WINDOW {
    int used;
    HWND hwnd;
    HWND parent;
    HWND owner;
    int top_level;
    int dialog;
    int visible;
    int enabled;
    int ended;
    INT_PTR dialog_result;
    UINT id;
    DWORD style;
    DWORD exstyle;
    RECT rect;
    WCHAR title[128];
    U32_CLASS *klass;
    WNDPROC proc;
    DLGPROC dlgproc;
    LONG_PTR user_data;
    HMENU menu;
    HMENU popup_menu;
    HWND header_hwnd;
    HWND menu_owner;
    int active_menu_index;
    int hot_index;
    int check_state;
    int tab_cur_sel;
    int tab_count;
    WCHAR tab_text[MAX_TAB_ITEMS][64];
    int listview_item_count;
    int listview_selected_count;
    int header_count;
    int status_parts_count;
    int focused;
    int pressed;
    int ctrl_type;
    int invalidated;
    int painting;
    int edit_limit;
    int edit_capacity;
    int edit_len;
    int edit_sel_start;
    int edit_sel_end;
    int edit_modified;
    int status_part_right[8];
    WCHAR status_text[8][64];
    void *listview_data;
    WCHAR *edit_text;
    HFONT hFont;
    HICON hIcon;
    HICON hIconSm;
    DWORD owner_pid;
    int scroll_min[2];
    int scroll_max[2];
    UINT scroll_page[2];
    int scroll_pos[2];
    int scroll_track[2];
    int scroll_visible[2];
    int scroll_drag_bar;
    int scroll_drag_origin;
    int scroll_drag_start_pos;
} U32_WINDOW;

#define U32_SCROLLBAR_SIZE 14

static void u32_mark_invalid(HWND hwnd);
static int u32_effectively_visible(HWND hwnd);

static int u32_scroll_bar_index(int bar) {
    return bar == SB_HORZ ? 0 : 1;
}

static int u32_scroll_limit(U32_WINDOW *win, int bar) {
    int i = u32_scroll_bar_index(bar);
    int page = (int)win->scroll_page[i];
    int limit = win->scroll_max[i] - (page > 0 ? page - 1 : 0);
    return limit < win->scroll_min[i] ? win->scroll_min[i] : limit;
}

static void u32_scroll_clamp(U32_WINDOW *win, int bar) {
    int i = u32_scroll_bar_index(bar);
    int limit = u32_scroll_limit(win, bar);
    if (win->scroll_pos[i] < win->scroll_min[i]) win->scroll_pos[i] = win->scroll_min[i];
    if (win->scroll_pos[i] > limit) win->scroll_pos[i] = limit;
    win->scroll_track[i] = win->scroll_pos[i];
}

static void u32_scroll_init(U32_WINDOW *win) {
    win->scroll_min[0] = win->scroll_min[1] = 0;
    win->scroll_max[0] = win->scroll_max[1] = 1;
    win->scroll_page[0] = win->scroll_page[1] = 1;
    win->scroll_pos[0] = win->scroll_pos[1] = 0;
    win->scroll_track[0] = win->scroll_track[1] = 0;
    win->scroll_visible[0] = (win->style & WS_HSCROLL) != 0;
    win->scroll_visible[1] = (win->style & WS_VSCROLL) != 0;
}

static int u32_scroll_geometry(U32_WINDOW *win, RECT *rc, int bar, int *start, int *length) {
    int i = u32_scroll_bar_index(bar);
    if (!win->scroll_visible[i]) return 0;
    if (bar == SB_VERT) {
        *start = 0;
        *length = rc->bottom - (win->scroll_visible[0] ? U32_SCROLLBAR_SIZE : 0);
    } else {
        *start = 0;
        *length = rc->right - (win->scroll_visible[1] ? U32_SCROLLBAR_SIZE : 0);
    }
    return *length > U32_SCROLLBAR_SIZE * 2;
}

static void u32_scroll_arrow(HDC hdc, int x, int y, int bar, int down) {
    int cx = x + U32_SCROLLBAR_SIZE / 2;
    int cy = y + U32_SCROLLBAR_SIZE / 2;
    MoveToEx(hdc, bar == SB_VERT ? cx - 3 : (down ? cx + 3 : cx - 3),
             bar == SB_VERT ? (down ? cy + 3 : cy - 3) : cy - 3, 0);
    LineTo(hdc, bar == SB_VERT ? cx : (down ? cx + 3 : cx - 3),
           bar == SB_VERT ? (down ? cy - 2 : cy + 2) : cy);
    LineTo(hdc, bar == SB_VERT ? cx + 3 : (down ? cx + 3 : cx - 3),
           bar == SB_VERT ? (down ? cy + 3 : cy - 3) : cy + 3);
}

static void u32_draw_scrollbar(HDC hdc, U32_WINDOW *win, RECT *rc, int bar) {
    int i = u32_scroll_bar_index(bar), start, length, track, thumb, travel, pos;
    RECT r;
    if (!u32_scroll_geometry(win, rc, bar, &start, &length)) return;
    if (bar == SB_VERT) {
        r.left = rc->right - U32_SCROLLBAR_SIZE; r.right = rc->right;
        r.top = 0; r.bottom = length;
    } else {
        r.left = 0; r.right = length;
        r.top = rc->bottom - U32_SCROLLBAR_SIZE; r.bottom = rc->bottom;
    }
    FillRect(hdc, &r, (HBRUSH)GetStockObject(0));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
    if (bar == SB_VERT) {
        Rectangle(hdc, r.left, r.top, r.right, r.top + U32_SCROLLBAR_SIZE);
        Rectangle(hdc, r.left, r.bottom - U32_SCROLLBAR_SIZE, r.right, r.bottom);
        u32_scroll_arrow(hdc, r.left, r.top, bar, 0);
        u32_scroll_arrow(hdc, r.left, r.bottom - U32_SCROLLBAR_SIZE, bar, 1);
    } else {
        Rectangle(hdc, r.left, r.top, r.left + U32_SCROLLBAR_SIZE, r.bottom);
        Rectangle(hdc, r.right - U32_SCROLLBAR_SIZE, r.top, r.right, r.bottom);
        u32_scroll_arrow(hdc, r.left, r.top, bar, 0);
        u32_scroll_arrow(hdc, r.right - U32_SCROLLBAR_SIZE, r.top, bar, 1);
    }
    track = length - U32_SCROLLBAR_SIZE * 2;
    thumb = ((int)win->scroll_page[i] * track) /
            (win->scroll_max[i] - win->scroll_min[i] + 1);
    if (thumb < 8) thumb = 8;
    if (thumb > track) thumb = track;
    travel = track - thumb;
    pos = u32_scroll_limit(win, bar) - win->scroll_min[i];
    if (pos < 0) pos = 0;
    if (bar == SB_VERT) {
        int y = r.top + U32_SCROLLBAR_SIZE +
                ((win->scroll_max[i] - win->scroll_min[i] > 0) ?
                 pos * travel / (win->scroll_max[i] - win->scroll_min[i]) : 0);
        r.top = y; r.bottom = y + thumb;
    } else {
        int x = r.left + U32_SCROLLBAR_SIZE +
                ((win->scroll_max[i] - win->scroll_min[i] > 0) ?
                 pos * travel / (win->scroll_max[i] - win->scroll_min[i]) : 0);
        r.left = x; r.right = x + thumb;
    }
    FillRect(hdc, &r, (HBRUSH)GetStockObject(7));
    Rectangle(hdc, r.left, r.top, r.right, r.bottom);
}

static void u32_draw_scrollbars(HDC hdc, U32_WINDOW *win, RECT *rc) {
    u32_draw_scrollbar(hdc, win, rc, SB_VERT);
    u32_draw_scrollbar(hdc, win, rc, SB_HORZ);
}

static int u32_scroll_hit(U32_WINDOW *win, int x, int y, int *bar, int *code) {
    RECT rc;
    int start, length, i, track, thumb, travel, p, thumb_start;
    if (!GetClientRect(win->hwnd, &rc)) return 0;
    if (win->scroll_visible[1] && x >= rc.right - U32_SCROLLBAR_SIZE &&
        u32_scroll_geometry(win, &rc, SB_VERT, &start, &length)) {
        track = length - U32_SCROLLBAR_SIZE * 2;
        i = 1; thumb = ((int)win->scroll_page[i] * track) /
            (win->scroll_max[i] - win->scroll_min[i] + 1);
        if (thumb < 8) thumb = 8; if (thumb > track) thumb = track;
        travel = track - thumb;
        p = u32_scroll_limit(win, SB_VERT) - win->scroll_min[i];
        thumb_start = U32_SCROLLBAR_SIZE + (travel > 0 ? p * travel / (win->scroll_max[i] - win->scroll_min[i]) : 0);
        *bar = SB_VERT;
        if (y < U32_SCROLLBAR_SIZE) *code = SB_LINEUP;
        else if (y >= length - U32_SCROLLBAR_SIZE) *code = SB_LINEDOWN;
        else if (y < thumb_start) *code = SB_PAGEUP;
        else if (y >= thumb_start + thumb) *code = SB_PAGEDOWN;
        else *code = SB_THUMBTRACK;
        return 1;
    }
    if (win->scroll_visible[0] && y >= rc.bottom - U32_SCROLLBAR_SIZE &&
        u32_scroll_geometry(win, &rc, SB_HORZ, &start, &length)) {
        track = length - U32_SCROLLBAR_SIZE * 2;
        i = 0; thumb = ((int)win->scroll_page[i] * track) /
            (win->scroll_max[i] - win->scroll_min[i] + 1);
        if (thumb < 8) thumb = 8; if (thumb > track) thumb = track;
        travel = track - thumb;
        p = u32_scroll_limit(win, SB_HORZ) - win->scroll_min[i];
        thumb_start = U32_SCROLLBAR_SIZE + (travel > 0 ? p * travel / (win->scroll_max[i] - win->scroll_min[i]) : 0);
        *bar = SB_HORZ;
        if (x < U32_SCROLLBAR_SIZE) *code = SB_LINELEFT;
        else if (x >= length - U32_SCROLLBAR_SIZE) *code = SB_LINERIGHT;
        else if (x < thumb_start) *code = SB_PAGELEFT;
        else if (x >= thumb_start + thumb) *code = SB_PAGERIGHT;
        else *code = SB_THUMBTRACK;
        return 1;
    }
    return 0;
}

static void u32_scroll_command(U32_WINDOW *win, int bar, int code) {
    int i = u32_scroll_bar_index(bar), delta = (int)win->scroll_page[i];
    if (code == SB_LINEUP || code == SB_LINELEFT) win->scroll_pos[i]--;
    else if (code == SB_LINEDOWN || code == SB_LINERIGHT) win->scroll_pos[i]++;
    else if (code == SB_PAGEUP || code == SB_PAGELEFT) win->scroll_pos[i] -= delta;
    else if (code == SB_PAGEDOWN || code == SB_PAGERIGHT) win->scroll_pos[i] += delta;
    else if (code == SB_TOP || code == SB_LEFT) win->scroll_pos[i] = win->scroll_min[i];
    else if (code == SB_BOTTOM || code == SB_RIGHT) win->scroll_pos[i] = u32_scroll_limit(win, bar);
    u32_scroll_clamp(win, bar);
    SendMessageW(win->hwnd, bar == SB_VERT ? WM_VSCROLL : WM_HSCROLL,
                 MAKEWPARAM(code, win->scroll_pos[i]), 0);
    u32_mark_invalid(win->hwnd);
}

typedef struct _U32_LISTVIEW_DATA {
    WCHAR columns[MAX_LV_COLUMNS][64];
    WCHAR items[MAX_LV_ITEMS][MAX_LV_COLUMNS][64];
} U32_LISTVIEW_DATA;

typedef struct _U32_TIMER {
    int used;
    HWND hwnd;
    UINT_PTR id;
    UINT elapse;
    uint32_t tick;
} U32_TIMER;

typedef struct _U32_MESSAGE {
    int used;
    MSG msg;
    DWORD owner_pid;
} U32_MESSAGE;

typedef struct _U32_ICON {
    int used;
    UINT resource_id;
    int width;
    int height;
    DISCOUNT_ICON icon;
} U32_ICON;

typedef struct _U32_QUIT_STATE {
    int used;
    DWORD owner_pid;
    int exit_requested;
    int exit_code;
} U32_QUIT_STATE;

typedef struct _U32_TEMPLATE_CONTROL {
    LPCWSTR class_name;
    LPCWSTR title;
    DWORD style;
    DWORD exstyle;
    UINT id;
    int x;
    int y;
    int w;
    int h;
} U32_TEMPLATE_CONTROL;

typedef struct _U32_DIALOG_TEMPLATE_DEF {
    UINT tmpl_id;
    LPCWSTR caption;
    DWORD style;
    int width;
    int height;
    int attach_to_tab_of_parent;
    const U32_TEMPLATE_CONTROL *controls;
    int control_count;
} U32_DIALOG_TEMPLATE_DEF;

static U32_CLASS g_classes[MAX_U32_CLASSES];
static U32_WINDOW g_windows[MAX_U32_WINDOWS];
static U32_TIMER g_timers[MAX_U32_TIMERS];
static U32_MENU g_menus[MAX_U32_MENUS];
static U32_MESSAGE g_messages[MAX_U32_MESSAGES];
static U32_ICON g_icons[MAX_U32_ICONS];
static U32_QUIT_STATE g_quit_states[MAX_U32_QUIT_STATES];
static U32_DIALOG_TEMPLATE_DEF g_registered_dialog;
static int g_registered_dialog_used;
static HWND g_focus = NULL;
static HWND g_active_window = NULL;
static HWND g_mouse_capture_window = NULL;
static DWORD g_user32_process_id = 1;

static DWORD u32_current_process_id(void) {
    return g_user32_process_id ? g_user32_process_id : GetCurrentProcessId();
}
static HWND g_open_menu_popup = NULL;

static void u32_paint_children(HWND hwnd);
static void u32_mark_invalid(HWND hwnd);
static void u32_mark_invalid_descendants(HWND hwnd);
static void u32_clear_invalid(HWND hwnd);
BOOL PostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
static int u32_is_int_resource(LPCWSTR ptr);
static UINT u32_resource_id(LPCWSTR ptr);

#ifndef BN_CLICKED
#define BN_CLICKED 0
#endif

#define U32_CTRL_GENERIC   0
#define U32_CTRL_DIALOG    1
#define U32_CTRL_TAB       2
#define U32_CTRL_LISTVIEW  3
#define U32_CTRL_BUTTON    4
#define U32_CTRL_STATIC    5
#define U32_CTRL_EDIT      6
#define U32_CTRL_GROUPBOX  7
#define U32_CTRL_STATUS    8
#define U32_CTRL_MENUPOPUP 9
#define U32_CTRL_COMBO     10

#ifndef BS_GROUPBOX
#define BS_GROUPBOX        0x00000007L
#endif
#ifndef BS_AUTOCHECKBOX
#define BS_AUTOCHECKBOX    0x00000003L
#endif
#ifndef LVS_SHOWSELALWAYS
#define LVS_SHOWSELALWAYS  0x0008
#endif
#ifndef LVS_SINGLESEL
#define LVS_SINGLESEL      0x0004
#endif
#ifndef LVS_OWNERDATA
#define LVS_OWNERDATA      0x1000
#endif
#ifndef SIZE_RESTORED
#define SIZE_RESTORED      0
#endif

static int u32_wstrlen(LPCWSTR s) {
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static void u32_wstrcpy(WCHAR *dst, LPCWSTR src, int max_chars) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void u32_wmemcpy(WCHAR *dst, const WCHAR *src, int count) {
    int i;
    if (!dst || !src || count <= 0) return;
    for (i = 0; i < count; i++) dst[i] = src[i];
}

static int u32_wstrcmp(LPCWSTR a, LPCWSTR b) {
    int i = 0;
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return (a[i] < b[i]) ? -1 : 1;
        i++;
    }
    if (a[i] == b[i]) return 0;
    return a[i] ? 1 : -1;
}

static WCHAR *u32_ensure_edit_buffer(U32_WINDOW *win, int min_chars) {
    WCHAR *newbuf;
    int alloc_chars;
    if (!win) return NULL;
    if (win->edit_limit <= 0) win->edit_limit = 65535;
    if (min_chars < 64) min_chars = 64;
    /* Do not allocate the edit limit up front.  A normal Win32 edit may
     * have a 64K limit, but most controls contain only a few characters.
     * The old code allocated 64K WCHARs for every empty edit, exhausting
     * the small kernel heap when applications created several controls. */
    if (win->edit_text && win->edit_capacity >= min_chars) return win->edit_text;
    alloc_chars = win->edit_capacity > 0 ? win->edit_capacity * 2 : 64;
    if (alloc_chars < min_chars) alloc_chars = min_chars;
    if (alloc_chars > win->edit_limit + 1) alloc_chars = win->edit_limit + 1;
    if (alloc_chars <= 0) return win->edit_text;
    newbuf = (WCHAR*)kmalloc((uint32_t)(alloc_chars * sizeof(WCHAR)));
    if (!newbuf) return win->edit_text;
    memset(newbuf, 0, (uint32_t)(alloc_chars * sizeof(WCHAR)));
    if (win->edit_text && win->edit_len > 0) {
        u32_wmemcpy(newbuf, win->edit_text, win->edit_len);
        kfree(win->edit_text);
    }
    win->edit_text = newbuf;
    win->edit_capacity = alloc_chars;
    return win->edit_text;
}

static void u32_edit_sync_title(U32_WINDOW *win) {
    int copy_len;
    if (!win || !win->edit_text) return;
    copy_len = win->edit_len;
    if (copy_len > 127) copy_len = 127;
    if (copy_len > 0) u32_wmemcpy(win->title, win->edit_text, copy_len);
    win->title[copy_len] = 0;
}

static void u32_edit_set_text(U32_WINDOW *win, LPCWSTR text) {
    int len;
    if (!win) return;
    len = u32_wstrlen(text);
    if (win->edit_limit > 0 && len > win->edit_limit) len = win->edit_limit;
    if (!u32_ensure_edit_buffer(win, len + 1)) return;
    if (len > 0) u32_wmemcpy(win->edit_text, text, len);
    win->edit_text[len] = 0;
    win->edit_len = len;
    win->edit_sel_start = len;
    win->edit_sel_end = len;
    win->edit_modified = 0;
    u32_edit_sync_title(win);
}

static void u32_edit_replace_sel(U32_WINDOW *win, LPCWSTR repl) {
    int start, end, repl_len, new_len, tail_len;
    if (!win) return;
    if (win->ctrl_type != U32_CTRL_EDIT) return;
    if (!repl) repl = L"";
    if (!u32_ensure_edit_buffer(win, 64)) return;
    start = win->edit_sel_start;
    end = win->edit_sel_end;
    if (start < 0) start = 0;
    if (end < start) end = start;
    if (end > win->edit_len) end = win->edit_len;
    repl_len = u32_wstrlen(repl);
    new_len = start + repl_len + (win->edit_len - end);
    if (win->edit_limit > 0 && new_len > win->edit_limit) {
        repl_len -= (new_len - win->edit_limit);
        if (repl_len < 0) repl_len = 0;
        new_len = start + repl_len + (win->edit_len - end);
    }
    if (!u32_ensure_edit_buffer(win, new_len + 1)) return;
    tail_len = win->edit_len - end;
    if (tail_len > 0 && start + repl_len != end) {
        int i;
        if (start + repl_len < end) {
            for (i = 0; i <= tail_len; i++) {
                win->edit_text[start + repl_len + i] = win->edit_text[end + i];
            }
        } else {
            for (i = tail_len; i >= 0; i--) {
                win->edit_text[start + repl_len + i] = win->edit_text[end + i];
            }
        }
    } else {
        win->edit_text[new_len] = 0;
    }
    if (repl_len > 0) u32_wmemcpy(win->edit_text + start, repl, repl_len);
    win->edit_len = new_len;
    win->edit_text[new_len] = 0;
    win->edit_sel_start = start + repl_len;
    win->edit_sel_end = start + repl_len;
    win->edit_modified = 1;
    u32_edit_sync_title(win);
    u32_mark_invalid(win->hwnd);
}

static void u32_update_scroll_content(U32_WINDOW *win) {
    RECT rc;
    int i, lines = 1, columns = 0, current = 0;
    if (!win || !GetClientRect(win->hwnd, &rc)) return;
    if (win->ctrl_type == U32_CTRL_EDIT && win->edit_text) {
        for (i = 0; i < win->edit_len; i++) {
            if (win->edit_text[i] == L'\n') {
                lines++; if (current > columns) columns = current; current = 0;
            } else current++;
        }
        if (current > columns) columns = current;
        win->scroll_max[1] = lines * 10;
        win->scroll_page[1] = rc.bottom > 0 ? (UINT)rc.bottom : 1;
        win->scroll_max[0] = columns * 8;
        win->scroll_page[0] = rc.right > 0 ? (UINT)rc.right : 1;
        u32_scroll_clamp(win, SB_VERT); u32_scroll_clamp(win, SB_HORZ);
    } else if (win->ctrl_type == U32_CTRL_LISTVIEW) {
        win->scroll_max[1] = 18 + win->listview_item_count * 14;
        win->scroll_page[1] = rc.bottom > 0 ? (UINT)rc.bottom : 1;
        u32_scroll_clamp(win, SB_VERT);
    }
}

static void u32_wide_to_ansi(LPCWSTR src, char *dst, int max_chars) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        WCHAR ch = src[i];
        dst[i] = (char)((ch >= 32 && ch < 127) ? ch : '?');
        i++;
    }
    dst[i] = 0;
}

static void u32_ansi_to_wide(LPCSTR src, WCHAR *dst, int max_chars) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        dst[i] = (unsigned char)src[i];
        i++;
    }
    dst[i] = 0;
}

static WCHAR u32_towupper(WCHAR ch) {
    if (ch >= L'a' && ch <= L'z') return ch - (L'a' - L'A');
    return ch;
}

static char u32_toupper_char(char ch) {
    return (ch >= 'a' && ch <= 'z') ? (ch - ('a' - 'A')) : ch;
}

static void u32_uppercase_copy(char *dst, const char *src, int max_chars) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        dst[i] = u32_toupper_char(src[i]);
        i++;
    }
    dst[i] = 0;
}

static int u32_append_number(WCHAR *dst, int pos, int max, int value, int unsig) {
    WCHAR temp[16];
    unsigned v;
    int count = 0;
    if (!dst || max <= 0) return pos;
    if (!unsig && value < 0) {
        if (pos < max - 1) dst[pos++] = L'-';
        v = (unsigned)(-value);
    } else {
        v = (unsigned)value;
    }
    if (v == 0) {
        if (pos < max - 1) dst[pos++] = L'0';
        return pos;
    }
    while (v && count < (int)ARRAY_SIZE(temp)) {
        temp[count++] = (WCHAR)(L'0' + (v % 10));
        v /= 10;
    }
    while (count--) {
        if (pos < max - 1) dst[pos++] = temp[count];
    }
    return pos;
}

static int u32_vsnprintfw(LPWSTR out, int max, LPCWSTR fmt, va_list ap) {
    int pos = 0;
    int i = 0;
    if (!out || max <= 0) return 0;
    if (!fmt) {
        out[0] = 0;
        return 0;
    }
    while (fmt[i] && pos < max - 1) {
        if (fmt[i] != L'%') {
            out[pos++] = fmt[i++];
            continue;
        }
        i++;
        if (fmt[i] == L'%') {
            out[pos++] = fmt[i++];
            continue;
        }
        while (fmt[i] >= L'0' && fmt[i] <= L'9') i++;
        switch (fmt[i]) {
        case L'd':
        case L'i':
            pos = u32_append_number(out, pos, max, va_arg(ap, int), 0);
            i++;
            break;
        case L'u':
            pos = u32_append_number(out, pos, max, (int)va_arg(ap, unsigned), 1);
            i++;
            break;
        case L's':
            {
                LPCWSTR s = va_arg(ap, LPCWSTR);
                if (!s) s = L"";
                while (*s && pos < max - 1) out[pos++] = *s++;
                i++;
            }
            break;
        default:
            out[pos++] = L'%';
            if (fmt[i] && pos < max - 1) out[pos++] = fmt[i++];
            break;
        }
    }
    out[pos] = 0;
    return pos;
}

static U32_CLASS *u32_find_class(LPCWSTR name) {
    int i;
    if (!name) return NULL;
    if (u32_is_int_resource(name)) {
        UINT atom = u32_resource_id(name);
        /* Atoms returned by RegisterClass are one-based slots in our local
           class table.  Predefined system atoms are materialised by
           CreateWindowExW before reaching this lookup. */
        if (atom >= 1 && atom <= MAX_U32_CLASSES && g_classes[atom - 1].used)
            return &g_classes[atom - 1];
        return NULL;
    }
    for (i = 0; i < MAX_U32_CLASSES; i++) {
        if (g_classes[i].used && u32_wstrcmp(g_classes[i].name, name) == 0) return &g_classes[i];
    }
    return NULL;
}

static U32_WINDOW *u32_lookup_window(HWND hwnd) {
    int i;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].hwnd == hwnd) return &g_windows[i];
    }
    return NULL;
}

static U32_QUIT_STATE *u32_get_quit_state(DWORD pid, int create) {
    int i;
    U32_QUIT_STATE *free_slot = NULL;
    for (i = 0; i < MAX_U32_QUIT_STATES; i++) {
        if (g_quit_states[i].used && g_quit_states[i].owner_pid == pid) return &g_quit_states[i];
        if (!g_quit_states[i].used && !free_slot) free_slot = &g_quit_states[i];
    }
    if (!create || !free_slot) return NULL;
    memset(free_slot, 0, sizeof(*free_slot));
    free_slot->used = 1;
    free_slot->owner_pid = pid;
    return free_slot;
}

static U32_WINDOW *u32_alloc_window(void) {
    int i;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (!g_windows[i].used) {
            memset(&g_windows[i], 0, sizeof(g_windows[i]));
            g_windows[i].used = 1;
            g_windows[i].enabled = 1;
            return &g_windows[i];
        }
    }
    return NULL;
}

static U32_LISTVIEW_DATA *u32_ensure_listview(U32_WINDOW *win) {
    U32_LISTVIEW_DATA *data;
    if (!win) return NULL;
    data = (U32_LISTVIEW_DATA*)win->listview_data;
    if (data) return data;
    data = (U32_LISTVIEW_DATA*)kmalloc(sizeof(U32_LISTVIEW_DATA));
    if (!data) return NULL;
    memset(data, 0, sizeof(*data));
    win->listview_data = data;
    return data;
}

static U32_MENU *u32_lookup_menu(HMENU hmenu) {
    int i;
    for (i = 0; i < MAX_U32_MENUS; i++) {
        if (g_menus[i].used && g_menus[i].handle == hmenu) return &g_menus[i];
    }
    return NULL;
}

static U32_MENU *u32_alloc_menu(void) {
    int i;
    for (i = 0; i < MAX_U32_MENUS; i++) {
        if (!g_menus[i].used) {
            memset(&g_menus[i], 0, sizeof(g_menus[i]));
            g_menus[i].used = 1;
            g_menus[i].handle = (HMENU)&g_menus[i];
            g_menus[i].default_item = -1;
            return &g_menus[i];
        }
    }
    return NULL;
}

static void u32_set_rect(U32_WINDOW *win, int x, int y, int w, int h) {
    if (!win) return;
    win->rect.left = x;
    win->rect.top = y;
    win->rect.right = x + w;
    win->rect.bottom = y + h;
}

static int u32_client_offset_x(const U32_WINDOW *win) {
    (void)win;
    return U32_FRAME_THICKNESS + U32_EDGE_THICKNESS;
}

static int u32_client_offset_y(const U32_WINDOW *win) {
    if (!win) return 0;
    if (win->style & WS_CAPTION) {
        int y = U32_FRAME_THICKNESS + U32_TITLEBAR_HEIGHT + U32_EDGE_THICKNESS;
        if (win->top_level && win->menu) y += U32_MENU_HEIGHT;
        return y;
    }
    return U32_FRAME_THICKNESS;
}

static void u32_sync_top_level(U32_WINDOW *win) {
    RECT rc;
    int moved;
    int sized;
    if (!win || !win->top_level || !win->hwnd) return;
    Win32kGetWindowRect((HANDLE)win->hwnd, &rc);
    moved = (win->rect.left != rc.left) || (win->rect.top != rc.top);
    sized = ((win->rect.right - win->rect.left) != (rc.right - rc.left)) ||
            ((win->rect.bottom - win->rect.top) != (rc.bottom - rc.top));
    win->rect = rc;
    if (moved || sized) {
        u32_mark_invalid_descendants(win->hwnd);
        u32_mark_invalid(win->hwnd);
    }
}

static HWND u32_get_root_window(HWND hwnd) {
    U32_WINDOW *win = u32_lookup_window(hwnd);
    while (win && win->parent) win = u32_lookup_window(win->parent);
    return win ? win->hwnd : hwnd;
}

static int u32_is_descendant(HWND parent, HWND child) {
    U32_WINDOW *win = u32_lookup_window(child);
    while (win) {
        if (win->hwnd == parent) return 1;
        if (!win->parent) break;
        win = u32_lookup_window(win->parent);
    }
    return 0;
}

static HWND u32_find_top_level_window_for_pid(DWORD pid) {
    int i;
    for (i = MAX_U32_WINDOWS - 1; i >= 0; i--) {
        if (g_windows[i].used &&
            g_windows[i].top_level &&
            g_windows[i].owner_pid == pid &&
            g_windows[i].visible) {
            return g_windows[i].hwnd;
        }
    }
    for (i = MAX_U32_WINDOWS - 1; i >= 0; i--) {
        if (g_windows[i].used &&
            g_windows[i].top_level &&
            g_windows[i].owner_pid == pid) {
            return g_windows[i].hwnd;
        }
    }

    /* Older discouNT processes report PID 1 until the native process-ID
     * bridge is installed.  Their windows are still valid USER32 windows;
     * refusing this fallback makes CSRSS lose the window and therefore lose
     * all standard mouse/keyboard input.  Prefer the newest visible window
     * with the legacy owner tag. */
    if (pid != 1) {
        for (i = MAX_U32_WINDOWS - 1; i >= 0; i--) {
            if (g_windows[i].used && g_windows[i].top_level &&
                g_windows[i].owner_pid == 1 && g_windows[i].visible)
                return g_windows[i].hwnd;
        }
        for (i = MAX_U32_WINDOWS - 1; i >= 0; i--) {
            if (g_windows[i].used && g_windows[i].top_level &&
                g_windows[i].owner_pid == 1)
                return g_windows[i].hwnd;
        }
    }
    return NULL;
}

static HWND u32_dialog_focused_child(HWND hDlg) {
    if (g_focus && u32_is_descendant(hDlg, g_focus)) return g_focus;
    return NULL;
}

static int u32_is_tabstop_candidate(const U32_WINDOW *win, HWND hDlg) {
    if (!win || !win->used) return 0;
    if (win->hwnd == hDlg) return 0;
    if (!u32_is_descendant(hDlg, win->hwnd)) return 0;
    if (!win->visible || !win->enabled) return 0;
    if (!(win->style & WS_TABSTOP)) return 0;
    if (win->ctrl_type == U32_CTRL_STATIC || win->ctrl_type == U32_CTRL_GROUPBOX) return 0;
    return 1;
}

static HWND u32_dialog_find_next_tabstop(HWND hDlg, HWND start) {
    int i;
    int start_index = -1;
    int first_index = -1;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].hwnd == start) {
            start_index = i;
            break;
        }
    }
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (u32_is_tabstop_candidate(&g_windows[i], hDlg)) {
            if (first_index < 0) first_index = i;
            if (start_index >= 0 && i > start_index) return g_windows[i].hwnd;
        }
    }
    if (first_index >= 0) return g_windows[first_index].hwnd;
    return NULL;
}

static void u32_dialog_click_button(HWND hButton) {
    U32_WINDOW *button = u32_lookup_window(hButton);
    if (!button) return;
    if (button->parent) {
        SendMessageW(button->parent, WM_COMMAND, MAKEWPARAM(button->id, BN_CLICKED), (LPARAM)hButton);
    }
}

static HWND u32_dialog_find_child_by_id(HWND hDlg, UINT id) {
    int i;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hDlg && g_windows[i].id == id) {
            return g_windows[i].hwnd;
        }
    }
    return NULL;
}

static HWND u32_hit_test_child(HWND parent, int x, int y) {
    int i;
    for (i = MAX_U32_WINDOWS - 1; i >= 0; i--) {
        U32_WINDOW *child = &g_windows[i];
        int cx, cy, cw, ch;
        if (!child->used || child->parent != parent ||
            !u32_effectively_visible(child->hwnd)) continue;
        cx = child->rect.left;
        cy = child->rect.top;
        cw = child->rect.right - child->rect.left;
        ch = child->rect.bottom - child->rect.top;
        if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) {
            HWND deeper = u32_hit_test_child(child->hwnd, x - cx, y - cy);
            return deeper ? deeper : child->hwnd;
        }
    }
    return NULL;
}

static int u32_get_descendant_pos_in_ancestor_client(HWND ancestor, HWND descendant, int *out_x, int *out_y) {
    U32_WINDOW *win = u32_lookup_window(descendant);
    int x = 0;
    int y = 0;
    if (!ancestor || !descendant || !out_x || !out_y) return 0;
    while (win) {
        if (win->hwnd == ancestor) {
            *out_x = x;
            *out_y = y;
            return 1;
        }
        x += win->rect.left;
        y += win->rect.top;
        if (!win->parent) break;
        win = u32_lookup_window(win->parent);
    }
    return 0;
}

static HWND u32_route_mouse_target(HWND root, int x, int y) {
    HWND child = u32_hit_test_child(root, x, y);
    U32_WINDOW *walk;
    if (!child || child == root) return child;
    walk = u32_lookup_window(child);
    while (walk && walk->hwnd != root) {
        if (walk->ctrl_type == U32_CTRL_TAB) {
            int rel_x = 0;
            int rel_y = 0;
            int width;
            int height;
            if (u32_get_descendant_pos_in_ancestor_client(root, walk->hwnd, &rel_x, &rel_y)) {
                width = walk->rect.right - walk->rect.left;
                height = walk->rect.bottom - walk->rect.top;
                if (x >= rel_x && x < rel_x + width &&
                    y >= rel_y && y < rel_y + height &&
                    /* The themed/3-D tab bevel can extend a few pixels
                     * below the nominal 20-pixel header. */
                    y < rel_y + 28) {
                    return walk->hwnd;
                }
            }
        }
        walk = walk->parent ? u32_lookup_window(walk->parent) : 0;
    }
    return child;
}

static void u32_get_absolute_rect(U32_WINDOW *win, RECT *out) {
    RECT rc;
    if (!win || !out) return;
    if (win->top_level) u32_sync_top_level(win);
    rc = win->rect;
    while (win->parent) {
        U32_WINDOW *parent = u32_lookup_window(win->parent);
        if (!parent) break;
        if (parent->top_level) u32_sync_top_level(parent);
        rc.left += parent->rect.left;
        rc.top += parent->rect.top;
        rc.right += parent->rect.left;
        rc.bottom += parent->rect.top;
        if (parent->top_level) {
            int ox = u32_client_offset_x(parent);
            int oy = u32_client_offset_y(parent);
            rc.left += ox;
            rc.top += oy;
            rc.right += ox;
            rc.bottom += oy;
        }
        win = parent;
    }
    *out = rc;
}

static int u32_is_int_resource(LPCWSTR ptr) {
    return (((uintptr_t)ptr) >> 16) == 0;
}

static UINT u32_resource_id(LPCWSTR ptr) {
    return (UINT)((uintptr_t)ptr & 0xFFFF);
}

static int u32_class_eq(U32_WINDOW *win, LPCWSTR name) {
    if (!win || !win->klass) return 0;
    return u32_wstrcmp(win->klass->name, name) == 0;
}

static int u32_pick_ctrl_type(LPCWSTR class_name, DWORD style) {
    if (class_name) {
        if (u32_wstrcmp(class_name, L"#32770") == 0) return U32_CTRL_DIALOG;
        if (u32_wstrcmp(class_name, L"SysTabControl32") == 0) return U32_CTRL_TAB;
        if (u32_wstrcmp(class_name, L"SysListView32") == 0) return U32_CTRL_LISTVIEW;
        if (u32_wstrcmp(class_name, L"Button") == 0) {
            if ((style & BS_GROUPBOX) == BS_GROUPBOX) return U32_CTRL_GROUPBOX;
            return U32_CTRL_BUTTON;
        }
        if (u32_wstrcmp(class_name, L"Static") == 0) return U32_CTRL_STATIC;
        if (u32_wstrcmp(class_name, L"Edit") == 0) return U32_CTRL_EDIT;
        if (u32_wstrcmp(class_name, L"ComboBox") == 0) return U32_CTRL_COMBO;
        if (u32_wstrcmp(class_name, L"msctls_statusbar32") == 0) return U32_CTRL_STATUS;
    }
    return U32_CTRL_GENERIC;
}

static U32_ICON *u32_lookup_icon(HICON hicon) {
    int i;
    for (i = 0; i < MAX_U32_ICONS; i++) {
        if (g_icons[i].used && (HICON)&g_icons[i] == hicon) return &g_icons[i];
    }
    return NULL;
}

static HICON u32_alloc_icon(UINT resource_id, int width, int height) {
    int i;
    for (i = 0; i < MAX_U32_ICONS; i++) {
        if (!g_icons[i].used) {
            memset(&g_icons[i], 0, sizeof(g_icons[i]));
            g_icons[i].used = 1;
            g_icons[i].resource_id = resource_id;
            g_icons[i].width = width;
            g_icons[i].height = height;
            g_icons[i].icon.magic = DISCOUNT_ICON_MAGIC;
            g_icons[i].icon.width = width;
            g_icons[i].icon.height = height;
            g_icons[i].icon.pixels = 0;
            return (HICON)&g_icons[i];
        }
    }
    return 0;
}

static uint32_t u32_pack_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16);
}

static int u32_ico_row_bytes(int width, int bpp) {
    int bits = width * bpp;
    return ((bits + 31) / 32) * 4;
}

static int u32_menu_text_copy(LPCWSTR src, WCHAR *dst, int max_chars, int strip_tab) {
    int si = 0;
    int di = 0;
    if (!dst || max_chars <= 0) return 0;
    if (!src) {
        dst[0] = 0;
        return 0;
    }
    while (src[si] && di < max_chars - 1) {
        WCHAR ch = src[si++];
        if (strip_tab && ch == L'\t') break;
        if (ch == L'&') continue;
        dst[di++] = ch;
    }
    dst[di] = 0;
    return di;
}

static int u32_menu_bar_item_width(LPCWSTR text) {
    WCHAR buf[64];
    int len = u32_menu_text_copy(text, buf, 64, 1);
    return (len * 8) + 14;
}

static int u32_popup_item_height(const U32_MENU_ITEM *item) {
    if (!item) return 0;
    return (item->flags & MF_SEPARATOR) ? 8 : 18;
}

static void u32_destroy_subtree(HWND hwnd) {
    int i;
    for (i = MAX_U32_WINDOWS - 1; i >= 0; i--) {
        if (g_windows[i].used && g_windows[i].parent == hwnd) {
            u32_destroy_subtree(g_windows[i].hwnd);
        }
    }
    DestroyWindow(hwnd);
}

static void u32_forget_native_window(HWND hwnd) {
    int i;
    U32_WINDOW *win = u32_lookup_window(hwnd);
    if (!win) return;
    for (i = MAX_U32_WINDOWS - 1; i >= 0; i--) {
        if (g_windows[i].used && g_windows[i].parent == hwnd) {
            u32_forget_native_window(g_windows[i].hwnd);
        }
    }
    if (win->listview_data) kfree(win->listview_data);
    if (win->edit_text) kfree(win->edit_text);
    memset(win, 0, sizeof(*win));
}

static U32_WINDOW *u32_find_desktop_window(void) {
    int i;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].top_level && g_windows[i].klass &&
            u32_wstrcmp(g_windows[i].klass->name, L"Desktop") == 0)
            return &g_windows[i];
    }
    return NULL;
}

static void u32_notify_desktop_window(HWND hwnd, UINT event) {
    U32_WINDOW *desktop = u32_find_desktop_window();
    U32_WINDOW *win = u32_lookup_window(hwnd);
    if (!desktop || !win || desktop == win || !win->top_level) return;
    /* Explorer only tracks windows from other processes.  Suppress its own
       tray/popups here rather than relying on ambient process-id state. */
    if (desktop->owner_pid == win->owner_pid) return;
    SendMessageW(desktop->hwnd, WM_PARENTNOTIFY, MAKEWPARAM(event, 0), (LPARAM)hwnd);
}

static int u32_has_top_level_window_for_pid(DWORD pid, HWND except) {
    int i;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].top_level &&
            g_windows[i].hwnd != except && g_windows[i].owner_pid == pid) return 1;
    }
    return 0;
}

static HWND u32_find_menu_root(HWND hwnd) {
    U32_WINDOW *win = u32_lookup_window(hwnd);
    while (win) {
        if (win->ctrl_type == U32_CTRL_MENUPOPUP && win->menu_owner) return win->menu_owner;
        if (win->top_level) return win->hwnd;
        if (!win->parent) break;
        win = u32_lookup_window(win->parent);
    }
    return hwnd;
}

static void u32_close_popup_chain(HWND owner_hwnd) {
    U32_WINDOW *owner = u32_lookup_window(owner_hwnd);
    if (!owner) return;
    if (owner->popup_menu) {
        HWND popup = owner->popup_menu;
        owner->popup_menu = NULL;
        u32_destroy_subtree(popup);
    }
    owner->active_menu_index = -1;
    owner->hot_index = -1;
    if (owner->top_level) {
        SendMessageW(owner_hwnd, WM_EXITMENULOOP, 0, 0);
        g_open_menu_popup = NULL;
    }
    u32_mark_invalid(owner_hwnd);
}

static int u32_menu_bar_hit_test(U32_WINDOW *win, int x, int y) {
    U32_MENU *menu;
    int i;
    int left = 6;
    if (!win || !win->menu || y < 0 || y >= U32_MENU_HEIGHT) return -1;
    menu = u32_lookup_menu(win->menu);
    if (!menu) return -1;
    for (i = 0; i < menu->count; i++) {
        int width = u32_menu_bar_item_width(menu->items[i].text);
        if (x >= left && x < left + width) return i;
        left += width;
    }
    return -1;
}

static int u32_popup_item_at(U32_MENU *menu, int y) {
    int i;
    int top = 2;
    if (!menu) return -1;
    for (i = 0; i < menu->count; i++) {
        int h = u32_popup_item_height(&menu->items[i]);
        if (y >= top && y < top + h) return i;
        top += h;
    }
    return -1;
}

static HWND u32_create_menu_popup(HWND parent, HWND owner_hwnd, HMENU hMenu,
                                  int x, int y, UINT align_flags) {
    U32_WINDOW *pwin = u32_lookup_window(parent);
    U32_WINDOW *owner = u32_lookup_window(owner_hwnd);
    U32_MENU *menu = u32_lookup_menu(hMenu);
    U32_WINDOW *win;
    int i;
    int width = 64;
    int height = 4;
    RECT parent_abs;
    if (!pwin || !owner || !menu) return NULL;
    /* Applications commonly populate submenus lazily from this message.  It
       must be delivered before the popup is measured, not after its window
       has already been created with the dimensions of an empty menu. */
    SendMessageW(owner_hwnd, WM_INITMENUPOPUP, (WPARAM)hMenu, 0);
    for (i = 0; i < menu->count; i++) {
        WCHAR text[64];
        int len = u32_menu_text_copy(menu->items[i].text, text, 64, 1);
        int item_width = (len * 8) + 24 + (menu->items[i].submenu ? 12 : 0);
        if (item_width > width) width = item_width;
        height += u32_popup_item_height(&menu->items[i]);
    }
    u32_get_absolute_rect(pwin, &parent_abs);
    x += parent_abs.left + u32_client_offset_x(pwin);
    y += parent_abs.top + u32_client_offset_y(pwin);
    if (align_flags & TPM_BOTTOMALIGN) y -= height;
    if (x + width > GetSystemMetrics(SM_CXSCREEN)) x = GetSystemMetrics(SM_CXSCREEN) - width;
    if (y + height > GetSystemMetrics(SM_CYSCREEN)) y = GetSystemMetrics(SM_CYSCREEN) - height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    /* Menu popups are allocated directly below instead of going through
       CreateWindowExW, so make sure their native win32k class exists first.
       Without this registration Win32kCreateWindow("MenuPopup", ...) always
       returns INVALID_HANDLE and TrackPopupMenuEx silently displays nothing. */
    if (!u32_find_class(L"MenuPopup")) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = DefWindowProcW;
        wc.lpszClassName = L"MenuPopup";
        if (!RegisterClassW(&wc)) return NULL;
    }
    win = u32_alloc_window();
    if (!win) return NULL;
    win->hwnd = (HWND)Win32kCreateWindow("MenuPopup", "", x, y, width, height,
                                         WS_POPUP | WS_VISIBLE);
    if (!win->hwnd) {
        win->used = 0;
        return NULL;
    }
    win->parent = NULL;
    win->owner = owner_hwnd;
    win->top_level = 1;
    win->visible = 1;
    win->enabled = 1;
    win->owner_pid = owner->owner_pid;
    win->style = WS_POPUP | WS_VISIBLE | WS_BORDER;
    win->ctrl_type = U32_CTRL_MENUPOPUP;
    win->menu = hMenu;
    win->popup_menu = NULL;
    win->menu_owner = owner_hwnd;
    win->hot_index = -1;
    win->active_menu_index = -1;
    u32_set_rect(win, x, y, width, height);
    u32_mark_invalid(win->hwnd);
    Win32kShowWindow((HANDLE)win->hwnd);
    Win32kRedrawAll();
    return win->hwnd;
}

static HWND u32_open_submenu(HWND owner_hwnd, HWND parent_popup_hwnd, int item_index) {
    U32_WINDOW *popup_win = u32_lookup_window(parent_popup_hwnd);
    U32_MENU *menu;
    int yoff = 2;
    int i;
    if (!popup_win || popup_win->ctrl_type != U32_CTRL_MENUPOPUP) return NULL;
    menu = u32_lookup_menu(popup_win->menu);
    if (!menu || item_index < 0 || item_index >= menu->count) return NULL;
    if (!menu->items[item_index].submenu) return NULL;
    if (popup_win->popup_menu && popup_win->active_menu_index == item_index) return popup_win->popup_menu;
    if (popup_win->popup_menu) u32_close_popup_chain(parent_popup_hwnd);
    for (i = 0; i < item_index; i++) yoff += u32_popup_item_height(&menu->items[i]);
    popup_win->active_menu_index = item_index;
    popup_win->popup_menu = u32_create_menu_popup(parent_popup_hwnd, owner_hwnd, menu->items[item_index].submenu,
                                                  popup_win->rect.right - popup_win->rect.left - 1, yoff - 1, 0);
    return popup_win->popup_menu;
}

static HWND u32_open_menu_bar_popup(HWND owner_hwnd, int menu_index) {
    U32_WINDOW *owner = u32_lookup_window(owner_hwnd);
    U32_MENU *menu;
    int i;
    int left = 6;
    if (!owner || !owner->menu) return NULL;
    menu = u32_lookup_menu(owner->menu);
    if (!menu || menu_index < 0 || menu_index >= menu->count) return NULL;
    if (!menu->items[menu_index].submenu) return NULL;
    if (owner->popup_menu) u32_close_popup_chain(owner_hwnd);
    SendMessageW(owner_hwnd, WM_ENTERMENULOOP, 0, 0);
    for (i = 0; i < menu_index; i++) left += u32_menu_bar_item_width(menu->items[i].text);
    owner->popup_menu = u32_create_menu_popup(owner_hwnd, owner_hwnd, menu->items[menu_index].submenu,
                                              left, U32_MENU_HEIGHT, 0);
    owner->active_menu_index = menu_index;
    owner->hot_index = menu_index;
    g_open_menu_popup = owner->popup_menu;
    u32_mark_invalid(owner_hwnd);
    return owner->popup_menu;
}

static void u32_draw_menu_bar(HDC hdc, U32_WINDOW *win, const RECT *rc) {
    U32_MENU *menu;
    int i;
    int x = 6;
    RECT bar_rc;
    if (!hdc || !win || !rc || !win->menu) return;
    menu = u32_lookup_menu(win->menu);
    if (!menu) return;

    bar_rc.left = 0;
    bar_rc.top = 0;
    bar_rc.right = rc->right;
    bar_rc.bottom = U32_MENU_HEIGHT;
    FillRect(hdc, &bar_rc, (HBRUSH)GetStockObject(0));
    MoveToEx(hdc, 0, U32_MENU_HEIGHT - 1, 0);
    LineTo(hdc, rc->right, U32_MENU_HEIGHT - 1);

    for (i = 0; i < menu->count; i++) {
        WCHAR text[64];
        int width = u32_menu_bar_item_width(menu->items[i].text);
        RECT item_rc;
        u32_menu_text_copy(menu->items[i].text, text, 64, 1);
        item_rc.left = x - 2;
        item_rc.top = 2;
        item_rc.right = x + width - 4;
        item_rc.bottom = U32_MENU_HEIGHT - 2;
        if (win->active_menu_index == i) {
            FillRect(hdc, &item_rc, (HBRUSH)GetStockObject(0));
            Rectangle(hdc, item_rc.left, item_rc.top, item_rc.right, item_rc.bottom);
        }
        TextOutW(hdc, x, 4, text, -1);
        x += width;
    }
}

static void u32_draw_menu_popup(HDC hdc, U32_WINDOW *win, const RECT *rc) {
    U32_MENU *menu;
    int i;
    int top = 2;
    if (!hdc || !win || !rc || !win->menu) return;
    menu = u32_lookup_menu(win->menu);
    if (!menu) return;
    FillRect(hdc, rc, (HBRUSH)GetStockObject(0));
    Rectangle(hdc, 0, 0, rc->right, rc->bottom);
    for (i = 0; i < menu->count; i++) {
        const U32_MENU_ITEM *item = &menu->items[i];
        int h = u32_popup_item_height(item);
        if (item->flags & MF_SEPARATOR) {
            MoveToEx(hdc, 4, top + 3, 0);
            LineTo(hdc, rc->right - 4, top + 3);
        } else {
            WCHAR text[64];
            RECT item_rc;
            u32_menu_text_copy(item->text, text, 64, 1);
            item_rc.left = 2;
            item_rc.top = top;
            item_rc.right = rc->right - 2;
            item_rc.bottom = top + h;
            if (win->hot_index == i || win->active_menu_index == i) {
                FillRect(hdc, &item_rc, (HBRUSH)GetStockObject(0));
                Rectangle(hdc, item_rc.left, item_rc.top, item_rc.right, item_rc.bottom);
            }
            TextOutW(hdc, 6, top + 4, text, -1);
            if (item->submenu) TextOutW(hdc, rc->right - 12, top + 4, L">", 1);
        }
        top += h;
    }
}

static int u32_ico_decode_bitmap(DISCOUNT_ICON *icon, const uint8_t *data, uint32_t size) {
    uint32_t dib_size;
    int width;
    int height;
    int xor_bpp;
    int palette_entries;
    const uint8_t *palette;
    const uint8_t *xor_bits;
    const uint8_t *and_bits;
    int xor_stride;
    int and_stride;

    if (!icon || !data || size < 40) return 0;
    dib_size = *(const uint32_t*)(data + 0);
    if (dib_size < 40 || size < dib_size) return 0;

    width = *(const int32_t*)(data + 4);
    height = *(const int32_t*)(data + 8) / 2;
    xor_bpp = *(const uint16_t*)(data + 14);
    if (width <= 0 || height <= 0) return 0;
    if (!(xor_bpp == 4 || xor_bpp == 8 || xor_bpp == 32)) return 0;

    palette_entries = 0;
    if (xor_bpp <= 8) palette_entries = 1 << xor_bpp;
    palette = data + dib_size;
    xor_bits = palette + (palette_entries * 4);
    xor_stride = u32_ico_row_bytes(width, xor_bpp);
    and_stride = u32_ico_row_bytes(width, 1);
    and_bits = xor_bits + (xor_stride * height);

    if ((uint32_t)(and_bits - data) > size) return 0;
    if ((uint32_t)(and_bits - data) + (uint32_t)(and_stride * height) > size) return 0;

    icon->width = width;
    icon->height = height;
    icon->pixels = (uint32_t*)kmalloc((uint32_t)(width * height * sizeof(uint32_t)));
    if (!icon->pixels) return 0;
    memset(icon->pixels, 0, (uint32_t)(width * height * sizeof(uint32_t)));

    for (int y = 0; y < height; y++) {
        int src_y = height - 1 - y;
        const uint8_t *xor_row = xor_bits + (src_y * xor_stride);
        const uint8_t *and_row = and_bits + (src_y * and_stride);
        for (int x = 0; x < width; x++) {
            uint8_t mask = (uint8_t)((and_row[x >> 3] >> (7 - (x & 7))) & 1);
            uint32_t color = 0;
            uint8_t alpha = 0xFF;

            if (xor_bpp == 32) {
                const uint8_t *px = xor_row + (x * 4);
                uint8_t b = px[0];
                uint8_t g = px[1];
                uint8_t r = px[2];
                alpha = px[3];
                color = u32_pack_rgb(r, g, b);
                if (alpha == 0 && !mask) alpha = 0xFF;
            } else if (xor_bpp == 8) {
                uint8_t idx = xor_row[x];
                const uint8_t *ent = palette + (idx * 4);
                color = u32_pack_rgb(ent[2], ent[1], ent[0]);
            } else if (xor_bpp == 4) {
                uint8_t nyb = xor_row[x >> 1];
                uint8_t idx = (x & 1) ? (nyb & 0x0F) : (nyb >> 4);
                const uint8_t *ent = palette + (idx * 4);
                color = u32_pack_rgb(ent[2], ent[1], ent[0]);
            }

            if (mask) alpha = 0;
            icon->pixels[(y * width) + x] = color | ((uint32_t)alpha << 24);
        }
    }

    return 1;
}

static HICON u32_create_icon_from_ico_buffer(const uint8_t *data, uint32_t size, int desired_w, int desired_h) {
    uint16_t count;
    const uint8_t *best_entry = 0;
    int best_score = 0x7FFFFFFF;
    HICON handle;
    U32_ICON *slot;

    if (!data || size < 6) return 0;
    if (*(const uint16_t*)(data + 0) != 0 || *(const uint16_t*)(data + 2) != 1) return 0;
    count = *(const uint16_t*)(data + 4);
    if (size < 6 + (count * 16)) return 0;

    for (uint16_t i = 0; i < count; i++) {
        const uint8_t *entry = data + 6 + (i * 16);
        int w = entry[0] ? entry[0] : 256;
        int h = entry[1] ? entry[1] : 256;
        int score = 0;
        if (desired_w > 0) score += (w > desired_w) ? (w - desired_w) : (desired_w - w);
        if (desired_h > 0) score += (h > desired_h) ? (h - desired_h) : (desired_h - h);
        if (score < best_score) {
            best_score = score;
            best_entry = entry;
        }
    }

    if (!best_entry) return 0;

    {
        uint32_t bytes = *(const uint32_t*)(best_entry + 8);
        uint32_t offset = *(const uint32_t*)(best_entry + 12);
        if (offset >= size || bytes > size || offset + bytes > size) return 0;
        handle = u32_alloc_icon(0, desired_w > 0 ? desired_w : (best_entry[0] ? best_entry[0] : 256),
                                desired_h > 0 ? desired_h : (best_entry[1] ? best_entry[1] : 256));
        if (!handle) return 0;
        slot = u32_lookup_icon(handle);
        if (!slot) return 0;
        if (!u32_ico_decode_bitmap(&slot->icon, data + offset, bytes)) {
            memset(slot, 0, sizeof(*slot));
            return 0;
        }
        slot->width = slot->icon.width;
        slot->height = slot->icon.height;
        return handle;
    }
}

static HICON u32_load_icon_file(const char *path, int desired_w, int desired_h) {
    uint8_t *file_buf = 0;
    uint32_t file_size = 0;
    HICON icon = 0;
    if (!path || !*path) return 0;
    if (!CdfsReadFile(path, &file_buf, &file_size)) return 0;
    icon = u32_create_icon_from_ico_buffer(file_buf, file_size, desired_w, desired_h);
    kfree(file_buf);
    return icon;
}

static HICON u32_try_load_module_sidecar_icon(HINSTANCE hInstance, int width, int height) {
    const char *image_path;
    char path[160];
    int i;
    int dot = -1;

    if (!hInstance) return 0;
    image_path = PeGetImagePath((void*)hInstance);
    if (!image_path || !*image_path) return 0;

    for (i = 0; image_path[i] && i < (int)sizeof(path) - 1; i++) {
        path[i] = u32_toupper_char(image_path[i]);
        if (path[i] == '.') dot = i;
    }
    path[i] = 0;
    if (dot < 0 || dot > (int)sizeof(path) - 5) return 0;
    path[dot + 0] = '.';
    path[dot + 1] = 'I';
    path[dot + 2] = 'C';
    path[dot + 3] = 'O';
    path[dot + 4] = 0;
    return u32_load_icon_file(path, width, height);
}

static HICON u32_try_load_icon_by_name(LPCWSTR name, int width, int height) {
    char path[160];
    int i = 0;
    if (!name || u32_is_int_resource(name)) return 0;
    while (name[i] && i < (int)sizeof(path) - 1) {
        path[i] = u32_toupper_char((char)name[i]);
        i++;
    }
    path[i] = 0;
    return u32_load_icon_file(path, width, height);
}

static LRESULT u32_dispatch(U32_WINDOW *win, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!win) return 0;
    if (win->dlgproc) {
        INT_PTR handled = win->dlgproc(win->hwnd, msg, wParam, lParam);
        if (handled) return handled;
        return DefWindowProcW(win->hwnd, msg, wParam, lParam);
    }
    if (win->proc) return win->proc(win->hwnd, msg, wParam, lParam);
    return DefWindowProcW(win->hwnd, msg, wParam, lParam);
}

static void u32_win32k_callback(HANDLE hwnd, uint32_t msg, uint32_t wParam, uint32_t lParam) {
    U32_WINDOW *win = u32_lookup_window((HWND)hwnd);
    if (!win) return;
    if (win->top_level) u32_sync_top_level(win);
    u32_dispatch(win, msg, (WPARAM)wParam, (LPARAM)lParam);
    if (msg == WM_PAINT && win->top_level) {
        u32_paint_children((HWND)hwnd);
    }
    if (msg == WM_DESTROY) {
        u32_notify_desktop_window((HWND)hwnd, WM_DESTROY);
        /* Win32k can destroy a window during caption handling or CSRSS
         * cleanup. Keep USER32's handle table in sync with that native event. */
        u32_forget_native_window((HWND)hwnd);
    }
}

static HWND u32_create_placeholder_child(HWND parent, UINT id) {
    U32_WINDOW *pwin = u32_lookup_window(parent);
    U32_WINDOW *win = u32_alloc_window();
    if (!win) return NULL;
    win->hwnd = (HWND)win;
    win->parent = parent;
    win->owner = parent;
    win->top_level = 0;
    win->visible = 1;
    win->enabled = 1;
    win->id = id;
    win->owner_pid = pwin ? pwin->owner_pid : u32_current_process_id();
    win->style = WS_CHILD | WS_VISIBLE;
    win->ctrl_type = U32_CTRL_GENERIC;
    if (pwin) u32_set_rect(win, 8, 8, 160, 24);
    else u32_set_rect(win, 0, 0, 160, 24);
    return win->hwnd;
}

static HWND u32_create_child_control(HWND parent, LPCWSTR class_name, LPCWSTR title, DWORD style, DWORD exstyle,
                                     UINT id, int x, int y, int w, int h) {
    U32_WINDOW *pwin = u32_lookup_window(parent);
    U32_WINDOW *win;
    RECT prc;
    if (!pwin) return NULL;
    if (!u32_find_class(class_name)) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpszClassName = class_name;
        RegisterClassW(&wc);
    }
    win = u32_alloc_window();
    if (!win) return NULL;
    win->hwnd = (HWND)win;
    win->parent = parent;
    win->owner = parent;
    win->top_level = 0;
    win->visible = (style & WS_VISIBLE) ? 1 : 0;
    win->enabled = (style & WS_DISABLED) ? 0 : 1;
    win->id = id;
    win->owner_pid = pwin->owner_pid;
    win->style = style | WS_CHILD;
    win->exstyle = exstyle;
    win->klass = u32_find_class(class_name);
    win->proc = win->klass ? win->klass->proc : NULL;
    win->ctrl_type = u32_pick_ctrl_type(class_name, style);
    u32_scroll_init(win);
    u32_wstrcpy(win->title, title, 128);
    (void)prc;
    u32_set_rect(win, x, y, w, h);
    SendMessageW(win->hwnd, WM_CREATE, 0, 0);
    SendMessageW(win->hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(w, h));
    return win->hwnd;
}

static const U32_TEMPLATE_CONTROL u32_dlg_102_controls[] = {
    {L"SysTabControl32", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 1015, 3, 3, 510, 350},
};

static const U32_TEMPLATE_CONTROL u32_dlg_106_controls[] = {
    {L"SysListView32", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS, 0, 1016, 4, 4, 239, 180},
    {L"Button", L"&New Task...", WS_CHILD | WS_VISIBLE, 0, 1014, 175, 189, 68, 14},
    {L"Button", L"&Switch To", WS_CHILD | WS_VISIBLE | WS_DISABLED, 0, 1013, 104, 189, 68, 14},
    {L"Button", L"&End Task", WS_CHILD | WS_VISIBLE | WS_DISABLED, 0, 1012, 33, 189, 68, 14},
};

static const U32_TEMPLATE_CONTROL u32_dlg_133_controls[] = {
    {L"SysListView32", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA, 0, 1018, 4, 4, 239, 180},
    {L"Button", L"&End Process", WS_CHILD | WS_VISIBLE, 0, 1017, 165, 189, 78, 14},
    {L"Button", L"&Show processes from all users", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 1021, 4, 191, 160, 10},
};

static const U32_TEMPLATE_CONTROL u32_dlg_134_controls[] = {
    {L"Button", L"CPU usage", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_TABSTOP, 0, 1043, 5, 5, 60, 54},
    {L"Button", L"Mem usage", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1044, 5, 63, 60, 54},
    {L"Button", L"Totals", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1037, 5, 122, 111, 39},
    {L"Button", L"Commit charge (K)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1038, 5, 166, 111, 39},
    {L"Button", L"Physical memory (K)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1040, 126, 122, 116, 39},
    {L"Button", L"Kernel memory (K)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1039, 126, 166, 116, 39},
    {L"Static", L"Handles", WS_CHILD | WS_VISIBLE, 0, 1060, 12, 131, 43, 8},
    {L"Static", L"Threads", WS_CHILD | WS_VISIBLE, 0, 1061, 12, 140, 43, 8},
    {L"Static", L"Processes", WS_CHILD | WS_VISIBLE, 0, 1062, 12, 149, 43, 8},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1024, 65, 130, 45, 10},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1026, 65, 140, 45, 10},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1027, 65, 150, 45, 10},
    {L"Static", L"Total", WS_CHILD | WS_VISIBLE, 0, 1063, 12, 175, 43, 8},
    {L"Static", L"Limit", WS_CHILD | WS_VISIBLE, 0, 1064, 12, 184, 43, 8},
    {L"Static", L"Peak", WS_CHILD | WS_VISIBLE, 0, 1065, 12, 193, 43, 8},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1028, 65, 173, 45, 10},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1029, 65, 184, 45, 10},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1030, 65, 195, 45, 10},
    {L"Static", L"Total", WS_CHILD | WS_VISIBLE, 0, 1066, 132, 131, 53, 8},
    {L"Static", L"Available", WS_CHILD | WS_VISIBLE, 0, 1067, 132, 140, 53, 8},
    {L"Static", L"System Cache", WS_CHILD | WS_VISIBLE, 0, 1068, 132, 149, 53, 8},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1031, 185, 130, 48, 10},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1032, 185, 140, 48, 10},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1033, 185, 150, 48, 10},
    {L"Static", L"Total", WS_CHILD | WS_VISIBLE, 0, 1069, 132, 174, 53, 8},
    {L"Static", L"Paged", WS_CHILD | WS_VISIBLE, 0, 1070, 132, 184, 53, 8},
    {L"Static", L"Nonpaged", WS_CHILD | WS_VISIBLE, 0, 1071, 132, 193, 53, 8},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1034, 185, 173, 48, 10},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1035, 185, 184, 48, 10},
    {L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1036, 185, 195, 48, 10},
    {L"Button", L"CPU usage history", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1045, 74, 5, 168, 54},
    {L"Button", L"Memory usage history", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1046, 74, 63, 168, 54},
    {L"Button", L"", WS_CHILD | WS_VISIBLE, 0, 1047, 12, 17, 47, 37},
    {L"Button", L"", WS_CHILD | WS_VISIBLE, 0, 1048, 12, 75, 47, 37},
    {L"Button", L"", WS_CHILD | WS_VISIBLE, 0, 1050, 81, 17, 153, 37},
    {L"Button", L"", WS_CHILD | WS_VISIBLE, 0, 1049, 81, 75, 153, 37},
};

static const U32_DIALOG_TEMPLATE_DEF u32_builtin_dialogs[] = {
    {102, L"Task Manager", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 540, 420, 0, u32_dlg_102_controls, (int)(sizeof(u32_dlg_102_controls)/sizeof(u32_dlg_102_controls[0]))},
    {106, L"Dialog", WS_CHILD | WS_VISIBLE, 510, 350, 1, u32_dlg_106_controls, (int)(sizeof(u32_dlg_106_controls)/sizeof(u32_dlg_106_controls[0]))},
    {133, L"Dialog", WS_CHILD | WS_VISIBLE, 510, 350, 1, u32_dlg_133_controls, (int)(sizeof(u32_dlg_133_controls)/sizeof(u32_dlg_133_controls[0]))},
    {134, L"Dialog", WS_CHILD | WS_VISIBLE, 510, 350, 1, u32_dlg_134_controls, (int)(sizeof(u32_dlg_134_controls)/sizeof(u32_dlg_134_controls[0]))},
};

static const U32_DIALOG_TEMPLATE_DEF *u32_find_builtin_dialog(UINT tmpl_id) {
    if (g_registered_dialog_used && g_registered_dialog.tmpl_id == tmpl_id)
        return &g_registered_dialog;
    int i;
    for (i = 0; i < (int)(sizeof(u32_builtin_dialogs)/sizeof(u32_builtin_dialogs[0])); i++) {
        if (u32_builtin_dialogs[i].tmpl_id == tmpl_id) return &u32_builtin_dialogs[i];
    }
    return 0;
}

BOOL WINAPI User32RegisterDialogTemplate(const DISCOUNT_DIALOG_TEMPLATE *tmpl) {
    if (!tmpl || !tmpl->controls || tmpl->control_count < 0) return FALSE;
    memset(&g_registered_dialog, 0, sizeof(g_registered_dialog));
    g_registered_dialog.tmpl_id = tmpl->id;
    g_registered_dialog.caption = tmpl->caption;
    g_registered_dialog.style = tmpl->style;
    g_registered_dialog.width = tmpl->width;
    g_registered_dialog.height = tmpl->height;
    g_registered_dialog.attach_to_tab_of_parent = 0;
    g_registered_dialog.controls = (const U32_TEMPLATE_CONTROL *)tmpl->controls;
    g_registered_dialog.control_count = tmpl->control_count;
    g_registered_dialog_used = 1;
    return TRUE;
}

static void u32_create_builtin_dialog_children(const U32_DIALOG_TEMPLATE_DEF *tmpl, HWND hwnd) {
    int i;
    if (!tmpl) return;
    for (i = 0; i < tmpl->control_count; i++) {
        const U32_TEMPLATE_CONTROL *ctl = &tmpl->controls[i];
        u32_create_child_control(hwnd, ctl->class_name, ctl->title, ctl->style, ctl->exstyle,
                                 ctl->id, ctl->x, ctl->y, ctl->w, ctl->h);
    }
}

static int u32_effectively_visible(HWND hwnd) {
    U32_WINDOW *win = u32_lookup_window(hwnd);
    int guard = 0;
    while (win && guard++ < MAX_U32_WINDOWS) {
        if (!win->visible) return 0;
        if (!win->parent) break;
        win = u32_lookup_window(win->parent);
    }
    return win != NULL;
}

static void u32_paint_window(HWND hwnd) {
    int i;
    U32_WINDOW *win = u32_lookup_window(hwnd);
    if (!win || !u32_effectively_visible(hwnd)) return;
    SendMessageW(hwnd, WM_PAINT, 0, 0);
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hwnd &&
            u32_effectively_visible(g_windows[i].hwnd)) {
            u32_paint_window(g_windows[i].hwnd);
        }
    }
}

static void u32_paint_children(HWND hwnd) {
    int i;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hwnd &&
            u32_effectively_visible(g_windows[i].hwnd)) {
            u32_paint_window(g_windows[i].hwnd);
        }
    }
}

static int u32_message_matches(const MSG *msg, HWND hWnd, UINT min_msg, UINT max_msg) {
    if (!msg) return 0;
    if (hWnd && msg->hwnd != hWnd) return 0;
    if (min_msg || max_msg) {
        if (msg->message < min_msg) return 0;
        if (max_msg && msg->message > max_msg) return 0;
    }
    return 1;
}

static int u32_enqueue_message(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    int i;
    DWORD owner_pid = u32_current_process_id();
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (win && win->owner_pid) owner_pid = win->owner_pid;
    for (i = 0; i < MAX_U32_MESSAGES; i++) {
        if (!g_messages[i].used) {
            g_messages[i].used = 1;
            g_messages[i].msg.hwnd = hWnd;
            g_messages[i].msg.message = Msg;
            g_messages[i].msg.wParam = wParam;
            g_messages[i].msg.lParam = lParam;
            g_messages[i].msg.time = GetTickCount();
            g_messages[i].msg.pt.x = 0;
            g_messages[i].msg.pt.y = 0;
            g_messages[i].owner_pid = owner_pid;
            return TRUE;
        }
    }
    return FALSE;
}

static int u32_dequeue_message(LPMSG lpMsg, HWND hWnd, UINT min_msg, UINT max_msg, int remove) {
    int i;
    DWORD current_pid = u32_current_process_id();
    if (!lpMsg) return FALSE;
    for (i = 0; i < MAX_U32_MESSAGES; i++) {
        if (g_messages[i].used &&
            g_messages[i].owner_pid == current_pid &&
            u32_message_matches(&g_messages[i].msg, hWnd, min_msg, max_msg)) {
            *lpMsg = g_messages[i].msg;
            if (remove) {
                int j;
                for (j = i; j < MAX_U32_MESSAGES - 1; j++) {
                    g_messages[j] = g_messages[j + 1];
                }
                memset(&g_messages[MAX_U32_MESSAGES - 1], 0, sizeof(g_messages[MAX_U32_MESSAGES - 1]));
            }
            return TRUE;
        }
    }
    return FALSE;
}

static int u32_find_invalid_window(HWND filter) {
    int i;
    DWORD current_pid = u32_current_process_id();
    HWND active = (HWND)Win32kGetActiveWindow();
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && u32_effectively_visible(g_windows[i].hwnd) &&
            g_windows[i].invalidated &&
            g_windows[i].owner_pid == current_pid) {
            /* USER32 paints into one shared framebuffer.  A background
             * window must not paint over the foreground window; leave it
             * dirty and repaint it when it becomes active. */
            if (active && u32_get_root_window(g_windows[i].hwnd) != active)
                continue;
            if (!filter || g_windows[i].hwnd == filter) return (int)i;
        }
    }
    return -1;
}

static void u32_mark_invalid(HWND hwnd) {
    U32_WINDOW *win = u32_lookup_window(hwnd);
    /* A child owns its own paint region.  Do not bubble ordinary child
     * invalidation to the top-level window: doing so turns a small graph
     * update into a complete scene redraw, briefly exposing the intermediate
     * framebuffer and disturbing z-order in the lightweight compositor.
     * Explicit callers still invalidate the parent when layout changes. */
    if (win && !u32_effectively_visible(hwnd)) return;
    if (win) win->invalidated = 1;
}

static void u32_mark_invalid_descendants(HWND hwnd) {
    int i;
    U32_WINDOW *win = u32_lookup_window(hwnd);
    if (!win) return;
    win->invalidated = 1;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hwnd) {
            u32_mark_invalid_descendants(g_windows[i].hwnd);
        }
    }
}

static void u32_clear_invalid_subtree(HWND hwnd) {
    int i;
    U32_WINDOW *win = u32_lookup_window(hwnd);
    if (!win) return;
    win->invalidated = 0;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hwnd) {
            u32_clear_invalid_subtree(g_windows[i].hwnd);
        }
    }
}

static void u32_clear_invalid(HWND hwnd) {
    U32_WINDOW *win = u32_lookup_window(hwnd);
    if (!win) return;
    win->invalidated = 0;
}

static void u32_flush_invalid_window(HWND hwnd) {
    int i;
    U32_WINDOW *win = u32_lookup_window(hwnd);
    static int logged_flush = 0;
    if (!win || !u32_effectively_visible(hwnd)) return;
    if (win->invalidated && !win->painting) {
        if (logged_flush < 12) {
            logged_flush++;
            SerialPutString("[USER32] flush invalid begin\r\n");
        }
        UpdateWindow(hwnd);
        if (logged_flush <= 12) {
            SerialPutString("[USER32] flush invalid end\r\n");
        }
        u32_clear_invalid(hwnd);
        return;
    }
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hwnd &&
            u32_effectively_visible(g_windows[i].hwnd)) {
            u32_flush_invalid_window(g_windows[i].hwnd);
        }
    }
}

static void u32_pump_timers(HWND modal_hwnd) {
    int i;
    uint32_t now = GetTickCount();
    for (i = 0; i < MAX_U32_TIMERS; i++) {
        if (g_timers[i].used && (!modal_hwnd || g_timers[i].hwnd == modal_hwnd)) {
            if (now >= g_timers[i].tick) {
                g_timers[i].tick = now + (g_timers[i].elapse ? g_timers[i].elapse : 1);
                PostMessageW(g_timers[i].hwnd, WM_TIMER, (WPARAM)g_timers[i].id, 0);
            }
        }
    }
}

static LPCWSTR u32_string_for_id(UINT id) {
    switch (id) {
    case 32826: return L"Applications";
    case 32827: return L"Processes";
    case 32828: return L"Performance";
    case 32829: return L"CPU Usage: %d%%";
    case 32830: return L"Processes: %d";
    case 32831: return L"Mem Usage: %dK";
    case 32832: return L"Image Name";
    case 32833: return L"PID";
    case 32834: return L"CPU";
    case 32835: return L"CPU Time";
    case 32836: return L"Mem Usage";
    case 32837: return L"Mem Delta";
    case 32838: return L"Peak Mem";
    case 32839: return L"Page Faults";
    case 32840: return L"USER Objects";
    case 32841: return L"I/O Reads";
    case 32842: return L"I/O Read Bytes";
    case 32843: return L"Session ID";
    case 32844: return L"User Name";
    case 32845: return L"PF Delta";
    case 32846: return L"VM Size";
    case 32847: return L"Paged Pool";
    case 32848: return L"Nonpaged Pool";
    case 32849: return L"Base Priority";
    case 32850: return L"Handles";
    case 32851: return L"Threads";
    case 32852: return L"GDI Objects";
    case 32853: return L"I/O Writes";
    case 32854: return L"I/O Write Bytes";
    case 32855: return L"I/O Other";
    case 32856: return L"I/O Other Bytes";
    case 32857: return L"Kernel Time";
    case 32858: return L"One Graph, All CPUs";
    case 32859: return L"One Graph Per CPU";
    case 32860: return L"Unable to access process";
    case 32861: return L"Warning";
    case 32862: return L"Unable to terminate process";
    case 32863: return L"Unable to change priority";
    case 32864: return L"Unable to debug process";
    case 32865: return L"ReactOS Task Manager";
    case 32816: return L"Large Icons";
    case 32817: return L"Small Icons";
    case 32818: return L"Details";
    case 32819: return L"Windows";
    case 32820: return L"Select Columns...";
    case 32821: return L"Show 16-bit tasks";
    case 32822: return L"CPU History";
    case 32825: return L"Show Kernel Times";
    case 110:   return L"Run";
    default:    return L"";
    }
}

int DllMain(void *h, uint32_t r, void *l) {
    (void)h; (void)r; (void)l;
    return 1;
}

ATOM RegisterClassW(const WNDCLASSW *lpWndClass) {
    U32_CLASS *cls;
    char class_name[64];
    if (!lpWndClass || !lpWndClass->lpszClassName) return 0;
    cls = u32_find_class(lpWndClass->lpszClassName);
    if (cls) return (ATOM)(cls - g_classes + 1);
    for (cls = g_classes; cls < g_classes + MAX_U32_CLASSES; cls++) {
        if (!cls->used) break;
    }
    if (cls >= g_classes + MAX_U32_CLASSES) return 0;
    memset(cls, 0, sizeof(*cls));
    cls->used = 1;
    cls->proc = lpWndClass->lpfnWndProc;
    cls->style = lpWndClass->style;
    cls->hIcon = lpWndClass->hIcon;
    cls->hIconSm = lpWndClass->hIcon;
    cls->hCursor = lpWndClass->hCursor;
    cls->hbrBackground = lpWndClass->hbrBackground;
    cls->hInstance = lpWndClass->hInstance;
    cls->menu_res_id = 0;
    cls->menu_name[0] = 0;
    if (lpWndClass->lpszMenuName) {
        if (u32_is_int_resource(lpWndClass->lpszMenuName)) cls->menu_res_id = u32_resource_id(lpWndClass->lpszMenuName);
        else u32_wstrcpy(cls->menu_name, lpWndClass->lpszMenuName, 64);
    }
    u32_wstrcpy(cls->name, lpWndClass->lpszClassName, 64);
    u32_wide_to_ansi(cls->name, class_name, 64);
    cls->win32k_class = Win32kRegisterClass(class_name, 0, u32_win32k_callback);
    return (ATOM)(cls - g_classes + 1);
}

ATOM RegisterClassExW(const WNDCLASSEXW *lpwcx) {
    WNDCLASSW wc;
    if (!lpwcx) return 0;
    memset(&wc, 0, sizeof(wc));
    wc.style = lpwcx->style;
    wc.lpfnWndProc = lpwcx->lpfnWndProc;
    wc.cbClsExtra = lpwcx->cbClsExtra;
    wc.cbWndExtra = lpwcx->cbWndExtra;
    wc.hInstance = lpwcx->hInstance;
    wc.hIcon = lpwcx->hIcon;
    wc.hCursor = lpwcx->hCursor;
    wc.hbrBackground = lpwcx->hbrBackground;
    wc.lpszMenuName = lpwcx->lpszMenuName;
    wc.lpszClassName = lpwcx->lpszClassName;
    {
        ATOM atom = RegisterClassW(&wc);
        U32_CLASS *cls = u32_find_class(lpwcx->lpszClassName);
        if (cls) {
            cls->hIcon = lpwcx->hIcon;
            cls->hIconSm = lpwcx->hIconSm ? lpwcx->hIconSm : lpwcx->hIcon;
            cls->hCursor = lpwcx->hCursor;
            cls->hbrBackground = lpwcx->hbrBackground;
            cls->hInstance = lpwcx->hInstance;
        }
        return atom;
    }
}

BOOL UnregisterClassW(LPCWSTR lpClassName, HINSTANCE hInstance) {
    U32_CLASS *cls = u32_find_class(lpClassName);
    int i;
    (void)hInstance;
    if (!cls) return FALSE;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].klass == cls) return FALSE;
    }
    memset(cls, 0, sizeof(*cls));
    return TRUE;
}

int GetClassNameW(HWND hWnd, LPWSTR lpClassName, int nMaxCount) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win || !win->klass || !lpClassName || nMaxCount <= 0) return 0;
    u32_wstrcpy(lpClassName, win->klass->name, nMaxCount);
    return u32_wstrlen(lpClassName);
}

BOOL GetClassInfoW(HINSTANCE hInstance, LPCWSTR lpClassName, LPWNDCLASSW lpWndClass) {
    U32_CLASS *cls = u32_find_class(lpClassName);
    (void)hInstance;
    if (!cls || !lpWndClass) return FALSE;
    memset(lpWndClass, 0, sizeof(*lpWndClass));
    lpWndClass->style = cls->style;
    lpWndClass->lpfnWndProc = cls->proc;
    lpWndClass->hInstance = cls->hInstance;
    lpWndClass->hIcon = cls->hIcon;
    lpWndClass->hCursor = cls->hCursor;
    lpWndClass->hbrBackground = cls->hbrBackground;
    lpWndClass->lpszMenuName = cls->menu_res_id ? MAKEINTRESOURCEW(cls->menu_res_id) : cls->menu_name;
    lpWndClass->lpszClassName = cls->name;
    return TRUE;
}

BOOL GetClassInfoExW(HINSTANCE hInstance, LPCWSTR lpClassName, LPWNDCLASSEXW lpwcx) {
    U32_CLASS *cls = u32_find_class(lpClassName);
    (void)hInstance;
    if (!cls || !lpwcx) return FALSE;
    memset(lpwcx, 0, sizeof(*lpwcx));
    lpwcx->cbSize = sizeof(*lpwcx);
    lpwcx->style = cls->style;
    lpwcx->lpfnWndProc = cls->proc;
    lpwcx->hInstance = cls->hInstance;
    lpwcx->hIcon = cls->hIcon;
    lpwcx->hCursor = cls->hCursor;
    lpwcx->hbrBackground = cls->hbrBackground;
    lpwcx->lpszMenuName = cls->menu_res_id ? MAKEINTRESOURCEW(cls->menu_res_id) : cls->menu_name;
    lpwcx->lpszClassName = cls->name;
    lpwcx->hIconSm = cls->hIconSm ? cls->hIconSm : cls->hIcon;
    return TRUE;
}

LRESULT DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    RECT line;
    int x = (short)(lParam & 0xFFFF);
    int y = (short)((lParam >> 16) & 0xFFFF);
    (void)wParam;
    if (Msg == WM_SYSCOMMAND) {
        UINT command = (UINT)wParam & 0xfff0;
        if (command == SC_MINIMIZE) ShowWindow(hWnd, SW_MINIMIZE);
        else if (command == SC_RESTORE) ShowWindow(hWnd, SW_RESTORE);
        return 0;
    } else if (Msg == WM_CLOSE) {
        if (win && win->popup_menu) u32_close_popup_chain(hWnd);
        if (win && win->dialog) {
            win->ended = 1;
            win->dialog_result = IDCANCEL;
        } else if (win) {
            DestroyWindow(hWnd);
        }
    } else if (win && Msg == WM_LBUTTONDOWN) {
        int scroll_bar, scroll_code;
        if (u32_scroll_hit(win, x, y, &scroll_bar, &scroll_code)) {
            if (scroll_code == SB_THUMBTRACK) {
                win->scroll_drag_bar = scroll_bar;
                win->scroll_drag_origin = scroll_bar == SB_VERT ? y : x;
                win->scroll_drag_start_pos = win->scroll_pos[u32_scroll_bar_index(scroll_bar)];
            } else {
                u32_scroll_command(win, scroll_bar, scroll_code);
            }
            return 0;
        }
        if (win->top_level && win->menu) {
            int menu_index = u32_menu_bar_hit_test(win, x, y);
            if (menu_index >= 0) {
                u32_open_menu_bar_popup(hWnd, menu_index);
                return 0;
            }
            if (win->popup_menu) {
                u32_close_popup_chain(hWnd);
                return 0;
            }
        }
        switch (win->ctrl_type) {
        case U32_CTRL_BUTTON:
            SerialPutString("[USER32] button down\r\n");
            win->pressed = 1;
            g_mouse_capture_window = hWnd;
            u32_mark_invalid(hWnd);
            return 0;
        case U32_CTRL_TAB:
            SerialPutString("[USER32] tab down\r\n");
            if (y >= 0 && y < 28) {
                int index = (x - 6) / 76;
                if (x >= 6 && index >= 0 && index < win->tab_count) {
                    SerialPutString("[USER32] tab setcurfocus\r\n");
                    /* A mouse click changes the selected page.  CURFOCUS is
                     * only the keyboard-focus operation; using it here made
                     * applications such as Task Manager lose their page
                     * selection during the next repaint. */
                    SendMessageW(hWnd, TCM_SETCURSEL, (WPARAM)index, 0);
                    return 0;
                }
            }
            break;
        case U32_CTRL_MENUPOPUP:
            {
                U32_MENU *menu = u32_lookup_menu(win->menu);
                int index = u32_popup_item_at(menu, y);
                win->hot_index = index;
                if (index >= 0 && menu && menu->items[index].submenu) {
                    u32_open_submenu(win->menu_owner, hWnd, index);
                } else if (win->popup_menu) {
                    u32_close_popup_chain(hWnd);
                }
                u32_mark_invalid(hWnd);
                return 0;
            }
        default:
            break;
        }
    } else if (win && Msg == WM_MOUSEMOVE) {
        if (win->scroll_drag_bar) {
            int bar = win->scroll_drag_bar;
            int i = u32_scroll_bar_index(bar);
            RECT srect;
            int start, length, track, thumb, travel, delta, range;
            GetClientRect(hWnd, &srect);
            u32_scroll_geometry(win, &srect, bar, &start, &length);
            track = length - U32_SCROLLBAR_SIZE * 2;
            thumb = ((int)win->scroll_page[i] * track) /
                    (win->scroll_max[i] - win->scroll_min[i] + 1);
            if (thumb < 8) thumb = 8; if (thumb > track) thumb = track;
            travel = track - thumb;
            delta = (bar == SB_VERT ? y : x) - win->scroll_drag_origin;
            range = u32_scroll_limit(win, bar) - win->scroll_min[i];
            if (travel > 0) win->scroll_pos[i] = win->scroll_drag_start_pos + delta * range / travel;
            u32_scroll_clamp(win, bar);
            SendMessageW(hWnd, bar == SB_VERT ? WM_VSCROLL : WM_HSCROLL,
                         MAKEWPARAM(SB_THUMBTRACK, win->scroll_pos[i]), 0);
            u32_mark_invalid(hWnd);
            return 0;
        }
        if (win->ctrl_type == U32_CTRL_MENUPOPUP) {
            U32_MENU *menu = u32_lookup_menu(win->menu);
            int index = u32_popup_item_at(menu, y);
            if (win->hot_index != index) {
                win->hot_index = index;
                if (index >= 0 && menu && menu->items[index].submenu) {
                    u32_open_submenu(win->menu_owner, hWnd, index);
                } else if (win->popup_menu) {
                    u32_close_popup_chain(hWnd);
                }
                u32_mark_invalid(hWnd);
            }
            return 0;
        }
    } else if (win && Msg == WM_LBUTTONUP) {
        /* Some applications receive the release after the native window
         * manager has completed its click dispatch.  Treat the tab header as
         * a committed selection on release as well, so selection cannot be
         * lost merely because the down event was consumed by the parent. */
        if (win->ctrl_type == U32_CTRL_TAB && y >= 0 && y < 40) {
            int index = (x - 6) / 76;
            if (x >= 6 && index >= 0 && index < win->tab_count) {
                SendMessageW(hWnd, TCM_SETCURSEL, (WPARAM)index, 0);
                return 0;
            }
        }
        if (win->scroll_drag_bar) {
            int bar = win->scroll_drag_bar;
            int i = u32_scroll_bar_index(bar);
            SendMessageW(hWnd, bar == SB_VERT ? WM_VSCROLL : WM_HSCROLL,
                         MAKEWPARAM(SB_THUMBPOSITION, win->scroll_pos[i]), 0);
            SendMessageW(hWnd, bar == SB_VERT ? WM_VSCROLL : WM_HSCROLL,
                         MAKEWPARAM(SB_ENDSCROLL, 0), 0);
            win->scroll_drag_bar = 0;
            u32_mark_invalid(hWnd);
            return 0;
        }
        switch (win->ctrl_type) {
        case U32_CTRL_BUTTON:
            if (win->pressed) {
                SerialPutString("[USER32] button up\r\n");
                win->pressed = 0;
                if (g_mouse_capture_window == hWnd)
                    g_mouse_capture_window = NULL;
                GetClientRect(hWnd, &rc);
                if (x >= 0 && y >= 0 && x < rc.right && y < rc.bottom && win->parent) {
                    if ((win->style & 0x0f) == BS_AUTOCHECKBOX) {
                        win->check_state = (win->check_state == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED;
                    }
                    SerialPutString("[USER32] button command sent\r\n");
                    SendMessageW(win->parent, WM_COMMAND, MAKEWPARAM(win->id, BN_CLICKED), (LPARAM)hWnd);
                }
                u32_mark_invalid(hWnd);
            }
            return 0;
        case U32_CTRL_MENUPOPUP:
            {
                U32_MENU *menu = u32_lookup_menu(win->menu);
                int index = u32_popup_item_at(menu, y);
                if (menu && index >= 0 && index < menu->count) {
                    U32_MENU_ITEM *item = &menu->items[index];
                    if (!(item->flags & MF_SEPARATOR)) {
                        if (item->submenu) {
                            u32_open_submenu(win->menu_owner, hWnd, index);
                        } else if (win->menu_owner) {
                            HWND root = u32_find_menu_root(win->menu_owner);
                            if (menu->style & MNS_NOTIFYBYPOS)
                                SendMessageW(win->menu_owner, WM_MENUCOMMAND, index, (LPARAM)win->menu);
                            else
                                SendMessageW(win->menu_owner, WM_COMMAND, item->id, 0);
                            u32_close_popup_chain(root);
                        }
                    }
                }
                return 0;
            }
        default:
            break;
        }
    } else if (win && Msg == WM_MOUSEWHEEL) {
        int delta = (short)((wParam >> 16) & 0xFFFF);
        if (win->scroll_visible[1]) u32_scroll_command(win, SB_VERT, delta > 0 ? SB_LINEUP : SB_LINEDOWN);
        return 0;
    } else if (Msg == WM_PAINT && win) {
        if (win->painting) return 0;
        win->painting = 1;
        u32_update_scroll_content(win);
        hdc = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rc);
        switch (win->ctrl_type) {
        case U32_CTRL_DIALOG:
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
            if (win->top_level && win->menu) {
                u32_draw_menu_bar(hdc, win, &rc);
            }
            break;
        case U32_CTRL_MENUPOPUP:
            u32_draw_menu_popup(hdc, win, &rc);
            break;
        case U32_CTRL_TAB:
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
            Rectangle(hdc, 0, 18, rc.right, rc.bottom);
            if (win->tab_count > 0) {
                int i;
                for (i = 0; i < win->tab_count; i++) {
                    int tx = 6 + (i * 76);
                    RECT tab_rc;
                    tab_rc.left = tx + 1;
                    tab_rc.top = 1;
                    tab_rc.right = tx + 71;
                    tab_rc.bottom = (i == win->tab_cur_sel) ? 19 : 17;
                    FillRect(hdc, &tab_rc, (HBRUSH)GetStockObject(0));
                    Rectangle(hdc, tx, 0, tx + 72, (i == win->tab_cur_sel) ? 20 : 18);
                    if (win->tab_text[i][0]) TextOutW(hdc, tx + 6, 5, win->tab_text[i], -1);
                }
            }
            break;
        case U32_CTRL_LISTVIEW:
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
            Rectangle(hdc, 0, 0, rc.right, rc.bottom);
            if (win->header_count > 0) {
                int i, x = 0;
                U32_LISTVIEW_DATA *data = (U32_LISTVIEW_DATA*)win->listview_data;
                int row_top = 16;
                int row_height = 14;
                for (i = 0; i < win->header_count; i++) {
                    int colw = rc.right / (win->header_count ? win->header_count : 1);
                    if (colw < 40) colw = 40;
                    Rectangle(hdc, x, 0, x + colw, 16);
                    if (data) TextOutW(hdc, x + 2, 3, data->columns[i], -1);
                    x += colw;
                    if (x >= rc.right) break;
                }
                if (data) {
                    int row;
                    for (row = 0; row < win->listview_item_count && row < 10; row++) {
                        int col_x = 0;
                        int y = row_top + (row * row_height);
                        if (y + row_height > rc.bottom) break;
                        for (i = 0; i < win->header_count; i++) {
                            int colw = rc.right / (win->header_count ? win->header_count : 1);
                            if (colw < 40) colw = 40;
                            Rectangle(hdc, col_x, y, col_x + colw, y + row_height);
                            if (data->items[row][i][0]) TextOutW(hdc, col_x + 2, y + 3, data->items[row][i], -1);
                            col_x += colw;
                            if (col_x >= rc.right) break;
                        }
                    }
                }
            }
            break;
        case U32_CTRL_GROUPBOX:
            line.left = 0; line.top = 8; line.right = rc.right; line.bottom = rc.bottom;
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
            Rectangle(hdc, 0, 8, rc.right, rc.bottom);
            if (win->title[0]) TextOutW(hdc, 8, 0, win->title, -1);
            break;
        case U32_CTRL_BUTTON:
            {
                RECT face = rc;
                HBRUSH face_brush = (HBRUSH)GetStockObject(0);
                HPEN light_pen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
                HPEN dark_pen = CreatePen(PS_SOLID, 1, RGB(64, 64, 64));
                HPEN black_pen = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
                FillRect(hdc, &face, face_brush);
                if (win->pressed) {
                    if (dark_pen) SelectObject(hdc, dark_pen);
                    Rectangle(hdc, 0, 0, rc.right, rc.bottom);
                    if (light_pen) SelectObject(hdc, light_pen);
                    MoveToEx(hdc, 1, 1, 0); LineTo(hdc, rc.right - 1, 1);
                    MoveToEx(hdc, 1, 1, 0); LineTo(hdc, 1, rc.bottom - 1);
                } else {
                    if (light_pen) SelectObject(hdc, light_pen);
                    MoveToEx(hdc, 0, rc.bottom - 1, 0); LineTo(hdc, 0, 0);
                    LineTo(hdc, rc.right - 1, 0);
                    if (dark_pen) SelectObject(hdc, dark_pen);
                    MoveToEx(hdc, 1, rc.bottom - 1, 0); LineTo(hdc, rc.right - 1, rc.bottom - 1);
                    MoveToEx(hdc, rc.right - 1, 1, 0); LineTo(hdc, rc.right - 1, rc.bottom - 1);
                    if (black_pen) SelectObject(hdc, black_pen);
                    Rectangle(hdc, 1, 1, rc.right - 1, rc.bottom - 1);
                }
                if (light_pen) DeleteObject(light_pen);
                if (dark_pen) DeleteObject(dark_pen);
                if (black_pen) DeleteObject(black_pen);
            }
            if ((win->style & 0x0f) == BS_AUTOCHECKBOX) {
                Rectangle(hdc, 1, 1, 11, 11);
                if (win->check_state == BST_CHECKED) {
                    MoveToEx(hdc, 3, 6, 0);
                    LineTo(hdc, 5, 8);
                    LineTo(hdc, 9, 3);
                }
            }
            if (win->title[0]) TextOutW(hdc,
                                        ((win->style & 0x0f) == BS_AUTOCHECKBOX ? 14 : 4) + (win->pressed ? 1 : 0),
                                        ((rc.bottom > 12) ? ((rc.bottom - 8) / 2) - 2 : 0) + (win->pressed ? 1 : 0),
                                        win->title, -1);
            break;
        case U32_CTRL_STATIC:
            if (win->title[0]) TextOutW(hdc, 0, 0, win->title, -1);
            break;
        case U32_CTRL_EDIT:
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
            Rectangle(hdc, 0, 0, rc.right, rc.bottom);
            if (win->edit_text) {
                int i, line = 0, col = 0, first_line = win->scroll_pos[1] / 10;
                int first_col = win->scroll_pos[0] / 8;
                WCHAR text_line[128];
                for (i = 0; i <= win->edit_len; i++) {
                    WCHAR ch = (i < win->edit_len) ? win->edit_text[i] : 0;
                    if (ch == L'\n' || ch == 0) {
                        if (line >= first_line && line * 10 - win->scroll_pos[1] < rc.bottom) {
                            if (first_col < col || col == 0) {
                                int k = 0, p = 0;
                                while (p < col && k < 127) {
                                    if (p++ >= first_col) text_line[k++] = win->edit_text[i - col + p - 1];
                                }
                                text_line[k] = 0;
                                /* Leave one pixel for the edit border.  The
                                 * old y=0 placement put the 8px glyph row
                                 * exactly under the border on small native
                                 * controls, making valid values invisible. */
                                TextOutW(hdc, 2, 1 + line * 10 - win->scroll_pos[1], text_line, -1);
                            }
                        }
                        line++; col = 0;
                    } else {
                        col++;
                    }
                }
            } else if (win->title[0]) TextOutW(hdc, 2, 0, win->title, -1);
            break;
        case U32_CTRL_COMBO:
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
            Rectangle(hdc, 0, 0, rc.right, rc.bottom);
            MoveToEx(hdc, rc.right - 14, 1, 0);
            LineTo(hdc, rc.right - 14, rc.bottom - 1);
            MoveToEx(hdc, rc.right - 10, 5, 0);
            LineTo(hdc, rc.right - 5, 5);
            LineTo(hdc, rc.right - 8, 9);
            LineTo(hdc, rc.right - 10, 5);
            if (win->title[0]) TextOutW(hdc, 3, 2, win->title, -1);
            break;
        case U32_CTRL_STATUS:
            {
                int i;
                int left = 0;
                FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
                Rectangle(hdc, 0, 0, rc.right, rc.bottom);
                if (win->status_parts_count <= 0) {
                    if (win->title[0]) TextOutW(hdc, 2, 2, win->title, -1);
                } else {
                    for (i = 0; i < win->status_parts_count && i < 8; i++) {
                        int right = win->status_part_right[i];
                        if (right <= left || right > rc.right) right = rc.right;
                        Rectangle(hdc, left, 0, right, rc.bottom);
                        if (win->status_text[i][0]) TextOutW(hdc, left + 2, 2, win->status_text[i], -1);
                        left = right;
                    }
                }
            }
            break;
        default:
            break;
        }
        u32_draw_scrollbars(hdc, win, &rc);
        EndPaint(hWnd, &ps);
        win->painting = 0;
        return 0;
    }
    return 0;
}

BOOL GetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
    DWORD current_pid;
    U32_QUIT_STATE *quit_state;
    if (!lpMsg) return FALSE;
    current_pid = u32_current_process_id();
    quit_state = u32_get_quit_state(current_pid, 1);
    for (;;) {
        int invalid_index;

        if (u32_dequeue_message(lpMsg, hWnd, wMsgFilterMin, wMsgFilterMax, TRUE)) {
            if (lpMsg->message == WM_QUIT) return FALSE;
            return TRUE;
        }

        u32_pump_timers(NULL);

        invalid_index = u32_find_invalid_window(hWnd);
        if (invalid_index >= 0) {
            lpMsg->hwnd = g_windows[invalid_index].hwnd;
            lpMsg->message = WM_PAINT;
            lpMsg->wParam = 0;
            lpMsg->lParam = 0;
            lpMsg->time = GetTickCount();
            lpMsg->pt.x = 0;
            lpMsg->pt.y = 0;
            return TRUE;
        }

        if (quit_state && quit_state->exit_requested) {
            lpMsg->hwnd = NULL;
            lpMsg->message = WM_QUIT;
            lpMsg->wParam = (WPARAM)quit_state->exit_code;
            lpMsg->lParam = 0;
            lpMsg->time = GetTickCount();
            lpMsg->pt.x = 0;
            lpMsg->pt.y = 0;
            return FALSE;
        }

        KeYield();
    }
}

LRESULT DispatchMessageW(const MSG *lpMsg) {
    if (!lpMsg) return 0;
    return SendMessageW(lpMsg->hwnd, lpMsg->message, lpMsg->wParam, lpMsg->lParam);
}

BOOL TranslateMessage(const MSG *lpMsg) {
    if (!lpMsg) return FALSE;
    if (lpMsg->message == WM_KEYDOWN) {
        WPARAM ch = lpMsg->wParam;
        if (ch == 8 || ch == 9 || ch == 13 || ch >= 32) {
            PostMessageW(lpMsg->hwnd, WM_CHAR, ch, lpMsg->lParam);
        }
    }
    return TRUE;
}

int ShowWindow(HWND hWnd, int nCmdShow) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return FALSE;
    if (win->top_level) {
        win->visible = nCmdShow == SW_HIDE ? 0 : 1;
        Win32kSetWindowShowState((HANDLE)hWnd, nCmdShow);
        return TRUE;
    }
    if (nCmdShow == SW_HIDE) {
        win->visible = 0;
        if (win->parent) u32_mark_invalid(win->parent);
        return TRUE;
    }
    win->visible = 1;
    u32_mark_invalid_descendants(hWnd);
    u32_mark_invalid(hWnd);
    if (win->parent) u32_mark_invalid(win->parent);
    return TRUE;
}

BOOL UpdateWindow(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return FALSE;
    if (win->painting) return TRUE;
    if (win->top_level) {
        Win32kUpdateWindow((HANDLE)hWnd);
        u32_paint_children(hWnd);
    } else {
        SendMessageW(hWnd, WM_PAINT, 0, 0);
        u32_paint_children(hWnd);
    }
    return TRUE;
}

void PostQuitMessage(int nExitCode) {
    DWORD current_pid = u32_current_process_id();
    U32_QUIT_STATE *quit_state = u32_get_quit_state(current_pid, 1);
    if (quit_state) {
        quit_state->exit_requested = 1;
        quit_state->exit_code = nExitCode;
    }
    u32_enqueue_message(NULL, WM_QUIT, (WPARAM)nExitCode, 0);
}

int MessageBoxW(HWND hWnd, LPCWSTR lpText, LPCWSTR lpCaption, UINT uType) {
    (void)hWnd; (void)lpText; (void)lpCaption; (void)uType;
    return IDOK;
}

int MessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType) {
    (void)hWnd; (void)lpText; (void)lpCaption; (void)uType;
    return IDOK;
}

HWND CreateWindowExW(DWORD dwExStyle, LPCWSTR lpClassName, LPCWSTR lpWindowName,
                     DWORD dwStyle, int X, int Y, int nWidth, int nHeight,
                     HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    U32_CLASS *cls;
    U32_WINDOW *win;
    char class_name[64];
    char title[128];
    LPCWSTR resolved_class_name = lpClassName;
    (void)hInstance;
    (void)lpParam;

    cls = u32_find_class(lpClassName);
    if (!cls) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof(wc));
        if (u32_is_int_resource(lpClassName)) {
            /* Wine Explorer uses the predefined desktop class atom.  Give it
               a named local class with DefWindowProc so GWLP_WNDPROC
               subclassing returns a callable original procedure. */
            if (u32_resource_id(lpClassName) != 32769) return NULL;
            resolved_class_name = L"Desktop";
            wc.lpfnWndProc = DefWindowProcW;
        }
        wc.lpszClassName = resolved_class_name;
        RegisterClassW(&wc);
        cls = u32_find_class(resolved_class_name);
        if (!cls) return NULL;
    }

    win = u32_alloc_window();
    if (!win) return NULL;

    /* CW_USEDEFAULT is a request for placement, not a screen coordinate.
       Passing its 0x80000000 sentinel through to win32k leaves otherwise
       valid top-level windows (notably Explorer) far outside the desktop. */
    if (!(hWndParent && (dwStyle & WS_CHILD))) {
        int cascade = (int)(win - g_windows) & 7;
        if (X == CW_USEDEFAULT) X = 24 + cascade * 24;
        if (Y == CW_USEDEFAULT) Y = 24 + cascade * 24;
        if (nWidth == CW_USEDEFAULT || nWidth <= 0) nWidth = 480;
        if (nHeight == CW_USEDEFAULT || nHeight <= 0) nHeight = 320;
    }
    u32_wstrcpy(win->title, lpWindowName, 128);
    win->parent = hWndParent;
    win->owner = hWndParent;
    win->style = dwStyle;
    win->exstyle = dwExStyle;
    win->menu = hMenu;
    win->enabled = TRUE;
    win->visible = (dwStyle & WS_VISIBLE) ? TRUE : FALSE;
    win->owner_pid = hWndParent ? (u32_lookup_window(hWndParent) ? u32_lookup_window(hWndParent)->owner_pid : u32_current_process_id())
                                : u32_current_process_id();
    win->klass = cls;
    win->proc = cls->proc;
    win->hIcon = cls->hIcon;
    win->hIconSm = cls->hIconSm ? cls->hIconSm : cls->hIcon;
    win->dialog = FALSE;
    win->ctrl_type = u32_pick_ctrl_type(cls->name, dwStyle);
    u32_scroll_init(win);
    u32_set_rect(win, X, Y, nWidth, nHeight);

    if (hWndParent && (dwStyle & WS_CHILD)) {
        win->top_level = 0;
        win->hwnd = (HWND)win;
        win->id = (UINT)(UINT_PTR)hMenu;
    } else {
        win->top_level = 1;
        if (!win->menu) {
            if (cls->menu_res_id) win->menu = LoadMenuW(cls->hInstance, MAKEINTRESOURCEW(cls->menu_res_id));
            else if (cls->menu_name[0]) win->menu = LoadMenuW(cls->hInstance, cls->menu_name);
        }
        u32_wide_to_ansi(cls->name, class_name, 64);
        u32_wide_to_ansi(win->title, title, 128);
        win->hwnd = (HWND)Win32kCreateWindow(class_name, title, X, Y, nWidth, nHeight, dwStyle);
        if (!win->hwnd) {
            win->used = 0;
            return NULL;
        }
        Win32kSetWindowIcons((HANDLE)win->hwnd, win->hIcon, win->hIconSm);
    }

    SendMessageW(win->hwnd, WM_CREATE, 0, 0);
    SendMessageW(win->hwnd, WM_SIZE, SIZE_RESTORED,
                 MAKELPARAM(win->rect.right - win->rect.left, win->rect.bottom - win->rect.top));
    if (win->top_level) u32_notify_desktop_window(win->hwnd, WM_CREATE);
    if (win->visible && win->top_level) {
        /* A newly shown application window receives the foreground once at
         * creation time.  Subsequent repaint/refresh operations must not
         * reactivate it. */
        Win32kActivateWindow((HANDLE)win->hwnd);
        Win32kShowWindow((HANDLE)win->hwnd);
        Win32kRedrawAll();
    }
    return win->hwnd;
}

HWND CreateDialogW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc) {
    U32_WINDOW *win;
    UINT tmpl_id = u32_is_int_resource(lpTemplate) ? u32_resource_id(lpTemplate) : 0;
    const U32_DIALOG_TEMPLATE_DEF *tmpl = u32_find_builtin_dialog(tmpl_id);
    HWND actual_parent = hWndParent;
    const WCHAR *caption = L"Dialog";
    DWORD style = WS_CHILD | WS_VISIBLE;
    int width = 247;
    int height = 210;
    int child_x = 15;
    int child_y = 30;
    if (tmpl && tmpl->attach_to_tab_of_parent && hWndParent) {
        HWND tab = GetDlgItem(hWndParent, 1015);
        if (tab) {
            RECT tab_client;
            actual_parent = tab;
            /* These are tab pages, not independent dialogs.  Position them
             * in the tab content area and keep them inside the tab control. */
            child_x = 0;
            child_y = 20;
            if (GetClientRect(tab, &tab_client)) {
                width = tab_client.right;
                height = tab_client.bottom - child_y;
                if (height < 1) height = 1;
            }
        }
    }
    if (tmpl) {
        if (tmpl->caption) caption = tmpl->caption;
        style = tmpl->style;
        width = tmpl->width;
        height = tmpl->height;
    }
    if (tmpl && tmpl->attach_to_tab_of_parent && actual_parent) {
        RECT tab_client;
        child_x = 0;
        child_y = 20;
        if (GetClientRect(actual_parent, &tab_client)) {
            width = tab_client.right;
            height = tab_client.bottom - child_y;
            if (height < 1) height = 1;
        }
    }
    HWND hwnd = CreateWindowExW(0, L"#32770", caption, style, child_x, child_y,
                                width, height,
                                actual_parent, 0, hInstance, NULL);
    win = u32_lookup_window(hwnd);
    if (!win) return NULL;
    win->dialog = TRUE;
    win->dlgproc = lpDialogFunc;
    /* The real Task Manager dialog carries IDR_TASKMANAGER in its dialog
     * resource.  Our compact builtin template has to attach that resource
     * explicitly or only menus added later (such as Windows) are visible. */
    if (tmpl_id == 102) {
        win->menu = LoadMenuW(hInstance, MAKEINTRESOURCEW(130));
        if (win->menu) u32_mark_invalid(hwnd);
    }
    u32_create_builtin_dialog_children(tmpl, hwnd);
    SendMessageW(hwnd, WM_INITDIALOG, 0, 0);
    /* Controls and child pages are created during WM_INITDIALOG.  Give the
     * dialog one layout notification after initialization as Win32 does;
     * otherwise pages remain at their small template fallback size. */
    {
        RECT client;
        GetClientRect(hwnd, &client);
        SendMessageW(hwnd, WM_SIZE, SIZE_RESTORED,
                     MAKELPARAM(client.right, client.bottom));
    }
    UpdateWindow(hwnd);
    return hwnd;
}

HWND CreateDialogIndirectParamW(HINSTANCE hInstance, LPCVOID lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    (void)lpTemplate;
    (void)dwInitParam;
    return CreateDialogW(hInstance, MAKEINTRESOURCEW(0), hWndParent, lpDialogFunc);
}

HWND CreateDialogParamW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    (void)dwInitParam;
    return CreateDialogW(hInstance, lpTemplate, hWndParent, lpDialogFunc);
}

HWND CreateDialogParamA(HINSTANCE hInstance, LPCSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    (void)dwInitParam;
    return CreateDialogW(hInstance, (ULONG_PTR)lpTemplate <= 0xFFFFu ? (LPCWSTR)lpTemplate : MAKEINTRESOURCEW(0), hWndParent, lpDialogFunc);
}

static INT_PTR u32_dialog_box_w(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent,
                                DLGPROC lpDialogFunc, LPARAM init_param) {
    HWND hwnd;
    U32_WINDOW *win;
    UINT tmpl_id = u32_is_int_resource(lpTemplate) ? u32_resource_id(lpTemplate) : 0;
    const U32_DIALOG_TEMPLATE_DEF *tmpl = u32_find_builtin_dialog(tmpl_id);
    const WCHAR *caption = L"Dialog";
    DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    int width = 540;
    int height = 420;
    SerialPutString("[USER32] DialogBoxW begin\r\n");
    if (tmpl) {
        if (tmpl->caption) caption = tmpl->caption;
        style = tmpl->style;
        width = tmpl->width;
        height = tmpl->height;
    }
    hwnd = CreateWindowExW(0, L"#32770", caption, style,
                           40, 40, width, height, hWndParent, 0, hInstance, NULL);
    if (!hwnd) {
        SerialPutString("[USER32] DialogBoxW create failed\r\n");
        return 0;
    }
    win = u32_lookup_window(hwnd);
    if (!win) {
        SerialPutString("[USER32] DialogBoxW lookup failed\r\n");
        return 0;
    }
    win->dialog = TRUE;
    win->dlgproc = lpDialogFunc;
    u32_create_builtin_dialog_children(tmpl, hwnd);
    SerialPutString("[USER32] DialogBoxW init\r\n");
    SendMessageW(hwnd, WM_INITDIALOG, 0, init_param);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SerialPutString("[USER32] DialogBoxW loop\r\n");
    while (!win->ended) {
        MSG msg;

        /* A modal dialog still owns a normal GUI thread.  The old loop only
         * repainted and yielded, which meant that keyboard/mouse messages
         * queued for the dialog's child controls were never dispatched.  In
         * practice that made MSGINA look frozen immediately after it opened.
         * GetMessage(NULL, ...) is intentional: edit/button messages target
         * children, while the dialog procedure receives the command routed
         * back from those children. */
        if (!GetMessageW(&msg, NULL, 0, 0)) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    /* EndDialog ends a modal dialog and removes its window.  Keeping the
     * window object visible here leaves the logon dialog painted over the
     * newly-created shell, even though the dialog procedure has returned. */
    {
        INT_PTR result = win->dialog_result;
        DestroyWindow(hwnd);
        SerialPutString("[USER32] DialogBoxW end\r\n");
        return result;
    }
}

INT_PTR DialogBoxW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc) {
    return u32_dialog_box_w(hInstance, lpTemplate, hWndParent, lpDialogFunc, 0);
}

INT_PTR DialogBoxIndirectParamW(HINSTANCE hInstance, LPCVOID lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    return u32_dialog_box_w(hInstance, (LPCWSTR)lpTemplate, hWndParent, lpDialogFunc, dwInitParam);
}

INT_PTR DialogBoxParamW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    return u32_dialog_box_w(hInstance, lpTemplate, hWndParent, lpDialogFunc, dwInitParam);
}

INT_PTR DialogBoxParamA(HINSTANCE hInstance, LPCSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam) {
    (void)dwInitParam;
    return DialogBoxW(hInstance, (ULONG_PTR)lpTemplate <= 0xFFFFu ? (LPCWSTR)lpTemplate : MAKEINTRESOURCEW(0), hWndParent, lpDialogFunc);
}

LRESULT DefDlgProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    switch (Msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) {
            EndDialog(hWnd, LOWORD(wParam));
            return TRUE;
        }
        break;
    case WM_CLOSE:
        EndDialog(hWnd, IDCANCEL);
        return TRUE;
    default:
        break;
    }
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}

static void u32_lv_copy_out(WCHAR *dst, int max_chars, const WCHAR *src) {
    int i = 0;
    if (!dst || max_chars <= 0) return;
    if (!src) {
        dst[0] = 0;
        return;
    }
    while (src[i] && i < max_chars - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

LRESULT SendMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    U32_LISTVIEW_DATA *lv;
    if (!win) return 0;

    if ((Msg == WM_KEYDOWN || Msg == WM_KEYUP || Msg == WM_CHAR) &&
        g_focus && g_focus != hWnd && u32_is_descendant(hWnd, g_focus)) {
        return SendMessageW(g_focus, Msg, wParam, lParam);
    }

    if (Msg == WM_MOUSEMOVE || Msg == WM_LBUTTONDOWN || Msg == WM_LBUTTONUP || Msg == WM_MOUSEWHEEL) {
        int x = (short)(lParam & 0xFFFF);
        int y = (short)((lParam >> 16) & 0xFFFF);
        HWND child;
        if (g_mouse_capture_window &&
            (Msg == WM_MOUSEMOVE || Msg == WM_LBUTTONUP) &&
            u32_is_descendant(hWnd, g_mouse_capture_window))
            child = g_mouse_capture_window;
        else
            child = win->top_level ? u32_route_mouse_target(hWnd, x, y) : u32_hit_test_child(hWnd, x, y);
        if (child && child != hWnd) {
            U32_WINDOW *child_win = u32_lookup_window(child);
            if (Msg == WM_LBUTTONDOWN) {
                SetFocus(child);
            }
            if (child_win) {
                int child_left = 0;
                int child_top = 0;
                if (!u32_get_descendant_pos_in_ancestor_client(hWnd, child, &child_left, &child_top)) {
                    child_left = child_win->rect.left;
                    child_top = child_win->rect.top;
                }
                int child_x = x - child_left;
                int child_y = y - child_top;
                return SendMessageW(child, Msg, wParam, MAKELPARAM(child_x, child_y));
            }
        }
    }

    if (Msg == WM_PAINT) {
        u32_clear_invalid(hWnd);
    }

    switch (Msg) {
    case WM_ENABLE:
        win->enabled = wParam ? TRUE : FALSE;
        u32_mark_invalid(hWnd);
        return 0;
    case WM_SHOWWINDOW:
        win->visible = wParam ? TRUE : FALSE;
        u32_mark_invalid(hWnd);
        return 0;
    case WM_SETFONT:
        win->hFont = (HFONT)wParam;
        if (lParam) u32_mark_invalid(hWnd);
        return 0;
    case WM_GETFONT:
        return (LRESULT)win->hFont;
    case WM_GETDLGCODE:
        switch (win->ctrl_type) {
        case U32_CTRL_EDIT:
            return DLGC_WANTCHARS | DLGC_HASSETSEL;
        case U32_CTRL_COMBO:
            return DLGC_WANTCHARS;
        case U32_CTRL_BUTTON:
            return DLGC_BUTTON;
        case U32_CTRL_GROUPBOX:
        case U32_CTRL_STATIC:
            return DLGC_STATIC;
        case U32_CTRL_TAB:
            return DLGC_WANTARROWS;
        default:
            break;
        }
        return 0;
    case WM_SETTEXT:
        if (win->ctrl_type == U32_CTRL_EDIT) {
            u32_edit_set_text(win, (LPCWSTR)lParam);
            u32_mark_invalid(hWnd);
            return TRUE;
        }
        if ((LPCWSTR)lParam) u32_wstrcpy(win->title, (LPCWSTR)lParam, 128);
        else win->title[0] = 0;
        u32_mark_invalid(hWnd);
        return TRUE;
    case WM_GETTEXT:
        {
            LPWSTR out = (LPWSTR)lParam;
            int max = (int)wParam;
            if (!out || max <= 0) return 0;
            if (win->ctrl_type == U32_CTRL_EDIT && win->edit_text) {
                u32_wstrcpy(out, win->edit_text, max);
            } else {
                u32_wstrcpy(out, win->title, max);
            }
            return u32_wstrlen(out);
        }
    case WM_GETTEXTLENGTH:
        if (win->ctrl_type == U32_CTRL_EDIT) return (LRESULT)win->edit_len;
        return (LRESULT)u32_wstrlen(win->title);
    case WM_CHAR:
        if (win->ctrl_type == U32_CTRL_EDIT) {
            WCHAR chbuf[2];
            if ((WCHAR)wParam == 8) {
                if (win->edit_sel_start == win->edit_sel_end && win->edit_sel_start > 0) {
                    win->edit_sel_start--;
                }
                u32_edit_replace_sel(win, L"");
                return 0;
            }
            chbuf[0] = (WCHAR)wParam;
            chbuf[1] = 0;
            u32_edit_replace_sel(win, chbuf);
            return 0;
        }
        break;
    case WM_SETICON:
        {
            HICON old_icon;
            if (wParam == ICON_SMALL) {
                old_icon = win->hIconSm;
                win->hIconSm = (HICON)lParam;
            } else {
                old_icon = win->hIcon;
                win->hIcon = (HICON)lParam;
            }
            if (win->top_level) {
                Win32kSetWindowIcons((HANDLE)hWnd, win->hIcon, win->hIconSm ? win->hIconSm : win->hIcon);
                Win32kRedrawAll();
            }
            u32_mark_invalid(hWnd);
            return (LRESULT)old_icon;
        }
    case WM_GETICON:
        if (wParam == ICON_SMALL) return (LRESULT)(win->hIconSm ? win->hIconSm : win->hIcon);
        return (LRESULT)win->hIcon;
    case DM_SETDEFID:
    case WM_SETREDRAW:
        return 0;
    case BM_SETCHECK:
        win->check_state = (int)wParam;
        u32_mark_invalid(hWnd);
        return 0;
    case BM_GETCHECK:
        return (LRESULT)win->check_state;
    case CB_RESETCONTENT:
        if (win->ctrl_type == U32_CTRL_COMBO) {
            win->title[0] = 0;
            u32_mark_invalid(hWnd);
            return 0;
        }
        return CB_ERR;
    case CB_ADDSTRING:
        if (win->ctrl_type == U32_CTRL_COMBO) {
            if (!win->title[0] && lParam) u32_wstrcpy(win->title, (LPCWSTR)lParam, 128);
            u32_mark_invalid(hWnd);
            return 0;
        }
        return CB_ERR;
    case CB_FINDSTRINGEXACT:
        if (win->ctrl_type == U32_CTRL_COMBO && lParam &&
            u32_wstrcmp(win->title, (LPCWSTR)lParam) == 0) return 0;
        return CB_ERR;
    case CB_SETCURSEL:
        if (win->ctrl_type == U32_CTRL_COMBO) {
            u32_mark_invalid(hWnd);
            return wParam == (WPARAM)-1 ? CB_ERR : 0;
        }
        return CB_ERR;
    case BM_CLICK:
        SendMessageW(hWnd, WM_LBUTTONDOWN, 0, MAKELPARAM(1, 1));
        SendMessageW(hWnd, WM_LBUTTONUP, 0, MAKELPARAM(1, 1));
        return 0;
    case TCM_INSERTITEMW:
        if (win->tab_count < MAX_TAB_ITEMS) {
            const TCITEMW *item = (const TCITEMW*)lParam;
            int index = win->tab_count++;
            if (item && (item->mask & TCIF_TEXT) && item->pszText) {
                u32_wstrcpy(win->tab_text[index], item->pszText, 64);
            } else {
                win->tab_text[index][0] = 0;
            }
            u32_mark_invalid(hWnd);
            return (LRESULT)index;
        }
        return -1;
    case TCM_GETITEMW:
        {
            TCITEMW *item = (TCITEMW*)lParam;
            int index = (int)wParam;
            if (!item || index < 0 || index >= win->tab_count) return FALSE;
            if ((item->mask & TCIF_TEXT) && item->pszText && item->cchTextMax > 0) {
                u32_wstrcpy(item->pszText, win->tab_text[index], item->cchTextMax);
            }
            return TRUE;
        }
    case TCM_SETCURSEL:
    case TCM_SETCURFOCUS:
        if (win->tab_cur_sel != (int)wParam) {
            SerialPutString("[USER32] TCM_SETCURFOCUS\r\n");
            win->tab_cur_sel = (int)wParam;
            if (win->parent) {
                NMHDR hdr;
                memset(&hdr, 0, sizeof(hdr));
                hdr.hwndFrom = hWnd;
                hdr.idFrom = (UINT_PTR)win->id;
                hdr.code = TCN_SELCHANGE;
                SerialPutString("[USER32] send WM_NOTIFY TCN_SELCHANGE\r\n");
                SendMessageW(win->parent, WM_NOTIFY, (WPARAM)win->id, (LPARAM)&hdr);
            }
            u32_mark_invalid(hWnd);
        }
        return 0;
    case TCM_GETCURSEL:
        /* These controls share the numeric message value. */
        if (Msg == TCM_GETCURSEL && win->ctrl_type == U32_CTRL_TAB)
            return (LRESULT)win->tab_cur_sel;
        {
            int part = (int)(wParam & 0xFF);
            if (part >= 0 && part < 8) {
                const WCHAR *text = (const WCHAR*)lParam;
                u32_wstrcpy(win->status_text[part], text ? text : L"", 64);
            }
            u32_mark_invalid(hWnd);
            return TRUE;
        }
    case SB_SETTEXTW:
        {
            int part = (int)(wParam & 0xFF);
            if (part >= 0 && part < 8) {
                const WCHAR *text = (const WCHAR*)lParam;
                u32_wstrcpy(win->status_text[part], text ? text : L"", 64);
            }
            u32_mark_invalid(hWnd);
            return TRUE;
        }
    case SB_SETPARTS:
        {
            const int *parts = (const int*)lParam;
            int i;
            win->status_parts_count = (int)wParam;
            if (win->status_parts_count > 8) win->status_parts_count = 8;
            for (i = 0; i < win->status_parts_count; i++) {
                win->status_part_right[i] = parts ? parts[i] : 0;
            }
            u32_mark_invalid(hWnd);
            return TRUE;
        }
    case LVM_GETHEADER:
        if (!win->header_hwnd) {
            win->header_hwnd = u32_create_placeholder_child(hWnd, 0xF001);
            {
                U32_WINDOW *hdr = u32_lookup_window(win->header_hwnd);
                if (hdr) hdr->ctrl_type = U32_CTRL_LISTVIEW;
            }
        }
        return (LRESULT)win->header_hwnd;
    case LVM_GETITEMCOUNT:
        return (LRESULT)win->listview_item_count;
    case LVM_SETITEMCOUNT:
        win->listview_item_count = (int)wParam;
        if (win->listview_selected_count > win->listview_item_count) win->listview_selected_count = win->listview_item_count;
        u32_mark_invalid(hWnd);
        return TRUE;
    case LVM_GETSELECTEDCOUNT:
        return (LRESULT)win->listview_selected_count;
    case LVM_DELETEALLITEMS:
        lv = (U32_LISTVIEW_DATA*)win->listview_data;
        win->listview_item_count = 0;
        win->listview_selected_count = 0;
        if (lv) memset(lv->items, 0, sizeof(lv->items));
        u32_mark_invalid(hWnd);
        return TRUE;
    case LVM_DELETEITEM:
        if (win->listview_item_count > 0) win->listview_item_count--;
        u32_mark_invalid(hWnd);
        return TRUE;
    case LVM_INSERTCOLUMNW:
        lv = u32_ensure_listview(win);
        if (!lv) return -1;
        if (win->header_count < MAX_LV_COLUMNS) {
            const LVCOLUMNW *col = (const LVCOLUMNW*)lParam;
            if (col && col->pszText) u32_lv_copy_out(lv->columns[win->header_count], 64, col->pszText);
            win->header_count++;
            u32_mark_invalid(hWnd);
            return win->header_count - 1;
        }
        return -1;
    case LVM_INSERTITEMW:
        lv = u32_ensure_listview(win);
        if (!lv) return -1;
        if (win->listview_item_count < MAX_LV_ITEMS) {
            const LVITEMW *item = (const LVITEMW*)lParam;
            int index = win->listview_item_count++;
            if (item && item->pszText && item->iSubItem < MAX_LV_COLUMNS) {
                u32_lv_copy_out(lv->items[index][item->iSubItem], 64, item->pszText);
            }
            if (win->listview_selected_count == 0) win->listview_selected_count = 1;
            u32_mark_invalid(hWnd);
            return index;
        }
        return -1;
    case LVM_SETITEMW:
        {
            const LVITEMW *item = (const LVITEMW*)lParam;
            lv = u32_ensure_listview(win);
            if (!lv) return FALSE;
            if (item && item->iItem >= 0 && item->iItem < MAX_LV_ITEMS &&
                item->iSubItem >= 0 && item->iSubItem < MAX_LV_COLUMNS && item->pszText) {
                u32_lv_copy_out(lv->items[item->iItem][item->iSubItem], 64, item->pszText);
            }
            u32_mark_invalid(hWnd);
            return TRUE;
        }
    case EM_GETSEL:
        if (win->ctrl_type == U32_CTRL_EDIT) {
            if (wParam) *(DWORD*)wParam = (DWORD)win->edit_sel_start;
            if (lParam) *(DWORD*)lParam = (DWORD)win->edit_sel_end;
            return MAKELPARAM(win->edit_sel_start, win->edit_sel_end);
        }
        return 0;
    case EM_SETSEL:
        if (win->ctrl_type == U32_CTRL_EDIT) {
            int start = (int)wParam;
            int end = (int)lParam;
            if (start < 0) start = 0;
            if (end < 0 || end > win->edit_len) end = win->edit_len;
            if (start > win->edit_len) start = win->edit_len;
            if (end < start) end = start;
            win->edit_sel_start = start;
            win->edit_sel_end = end;
            return 0;
        }
        return 0;
    case EM_GETMODIFY:
        if (win->ctrl_type == U32_CTRL_EDIT) return (LRESULT)win->edit_modified;
        return 0;
    case EM_SETMODIFY:
        if (win->ctrl_type == U32_CTRL_EDIT) {
            win->edit_modified = wParam ? 1 : 0;
            return 0;
        }
        return 0;
    case EM_LINEINDEX:
        if (win->ctrl_type == U32_CTRL_EDIT) {
            int line = (int)wParam;
            int pos = 0;
            int current = 0;
            if (line < 0) return win->edit_sel_start;
            while (pos < win->edit_len && current < line) {
                if (win->edit_text[pos] == L'\n') current++;
                pos++;
            }
            return pos;
        }
        return 0;
    case EM_REPLACESEL:
        if (win->ctrl_type == U32_CTRL_EDIT) {
            u32_edit_replace_sel(win, (LPCWSTR)lParam);
            return TRUE;
        }
        return FALSE;
    case EM_LIMITTEXT:
        if (win->ctrl_type == U32_CTRL_EDIT) {
            win->edit_limit = wParam ? (int)wParam : 65535;
            return 0;
        }
        return 0;
    case EM_CANUNDO:
        if (win->ctrl_type == U32_CTRL_EDIT) return FALSE;
        return FALSE;
    case EM_UNDO:
    case EM_EMPTYUNDOBUFFER:
        return FALSE;
    case LVM_GETITEMW:
        {
            LVITEMW *item = (LVITEMW*)lParam;
            if (!item) return FALSE;
            if ((item->mask & LVIF_STATE) != 0) {
                item->state = (item->iItem == 0 && win->listview_item_count > 0) ? LVIS_SELECTED : 0;
            }
            if ((item->mask & LVIF_TEXT) != 0 && item->pszText && item->iItem >= 0 && item->iItem < MAX_LV_ITEMS &&
                item->iSubItem >= 0 && item->iSubItem < MAX_LV_COLUMNS) {
                lv = (U32_LISTVIEW_DATA*)win->listview_data;
                if (!lv) {
                    item->pszText[0] = 0;
                    return TRUE;
                }
                u32_lv_copy_out(item->pszText, item->cchTextMax > 0 ? item->cchTextMax : 64,
                                lv->items[item->iItem][item->iSubItem]);
            }
            return TRUE;
        }
    case HDM_GETITEMCOUNT:
        return (LRESULT)win->header_count;
    case HDM_GETITEMW:
        {
            HDITEMW *item = (HDITEMW*)lParam;
            int index = (int)wParam;
            lv = (U32_LISTVIEW_DATA*)win->listview_data;
            if (item && index >= 0 && index < win->header_count) {
                item->cxy = 80;
                if (item->pszText) {
                    if (lv) u32_lv_copy_out(item->pszText, item->cchTextMax > 0 ? item->cchTextMax : 64,
                                            lv->columns[index]);
                    else item->pszText[0] = 0;
                }
                return TRUE;
            }
            return FALSE;
        }
    case HDM_GETORDERARRAY:
        {
            int *order = (int*)lParam;
            UINT i;
            if (!order) return FALSE;
            for (i = 0; i < wParam; i++) order[i] = (int)i;
            return TRUE;
        }
    default:
        break;
    }

    return u32_dispatch(win, Msg, wParam, lParam);
}

BOOL DestroyWindow(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    DWORD owner_pid;
    int top_level;
    if (!win) return FALSE;
    owner_pid = win->owner_pid;
    top_level = win->top_level;
    if (win->top_level) Win32kDestroyWindow((HANDLE)hWnd);
    if (win->listview_data) {
        kfree(win->listview_data);
        win->listview_data = NULL;
    }
    if (win->edit_text) {
        kfree(win->edit_text);
        win->edit_text = NULL;
    }
    memset(win, 0, sizeof(*win));
    if (top_level && !u32_has_top_level_window_for_pid(owner_pid, hWnd)) {
        PostQuitMessage(0);
    }
    return TRUE;
}

int GetScrollPos(HWND hWnd, int nBar) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    return win ? win->scroll_pos[u32_scroll_bar_index(nBar)] : 0;
}

BOOL GetScrollInfo(HWND hWnd, int nBar, LPSCROLLINFO si) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    int i;
    if (!win || !si) return FALSE;
    i = u32_scroll_bar_index(nBar);
    if (si->fMask & SIF_RANGE) { si->nMin = win->scroll_min[i]; si->nMax = win->scroll_max[i]; }
    if (si->fMask & SIF_PAGE) si->nPage = win->scroll_page[i];
    if (si->fMask & SIF_POS) si->nPos = win->scroll_pos[i];
    if (si->fMask & SIF_TRACKPOS) si->nTrackPos = win->scroll_track[i];
    return TRUE;
}

int SetScrollPos(HWND hWnd, int nBar, int nPos, BOOL bRedraw) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    int i, old;
    if (!win) return 0;
    i = u32_scroll_bar_index(nBar); old = win->scroll_pos[i];
    win->scroll_pos[i] = nPos; u32_scroll_clamp(win, nBar);
    if (bRedraw) u32_mark_invalid(hWnd);
    return old;
}

int SetScrollInfo(HWND hWnd, int nBar, LPCSCROLLINFO si, BOOL bRedraw) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    int i, old;
    if (!win || !si) return 0;
    i = u32_scroll_bar_index(nBar); old = win->scroll_pos[i];
    if (si->fMask & SIF_RANGE) { win->scroll_min[i] = si->nMin; win->scroll_max[i] = si->nMax; }
    if (si->fMask & SIF_PAGE) win->scroll_page[i] = si->nPage ? si->nPage : 1;
    if (si->fMask & SIF_POS) win->scroll_pos[i] = si->nPos;
    if (si->fMask & SIF_TRACKPOS) win->scroll_track[i] = si->nTrackPos;
    u32_scroll_clamp(win, nBar);
    if (bRedraw) u32_mark_invalid(hWnd);
    return old;
}

BOOL ShowScrollBar(HWND hWnd, int wBar, BOOL bShow) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return FALSE;
    if (wBar == SB_BOTH || wBar == SB_HORZ) win->scroll_visible[0] = bShow ? TRUE : FALSE;
    if (wBar == SB_BOTH || wBar == SB_VERT) win->scroll_visible[1] = bShow ? TRUE : FALSE;
    u32_mark_invalid(hWnd);
    return TRUE;
}

#include "user32_resources.inc"

LRESULT WINAPI NtUserMessageCall(HWND hwnd,UINT msg,WPARAM wparam,LPARAM lparam,void *result,UINT type,BOOL ansi)
{
    (void)result;(void)ansi;
    if(type==NtUserClipboardWindowProc)return DefWindowProcW(hwnd,msg,wparam,lparam);
    /* Returning -1 asks Wine Explorer's tray code to use its local fallback. */
    if(type==NtUserSystemTrayCall)return -1;
    return SendMessageW(hwnd,msg,wparam,lparam);
}
BOOL WINAPI EqualRect(const RECT*a,const RECT*b){return a&&b&&a->left==b->left&&a->top==b->top&&a->right==b->right&&a->bottom==b->bottom;}
BOOL WINAPI SubtractRect(RECT*out,const RECT*a,const RECT*b){if(!out||!a||!b)return FALSE;*out=*a;if(b->left<=a->left&&b->right>=a->right){if(b->top<=a->top&&b->bottom<a->bottom)out->top=b->bottom;else if(b->bottom>=a->bottom&&b->top>a->top)out->bottom=b->top;}return out->right>out->left&&out->bottom>out->top;}
BOOL WINAPI ExitWindows(DWORD reserved,UINT reason){(void)reserved;(void)reason;CsrssShutdownSystem();return TRUE;}
BOOL WINAPI AdjustWindowRectEx(RECT *rect, DWORD style, BOOL menu, DWORD exstyle)
{
    int border_x = 0, border_y = 0, caption = 0, menu_height = 0;
    (void)exstyle;
    if (!rect) return FALSE;
    if (style & WS_THICKFRAME) {
        border_x = GetSystemMetrics(SM_CXFRAME);
        border_y = GetSystemMetrics(SM_CYFRAME);
    } else if (style & (WS_BORDER | WS_DLGFRAME)) {
        border_x = GetSystemMetrics(SM_CXBORDER);
        border_y = GetSystemMetrics(SM_CYBORDER);
    }
    if (style & WS_CAPTION) caption = GetSystemMetrics(SM_CYCAPTION);
    if (menu) menu_height = GetSystemMetrics(SM_CYMENU);
    rect->left -= border_x;
    rect->right += border_x;
    rect->top -= border_y + caption + menu_height;
    rect->bottom += border_y;
    return TRUE;
}

BOOL WINAPI AdjustWindowRect(RECT*r,DWORD style,BOOL menu){return AdjustWindowRectEx(r,style,menu,0);}
BOOL WINAPI DrawIconEx(HDC dc,int x,int y,HICON icon,int cx,int cy,UINT step,HBRUSH brush,UINT flags){(void)dc;(void)x;(void)y;(void)icon;(void)cx;(void)cy;(void)step;(void)brush;(void)flags;return TRUE;}
BOOL WINAPI UpdateLayeredWindow(HWND hwnd,HDC dst,const POINT*dp,const SIZE*s,HDC src,const POINT*sp,COLORREF key,const BLENDFUNCTION*b,DWORD flags){(void)dst;(void)dp;(void)s;(void)src;(void)sp;(void)key;(void)b;(void)flags;InvalidateRect(hwnd,0,TRUE);return TRUE;}
BOOL WINAPI SendNotifyMessageW(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){PostMessageW(hwnd,msg,wp,lp);return TRUE;}
HWND WINAPI SetParent(HWND child,HWND parent){(void)child;return parent;}
HICON WINAPI CopyIcon(HICON icon){return icon;}
HWND WINAPI GetAncestor(HWND hwnd,UINT flags){(void)flags;while(GetParent(hwnd))hwnd=GetParent(hwnd);return hwnd;}
HWND WINAPI GetForegroundWindow(void){return GetActiveWindow();}
BOOL WINAPI SystemParametersInfoW(UINT action,UINT param,PVOID data,UINT flags){(void)param;(void)flags;if(action==SPI_SETDESKWALLPAPER)return TRUE;if(!data)return FALSE;if(action==SPI_GETWORKAREA){RECT*r=data;r->left=r->top=0;r->right=GetSystemMetrics(SM_CXSCREEN);r->bottom=GetSystemMetrics(SM_CYSCREEN)-28;return TRUE;}if(action==SPI_GETICONTITLELOGFONT){memset(data,0,sizeof(LOGFONTW));((LOGFONTW*)data)->lfHeight=16;return TRUE;}if(action==SPI_GETNONCLIENTMETRICS){NONCLIENTMETRICSW*n=data;n->lfCaptionFont.lfHeight=16;return TRUE;}return FALSE;}
HICON WINAPI CreateIcon(HINSTANCE i,int w,int h,BYTE p,BYTE b,const BYTE*a,const BYTE*x){(void)i;(void)w;(void)h;(void)p;(void)b;(void)a;(void)x;return (HICON)(ULONG_PTR)1;}
BOOL WINAPI DrawFrameControl(HDC dc,LPRECT r,UINT type,UINT state){(void)dc;(void)r;(void)type;(void)state;return TRUE;}
BOOL WINAPI DrawCaptionTempW(HWND h,HDC dc,const RECT*r,HFONT f,HICON i,LPCWSTR t,UINT flags){(void)h;(void)dc;(void)r;(void)f;(void)i;(void)t;(void)flags;return TRUE;}
BOOL WINAPI IsWindowEnabled(HWND hwnd){return IsWindow(hwnd);}
#include "user32_menu.inc"
BOOL BringWindowToTop(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return FALSE;
    if (win->top_level) {
        Win32kActivateWindow((HANDLE)hWnd);
    } else {
        /* BringWindowToTop on a child changes child ordering only; it must
         * never activate the application's top-level window. */
        u32_mark_invalid_descendants(hWnd);
        u32_mark_invalid(hWnd);
    }
    return TRUE;
}
HWND SetFocus(HWND hWnd) {
    HWND old = g_focus;
    if (old == hWnd) return old;
    g_focus = hWnd;
    if (old) SendMessageW(old, WM_KILLFOCUS, (WPARAM)hWnd, 0);
    if (hWnd) SendMessageW(hWnd, WM_SETFOCUS, (WPARAM)old, 0);
    return old;
}

/* Kernel/CSRSS input bridge.  Real USER32 queues are thread-owned; the
 * current cooperative scheduler does not yet have separate native message
 * queues, so a cross-thread PostMessage can otherwise sit undispatched. */
void WINAPI User32InjectKeyboard(HWND hWnd, UINT key, BOOL pressed) {
    HWND root, focus, next, button;
    U32_WINDOW *focus_win;
    if (!pressed) return;
    if (!u32_lookup_window(hWnd)) {
        if (g_active_window && u32_lookup_window(g_active_window))
            hWnd = g_active_window;
        else
            hWnd = u32_find_top_level_window_for_pid(1);
    }
    if (!hWnd || !u32_lookup_window(hWnd)) return;
    root = u32_get_root_window(hWnd);
    focus = u32_dialog_focused_child(root);
    focus_win = u32_lookup_window(focus);
    if (key == VK_TAB) {
        next = u32_dialog_find_next_tabstop(root, focus);
        if (next) SetFocus(next);
        return;
    }
    if (key == VK_RETURN) {
        if (focus_win && focus_win->ctrl_type == U32_CTRL_BUTTON) {
            u32_dialog_click_button(focus);
            return;
        }
        button = u32_dialog_find_child_by_id(root, IDOK);
        if (button) u32_dialog_click_button(button);
        return;
    }
    if (key == 8 || key >= 32) {
        SendMessageW(root, WM_CHAR, key, 0);
    }
}

void WINAPI User32InjectMouse(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!u32_lookup_window(hWnd)) {
        if (g_active_window && u32_lookup_window(g_active_window))
            hWnd = g_active_window;
        else
            hWnd = u32_find_top_level_window_for_pid(1);
    }
    if (!hWnd || !u32_lookup_window(hWnd)) return;
    SendMessageW(hWnd, msg, wParam, lParam);
}

void WINAPI User32SetProcessId(DWORD pid) {
    g_user32_process_id = pid ? pid : 1;
}

HWND GetActiveWindow(void) { return g_active_window; }
HWND SetActiveWindow(HWND hWnd) {
    HWND old = g_active_window;
    g_active_window = hWnd;
    if (hWnd) BringWindowToTop(hWnd);
    return old;
}
HWND GetDesktopWindow(void) { return (HWND)1; }
DWORD FormatMessageW(DWORD dwFlags, LPCVOID lpSource, DWORD dwMessageId, DWORD dwLanguageId,
                     LPWSTR lpBuffer, DWORD nSize, void *Arguments) {
    LPCWSTR text = L"Error";
    (void)dwFlags; (void)lpSource; (void)dwMessageId; (void)dwLanguageId; (void)Arguments;
    if (!lpBuffer || nSize == 0) return 0;
    u32_lv_copy_out(lpBuffer, (int)nSize, text);
    return (DWORD)u32_wstrlen(lpBuffer);
}
HLOCAL LocalFree(HLOCAL hMem) { return hMem; }
BOOL EndDialog(HWND hDlg, INT_PTR nResult) {
    U32_WINDOW *win = u32_lookup_window(hDlg);
    if (!win) return FALSE;
    SerialPutString("[USER32] EndDialog\r\n");
    win->ended = 1;
    win->dialog_result = nResult;
    /* Wake a modal GetMessage loop so it can observe the ended flag. */
    PostMessageW(hDlg, WM_NULL, 0, 0);
    return TRUE;
}
BOOL PostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    return u32_enqueue_message(hWnd, Msg, wParam, lParam);
}
BOOL WinHelpW(HWND hWndMain, LPCWSTR lpszHelp, UINT uCommand, ULONG_PTR dwData) { (void)hWndMain; (void)lpszHelp; (void)uCommand; (void)dwData; return TRUE; }
BOOL GetCursorPos(LPPOINT lpPoint) { if (!lpPoint) return FALSE; lpPoint->x = 0; lpPoint->y = 0; return TRUE; }
LONG_PTR GetWindowLongW(HWND hWnd, int nIndex) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return 0;
    if (nIndex == GWL_STYLE) return (LONG_PTR)win->style;
    if (nIndex == GWL_EXSTYLE) return (LONG_PTR)win->exstyle;
    if (nIndex == GWLP_WNDPROC) return (LONG_PTR)(win->dlgproc ? (LONG_PTR)win->dlgproc : (LONG_PTR)win->proc);
    if (nIndex == GWLP_USERDATA) return win->user_data;
    if (nIndex == GWLP_ID) return (LONG_PTR)win->id;
    return 0;
}
LONG SetWindowLongW(HWND hWnd, int nIndex, LONG dwNewLong) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    LONG old = 0;
    if (!win) return 0;
    if (nIndex == GWL_STYLE) { old = (LONG)win->style; win->style = (DWORD)dwNewLong; }
    else if (nIndex == GWL_EXSTYLE) { old = (LONG)win->exstyle; win->exstyle = (DWORD)dwNewLong; }
    else if (nIndex == GWLP_ID) { old = (LONG)win->id; win->id = (UINT)dwNewLong; }
    else if (nIndex == GWLP_USERDATA) { old = (LONG)win->user_data; win->user_data = (LONG_PTR)dwNewLong; }
    return old;
}
LONG_PTR GetWindowLongPtrW(HWND hWnd, int nIndex) { return GetWindowLongW(hWnd, nIndex); }
LONG_PTR SetWindowLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    LONG_PTR old;
    if (!win) return 0;
    if (nIndex == GWLP_WNDPROC) {
        old = (LONG_PTR)(win->dlgproc ? (LONG_PTR)win->dlgproc : (LONG_PTR)win->proc);
        win->dlgproc = NULL;
        win->proc = (WNDPROC)dwNewLong;
        return old;
    }
    if (nIndex == GWLP_USERDATA) {
        old = win->user_data;
        win->user_data = dwNewLong;
        return old;
    }
    return (LONG_PTR)SetWindowLongW(hWnd, nIndex, (LONG)dwNewLong);
}
LONG_PTR GetClassLongPtrW(HWND hWnd, int nIndex) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    U32_CLASS *cls;
    if (!win) return 0;
    cls = win->klass;
    if (!cls) return 0;
    if (nIndex == GCLP_WNDPROC) return (LONG_PTR)cls->proc;
    if (nIndex == GCLP_HICON) return (LONG_PTR)cls->hIcon;
    if (nIndex == GCLP_HCURSOR) return (LONG_PTR)cls->hCursor;
    if (nIndex == GCLP_HMODULE) return (LONG_PTR)cls->hInstance;
    if (nIndex == GCLP_HICONSM) return (LONG_PTR)(cls->hIconSm ? cls->hIconSm : cls->hIcon);
    if (nIndex == GCLP_HBRBACKGROUND) return (LONG_PTR)cls->hbrBackground;
    return 0;
}
ULONG_PTR SetClassLongPtrW(HWND hWnd, int nIndex, LONG_PTR dwNewLong) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    U32_CLASS *cls;
    ULONG_PTR old = 0;
    if (!win) return 0;
    cls = win->klass;
    if (!cls) return 0;
    if (nIndex == GCLP_WNDPROC) {
        old = (ULONG_PTR)cls->proc;
        cls->proc = (WNDPROC)dwNewLong;
    } else if (nIndex == GCLP_HICON) {
        old = (ULONG_PTR)cls->hIcon;
        cls->hIcon = (HICON)dwNewLong;
    } else if (nIndex == GCLP_HICONSM) {
        old = (ULONG_PTR)cls->hIconSm;
        cls->hIconSm = (HICON)dwNewLong;
    } else if (nIndex == GCLP_HCURSOR) {
        old = (ULONG_PTR)cls->hCursor;
        cls->hCursor = (HCURSOR)dwNewLong;
    } else if (nIndex == GCLP_HMODULE) {
        old = (ULONG_PTR)cls->hInstance;
        cls->hInstance = (HINSTANCE)dwNewLong;
    } else if (nIndex == GCLP_HBRBACKGROUND) {
        old = (ULONG_PTR)cls->hbrBackground;
        cls->hbrBackground = (HBRUSH)dwNewLong;
    }
    return old;
}
BOOL IsWindowVisible(HWND hWnd) { U32_WINDOW *win = u32_lookup_window(hWnd); return win ? win->visible : FALSE; }
BOOL IsIconic(HWND hWnd) { U32_WINDOW *win=u32_lookup_window(hWnd);return win&&win->top_level?Win32kIsWindowMinimized((HANDLE)hWnd):FALSE; }
HWND GetWindow(HWND hWnd, UINT uCmd) { (void)uCmd; return hWnd; }
LRESULT CallWindowProcW(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) { return lpPrevWndFunc ? lpPrevWndFunc(hWnd, Msg, wParam, lParam) : 0; }
DWORD GetSysColor(int nIndex) { (void)nIndex; return RGB(192,192,192); }
BOOL OpenIcon(HWND hWnd) { return ShowWindow(hWnd, SW_RESTORE); }
BOOL SetForegroundWindow(HWND hWnd) { BringWindowToTop(hWnd); return TRUE; }
int wsprintfW(LPWSTR lpOut, LPCWSTR lpFmt, ...) { int r; va_list ap; va_start(ap, lpFmt); r = u32_vsnprintfw(lpOut, 1024, lpFmt, ap); va_end(ap); return r; }
int wnsprintfW(LPWSTR buffer, int count, LPCWSTR format, ...) { int r; va_list ap; va_start(ap, format); r = u32_vsnprintfw(buffer, count, format, ap); va_end(ap); return r; }
int swprintf(WCHAR *buffer, size_t count, const WCHAR *format, ...) { int r; va_list ap; va_start(ap, format); r = u32_vsnprintfw(buffer, (int)count, format, ap); va_end(ap); return r; }
BOOL EnableWindow(HWND hWnd, BOOL bEnable) { U32_WINDOW *win = u32_lookup_window(hWnd); if (!win) return FALSE; win->enabled = bEnable; return TRUE; }
BOOL IsClipboardFormatAvailable(UINT format) { (void)format; return FALSE; }
BOOL CopyRect(LPRECT lprcDst, const RECT *lprcSrc) { if (!lprcDst || !lprcSrc) return FALSE; *lprcDst = *lprcSrc; return TRUE; }
BOOL IsWindow(HWND hWnd) { return u32_lookup_window(hWnd) ? TRUE : FALSE; }
HWND GetParent(HWND hWnd) { U32_WINDOW *win = u32_lookup_window(hWnd); return win ? win->parent : NULL; }
LRESULT SendMessageTimeoutW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult) { LRESULT r = SendMessageW(hWnd, Msg, wParam, lParam); (void)fuFlags; (void)uTimeout; if (lpdwResult) *lpdwResult = (DWORD_PTR)r; return r; }
BOOL EnumWindows(BOOL (CALLBACK *lpEnumFunc)(HWND, LPARAM), LPARAM lParam) { int i; for (i = 0; i < MAX_U32_WINDOWS; i++) if (g_windows[i].used && g_windows[i].top_level) if (!lpEnumFunc(g_windows[i].hwnd, lParam)) break; return TRUE; }
BOOL IsHungAppWindow(HWND hWnd) { (void)hWnd; return FALSE; }
#include "user32_paint.inc"
WORD TileWindows(HWND hwndParent, UINT wHow, const RECT *lpRect, UINT cKids, const HWND *lpKids) { (void)hwndParent; (void)wHow; (void)lpRect; (void)cKids; (void)lpKids; return 0; }
WORD CascadeWindows(HWND hwndParent, UINT wHow, const RECT *lpRect, UINT cKids, const HWND *lpKids) { (void)hwndParent; (void)wHow; (void)lpRect; (void)cKids; (void)lpKids; return 0; }
void SwitchToThisWindow(HWND hWnd, BOOL fAltTab) { (void)fAltTab; BringWindowToTop(hWnd); }
DWORD GetWindowThreadProcessId(HWND hWnd, DWORD *lpdwProcessId) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    DWORD pid = win ? win->owner_pid : 0;
    if (lpdwProcessId) *lpdwProcessId = pid;
    return pid;
}
HWND FindTopLevelWindowForProcessId(DWORD pid) {
    return u32_find_top_level_window_for_pid(pid);
}
HCURSOR LoadCursorW(HINSTANCE hInstance, LPCWSTR lpCursorName) { (void)hInstance; (void)lpCursorName; return (HCURSOR)1; }
UINT RegisterWindowMessageW(LPCWSTR lpString) { static UINT next = 0xC000; (void)lpString; return next++; }
BOOL IsDialogMessageW(HWND hDlg, LPMSG lpMsg) {
    HWND focus;
    U32_WINDOW *focus_win;
    HWND next;
    HWND button;
    if (!lpMsg) return FALSE;
    if (lpMsg->message != WM_KEYDOWN && lpMsg->message != WM_SYSKEYDOWN) return FALSE;
    if (lpMsg->hwnd != hDlg && !u32_is_descendant(hDlg, lpMsg->hwnd)) return FALSE;

    focus = u32_dialog_focused_child(hDlg);
    focus_win = u32_lookup_window(focus);
    switch ((UINT)lpMsg->wParam) {
    case VK_TAB:
        next = u32_dialog_find_next_tabstop(hDlg, focus);
        if (next) {
            SetFocus(next);
            return TRUE;
        }
        return FALSE;
    case VK_RETURN:
        if (focus_win && focus_win->ctrl_type == U32_CTRL_BUTTON) {
            u32_dialog_click_button(focus);
            return TRUE;
        }
        if (focus_win && focus_win->ctrl_type == U32_CTRL_EDIT) {
            LRESULT code = SendMessageW(focus, WM_GETDLGCODE, lpMsg->wParam, (LPARAM)lpMsg);
            if (code & (DLGC_WANTALLKEYS | DLGC_WANTCHARS)) return FALSE;
        }
        button = u32_dialog_find_child_by_id(hDlg, IDOK);
        if (button) {
            u32_dialog_click_button(button);
            return TRUE;
        }
        return FALSE;
    case VK_ESCAPE:
        button = u32_dialog_find_child_by_id(hDlg, IDCANCEL);
        if (button) {
            u32_dialog_click_button(button);
            return TRUE;
        }
        if (u32_lookup_window(hDlg) && u32_lookup_window(hDlg)->dialog) {
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        return FALSE;
    default:
        break;
    }
    return FALSE;
}
HACCEL LoadAcceleratorsW(HINSTANCE hInstance, LPCWSTR lpTableName) { (void)hInstance; (void)lpTableName; return (HACCEL)1; }
int TranslateAcceleratorW(HWND hWnd, HACCEL hAccTable, LPMSG lpMsg) { (void)hWnd; (void)hAccTable; (void)lpMsg; return 0; }
BOOL DragAcceptFiles(HWND hWnd, BOOL fAccept) { (void)hWnd; (void)fAccept; return TRUE; }
UINT DragQueryFileW(HANDLE hDrop, UINT iFile, LPWSTR lpszFile, UINT cch) {
    (void)hDrop;
    if (iFile == 0xFFFFFFFFu) return 0;
    if (lpszFile && cch) lpszFile[0] = 0;
    return 0;
}
void DragFinish(HANDLE hDrop) { (void)hDrop; }
HWND CreateWindowW(LPCWSTR lpClassName, LPCWSTR lpWindowName, DWORD dwStyle,
                   int X, int Y, int nWidth, int nHeight, HWND hWndParent,
                   HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
    return CreateWindowExW(0, lpClassName, lpWindowName, dwStyle, X, Y, nWidth, nHeight,
                           hWndParent, hMenu, hInstance, lpParam);
}

BOOL IntersectRect(LPRECT out,const RECT *a,const RECT *b){if(!out||!a||!b)return FALSE;out->left=a->left>b->left?a->left:b->left;out->top=a->top>b->top?a->top:b->top;out->right=a->right<b->right?a->right:b->right;out->bottom=a->bottom<b->bottom?a->bottom:b->bottom;if(out->right<=out->left||out->bottom<=out->top){memset(out,0,sizeof(*out));return FALSE;}return TRUE;}
HCURSOR SetCursor(HCURSOR cursor){static HCURSOR current;HCURSOR old=current;current=cursor;return old;}
HCURSOR LoadCursorA(HINSTANCE instance,LPCSTR cursor){return LoadCursorW(instance,(LPCWSTR)cursor);}
BOOL ClipCursor(const RECT *rect){(void)rect;return TRUE;}
HWINSTA GetProcessWindowStation(void){return (HWINSTA)(ULONG_PTR)1;}
HDESK GetThreadDesktop(DWORD thread){(void)thread;return (HDESK)(ULONG_PTR)1;}
HDESK CreateDesktopW(LPCWSTR desktop,LPCWSTR device,DEVMODEW *mode,DWORD flags,DWORD access,void *attributes){(void)desktop;(void)device;(void)mode;(void)flags;(void)access;(void)attributes;return (HDESK)(ULONG_PTR)1;}
BOOL GetUserObjectInformationW(HANDLE object,int index,PVOID info,DWORD length,DWORD *needed){(void)object;if(index==UOI_FLAGS){if(needed)*needed=sizeof(USEROBJECTFLAGS);if(info&&length>=sizeof(USEROBJECTFLAGS)){USEROBJECTFLAGS*f=info;f->fInherit=FALSE;f->fReserved=FALSE;f->dwFlags=WSF_VISIBLE;return TRUE;}}return FALSE;}
BOOL SetShellWindow(HWND shell){(void)shell;return TRUE;}
BOOL PaintDesktop(HDC dc){RECT r={0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN)};HBRUSH b=CreateSolidBrush(RGB(0,0,128));BOOL ok=FillRect(dc,&r,b);if(b)DeleteObject(b);return ok;}
BOOL PeekMessageW(LPMSG msg,HWND hwnd,UINT min,UINT max,UINT remove){return u32_dequeue_message(msg,hwnd,min,max,(remove&PM_REMOVE)!=0);}
DWORD MsgWaitForMultipleObjects(DWORD count,const HANDLE *handles,BOOL all,DWORD timeout,DWORD mask){(void)mask;return WaitForMultipleObjects(count,handles,all,timeout);}
LONG ChangeDisplaySettingsExW(LPCWSTR device,DEVMODEW *mode,HWND hwnd,DWORD flags,LPVOID param){(void)device;(void)mode;(void)hwnd;(void)flags;(void)param;return 0;}
BOOL EnumDisplayDevicesW(LPCWSTR device,DWORD index,PDISPLAY_DEVICEW display,DWORD flags){(void)device;(void)flags;if(index||!display)return FALSE;display->StateFlags=4;u32_wstrcpy(display->DeviceName,L"DISPLAY1",32);return TRUE;}
BOOL EnumDisplaySettingsExW(LPCWSTR device,DWORD mode,LPDEVMODEW settings,DWORD flags){(void)device;(void)mode;(void)flags;if(!settings)return FALSE;settings->dmPelsWidth=GetSystemMetrics(SM_CXSCREEN);settings->dmPelsHeight=GetSystemMetrics(SM_CYSCREEN);settings->dmBitsPerPel=32;settings->dmDisplayFrequency=60;settings->dmFields=DM_PELSWIDTH|DM_PELSHEIGHT;return TRUE;}
UINT ExtractIconExW(LPCWSTR file,int index,HICON *large,HICON *small,UINT count){(void)file;(void)index;if(!count)return 1;if(large)*large=LoadIconW(0,IDI_WINLOGO);if(small)*small=LoadIconW(0,IDI_WINLOGO);return 1;}
static U32_MENU_ITEM *u32_info_item(U32_MENU*m,UINT item,BOOL pos){if(!m)return 0;if(pos)return item<(UINT)m->count?&m->items[item]:0;for(int i=0;i<m->count;i++)if(m->items[i].id==item)return&m->items[i];return 0;}
BOOL GetMenuItemInfoW(HMENU h,UINT item,BOOL pos,LPMENUITEMINFOW info){U32_MENU_ITEM*m=u32_info_item(u32_lookup_menu(h),item,pos);if(!m||!info)return FALSE;if(info->fMask&MIIM_ID)info->wID=m->id;if(info->fMask&MIIM_STATE)info->fState=m->flags;if(info->fMask&MIIM_FTYPE)info->fType=m->flags;if(info->fMask&MIIM_SUBMENU)info->hSubMenu=m->submenu;if(info->fMask&MIIM_DATA)info->dwItemData=m->item_data;if(info->fMask&MIIM_BITMAP)info->hbmpItem=m->bitmap;if((info->fMask&MIIM_STRING)&&info->dwTypeData&&info->cch)u32_wstrcpy(info->dwTypeData,m->text,(int)info->cch);return TRUE;}
BOOL SetMenuItemInfoW(HMENU h,UINT item,BOOL pos,const MENUITEMINFOW *info){U32_MENU_ITEM*m=u32_info_item(u32_lookup_menu(h),item,pos);if(!m||!info)return FALSE;if(info->fMask&MIIM_ID)m->id=info->wID;if(info->fMask&MIIM_STATE)m->flags=info->fState;if(info->fMask&MIIM_FTYPE)m->flags=info->fType;if(info->fMask&MIIM_SUBMENU)m->submenu=info->hSubMenu;if(info->fMask&MIIM_DATA)m->item_data=info->dwItemData;if(info->fMask&MIIM_BITMAP)m->bitmap=info->hbmpItem;if((info->fMask&MIIM_STRING)&&info->dwTypeData)u32_wstrcpy(m->text,info->dwTypeData,64);return TRUE;}
BOOL InsertMenuItemW(HMENU h,UINT item,BOOL pos,const MENUITEMINFOW *info){U32_MENU*m=u32_lookup_menu(h);int at;if(!m||!info||m->count>=32)return FALSE;at=pos&&(int)item>=0&&(int)item<=m->count?(int)item:m->count;for(int i=m->count;i>at;i--)m->items[i]=m->items[i-1];memset(&m->items[at],0,sizeof(m->items[at]));m->count++;return SetMenuItemInfoW(h,(UINT)at,TRUE,info);}
BOOL GetMenuInfo(HMENU menu,LPMENUINFO info){U32_MENU*m=u32_lookup_menu(menu);if(!m||!info)return FALSE;if(info->fMask&MIM_STYLE)info->dwStyle=m->style;if(info->fMask&MIM_MENUDATA)info->dwMenuData=m->menu_data;return TRUE;}
BOOL SetMenuInfo(HMENU menu,const MENUINFO *info){U32_MENU*m=u32_lookup_menu(menu);if(!m||!info)return FALSE;if(info->fMask&MIM_STYLE)m->style=info->dwStyle;if(info->fMask&MIM_MENUDATA)m->menu_data=info->dwMenuData;return TRUE;}
