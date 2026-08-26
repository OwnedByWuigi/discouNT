#ifndef DISCOUNT_STDIO_H
#define DISCOUNT_STDIO_H
#include <stdarg.h>
#include <stddef.h>
#include "windef.h"

#define SEEK_SET 0

int swprintf(WCHAR *buffer, size_t count, const WCHAR *format, ...);
int wnsprintfW(WCHAR *buffer, int count, const WCHAR *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t count, const char *format, ...);
int vsnprintf(char *buffer, size_t count, const char *format, va_list args);
int printf(const char *format, ...);
int sscanf(const char *buffer, const char *format, ...);

#endif
