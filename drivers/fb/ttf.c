#include <stdint.h>
#include "ttf.h"
#include "cdfs.h"
#include "mm/mm.h"

typedef struct {
    uint8_t *data;
    uint32_t size;
    uint32_t cmap, glyf, loca, head, hhea, hmtx;
    uint16_t glyph_count;
    uint16_t hmetrics;
    int16_t loca_format;
    uint16_t units_per_em;
    int16_t ascender, descender;
    int ready;
} TTF;

static TTF ttf;

/* 8x12 cell, 4x4 subsamples per pixel */
static uint16_t sub_cover[8][12];

static int popcount16(uint16_t v) {
    int n = 0;
    while (v) { v &= (uint16_t)(v - 1); n++; }
    return n;
}

static uint16_t u16(const uint8_t *p) { return ((uint16_t)p[0] << 8) | p[1]; }
static int16_t s16(const uint8_t *p) { return (int16_t)u16(p); }
static uint32_t u32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static int range_ok(uint32_t off, uint32_t len) {
    return off <= ttf.size && len <= ttf.size - off;
}

static uint32_t table(uint32_t tag) {
    uint16_t count, i;
    if (!ttf.data || ttf.size < 12) return 0;
    count = u16(ttf.data + 4);
    if (!range_ok(12, (uint32_t)count * 16U)) return 0;
    for (i = 0; i < count; i++) {
        uint8_t *r = ttf.data + 12 + i * 16;
        uint32_t off, len;
        if (u32(r) != tag) continue;
        off = u32(r + 8); len = u32(r + 12);
        return range_ok(off, len) ? off : 0;
    }
    return 0;
}

/* Prefer Windows Unicode BMP cmap (platform 3, encoding 1/0), then Unicode (platform 0). */
static uint16_t glyph_for(uint8_t ch) {
    uint8_t *base;
    uint16_t records, i;
    int pass;
    if (!ttf.cmap) return 0;
    base = ttf.data + ttf.cmap;
    if (!range_ok(ttf.cmap, 4)) return 0;
    records = u16(base + 2);
    if (!range_ok(ttf.cmap + 4, (uint32_t)records * 8U)) return 0;

    /* Windows Unicode BMP (3/1) must win over Symbol (3/0).  Both are
     * commonly present in older Windows fonts, but a Symbol cmap assigns
     * ordinary byte values to private glyphs.  Selecting by table order was
     * the reason menu text could have plausible outlines for the wrong
     * letters. */
    for (pass = 0; pass < 3; pass++) {
      for (i = 0; i < records; i++) {
        uint8_t *record = base + 4 + i * 8;
        uint16_t platform = u16(record);
        uint16_t encoding = u16(record + 2);
        uint32_t off = u32(record + 4);
        uint8_t *sub;
        uint16_t seg_count, j;

        if (!((pass == 0 && platform == 3 && encoding == 1) ||
              (pass == 1 && platform == 3 && encoding == 0) ||
              (pass == 2 && platform == 0))) {
            continue;
        }

        if (off > ttf.size - ttf.cmap || !range_ok(ttf.cmap + off, 16)) continue;
        sub = base + off;
        if (u16(sub) != 4) continue;

        seg_count = u16(sub + 6) / 2;
        if (!seg_count ||
            (uint32_t)16 + seg_count * 8U > ttf.size - (ttf.cmap + off)) continue;

        for (j = 0; j < seg_count; j++) {
            uint8_t *ends   = sub + 14;
            uint8_t *starts = sub + 16 + seg_count * 2;
            uint8_t *deltas = starts + seg_count * 2;
            uint8_t *ranges = deltas + seg_count * 2;
            uint16_t start  = u16(starts + j * 2);
            uint16_t end    = u16(ends   + j * 2);
            int32_t g;

            if (ch < start || ch > end) continue;

            if (!u16(ranges + j * 2)) {
                g = ch + s16(deltas + j * 2);
            } else {
                uint32_t at = (uint32_t)(ranges + j * 2 - ttf.data) +
                              u16(ranges + j * 2) + (ch - start) * 2U;
                if (!range_ok(at, 2)) return 0;
                g = u16(ttf.data + at);
                if (g) g += s16(deltas + j * 2);
            }
            return (g >= 0 && g < ttf.glyph_count) ? (uint16_t)g : 0;
        }
      }
    }
    return 0;
}

/* loca has numGlyphs + 1 entries; glyph_count == numGlyphs. */
static uint32_t glyph_offset(uint16_t glyph) {
    uint32_t at;
    if (!ttf.loca || glyph > ttf.glyph_count) return 0;
    at = ttf.loca + (ttf.loca_format ? glyph * 4U : glyph * 2U);
    if (!range_ok(at, ttf.loca_format ? 4 : 2)) return 0;
    {
        uint32_t off = ttf.loca_format ? u32(ttf.data + at) : u16(ttf.data + at) * 2U;
        /* loca offsets are relative to glyf, and must be checked before the
         * caller adds the table base.  A corrupt font must not wrap that
         * addition into an apparently valid pointer. */
        return off <= ttf.size - ttf.glyf ? off : 0;
    }
}

static int add_point(int32_t *x, int32_t *y, int *count, int32_t px, int32_t py) {
    if (*count >= 1024) return 0;
    x[*count] = px; y[*count] = py; (*count)++;
    return 1;
}

static int flatten(int32_t *sx, int32_t *sy, uint8_t *flags, int begin, int end,
                   int32_t *dx, int32_t *dy) {
    int n = end - begin + 1, i, out = 0;
    int32_t cx, cy;
    if (n < 1) return 0;
    if (flags[begin] & 1) { cx = sx[begin]; cy = sy[begin]; }
    else if (flags[end] & 1) { cx = sx[end]; cy = sy[end]; }
    else { cx = (sx[begin] + sx[end]) / 2; cy = (sy[begin] + sy[end]) / 2; }
    add_point(dx, dy, &out, cx, cy);
    for (i = 0; i < n; i++) {
        int at = begin + i, next = (i + 1 < n) ? at + 1 : begin;
        int next2 = (i + 2 < n) ? at + 2 : begin + (i + 2 - n);
        int32_t ex, ey;
        if (flags[at] & 1) {
            add_point(dx, dy, &out, sx[at], sy[at]); cx = sx[at]; cy = sy[at]; continue;
        }
        if (flags[next] & 1) { ex = sx[next]; ey = sy[next]; }
        else { ex = (sx[next] + sx[next2]) / 2; ey = (sy[next] + sy[next2]) / 2; }
        {
            int step;
            for (step = 1; step <= 4; step++) {
                int32_t t = step * 256 / 4, a = 256 - t;
                int32_t qx = (a*a*cx + 2*a*t*sx[at] + t*t*ex) / 65536;
                int32_t qy = (a*a*cy + 2*a*t*sy[at] + t*t*ey) / 65536;
                add_point(dx, dy, &out, qx, qy);
            }
        }
        cx = ex; cy = ey;
    }
    return out;
}

static int inside(int32_t sx, int32_t sy, int32_t *x, int32_t *y, int n) {
    int i, j, result = 0;
    for (i = 0, j = n - 1; i < n; j = i++) {
        if (((y[i] > sy) != (y[j] > sy)) &&
            sx < (x[j] - x[i]) * (sy - y[i]) / (y[j] - y[i]) + x[i]) result = !result;
    }
    return result;
}

/* Render a simple glyph outline into sub_cover, given its glyph index. */
static int render_simple_outline(uint16_t glyph) {
    uint32_t off = glyph_offset(glyph), next = glyph_offset(glyph + 1);
    uint8_t *p, *limit;
    int16_t contours;
    int32_t x[256], y[256], px[1024], py[1024];
    uint16_t ends[64], points, i, c;
    uint8_t flags[256];
    int32_t ymin, ymax;
    int32_t advance;
    int sx, sy;

    if (!ttf.glyf) return 0;
    if (next <= off || next > ttf.size - ttf.glyf) return 0;

    p     = ttf.data + ttf.glyf + off;
    limit = ttf.data + ttf.glyf + next;

    if (p + 10 > limit) return 0;
    contours = s16(p);
    if (contours <= 0 || contours > 64) return 0; /* simple only here */
    ymin = s16(p + 4);
    ymax = s16(p + 8);
    p += 10;

    if (p + contours * 2 + 2 > limit) return 0;
    for (c = 0; c < (uint16_t)contours; c++) ends[c] = u16(p + c * 2);
    points = ends[contours - 1] + 1;
    if (!points || points > 256) return 0;
    p += contours * 2;

    {
        uint16_t il = u16(p);
        p += 2;
        if (p + il > limit) return 0;
        p += il;
    }

    for (i = 0; i < points; i++) {
        if (p >= limit) return 0;
        flags[i] = *p++;
        if (flags[i] & 8) {
            uint8_t n = *p++;
            while (n-- && i + 1 < points) flags[++i] = flags[i - 1];
        }
    }

    x[0] = y[0] = 0;
    for (i = 0; i < points; i++) {
        int16_t d = 0;
        if (flags[i] & 2)
            d = (flags[i] & 16) ? *p++ : -(int16_t)*p++;
        else if (!(flags[i] & 16)) {
            if (p + 2 > limit) return 0;
            d = s16(p);
            p += 2;
        }
        x[i] = (i ? x[i - 1] : 0) + d;
    }

    for (i = 0; i < points; i++) {
        int16_t d = 0;
        if (flags[i] & 4)
            d = (flags[i] & 32) ? *p++ : -(int16_t)*p++;
        else if (!(flags[i] & 32)) {
            if (p + 2 > limit) return 0;
            d = s16(p);
            p += 2;
        }
        y[i] = (i ? y[i - 1] : 0) + d;
    }

    /* Normalize to font em box using units_per_em and ascender/descender. */
    int32_t em = ttf.units_per_em ? ttf.units_per_em : 2048;
    int32_t top    = ttf.ascender;
    int32_t bottom = ttf.descender;
    if (top <= bottom) { top = ymax; bottom = ymin; }

    /* hmtx advanceWidth is the glyph's design-space cell width.  Resolve it
     * once per glyph; doing this inside the 16x supersampling loop made it
     * unnecessarily easy for a malformed metric table to produce mixed
     * horizontal scales within one glyph. */
    advance = em;
    if (ttf.hmtx && ttf.hmetrics) {
        uint16_t metric = glyph < ttf.hmetrics ? glyph : ttf.hmetrics - 1;
        uint32_t metric_at = ttf.hmtx + (uint32_t)metric * 4U;
        if (range_ok(metric_at, 2) && u16(ttf.data + metric_at))
            advance = u16(ttf.data + metric_at);
    }

    for (c = 0, i = 0; c < (uint16_t)contours; c++) {
        uint16_t end = ends[c], begin = i;
        int n = flatten(x, y, flags, begin, end, px, py);
        int a, b;

        if (n < 3) { i = end + 1; continue; }

        for (sx = 0; sx < 8; sx++) {
            for (sy = 0; sy < 12; sy++) {
                uint16_t inside_mask = 0;
                for (a = 0; a < 4; a++) {
                    for (b = 0; b < 4; b++) {
                        int32_t cell_x = ((sx * 4 + b) * em) / (8 * 4);
                        int32_t cell_y = ((sy * 4 + a) * em) / (12 * 4);

                        /* Scale from the font coordinate system, not from
                         * this glyph's bbox. Stretching each bbox to the
                         * entire cell turns narrow glyphs (i, l, punctuation)
                         * into solid bars and destroys side bearings. */
                        int32_t xx = (cell_x * advance) / em;
                        int32_t yy = top - (cell_y * (top - bottom)) / em;

                        if (inside(xx, yy, px, py, n))
                            inside_mask |= (uint16_t)(1u << (a * 4 + b));
                    }
                }
                sub_cover[sx][sy] ^= inside_mask;
            }
        }
        i = end + 1;
    }

    return 1;
}

/* Basic composite glyph support: translate components, ignore scaling for now. */
static int render_composite_outline(uint16_t glyph) {
    uint32_t off = glyph_offset(glyph), next = glyph_offset(glyph + 1);
    uint8_t *p, *limit;
    int16_t contours;

    if (!ttf.glyf) return 0;
    if (!next || next <= off) return 0;
    if (ttf.glyf + next > ttf.size) return 0;

    p     = ttf.data + ttf.glyf + off;
    limit = ttf.data + ttf.glyf + next;

    if (p + 10 > limit) return 0;
    contours = s16(p);
    if (contours >= 0) return 0; /* not composite */

    p += 10;

    /* Composite glyph flags */
    enum {
        ARG_1_AND_2_ARE_WORDS = 1,
        ARGS_ARE_XY_VALUES    = 2,
        WE_HAVE_A_SCALE       = 8,
        MORE_COMPONENTS       = 32,
        WE_HAVE_AN_XY_SCALE   = 64,
        WE_HAVE_A_TWO_BY_TWO  = 128
    };

    while (1) {
        if (p + 4 > limit) return 0;
        uint16_t flags = u16(p); p += 2;
        uint16_t comp_glyph = u16(p); p += 2;

        int16_t arg1 = 0, arg2 = 0;
        if (flags & ARG_1_AND_2_ARE_WORDS) {
            if (p + 4 > limit) return 0;
            arg1 = s16(p); p += 2;
            arg2 = s16(p); p += 2;
        } else {
            if (p + 2 > limit) return 0;
            arg1 = (int8_t)p[0];
            arg2 = (int8_t)p[1];
            p += 2;
        }

        int32_t dx = 0, dy = 0;
        int32_t m00 = 16384, m01 = 0, m10 = 0, m11 = 16384;
        if (flags & ARGS_ARE_XY_VALUES) {
            dx = arg1;
            dy = arg2;
        }

        /* Component matrices are signed 2.14 fixed-point values.  The old
         * code skipped these bytes entirely, which made composite glyphs
         * such as accented letters and many capitals collapse or overlap.
         * This follows the TrueType composite convention used by FreeType
         * and stb_truetype, while retaining our tiny bitmap rasterizer. */
        if (flags & WE_HAVE_A_SCALE) {
            if (p + 2 > limit) return 0;
            m00 = m11 = s16(p); p += 2;
        } else if (flags & WE_HAVE_AN_XY_SCALE) {
            if (p + 4 > limit) return 0;
            m00 = s16(p); m11 = s16(p + 2); p += 4;
        } else if (flags & WE_HAVE_A_TWO_BY_TWO) {
            if (p + 8 > limit) return 0;
            m00 = s16(p); m01 = s16(p + 2);
            m10 = s16(p + 4); m11 = s16(p + 6); p += 8;
        }

        /* For now, ignore scaling; just translate the component glyph. */
        uint16_t temp_cover[8][12];
        int sx, sy;

        for (sy = 0; sy < 12; sy++)
            for (sx = 0; sx < 8; sx++)
                temp_cover[sx][sy] = 0;

        /* Render component into temp_cover by temporarily swapping sub_cover. */
        uint16_t saved_cover[8][12];
        for (sy = 0; sy < 12; sy++)
            for (sx = 0; sx < 8; sx++)
                saved_cover[sx][sy] = sub_cover[sx][sy];

        for (sy = 0; sy < 12; sy++)
            for (sx = 0; sx < 8; sx++)
                sub_cover[sx][sy] = 0;

        render_simple_outline(comp_glyph);

        for (sy = 0; sy < 12; sy++)
            for (sx = 0; sx < 8; sx++)
                temp_cover[sx][sy] = sub_cover[sx][sy];

        for (sy = 0; sy < 12; sy++)
            for (sx = 0; sx < 8; sx++)
                sub_cover[sx][sy] = saved_cover[sx][sy];

        /* Transform the component bitmap around its center. TrueType uses a
         * Y-up coordinate system, while our rows are Y-down, hence the
         * negative Y translation. */
        {
            int32_t em = ttf.units_per_em ? ttf.units_per_em : 2048;
            int px_shift = (dx * 8) / em;
            int py_shift = -(dy * 12) / em;
            for (sy = 0; sy < 12; sy++) {
                for (sx = 0; sx < 8; sx++) {
                    int32_t cx = sx * 2 - 7;
                    int32_t cy = sy * 2 - 11;
                    int tsx = 4 + (int)((m00 * cx + m01 * cy) / 32768) + px_shift;
                    int tsy = 6 + (int)((m10 * cx + m11 * cy) / 32768) + py_shift;
                    if (tsx >= 0 && tsx < 8 && tsy >= 0 && tsy < 12)
                        sub_cover[tsx][tsy] ^= temp_cover[sx][sy];
                }
            }
        }

        if (!(flags & MORE_COMPONENTS)) break;

    }

    return 1;
}

/* High-level render: decide simple vs composite, then composite sub_cover into rows. */
static int render(uint16_t glyph, uint8_t out[12]) {
    int sx, sy;
    for (sy = 0; sy < 12; sy++)
        for (sx = 0; sx < 8; sx++)
            sub_cover[sx][sy] = 0;

    /* Peek contours to decide simple vs composite. */
    uint32_t off = glyph_offset(glyph), next = glyph_offset(glyph + 1);
    if (!ttf.glyf || !next || next <= off || ttf.glyf + next > ttf.size) return 0;
    uint8_t *p = ttf.data + ttf.glyf + off;
    if (p + 10 > ttf.data + ttf.glyf + next) return 0;
    int16_t contours = s16(p);

    if (contours >= 0) {
        if (!render_simple_outline(glyph)) return 0;
    } else {
        if (!render_composite_outline(glyph)) return 0;
    }

    for (sy = 0; sy < 12; sy++) {
        for (sx = 0; sx < 8; sx++)
            /* At this size a one-pixel stem can cover only a few of the 16
             * samples. Keep light coverage instead of turning letters into
             * dotted fragments. */
            if (popcount16(sub_cover[sx][sy]) >= 2)
                out[sy] |= (uint8_t)(0x80 >> sx);
    }
    return 1;
}

int FbTtfLoad(const char *path) {
#if defined(__loongarch64)
    (void)path;
    return 0;
#else
    uint8_t *data = 0;
    uint32_t size = 0;
    (void)path;
    if (ttf.ready || !CdfsReadFile("/SYSTEM32/FONTS/TAHOMA.TTF", &data, &size))
        return ttf.ready;

    ttf.data = data;
    ttf.size = size;
    ttf.head = table(0x68656164);
    ttf.cmap = table(0x636D6170);
    ttf.glyf = table(0x676C7966);
    ttf.loca = table(0x6C6F6361);
    ttf.hhea = table(0x68686561);
    ttf.hmtx = table(0x686D7478);

    {
        uint32_t maxp = table(0x6D617870);
        if (!ttf.head || !ttf.cmap || !ttf.glyf || !ttf.loca || !maxp || !ttf.hhea) {
            kfree(data);
            ttf.data = 0;
            return 0;
        }
        ttf.glyph_count = u16(data + maxp + 4); /* numGlyphs */
        ttf.units_per_em = u16(data + ttf.head + 18); /* unitsPerEm */
        ttf.ascender      = s16(data + ttf.hhea + 4); /* ascender */
        ttf.descender     = s16(data + ttf.hhea + 6); /* descender */
        ttf.hmetrics     = u16(data + ttf.hhea + 34);
    }

    ttf.loca_format = s16(data + ttf.head + 50); /* indexToLocFormat */
    ttf.ready = 1;
    return 1;
#endif
}

int FbTtfReady(void) { return ttf.ready; }

int FbTtfGlyph(char c, uint8_t rows[12]) {
    int i;
    if (!ttf.ready || c < 32 || c > 126) return 0;
    for (i = 0; i < 12; i++) rows[i] = 0;
    return render(glyph_for((uint8_t)c), rows);
}
