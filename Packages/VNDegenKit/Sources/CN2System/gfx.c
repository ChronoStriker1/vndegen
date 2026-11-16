// Images: decoding through stb_image (public domain or MIT) into premultiplied RGBA8, and the blur SetShade asks for.
#include "n2.h"

#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#include "third_party/stb_image.h"

Image *img_new(int w, int h) {
    Image *i = n2_alloc(sizeof *i);
    i->w = w;
    i->h = h;
    i->px = n2_alloc(sizeof(u32) * (size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1));
    i->rc = 1;
    return i;
}

Image *img_ref(Image *i) {
    if (i) i->rc++;
    return i;
}

void img_unref(Image *i) {
    if (i && --i->rc == 0) {
        free(i->px);
        free(i);
    }
}

Image *img_decode(const u8 *data, size_t size) {
    int w, h, comp;
    u8 *px = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 4);
    if (!px) return NULL;
    Image *img = img_new(w, h);
    for (int i = 0; i < w * h; i++) {
        u32 r = px[i * 4], g = px[i * 4 + 1], b = px[i * 4 + 2], a = px[i * 4 + 3];
        if (a != 255) {
            r = (r * a + 127) / 255;
            g = (g * a + 127) / 255;
            b = (b * a + 127) / 255;
        }
        img->px[i] = r | g << 8 | b << 16 | a << 24;
    }
    stbi_image_free(px);
    return img;
}

// Three box-blur passes in each direction approximate a Gaussian.
static void box_pass(const u32 *src, u32 *dst, int len, int stride, int count, int cstride, int r) {
    for (int c = 0; c < count; c++) {
        const u32 *s = src + (size_t)c * cstride;
        u32 *d = dst + (size_t)c * cstride;
        u32 acc[4] = {0};
        int win = 2 * r + 1;
        for (int i = -r; i <= r; i++) {
            int k = i < 0 ? 0 : i >= len ? len - 1 : i;
            u32 p = s[(size_t)k * stride];
            for (int ch = 0; ch < 4; ch++) acc[ch] += (p >> (8 * ch)) & 0xff;
        }
        for (int i = 0; i < len; i++) {
            d[(size_t)i * stride] = (acc[0] / win) | (acc[1] / win) << 8 | (acc[2] / win) << 16 | (acc[3] / win) << 24;
            int out = i - r < 0 ? 0 : i - r, in = i + r + 1 >= len ? len - 1 : i + r + 1;
            u32 po = s[(size_t)out * stride], pi = s[(size_t)in * stride];
            for (int ch = 0; ch < 4; ch++) acc[ch] += ((pi >> (8 * ch)) & 0xff) - ((po >> (8 * ch)) & 0xff);
        }
    }
}

Image *img_blur(Image *src, int radius) {
    Image *a = img_new(src->w, src->h), *b = img_new(src->w, src->h);
    memcpy(a->px, src->px, sizeof(u32) * (size_t)src->w * (size_t)src->h);
    for (int pass = 0; pass < 3; pass++) {
        box_pass(a->px, b->px, src->w, 1, src->h, src->w, radius);
        box_pass(b->px, a->px, src->h, src->w, src->w, 1, radius);
    }
    img_unref(b);
    return a;
}
