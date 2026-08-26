#ifndef DISCOUNT_WINE_DEBUG_H
#define DISCOUNT_WINE_DEBUG_H

#include <stdarg.h>
#include <stddef.h>

int vsnprintf(char *buffer, size_t count, const char *format, va_list args);
int printf(const char *format, ...);

static inline const char *wine_dbg_sprintf(const char *format, ...)
{
    static char buffers[4][256];
    static unsigned int next;
    char *buffer = buffers[next++ & 3];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffers[0]), format, args);
    va_end(args);
    return buffer;
}

struct __wine_debug_channel {
    char name[32];
    unsigned char flags;
    unsigned char pad[3];
};

enum {
    __WINE_DBCL_FIXME = 0,
    __WINE_DBCL_ERR   = 1,
    __WINE_DBCL_WARN  = 2,
    __WINE_DBCL_TRACE = 3,
    __WINE_DBCL_INIT  = 7
};

#define WINE_DEFAULT_DEBUG_CHANNEL(name) static const char *wine_debug_channel = #name
#define TRACE(...) ((void)0)
#define WINE_TRACE(...) TRACE(__VA_ARGS__)
#define FIXME(...) ((void)0)
#define WINE_FIXME(...) printf(__VA_ARGS__)
#define WINE_MESSAGE(...) printf(__VA_ARGS__)
#define WINE_ERR(...) printf(__VA_ARGS__)
#define WINE_WARN(...) WARN(__VA_ARGS__)
#define WARN(...) ((void)0)
#define ERR(...) ((void)0)
#define MESSAGE(...) ((void)0)
#define UNIMPLEMENTED ((void)0)

static inline const char *wine_dbgstr_a(const char *str) { return str ? str : "(null)"; }
static inline const char *wine_dbgstr_w(const WCHAR *str) {
    return str ? wine_dbg_sprintf("%ls", str) : "(null)";
}

#endif
