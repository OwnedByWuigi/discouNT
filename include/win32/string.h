#ifndef DISCOUNT_STRING_H
#define DISCOUNT_STRING_H

#include <stddef.h>
#include "windef.h"

void *memset(void *s, int c, uint32_t n);
void *memcpy(void *d, const void *s, uint32_t n);
void *memmove(void *d,const void *s,uint32_t n);
int memcmp(const void *a, const void *b, uint32_t n);
int strcmp(const char *a, const char *b);
int strcasecmp(const char *a, const char *b);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strdup(const char *s);
int _snprintf(char *buffer, SIZE_T size, const char *format, ...);
uint32_t strlen(const char *s);
void strcpy(char *d, const char *s);
void strcat(char *d, const char *s);

#define ZeroMemory(dst,len) memset((dst), 0, (len))
#define CopyMemory(dst,src,len) memcpy((dst),(src),(len))

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

static inline WCHAR *wcspbrk(const WCHAR *s, const WCHAR *accept) {
    while (*s) {
        const WCHAR *p = accept;
        while (*p) if (*s == *p++) return (WCHAR *)s;
        s++;
    }
    return 0;
}

static inline SIZE_T wcsspn(const WCHAR *s, const WCHAR *accept) {
    const WCHAR *start = s;
    while (*s) {
        const WCHAR *p = accept;
        while (*p && *p != *s) p++;
        if (!*p) break;
        s++;
    }
    return (SIZE_T)(s - start);
}

static inline SIZE_T wcscspn(const WCHAR *s, const WCHAR *reject) {
    const WCHAR *start = s;
    while (*s) {
        const WCHAR *p = reject;
        while (*p && *p != *s) p++;
        if (*p) break;
        s++;
    }
    return (SIZE_T)(s - start);
}

static inline int wcsnicmp(const WCHAR *a,const WCHAR *b,SIZE_T count) {
    while(count--){WCHAR x=*a++,y=*b++;if(x>=L'a'&&x<=L'z')x-=L'a'-L'A';if(y>=L'a'&&y<=L'z')y-=L'a'-L'A';if(x!=y)return x<y?-1:1;if(!x)return 0;}return 0;
}

#endif
