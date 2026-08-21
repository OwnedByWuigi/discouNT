#include <stdint.h>
#include "windows.h"
#include "wingdi.h"
#include "string.h"
#include "core/version.h"

static WCHAR g_about_caption[128];
static WCHAR g_about_other[256];
static int g_about_result = IDOK;
static HICON g_about_icon = 0;

static int sh_wstrlen(const WCHAR *s) {
    int n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static void sh_wstrcpy(WCHAR *dst, const WCHAR *src, int max_chars) {
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

static void sh_ansi_to_wide(WCHAR *dst, const char *src, int max_chars) {
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

static int sh_append_ansi(WCHAR *dst, int pos, int max, const char *src) {
    int i = 0;
    if (!dst || !src || max <= 0) return pos;
    while (src[i] && pos < max - 1) dst[pos++] = (unsigned char)src[i++];
    dst[pos] = 0;
    return pos;
}

static int sh_append_wide(WCHAR *dst, int pos, int max, const WCHAR *src) {
    int i = 0;
    if (!dst || !src || max <= 0) return pos;
    while (src[i] && pos < max - 1) dst[pos++] = src[i++];
    dst[pos] = 0;
    return pos;
}

static int sh_append_uint(WCHAR *dst, int pos, int max, unsigned value) {
    WCHAR temp[16];
    int count = 0;
    if (value == 0) {
        if (pos < max - 1) dst[pos++] = L'0';
        dst[pos] = 0;
        return pos;
    }
    while (value && count < 16) {
        temp[count++] = (WCHAR)(L'0' + (value % 10));
        value /= 10;
    }
    while (count-- && pos < max - 1) dst[pos++] = temp[count];
    dst[pos] = 0;
    return pos;
}

static void sh_build_version_line(WCHAR *dst, int max_chars) {
    OSVERSIONINFOW ver;
    int pos = 0;
    if (!dst || max_chars <= 0) return;
    ver.dwOSVersionInfoSize = sizeof(ver);
    if (!GetVersionExW(&ver)) {
        sh_wstrcpy(dst, L"Version information unavailable", max_chars);
        return;
    }
    pos = sh_append_ansi(dst, pos, max_chars, "Reporting as NT version ");
    pos = sh_append_uint(dst, pos, max_chars, ver.dwMajorVersion);
    if (pos < max_chars - 1) dst[pos++] = L'.';
    pos = sh_append_uint(dst, pos, max_chars, ver.dwMinorVersion);
    pos = sh_append_ansi(dst, pos, max_chars, " (build ");
    pos = sh_append_uint(dst, pos, max_chars, ver.dwBuildNumber);
    pos = sh_append_ansi(dst, pos, max_chars, ")");
    dst[pos] = 0;
}

static int sh_point_in_rect(int x, int y, const RECT *rc) {
    if (!rc) return 0;
    return x >= rc->left && x < rc->right && y >= rc->top && y < rc->bottom;
}

static void sh_about_wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    RECT rc;
    RECT ok_rc;
    ok_rc.left = 198;
    ok_rc.top = 180;
    ok_rc.right = 280;
    ok_rc.bottom = 204;

    switch (msg) {
    case WM_LBUTTONDOWN:
        {
            int x = (short)(lParam & 0xFFFF);
            int y = (short)((lParam >> 16) & 0xFFFF);
            if (sh_point_in_rect(x, y, &ok_rc)) {
                g_about_result = IDOK;
                DestroyWindow(hwnd);
            }
        }
        return;
    case WM_KEYDOWN:
        if (wParam == VK_RETURN || wParam == 0x1B) {
            g_about_result = IDOK;
            DestroyWindow(hwnd);
            return;
        }
        break;
    case WM_CLOSE:
        g_about_result = IDCANCEL;
        DestroyWindow(hwnd);
        return;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            HBRUSH bg = CreateSolidBrush(RGB(192, 192, 192));
            HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
            HBRUSH blue = CreateSolidBrush(RGB(0, 0, 128));
            HBRUSH button = CreateSolidBrush(RGB(224, 224, 224));
            WCHAR line[128];
            int ok_text_x;

            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, bg);

            rc.left = 12; rc.top = 20; rc.right = 466; rc.bottom = 164;
            FillRect(hdc, &rc, white);
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

            rc.left = 24; rc.top = 32; rc.right = 96; rc.bottom = 104;
            FillRect(hdc, &rc, white);
            Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);

            SetTextColor(hdc, RGB(0,0,0));
            TextOutW(hdc, 116, 42, L"discouNT", -1);
            sh_build_version_line(line, 128);
            TextOutW(hdc, 116, 62, line, -1);

            if (g_about_other[0]) TextOutW(hdc, 24, 134, g_about_other, -1);

            FillRect(hdc, &ok_rc, button);
            Rectangle(hdc, ok_rc.left, ok_rc.top, ok_rc.right, ok_rc.bottom);
            ok_text_x = ok_rc.left + 28;
            TextOutW(hdc, ok_text_x, ok_rc.top + 7, L"OK", -1);

            DeleteObject(button);
            DeleteObject(white);
            DeleteObject(bg);
            EndPaint(hwnd, &ps);
        }
        return;
    default:
        break;
    }
    DefWindowProcW(hwnd, msg, wParam, lParam);
}

static int sh_run_about_window(const WCHAR *caption, const WCHAR *other) {
    static int class_registered = 0;
    WNDCLASSW wc;
    MSG msg;
    HWND hwnd;

    sh_wstrcpy(g_about_caption, caption ? caption : L"About Windows", 128);
    sh_wstrcpy(g_about_other, other ? other : L"", 256);
    g_about_result = IDOK;

    if (!class_registered) {
        memset(&wc, 0, sizeof(wc));
        wc.lpszClassName = L"ShellAboutWindow";
        wc.lpfnWndProc = (WNDPROC)sh_about_wndproc;
        if (!RegisterClassW(&wc)) return 0;
        class_registered = 1;
    }

    hwnd = CreateWindowExW(0, L"ShellAboutWindow", g_about_caption[0] ? g_about_caption : L"About Windows",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                           120, 90, 480, 244, NULL, NULL, NULL, NULL);
    if (!hwnd) return 0;

    if (g_about_icon) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_about_icon);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)g_about_icon);
    }

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    while (IsWindow(hwnd) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return g_about_result;
}

__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

__attribute__((stdcall)) int Shell_NotifyIconW(uint32_t dwMessage, void *lpData) {
    (void)dwMessage;
    (void)lpData;
    return 1;
}

__attribute__((stdcall)) int ShellAboutA(void *hWnd, const char *szApp, const char *szOtherStuff, void *hIcon) {
    WCHAR caption[128];
    WCHAR other[256];
    (void)hWnd;
    g_about_icon = (HICON)hIcon;
    sh_ansi_to_wide(caption, szApp ? szApp : "About Windows", 128);
    sh_ansi_to_wide(other, szOtherStuff ? szOtherStuff : "", 256);
    return sh_run_about_window(caption, other);
}

__attribute__((stdcall)) int ShellAboutW(void *hWnd, const uint16_t *szApp, const uint16_t *szOtherStuff, void *hIcon) {
    (void)hWnd;
    g_about_icon = (HICON)hIcon;
    return sh_run_about_window((const WCHAR*)szApp, (const WCHAR*)szOtherStuff);
}
