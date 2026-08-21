#include <stdint.h>
#include "ttf.h"
#include "cdfs.h"
#include "mm/mm.h"

typedef struct {
    uint8_t *data;
    uint32_t size, cmap, glyf, loca, head;
    uint16_t glyph_count;
    int16_t loca_format;
    int ready;
} TTF;

static TTF ttf;

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

static uint16_t glyph_for(uint8_t ch) {
    uint8_t *base;
    uint16_t records, i;
    if (!ttf.cmap) return 0;
    base = ttf.data + ttf.cmap;
    if (!range_ok(ttf.cmap, 4)) return 0;
    records = u16(base + 2);
    if (!range_ok(ttf.cmap + 4, (uint32_t)records * 8U)) return 0;

    for (i = 0; i < records; i++) {
        uint8_t *record = base + 4 + i * 8;
        uint32_t off = u32(record + 4);
        uint8_t *sub;
        uint16_t seg_count, j;
        if (off > ttf.size - ttf.cmap || !range_ok(ttf.cmap + off, 16)) continue;
        sub = base + off;
        if (u16(sub) != 4) continue;
        seg_count = u16(sub + 6) / 2;
        if (!seg_count || (uint32_t)16 + seg_count * 8U > ttf.size - (ttf.cmap + off)) continue;
        for (j = 0; j < seg_count; j++) {
            uint8_t *ends = sub + 14;
            uint8_t *starts = sub + 16 + seg_count * 2;
            uint8_t *deltas = starts + seg_count * 2;
            uint8_t *ranges = deltas + seg_count * 2;
            uint16_t start = u16(starts + j * 2);
            uint16_t end = u16(ends + j * 2);
            int32_t g;
            if (ch < start || ch > end) continue;
            if (!u16(ranges + j * 2)) g = ch + s16(deltas + j * 2);
            else {
                uint32_t at = (uint32_t)(ranges + j * 2 - ttf.data) +
                              u16(ranges + j * 2) + (ch - start) * 2U;
                if (!range_ok(at, 2)) return 0;
                g = u16(ttf.data + at);
                if (g) g += s16(deltas + j * 2);
            }
            return (g >= 0 && g < ttf.glyph_count) ? (uint16_t)g : 0;
        }
    }
    return 0;
}

static uint32_t glyph_offset(uint16_t glyph) {
    uint32_t at;
    if (!ttf.loca || glyph >= ttf.glyph_count) return 0;
    at = ttf.loca + (ttf.loca_format ? glyph * 4U : glyph * 2U);
    if (!range_ok(at, ttf.loca_format ? 4 : 2)) return 0;
    return ttf.loca_format ? u32(ttf.data + at) : u16(ttf.data + at) * 2U;
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

static int render(uint16_t glyph, uint8_t out[12]) {
    uint32_t off = glyph_offset(glyph), next = glyph_offset(glyph + 1);
    uint8_t *p, *limit;
    int16_t contours;
    int32_t x[256], y[256], px[1024], py[1024];
    uint16_t ends[64], points, i, c;
    uint8_t flags[256];
    int32_t xmin, ymin, xmax, ymax;
    if (!off || !next || next <= off || next > ttf.size - ttf.glyf) return 0;
    p = ttf.data + ttf.glyf + off; limit = ttf.data + ttf.glyf + next;
    if (p + 10 > limit || (contours = s16(p)) <= 0 || contours > 64) return 0;
    xmin=s16(p+2); ymin=s16(p+4); xmax=s16(p+6); ymax=s16(p+8); p += 10;
    if (p + contours * 2 + 2 > limit) return 0;
    for (c=0;c<(uint16_t)contours;c++) ends[c]=u16(p+c*2);
    points=ends[contours-1]+1; if (!points || points>256) return 0;
    p += contours*2; { uint16_t il=u16(p); p+=2; if (p+il>limit) return 0; p+=il; }
    for (i=0;i<points;i++) { if (p>=limit) return 0; flags[i]=*p++; if(flags[i]&8) { uint8_t n=*p++; while(n-- && i+1<points) flags[++i]=flags[i-1]; } }
    x[0]=y[0]=0;
    for(i=0;i<points;i++){int16_t d=0;if(flags[i]&2)d=(flags[i]&16)?*p++:-(int16_t)*p++;else if(!(flags[i]&16)){if(p+2>limit)return 0;d=s16(p);p+=2;}x[i]=(i?x[i-1]:0)+d;}
    for(i=0;i<points;i++){int16_t d=0;if(flags[i]&4)d=(flags[i]&32)?*p++:-(int16_t)*p++;else if(!(flags[i]&32)){if(p+2>limit)return 0;d=s16(p);p+=2;}y[i]=(i?y[i-1]:0)+d;}
    for(c=0,i=0;c<(uint16_t)contours;c++){uint16_t end=ends[c],begin=i;int n=flatten(x,y,flags,begin,end,px,py);int q;
        for(q=0;q<8*12;q++){int sx=q%8,sy=q/8, a,b;int covered=0;
            for(a=0;a<4;a++)for(b=0;b<4;b++){int32_t xx=xmin+(((sx*4+b)*2+1)*(xmax-xmin))/64;int32_t yy=ymax-(((sy*4+a)*2+1)*(ymax-ymin))/96;int z;if(inside(xx,yy,px,py,n))covered++;}
            if(covered>=6)out[sy]|=(uint8_t)(0x80>>sx);
        } i=end+1;
    }
    return 1;
}

int FbTtfLoad(const char *path) {
    uint8_t *data=0; uint32_t size=0;
    (void)path;
    if (ttf.ready || !CdfsReadFile("/SYSTEM32/FONTS/TAHOMA.TTF", &data, &size)) return ttf.ready;
    ttf.data=data;ttf.size=size;ttf.head=table(0x68656164);ttf.cmap=table(0x636D6170);ttf.glyf=table(0x676C7966);ttf.loca=table(0x6C6F6361);
    {uint32_t maxp=table(0x6D617870);if(!ttf.head||!ttf.cmap||!ttf.glyf||!ttf.loca||!maxp){kfree(data);ttf.data=0;return 0;}ttf.glyph_count=u16(data+maxp+4);}
    ttf.loca_format=s16(data+ttf.head+50);ttf.ready=1;return 1;
}

int FbTtfReady(void) { return ttf.ready; }
int FbTtfGlyph(char c, uint8_t rows[12]) { int i; if(!ttf.ready||c<32||c>126)return 0;for(i=0;i<12;i++)rows[i]=0;return render(glyph_for((uint8_t)c),rows); }
