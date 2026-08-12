#include <stdint.h>
#include "windows.h"

__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

static int append_ascii_as_wide(WCHAR *dst, unsigned value) {
    char buf[16];
    int len = 0;
    int i;

    if (value == 0) {
        dst[0] = (WCHAR)'0';
        return 1;
    }

    while (value && len < (int)sizeof(buf)) {
        buf[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (i = 0; i < len; i++) dst[i] = (WCHAR)buf[len - 1 - i];
    return len;
}

__attribute__((stdcall)) int StrFormatKBSizeW(int64_t qdw, WCHAR *pszBuf, uint32_t cchBuf) {
    uint32_t value;
    int pos;
    if (!pszBuf || cchBuf < 4) return 0;
    value = (uint32_t)((qdw + 1023) / 1024);
    pos = append_ascii_as_wide(pszBuf, value);
    if ((uint32_t)(pos + 4) >= cchBuf) return 0;
    pszBuf[pos++] = (WCHAR)' ';
    pszBuf[pos++] = (WCHAR)'K';
    pszBuf[pos++] = (WCHAR)'B';
    pszBuf[pos] = 0;
    return 1;
}

__attribute__((stdcall)) int StrFormatByteSizeW(int64_t qdw, WCHAR *pszBuf, uint32_t cchBuf) {
    if (qdw >= 1024) return StrFormatKBSizeW(qdw, pszBuf, cchBuf);
    if (!pszBuf || cchBuf < 6) return 0;
    {
        int pos = append_ascii_as_wide(pszBuf, (uint32_t)qdw);
        if ((uint32_t)(pos + 6) >= cchBuf) return 0;
        pszBuf[pos++] = (WCHAR)' ';
        pszBuf[pos++] = (WCHAR)'b';
        pszBuf[pos++] = (WCHAR)'y';
        pszBuf[pos++] = (WCHAR)'t';
        pszBuf[pos++] = (WCHAR)'e';
        pszBuf[pos++] = (WCHAR)'s';
        pszBuf[pos] = 0;
    }
    return 1;
}

static uint16_t shlw_upper(uint16_t ch) {
    if (ch >= 'a' && ch <= 'z') return ch - ('a' - 'A');
    return ch;
}

__attribute__((stdcall)) int StrCmpNW(const uint16_t *psz1, const uint16_t *psz2, int iLen) {
    int i = 0;
    if (iLen <= 0) return 0;
    while (i < iLen) {
        uint16_t a = psz1 ? psz1[i] : 0;
        uint16_t b = psz2 ? psz2[i] : 0;
        if (a != b) return (a < b) ? -1 : 1;
        if (!a) break;
        i++;
    }
    return 0;
}

__attribute__((stdcall)) int StrCmpNIW(const uint16_t *psz1, const uint16_t *psz2, int iLen) {
    int i = 0;
    if (iLen <= 0) return 0;
    while (i < iLen) {
        uint16_t a = shlw_upper(psz1 ? psz1[i] : 0);
        uint16_t b = shlw_upper(psz2 ? psz2[i] : 0);
        if (a != b) return (a < b) ? -1 : 1;
        if (!a) break;
        i++;
    }
    return 0;
}

__attribute__((stdcall)) uint16_t *StrStrW(const uint16_t *pszFirst, const uint16_t *pszSrch) {
    int i, j;
    if (!pszFirst || !pszSrch) return 0;
    if (!*pszSrch) return (uint16_t*)pszFirst;
    for (i = 0; pszFirst[i]; i++) {
        for (j = 0; pszSrch[j] && pszFirst[i + j] == pszSrch[j]; j++) {}
        if (!pszSrch[j]) return (uint16_t*)(pszFirst + i);
    }
    return 0;
}

__attribute__((stdcall)) uint16_t *StrStrIW(const uint16_t *pszFirst, const uint16_t *pszSrch) {
    int i, j;
    if (!pszFirst || !pszSrch) return 0;
    if (!*pszSrch) return (uint16_t*)pszFirst;
    for (i = 0; pszFirst[i]; i++) {
        for (j = 0; pszSrch[j] && shlw_upper(pszFirst[i + j]) == shlw_upper(pszSrch[j]); j++) {}
        if (!pszSrch[j]) return (uint16_t*)(pszFirst + i);
    }
    return 0;
}

__attribute__((stdcall)) uint16_t *StrRStrIW(const uint16_t *pszSource, const uint16_t *pszLast, const uint16_t *pszSrch) {
    const uint16_t *match = 0;
    const uint16_t *p;
    if (!pszSource || !pszSrch) return 0;
    if (!pszLast) {
        pszLast = pszSource;
        while (*pszLast) pszLast++;
    }
    for (p = pszSource; p < pszLast && *p; p++) {
        const uint16_t *cand = StrStrIW(p, pszSrch);
        if (cand && cand < pszLast) match = cand;
        if (!cand) break;
        p = cand;
    }
    return (uint16_t*)match;
}

__attribute__((stdcall)) const uint16_t *PathFindFileNameW(const uint16_t *path) {
    const uint16_t *last = path;
    if (!path) return 0;
    while (*path) {
        if (*path == '/' || *path == '\\') last = path + 1;
        path++;
    }
    return last;
}
