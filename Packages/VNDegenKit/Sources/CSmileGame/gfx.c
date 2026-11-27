#include "sg.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "third_party/stb_truetype.h"

Image *img_decode(const u8 *data, size_t size) {
    int w, h, n;
    u8 *px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!px) return NULL;
    Image *im = sg_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = sg_alloc(sizeof(u32) * (size_t)w * (size_t)h);
    memcpy(im->px, px, sizeof(u32) * (size_t)w * (size_t)h);
    stbi_image_free(px);
    return im;
}

void img_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

static inline u32 blend(u32 dst, u32 src, int alpha) {
    int a = (int)(src >> 24) * alpha / 255;
    if (a <= 0) return dst;
    if (a >= 255) return src | 0xff000000u;
    int ia = 255 - a;
    u32 r = ((src & 255) * (u32)a + (dst & 255) * (u32)ia) / 255;
    u32 g = ((src >> 8 & 255) * (u32)a + (dst >> 8 & 255) * (u32)ia) / 255;
    u32 b = ((src >> 16 & 255) * (u32)a + (dst >> 16 & 255) * (u32)ia) / 255;
    return 0xff000000u | b << 16 | g << 8 | r;
}

void blit(u32 *fb, int fw, int fh, const Image *im, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, int alpha) {
    if (!im || dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    for (int y = 0; y < dh; y++) {
        int ty = dy + y;
        if (ty < 0 || ty >= fh) continue;
        int syy = sy + y * sh / dh;
        if (syy < 0 || syy >= im->h) continue;
        const u32 *row = im->px + (size_t)syy * (size_t)im->w;
        u32 *out = fb + (size_t)ty * (size_t)fw;
        for (int x = 0; x < dw; x++) {
            int tx = dx + x;
            if (tx < 0 || tx >= fw) continue;
            int sxx = sx + x * sw / dw;
            if (sxx < 0 || sxx >= im->w) continue;
            out[tx] = blend(out[tx], row[sxx], alpha);
        }
    }
}

void fill_rect(u32 *fb, int fw, int fh, int x, int y, int w, int h, u32 color) {
    for (int yy = y < 0 ? 0 : y; yy < y + h && yy < fh; yy++)
        for (int xx = x < 0 ? 0 : x; xx < x + w && xx < fw; xx++) fb[(size_t)yy * (size_t)fw + (size_t)xx] = blend(fb[(size_t)yy * (size_t)fw + (size_t)xx], color, 255);
}

struct Font {
    u8 *data;
    stbtt_fontinfo info;
    Font *fallback;
};

Font *font_load(const u8 *data, size_t size) {
    Font *f = sg_alloc(sizeof *f);
    f->data = sg_alloc(size);
    memcpy(f->data, data, size);
    int off = stbtt_GetFontOffsetForIndex(f->data, 0);
    if (off < 0 || !stbtt_InitFont(&f->info, f->data, off)) {
        font_free(f);
        return NULL;
    }
    return f;
}

void font_free(Font *f) {
    if (!f) return;
    font_free(f->fallback);
    free(f->data);
    free(f);
}

void font_set_fallback(Font *f, Font *fallback) {
    font_free(f->fallback);
    f->fallback = fallback;
}

static Font *glyph_font(Font *f, u32 cp) {
    if (cp <= ' ' || !f->fallback || stbtt_FindGlyphIndex(&f->info, (int)cp)) return f;
    return stbtt_FindGlyphIndex(&f->fallback->info, (int)cp) ? f->fallback : f;
}

int font_width(Font *f, int px, const char *s) {
    if (!f) return 0;
    float sc = stbtt_ScaleForMappingEmToPixels(&f->info, (float)px);
    float x = 0;
    while (*s) {
        u32 cp;
        s += utf8_decode(s, &cp);
        Font *gf = glyph_font(f, cp);
        float gs = gf == f ? sc : stbtt_ScaleForMappingEmToPixels(&gf->info, (float)px);
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&gf->info, (int)cp, &adv, &lsb);
        x += (float)adv * gs;
    }
    return (int)(x + 0.5f);
}

int font_draw(Font *f, u32 *fb, int fw, int fh, int x0, int y0, int px, const char *s, u32 color) {
    if (!f) return x0;
    float sc = stbtt_ScaleForMappingEmToPixels(&f->info, (float)px);
    int asc, desc, gap;
    stbtt_GetFontVMetrics(&f->info, &asc, &desc, &gap);
    int base = y0 + (int)((float)asc * sc + 0.5f);
    float x = (float)x0;
    while (*s) {
        u32 cp;
        s += utf8_decode(s, &cp);
        if (cp == '\r' || cp == '\n') continue;
        Font *gf = glyph_font(f, cp);
        float gs = gf == f ? sc : stbtt_ScaleForMappingEmToPixels(&gf->info, (float)px);
        int adv, lsb;
        stbtt_GetCodepointHMetrics(&gf->info, (int)cp, &adv, &lsb);
        int w, h, xo, yo;
        u8 *bmp = stbtt_GetCodepointBitmapSubpixel(&gf->info, gs, gs, x - (float)(int)x, 0, (int)cp, &w, &h, &xo, &yo);
        if (bmp) {
            for (int yy = 0; yy < h; yy++) {
                int ty = base + yo + yy;
                if (ty < 0 || ty >= fh) continue;
                for (int xx = 0; xx < w; xx++) {
                    int tx = (int)x + xo + xx;
                    if (tx < 0 || tx >= fw) continue;
                    u8 a = bmp[yy * w + xx];
                    if (!a) continue;
                    u32 src = (color & 0x00ffffffu) | (u32)(a * (color >> 24) / 255) << 24;
                    fb[(size_t)ty * (size_t)fw + (size_t)tx] = blend(fb[(size_t)ty * (size_t)fw + (size_t)tx], src, 255);
                }
            }
            stbtt_FreeBitmap(bmp, NULL);
        }
        x += (float)adv * gs;
    }
    return (int)x;
}
