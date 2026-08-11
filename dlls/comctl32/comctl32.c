#include <stdint.h>

extern void *CreateWindowExW(uint32_t exStyle, const uint16_t *className, const uint16_t *windowName,
                             uint32_t style, int x, int y, int w, int h, void *parent, void *menu,
                             void *instance, void *param);

static const uint16_t g_status_class[] = {
    'm','s','c','t','l','s','_','s','t','a','t','u','s','b','a','r','3','2',0
};

__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

extern uint32_t SendMessageW(void *hWnd, uint32_t Msg, uintptr_t wParam, intptr_t lParam);

__attribute__((stdcall)) void InitCommonControls(void) {
}

__attribute__((stdcall)) int InitCommonControlsEx(const void *lpInitCtrls) {
    (void)lpInitCtrls;
    return 1;
}

__attribute__((stdcall)) void *CreateStatusWindowW(int32_t style, const uint16_t *text, void *parent, uint32_t id) {
    return CreateWindowExW(0, g_status_class, text, (uint32_t)style, 2, 226, 532, 18, parent, (void*)(uintptr_t)id, 0, 0);
}

__attribute__((stdcall)) void *ImageList_Create(int cx, int cy, uint32_t flags, int cInitial, int cGrow) {
    (void)cx; (void)cy; (void)flags; (void)cInitial; (void)cGrow;
    return (void*)0x300;
}

__attribute__((stdcall)) int ImageList_AddIcon(void *himl, void *hicon) {
    (void)himl; (void)hicon;
    return 0;
}

__attribute__((stdcall)) int ImageList_ReplaceIcon(void *himl, int i, void *hicon) {
    (void)himl; (void)i; (void)hicon;
    return i;
}

__attribute__((stdcall)) int ImageList_Remove(void *himl, int i) {
    (void)himl; (void)i;
    return 1;
}

__attribute__((stdcall)) int ListView_InsertItemW(void *hwnd, const void *pitem) {
    return (int)SendMessageW(hwnd, 0x1000 + 77, 0, (intptr_t)pitem);
}

__attribute__((stdcall)) int ListView_InsertColumnW(void *hwnd, int iCol, const void *pcol) {
    return (int)SendMessageW(hwnd, 0x1000 + 97, (uintptr_t)iCol, (intptr_t)pcol);
}

__attribute__((stdcall)) int ListView_SetItemTextW(void *hwnd, int i, int iSubItem, uint16_t *pszText) {
    struct { uint32_t mask; int iItem; int iSubItem; uint32_t state; uint32_t stateMask; uint16_t *pszText; int cchTextMax; int iImage; intptr_t lParam; int iIndent; } item;
    item.mask = 0x0001;
    item.iItem = i;
    item.iSubItem = iSubItem;
    item.state = 0;
    item.stateMask = 0;
    item.pszText = pszText;
    item.cchTextMax = 0;
    item.iImage = 0;
    item.lParam = 0;
    item.iIndent = 0;
    return (int)SendMessageW(hwnd, 0x1000 + 76, 0, (intptr_t)&item);
}

__attribute__((stdcall)) void ListView_GetItemTextW(void *hwnd, int i, int iSubItem, uint16_t *pszText, int cchTextMax) {
    struct { uint32_t mask; int iItem; int iSubItem; uint32_t state; uint32_t stateMask; uint16_t *pszText; int cchTextMax; int iImage; intptr_t lParam; int iIndent; } item;
    item.mask = 0x0001;
    item.iItem = i;
    item.iSubItem = iSubItem;
    item.state = 0;
    item.stateMask = 0;
    item.pszText = pszText;
    item.cchTextMax = cchTextMax;
    item.iImage = 0;
    item.lParam = 0;
    item.iIndent = 0;
    SendMessageW(hwnd, 0x1000 + 75, 0, (intptr_t)&item);
}

__attribute__((stdcall)) void ListView_GetItemTextA(void *hwnd, int i, int iSubItem, char *pszText, int cchTextMax) {
    (void)hwnd; (void)i; (void)iSubItem;
    if (!pszText || cchTextMax <= 0) return;
    pszText[0] = 0;
}
