/* Small native RichEdit implementation used by WinHelp32.  This is
 * deliberately a control DLL, not an application-side compatibility shim. */
#include <stdint.h>
#include "windows.h"
#include "richedit.h"
#include "wingdi.h"

#define RICH_MAX_CONTROLS 32
#define RICH_MAX_TEXT 16384

typedef struct {
    HWND hwnd;
    WCHAR text[RICH_MAX_TEXT];
    int length;
    int sel_start, sel_end;
    int event_mask;
    int scroll_x, scroll_y;
    COLORREF background;
} RICH_CONTROL;

static RICH_CONTROL controls[RICH_MAX_CONTROLS];

static RICH_CONTROL *rich_control(HWND hwnd, int create) {
    int i, free_slot = -1;
    for (i = 0; i < RICH_MAX_CONTROLS; i++) {
        if (controls[i].hwnd == hwnd) return &controls[i];
        if (!controls[i].hwnd && free_slot < 0) free_slot = i;
    }
    if (!create || free_slot < 0) return 0;
    controls[free_slot].hwnd = hwnd;
    controls[free_slot].background = RGB(255, 255, 255);
    return &controls[free_slot];
}

static int rich_copy_plain(RICH_CONTROL *control, const char *source, int bytes) {
    int i = 0, out = 0, brace_depth = 0, control_word;
    if (!control || !source) return 0;
    while (i < bytes && out < RICH_MAX_TEXT - 1) {
        unsigned char ch = (unsigned char)source[i++];
        if (ch == '{') { brace_depth++; continue; }
        if (ch == '}') { if (brace_depth) brace_depth--; continue; }
        if (ch == '\\') {
            if (i < bytes && source[i] == '\\') { control->text[out++] = '\\'; i++; continue; }
            if (i + 3 < bytes && source[i] == '\'' &&
                ((source[i + 1] >= '0' && source[i + 1] <= '9') ||
                 (source[i + 1] >= 'a' && source[i + 1] <= 'f') ||
                 (source[i + 1] >= 'A' && source[i + 1] <= 'F'))) {
                int hi = source[i + 1], lo = source[i + 2];
                hi = hi >= 'a' ? hi - 'a' + 10 : hi >= 'A' ? hi - 'A' + 10 : hi - '0';
                lo = lo >= 'a' ? lo - 'a' + 10 : lo >= 'A' ? lo - 'A' + 10 : lo - '0';
                control->text[out++] = (WCHAR)((hi << 4) | lo); i += 3; continue;
            }
            if (i < bytes && (source[i] == '\n' || source[i] == '\r')) { i++; continue; }
            control_word = (i < bytes && source[i] == 'p' && i + 2 < bytes &&
                            source[i + 1] == 'a' && source[i + 2] == 'r');
            if (control_word) control->text[out++] = L'\n';
            control_word = 1;
            while (i < bytes && control_word) {
                ch = (unsigned char)source[i];
                if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '-' || (ch >= '0' && ch <= '9')) i++;
                else control_word = 0;
            }
            if (i < bytes && source[i] == ' ') i++;
            continue;
        }
        if (ch == '\r') continue;
        if (ch == '\n') { control->text[out++] = L'\n'; continue; }
        if (ch >= 32 || ch == '\t') control->text[out++] = (WCHAR)ch;
    }
    control->text[out] = 0;
    control->length = out;
    control->sel_start = control->sel_end = out;
    return out;
}

static void rich_set_text(RICH_CONTROL *control, const WCHAR *text) {
    int i = 0;
    if (!control) return;
    if (!text) text = L"";
    while (text[i] && i < RICH_MAX_TEXT - 1) { control->text[i] = text[i]; i++; }
    control->text[i] = 0;
    control->length = i;
    control->sel_start = control->sel_end = i;
}

static LRESULT CALLBACK rich_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    RICH_CONTROL *control = rich_control(hwnd, msg != WM_DESTROY);
    if (!control) return 0;
    switch (msg) {
    case WM_DESTROY:
        control->hwnd = 0;
        return 0;
    case WM_SETTEXT:
        rich_set_text(control, (const WCHAR *)lparam);
        InvalidateRect(hwnd, 0, TRUE);
        return TRUE;
    case WM_GETTEXT:
        if (lparam && wparam) {
            WCHAR *dst = (WCHAR *)lparam; int i, max = (int)wparam;
            for (i = 0; i < max - 1 && control->text[i]; i++) dst[i] = control->text[i];
            dst[i] = 0; return i;
        }
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; RECT rc; HDC dc; int x = 2, y = 2, i = 0;
        dc = BeginPaint(hwnd, &ps);
        GetClientRect(hwnd, &rc);
        { HBRUSH brush = CreateSolidBrush(control->background); FillRect(dc, &rc, brush); DeleteObject(brush); }
        SetTextColor(dc, RGB(0, 0, 0));
        while (control->text[i]) {
            int start = i;
            while (control->text[i] && control->text[i] != L'\n') i++;
            if (i > start) TextOutW(dc, x - control->scroll_x, y - control->scroll_y, &control->text[start], i - start);
            if (control->text[i] == L'\n') i++;
            y += 16;
            if (y > rc.bottom + control->scroll_y) break;
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case EM_SETBKGNDCOLOR:
        { COLORREF old = control->background; control->background = (COLORREF)lparam; InvalidateRect(hwnd, 0, TRUE); return old; }
    case EM_GETEVENTMASK: return control->event_mask;
    case EM_SETEVENTMASK: control->event_mask = (int)lparam; return control->event_mask;
    case EM_SETSEL: control->sel_start = (int)wparam; control->sel_end = (int)lparam; return 0;
    case EM_GETSEL:
        if (wparam) *(DWORD *)wparam = control->sel_start;
        if (lparam) *(DWORD *)lparam = control->sel_end;
        return MAKELONG(control->sel_start, control->sel_end);
    case EM_STREAMIN: {
        EDITSTREAM *stream = (EDITSTREAM *)lparam;
        char raw[RICH_MAX_TEXT]; LONG got, total = 0;
        if (!stream || !stream->pfnCallback) return 0;
        while (total < (LONG)sizeof(raw)) {
            got = 0;
            if (stream->pfnCallback(stream->dwCookie, (BYTE *)raw + total,
                                     (LONG)sizeof(raw) - total, &got) || got <= 0) break;
            total += got;
        }
        rich_copy_plain(control, raw, total);
        stream->dwError = 0;
        InvalidateRect(hwnd, 0, TRUE);
        return total;
    }
    case EM_POSFROMCHAR: {
        POINT *point = (POINT *)wparam; int cp = (int)lparam, col = 0, row = 0, i;
        if (!point) return -1;
        if (cp > control->length) cp = control->length;
        for (i = 0; i < cp; i++) { if (control->text[i] == L'\n') { row++; col = 0; } else col++; }
        point->x = 2 + col * 8 - control->scroll_x; point->y = 2 + row * 16 - control->scroll_y;
        return 0;
    }
    case EM_CHARFROMPOS: {
        POINT *point = (POINT *)lparam; int row, col, i, current = 0;
        if (!point) return 0;
        row = ((point->y + control->scroll_y - 2) / 16); col = ((point->x + control->scroll_x - 2) / 8);
        if (row < 0) row = 0; if (col < 0) col = 0;
        for (i = 0; i < control->length && current < row; i++) if (control->text[i] == L'\n') current++;
        while (i < control->length && col-- > 0 && control->text[i] != L'\n') i++;
        return MAKELONG(i, row);
    }
    case EM_SETSCROLLPOS:
        if (lparam) { POINT *p = (POINT *)lparam; control->scroll_x = p->x; control->scroll_y = p->y; InvalidateRect(hwnd, 0, TRUE); } return 0;
    case EM_SETTARGETDEVICE: return 0;
    case EM_REQUESTRESIZE: return 0;
    default: return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

int DllMain(void *module, uint32_t reason, void *reserved) {
    WNDCLASSEXA cls;
    (void)module; (void)reserved;
    if (reason != 1) return 1;
    ZeroMemory(&cls, sizeof(cls));
    cls.cbSize = sizeof(cls); cls.lpfnWndProc = rich_wndproc;
    cls.lpszClassName = "RichEdit20A";
    RegisterClassExA(&cls);
    return 1;
}
