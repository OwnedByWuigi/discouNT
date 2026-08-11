#ifndef DISCOUNT_STRING_H
#define DISCOUNT_STRING_H

#include <stddef.h>
#include "windef.h"

void *memset(void *s, int c, uint32_t n);
void *memcpy(void *d, const void *s, uint32_t n);
int strcmp(const char *a, const char *b);
void strcpy(char *d, const char *s);

static inline WCHAR *wcscat(WCHAR *dst, const WCHAR *src) {
    WCHAR *out = dst;
    while (*dst) dst++;
    while ((*dst++ = *src++));
    return out;
}

static inline WCHAR *wcsstr(const WCHAR *haystack, const WCHAR *needle) {
    const WCHAR *h;
    const WCHAR *n;
    if (!haystack || !needle || !*needle) return (WCHAR*)haystack;
    while (*haystack) {
        h = haystack;
        n = needle;
        while (*h && *n && *h == *n) {
            h++;
            n++;
        }
        if (!*n) return (WCHAR*)haystack;
        haystack++;
    }
    return 0;
}

#endif
