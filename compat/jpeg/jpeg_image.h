#ifndef DISCOUNT_JPEG_IMAGE_H
#define DISCOUNT_JPEG_IMAGE_H
#include <stdint.h>
typedef struct {
    int width;
    int height;
    uint32_t *pixels;
} JPEG_IMAGE;
int JpegDecodeImage(const void *data, uint32_t size, JPEG_IMAGE *image);
void JpegFreeImage(JPEG_IMAGE *image);
#endif
