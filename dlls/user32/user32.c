#include <stdint.h>
#include <stdarg.h>
#include "windows.h"
#include "commctrl.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);
extern void *memset(void *dest, int c, uint32_t n);
extern void *memcpy(void *dest, const void *src, uint32_t n);
extern uint32_t strlen(const char *s);
extern void KeYield(void);
extern uint32_t GetTickCount(void);
extern uint32_t GetCurrentProcessId(void);
extern void SerialPutString(const char *str);
extern void *Win32kRegisterClass(const char *className, uint32_t style, void (*wndProc)(void *, uint32_t, uint32_t, uint32_t));
extern void *Win32kCreateWindow(const char *className, const char *title, int x, int y, int w, int h, uint32_t style);
extern void Win32kShowWindow(void *hwnd);
extern void Win32kUpdateWindow(void *hwnd);
extern void Win32kGetWindowRect(void *hwnd, LPRECT lpRect);
extern void Win32kDestroyWindow(void *hwnd);
extern void Win32kActivateWindow(void *hwnd);
extern void Win32kRedrawAll(void);
extern void Win32kRefreshCursor(void);
extern HDC GdiCreateScreenDC(HWND hwnd);
extern HDC GdiCreateScreenDCEx(HWND hwnd, int origin_x, int origin_y, int width, int height);
extern void GdiDestroyScreenDC(HDC hdc);

#define MAX_U32_CLASSES 64
#define MAX_U32_WINDOWS 256
#define MAX_U32_TIMERS 64
#define MAX_U32_MENUS 64
#define MAX_U32_MESSAGES 256
#define MAX_LV_COLUMNS 32
#define MAX_LV_ITEMS 256
#define MAX_TAB_ITEMS 16
#define U32_FRAME_THICKNESS 2
#define U32_EDGE_THICKNESS 1
#define U32_TITLEBAR_HEIGHT 18

typedef struct _U32_CLASS {
    int used;
    WCHAR name[64];
    WNDPROC proc;
    UINT style;
    HANDLE win32k_class;
} U32_CLASS;

typedef struct _U32_MENU_ITEM {
    UINT id;
    UINT flags;
    WCHAR text[64];
    HMENU submenu;
} U32_MENU_ITEM;

typedef struct _U32_MENU {
    int used;
    HMENU handle;
    int count;
    int default_item;
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
    HMENU menu;
    HWND header_hwnd;
    int check_state;
    int tab_cur_sel;
    int tab_count;
    int listview_item_count;
    int listview_selected_count;
    int header_count;
    int status_parts_count;
    int focused;
    int ctrl_type;
    int invalidated;
    int painting;
    int edit_limit;
    int edit_len;
    int edit_sel_start;
    int edit_sel_end;
    int edit_modified;
    int status_part_right[8];
    WCHAR status_text[8][64];
    void *listview_data;
    WCHAR *edit_text;
    DWORD owner_pid;
} U32_WINDOW;

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

static U32_CLASS g_classes[MAX_U32_CLASSES];
static U32_WINDOW g_windows[MAX_U32_WINDOWS];
static U32_TIMER g_timers[MAX_U32_TIMERS];
static U32_MENU g_menus[MAX_U32_MENUS];
static U32_MESSAGE g_messages[MAX_U32_MESSAGES];
static HWND g_focus = NULL;
static HWND g_active_window = NULL;
static int g_message_loop_exit = 0;
static int g_quit_exit_code = 0;

static void u32_paint_children(HWND hwnd);
static void u32_mark_invalid(HWND hwnd);
BOOL PostMessageW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

#define U32_CTRL_GENERIC   0
#define U32_CTRL_DIALOG    1
#define U32_CTRL_TAB       2
#define U32_CTRL_LISTVIEW  3
#define U32_CTRL_BUTTON    4
#define U32_CTRL_STATIC    5
#define U32_CTRL_EDIT      6
#define U32_CTRL_GROUPBOX  7
#define U32_CTRL_STATUS    8

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
    alloc_chars = win->edit_limit + 1;
    if (alloc_chars < min_chars) alloc_chars = min_chars;
    if (alloc_chars > 65536) alloc_chars = 65536;
    if (win->edit_text && alloc_chars <= win->edit_limit + 1) return win->edit_text;
    newbuf = (WCHAR*)kmalloc((uint32_t)(alloc_chars * sizeof(WCHAR)));
    if (!newbuf) return win->edit_text;
    memset(newbuf, 0, (uint32_t)(alloc_chars * sizeof(WCHAR)));
    if (win->edit_text && win->edit_len > 0) {
        u32_wmemcpy(newbuf, win->edit_text, win->edit_len);
        kfree(win->edit_text);
    }
    win->edit_text = newbuf;
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

static WCHAR u32_towupper(WCHAR ch) {
    if (ch >= L'a' && ch <= L'z') return ch - (L'a' - L'A');
    return ch;
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
        return U32_FRAME_THICKNESS + U32_TITLEBAR_HEIGHT + U32_EDGE_THICKNESS;
    }
    return U32_FRAME_THICKNESS;
}

static void u32_sync_top_level(U32_WINDOW *win) {
    RECT rc;
    if (!win || !win->top_level || !win->hwnd) return;
    Win32kGetWindowRect((HANDLE)win->hwnd, &rc);
    win->rect = rc;
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

static HWND u32_hit_test_child(HWND parent, int x, int y) {
    int i;
    for (i = MAX_U32_WINDOWS - 1; i >= 0; i--) {
        U32_WINDOW *child = &g_windows[i];
        int cx, cy, cw, ch;
        if (!child->used || child->parent != parent || !child->visible) continue;
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
        if (u32_wstrcmp(class_name, L"msctls_statusbar32") == 0) return U32_CTRL_STATUS;
    }
    return U32_CTRL_GENERIC;
}

static LRESULT u32_dispatch(U32_WINDOW *win, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (!win) return 0;
    if (win->dlgproc) return win->dlgproc(win->hwnd, msg, wParam, lParam);
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
    win->owner_pid = pwin ? pwin->owner_pid : GetCurrentProcessId();
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
    u32_wstrcpy(win->title, title, 128);
    (void)prc;
    u32_set_rect(win, x, y, w, h);
    SendMessageW(win->hwnd, WM_CREATE, 0, 0);
    SendMessageW(win->hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(w, h));
    return win->hwnd;
}

static void u32_taskmgr_create_dialog_children(UINT tmpl_id, HWND hwnd) {
    switch (tmpl_id) {
    case 102:
        u32_create_child_control(hwnd, L"SysTabControl32", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 1015, 3, 3, 257, 228);
        break;
    case 106:
        u32_create_child_control(hwnd, L"SysListView32", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS, 0, 1016, 4, 4, 239, 180);
        u32_create_child_control(hwnd, L"Button", L"&New Task...", WS_CHILD | WS_VISIBLE, 0, 1014, 175, 189, 68, 14);
        u32_create_child_control(hwnd, L"Button", L"&Switch To", WS_CHILD | WS_VISIBLE | WS_DISABLED, 0, 1013, 104, 189, 68, 14);
        u32_create_child_control(hwnd, L"Button", L"&End Task", WS_CHILD | WS_VISIBLE | WS_DISABLED, 0, 1012, 33, 189, 68, 14);
        break;
    case 133:
        u32_create_child_control(hwnd, L"SysListView32", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_OWNERDATA, 0, 1018, 4, 4, 239, 180);
        u32_create_child_control(hwnd, L"Button", L"&End Process", WS_CHILD | WS_VISIBLE, 0, 1017, 165, 189, 78, 14);
        u32_create_child_control(hwnd, L"Button", L"&Show processes from all users", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 1021, 4, 191, 160, 10);
        break;
    case 134:
        u32_create_child_control(hwnd, L"Button", L"CPU usage", WS_CHILD | WS_VISIBLE | BS_GROUPBOX | WS_TABSTOP, 0, 1043, 5, 5, 60, 54);
        u32_create_child_control(hwnd, L"Button", L"Mem usage", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1044, 5, 63, 60, 54);
        u32_create_child_control(hwnd, L"Button", L"Totals", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1037, 5, 122, 111, 39);
        u32_create_child_control(hwnd, L"Button", L"Commit charge (K)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1038, 5, 166, 111, 39);
        u32_create_child_control(hwnd, L"Button", L"Physical memory (K)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1040, 126, 122, 116, 39);
        u32_create_child_control(hwnd, L"Button", L"Kernel memory (K)", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1039, 126, 166, 116, 39);
        u32_create_child_control(hwnd, L"Static", L"Handles", WS_CHILD | WS_VISIBLE, 0, 1060, 12, 131, 43, 8);
        u32_create_child_control(hwnd, L"Static", L"Threads", WS_CHILD | WS_VISIBLE, 0, 1061, 12, 140, 43, 8);
        u32_create_child_control(hwnd, L"Static", L"Processes", WS_CHILD | WS_VISIBLE, 0, 1062, 12, 149, 43, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1024, 65, 131, 45, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1026, 65, 140, 45, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1027, 65, 149, 45, 8);
        u32_create_child_control(hwnd, L"Static", L"Total", WS_CHILD | WS_VISIBLE, 0, 1063, 12, 175, 43, 8);
        u32_create_child_control(hwnd, L"Static", L"Limit", WS_CHILD | WS_VISIBLE, 0, 1064, 12, 184, 43, 8);
        u32_create_child_control(hwnd, L"Static", L"Peak", WS_CHILD | WS_VISIBLE, 0, 1065, 12, 193, 43, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1028, 65, 174, 45, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1029, 65, 184, 45, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1030, 65, 193, 45, 8);
        u32_create_child_control(hwnd, L"Static", L"Total", WS_CHILD | WS_VISIBLE, 0, 1066, 132, 131, 53, 8);
        u32_create_child_control(hwnd, L"Static", L"Available", WS_CHILD | WS_VISIBLE, 0, 1067, 132, 140, 53, 8);
        u32_create_child_control(hwnd, L"Static", L"System Cache", WS_CHILD | WS_VISIBLE, 0, 1068, 132, 149, 53, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1031, 185, 131, 48, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1032, 185, 140, 48, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1033, 185, 149, 48, 8);
        u32_create_child_control(hwnd, L"Static", L"Total", WS_CHILD | WS_VISIBLE, 0, 1069, 132, 174, 53, 8);
        u32_create_child_control(hwnd, L"Static", L"Paged", WS_CHILD | WS_VISIBLE, 0, 1070, 132, 184, 53, 8);
        u32_create_child_control(hwnd, L"Static", L"Nonpaged", WS_CHILD | WS_VISIBLE, 0, 1071, 132, 193, 53, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1034, 185, 174, 48, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1035, 185, 184, 48, 8);
        u32_create_child_control(hwnd, L"Edit", L"", WS_CHILD | WS_VISIBLE, 0, 1036, 185, 193, 48, 8);
        u32_create_child_control(hwnd, L"Button", L"CPU usage history", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1045, 74, 5, 168, 54);
        u32_create_child_control(hwnd, L"Button", L"Memory usage history", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 0, 1046, 74, 63, 168, 54);
        u32_create_child_control(hwnd, L"Button", L"", WS_CHILD | WS_VISIBLE, 0, 1047, 12, 17, 47, 37);
        u32_create_child_control(hwnd, L"Button", L"", WS_CHILD | WS_VISIBLE, 0, 1048, 12, 75, 47, 37);
        u32_create_child_control(hwnd, L"Button", L"", WS_CHILD | WS_VISIBLE, 0, 1050, 81, 17, 153, 37);
        u32_create_child_control(hwnd, L"Button", L"", WS_CHILD | WS_VISIBLE, 0, 1049, 81, 75, 153, 37);
        break;
    default:
        break;
    }
}

static void u32_paint_window(HWND hwnd) {
    int i;
    U32_WINDOW *win = u32_lookup_window(hwnd);
    if (!win || !win->visible) return;
    SendMessageW(hwnd, WM_PAINT, 0, 0);
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hwnd && g_windows[i].visible) {
            u32_paint_window(g_windows[i].hwnd);
        }
    }
}

static void u32_paint_children(HWND hwnd) {
    int i;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hwnd && g_windows[i].visible) {
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
    DWORD owner_pid = GetCurrentProcessId();
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
    DWORD current_pid = GetCurrentProcessId();
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
    DWORD current_pid = GetCurrentProcessId();
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].visible && g_windows[i].invalidated &&
            g_windows[i].owner_pid == current_pid) {
            if (!filter || g_windows[i].hwnd == filter) return (int)i;
        }
    }
    return -1;
}

static void u32_mark_invalid(HWND hwnd) {
    U32_WINDOW *win = u32_lookup_window(hwnd);
    while (win) {
        win->invalidated = 1;
        if (!win->parent) break;
        win = u32_lookup_window(win->parent);
    }
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

static void u32_flush_invalid_window(HWND hwnd) {
    int i;
    U32_WINDOW *win = u32_lookup_window(hwnd);
    static int logged_flush = 0;
    if (!win || !win->visible) return;
    if (win->invalidated && !win->painting) {
        if (logged_flush < 12) {
            logged_flush++;
            SerialPutString("[USER32] flush invalid begin\r\n");
        }
        UpdateWindow(hwnd);
        if (logged_flush <= 12) {
            SerialPutString("[USER32] flush invalid end\r\n");
        }
        u32_clear_invalid_subtree(hwnd);
        return;
    }
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hwnd && g_windows[i].visible) {
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
    return RegisterClassW(&wc);
}

LRESULT DefWindowProcW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    PAINTSTRUCT ps;
    HDC hdc;
    RECT rc;
    RECT line;
    (void)wParam;
    (void)lParam;
    if (Msg == WM_CLOSE) {
        if (win && win->dialog) {
            win->ended = 1;
            win->dialog_result = IDCANCEL;
        } else if (win) {
            DestroyWindow(hWnd);
        }
    } else if (Msg == WM_PAINT && win) {
        if (win->painting) return 0;
        win->painting = 1;
        hdc = BeginPaint(hWnd, &ps);
        GetClientRect(hWnd, &rc);
        switch (win->ctrl_type) {
        case U32_CTRL_DIALOG:
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
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
                    if (i == 0) TextOutW(hdc, tx + 6, 5, L"Applications", -1);
                    else if (i == 1) TextOutW(hdc, tx + 6, 5, L"Processes", -1);
                    else if (i == 2) TextOutW(hdc, tx + 6, 5, L"Performance", -1);
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
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
            Rectangle(hdc, 0, 0, rc.right, rc.bottom);
            if (win->title[0]) TextOutW(hdc, 4, (rc.bottom > 12) ? ((rc.bottom - 8) / 2) - 2 : 0, win->title, -1);
            break;
        case U32_CTRL_STATIC:
            if (win->title[0]) TextOutW(hdc, 0, 0, win->title, -1);
            break;
        case U32_CTRL_EDIT:
            FillRect(hdc, &rc, (HBRUSH)GetStockObject(0));
            Rectangle(hdc, 0, 0, rc.right, rc.bottom);
            if (win->title[0]) TextOutW(hdc, 2, 0, win->title, -1);
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
        EndPaint(hWnd, &ps);
        win->painting = 0;
        return 0;
    }
    return 0;
}

BOOL GetMessageW(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax) {
    if (!lpMsg) return FALSE;
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

        if (g_message_loop_exit) {
            lpMsg->hwnd = NULL;
            lpMsg->message = WM_QUIT;
            lpMsg->wParam = (WPARAM)g_quit_exit_code;
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
    if (nCmdShow == SW_HIDE) {
        win->visible = 0;
        if (win->parent) u32_mark_invalid(win->parent);
        return TRUE;
    }
    win->visible = 1;
    if (win->top_level) Win32kShowWindow((HANDLE)hWnd);
    else {
        u32_mark_invalid_descendants(hWnd);
        u32_mark_invalid(hWnd);
        if (win->parent) u32_mark_invalid(win->parent);
    }
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
    g_message_loop_exit = 1;
    g_quit_exit_code = nExitCode;
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
    (void)hInstance;
    (void)lpParam;

    cls = u32_find_class(lpClassName);
    if (!cls) {
        WNDCLASSW wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpszClassName = lpClassName;
        RegisterClassW(&wc);
        cls = u32_find_class(lpClassName);
        if (!cls) return NULL;
    }

    win = u32_alloc_window();
    if (!win) return NULL;
    u32_wstrcpy(win->title, lpWindowName, 128);
    win->parent = hWndParent;
    win->owner = hWndParent;
    win->style = dwStyle;
    win->exstyle = dwExStyle;
    win->menu = hMenu;
    win->enabled = TRUE;
    win->visible = (dwStyle & WS_VISIBLE) ? TRUE : FALSE;
    win->owner_pid = hWndParent ? (u32_lookup_window(hWndParent) ? u32_lookup_window(hWndParent)->owner_pid : GetCurrentProcessId())
                                : GetCurrentProcessId();
    win->klass = cls;
    win->proc = cls->proc;
    win->dialog = FALSE;
    win->ctrl_type = u32_pick_ctrl_type(lpClassName, dwStyle);
    u32_set_rect(win, X, Y, nWidth, nHeight);

    if (hWndParent && (dwStyle & WS_CHILD)) {
        win->top_level = 0;
        win->hwnd = (HWND)win;
        win->id = (UINT)(UINT_PTR)hMenu;
    } else {
        win->top_level = 1;
        u32_wide_to_ansi(cls->name, class_name, 64);
        u32_wide_to_ansi(win->title, title, 128);
        if (nWidth <= 0) nWidth = 480;
        if (nHeight <= 0) nHeight = 320;
        win->hwnd = (HWND)Win32kCreateWindow(class_name, title, X, Y, nWidth, nHeight, dwStyle);
        if (!win->hwnd) {
            win->used = 0;
            return NULL;
        }
    }

    SendMessageW(win->hwnd, WM_CREATE, 0, 0);
    SendMessageW(win->hwnd, WM_SIZE, SIZE_RESTORED,
                 MAKELPARAM(win->rect.right - win->rect.left, win->rect.bottom - win->rect.top));
    if (win->visible && win->top_level) {
        Win32kShowWindow((HANDLE)win->hwnd);
        Win32kRedrawAll();
    }
    return win->hwnd;
}

HWND CreateDialogW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc) {
    U32_WINDOW *win;
    UINT tmpl_id = u32_is_int_resource(lpTemplate) ? u32_resource_id(lpTemplate) : 0;
    HWND actual_parent = hWndParent;
    if ((tmpl_id == 106 || tmpl_id == 133 || tmpl_id == 134) && hWndParent) {
        HWND tab = GetDlgItem(hWndParent, 1015);
        if (tab) actual_parent = tab;
    }
    HWND hwnd = CreateWindowExW(0, L"#32770", L"Dialog", WS_CHILD | WS_VISIBLE, 15, 30,
                                (tmpl_id == 134) ? 247 : 247, (tmpl_id == 134) ? 210 : 210,
                                actual_parent, 0, hInstance, NULL);
    win = u32_lookup_window(hwnd);
    if (!win) return NULL;
    win->dialog = TRUE;
    win->dlgproc = lpDialogFunc;
    u32_taskmgr_create_dialog_children(tmpl_id, hwnd);
    SendMessageW(hwnd, WM_INITDIALOG, 0, 0);
    UpdateWindow(hwnd);
    return hwnd;
}

INT_PTR DialogBoxW(HINSTANCE hInstance, LPCWSTR lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc) {
    HWND hwnd;
    U32_WINDOW *win;
    UINT tmpl_id = u32_is_int_resource(lpTemplate) ? u32_resource_id(lpTemplate) : 0;
    SerialPutString("[USER32] DialogBoxW begin\r\n");
    hwnd = CreateWindowExW(0, L"#32770", L"Task Manager", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           40, 40, 540, 420, hWndParent, 0, hInstance, NULL);
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
    u32_taskmgr_create_dialog_children(tmpl_id, hwnd);
    SerialPutString("[USER32] DialogBoxW init\r\n");
    SendMessageW(hwnd, WM_INITDIALOG, 0, 0);
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SerialPutString("[USER32] DialogBoxW loop\r\n");
    while (!win->ended) {
        u32_pump_timers(hwnd);
        u32_flush_invalid_window(hwnd);
        KeYield();
    }
    SerialPutString("[USER32] DialogBoxW end\r\n");
    return win->dialog_result;
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

    if ((Msg == WM_MOUSEMOVE || Msg == WM_LBUTTONDOWN || Msg == WM_LBUTTONUP) && win->top_level) {
        int x = (short)(lParam & 0xFFFF);
        int y = (short)((lParam >> 16) & 0xFFFF);
        HWND child = u32_hit_test_child(hWnd, x, y);
        if (child && child != hWnd) {
            U32_WINDOW *child_win = u32_lookup_window(child);
            if (Msg == WM_LBUTTONDOWN) {
                SetFocus(child);
            }
            if (child_win) {
                int child_x = x - child_win->rect.left;
                int child_y = y - child_win->rect.top;
                return SendMessageW(child, Msg, wParam, MAKELPARAM(child_x, child_y));
            }
        }
    }

    if (Msg == WM_PAINT) {
        u32_clear_invalid_subtree(hWnd);
    }

    switch (Msg) {
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
    case DM_SETDEFID:
    case WM_SETREDRAW:
        return 0;
    case BM_SETCHECK:
        win->check_state = (int)wParam;
        return 0;
    case BM_GETCHECK:
        return (LRESULT)win->check_state;
    case TCM_INSERTITEMW:
        if (win->tab_count < MAX_TAB_ITEMS) win->tab_count++;
        return (LRESULT)(win->tab_count - 1);
    case TCM_SETCURFOCUS:
        if (win->tab_cur_sel != (int)wParam) {
            win->tab_cur_sel = (int)wParam;
            if (win->parent) {
                NMHDR hdr;
                memset(&hdr, 0, sizeof(hdr));
                hdr.hwndFrom = hWnd;
                hdr.idFrom = (UINT_PTR)win->id;
                hdr.code = TCN_SELCHANGE;
                SendMessageW(win->parent, WM_NOTIFY, (WPARAM)win->id, (LPARAM)&hdr);
            }
            u32_mark_invalid(hWnd);
        }
        return 0;
    case TCM_GETCURSEL:
        return (LRESULT)win->tab_cur_sel;
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
    if (!win) return FALSE;
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
    return TRUE;
}

BOOL GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT *lpwndpl) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win || !lpwndpl) return FALSE;
    lpwndpl->length = sizeof(*lpwndpl);
    lpwndpl->flags = 0;
    lpwndpl->showCmd = win->visible ? SW_SHOW : SW_HIDE;
    lpwndpl->rcNormalPosition = win->rect;
    return TRUE;
}

BOOL SetWindowPlacement(HWND hWnd, const WINDOWPLACEMENT *lpwndpl) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win || !lpwndpl) return FALSE;
    win->rect = lpwndpl->rcNormalPosition;
    u32_sync_top_level(win);
    return TRUE;
}

UINT SetTimer(HWND hWnd, UINT_PTR nIDEvent, UINT uElapse, void *lpTimerFunc) {
    int i;
    (void)lpTimerFunc;
    for (i = 0; i < MAX_U32_TIMERS; i++) {
        if (!g_timers[i].used) {
            g_timers[i].used = 1;
            g_timers[i].hwnd = hWnd;
            g_timers[i].id = nIDEvent;
            g_timers[i].elapse = uElapse;
            g_timers[i].tick = GetTickCount() + (uElapse ? uElapse : 1);
            return (UINT)nIDEvent;
        }
    }
    return 0;
}

BOOL KillTimer(HWND hWnd, UINT_PTR uIDEvent) {
    int i;
    for (i = 0; i < MAX_U32_TIMERS; i++) {
        if (g_timers[i].used && g_timers[i].hwnd == hWnd && g_timers[i].id == uIDEvent) {
            memset(&g_timers[i], 0, sizeof(g_timers[i]));
            return TRUE;
        }
    }
    return FALSE;
}

HICON LoadIconW(HINSTANCE hInstance, LPCWSTR lpIconName) {
    (void)hInstance; (void)lpIconName;
    return (HICON)1;
}

HANDLE LoadImageW(HINSTANCE hinst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad) {
    (void)hinst; (void)name; (void)type; (void)cx; (void)cy; (void)fuLoad;
    return (HANDLE)1;
}

int LoadStringW(HINSTANCE hInstance, UINT uID, LPWSTR lpBuffer, int cchBufferMax) {
    LPCWSTR text;
    int len;
    (void)hInstance;
    if (!lpBuffer || cchBufferMax <= 0) return 0;
    text = u32_string_for_id(uID);
    len = u32_wstrlen(text);
    if (len >= cchBufferMax) len = cchBufferMax - 1;
    memcpy(lpBuffer, text, len * sizeof(WCHAR));
    lpBuffer[len] = 0;
    return len;
}

int LoadStringA(HINSTANCE hInstance, UINT uID, LPSTR lpBuffer, int cchBufferMax) {
    WCHAR temp[128];
    int i, len;
    (void)hInstance;
    len = LoadStringW(hInstance, uID, temp, 128);
    if (!lpBuffer || cchBufferMax <= 0) return 0;
    if (len >= cchBufferMax) len = cchBufferMax - 1;
    for (i = 0; i < len; i++) lpBuffer[i] = (char)temp[i];
    lpBuffer[len] = 0;
    return len;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    int width;
    int height;
    if (!win || !lpRect) return FALSE;
    if (win->top_level) u32_sync_top_level(win);
    width = win->rect.right - win->rect.left;
    height = win->rect.bottom - win->rect.top;
    lpRect->left = 0;
    lpRect->top = 0;
    if (win->top_level) {
        width -= (u32_client_offset_x(win) + U32_FRAME_THICKNESS + U32_EDGE_THICKNESS);
        height -= (u32_client_offset_y(win) + U32_FRAME_THICKNESS + U32_EDGE_THICKNESS);
        if (width < 0) width = 0;
        if (height < 0) height = 0;
    }
    lpRect->right = width;
    lpRect->bottom = height;
    return TRUE;
}

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win || !lpRect) return FALSE;
    if (win->top_level) {
        u32_sync_top_level(win);
        *lpRect = win->rect;
    } else {
        u32_get_absolute_rect(win, lpRect);
    }
    return TRUE;
}

int GetWindowTextW(HWND hWnd, LPWSTR lpString, int nMaxCount) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win || !lpString || nMaxCount <= 0) return 0;
    if (win->ctrl_type == U32_CTRL_EDIT && win->edit_text) u32_lv_copy_out(lpString, nMaxCount, win->edit_text);
    else u32_lv_copy_out(lpString, nMaxCount, win->title);
    return u32_wstrlen(lpString);
}

int GetWindowTextLengthW(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return 0;
    if (win->ctrl_type == U32_CTRL_EDIT) return win->edit_len;
    return u32_wstrlen(win->title);
}

HWND GetDlgItem(HWND hDlg, int nIDDlgItem) {
    int i;
    for (i = 0; i < MAX_U32_WINDOWS; i++) {
        if (g_windows[i].used && g_windows[i].parent == hDlg && g_windows[i].id == (UINT)nIDDlgItem) return g_windows[i].hwnd;
    }
    return u32_create_placeholder_child(hDlg, (UINT)nIDDlgItem);
}

UINT GetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPWSTR lpString, int cchMax) {
    HWND child = GetDlgItem(hDlg, nIDDlgItem);
    if (!child || !lpString || cchMax <= 0) return 0;
    return (UINT)GetWindowTextW(child, lpString, cchMax);
}

BOOL SetDlgItemTextW(HWND hDlg, int nIDDlgItem, LPCWSTR lpString) {
    HWND child = GetDlgItem(hDlg, nIDDlgItem);
    if (!child) return FALSE;
    return SetWindowTextW(child, lpString);
}

UINT GetDlgItemInt(HWND hDlg, int nIDDlgItem, BOOL *lpTranslated, BOOL bSigned) {
    WCHAR buf[64];
    UINT value = 0;
    int i = 0;
    int neg = 0;
    if (lpTranslated) *lpTranslated = FALSE;
    if (!GetDlgItemTextW(hDlg, nIDDlgItem, buf, ARRAY_SIZE(buf))) return 0;
    if (bSigned && buf[0] == L'-') {
        neg = 1;
        i++;
    }
    while (buf[i] >= L'0' && buf[i] <= L'9') {
        value = value * 10 + (UINT)(buf[i] - L'0');
        i++;
    }
    if (lpTranslated) *lpTranslated = TRUE;
    return neg ? (UINT)(-(int)value) : value;
}

BOOL SetDlgItemInt(HWND hDlg, int nIDDlgItem, UINT uValue, BOOL bSigned) {
    WCHAR buf[32];
    int pos = 0;
    if (bSigned) pos = u32_append_number(buf, pos, ARRAY_SIZE(buf), (int)uValue, 0);
    else pos = u32_append_number(buf, pos, ARRAY_SIZE(buf), (int)uValue, 1);
    buf[pos] = 0;
    return SetDlgItemTextW(hDlg, nIDDlgItem, buf);
}

BOOL SetRect(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom) {
    if (!lprc) return FALSE;
    lprc->left = xLeft;
    lprc->top = yTop;
    lprc->right = xRight;
    lprc->bottom = yBottom;
    return TRUE;
}

int GetSystemMetrics(int nIndex) {
    switch (nIndex) {
    case SM_CXSCREEN:
        return 800;
    case SM_CYSCREEN:
        return 600;
    case SM_CXSMICON:
    case SM_CYSMICON:
        return 16;
    default:
        return 0;
    }
}

UINT GetDpiForWindow(HWND hWnd) {
    (void)hWnd;
    return 96;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    (void)hWndInsertAfter;
    if (!win) return FALSE;
    if (!(uFlags & SWP_NOMOVE)) {
        win->rect.left = X;
        win->rect.top = Y;
    }
    if (!(uFlags & SWP_NOSIZE)) {
        win->rect.right = win->rect.left + cx;
        win->rect.bottom = win->rect.top + cy;
    }
    if (uFlags & SWP_SHOWWINDOW) win->visible = 1;
    u32_sync_top_level(win);
    SendMessageW(hWnd, WM_SIZE, SIZE_RESTORED,
                 MAKELPARAM(win->rect.right - win->rect.left, win->rect.bottom - win->rect.top));
    return TRUE;
}

HMENU GetMenu(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return NULL;
    if (!win->menu) win->menu = CreatePopupMenu();
    return win->menu;
}

HMENU GetSubMenu(HMENU hMenu, int nPos) {
    U32_MENU *menu = u32_lookup_menu(hMenu);
    if (!menu || nPos < 0 || nPos >= menu->count) return NULL;
    return menu->items[nPos].submenu;
}

int GetMenuItemCount(HMENU hMenu) {
    U32_MENU *menu = u32_lookup_menu(hMenu);
    return menu ? menu->count : -1;
}

BOOL CheckMenuRadioItem(HMENU hmenu, UINT first, UINT last, UINT check, UINT flags) {
    U32_MENU *menu = u32_lookup_menu(hmenu);
    int i;
    (void)flags;
    if (!menu) return FALSE;
    for (i = 0; i < menu->count; i++) {
        if (menu->items[i].id >= first && menu->items[i].id <= last) {
            menu->items[i].flags &= ~MF_CHECKED;
            if (menu->items[i].id == check) menu->items[i].flags |= MF_CHECKED;
        }
    }
    return TRUE;
}

BOOL CheckMenuItem(HMENU hmenu, UINT idCheckItem, UINT uCheck) {
    U32_MENU *menu = u32_lookup_menu(hmenu);
    int i;
    if (!menu) return FALSE;
    for (i = 0; i < menu->count; i++) {
        if (menu->items[i].id == idCheckItem) {
            menu->items[i].flags = uCheck;
            return TRUE;
        }
    }
    return FALSE;
}

BOOL EnableMenuItem(HMENU hMenu, UINT uIDEnableItem, UINT uEnable) {
    return CheckMenuItem(hMenu, uIDEnableItem, uEnable);
}

BOOL DestroyMenu(HMENU hMenu) {
    U32_MENU *menu = u32_lookup_menu(hMenu);
    if (!menu) return FALSE;
    memset(menu, 0, sizeof(*menu));
    return TRUE;
}

BOOL RemoveMenu(HMENU hMenu, UINT uPosition, UINT uFlags) {
    U32_MENU *menu = u32_lookup_menu(hMenu);
    int index = (int)uPosition;
    int i;
    if (!menu) return FALSE;
    if (!(uFlags & MF_BYPOSITION)) {
        for (index = 0; index < menu->count; index++) if (menu->items[index].id == uPosition) break;
    }
    if (index < 0 || index >= menu->count) return FALSE;
    for (i = index; i < menu->count - 1; i++) menu->items[i] = menu->items[i + 1];
    menu->count--;
    return TRUE;
}

BOOL AppendMenuW(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem) {
    U32_MENU *menu = u32_lookup_menu(hMenu);
    if (!menu || menu->count >= 32) return FALSE;
    menu->items[menu->count].id = (UINT)uIDNewItem;
    menu->items[menu->count].flags = uFlags;
    menu->items[menu->count].submenu = (uFlags & MF_POPUP) ? (HMENU)uIDNewItem : NULL;
    u32_wstrcpy(menu->items[menu->count].text, lpNewItem, 64);
    menu->count++;
    return TRUE;
}

HMENU LoadMenuW(HINSTANCE hInstance, LPCWSTR lpMenuName) {
    U32_MENU *root = u32_alloc_menu();
    U32_MENU *sub = u32_alloc_menu();
    (void)hInstance;
    (void)lpMenuName;
    if (!root || !sub) return NULL;
    AppendMenuW(root->handle, MF_POPUP, (UINT_PTR)sub->handle, L"Menu");
    return root->handle;
}

BOOL InsertMenuW(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem) {
    U32_MENU *menu = u32_lookup_menu(hMenu);
    int i;
    if (!menu || menu->count >= 32) return FALSE;
    if (!(uFlags & MF_BYPOSITION)) return AppendMenuW(hMenu, uFlags, uIDNewItem, lpNewItem);
    if ((int)uPosition > menu->count) uPosition = (UINT)menu->count;
    for (i = menu->count; i > (int)uPosition; i--) menu->items[i] = menu->items[i - 1];
    memset(&menu->items[uPosition], 0, sizeof(menu->items[uPosition]));
    menu->items[uPosition].id = (UINT)uIDNewItem;
    menu->items[uPosition].flags = uFlags;
    menu->items[uPosition].submenu = (uFlags & MF_POPUP) ? (HMENU)uIDNewItem : NULL;
    u32_wstrcpy(menu->items[uPosition].text, lpNewItem, 64);
    menu->count++;
    return TRUE;
}

BOOL DrawMenuBar(HWND hWnd) { (void)hWnd; return TRUE; }
BOOL BringWindowToTop(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return FALSE;
    if (win->top_level) {
        Win32kActivateWindow((HANDLE)hWnd);
    } else {
        u32_mark_invalid_descendants(hWnd);
        u32_mark_invalid(hWnd);
        if (win->parent) u32_mark_invalid(win->parent);
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
HWND SetActiveWindow(HWND hWnd) {
    HWND old = g_active_window;
    g_active_window = hWnd;
    if (hWnd) BringWindowToTop(hWnd);
    return old;
}
HWND GetDesktopWindow(void) { return (HWND)1; }
HMENU CreatePopupMenu(void) { U32_MENU *menu = u32_alloc_menu(); return menu ? menu->handle : NULL; }
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
    return (LONG_PTR)SetWindowLongW(hWnd, nIndex, (LONG)dwNewLong);
}
LONG_PTR GetClassLongPtrW(HWND hWnd, int nIndex) { (void)hWnd; (void)nIndex; return 0; }
BOOL IsWindowVisible(HWND hWnd) { U32_WINDOW *win = u32_lookup_window(hWnd); return win ? win->visible : FALSE; }
BOOL IsIconic(HWND hWnd) { (void)hWnd; return FALSE; }
BOOL DeleteMenu(HMENU hMenu, UINT uPosition, UINT uFlags) { return RemoveMenu(hMenu, uPosition, uFlags); }
BOOL SetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos) { U32_MENU *menu = u32_lookup_menu(hMenu); (void)fByPos; if (!menu) return FALSE; menu->default_item = (int)uItem; return TRUE; }
BOOL TrackPopupMenuEx(HMENU hmenu, UINT fuFlags, int x, int y, HWND hwnd, void *lptpm) { (void)hmenu; (void)fuFlags; (void)x; (void)y; (void)hwnd; (void)lptpm; return TRUE; }
UINT GetMenuState(HMENU hMenu, UINT uId, UINT uFlags) { U32_MENU *menu = u32_lookup_menu(hMenu); int i; (void)uFlags; if (!menu) return 0; for (i = 0; i < menu->count; i++) if (menu->items[i].id == uId) return menu->items[i].flags; return 0; }
HWND GetWindow(HWND hWnd, UINT uCmd) { (void)uCmd; return hWnd; }
LRESULT CallWindowProcW(WNDPROC lpPrevWndFunc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) { return lpPrevWndFunc ? lpPrevWndFunc(hWnd, Msg, wParam, lParam) : 0; }
HDC GetDC(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (win && !win->top_level) {
        RECT rc;
        u32_get_absolute_rect(win, &rc);
        return GdiCreateScreenDCEx(u32_get_root_window(hWnd), rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
    }
    return GdiCreateScreenDC(hWnd);
}
int ReleaseDC(HWND hWnd, HDC hDC) { (void)hWnd; if (hDC) GdiDestroyScreenDC(hDC); return 1; }
DWORD GetSysColor(int nIndex) { (void)nIndex; return RGB(192,192,192); }
HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT lpPaint) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    HDC hdc;
    if (win && !win->top_level) {
        RECT rc;
        u32_get_absolute_rect(win, &rc);
        hdc = GdiCreateScreenDCEx(u32_get_root_window(hWnd), rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
    } else {
        hdc = GdiCreateScreenDC(hWnd);
    }
    if (lpPaint) {
        memset(lpPaint, 0, sizeof(*lpPaint));
        lpPaint->hdc = hdc;
        GetClientRect(hWnd, &lpPaint->rcPaint);
    }
    return hdc;
}
BOOL EndPaint(HWND hWnd, const PAINTSTRUCT *lpPaint) {
    (void)hWnd;
    if (lpPaint && lpPaint->hdc) GdiDestroyScreenDC(lpPaint->hdc);
    return TRUE;
}
BOOL OpenIcon(HWND hWnd) { return ShowWindow(hWnd, SW_RESTORE); }
BOOL SetForegroundWindow(HWND hWnd) { BringWindowToTop(hWnd); return TRUE; }
int wsprintfW(LPWSTR lpOut, LPCWSTR lpFmt, ...) { int r; va_list ap; va_start(ap, lpFmt); r = u32_vsnprintfw(lpOut, 1024, lpFmt, ap); va_end(ap); return r; }
int wnsprintfW(LPWSTR buffer, int count, LPCWSTR format, ...) { int r; va_list ap; va_start(ap, format); r = u32_vsnprintfw(buffer, count, format, ap); va_end(ap); return r; }
int swprintf(WCHAR *buffer, size_t count, const WCHAR *format, ...) { int r; va_list ap; va_start(ap, format); r = u32_vsnprintfw(buffer, (int)count, format, ap); va_end(ap); return r; }
BOOL EnableWindow(HWND hWnd, BOOL bEnable) { U32_WINDOW *win = u32_lookup_window(hWnd); if (!win) return FALSE; win->enabled = bEnable; return TRUE; }
BOOL IsClipboardFormatAvailable(UINT format) { (void)format; return FALSE; }
BOOL CopyRect(LPRECT lprcDst, const RECT *lprcSrc) { if (!lprcDst || !lprcSrc) return FALSE; *lprcDst = *lprcSrc; return TRUE; }
int DrawTextW(HDC hdc, LPCWSTR lpchText, int cchText, LPRECT lprc, UINT format) {
    int len = cchText;
    int x = 0, y = 0;
    (void)format;
    if (!lpchText) return 0;
    if (len < 0) len = u32_wstrlen(lpchText);
    if (lprc) {
        x = lprc->left;
        y = lprc->top;
    }
    TextOutW(hdc, x, y, lpchText, len);
    return len;
}
BOOL InvalidateRect(HWND hWnd, const RECT *lpRect, BOOL bErase) {
    static int g_logged_graph_invalidates = 0;
    U32_WINDOW *win = u32_lookup_window(hWnd);
    (void)lpRect;
    (void)bErase;
    if (win && g_logged_graph_invalidates < 8 &&
        (win->id == 1049 || win->id == 1050 || win->id == 1047 || win->id == 1048)) {
        g_logged_graph_invalidates++;
        SerialPutString("[USER32] graph invalidate\r\n");
    }
    u32_mark_invalid(hWnd);
    return TRUE;
}
BOOL IsWindow(HWND hWnd) { return u32_lookup_window(hWnd) ? TRUE : FALSE; }
HWND GetParent(HWND hWnd) { U32_WINDOW *win = u32_lookup_window(hWnd); return win ? win->parent : NULL; }
int MapWindowPoints(HWND hWndFrom, HWND hWndTo, LPPOINT lpPoints, UINT cPoints) {
    U32_WINDOW *from = u32_lookup_window(hWndFrom);
    U32_WINDOW *to = u32_lookup_window(hWndTo);
    RECT frc, trc;
    int dx = 0, dy = 0;
    UINT i;
    if (!lpPoints) return 0;
    if (from) { u32_get_absolute_rect(from, &frc); dx += frc.left; dy += frc.top; }
    if (to) { u32_get_absolute_rect(to, &trc); dx -= trc.left; dy -= trc.top; }
    for (i = 0; i < cPoints; i++) {
        lpPoints[i].x += dx;
        lpPoints[i].y += dy;
    }
    return (int)cPoints;
}
BOOL SetWindowTextW(HWND hWnd, LPCWSTR lpString) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return FALSE;
    if (win->ctrl_type == U32_CTRL_EDIT) {
        u32_edit_set_text(win, lpString ? lpString : L"");
    } else {
        u32_wstrcpy(win->title, lpString, 128);
    }
    u32_mark_invalid(hWnd);
    return TRUE;
}
BOOL TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect) { (void)hMenu; (void)uFlags; (void)x; (void)y; (void)nReserved; (void)hWnd; (void)prcRect; return TRUE; }
LRESULT SendMessageTimeoutW(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, UINT fuFlags, UINT uTimeout, PDWORD_PTR lpdwResult) { LRESULT r = SendMessageW(hWnd, Msg, wParam, lParam); (void)fuFlags; (void)uTimeout; if (lpdwResult) *lpdwResult = (DWORD_PTR)r; return r; }
BOOL EnumWindows(BOOL (CALLBACK *lpEnumFunc)(HWND, LPARAM), LPARAM lParam) { int i; for (i = 0; i < MAX_U32_WINDOWS; i++) if (g_windows[i].used && g_windows[i].top_level) if (!lpEnumFunc(g_windows[i].hwnd, lParam)) break; return TRUE; }
BOOL IsHungAppWindow(HWND hWnd) { (void)hWnd; return FALSE; }
BOOL DestroyIcon(HICON hIcon) { (void)hIcon; return TRUE; }
WORD TileWindows(HWND hwndParent, UINT wHow, const RECT *lpRect, UINT cKids, const HWND *lpKids) { (void)hwndParent; (void)wHow; (void)lpRect; (void)cKids; (void)lpKids; return 0; }
WORD CascadeWindows(HWND hwndParent, UINT wHow, const RECT *lpRect, UINT cKids, const HWND *lpKids) { (void)hwndParent; (void)wHow; (void)lpRect; (void)cKids; (void)lpKids; return 0; }
void SwitchToThisWindow(HWND hWnd, BOOL fAltTab) { (void)fAltTab; BringWindowToTop(hWnd); }
DWORD GetWindowThreadProcessId(HWND hWnd, DWORD *lpdwProcessId) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    DWORD pid = win ? win->owner_pid : 0;
    if (lpdwProcessId) *lpdwProcessId = pid;
    return pid;
}
HCURSOR LoadCursorW(HINSTANCE hInstance, LPCWSTR lpCursorName) { (void)hInstance; (void)lpCursorName; return (HCURSOR)1; }
UINT RegisterWindowMessageW(LPCWSTR lpString) { static UINT next = 0xC000; (void)lpString; return next++; }
BOOL IsDialogMessageW(HWND hDlg, LPMSG lpMsg) { (void)hDlg; (void)lpMsg; return FALSE; }
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
