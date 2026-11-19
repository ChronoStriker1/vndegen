#include "ex.h"

Image *image_new(int w, int h) {
    Image *im = ex_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = ex_alloc(sizeof(u32) * (size_t)w * (size_t)h);
    return im;
}

void image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

static size_t lzss(const u8 *src, size_t n, u8 *out, size_t outLen) {
    u8 win[4096] = {0};
    int wp = 0xfee;
    size_t i = 0, o = 0;
    while (i < n && o < outLen) {
        u8 c = src[i++];
        for (int b = 0; b < 8 && o < outLen; b++, c >>= 1) {
            if (i >= n) break;
            if (c & 1) {
                out[o++] = src[i];
                win[wp] = src[i++];
                wp = (wp + 1) & 0xfff;
            } else {
                if (i + 1 >= n) return o;
                int p = src[i] | (src[i + 1] & 0xf0) << 4, l = (src[i + 1] & 0xf) + 3;
                i += 2;
                for (int k = 0; k < l && o < outLen; k++) {
                    u8 v = win[(p + k) & 0xfff];
                    out[o++] = v;
                    win[wp] = v;
                    wp = (wp + 1) & 0xfff;
                }
            }
        }
    }
    return o;
}

typedef struct Bits {
    const u8 *p, *end;
    int buf, n;
} Bits;

static int bit(Bits *b) {
    if (!b->n) {
        b->buf = b->p < b->end ? *b->p++ : 0;
        b->n = 8;
    }
    b->n--;
    return (b->buf >> b->n) & 1;
}

static int byte(Bits *b) { return b->p < b->end ? *b->p++ : 0; }

static void gyu_lz(const u8 *src, size_t n, u8 *out, size_t outLen) {
    Bits b = {src + 4, src + n, 0, 0};
    size_t dst = 0;
    if (outLen) out[dst++] = (u8)byte(&b);
    while (dst < outLen) {
        if (bit(&b)) {
            out[dst++] = (u8)byte(&b);
            continue;
        }
        int cnt;
        long off;
        if (bit(&b)) {
            int hi = byte(&b), lo = byte(&b);
            cnt = hi << 8 | lo;
            off = (long)(cnt >> 3) - 8192;
            cnt &= 7;
            if (cnt) cnt++;
            else {
                cnt = byte(&b);
                if (!cnt) break;
            }
        } else {
            int a = bit(&b), c = bit(&b);
            cnt = 1 + (a << 1 | c);
            off = (long)byte(&b) - 256;
        }
        cnt++;
        for (int k = 0; k < cnt && dst < outLen; k++, dst++) {
            long from = (long)dst + off;
            out[dst] = from >= 0 ? out[from] : 0;
        }
    }
}

static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }

Image *gyu_decode(const u8 *d, size_t n) {
    if (n < 0x24 || memcmp(d, "GYU\x1a", 4)) return NULL;
    int flags = d[4] | d[5] << 8, mode = d[6] | d[7] << 8;
    u32 key = rd32(d + 8);
    int bpp = (int)rd32(d + 12), w = (int)rd32(d + 16), h = (int)rd32(d + 20);
    size_t ds = rd32(d + 24), als = rd32(d + 28), ps = rd32(d + 32);
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (bpp != 8 && bpp != 24 && bpp != 32)) return NULL;
    size_t o = 0x24;
    const u8 *pal = NULL;
    if (ps) {
        pal = d + o;
        o += ps * 4;
    }
    if (o + ds > n) return NULL;
    u8 *packed = ex_alloc(ds ? ds : 1);
    memcpy(packed, d + o, ds);
    o += ds;
    if (key != 0xffffffffu && ds) {
        MT m;
        mt_seed(&m, key);
        for (int i = 0; i < 10; i++) {
            u32 a = mt_next(&m) % (u32)ds, b = mt_next(&m) % (u32)ds;
            u8 t = packed[a];
            packed[a] = packed[b];
            packed[b] = t;
        }
    }
    size_t stride = ((size_t)w * (size_t)bpp / 8 + 3) & ~(size_t)3;
    u8 *raw = ex_alloc(stride * (size_t)h);
    if (mode == 0x100) memcpy(raw, packed, ds < stride * (size_t)h ? ds : stride * (size_t)h);
    else if (mode == 0x800) gyu_lz(packed, ds, raw, stride * (size_t)h);
    else lzss(packed, ds, raw, stride * (size_t)h);
    free(packed);
    Image *im = image_new(w, h);
    for (int y = 0; y < h; y++) {
        const u8 *row = raw + (size_t)(h - 1 - y) * stride;
        u32 *out = im->px + (size_t)y * (size_t)w;
        for (int x = 0; x < w; x++) {
            u32 r, g, b;
            if (bpp == 8) {
                const u8 *c = pal ? pal + row[x] * 4 : (const u8 *)"\0\0\0\0";
                b = c[0]; g = c[1]; r = c[2];
            } else {
                const u8 *c = row + (size_t)x * (size_t)(bpp / 8);
                b = c[0]; g = c[1]; r = c[2];
            }
            out[x] = r | g << 8 | b << 16 | 0xff000000u;
        }
    }
    free(raw);
    if (als && o + als <= n) {
        size_t ast = ((size_t)w + 3) & ~(size_t)3;
        u8 *a = ex_alloc(ast * (size_t)h);
        if (als == ast * (size_t)h) memcpy(a, d + o, als);
        else lzss(d + o, als, a, ast * (size_t)h);
        for (int y = 0; y < h; y++) {
            const u8 *row = a + (size_t)(h - 1 - y) * ast;
            u32 *out = im->px + (size_t)y * (size_t)w;
            for (int x = 0; x < w; x++) {
                u32 v = row[x];
                if (flags != 3) v = v >= 0x10 ? 255 : v * 16;
                out[x] = (out[x] & 0xffffff) | v << 24;
            }
        }
        free(a);
        im->alpha = true;
    }
    return im;
}

Image *image_load(EX *e, const char *rel) {
    if (!rel || !*rel || !strcmp(rel, "*")) return NULL;
    char *path = ex_game_path(e, rel);
    size_t n;
    u8 *d = ex_read_file(path, &n);
    free(path);
    if (!d) {
        ex_log(e, "missing picture %s", rel);
        return NULL;
    }
    Image *im = gyu_decode(d, n);
    free(d);
    if (!im) ex_log(e, "can't decode %s", rel);
    return im;
}

static inline u32 blend_px(u32 d, u32 s, u32 a) {
    if (a >= 255) return s | 0xff000000u;
    if (!a) return d;
    u32 ia = 255 - a;
    u32 r = ((s & 0xff) * a + (d & 0xff) * ia) / 255;
    u32 g = (((s >> 8) & 0xff) * a + ((d >> 8) & 0xff) * ia) / 255;
    u32 b = (((s >> 16) & 0xff) * a + ((d >> 16) & 0xff) * ia) / 255;
    return r | g << 8 | b << 16 | 0xff000000u;
}

void blit_part(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int w, int h, int dx, int dy, int alpha) {
    if (!im || alpha <= 0) return;
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    if (sx + w > im->w) w = im->w - sx;
    if (sy + h > im->h) h = im->h - sy;
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (dx + w > dw) w = dw - dx;
    if (dy + h > dh) h = dh - dy;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        const u32 *s = im->px + (size_t)(sy + j) * (size_t)im->w + sx;
        u32 *d = dst + (size_t)(dy + j) * (size_t)dw + dx;
        if (!im->alpha && alpha >= 255) {
            memcpy(d, s, sizeof(u32) * (size_t)w);
            continue;
        }
        for (int i = 0; i < w; i++) {
            u32 a = im->alpha ? (s[i] >> 24) * (u32)alpha / 255 : (u32)alpha;
            d[i] = blend_px(d[i], s[i] & 0xffffff, a);
        }
    }
}

void blit(u32 *dst, int dw, int dh, const Image *im, int dx, int dy, int alpha) {
    if (im) blit_part(dst, dw, dh, im, 0, 0, im->w, im->h, dx, dy, alpha);
}

void fill(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 color, int alpha) {
    if (alpha <= 0) return;
    for (int j = y < 0 ? 0 : y; j < y + h && j < dh; j++)
        for (int i = x < 0 ? 0 : x; i < x + w && i < dw; i++)
            dst[(size_t)j * (size_t)dw + i] = blend_px(dst[(size_t)j * (size_t)dw + i], color & 0xffffff, (u32)(alpha > 255 ? 255 : alpha));
}
