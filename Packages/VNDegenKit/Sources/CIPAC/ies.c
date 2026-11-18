#include "ipac.h"

#define CACHE_MAX 48

Image *image_new(int w, int h) {
    Image *im = ip_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = ip_alloc(sizeof(u32) * (size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1));
    return im;
}

void image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

Image *ies_decode(const u8 *d, size_t n) {
    if (n < 0x20 || memcmp(d, "IES2", 4)) return NULL;
    int w = (int)rd32(d + 8), h = (int)rd32(d + 12), bpp = (int)rd32(d + 16);
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return NULL;
    size_t px = (size_t)w * (size_t)h;
    Image *im = image_new(w, h);
    if (bpp == 24) {
        if (n < 0x420 + px * 3) {
            image_free(im);
            return NULL;
        }
        const u8 *rgb = d + 0x420, *a = n >= 0x420 + px * 4 ? d + 0x420 + px * 3 : NULL;
        bool opaque = true;
        for (size_t i = 0; i < px; i++) {
            u32 al = a ? a[i] : 255;
            if (al != 255) opaque = false;
            im->px[i] = (u32)rgb[i * 3 + 2] | (u32)rgb[i * 3 + 1] << 8 | (u32)rgb[i * 3] << 16 | al << 24;
        }
        im->opaque = opaque;
    } else if (bpp == 8) {
        if (n < 0x420 + px) {
            image_free(im);
            return NULL;
        }
        const u8 *pal = d + 0x20, *p = d + 0x420;
        for (size_t i = 0; i < px; i++) {
            const u8 *c = pal + p[i] * 4;
            im->px[i] = (u32)c[2] | (u32)c[1] << 8 | (u32)c[0] << 16 | 0xff000000u;
        }
        im->opaque = true;
    } else {
        image_free(im);
        return NULL;
    }
    return im;
}

static const int fallback[PK_KINDS][3] = {
    [PK_BG] = {PK_BG, PK_PARTS, PK_CHR},
    [PK_CHR] = {PK_CHR, PK_PARTS, PK_BG},
    [PK_PARTS] = {PK_PARTS, PK_BG, PK_CHR},
};

Image *image_load(IP *t, int kind, const char *name) {
    Image *im = NULL;
    for (int k = 0; k < 3 && !im; k++) {
        size_t n;
        u8 *d = store_load(&t->store, fallback[kind][k], name, ".IES", &n);
        if (!d) continue;
        im = ies_decode(d, n);
        free(d);
    }
    if (!im) {
        char *u = ip_sjis_to_utf8(name);
        ip_log(t, "image %s not found", u);
        free(u);
    }
    return im;
}

Image *image_get(IP *t, int kind, const char *name) {
    if (!name || !*name) return NULL;
    t->cacheTick++;
    for (int i = 0; i < t->cacheCount; i++)
        if (t->cache[i].kind == kind && !strcmp(t->cache[i].name, name)) {
            t->cache[i].used = t->cacheTick;
            return t->cache[i].img;
        }
    Image *im = image_load(t, kind, name);
    if (!im) return NULL;
    t->cache = ip_realloc(t->cache, sizeof *t->cache * (size_t)(t->cacheCount + 1));
    t->cache[t->cacheCount++] = (struct CacheEntry){kind, ip_strdup(name), im, t->cacheTick};
    return im;
}

static bool in_use(IP *t, const Image *im) {
    if (im == t->bg || im == t->clickMap || im == t->rule) return true;
    for (int i = 0; i < SP_COUNT; i++)
        if (t->spr[i].img == im) return true;
    for (int i = 0; i < IP_OBJS; i++)
        if (t->obj[i].img == im) return true;
    return false;
}

void image_cache_trim(IP *t) {
    while (t->cacheCount > CACHE_MAX) {
        int oldest = -1;
        for (int i = 0; i < t->cacheCount; i++)
            if (!in_use(t, t->cache[i].img) && (oldest < 0 || t->cache[i].used < t->cache[oldest].used)) oldest = i;
        if (oldest < 0) return;
        image_free(t->cache[oldest].img);
        free(t->cache[oldest].name);
        t->cache[oldest] = t->cache[--t->cacheCount];
    }
}
