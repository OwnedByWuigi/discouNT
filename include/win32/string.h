#ifndef DISCOUNT_STRING_H
#define DISCOUNT_STRING_H

#include <stddef.h>
#include "windef.h"

void *memset(void *s, int c, uint32_t n);
void *memcpy(void *d, const void *s, uint32_t n);
int memcmp(const void *a, const void *b, uint32_t n);
int strcmp(const char *a, const char *b);
uint32_t strlen(const char *s);
void strcpy(char *d, const char *s);
void strcat(char *d, const char *s);

#define ZeroMemory(dst,len) memset((dst), 0, (len))

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

static inline WCHAR *wcschr(const WCHAR *s, WCHAR ch) {
    if (!s) return 0;
    while (*s) {
        if (*s == ch) return (WCHAR*)s;
        s++;
    }
    return ch == 0 ? (WCHAR*)s : 0;
}

#endif
