#include <stdarg.h>
#include <stddef.h>
#include <windows.h>

static size_t tmgr_wcslen(const WCHAR *s) {
    size_t n = 0;
    if (!s) return 0;
    while (s[n]) n++;
    return n;
}

static void tmgr_append_wstr(WCHAR *dst, size_t count, size_t *pos, const WCHAR *src) {
    size_t i = 0;
    if (!dst || !pos || !src || count == 0) return;
    while (src[i] && (*pos + 1) < count) {
        dst[*pos] = src[i];
        (*pos)++;
        i++;
    }
    dst[*pos] = 0;
}

static void tmgr_append_uint(WCHAR *dst, size_t count, size_t *pos, unsigned int value, int min_width) {
    WCHAR buf[16];
    int i = 0;
    int digits;
    if (!dst || !pos || count == 0) return;
    if (value == 0) {
        buf[i++] = L'0';
    } else {
        while (value && i < (int)(sizeof(buf) / sizeof(buf[0]))) {
            buf[i++] = (WCHAR)(L'0' + (value % 10));
            value /= 10;
        }
    }
    digits = i;
    while (digits < min_width && (*pos + 1) < count) {
        dst[*pos] = L' ';
        (*pos)++;
        digits++;
    }
    while (i > 0 && (*pos + 1) < count) {
        dst[*pos] = buf[--i];
        (*pos)++;
    }
    dst[*pos] = 0;
}

int swprintf(WCHAR *buffer, size_t count, const WCHAR *format, ...) {
    va_list ap;
    size_t pos = 0;
    size_t i = 0;

    if (!buffer || count == 0 || !format) return 0;
    buffer[0] = 0;

    va_start(ap, format);
    while (format[i] && (pos + 1) < count) {
        if (format[i] != L'%') {
            buffer[pos++] = format[i++];
            continue;
        }

        i++;
        if (format[i] == L'%') {
            buffer[pos++] = L'%';
            i++;
            continue;
        }

        if (format[i] == L'3' && format[i + 1] == L'd') {
            int v = va_arg(ap, int);
            if (v < 0) {
                if ((pos + 1) < count) buffer[pos++] = L'-';
                v = -v;
            }
            tmgr_append_uint(buffer, count, &pos, (unsigned int)v, 3);
            i += 2;
            continue;
        }

        if (format[i] == L'd' || format[i] == L'u') {
            unsigned int v = (format[i] == L'd') ? (unsigned int)va_arg(ap, int) : va_arg(ap, unsigned int);
            tmgr_append_uint(buffer, count, &pos, v, 0);
            i++;
            continue;
        }

        if (format[i] == L's') {
            const WCHAR *s = va_arg(ap, const WCHAR*);
            tmgr_append_wstr(buffer, count, &pos, s ? s : L"");
            i++;
            continue;
        }

        if ((pos + 1) < count) buffer[pos++] = L'%';
        if (format[i] && (pos + 1) < count) buffer[pos++] = format[i++];
    }
    va_end(ap);
    if (pos >= count) pos = count - 1;
    buffer[pos] = 0;
    return (int)tmgr_wcslen(buffer);
}
