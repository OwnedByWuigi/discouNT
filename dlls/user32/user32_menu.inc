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
HMENU CreatePopupMenu(void) { U32_MENU *menu = u32_alloc_menu(); return menu ? menu->handle : NULL; }
BOOL DeleteMenu(HMENU hMenu, UINT uPosition, UINT uFlags) { return RemoveMenu(hMenu, uPosition, uFlags); }
BOOL SetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos) { U32_MENU *menu = u32_lookup_menu(hMenu); (void)fByPos; if (!menu) return FALSE; menu->default_item = (int)uItem; return TRUE; }
BOOL TrackPopupMenuEx(HMENU hmenu, UINT fuFlags, int x, int y, HWND hwnd, void *lptpm) { (void)hmenu; (void)fuFlags; (void)x; (void)y; (void)hwnd; (void)lptpm; return TRUE; }
UINT GetMenuState(HMENU hMenu, UINT uId, UINT uFlags) { U32_MENU *menu = u32_lookup_menu(hMenu); int i; (void)uFlags; if (!menu) return 0; for (i = 0; i < menu->count; i++) if (menu->items[i].id == uId) return menu->items[i].flags; return 0; }
BOOL TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect) { (void)hMenu; (void)uFlags; (void)x; (void)y; (void)nReserved; (void)hWnd; (void)prcRect; return TRUE; }
