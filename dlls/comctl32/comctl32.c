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
extern intptr_t DefWindowProcW(void *hWnd, uint32_t Msg, uintptr_t wParam, intptr_t lParam);

#define COMCTL_IMAGE_LISTS 16
typedef struct {
    int used;
    int count;
    int capacity;
    void *handle;
} COMCTL_IMAGE_LIST;
static COMCTL_IMAGE_LIST g_image_lists[COMCTL_IMAGE_LISTS];

__attribute__((stdcall)) void InitCommonControls(void) {
}

__attribute__((stdcall)) int InitCommonControlsEx(const void *lpInitCtrls) {
    (void)lpInitCtrls;
    return 1;
}

__attribute__((stdcall)) void *CreatePropertySheetPageA(const void *page) {
    (void)page;
    return (void*)1;
}
__attribute__((stdcall)) int PropertySheetA(const void *header) {
    (void)header;
    return 1;
}

__attribute__((stdcall)) void *CreateStatusWindowW(int32_t style, const uint16_t *text, void *parent, uint32_t id) {
    return CreateWindowExW(0, g_status_class, text, (uint32_t)style, 2, 226, 532, 18, parent, (void*)(uintptr_t)id, 0, 0);
}

__attribute__((stdcall)) void *ImageList_Create(int cx, int cy, uint32_t flags, int cInitial, int cGrow) {
    int i;
    (void)cx; (void)cy; (void)flags;
    for (i = 0; i < COMCTL_IMAGE_LISTS; i++) {
        if (!g_image_lists[i].used) {
            g_image_lists[i].used = 1;
            g_image_lists[i].count = 0;
            g_image_lists[i].capacity = cInitial > 0 ? cInitial : 1;
            g_image_lists[i].handle = (void*)&g_image_lists[i];
            (void)cGrow;
            return g_image_lists[i].handle;
        }
    }
    return 0;
}

static COMCTL_IMAGE_LIST *comctl_image_list(void *himl) {
    int i;
    for (i = 0; i < COMCTL_IMAGE_LISTS; i++)
        if (g_image_lists[i].used && g_image_lists[i].handle == himl) return &g_image_lists[i];
    return 0;
}

__attribute__((stdcall)) int ImageList_AddIcon(void *himl, void *hicon) {
    COMCTL_IMAGE_LIST *list = comctl_image_list(himl);
    (void)hicon;
    if (!list) return -1;
    return list->count++;
}

__attribute__((stdcall)) int ImageList_ReplaceIcon(void *himl, int i, void *hicon) {
    (void)himl; (void)i; (void)hicon;
    return i;
}

__attribute__((stdcall)) int ImageList_Remove(void *himl, int i) {
    COMCTL_IMAGE_LIST *list = comctl_image_list(himl);
    if (!list || i < 0 || i >= list->count) return 0;
    list->count--;
    return 1;
}
__attribute__((stdcall)) int ImageList_Destroy(void *himl) {
    COMCTL_IMAGE_LIST *list = comctl_image_list(himl);
    if (!list) return 0;
    list->used = 0;
    return 1;
}
__attribute__((stdcall)) int ImageList_GetImageCount(void *himl) {
    COMCTL_IMAGE_LIST *list = comctl_image_list(himl);
    return list ? list->count : 0;
}
__attribute__((stdcall)) int TreeView_GetItemRect(void *hwnd, void *item, void *rect, int textOnly) { (void)hwnd;(void)item;(void)rect;(void)textOnly; return 0; }
__attribute__((stdcall)) int ImageList_Draw(void *himl,int image,void *dc,int x,int y,uint32_t style){(void)himl;(void)image;(void)dc;(void)x;(void)y;(void)style;return 1;}

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

__attribute__((stdcall)) int SetWindowSubclass(void *hWnd, void *pfnSubclass, uintptr_t uIdSubclass, uintptr_t dwRefData) {
    (void)hWnd; (void)pfnSubclass; (void)uIdSubclass; (void)dwRefData;
    return 1;
}

__attribute__((stdcall)) intptr_t DefSubclassProc(void *hWnd, uint32_t Msg, uintptr_t wParam, intptr_t lParam) {
    return DefWindowProcW(hWnd, Msg, wParam, lParam);
}
