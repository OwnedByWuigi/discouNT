#include <stdint.h>
#include "jpeg_image.h"
#include "picojpeg.h"

extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);

typedef struct { const uint8_t *data; uint32_t size, offset; } JPEG_INPUT;
static unsigned char jpeg_read(unsigned char *buffer, unsigned char count,
                               unsigned char *actual, void *context) {
    JPEG_INPUT *input = (JPEG_INPUT *)context;
    uint32_t left = input->size - input->offset;
    if (count > left) count = (unsigned char)left;
    for (uint32_t i = 0; i < count; ++i) buffer[i] = input->data[input->offset + i];
    input->offset += count; *actual = count;
    return 0;
}

int JpegDecodeImage(const void *data, uint32_t size, JPEG_IMAGE *image) {
    JPEG_INPUT input;
    pjpeg_image_info_t info;
    uint8_t error;
    uint32_t total, mcu_x, mcu_y;
    if (!data || !image || size < 4) return 0;
    image->width = image->height = 0; image->pixels = 0;
    input.data = (const uint8_t *)data; input.size = size; input.offset = 0;
    error = pjpeg_decode_init(&info, jpeg_read, &input, 0);
    if (error || info.m_width <= 0 || info.m_height <= 0) return 0;
    total = (uint32_t)info.m_width * (uint32_t)info.m_height;
    image->pixels = (uint32_t *)kmalloc(total * sizeof(uint32_t));
    if (!image->pixels) return 0;
    image->width = info.m_width; image->height = info.m_height;
    for (mcu_y = 0; mcu_y < (uint32_t)info.m_MCUSPerCol; ++mcu_y) {
        for (mcu_x = 0; mcu_x < (uint32_t)info.m_MCUSPerRow; ++mcu_x) {
            error = pjpeg_decode_mcu();
            if (error) { JpegFreeImage(image); return 0; }
            for (uint32_t y = 0; y < (uint32_t)info.m_MCUHeight; ++y) {
                for (uint32_t x = 0; x < (uint32_t)info.m_MCUWidth; ++x) {
                    uint32_t px = mcu_x * info.m_MCUWidth + x;
                    uint32_t py = mcu_y * info.m_MCUHeight + y;
                    uint32_t block = (y / 8) * (info.m_MCUWidth / 8) * 64 + (x / 8) * 64;
                    uint32_t index = block + (y & 7) * 8 + (x & 7);
                    uint8_t r = info.m_pMCUBufR[index];
                    uint8_t g = info.m_comps == 1 ? r : info.m_pMCUBufG[index];
                    uint8_t b = info.m_comps == 1 ? r : info.m_pMCUBufB[index];
                    if (px < (uint32_t)image->width && py < (uint32_t)image->height)
                        /* The shared framebuffer surface uses 0x00RRGGBB,
                           matching the packed 32-bit BGA pixel layout. */
                        image->pixels[py * image->width + px] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                }
            }
        }
    }
    return 1;
}

void JpegFreeImage(JPEG_IMAGE *image) {
    if (image && image->pixels) { kfree(image->pixels); image->pixels = 0; }
}
