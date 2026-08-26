#include <stdint.h>
#include "fb.h"
#include "vga.h"
#include "serial.h"
#include "io/port.h"
#include "io/pci.h"
#include "mm/mm.h"
#include "core/util.h"
#include "cdfs.h"
#include "ttf.h"

#define BGA_IOPORT_INDEX      0x01CE
#define BGA_IOPORT_DATA       0x01CF

#define BGA_INDEX_ID          0
#define BGA_INDEX_XRES        1
#define BGA_INDEX_YRES        2
#define BGA_INDEX_BPP         3
#define BGA_INDEX_ENABLE      4
#define BGA_INDEX_VIRT_WIDTH  6
#define BGA_INDEX_VIRT_HEIGHT 7
#define BGA_INDEX_X_OFFSET    8
#define BGA_INDEX_Y_OFFSET    9

#define BGA_ID0               0xB0C0
#define BGA_ID5               0xB0C5

#define BGA_DISABLED          0x00
#define BGA_ENABLED           0x01
#define BGA_LFB_ENABLED       0x40
#define BGA_NOCLEARMEM        0x80

#define QEMU_VGA_VENDOR_ID    0x1234
#define QEMU_VGA_DEVICE_ID    0x1111
#define VMWARE_VENDOR_ID      0x15AD
#define VMWARE_SVGA_DEVICE_ID 0x0405
#define VMWARE_SVGA2_DEVICE_ID 0x0710

int fb_width = 640;
int fb_height = 480;

static int use_framebuffer = 0;
static uint8_t *fb_addr = 0;
static uint8_t *fb_present_addr = 0;
static uint32_t fb_pitch = 0;
static uint8_t fb_bpp = 0;
static uint8_t *fb_shadow = 0;
static uint32_t *fb_surface = 0;
static int fb_clip_enabled;
static int fb_clip_left, fb_clip_top, fb_clip_right, fb_clip_bottom;
static uint16_t svga_io_port = 0;
static uint32_t *svga_fifo = 0;
static uint32_t svga_fifo_max = 0;
static int svga_active = 0;
static int bga_page_flip = 0;
static int bga_display_page = 0;
static int bga_pages_initialized = 0;
static int dirty_valid = 0;
static int dirty_x1 = 0;
static int dirty_y1 = 0;
static int dirty_x2 = 0;
static int dirty_y2 = 0;

typedef struct _FB_MODE {
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
} FB_MODE;

static const FB_MODE fb_modes[] = {
    {640, 480, 16}, {640, 480, 24}, {640, 480, 32},
    {800, 600, 16}, {800, 600, 24}, {800, 600, 32},
    {1024, 768, 16}, {1024, 768, 24}, {1024, 768, 32},
    {1152, 864, 16}, {1152, 864, 24}, {1152, 864, 32},
    {1280, 1024, 16}, {1280, 1024, 24}, {1280, 1024, 32}
    ,{640, 360, 16}, {640, 360, 24}, {640, 360, 32}
    ,{800, 450, 16}, {800, 450, 24}, {800, 450, 32}
    ,{960, 540, 16}, {960, 540, 24}, {960, 540, 32}
    ,{1280, 720, 16}, {1280, 720, 24}, {1280, 720, 32}
    ,{1366, 768, 16}, {1366, 768, 24}, {1366, 768, 32}
    ,{1600, 900, 16}, {1600, 900, 24}, {1600, 900, 32}
    ,{1920, 1080, 16}, {1920, 1080, 24}, {1920, 1080, 32}
};

#define FB_MODE_COUNT ((int)(sizeof(fb_modes) / sizeof(fb_modes[0])))

static const uint16_t vga_to_rgb565[16] = {
    0x0000, 0x001F, 0x07E0, 0x07FF, 0xF800, 0xF81F, 0xA145, 0xC618,
    0x4208, 0x3C7F, 0x87E0, 0x87FF, 0xFC10, 0xFC1F, 0xFFE0, 0xFFFF
};

static const uint32_t vga_to_rgb888[16] = {
    0x000000, 0x0000AA, 0x00AA00, 0x00AAAA,
    0xAA0000, 0xAA00AA, 0xAA5500, 0xAAAAAA,
    0x555555, 0x5555FF, 0x55FF55, 0x55FFFF,
    0xFF5555, 0xFF55FF, 0xFFFF55, 0xFFFFFF
};

static uint8_t fb_rgb_to_index(uint32_t rgb) {
    int r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
    uint32_t best = 0xFFFFFFFFU;
    uint8_t result = 0;
    for (uint8_t i = 0; i < 16; i++) {
        int dr = r - ((vga_to_rgb888[i] >> 16) & 0xFF);
        int dg = g - ((vga_to_rgb888[i] >> 8) & 0xFF);
        int db = b - (vga_to_rgb888[i] & 0xFF);
        uint32_t distance = (uint32_t)(dr * dr + dg * dg + db * db);
        if (distance < best) { best = distance; result = i; }
    }
    return result;
}

static const uint8_t font[96][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},{0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    {0x66,0x66,0x00,0x00,0x00,0x00,0x00,0x00},{0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00},
    {0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00},{0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00},
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00},{0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00},
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00},{0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},{0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},{0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},{0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00},
    {0x7C,0xC6,0xCE,0xD6,0xE6,0xC6,0x7C,0x00},{0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    {0x7C,0xC6,0x06,0x1C,0x30,0x66,0xFE,0x00},{0x7C,0xC6,0x06,0x3C,0x06,0xC6,0x7C,0x00},
    {0x1C,0x3C,0x6C,0xCC,0xFE,0x0C,0x1E,0x00},{0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00},
    {0x38,0x60,0xC0,0xFC,0xC6,0xC6,0x7C,0x00},{0xFE,0xC6,0x0C,0x18,0x30,0x30,0x30,0x00},
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00},{0x7C,0xC6,0xC6,0x7E,0x06,0x0C,0x78,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},{0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00},{0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00},
    {0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00},{0x7C,0xC6,0x0C,0x18,0x18,0x00,0x18,0x00},
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x78,0x00},{0x38,0x6C,0xC6,0xFE,0xC6,0xC6,0xC6,0x00},
    {0xFC,0x66,0x66,0x7C,0x66,0x66,0xFC,0x00},{0x3C,0x66,0xC0,0xC0,0xC0,0x66,0x3C,0x00},
    {0xF8,0x6C,0x66,0x66,0x66,0x6C,0xF8,0x00},{0xFE,0x62,0x68,0x78,0x68,0x62,0xFE,0x00},
    {0xFE,0x62,0x68,0x78,0x68,0x60,0xF0,0x00},{0x3C,0x66,0xC0,0xCE,0xC6,0x66,0x3E,0x00},
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00},{0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    {0x1E,0x0C,0x0C,0x0C,0xCC,0xCC,0x78,0x00},{0xE6,0x66,0x6C,0x78,0x6C,0x66,0xE6,0x00},
    {0xF0,0x60,0x60,0x60,0x62,0x66,0xFE,0x00},{0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00},
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00},{0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00},
    {0xFC,0x66,0x66,0x7C,0x60,0x60,0xF0,0x00},{0x7C,0xC6,0xC6,0xC6,0xC6,0x7C,0x0E,0x00},
    {0xFC,0x66,0x66,0x7C,0x6C,0x66,0xE6,0x00},{0x7C,0xC6,0x60,0x38,0x0C,0xC6,0x7C,0x00},
    {0x7E,0x5A,0x18,0x18,0x18,0x18,0x3C,0x00},{0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00},
    {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00},{0xC6,0xC6,0xD6,0xD6,0xFE,0x6C,0x6C,0x00},
    {0xC6,0x6C,0x38,0x38,0x6C,0xC6,0xC6,0x00},{0x66,0x66,0x3C,0x18,0x18,0x18,0x3C,0x00},
    {0xFE,0xC6,0x8C,0x18,0x32,0x66,0xFE,0x00},{0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00},
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00},{0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00},
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00},{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF},
    {0x30,0x18,0x0C,0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x78,0x0C,0x7C,0xCC,0x76,0x00},
    {0xE0,0x60,0x7C,0x66,0x66,0x66,0xDC,0x00},{0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00},
    {0x1C,0x0C,0x7C,0xCC,0xCC,0xCC,0x76,0x00},{0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00},
    {0x38,0x6C,0x60,0xF0,0x60,0x60,0xF0,0x00},{0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0xF8},
    {0xE0,0x60,0x6C,0x76,0x66,0x66,0xE6,0x00},{0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00},
    {0x06,0x00,0x06,0x06,0x06,0x66,0x66,0x3C},{0xE0,0x60,0x66,0x6C,0x78,0x6C,0xE6,0x00},
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},{0x00,0x00,0xEC,0xFE,0xD6,0xD6,0xC6,0x00},
    {0x00,0x00,0xF8,0xCC,0xCC,0xCC,0xCC,0x00},{0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00},
    {0x00,0x00,0xDC,0x66,0x66,0x7C,0x60,0xF0},{0x00,0x00,0x76,0xCC,0xCC,0x7C,0x0C,0x1E},
    {0x00,0x00,0xDC,0x76,0x60,0x60,0xF0,0x00},{0x00,0x00,0x7C,0xC0,0x7C,0x06,0xFC,0x00},
    {0x30,0x30,0xFC,0x30,0x30,0x36,0x1C,0x00},{0x00,0x00,0xCC,0xCC,0xCC,0xCC,0x76,0x00},
    {0x00,0x00,0xC6,0xC6,0x6C,0x38,0x10,0x00},{0x00,0x00,0xC6,0xD6,0xFE,0x6C,0x6C,0x00},
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00},{0x00,0x00,0xC6,0xC6,0x7E,0x06,0xFC,0x00},
    {0x00,0x00,0xFE,0x8C,0x18,0x32,0xFE,0x00},{0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00},
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},{0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00},
    {0x76,0xDC,0x00,0x00,0x00,0x00,0x00,0x00},
};

static const uint8_t *fb_get_font(char c) {
    if (c >= 32 && c <= 126) return font[c - 32];
    return font[0];
}

/* The TrueType renderer in drivers/fb/ttf.c is NOT the active font backend ATM. */
/* TrueType support is broken in discouNT, and IDK why. */
static int fb_use_ttf_glyphs = 0;

#if 0
/* Retired experimental runtime TrueType reader. The implementation in
 * drivers/fb/ttf.c is the only active font backend. It intentionally covers
 * by the system UI: cmap format 4, hmtx, loca, and simple glyf outlines. */
typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t cmap, head, hhea, hmtx, loca, glyf;
    uint16_t units, metrics, glyphs;
    int16_t loca_format;
    uint8_t glyphs8[95][12];
    int ready;
} FB_TTF;

static FB_TTF fb_ttf;
/* The outline backend is kept behind this gate while it is being replaced.
 * A malformed glyph must never corrupt the system UI; the built-in 8x8
 * console font remains the safe runtime renderer until the new backend is
 * validated. */
/* Curve flattening can need up to four samples per source point.  Keep these
 * buffers out of the early kernel stack; the font is loaded synchronously. */
static int32_t ttf_curve_x[1024];
static int32_t ttf_curve_y[1024];
static int32_t ttf_edge_x1[4096], ttf_edge_y1[4096];
static int32_t ttf_edge_x2[4096], ttf_edge_y2[4096];
static int ttf_edge_count;

static uint16_t ttf_u16(const uint8_t *p) { return ((uint16_t)p[0] << 8) | p[1]; }
static int16_t ttf_s16(const uint8_t *p) { return (int16_t)ttf_u16(p); }
static uint32_t ttf_u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint32_t ttf_table(uint8_t *data, uint32_t size, uint32_t tag) {
    uint16_t count;
    uint32_t i;
    if (!data || size < 12) return 0;
    count = ttf_u16(data + 4);
    if (12U + (uint32_t)count * 16U > size) return 0;
    for (i = 0; i < count; i++) {
        uint8_t *rec = data + 12 + i * 16;
        if (ttf_u32(rec) == tag) {
            uint32_t off = ttf_u32(rec + 8), len = ttf_u32(rec + 12);
            if (off <= size && len <= size - off) return off;
        }
    }
    return 0;
}

static int ttf_cmap_glyph(uint32_t code) {
    uint8_t *base;
    uint16_t n, i;
    if (!fb_ttf.cmap || code > 0xFFFF) return 0;
    base = fb_ttf.data + fb_ttf.cmap;
    n = ttf_u16(base + 2);
    for (i = 0; i < n; i++) {
        uint8_t *rec = base + 4 + i * 8;
        uint32_t off = ttf_u32(rec + 4);
        uint8_t *sub;
        uint16_t format, segs, j;
        if (4U + (uint32_t)i * 8U + 8U > 0x100000U || off >= fb_ttf.size - fb_ttf.cmap) continue;
        sub = base + off;
        format = ttf_u16(sub);
        if (format != 4) continue;
        segs = ttf_u16(sub + 6) / 2;
        if (segs == 0 || 16U + (uint32_t)segs * 8U > fb_ttf.size - fb_ttf.cmap - off) continue;
        for (j = 0; j < segs; j++) {
            uint8_t *end = sub + 14 + j * 2;
            uint8_t *start = sub + 16 + segs * 2 + j * 2;
            uint8_t *delta = start + segs * 2;
            uint8_t *range = delta + segs * 2;
            uint16_t end_code = ttf_u16(end), start_code = ttf_u16(start);
            int32_t glyph;
            if (code > end_code || code < start_code) continue;
            if (ttf_u16(range) == 0) glyph = (int32_t)code + ttf_s16(delta);
            else {
                uint8_t *entry = range + ttf_u16(range) + (code - start_code) * 2;
                if (entry + 2 > fb_ttf.data + fb_ttf.size) return 0;
                glyph = ttf_u16(entry);
                if (glyph) glyph += ttf_s16(delta);
            }
            if (glyph < 0 || glyph >= fb_ttf.glyphs) return 0;
            return glyph;
        }
    }
    return 0;
}

static uint32_t ttf_glyph_offset(uint16_t glyph) {
    if (glyph >= fb_ttf.glyphs || !fb_ttf.loca) return 0;
    if (fb_ttf.loca_format == 0) return (uint32_t)ttf_u16(fb_ttf.data + fb_ttf.loca + glyph * 2) * 2;
    return ttf_u32(fb_ttf.data + fb_ttf.loca + glyph * 4);
}

static void ttf_plot_edges(uint8_t out[12], int32_t xmin, int32_t xmax,
                           int32_t ymin, int32_t ymax) {
    int py, px, sx4, sy4, i, j;
    if (xmax <= xmin || ymax <= ymin || ttf_edge_count < 3) return;
    for (py = 0; py < 12; py++) for (px = 0; px < 8; px++) {
        int covered = 0;
        /* Four-by-four supersampling keeps thin stems and curves visible at
         * the small console size without storing a pre-rasterized font. */
        for (sy4 = 0; sy4 < 4; sy4++) for (sx4 = 0; sx4 < 4; sx4++) {
            int inside = 0;
            int32_t sx = xmin + (((px * 4 + sx4) * 2 + 1) * (xmax - xmin)) / 64;
            int32_t sy = ymax - (((py * 4 + sy4) * 2 + 1) * (ymax - ymin)) / 96;
            for (i = 0; i < ttf_edge_count; i++) {
                j = i;
                if (((ttf_edge_y1[i] > sy) != (ttf_edge_y2[i] > sy)) &&
                    sx < (ttf_edge_x2[i] - ttf_edge_x1[i]) *
                         (sy - ttf_edge_y1[i]) /
                         (ttf_edge_y2[i] - ttf_edge_y1[i]) + ttf_edge_x1[i])
                    inside = !inside;
            }
            if (inside) covered++;
        }
        if (covered >= 6) out[py] |= (uint8_t)(0x80 >> px);
    }
}

static void ttf_add_curve_point(int32_t *x, int32_t *y, int *count,
                                int32_t px, int32_t py) {
    if (*count >= 1024) return;
    x[*count] = px;
    y[*count] = py;
    (*count)++;
}

/* Convert a quadratic TrueType contour into a polygon.  Points with the
 * on-curve flag clear are quadratic control points; treating them as normal
 * vertices produces the broken, spiky glyphs seen with small fonts. */
static int ttf_flatten_contour(int32_t *srcx, int32_t *srcy, uint8_t *flags,
                               int begin, int end, int32_t *dstx, int32_t *dsty) {
    int n = end - begin + 1;
    int first = begin;
    int last = end;
    int i;
    int out = 0;
    int32_t curx, cury;

    if (n < 1) return 0;
    if (flags[first] & 1) {
        curx = srcx[first];
        cury = srcy[first];
    } else if (flags[last] & 1) {
        curx = srcx[last];
        cury = srcy[last];
    } else {
        curx = (srcx[first] + srcx[last]) / 2;
        cury = (srcy[first] + srcy[last]) / 2;
    }
    ttf_add_curve_point(dstx, dsty, &out, curx, cury);

    for (i = 0; i < n; i++) {
        int at = begin + i;
        int next = (i + 1 < n) ? at + 1 : begin;
        int next2 = (i + 2 < n) ? at + 2 : begin + ((i + 2) - n);
        int32_t ex, ey;

        if (flags[at] & 1) {
            ex = srcx[at];
            ey = srcy[at];
            ttf_add_curve_point(dstx, dsty, &out, ex, ey);
            curx = ex;
            cury = ey;
            continue;
        }

        if (flags[next] & 1) {
            ex = srcx[next];
            ey = srcy[next];
        } else {
            ex = (srcx[next] + srcx[next2]) / 2;
            ey = (srcy[next] + srcy[next2]) / 2;
        }

        {
            int step;
            int32_t cx = srcx[at], cy = srcy[at];
            for (step = 1; step <= 4; step++) {
                int32_t t = step * 256 / 4;
                int32_t a = 256 - t;
                int32_t qx = (a * a * curx + 2 * a * t * cx + t * t * ex) / 65536;
                int32_t qy = (a * a * cury + 2 * a * t * cy + t * t * ey) / 65536;
                ttf_add_curve_point(dstx, dsty, &out, qx, qy);
            }
        }
        curx = ex;
        cury = ey;
    }
    return out;
}

static void ttf_render_glyph(uint16_t glyph, uint8_t out[12]) {
    uint32_t off = ttf_glyph_offset(glyph), next = ttf_glyph_offset(glyph + 1);
    int16_t contours;
    int32_t x[256], y[256];
    uint16_t ends[64];
    uint16_t points = 0, i, c;
    uint8_t flags[256];
    uint8_t *p;
    int32_t xmin, xmax, ymin, ymax;
    if (!off || !next || next <= off || next > fb_ttf.size - fb_ttf.glyf) return;
    p = fb_ttf.data + fb_ttf.glyf + off;
    if (p + 10 > fb_ttf.data + fb_ttf.size || (contours = ttf_s16(p)) <= 0 || contours > 64) return;
    /* glyf header order is:
     *   numberOfContours, xMin, yMin, xMax, yMax
     * Keep the coordinate order intact; treating yMin as xMax makes every
     * glyph appear to have a zero-width outline. */
    xmin = ttf_s16(p + 2);
    ymin = ttf_s16(p + 4);
    xmax = ttf_s16(p + 6);
    ymax = ttf_s16(p + 8);
    p += 10;
    for (c = 0; c < (uint16_t)contours; c++) ends[c] = ttf_u16(p + c * 2);
    points = ends[contours - 1] + 1;
    if (points == 0 || points > 256) return;
    p += contours * 2;
    p += 2 + ttf_u16(p);
    for (i = 0; i < points; i++) {
        if (p >= fb_ttf.data + fb_ttf.size) return;
        flags[i] = *p++;
        if (flags[i] & 8) {
            uint8_t repeat = *p++;
            while (repeat-- && i + 1 < points) flags[++i] = flags[i - 1];
        }
    }
    x[0] = y[0] = 0;
    for (i = 0; i < points; i++) {
        int16_t delta = 0;
        if (flags[i] & 2) delta = (flags[i] & 16) ? *p++ : -(int16_t)*p++;
        else if (!(flags[i] & 16)) delta = ttf_s16(p), p += 2;
        x[i] = (i ? x[i - 1] : 0) + delta;
    }
    for (i = 0; i < points; i++) {
        int16_t delta = 0;
        if (flags[i] & 4) delta = (flags[i] & 32) ? *p++ : -(int16_t)*p++;
        else if (!(flags[i] & 32)) delta = ttf_s16(p), p += 2;
        y[i] = (i ? y[i - 1] : 0) + delta;
    }
    ttf_edge_count = 0;
    for (c = 0, i = 0; c < (uint16_t)contours; c++) {
        uint16_t end = ends[c], begin = i;
        uint16_t n = end - begin + 1;
        int flattened;
        int k;
        if (n > 256) return;
        flattened = ttf_flatten_contour(x, y, flags, begin, end,
                                        ttf_curve_x, ttf_curve_y);
        for (k = 0; k < flattened; k++) {
            int next = (k + 1 < flattened) ? k + 1 : 0;
            if (ttf_edge_count >= 4096) return;
            if (ttf_curve_x[k] == ttf_curve_x[next] &&
                ttf_curve_y[k] == ttf_curve_y[next]) continue;
            ttf_edge_x1[ttf_edge_count] = ttf_curve_x[k];
            ttf_edge_y1[ttf_edge_count] = ttf_curve_y[k];
            ttf_edge_x2[ttf_edge_count] = ttf_curve_x[next];
            ttf_edge_y2[ttf_edge_count] = ttf_curve_y[next];
            ttf_edge_count++;
        }
        i = end + 1;
    }
    ttf_plot_edges(out, xmin, xmax, ymin, ymax);
}

static void fb_load_ttf(void) {
    uint8_t *data = 0;
    uint32_t size = 0;
    int c;
    if (fb_ttf.ready || !CdfsReadFile("/SYSTEM32/FONTS/TAHOMA.TTF", &data, &size)) return;
    if (size < 12 || !ttf_table(data, size, 0x68656164U) ||
        !ttf_table(data, size, 0x636D6170U) || !ttf_table(data, size, 0x676C7966U)) { kfree(data); return; }
    fb_ttf.data = data; fb_ttf.size = size;
    fb_ttf.head = ttf_table(data, size, 0x68656164U);
    fb_ttf.cmap = ttf_table(data, size, 0x636D6170U);
    fb_ttf.hhea = ttf_table(data, size, 0x68686561U);
    fb_ttf.hmtx = ttf_table(data, size, 0x686D7478U);
    fb_ttf.loca = ttf_table(data, size, 0x6C6F6361U);
    fb_ttf.glyf = ttf_table(data, size, 0x676C7966U);
    fb_ttf.units = ttf_u16(data + fb_ttf.head + 18);
    fb_ttf.loca_format = ttf_s16(data + fb_ttf.head + 50);
    fb_ttf.metrics = ttf_u16(data + fb_ttf.hhea + 34);
    {
        uint32_t maxp = ttf_table(data, size, 0x6D617870U);
        if (!maxp) { kfree(data); fb_ttf.data = 0; return; }
        fb_ttf.glyphs = ttf_u16(data + maxp + 4);
    }
    for (c = 0; c < 95; c++) ttf_render_glyph((uint16_t)ttf_cmap_glyph((uint32_t)c + 32), fb_ttf.glyphs8[c]);
    fb_ttf.ready = 1;
}
#endif

static uint8_t *fb_indexed_buffer(void) {
    if (use_framebuffer) return fb_shadow;
    return back_buffer;
}

static int fb_indexed_stride(void) {
    if (use_framebuffer) return fb_width;
    return 640;
}

static int fb_alloc_surfaces(uint32_t pixels) {
    if (fb_shadow) kfree(fb_shadow);
    if (fb_surface) kfree(fb_surface);
    fb_shadow = (uint8_t*)kmalloc(pixels);
    fb_surface = (uint32_t*)kmalloc(pixels * sizeof(uint32_t));
    if (!fb_shadow || !fb_surface) {
        if (fb_shadow) kfree(fb_shadow);
        if (fb_surface) kfree(fb_surface);
        fb_shadow = 0;
        fb_surface = 0;
        return 0;
    }
    return 1;
}

static void bga_write(uint16_t index, uint16_t value) {
#if defined(__loongarch64)
    volatile uint16_t *vbe = (volatile uint16_t *)(uintptr_t)0x41000500UL;
    vbe[index] = value;
    __asm__ volatile("dbar 0" ::: "memory");
#else
    outw(BGA_IOPORT_INDEX, index);
    outw(BGA_IOPORT_DATA, value);
#endif
}

static uint16_t bga_read(uint16_t index) {
#if defined(__loongarch64)
    volatile uint16_t *vbe = (volatile uint16_t *)(uintptr_t)0x41000500UL;
    return vbe[index];
#else
    outw(BGA_IOPORT_INDEX, index);
    return inw(BGA_IOPORT_DATA);
#endif
}

static void fb_reset_dirty(void) {
    dirty_valid = 0;
    dirty_x1 = dirty_y1 = dirty_x2 = dirty_y2 = 0;
}

static void fb_mark_dirty(int x, int y, int w, int h) {
    int x2;
    int y2;

    if (!use_framebuffer) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= fb_width || y >= fb_height || w <= 0 || h <= 0) return;
    if (x + w > fb_width) w = fb_width - x;
    if (y + h > fb_height) h = fb_height - y;
    if (w <= 0 || h <= 0) return;

    x2 = x + w - 1;
    y2 = y + h - 1;

    if (!dirty_valid) {
        dirty_x1 = x;
        dirty_y1 = y;
        dirty_x2 = x2;
        dirty_y2 = y2;
        dirty_valid = 1;
    } else {
        if (x < dirty_x1) dirty_x1 = x;
        if (y < dirty_y1) dirty_y1 = y;
        if (x2 > dirty_x2) dirty_x2 = x2;
        if (y2 > dirty_y2) dirty_y2 = y2;
    }
}

static uint32_t fb_find_lfb_phys(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = PciConfigRead32((uint8_t)bus, slot, func, 0x00);
                if (id == 0xFFFFFFFFU) {
                    if (func == 0) break;
                    continue;
                }

                if ((id & 0xFFFFU) == QEMU_VGA_VENDOR_ID &&
                    ((id >> 16) & 0xFFFFU) == QEMU_VGA_DEVICE_ID) {
                    uint32_t bar0 = PciConfigRead32((uint8_t)bus, slot, func, 0x10);
                    if ((bar0 & 0xFFFFFFF0U) != 0) {
                        SerialPutString("[FB] QEMU VGA PCI BAR0=0x");
                        SerialPrintHex(bar0 & 0xFFFFFFF0U);
                        SerialPutString("\r\n");
                        return bar0 & 0xFFFFFFF0U;
                    }
                }
            }
        }
    }

    SerialPutString("[FB] VGA PCI BAR0 not found, trying legacy LFB\r\n");
    return 0xE0000000U;
}

static int fb_try_bga_mode(uint16_t width, uint16_t height, uint16_t bpp) {
    uint16_t id = bga_read(BGA_INDEX_ID);
    uint32_t phys;
    uint32_t shadow_size;

    SerialPutString("[FB] BGA ID=0x");
    SerialPrintHex(id);
    SerialPutString("\r\n");

    if (id < BGA_ID0 || id > BGA_ID5) {
        SerialPutString("[FB] Bochs/QEMU BGA not present\r\n");
        return 0;
    }

    phys = fb_find_lfb_phys();
    if (phys < 0x100000U) {
        SerialPutString("[FB] Invalid LFB address\r\n");
        return 0;
    }

    bga_write(BGA_INDEX_ENABLE, BGA_DISABLED);
    bga_write(BGA_INDEX_XRES, width);
    bga_write(BGA_INDEX_YRES, height);
    /* Reserve two scanout pages. The compositor presents by changing the
       display offset after the hidden page has been fully updated. */
    bga_write(BGA_INDEX_VIRT_WIDTH, width);
    bga_write(BGA_INDEX_VIRT_HEIGHT, height * 2);
    bga_write(BGA_INDEX_BPP, bpp);
    bga_write(BGA_INDEX_X_OFFSET, 0);
    bga_write(BGA_INDEX_Y_OFFSET, 0);
    bga_write(BGA_INDEX_ENABLE, BGA_ENABLED | BGA_LFB_ENABLED | BGA_NOCLEARMEM);

    fb_width = bga_read(BGA_INDEX_XRES);
    fb_height = bga_read(BGA_INDEX_YRES);
    fb_bpp = (uint8_t)bga_read(BGA_INDEX_BPP);

    if (fb_width != width || fb_height != height || fb_bpp != bpp) {
        SerialPutString("[FB] BGA mode set mismatch\r\n");
        return 0;
    }

    fb_addr = (uint8_t*)(uintptr_t)phys;
    fb_present_addr = fb_addr;
    fb_pitch = fb_width * (fb_bpp / 8);
    bga_page_flip = 1;
    bga_display_page = 0;
    bga_pages_initialized = 0;
    shadow_size = (uint32_t)fb_width * (uint32_t)fb_height;

    if (!fb_alloc_surfaces(shadow_size)) {
        SerialPutString("[FB] Software surface allocation failed\r\n");
        return 0;
    }

    for (uint32_t i = 0; i < shadow_size; i++) fb_shadow[i] = 0;

    use_framebuffer = 1;
    fb_reset_dirty();
    fb_mark_dirty(0, 0, fb_width, fb_height);

    SerialPutString("[FB] BGA framebuffer active: ");
    SerialPrintDec(fb_width);
    SerialPutString("x");
    SerialPrintDec(fb_height);
    SerialPutString("x");
    SerialPrintDec(fb_bpp);
    SerialPutString(" @ 0x");
    SerialPrintHex((uint32_t)(uintptr_t)fb_addr);
    SerialPutString("\r\n");

    return 1;
}

/* VMware SVGA II register protocol.  VMware presents the framebuffer as a
 * PCI memory BAR and the device registers through an I/O BAR; it does not
 * implement the Bochs BGA ports used by QEMU's std VGA device. */
#define SVGA_INDEX_ID             0
#define SVGA_INDEX_ENABLE         1
#define SVGA_INDEX_WIDTH          2
#define SVGA_INDEX_HEIGHT         3
#define SVGA_INDEX_DEPTH          4
#define SVGA_INDEX_BITS_PER_PIXEL 7
#define SVGA_INDEX_BYTES_PER_LINE 12
#define SVGA_INDEX_FB_START       13
#define SVGA_INDEX_FB_OFFSET      14

#define SVGA_ID_1                 0x90000001U
#define SVGA_ID_2                 0x90000002U
#define SVGA_CMD_UPDATE           1U

static void svga_write(uint16_t index_port, uint32_t index, uint32_t value) {
    outl(index_port, index);
    /* VMware's SVGA value register is at BAR1 + 1.  The register interface
     * is byte-spaced even though values are transferred as 32-bit words. */
    outl((uint16_t)(index_port + 1), value);
}

static uint32_t svga_read(uint16_t index_port, uint32_t index) {
    outl(index_port, index);
    return inl((uint16_t)(index_port + 1));
}

static int fb_find_vmware(uint16_t *io_port, uint32_t *fb_phys) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            for (uint8_t func = 0; func < 8; func++) {
                uint32_t id = PciConfigRead32((uint8_t)bus, slot, func, 0x00);
                uint16_t vendor;
                uint16_t device;
                uint32_t bar0;
                uint32_t bar1;

                if (id == 0xFFFFFFFFU) {
                    if (func == 0) break;
                    continue;
                }
                vendor = (uint16_t)(id & 0xFFFFU);
                device = (uint16_t)(id >> 16);
                if (vendor != VMWARE_VENDOR_ID ||
                    (device != VMWARE_SVGA_DEVICE_ID &&
                     device != VMWARE_SVGA2_DEVICE_ID)) continue;

                /* Enable I/O and memory decoding plus bus mastering. */
                PciConfigWrite32((uint8_t)bus, slot, func, 0x04,
                                 PciConfigRead32((uint8_t)bus, slot, func, 0x04) | 0x7U);
                bar0 = PciConfigRead32((uint8_t)bus, slot, func, 0x10);
                bar1 = PciConfigRead32((uint8_t)bus, slot, func, 0x14);

                /* 0405 uses BAR0 for the index/value I/O pair and BAR1 for
                 * the framebuffer.  The older 0710 keeps the legacy I/O
                 * ports and normally exposes the framebuffer in BAR0. */
                if (device == VMWARE_SVGA_DEVICE_ID) {
                    if ((bar0 & 1U) == 0 || (bar1 & 1U) != 0) continue;
                    *io_port = (uint16_t)(bar0 & 0xFFFCU);
                    *fb_phys = bar1 & 0xFFFFFFF0U;
                } else {
                    *io_port = 0x4560;
                    if ((bar0 & 1U) == 0 && (bar0 & 0xFFFFFFF0U))
                        *fb_phys = bar0 & 0xFFFFFFF0U;
                    else if ((bar1 & 1U) == 0 && (bar1 & 0xFFFFFFF0U))
                        *fb_phys = bar1 & 0xFFFFFFF0U;
                    else
                        continue;
                }
                SerialPutString("[FB] VMware SVGA PCI BAR0=0x");
                SerialPrintHex(*fb_phys);
                SerialPutString(" BAR1=0x");
                SerialPrintHex(*io_port);
                SerialPutString("\r\n");
                return 1;
            }
        }
    }
    return 0;
}

static int fb_try_vmware_mode(uint16_t width, uint16_t height, uint16_t bpp) {
    uint16_t io_port;
    uint32_t bar_fb;
    uint32_t fb_start;
    uint32_t pitch;
    uint32_t shadow_size;
    uint32_t id;
    uint32_t mem_start;
    uint32_t mem_size;

    if (!fb_find_vmware(&io_port, &bar_fb)) return 0;

    svga_write(io_port, SVGA_INDEX_ID, SVGA_ID_2);
    id = svga_read(io_port, SVGA_INDEX_ID);
    if (id != SVGA_ID_2 && id != SVGA_ID_1) {
        SerialPutString("[FB] VMware SVGA protocol negotiation failed\r\n");
        return 0;
    }

    svga_write(io_port, SVGA_INDEX_ENABLE, 0);
    svga_write(io_port, SVGA_INDEX_WIDTH, width);
    svga_write(io_port, SVGA_INDEX_HEIGHT, height);
    svga_write(io_port, SVGA_INDEX_BITS_PER_PIXEL, bpp);
    svga_write(io_port, SVGA_INDEX_ENABLE, 1);

    fb_width = (int)svga_read(io_port, SVGA_INDEX_WIDTH);
    fb_height = (int)svga_read(io_port, SVGA_INDEX_HEIGHT);
    fb_bpp = (uint8_t)svga_read(io_port, SVGA_INDEX_BITS_PER_PIXEL);
    pitch = svga_read(io_port, SVGA_INDEX_BYTES_PER_LINE);
    fb_start = svga_read(io_port, SVGA_INDEX_FB_START);
    if (!fb_start) fb_start = bar_fb;

    if (fb_width != width || fb_height != height ||
        (fb_bpp != bpp && fb_bpp != 32) || pitch < (uint32_t)fb_width * 4U ||
        fb_start < 0x100000U) {
        SerialPutString("[FB] VMware SVGA mode set mismatch\r\n");
        return 0;
    }

    fb_addr = (uint8_t*)(uintptr_t)(fb_start + svga_read(io_port, SVGA_INDEX_FB_OFFSET));
    fb_pitch = pitch;
    svga_io_port = io_port;
    svga_active = 0;

    /* VMware does not refresh the host display merely because guest memory
     * changed.  Set up the minimum command FIFO and use UPDATE commands for
     * dirty rectangles. */
    mem_start = svga_read(io_port, 18); /* SVGA_REG_MEM_START */
    mem_size = svga_read(io_port, 19);  /* SVGA_REG_MEM_SIZE */
    if (mem_start >= 0x100000U && mem_size >= 1024U) {
        svga_fifo = (uint32_t*)(uintptr_t)mem_start;
        svga_fifo[0] = 16;                    /* FIFO_MIN */
        svga_fifo[1] = 16 + 10 * 1024;       /* FIFO_MAX */
        if (svga_fifo[1] > mem_size) svga_fifo[1] = mem_size & ~3U;
        svga_fifo[2] = 16;                    /* FIFO_NEXT_CMD */
        svga_fifo[3] = 16;                    /* FIFO_STOP */
        svga_fifo_max = svga_fifo[1];
        svga_write(io_port, 20, 1);           /* SVGA_REG_CONFIG_DONE */
        if (svga_fifo_max >= 36) svga_active = 1;
    }
    shadow_size = (uint32_t)fb_width * (uint32_t)fb_height;
    if (!fb_alloc_surfaces(shadow_size)) {
        SerialPutString("[FB] VMware software surface allocation failed\r\n");
        return 0;
    }
    for (uint32_t i = 0; i < shadow_size; i++) fb_shadow[i] = 0;

    use_framebuffer = 1;
    fb_reset_dirty();
    fb_mark_dirty(0, 0, fb_width, fb_height);
    SerialPutString("[FB] VMware SVGA framebuffer active: ");
    SerialPrintDec(fb_width);
    SerialPutString("x");
    SerialPrintDec(fb_height);
    SerialPutString("x");
    SerialPrintDec(fb_bpp);
    SerialPutString(" pitch=");
    SerialPrintDec(fb_pitch);
    SerialPutString(" @ 0x");
    SerialPrintHex((uint32_t)(uintptr_t)fb_addr);
    SerialPutString("\r\n");
    if (!svga_active) {
        SerialPutString("[FB] VMware FIFO unavailable; framebuffer disabled\r\n");
        use_framebuffer = 0;
        kfree(fb_shadow);
        fb_shadow = 0;
        kfree(fb_surface);
        fb_surface = 0;
        fb_addr = 0;
        return 0;
    }
    return 1;
}

static void svga_update(int x, int y, int w, int h) {
    uint32_t next;
    if (!svga_active || !svga_fifo || w <= 0 || h <= 0) return;
    next = svga_fifo[2];
    if (next < svga_fifo[0] || next + 20 > svga_fifo_max) next = svga_fifo[0];
    *(uint32_t*)((uint8_t*)svga_fifo + next + 0) = SVGA_CMD_UPDATE;
    *(uint32_t*)((uint8_t*)svga_fifo + next + 4) = (uint32_t)x;
    *(uint32_t*)((uint8_t*)svga_fifo + next + 8) = (uint32_t)y;
    *(uint32_t*)((uint8_t*)svga_fifo + next + 12) = (uint32_t)w;
    *(uint32_t*)((uint8_t*)svga_fifo + next + 16) = (uint32_t)h;
    next += 20;
    if (next >= svga_fifo_max) next = svga_fifo[0];
    svga_fifo[2] = next;
}

static void fb_write_hw_rgb(int x, int y, uint32_t rgb) {
    uint8_t *row;
    if (!fb_addr || x < 0 || x >= fb_width || y < 0 || y >= fb_height) return;
    row = fb_present_addr + y * fb_pitch;
    if (fb_bpp == 16) ((uint16_t*)row)[x] = (uint16_t)(((rgb >> 19) << 11) | (((rgb >> 10) & 0x3F) << 5) | ((rgb >> 3) & 0x1F));
    else if (fb_bpp == 24) { uint8_t *p = row + x * 3; p[0] = rgb; p[1] = rgb >> 8; p[2] = rgb >> 16; }
    else ((uint32_t*)row)[x] = rgb & 0x00FFFFFFU;
}

void FbInit(void *mb_info_ptr) {
    (void)mb_info_ptr;

    use_framebuffer = 0;
    svga_active = 0;
    svga_fifo = 0;
    svga_fifo_max = 0;
    fb_addr = 0;
    fb_present_addr = 0;
    fb_surface = 0;
    fb_pitch = 0;
    fb_bpp = 0;
    bga_page_flip = 0;
    bga_display_page = 0;
    bga_pages_initialized = 0;
    fb_width = 640;
    fb_height = 480;
    fb_reset_dirty();

    if (fb_try_bga_mode(800, 600, 32) || fb_try_bga_mode(640, 480, 32) ||
        fb_try_vmware_mode(800, 600, 32) || fb_try_vmware_mode(640, 480, 32)) {
        SerialPutString("[FB] Native software compositor active\r\n");
        FbTtfLoad("/SYSTEM32/FONTS/TAHOMA.TTF");
        FbSwapBuffers();
        return;
    }

    SerialPutString("[FB] Falling back to VGA\r\n");
    VgaInit();
}

int FbIsFramebuffer(void) { return use_framebuffer; }
int FbGetWidth(void) { return fb_width; }
int FbGetHeight(void) { return fb_height; }
int FbGetModeCount(void) { return FB_MODE_COUNT; }

int FbGetModeInfo(int index, int *width, int *height, int *bpp) {
    if (index < 0 || index >= FB_MODE_COUNT) return 0;
    if (width) *width = fb_modes[index].width;
    if (height) *height = fb_modes[index].height;
    if (bpp) *bpp = fb_modes[index].bpp;
    return 1;
}

int FbSetResolution(int width, int height, int bpp) {
    int valid = 0;
    if (!use_framebuffer) return 0;
    if (width < 320 || height < 200) return 0;
    if (bpp != 16 && bpp != 24 && bpp != 32) return 0;
    for (int i = 0; i < FB_MODE_COUNT; i++) {
        if (fb_modes[i].width == width &&
            fb_modes[i].height == height &&
            fb_modes[i].bpp == bpp) {
            valid = 1;
            break;
        }
    }
    if (!valid) return 0;
    if (!fb_try_bga_mode((uint16_t)width, (uint16_t)height, (uint16_t)bpp)) return 0;
    FbClearScreen(COLOR_BLUE);
    FbSwapBuffers();
    return 1;
}

uint8_t FbGetPixel(int x, int y) {
    uint8_t *buf = fb_indexed_buffer();
    int stride = fb_indexed_stride();
    int width = use_framebuffer ? fb_width : 640;
    int height = use_framebuffer ? fb_height : 480;
    if (!buf || x < 0 || x >= width || y < 0 || y >= height) return 0;
    return buf[y * stride + x];
}

uint32_t FbGetPixelRGB(int x, int y) {
    if (use_framebuffer && fb_surface && x >= 0 && x < fb_width && y >= 0 && y < fb_height)
        return fb_surface[y * fb_width + x];
    return vga_to_rgb888[FbGetPixel(x, y) & 0x0F];
}

void FbPutPixelRGB(int x, int y, uint32_t rgb) {
    if (!use_framebuffer) { FbPutPixel(x, y, fb_rgb_to_index(rgb)); return; }
    if (!fb_surface || !fb_shadow || x < 0 || x >= fb_width || y < 0 || y >= fb_height) return;
    fb_surface[y * fb_width + x] = rgb;
    fb_shadow[y * fb_width + x] = fb_rgb_to_index(rgb);
    fb_mark_dirty(x, y, 1, 1);
}

void FbCaptureRGB(int x, int y, int w, int h, uint32_t *dst, int dst_stride) {
    if (!dst || !fb_surface || w <= 0 || h <= 0 || dst_stride < w) return;
    for (int row = 0; row < h; row++) {
        int sy = y + row;
        if (sy < 0 || sy >= fb_height) continue;
        for (int col = 0; col < w; col++) {
            int sx = x + col;
            dst[row * dst_stride + col] = (sx >= 0 && sx < fb_width) ?
                fb_surface[sy * fb_width + sx] : 0;
        }
    }
}

void FbBlitRGB(int x, int y, int w, int h, const uint32_t *src, int src_stride) {
    if (!src || !fb_surface || w <= 0 || h <= 0 || src_stride < w) return;
    if (x >= 0 && y >= 0 && x + w <= fb_width && y + h <= fb_height) {
        for (int row = 0; row < h; row++)
            memcpy(fb_surface + (y + row) * fb_width + x,
                   src + row * src_stride, (uint32_t)w * sizeof(uint32_t));
        fb_mark_dirty(x, y, w, h);
        return;
    }
    for (int row = 0; row < h; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= fb_height) continue;
        for (int col = 0; col < w; col++) {
            int dx = x + col;
            if (dx < 0 || dx >= fb_width) continue;
            uint32_t rgb = src[row * src_stride + col];
            fb_surface[dy * fb_width + dx] = rgb;
        }
    }
    fb_mark_dirty(x, y, w, h);
}

void FbClearScreen(uint8_t color) {
    if (use_framebuffer) {
        uint32_t count = (uint32_t)fb_width * (uint32_t)fb_height;
        uint8_t indexed = color & 0x0F;
        uint32_t rgb = vga_to_rgb888[indexed];
        if (!fb_shadow || !fb_surface) return;
        for (uint32_t i = 0; i < count; i++) {
            fb_shadow[i] = indexed;
            fb_surface[i] = rgb;
        }
        fb_mark_dirty(0, 0, fb_width, fb_height);
    } else {
        VgaClearScreen(color);
    }
}

void FbPutPixel(int x, int y, uint8_t color) {
    if (use_framebuffer) {
        if (!fb_shadow || !fb_surface || x < 0 || x >= fb_width || y < 0 || y >= fb_height) return;
        if (fb_clip_enabled && (x < fb_clip_left || x >= fb_clip_right || y < fb_clip_top || y >= fb_clip_bottom)) return;
        fb_shadow[y * fb_width + x] = color & 0x0F;
        fb_surface[y * fb_width + x] = vga_to_rgb888[color & 0x0F];
        fb_mark_dirty(x, y, 1, 1);
    } else {
        VgaPutPixel(x, y, color);
    }
}

void FbFillRect(int x, int y, int w, int h, uint8_t color) {
    if (use_framebuffer) {
        if (!fb_shadow || !fb_surface) return;
        if (fb_clip_enabled) {
            if (x < fb_clip_left) { w -= fb_clip_left - x; x = fb_clip_left; }
            if (y < fb_clip_top) { h -= fb_clip_top - y; y = fb_clip_top; }
            if (x + w > fb_clip_right) w = fb_clip_right - x;
            if (y + h > fb_clip_bottom) h = fb_clip_bottom - y;
        }
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (x + w > fb_width) w = fb_width - x;
        if (y + h > fb_height) h = fb_height - y;
        if (w <= 0 || h <= 0) return;

        for (int row = y; row < y + h; row++) {
            uint8_t *dst = fb_shadow + (row * fb_width) + x;
            uint32_t *surface = fb_surface + (row * fb_width) + x;
            for (int col = 0; col < w; col++) {
                dst[col] = color & 0x0F;
                surface[col] = vga_to_rgb888[color & 0x0F];
            }
        }
        fb_mark_dirty(x, y, w, h);
    } else {
        VgaFillRect(x, y, w, h, color);
    }
}

void FbFillRectRGB(int x, int y, int w, int h, uint32_t rgb) {
    if (!use_framebuffer || !fb_surface) { FbFillRect(x, y, w, h, 0); return; }
    if (fb_clip_enabled) {
        if (x < fb_clip_left) { w -= fb_clip_left - x; x = fb_clip_left; }
        if (y < fb_clip_top) { h -= fb_clip_top - y; y = fb_clip_top; }
        if (x + w > fb_clip_right) w = fb_clip_right - x;
        if (y + h > fb_clip_bottom) h = fb_clip_bottom - y;
    }
    if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
    if (x + w > fb_width) w = fb_width - x; if (y + h > fb_height) h = fb_height - y;
    if (w <= 0 || h <= 0) return;
    {
        uint8_t palette = fb_rgb_to_index(rgb);
    for (int yy = y; yy < y + h; yy++) {
        uint8_t *indexed = fb_shadow + yy * fb_width + x;
        uint32_t *surface = fb_surface + yy * fb_width + x;
        for (int xx = 0; xx < w; xx++) { surface[xx] = rgb; indexed[xx] = palette; }
    }
    }
    fb_mark_dirty(x, y, w, h);
}

void FbSetClipRect(int x, int y, int w, int h) {
    fb_clip_enabled = 1;
    fb_clip_left = x < 0 ? 0 : x;
    fb_clip_top = y < 0 ? 0 : y;
    fb_clip_right = x + w > fb_width ? fb_width : x + w;
    fb_clip_bottom = y + h > fb_height ? fb_height : y + h;
    if (fb_clip_right < fb_clip_left) fb_clip_right = fb_clip_left;
    if (fb_clip_bottom < fb_clip_top) fb_clip_bottom = fb_clip_top;
}

void FbResetClipRect(void) { fb_clip_enabled = 0; }

void FbDrawRect(int x, int y, int w, int h, uint8_t color) {
    FbFillRect(x, y, w, 1, color);
    FbFillRect(x, y + h - 1, w, 1, color);
    FbFillRect(x, y, 1, h, color);
    FbFillRect(x + w - 1, y, 1, h, color);
}

void FbDrawChar(int x, int y, char c, uint8_t fg, uint8_t bg) {
    if (use_framebuffer) {
        uint8_t ttf_glyph[12];
        const uint8_t *glyph = fb_get_font(c);
        int glyph_height = 8;
        if (fb_use_ttf_glyphs && FbTtfReady() && FbTtfGlyph(c, ttf_glyph)) {
            glyph = ttf_glyph;
            glyph_height = 12;
        }
        for (int row = 0; row < glyph_height; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++) {
                FbPutPixel(x + col, y + row, (bits & (0x80 >> col)) ? fg : bg);
            }
        }
    } else {
        VgaDrawChar(x, y, c, fg, bg);
    }
}

void FbDrawCharTransparent(int x, int y, char c, uint8_t fg) {
    if (!use_framebuffer) { VgaDrawChar(x, y, c, fg, COLOR_BLUE); return; }
    {
        uint8_t ttf_glyph[12];
        const uint8_t *glyph = fb_get_font(c);
        int glyph_height = 8;
        if (fb_use_ttf_glyphs && FbTtfReady() && FbTtfGlyph(c, ttf_glyph)) {
            glyph = ttf_glyph;
            glyph_height = 12;
        }
        for (int row = 0; row < glyph_height; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < 8; col++)
                if (bits & (0x80 >> col)) FbPutPixel(x + col, y + row, fg);
        }
    }
}

void FbDrawStringTransparent(int x, int y, const char *str, uint8_t fg) {
    int cx = x;
    while (str && *str) {
        if (*str == '\n') { cx = x; y += 10; }
        else { FbDrawCharTransparent(cx, y, *str, fg); cx += 8; }
        str++;
    }
}

void FbDrawString(int x, int y, const char *str, uint8_t fg, uint8_t bg) {
    if (use_framebuffer) {
        int cx = x;
        int cy = y;
        while (*str) {
            if (*str == '\n') {
                cx = x;
                cy += (fb_use_ttf_glyphs && FbTtfReady() ? 14 : 10);
            } else {
                FbDrawChar(cx, cy, *str, fg, bg);
                cx += 8;
                if (cx + 8 > fb_width) {
                    cx = x;
                    cy += (fb_use_ttf_glyphs && FbTtfReady() ? 14 : 10);
                }
            }
            str++;
            if (cy + (fb_use_ttf_glyphs && FbTtfReady() ? 12 : 8) > fb_height) break;
        }
    } else {
        VgaDrawString(x, y, str, fg, bg);
    }
}

void FbSwapBuffers(void) {
    if (use_framebuffer) {
        if (!fb_surface || !dirty_valid) return;

        int update_x = dirty_x1;
        int update_y = dirty_y1;
        int update_w = dirty_x2 - dirty_x1 + 1;
        int update_h = dirty_y2 - dirty_y1 + 1;

        if (bga_page_flip) {
            int target_page;
            uint8_t *target;
            uint8_t *old_page;

            if (!fb_present_addr) fb_present_addr = fb_addr;
            target_page = bga_display_page ^ 1;
            target = fb_addr + (uint32_t)target_page * fb_height * fb_pitch;
            old_page = fb_addr + (uint32_t)bga_display_page * fb_height * fb_pitch;
            fb_present_addr = target;

            if (!bga_pages_initialized && fb_bpp == 32) {
                for (int y = 0; y < fb_height; y++) {
                    memcpy(fb_addr + y * fb_pitch,
                           fb_surface + y * fb_width,
                           (uint32_t)fb_width * 4U);
                    memcpy(fb_addr + (uint32_t)fb_height * fb_pitch + y * fb_pitch,
                           fb_surface + y * fb_width,
                           (uint32_t)fb_width * 4U);
                }
                bga_pages_initialized = 1;
            } else if (!bga_pages_initialized) {
                for (int page = 0; page < 2; page++) {
                    fb_present_addr = fb_addr + (uint32_t)page * fb_height * fb_pitch;
                    for (int y = 0; y < fb_height; y++)
                        for (int x = 0; x < fb_width; x++)
                            fb_write_hw_rgb(x, y, fb_surface[y * fb_width + x]);
                }
                fb_present_addr = target;
                bga_pages_initialized = 1;
            }

            if (fb_bpp == 32) {
                for (int y = dirty_y1; y <= dirty_y2; y++) {
                    memcpy(target + y * fb_pitch + dirty_x1 * 4,
                           fb_surface + y * fb_width + dirty_x1,
                           (uint32_t)update_w * 4U);
                }
            } else {
                for (int y = dirty_y1; y <= dirty_y2; y++) {
                    for (int x = dirty_x1; x <= dirty_x2; x++)
                        fb_write_hw_rgb(x, y, fb_surface[y * fb_width + x]);
                }
            }
            bga_write(BGA_INDEX_X_OFFSET, 0);
            bga_write(BGA_INDEX_Y_OFFSET, target_page * fb_height);
            bga_display_page = target_page;

            /* The former display page is hidden now. Bring it up to date
               after the flip so cursor/background changes cannot leave a
               stale cursor trail on the next alternating frame. */
            if (fb_bpp == 32) {
                for (int y = dirty_y1; y <= dirty_y2; y++) {
                    memcpy(old_page + y * fb_pitch + dirty_x1 * 4,
                           fb_surface + y * fb_width + dirty_x1,
                           (uint32_t)update_w * 4U);
                }
            } else {
                fb_present_addr = old_page;
                for (int y = dirty_y1; y <= dirty_y2; y++) {
                    for (int x = dirty_x1; x <= dirty_x2; x++)
                        fb_write_hw_rgb(x, y, fb_surface[y * fb_width + x]);
                }
                fb_present_addr = target;
            }
        } else if (fb_bpp == 32) {
            /* The compositor surface and the native framebuffer have the
               same packed RGB layout. Copy complete scanlines so a drag
               does not spend time touching every pixel through a helper. */
            for (int y = dirty_y1; y <= dirty_y2; y++) {
                memcpy(fb_addr + y * fb_pitch + dirty_x1 * 4,
                       fb_surface + y * fb_width + dirty_x1,
                       (uint32_t)update_w * 4U);
            }
        } else {
            for (int y = dirty_y1; y <= dirty_y2; y++) {
                for (int x = dirty_x1; x <= dirty_x2; x++)
                    fb_write_hw_rgb(x, y, fb_surface[y * fb_width + x]);
            }
        }

        svga_update(update_x, update_y, update_w, update_h);

        fb_reset_dirty();
    } else {
        VgaSwapBuffers();
    }
}

void FbCapture(uint8_t *dst, int dst_stride) {
    uint8_t *src = fb_indexed_buffer();
    int stride = fb_indexed_stride();
    int width = use_framebuffer ? fb_width : 640;
    int height = use_framebuffer ? fb_height : 480;

    if (!dst || !src || dst_stride < width) return;

    for (int y = 0; y < height; y++) {
        memcpy(dst + (y * dst_stride), src + (y * stride), (uint32_t)width);
    }
}

void FbBlitIndexed(int x, int y, int w, int h, const uint8_t *src, int src_stride) {
    uint8_t *dst = fb_indexed_buffer();
    int stride = fb_indexed_stride();
    int width = use_framebuffer ? fb_width : 640;
    int height = use_framebuffer ? fb_height : 480;

    if (!dst || !src) return;
    if (x < 0) { src -= x; w += x; x = 0; }
    if (y < 0) { src -= y * src_stride; h += y; y = 0; }
    if (x >= width || y >= height || w <= 0 || h <= 0) return;
    if (x + w > width) w = width - x;
    if (y + h > height) h = height - y;
    if (w <= 0 || h <= 0) return;

    for (int row = 0; row < h; row++) {
        memcpy(dst + ((y + row) * stride) + x,
               src + (row * src_stride),
               (uint32_t)w);
    }

    if (use_framebuffer) fb_mark_dirty(x, y, w, h);
}
