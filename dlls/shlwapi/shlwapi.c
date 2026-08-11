#include <stdint.h>

__attribute__((stdcall)) int DllMain(void *hModule, uint32_t reason, void *lpReserved) {
    (void)hModule;
    (void)reason;
    (void)lpReserved;
    return 1;
}

static int append_ascii_as_wide(uint16_t *dst, unsigned value) {
    char buf[16];
    int len = 0;
    int i;

    if (value == 0) {
        dst[0] = '0';
        return 1;
    }

    while (value && len < (int)sizeof(buf)) {
        buf[len++] = (char)('0' + (value % 10));
        value /= 10;
    }

    for (i = 0; i < len; i++) dst[i] = (uint16_t)buf[len - 1 - i];
    return len;
}

__attribute__((stdcall)) int StrFormatKBSizeW(int64_t qdw, uint16_t *pszBuf, uint32_t cchBuf) {
    uint32_t value;
    int pos;
    if (!pszBuf || cchBuf < 4) return 0;
    value = (uint32_t)((qdw + 1023) / 1024);
    pos = append_ascii_as_wide(pszBuf, value);
    if ((uint32_t)(pos + 4) >= cchBuf) return 0;
    pszBuf[pos++] = ' ';
    pszBuf[pos++] = 'K';
    pszBuf[pos++] = 'B';
    pszBuf[pos] = 0;
    return 1;
}

__attribute__((stdcall)) int StrFormatByteSizeW(int64_t qdw, uint16_t *pszBuf, uint32_t cchBuf) {
    if (qdw >= 1024) return StrFormatKBSizeW(qdw, pszBuf, cchBuf);
    if (!pszBuf || cchBuf < 6) return 0;
    {
        int pos = append_ascii_as_wide(pszBuf, (uint32_t)qdw);
        if ((uint32_t)(pos + 6) >= cchBuf) return 0;
        pszBuf[pos++] = ' ';
        pszBuf[pos++] = 'b';
        pszBuf[pos++] = 'y';
        pszBuf[pos++] = 't';
        pszBuf[pos++] = 'e';
        pszBuf[pos++] = 's';
        pszBuf[pos] = 0;
    }
    return 1;
}
