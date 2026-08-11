#ifndef DISCOUNT_STDIO_H
#define DISCOUNT_STDIO_H
#include <stdarg.h>
#include <stddef.h>
#include "windef.h"

int swprintf(WCHAR *buffer, size_t count, const WCHAR *format, ...);

#endif
