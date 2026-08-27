#ifndef DISCOUNT_STDLIB_H
#define DISCOUNT_STDLIB_H
#include "windef.h"
#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
void *calloc(SIZE_T count,SIZE_T size);
void *realloc(void *memory,SIZE_T size);
ULONG wcstoul(const WCHAR *text,WCHAR **end,int base);
long wcstol(const WCHAR *text,WCHAR **end,int base);
long strtol(const char *text,char **end,int base);
WCHAR *wcsdup(const WCHAR *text);
void exit(int status);

static inline int abs(int x) {
    return (x < 0) ? -x : x;
}

#endif
