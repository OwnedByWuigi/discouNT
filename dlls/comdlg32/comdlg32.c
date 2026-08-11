#include <stdint.h>
#include "commdlg.h"

static DWORD g_commdlg_error = 0;

static void cdlg_copy_wstr(LPWSTR dst, LPCWSTR src, DWORD max_chars) {
    DWORD i = 0;
    if (!dst || max_chars == 0) return;
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

int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

BOOL GetOpenFileNameW(LPOPENFILENAMEW ofn) {
    g_commdlg_error = 0;
    if (!ofn || !ofn->lpstrFile || ofn->nMaxFile == 0) return FALSE;
    if (!ofn->lpstrFile[0]) cdlg_copy_wstr(ofn->lpstrFile, L"/UNTITLED.TXT", ofn->nMaxFile);
    if (ofn->lpstrFileTitle && ofn->nMaxFileTitle) cdlg_copy_wstr(ofn->lpstrFileTitle, ofn->lpstrFile, ofn->nMaxFileTitle);
    return TRUE;
}

BOOL GetSaveFileNameW(LPOPENFILENAMEW ofn) {
    return GetOpenFileNameW(ofn);
}

BOOL ChooseFontW(LPCHOOSEFONTW cf) {
    if (!cf || !cf->lpLogFont) {
        g_commdlg_error = 0xFFFF;
        return FALSE;
    }
    if (!cf->iPointSize) {
        int h = cf->lpLogFont->lfHeight;
        if (h < 0) h = -h;
        cf->iPointSize = h * 10;
    }
    g_commdlg_error = 0;
    return TRUE;
}

HWND FindTextW(LPFINDREPLACEW fr) {
    g_commdlg_error = 0;
    return fr ? fr->hwndOwner : NULL;
}

HWND ReplaceTextW(LPFINDREPLACEW fr) {
    g_commdlg_error = 0;
    return fr ? fr->hwndOwner : NULL;
}

BOOL PrintDlgW(LPPRINTDLGW pd) {
    if (!pd) {
        g_commdlg_error = 0xFFFF;
        return FALSE;
    }
    pd->hDC = (HDC)1;
    if (!pd->nCopies) pd->nCopies = 1;
    g_commdlg_error = 0;
    return TRUE;
}

DWORD CommDlgExtendedError(void) {
    return g_commdlg_error;
}
