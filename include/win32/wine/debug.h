#ifndef DISCOUNT_WINE_DEBUG_H
#define DISCOUNT_WINE_DEBUG_H

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
#define FIXME(...) ((void)0)
#define WARN(...) ((void)0)
#define ERR(...) ((void)0)
#define MESSAGE(...) ((void)0)
#define UNIMPLEMENTED ((void)0)

#endif
