#ifndef DISCOUNT_ICON_H
#define DISCOUNT_ICON_H

#include <stdint.h>

#define DISCOUNT_ICON_MAGIC 0x4E4F4349U

typedef struct _DISCOUNT_ICON {
    uint32_t magic;
    int width;
    int height;
    uint32_t *pixels;
} DISCOUNT_ICON;

#endif
