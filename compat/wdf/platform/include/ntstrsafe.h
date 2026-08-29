#ifndef DISCOUNT_WDF_NTSTRSAFE_H
#define DISCOUNT_WDF_NTSTRSAFE_H
#include <stddef.h>
#include <stdint.h>
static inline int RtlStringCchCopyW(wchar_t *d, size_t n, const wchar_t *s) {
    size_t i = 0; if (!d || !n) return -1; while (i + 1 < n && s && s[i]) { d[i] = s[i]; i++; } d[i] = 0; return (s && !s[i]) ? 0 : -1;
}
static inline int RtlStringCchPrintfW(wchar_t *d, size_t n, const wchar_t *f, ...) { (void)d; (void)n; (void)f; return -1; }
#define RtlStringCchLengthW(s,n,l) (*(l)=0,0)
#endif
