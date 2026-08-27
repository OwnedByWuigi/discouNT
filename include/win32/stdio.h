#ifndef DISCOUNT_STDIO_H
#define DISCOUNT_STDIO_H
#include <stdarg.h>
#include <stddef.h>
#include "windef.h"

#define SEEK_SET 0
#define EOF (-1)
typedef struct _DISCOUNT_FILE FILE;
extern FILE *stdin, *stdout, *stderr;

int swprintf(WCHAR *buffer, size_t count, const WCHAR *format, ...);
int wnsprintfW(WCHAR *buffer, int count, const WCHAR *format, ...);
int sprintf(char *buffer, const char *format, ...);
int snprintf(char *buffer, size_t count, const char *format, ...);
int vsnprintf(char *buffer, size_t count, const char *format, va_list args);
int printf(const char *format, ...);
int sscanf(const char *buffer, const char *format, ...);
int ferror(FILE *stream);
int getc(FILE *stream);
int clearerr(FILE *stream);
size_t fread(void *buffer, size_t size, size_t count, FILE *stream);
size_t fwrite(const void *buffer, size_t size, size_t count, FILE *stream);
int fprintf(FILE *stream, const char *format, ...);

#endif
