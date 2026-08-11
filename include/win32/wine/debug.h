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

#endif
