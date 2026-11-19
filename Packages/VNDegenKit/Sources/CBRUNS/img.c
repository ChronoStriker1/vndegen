#include "br.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

#define CACHE_SIZE 192
#define CACHE_BYTES (192u << 20)

typedef struct CacheEnt {
    char key[96];
    Image *img;
    u32 used;
} CacheEnt;

static CacheEnt cache[CACHE_SIZE];
static char missing[64][96];
static int missingNext;
static size_t cacheBytes;
static u32 cacheClock;

Image *image_new(int w, int h) {
    Image *im = br_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = br_alloc(sizeof(u32) * (size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1));
    im->refs = 1;
    return im;
}

void image_free(Image *im) {
    if (!im || --im->refs > 0) return;
    free(im->px);
    free(im);
}

Image *image_decode(const u8 *d, size_t n) {
    int w, h, c;
    u8 *px = stbi_load_from_memory(d, (int)n, &w, &h, &c, 4);
    if (!px) return NULL;
    Image *im = br_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = (u32 *)px;
    im->refs = 1;
    return im;
}

u8 *image_decode_gray(const u8 *d, size_t n, int *w, int *h) {
    int c;
    return stbi_load_from_memory(d, (int)n, w, h, &c, 1);
}

static void evict(void) {
    while (cacheBytes > CACHE_BYTES) {
        int best = -1;
        for (int i = 0; i < CACHE_SIZE; i++)
            if (cache[i].img && (best < 0 || cache[i].used < cache[best].used)) best = i;
        if (best < 0) return;
        cacheBytes -= (size_t)cache[best].img->w * cache[best].img->h * 4;
        image_free(cache[best].img);
        cache[best].img = NULL;
        cache[best].key[0] = 0;
    }
}

Image *image_load(BR *e, const char *rel) {
    for (int i = 0; i < CACHE_SIZE; i++)
        if (cache[i].img && !strcmp(cache[i].key, rel)) {
            cache[i].used = ++cacheClock;
            cache[i].img->refs++;
            return cache[i].img;
        }
    for (int i = 0; i < 64; i++)
        if (!strcmp(missing[i], rel)) return NULL;
    size_t n;
    u8 *d = br_read_game(e, rel, &n);
    if (!d) {
        br_log(e, "image %s is missing", rel);
        snprintf(missing[missingNext], sizeof missing[0], "%s", rel);
        missingNext = (missingNext + 1) % 64;
        return NULL;
    }
    Image *im = image_decode(d, n);
    free(d);
    if (!im) {
        br_log(e, "image %s can't be decoded", rel);
        return NULL;
    }
    int slot = 0;
    for (int i = 0; i < CACHE_SIZE; i++) {
        if (!cache[i].img) {
            slot = i;
            break;
        }
        if (cache[i].used < cache[slot].used) slot = i;
    }
    if (cache[slot].img) {
        cacheBytes -= (size_t)cache[slot].img->w * cache[slot].img->h * 4;
        image_free(cache[slot].img);
    }
    snprintf(cache[slot].key, sizeof cache[slot].key, "%s", rel);
    cache[slot].img = im;
    cache[slot].used = ++cacheClock;
    cacheBytes += (size_t)im->w * im->h * 4;
    im->refs++;
    evict();
    return im;
}

void image_cache_clear(void) {
    for (int i = 0; i < CACHE_SIZE; i++) {
        image_free(cache[i].img);
        cache[i].img = NULL;
        cache[i].key[0] = 0;
    }
    cacheBytes = 0;
    memset(missing, 0, sizeof missing);
}

Image *image_storage(BR *e, int num) {
    if (e->sysRoot && num >= 0) view_flag_set(e, 1, num, true);
    char rel[128];
    snprintf(rel, sizeof rel, "%s/%08d.png", e->cfg.imageDir, num);
    return image_load(e, rel);
}

Image *image_parts(BR *e, int num) {
    char rel[128];
    snprintf(rel, sizeof rel, "%s/%s%0*d.png", e->cfg.partsDir, e->cfg.partsBase, e->cfg.partsDigits, num);
    return image_load(e, rel);
}

Image *image_url(BR *e, Node *url) {
    if (!url) return NULL;
    if (url->t == N_INT) return image_parts(e, url->i);
    const char *s = sx_str(url);
    if (!s) return NULL;
    if (s[0] == '#') {
        int r = 0, g = 0, b = 0;
        sscanf(s + 1, "%d,%d,%d", &r, &g, &b);
        Image *im = image_new(1, 1);
        im->px[0] = (u32)r | (u32)g << 8 | (u32)b << 16 | 0xff000000u;
        return im;
    }
    char rel[256];
    snprintf(rel, sizeof rel, "%s", s);
    for (char *c = rel; *c; c++)
        if (*c == '\\') *c = '/';
    return image_load(e, rel);
}

static inline u32 mix(u32 d, u32 s, int a) {
    if (a <= 0) return d;
    if (a >= 255) return s | 0xff000000u;
    int ia = 255 - a;
    u32 r = ((s & 0xff) * a + (d & 0xff) * ia) / 255;
    u32 g = ((s >> 8 & 0xff) * a + (d >> 8 & 0xff) * ia) / 255;
    u32 b = ((s >> 16 & 0xff) * a + (d >> 16 & 0xff) * ia) / 255;
    return r | g << 8 | b << 16 | 0xff000000u;
}

void blit(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int alpha) {
    if (!im || alpha <= 0) return;
    if (sw < 0) sw = im->w;
    if (sh < 0) sh = im->h;
    if (sx < 0) {
        dx -= sx;
        sw += sx;
        sx = 0;
    }
    if (sy < 0) {
        dy -= sy;
        sh += sy;
        sy = 0;
    }
    if (sx + sw > im->w) sw = im->w - sx;
    if (sy + sh > im->h) sh = im->h - sy;
    if (dx < 0) {
        sx -= dx;
        sw += dx;
        dx = 0;
    }
    if (dy < 0) {
        sy -= dy;
        sh += dy;
        dy = 0;
    }
    if (dx + sw > dw) sw = dw - dx;
    if (dy + sh > dh) sh = dh - dy;
    if (sw <= 0 || sh <= 0) return;
    for (int y = 0; y < sh; y++) {
        const u32 *s = im->px + (size_t)(sy + y) * im->w + sx;
        u32 *d = dst + (size_t)(dy + y) * dw + dx;
        for (int x = 0; x < sw; x++) {
            u32 p = s[x];
            int a = (int)(p >> 24) * alpha / 255;
            d[x] = mix(d[x], p, a);
        }
    }
}

void blit_scaled(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int w, int h, int alpha) {
    if (!im || alpha <= 0 || w <= 0 || h <= 0 || sw <= 0 || sh <= 0) return;
    if (w == sw && h == sh) {
        blit(dst, dw, dh, im, sx, sy, sw, sh, dx, dy, alpha);
        return;
    }
    int x0 = dx < 0 ? 0 : dx, y0 = dy < 0 ? 0 : dy;
    int x1 = dx + w > dw ? dw : dx + w, y1 = dy + h > dh ? dh : dy + h;
    for (int y = y0; y < y1; y++) {
        int iy = sy + (int)((i64)(y - dy) * sh / h);
        if (iy < 0 || iy >= im->h) continue;
        const u32 *s = im->px + (size_t)iy * im->w;
        u32 *d = dst + (size_t)y * dw;
        for (int x = x0; x < x1; x++) {
            int ix = sx + (int)((i64)(x - dx) * sw / w);
            if (ix < 0 || ix >= im->w) continue;
            u32 p = s[ix];
            d[x] = mix(d[x], p, (int)(p >> 24) * alpha / 255);
        }
    }
}

void fill_rect(u32 *dst, int dw, int dh, int x, int y, int w, int h, u32 color, int alpha) {
    if (alpha <= 0) return;
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > dw ? dw : x + w, y1 = y + h > dh ? dh : y + h;
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) dst[(size_t)yy * dw + xx] = mix(dst[(size_t)yy * dw + xx], color, alpha);
}

void blit_tinted(u32 *dst, int dw, int dh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int w, int h, int alpha, u32 tint) {
    if (!im || alpha <= 0 || w <= 0 || h <= 0 || sw <= 0 || sh <= 0) return;
    int tr = tint & 0xff, tg = tint >> 8 & 0xff, tb = tint >> 16 & 0xff;
    int x0 = dx < 0 ? 0 : dx, y0 = dy < 0 ? 0 : dy;
    int x1 = dx + w > dw ? dw : dx + w, y1 = dy + h > dh ? dh : dy + h;
    for (int y = y0; y < y1; y++) {
        int iy = sy + (int)((i64)(y - dy) * sh / h);
        if (iy < 0 || iy >= im->h) continue;
        const u32 *s = im->px + (size_t)iy * im->w;
        u32 *d = dst + (size_t)y * dw;
        for (int x = x0; x < x1; x++) {
            int ix = sx + (int)((i64)(x - dx) * sw / w);
            if (ix < 0 || ix >= im->w) continue;
            u32 p = s[ix];
            u32 q = ((p & 0xff) * tr / 255) | ((p >> 8 & 0xff) * tg / 255) << 8 | ((p >> 16 & 0xff) * tb / 255) << 16 | (p & 0xff000000u);
            d[x] = mix(d[x], q, (int)(p >> 24) * alpha / 255);
        }
    }
}

void image_bbox(Image *im, int *x0, int *y0, int *x1, int *y1) {
    if (!im->boxed) {
        int a = im->w, b = im->h, c = 0, d = 0;
        for (int y = 0; y < im->h; y++)
            for (int x = 0; x < im->w; x++)
                if (im->px[(size_t)y * im->w + x] >> 24) {
                    if (x < a) a = x;
                    if (y < b) b = y;
                    if (x + 1 > c) c = x + 1;
                    if (y + 1 > d) d = y + 1;
                }
        if (c <= a || d <= b) a = b = c = d = 0;
        im->bx0 = a;
        im->by0 = b;
        im->bx1 = c;
        im->by1 = d;
        im->boxed = true;
    }
    *x0 = im->bx0;
    *y0 = im->by0;
    *x1 = im->bx1;
    *y1 = im->by1;
}
