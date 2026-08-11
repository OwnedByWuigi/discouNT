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

BOOL DestroyIcon(HICON hIcon) {
    U32_ICON *icon = u32_lookup_icon(hIcon);
    if (icon) {
        memset(icon, 0, sizeof(*icon));
    }
    return TRUE;
}
