typedef struct {
    uint32_t magic;
    uint32_t flags;
    uint32_t image_size;
    uint32_t elf_base_vaddr;
    void *source_image;
    uint32_t source_size;
    char image_path[128];
} U32_RUNTIME_HEADER;

typedef struct {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) U32_ELF32_HEADER;

typedef struct {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t addralign;
    uint32_t entsize;
} __attribute__((packed)) U32_ELF32_SHDR;

#define U32_RUNTIME_MAGIC 0x544E4C44U

static int u32_menu_strcmp_a(const char *a, const char *b) {
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

static int u32_menu_has_prefix(const char *a, const char *prefix, int len) {
    int i;
    if (!a || !prefix) return 0;
    for (i = 0; i < len; i++) {
        if (!a[i] || a[i] != prefix[i]) return 0;
    }
    return 1;
}

static U32_RUNTIME_HEADER *u32_get_runtime_header(void *image_base) {
    U32_RUNTIME_HEADER *hdr;
    if (!image_base) return 0;
    hdr = (U32_RUNTIME_HEADER*)((uint8_t*)image_base - sizeof(U32_RUNTIME_HEADER));
    if (hdr->magic != U32_RUNTIME_MAGIC) return 0;
    return hdr;
}

static const uint8_t *u32_get_embedded_menu_blob(HINSTANCE hInstance, uint32_t *out_size) {
    return u32_get_embedded_blob(hInstance, ".disres", out_size);
}

static const uint8_t *u32_get_embedded_blob(HINSTANCE hInstance, const char *section_name, uint32_t *out_size) {
    U32_RUNTIME_HEADER *rt;
    U32_ELF32_HEADER *elf;
    U32_ELF32_SHDR *sh;
    const char *shstrtab;
    int i;
    if (out_size) *out_size = 0;
    rt = u32_get_runtime_header((void*)hInstance);
    if (!rt || !rt->source_image || rt->source_size < sizeof(U32_ELF32_HEADER)) return 0;
    elf = (U32_ELF32_HEADER*)rt->source_image;
    if (*(uint32_t*)elf->ident != 0x464C457F) return 0;
    if (!elf->shoff || !elf->shnum || elf->shstrndx >= elf->shnum) return 0;
    if (elf->shoff + (elf->shnum * sizeof(U32_ELF32_SHDR)) > rt->source_size) return 0;
    sh = (U32_ELF32_SHDR*)((uint8_t*)rt->source_image + elf->shoff);
    if (sh[elf->shstrndx].offset >= rt->source_size) return 0;
    shstrtab = (const char*)((uint8_t*)rt->source_image + sh[elf->shstrndx].offset);
    for (i = 0; i < elf->shnum; i++) {
        const char *name;
        if (sh[i].offset >= rt->source_size) continue;
        name = shstrtab + sh[i].name;
        if (name && section_name && u32_menu_strcmp_a(name, section_name) == 0) {
            if (sh[i].offset + sh[i].size > rt->source_size) return 0;
            if (out_size) *out_size = sh[i].size;
            return (const uint8_t*)rt->source_image + sh[i].offset;
        }
    }
    return 0;
}

static int u32_menu_parse_u32(const char *text, uint32_t *out) {
    uint32_t value = 0;
    int base = 10;
    int i = 0;
    if (!text || !*text || !out) return 0;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16;
        i = 2;
    }
    for (; text[i]; i++) {
        char ch = text[i];
        uint32_t digit;
        if (ch >= '0' && ch <= '9') digit = (uint32_t)(ch - '0');
        else if (base == 16 && ch >= 'a' && ch <= 'f') digit = 10u + (uint32_t)(ch - 'a');
        else if (base == 16 && ch >= 'A' && ch <= 'F') digit = 10u + (uint32_t)(ch - 'A');
        else return 0;
        value = (value * (uint32_t)base) + digit;
    }
    *out = value;
    return 1;
}

static const char *u32_menu_skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
    return p;
}

static const char *u32_menu_next_line(const char *p, const char *end) {
    while (p < end && *p != '\n') p++;
    if (p < end) p++;
    return p;
}

static HMENU u32_create_progman_menu(void) {
    HMENU root = CreateMenu();
    HMENU file, options, windows, help;
    if (!root) return NULL;
    file = CreateMenu(); options = CreateMenu(); windows = CreateMenu(); help = CreateMenu();
    if (!file || !options || !windows || !help) return root;
    AppendMenuW(root, MF_POPUP, (UINT_PTR)file, L"&File");
    AppendMenuW(file, MF_STRING, 256, L"&New...");
    AppendMenuW(file, MF_STRING, 257, L"O&pen\tEnter");
    AppendMenuW(file, MF_GRAYED, 258, L"&Move...\tF7");
    AppendMenuW(file, MF_GRAYED, 259, L"&Copy...\tF8");
    AppendMenuW(file, MF_STRING, 260, L"&Delete\tDel");
    AppendMenuW(file, MF_STRING, 261, L"&Properties\tAlt+Enter");
    AppendMenuW(file, MF_SEPARATOR, 0, L"");
    AppendMenuW(file, MF_STRING, 263, L"E&xecute...");
    AppendMenuW(file, MF_SEPARATOR, 0, L"");
    AppendMenuW(file, MF_STRING, 264, L"E&xit Windows");
    AppendMenuW(root, MF_POPUP, (UINT_PTR)options, L"&Options");
    AppendMenuW(options, MF_STRING, 272, L"&Arrange automatically");
    AppendMenuW(options, MF_STRING, 273, L"&Minimize on run");
    AppendMenuW(options, MF_STRING, 275, L"&Save settings on exit");
    AppendMenuW(root, MF_POPUP, (UINT_PTR)windows, L"&Windows");
    AppendMenuW(windows, MF_STRING, 288, L"&Overlapped\tShift+F5");
    AppendMenuW(windows, MF_STRING, 289, L"&Side by side\tShift+F4");
    AppendMenuW(windows, MF_STRING, 290, L"&Arrange Icons");
    AppendMenuW(root, MF_POPUP, (UINT_PTR)help, L"&Help");
    AppendMenuW(help, MF_STRING, 305, L"&Contents\tF1");
    AppendMenuW(help, MF_STRING, 322, L"&About Program Manager");
    return root;
}

static int u32_menu_match_key(const char *quoted_key, LPCWSTR lpMenuName) {
    char wanted[64];
    if (!quoted_key || !lpMenuName) return 0;
    if ((ULONG_PTR)lpMenuName <= 0xFFFFu) {
        int pos = 0;
        uint32_t id = (uint32_t)((ULONG_PTR)lpMenuName & 0xFFFFu);
        if (id == 0) {
            wanted[0] = '0';
            wanted[1] = 0;
        } else {
            char tmp[16];
            int n = 0;
            while (id && n < (int)sizeof(tmp)) {
                tmp[n++] = (char)('0' + (id % 10u));
                id /= 10u;
            }
            while (n > 0 && pos < (int)sizeof(wanted) - 1) wanted[pos++] = tmp[--n];
            wanted[pos] = 0;
        }
    } else {
        u32_wide_to_ansi(lpMenuName, wanted, sizeof(wanted));
    }
    return u32_menu_strcmp_a(quoted_key, wanted) == 0;
}

static const char *u32_menu_parse_quoted(const char *p, const char *end, WCHAR *out, int out_chars) {
    int pos = 0;
    if (!out || out_chars <= 0) return 0;
    p = u32_menu_skip_ws(p, end);
    if (p >= end || *p != '"') return 0;
    p++;
    while (p < end && *p != '"') {
        char ch = *p++;
        if (ch == '\\' && p < end) {
            char esc = *p++;
            if (esc == 'n') ch = '\n';
            else if (esc == 't') ch = '\t';
            else ch = esc;
        }
        if (pos < out_chars - 1) out[pos++] = (WCHAR)(unsigned char)ch;
    }
    if (p >= end || *p != '"') return 0;
    out[pos] = 0;
    return p + 1;
}

HMENU GetMenu(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return NULL;
    if (!win->menu) win->menu = CreatePopupMenu();
    return win->menu;
}

HMENU CreateMenu(void) {
    U32_MENU *menu = u32_alloc_menu();
    return menu ? menu->handle : NULL;
}

BOOL SetMenu(HWND hWnd, HMENU hMenu) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return FALSE;
    win->menu = hMenu;
    u32_mark_invalid(hWnd);
    return TRUE;
}

BOOL IsMenu(HMENU hMenu) {
    return u32_lookup_menu(hMenu) ? TRUE : FALSE;
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
    const uint8_t *blob;
    uint32_t blob_size = 0;
    const char *p;
    const char *end;
    U32_MENU *stack[16];
    int depth = -1;
    U32_MENU *root = 0;
    int selected = 0;

    blob = u32_get_embedded_menu_blob(hInstance, &blob_size);
    if ((!blob || !blob_size) && (ULONG_PTR)lpMenuName == 0x109u)
        return u32_create_progman_menu();
    if (!blob || !blob_size) return NULL;

    p = (const char*)blob;
    end = p + blob_size;
    while (p < end) {
        const char *line = u32_menu_skip_ws(p, end);
        if (line >= end) break;
        if (line + 4 <= end && u32_menu_has_prefix(line, "MENU", 4)) {
            WCHAR key_w[64];
            char key_a[64];
            const char *after = u32_menu_parse_quoted(line + 4, end, key_w, 64);
            selected = 0;
            root = 0;
            depth = -1;
            if (after) {
                u32_wide_to_ansi(key_w, key_a, sizeof(key_a));
                if (u32_menu_match_key(key_a, lpMenuName)) {
                    root = u32_alloc_menu();
                    if (!root) return NULL;
                    stack[0] = root;
                    depth = 0;
                    selected = 1;
                }
            }
        } else if (line + 5 <= end && u32_menu_has_prefix(line, "POPUP", 5)) {
            if (selected && depth >= 0) {
                uint32_t flags = 0;
                WCHAR text[64];
                const char *q = u32_menu_skip_ws(line + 5, end);
                char numbuf[16];
                int i = 0;
                while (q < end && *q && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n' && i < 15) numbuf[i++] = *q++;
                numbuf[i] = 0;
                if (!u32_menu_parse_u32(numbuf, &flags)) flags = 0;
                q = u32_menu_parse_quoted(q, end, text, 64);
                if (q && depth < 15) {
                    U32_MENU *sub = u32_alloc_menu();
                    if (!sub) return root ? root->handle : NULL;
                    AppendMenuW(stack[depth]->handle, flags | MF_POPUP, (UINT_PTR)sub->handle, text);
                    stack[++depth] = sub;
                }
            }
        } else if (line + 4 <= end && u32_menu_has_prefix(line, "ITEM", 4)) {
            if (selected && depth >= 0) {
                uint32_t flags = 0, id = 0;
                WCHAR text[64];
                const char *q = u32_menu_skip_ws(line + 4, end);
                char numbuf[16];
                int i = 0;
                while (q < end && *q && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n' && i < 15) numbuf[i++] = *q++;
                numbuf[i] = 0;
                if (!u32_menu_parse_u32(numbuf, &flags)) flags = 0;
                q = u32_menu_skip_ws(q, end);
                i = 0;
                while (q < end && *q && *q != ' ' && *q != '\t' && *q != '\r' && *q != '\n' && i < 15) numbuf[i++] = *q++;
                numbuf[i] = 0;
                if (!u32_menu_parse_u32(numbuf, &id)) id = 0;
                q = u32_menu_parse_quoted(q, end, text, 64);
                if (q) AppendMenuW(stack[depth]->handle, flags, id, text);
            }
        } else if (line + 3 <= end && u32_menu_has_prefix(line, "SEP", 3)) {
            if (selected && depth >= 0) AppendMenuW(stack[depth]->handle, MF_SEPARATOR, 0, L"");
        } else if (line + 7 <= end && u32_menu_has_prefix(line, "ENDMENU", 7)) {
            if (selected && root) return root->handle;
            selected = 0;
            root = 0;
            depth = -1;
        } else if (line + 3 <= end && u32_menu_has_prefix(line, "END", 3)) {
            if (selected && depth > 0) depth--;
        }
        p = u32_menu_next_line(p, end);
    }
    if (root) return root->handle;
    if ((ULONG_PTR)lpMenuName == 0x109u) return u32_create_progman_menu();
    return NULL;
}

BOOL InsertMenuW(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCWSTR lpNewItem) {
    U32_MENU *menu = u32_lookup_menu(hMenu);
    int i;
    if (!menu || menu->count >= 32) return FALSE;
    if (!(uFlags & MF_BYPOSITION)) return AppendMenuW(hMenu, uFlags, uIDNewItem, lpNewItem);
    /* Win32 callers use (UINT)-1 to append.  Normalize it before indexing;
       otherwise the signed -1 position writes immediately before items[]. */
    if ((int)uPosition < 0 || (int)uPosition > menu->count) uPosition = (UINT)menu->count;
    for (i = menu->count; i > (int)uPosition; i--) menu->items[i] = menu->items[i - 1];
    memset(&menu->items[uPosition], 0, sizeof(menu->items[uPosition]));
    menu->items[uPosition].id = (UINT)uIDNewItem;
    menu->items[uPosition].flags = uFlags;
    menu->items[uPosition].submenu = (uFlags & MF_POPUP) ? (HMENU)uIDNewItem : NULL;
    u32_wstrcpy(menu->items[uPosition].text, lpNewItem, 64);
    menu->count++;
    return TRUE;
}

BOOL DrawMenuBar(HWND hWnd) {
    U32_WINDOW *win = u32_lookup_window(hWnd);
    if (!win) return FALSE;
    u32_mark_invalid(hWnd);
    if (win->top_level) Win32kRedrawAll();
    return TRUE;
}
HMENU CreatePopupMenu(void) { U32_MENU *menu = u32_alloc_menu(); return menu ? menu->handle : NULL; }
BOOL DeleteMenu(HMENU hMenu, UINT uPosition, UINT uFlags) { return RemoveMenu(hMenu, uPosition, uFlags); }
BOOL SetMenuDefaultItem(HMENU hMenu, UINT uItem, UINT fByPos) { U32_MENU *menu = u32_lookup_menu(hMenu); (void)fByPos; if (!menu) return FALSE; menu->default_item = (int)uItem; return TRUE; }
BOOL TrackPopupMenuEx(HMENU hmenu, UINT fuFlags, int x, int y, HWND hwnd, void *lptpm) {
    HWND root;
    U32_WINDOW *root_win;
    RECT abs;
    int client_x = x;
    int client_y = y;
    (void)fuFlags;
    (void)lptpm;
    if (!hmenu || !hwnd) return FALSE;
    root = u32_find_menu_root(hwnd);
    root_win = u32_lookup_window(root);
    if (!root_win) return FALSE;
    if (root_win->popup_menu) u32_close_popup_chain(root);
    u32_get_absolute_rect(root_win, &abs);
    client_x -= abs.left + u32_client_offset_x(root_win);
    client_y -= abs.top + u32_client_offset_y(root_win);
    root_win->popup_menu = u32_create_menu_popup(root, root, hmenu, client_x, client_y, fuFlags);
    g_open_menu_popup = root_win->popup_menu;
    SendMessageW(root, WM_ENTERMENULOOP, 0, 0);
    u32_mark_invalid(root);
    return root_win->popup_menu ? TRUE : FALSE;
}
UINT GetMenuState(HMENU hMenu, UINT uId, UINT uFlags) { U32_MENU *menu = u32_lookup_menu(hMenu); int i; (void)uFlags; if (!menu) return 0; for (i = 0; i < menu->count; i++) if (menu->items[i].id == uId) return menu->items[i].flags; return 0; }
BOOL TrackPopupMenu(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, const RECT *prcRect) { (void)nReserved; (void)prcRect; return TrackPopupMenuEx(hMenu, uFlags, x, y, hWnd, 0); }
UINT GetMenuItemID(HMENU hMenu, int nPos) {
    U32_MENU *menu = u32_lookup_menu(hMenu);
    if (!menu || nPos < 0 || nPos >= menu->count) return (UINT)-1;
    if (menu->items[nPos].flags & MF_POPUP) return (UINT)-1;
    return menu->items[nPos].id;
}
BOOL AppendMenuA(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem) {
    WCHAR text[64];
    u32_ansi_to_wide(lpNewItem, text, 64);
    return AppendMenuW(hMenu, uFlags, uIDNewItem, lpNewItem ? text : NULL);
}
HMENU LoadMenuA(HINSTANCE hInstance, LPCSTR lpMenuName) {
    WCHAR name[64];
    if (!lpMenuName) return LoadMenuW(hInstance, NULL);
    if ((ULONG_PTR)lpMenuName <= 0xFFFFu) return LoadMenuW(hInstance, (LPCWSTR)lpMenuName);
    u32_ansi_to_wide(lpMenuName, name, 64);
    return LoadMenuW(hInstance, name);
}
BOOL InsertMenuA(HMENU hMenu, UINT uPosition, UINT uFlags, UINT_PTR uIDNewItem, LPCSTR lpNewItem) {
    WCHAR text[64];
    u32_ansi_to_wide(lpNewItem, text, 64);
    return InsertMenuW(hMenu, uPosition, uFlags, uIDNewItem, lpNewItem ? text : NULL);
}
