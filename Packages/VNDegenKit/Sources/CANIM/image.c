#include "anim.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

typedef struct CacheEntry {
    char *name;
    Image *im;
    u32 used;
    size_t bytes;
} CacheEntry;

typedef struct ImageCache {
    CacheEntry *e;
    int count, cap;
    size_t bytes;
    u32 tick;
} ImageCache;

enum { CACHE_BUDGET = 320u << 20 };

Image *an_image_new(int w, int h) {
    Image *im = an_alloc(sizeof *im);
    im->w = im->cw = w;
    im->h = im->ch = h;
    im->px = an_alloc(sizeof(u32) * (size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1));
    return im;
}

void an_image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

Image *an_image_png(const u8 *data, size_t size) {
    int w, h, n;
    u8 *px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!px) return NULL;
    const u32 *src = (const u32 *)px;
    int x0 = w, y0 = h, x1 = -1, y1 = -1;
    for (int y = 0; y < h; y++) {
        const u32 *row = src + (size_t)y * (size_t)w;
        int first = -1, last = -1;
        for (int x = 0; x < w; x++)
            if (row[x] >> 24) {
                if (first < 0) first = x;
                last = x;
            }
        if (first < 0) continue;
        if (first < x0) x0 = first;
        if (last > x1) x1 = last;
        if (y < y0) y0 = y;
        y1 = y;
    }
    Image *im = an_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    if (x1 < 0) {
        im->cw = im->ch = 1;
        im->px = an_alloc(sizeof(u32));
    } else {
        im->x0 = x0;
        im->y0 = y0;
        im->cw = x1 - x0 + 1;
        im->ch = y1 - y0 + 1;
        im->px = an_alloc(sizeof(u32) * (size_t)im->cw * (size_t)im->ch);
        for (int y = 0; y < im->ch; y++)
            memcpy(im->px + (size_t)y * (size_t)im->cw, src + (size_t)(y + y0) * (size_t)w + (size_t)x0, sizeof(u32) * (size_t)im->cw);
    }
    stbi_image_free(px);
    return im;
}

static Image *load_file(AN *a, const char *file) {
    char *path = an_fmt("%s/cg/%s.gax", a->dir, file);
    size_t size;
    u8 *raw = an_read_file(path, &size);
    free(path);
    Image *im = NULL;
    if (raw) {
        size_t n;
        u8 *png = an_gax_open(raw, size, &n);
        if (png) im = an_image_png(png, n);
        free(png);
        free(raw);
        return im;
    }
    path = an_fmt("%s/cg/%s.png", a->dir, file);
    raw = an_read_file(path, &size);
    free(path);
    if (raw) {
        im = an_image_png(raw, size);
        free(raw);
    }
    return im;
}

static void evict(ImageCache *c) {
    while (c->bytes > CACHE_BUDGET && c->count > 1) {
        int oldest = 0;
        for (int i = 1; i < c->count; i++)
            if (c->e[i].used < c->e[oldest].used) oldest = i;
        c->bytes -= c->e[oldest].bytes;
        an_image_free(c->e[oldest].im);
        free(c->e[oldest].name);
        c->e[oldest] = c->e[--c->count];
    }
}

Image *an_image_load(AN *a, const char *name) {
    if (!name || !*name) return NULL;
    if (!a->images) a->images = an_alloc(sizeof(ImageCache));
    ImageCache *c = a->images;
    c->tick++;
    for (int i = 0; i < c->count; i++)
        if (an_ieq(c->e[i].name, name)) {
            c->e[i].used = c->tick;
            return c->e[i].im;
        }
    const char *file = an_define_sprite(&a->def, name);
    Image *im = file ? load_file(a, file) : NULL;
    if (!im) im = load_file(a, name);
    if (!im) an_log(a, "no picture %s", name);
    if (c->count == c->cap) {
        c->cap = c->cap ? c->cap * 2 : 64;
        c->e = an_realloc(c->e, sizeof(CacheEntry) * (size_t)c->cap);
    }
    size_t bytes = im ? sizeof(u32) * (size_t)im->cw * (size_t)im->ch : 0;
    c->e[c->count++] = (CacheEntry){an_strdup(name), im, c->tick, bytes};
    c->bytes += bytes;
    evict(c);
    return im;
}

void an_image_cache_clear(AN *a) {
    ImageCache *c = a->images;
    if (!c) return;
    for (int i = 0; i < c->count; i++) {
        an_image_free(c->e[i].im);
        free(c->e[i].name);
    }
    free(c->e);
    free(c);
    a->images = NULL;
}
