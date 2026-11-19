#include "bsx.h"

Image *bx_image_new(int w, int h) {
    Image *im = bx_alloc(sizeof *im);
    im->w = w > 0 ? w : 1;
    im->h = h > 0 ? h : 1;
    im->px = bx_alloc(sizeof(u32) * (size_t)im->w * (size_t)im->h);
    return im;
}

void bx_image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

typedef struct Part {
    int w, h, x, y;
    int mode, comp;
    u32 unpacked, dataOff, dataSize, palOff;
    size_t base;
} Part;

static bool part_header(const u8 *d, size_t n, size_t base, Part *p) {
    if (base + 0x40 > n || memcmp(d + base, "BSS-Graphics", 12)) return false;
    const u8 *h = d + base;
    p->unpacked = rd32(h + 0x12);
    p->w = rd16(h + 0x16);
    p->h = rd16(h + 0x18);
    p->x = (i16)rd16(h + 0x20);
    p->y = (i16)rd16(h + 0x22);
    p->mode = h[0x30];
    p->comp = h[0x31];
    p->dataOff = rd32(h + 0x32);
    p->dataSize = rd32(h + 0x36);
    p->palOff = rd32(h + 0x3a);
    p->base = base;
    if (p->mode > 2 || p->comp > 2 || !p->w || !p->h) return false;
    size_t need = p->mode == 2 ? (size_t)p->w * p->h : (size_t)p->w * p->h * 4;
    return p->unpacked >= need && base + p->dataOff <= n;
}

static const u8 *unpack_rle(const u8 *s, const u8 *end, u8 *out, size_t outLen, size_t dst, int step) {
    if (s + 4 > end) return end;
    i32 remaining = (i32)rd32(s);
    s += 4;
    while (remaining > 0 && s < end) {
        int count = (signed char)*s++;
        remaining--;
        if (count >= 0) {
            for (int i = 0; i <= count && s < end; i++) {
                if (dst < outLen) out[dst] = *s;
                s++;
                remaining--;
                dst += (size_t)step;
            }
        } else {
            count = 1 - count;
            if (s >= end) break;
            u8 v = *s++;
            remaining--;
            for (int i = 0; i < count; i++) {
                if (dst < outLen) out[dst] = v;
                dst += (size_t)step;
            }
        }
    }
    return s;
}

static const u8 *unpack_lz(const u8 *s, const u8 *end, u8 *out, size_t outLen, size_t plane, int step) {
    if (s + 5 > end) return end;
    u8 control = s[0];
    i32 remaining = (i32)rd32(s + 1) - 5;
    s += 5;
    size_t dst = plane;
    while (remaining > 0 && s < end) {
        u8 c = *s++;
        remaining--;
        if (c == control && s < end) {
            int offset = *s++;
            remaining--;
            if (offset != control && s < end) {
                int count = *s++;
                remaining--;
                if (offset > control) offset--;
                size_t back = (size_t)offset * (size_t)step;
                while (count-- > 0) {
                    if (dst < outLen && dst >= back) out[dst] = out[dst - back];
                    dst += (size_t)step;
                }
                continue;
            }
        }
        if (dst < outLen) out[dst] = c;
        dst += (size_t)step;
    }
    for (size_t i = plane + (size_t)step; i < outLen; i += (size_t)step) out[i] = (u8)(out[i] + out[i - (size_t)step]);
    return s;
}

static Image *decode_part(const u8 *d, size_t n, const Part *p) {
    size_t outLen = p->unpacked;
    u8 *out = bx_alloc(outLen);
    const u8 *s = d + p->base + p->dataOff;
    const u8 *end = d + n;
    if (p->comp == 0) {
        size_t avail = (size_t)(end - s) < p->dataSize ? (size_t)(end - s) : p->dataSize;
        if (p->mode == 1) {
            size_t px = avail / 3;
            for (size_t i = 0; i < px && i * 4 + 3 <= outLen; i++) memcpy(out + i * 4, s + i * 3, 3);
        } else memcpy(out, s, avail < outLen ? avail : outLen);
    } else {
        int planes = p->mode == 0 ? 4 : p->mode == 1 ? 3 : 1;
        int step = p->mode == 2 ? 1 : 4;
        for (int c = 0; c < planes; c++) {
            if (p->comp == 1) s = unpack_rle(s, end, out, outLen, (size_t)c, step);
            else s = unpack_lz(s, end, out, outLen, (size_t)c, step);
        }
    }
    Image *im = bx_image_new(p->w, p->h);
    im->x = p->x;
    im->y = p->y;
    const u8 *pal = d + p->base + p->palOff;
    bool palOk = p->mode == 2 && p->palOff && p->base + p->palOff + 1024 <= n;
    for (int y = 0; y < p->h; y++) {
        u32 *row = im->px + (size_t)(p->h - 1 - y) * (size_t)p->w;
        for (int x = 0; x < p->w; x++) {
            u8 bl, g, r, a;
            if (p->mode == 2) {
                u8 i = out[(size_t)y * p->w + x];
                bl = palOk ? pal[i * 4] : i;
                g = palOk ? pal[i * 4 + 1] : i;
                r = palOk ? pal[i * 4 + 2] : i;
                a = 255;
            } else {
                const u8 *q = out + ((size_t)y * p->w + x) * 4;
                bl = q[0];
                g = q[1];
                r = q[2];
                a = p->mode == 0 ? q[3] : 255;
            }
            row[x] = (u32)r | (u32)g << 8 | (u32)bl << 16 | (u32)a << 24;
        }
    }
    free(out);
    return im;
}

static void over(Image *dst, const Image *src) {
    for (int y = 0; y < src->h; y++) {
        int dy = src->y - dst->y + y;
        if (dy < 0 || dy >= dst->h) continue;
        for (int x = 0; x < src->w; x++) {
            int dx = src->x - dst->x + x;
            if (dx < 0 || dx >= dst->w) continue;
            u32 s = src->px[(size_t)y * src->w + x];
            u32 a = s >> 24;
            if (!a) continue;
            u32 *dp = &dst->px[(size_t)dy * dst->w + dx];
            if (a == 255) {
                *dp = s;
                continue;
            }
            u32 d = *dp, da = d >> 24;
            u32 oa = a + da * (255 - a) / 255;
            if (!oa) continue;
            u32 out = oa << 24;
            for (int k = 0; k < 24; k += 8) {
                u32 sc = (s >> k) & 0xff, dc = (d >> k) & 0xff;
                u32 c = (sc * a + dc * da * (255 - a) / 255) / oa;
                out |= (c > 255 ? 255 : c) << k;
            }
            *dp = out;
        }
    }
}

Image *bx_bsg_part(const u8 *d, size_t n, int index) {
    if (n < 0x20 || memcmp(d, "BSS-Composition", 15)) return index == 0 ? bx_bsg_decode(d, n) : NULL;
    size_t cur = 0x20;
    for (int i = 0; i < d[0x11]; i++) {
        Part p;
        if (!part_header(d, n, cur, &p)) return NULL;
        if (i == index) return decode_part(d, n, &p);
        cur += 0x40 + p.dataSize;
    }
    return NULL;
}

Image *bx_bsg_decode(const u8 *d, size_t n) {
    if (n >= 0x20 && !memcmp(d, "BSS-Composition", 15)) {
        int count = d[0x11];
        Part parts[64];
        int pc = 0;
        size_t cur = 0x20;
        for (int i = 0; i < count && pc < 64; i++) {
            if (!part_header(d, n, cur, &parts[pc])) break;
            cur += 0x40 + parts[pc].dataSize;
            pc++;
        }
        if (!pc) return NULL;
        int x0 = parts[0].x, y0 = parts[0].y, x1 = x0 + parts[0].w, y1 = y0 + parts[0].h;
        for (int i = 1; i < pc; i++) {
            if (parts[i].x < x0) x0 = parts[i].x;
            if (parts[i].y < y0) y0 = parts[i].y;
            if (parts[i].x + parts[i].w > x1) x1 = parts[i].x + parts[i].w;
            if (parts[i].y + parts[i].h > y1) y1 = parts[i].y + parts[i].h;
        }
        if (pc == 1) return decode_part(d, n, &parts[0]);
        Image *im = bx_image_new(x1 - x0, y1 - y0);
        im->x = x0;
        im->y = y0;
        for (int i = 0; i < pc; i++) {
            Image *p = decode_part(d, n, &parts[i]);
            over(im, p);
            bx_image_free(p);
        }
        return im;
    }
    Part p;
    if (!part_header(d, n, 0, &p)) return NULL;
    return decode_part(d, n, &p);
}

Image *bx_image_load(BX *b, const char *name) {
    for (int i = 0; i < 32; i++)
        if (b->cache[i].key && bx_ieq(b->cache[i].key, name)) {
            b->cache[i].used = ++b->cacheClock;
            return b->cache[i].img;
        }
    size_t n;
    char base[128];
    snprintf(base, sizeof base, "%s", name);
    char *hash = strchr(base, '#');
    int part = -1;
    if (hash) {
        part = atoi(hash + 1);
        *hash = 0;
    }
    u8 *d = bx_res_read(&b->arc, RX_GFX, base, &n);
    Image *im = NULL;
    if (d) {
        im = part >= 0 ? bx_bsg_part(d, n, part) : bx_bsg_decode(d, n);
        free(d);
    } else if (bx_ieq(name, "black") || bx_ieq(name, "white")) {
        im = bx_image_new(b->W, b->H);
        u32 c = bx_ieq(name, "white") ? 0xffffffffu : 0xff000000u;
        for (size_t i = 0; i < (size_t)b->W * b->H; i++) im->px[i] = c;
    } else bx_log(b, "missing picture %s", name);
    if (!im && d) bx_log(b, "can't decode picture %s", name);
    int slot = 0;
    for (int i = 1; i < 32; i++)
        if (b->cache[i].used < b->cache[slot].used) slot = i;
    free(b->cache[slot].key);
    bx_image_free(b->cache[slot].img);
    b->cache[slot].key = bx_strdup(name);
    b->cache[slot].img = im;
    b->cache[slot].used = ++b->cacheClock;
    return im;
}

void bx_image_cache_clear(BX *b) {
    for (int i = 0; i < 32; i++) {
        free(b->cache[i].key);
        bx_image_free(b->cache[i].img);
        b->cache[i].key = NULL;
        b->cache[i].img = NULL;
    }
}
