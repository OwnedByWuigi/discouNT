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
    HICON icon = 0;
    uint32_t icon_size = 0;
    const uint8_t *icon_data = u32_get_embedded_blob(hInstance, ".disicon", &icon_size);
    if (u32_is_int_resource(lpIconName)) {
        if (icon_data && icon_size) {
            icon = u32_create_icon_from_ico_buffer(icon_data, icon_size, 32, 32);
            if (icon) return icon;
        }
        icon = u32_try_load_module_sidecar_icon(hInstance, 32, 32);
        if (icon) return icon;
        return u32_alloc_icon(u32_resource_id(lpIconName), 32, 32);
    }
    icon = u32_try_load_icon_by_name(lpIconName, 32, 32);
    if (icon) return icon;
    return u32_alloc_icon(0, 32, 32);
}

HANDLE LoadImageW(HINSTANCE hinst, LPCWSTR name, UINT type, int cx, int cy, UINT fuLoad) {
    int width;
    int height;
    (void)hinst;
    (void)fuLoad;
    /* MSGINA's dialog resources refer to the logo and separator as bitmap
     * resources.  They are not icons and must not go through the ICO path.
     * The PE resource reader currently exposes the resource identity here;
     * MSGINA's GDI compatibility layer uses that identity to paint the
     * corresponding built-in bitmap. */
    if (type == IMAGE_BITMAP && u32_is_int_resource(name)) {
        const char *section = u32_resource_id(name) == 1007 ? ".disbmp_logo" : ".disbmp_bar";
        uint32_t bitmap_size = 0;
        const uint8_t *bitmap_data = u32_get_embedded_blob(hinst, section, &bitmap_size);
        extern HBITMAP GdiCreateBitmapFromBmp(const void *, uint32_t);
        if (bitmap_data) return (HANDLE)GdiCreateBitmapFromBmp(bitmap_data, bitmap_size);
        return 0;
    }
    if (type != IMAGE_ICON) return 0;
    width = cx > 0 ? cx : 32;
    height = cy > 0 ? cy : 32;
    if (u32_is_int_resource(name)) {
        {
            uint32_t icon_size = 0;
            const uint8_t *icon_data = u32_get_embedded_blob(hinst, ".disicon", &icon_size);
            if (icon_data && icon_size) {
                HANDLE icon = (HANDLE)u32_create_icon_from_ico_buffer(icon_data, icon_size, width, height);
                if (icon) return icon;
            }
        }
        HANDLE icon = (HANDLE)u32_try_load_module_sidecar_icon(hinst, width, height);
        if (icon) return icon;
        return (HANDLE)u32_alloc_icon(u32_resource_id(name), width, height);
    }
    {
        HANDLE icon = (HANDLE)u32_try_load_icon_by_name(name, width, height);
        if (icon) return icon;
    }
    return (HANDLE)u32_alloc_icon(0, width, height);
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

LRESULT SendDlgItemMessageW(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam) {
    HWND child = GetDlgItem(hDlg, nIDDlgItem);
    if (!child) return 0;
    return SendMessageW(child, Msg, wParam, lParam);
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

BOOL CheckDlgButton(HWND hDlg, int nIDButton, UINT uCheck) {
    HWND child = GetDlgItem(hDlg, nIDButton);
    if (!child) return FALSE;
    SendMessageW(child, BM_SETCHECK, uCheck, 0);
    return TRUE;
}

UINT IsDlgButtonChecked(HWND hDlg, int nIDButton) {
    HWND child = GetDlgItem(hDlg, nIDButton);
    if (!child) return BST_UNCHECKED;
    return (UINT)SendMessageW(child, BM_GETCHECK, 0, 0);
}

BOOL CheckRadioButton(HWND hDlg, int firstID, int lastID, int checkID) {
    int id;
    BOOL found = FALSE;
    for (id = firstID; id <= lastID; id++) {
        HWND child = GetDlgItem(hDlg, id);
        if (!child) continue;
        SendMessageW(child, BM_SETCHECK, id == checkID ? BST_CHECKED : BST_UNCHECKED, 0);
        if (id == checkID) found = TRUE;
    }
    return found;
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
    int old_w;
    int old_h;
    int moved = 0;
    int sized = 0;
    (void)hWndInsertAfter;
    if (!win) return FALSE;
    old_w = win->rect.right - win->rect.left;
    old_h = win->rect.bottom - win->rect.top;
    if (!(uFlags & SWP_NOMOVE)) {
        moved = (win->rect.left != X) || (win->rect.top != Y);
        win->rect.left = X;
        win->rect.top = Y;
    }
    if (!(uFlags & SWP_NOSIZE)) {
        sized = (old_w != cx) || (old_h != cy);
        win->rect.right = win->rect.left + cx;
        win->rect.bottom = win->rect.top + cy;
    }
    if (uFlags & SWP_SHOWWINDOW) win->visible = 1;
    u32_sync_top_level(win);
    if (moved || sized) {
        u32_mark_invalid_descendants(hWnd);
        u32_mark_invalid(hWnd);
        if (win->parent) u32_mark_invalid(win->parent);
    }
    SendMessageW(hWnd, WM_SIZE, SIZE_RESTORED,
                 MAKELPARAM(win->rect.right - win->rect.left, win->rect.bottom - win->rect.top));
    return TRUE;
}
