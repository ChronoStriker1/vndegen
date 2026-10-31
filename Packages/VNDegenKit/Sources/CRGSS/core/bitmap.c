// Bitmap drawing: blt, stretch_blt, fills, gradients, hue change and blurs, on straight-alpha RGBA images.
#include "rgss.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

IRect irect_clip(IRect r, int w, int h) {
    if (r.x < 0) { r.w += r.x; r.x = 0; }
    if (r.y < 0) { r.h += r.y; r.y = 0; }
    if (r.x + r.w > w) r.w = w - r.x;
    if (r.y + r.h > h) r.h = h - r.y;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

// Source over destination with straight alpha. `alpha` is the source pixel's alpha already scaled by the opacity.
static inline Pixel blend_over(Pixel dst, Pixel src, int alpha) {
    if (alpha <= 0) return dst;
    int da = px_a(dst);
    if (alpha >= 255) return src | 0xFF000000u;
    if (da == 0) return (src & 0x00FFFFFFu) | (Pixel)alpha << 24;
    // Result alpha: a + d(1 - a). Colors weighted by each side's share of it.
    int outA = alpha + da * (255 - alpha) / 255;
    int dw = da * (255 - alpha) / 255;
    int r = (px_r(src) * alpha + px_r(dst) * dw) / outA;
    int g = (px_g(src) * alpha + px_g(dst) * dw) / outA;
    int b = (px_b(src) * alpha + px_b(dst) * dw) / outA;
    return px_make(r, g, b, outA);
}

void image_blt(Image *dst, int x, int y, const Image *src, IRect s, int opacity) {
    if (opacity <= 0) return;
    if (opacity > 255) opacity = 255;
    // Clip the source rectangle to the source, then the destination.
    IRect clipped = irect_clip(s, src->w, src->h);
    x += clipped.x - s.x;
    y += clipped.y - s.y;
    s = clipped;
    if (x < 0) { s.x -= x; s.w += x; x = 0; }
    if (y < 0) { s.y -= y; s.h += y; y = 0; }
    if (x + s.w > dst->w) s.w = dst->w - x;
    if (y + s.h > dst->h) s.h = dst->h - y;
    if (s.w <= 0 || s.h <= 0) return;
    for (int row = 0; row < s.h; row++) {
        const Pixel *in = src->px + (size_t)(s.y + row) * src->w + s.x;
        Pixel *out = dst->px + (size_t)(y + row) * dst->w + x;
        for (int col = 0; col < s.w; col++) {
            int a = px_a(in[col]) * opacity / 255;
            out[col] = blend_over(out[col], in[col], a);
        }
    }
}

void image_stretch_blt(Image *dst, IRect d, const Image *src, IRect s, int opacity) {
    if (opacity <= 0 || d.w == 0 || d.h == 0 || s.w == 0 || s.h == 0) return;
    if (opacity > 255) opacity = 255;
    IRect clip = irect_clip(d, dst->w, dst->h);
    for (int y = clip.y; y < clip.y + clip.h; y++) {
        int sy = s.y + (int)((int64_t)(y - d.y) * s.h / d.h);
        if (sy < 0 || sy >= src->h) continue;
        Pixel *out = dst->px + (size_t)y * dst->w;
        for (int x = clip.x; x < clip.x + clip.w; x++) {
            int sx = s.x + (int)((int64_t)(x - d.x) * s.w / d.w);
            if (sx < 0 || sx >= src->w) continue;
            Pixel p = src->px[(size_t)sy * src->w + sx];
            out[x] = blend_over(out[x], p, px_a(p) * opacity / 255);
        }
    }
}

void image_fill(Image *dst, IRect r, Pixel color) {
    r = irect_clip(r, dst->w, dst->h);
    for (int y = r.y; y < r.y + r.h; y++) {
        Pixel *row = dst->px + (size_t)y * dst->w + r.x;
        for (int x = 0; x < r.w; x++) row[x] = color;
    }
}

void image_gradient(Image *dst, IRect r, Pixel a, Pixel b, bool vertical) {
    IRect c = irect_clip(r, dst->w, dst->h);
    int span = vertical ? r.h : r.w;
    if (span <= 0) return;
    for (int y = c.y; y < c.y + c.h; y++) {
        for (int x = c.x; x < c.x + c.w; x++) {
            int t = vertical ? y - r.y : x - r.x;
            int u = span > 1 ? t * 255 / (span - 1) : 0;
            int v = 255 - u;
            dst->px[(size_t)y * dst->w + x] =
                px_make((px_r(a) * v + px_r(b) * u) / 255, (px_g(a) * v + px_g(b) * u) / 255,
                        (px_b(a) * v + px_b(b) * u) / 255, (px_a(a) * v + px_a(b) * u) / 255);
        }
    }
}

// Rotates each pixel's hue by `hue` degrees, keeping its lightness and saturation, through HSV.
void image_hue_change(Image *image, int hue) {
    hue %= 360;
    if (hue < 0) hue += 360;
    if (hue == 0) return;
    size_t count = (size_t)image->w * image->h;
    for (size_t i = 0; i < count; i++) {
        Pixel p = image->px[i];
        if (!px_a(p)) continue;
        int r = px_r(p), g = px_g(p), b = px_b(p);
        int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
        int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
        int delta = max - min;
        if (delta == 0) continue;
        double h;
        if (max == r) h = 60.0 * (g - b) / delta;
        else if (max == g) h = 60.0 * (b - r) / delta + 120;
        else h = 60.0 * (r - g) / delta + 240;
        h = fmod(h + hue + 360.0, 360.0);
        double s = (double)delta / max, v = max;
        int sector = (int)(h / 60) % 6;
        double f = h / 60 - floor(h / 60);
        double pv = v * (1 - s), qv = v * (1 - s * f), tv = v * (1 - s * (1 - f));
        double rr, gg, bb;
        switch (sector) {
        case 0: rr = v; gg = tv; bb = pv; break;
        case 1: rr = qv; gg = v; bb = pv; break;
        case 2: rr = pv; gg = v; bb = tv; break;
        case 3: rr = pv; gg = qv; bb = v; break;
        case 4: rr = tv; gg = pv; bb = v; break;
        default: rr = v; gg = pv; bb = qv; break;
        }
        image->px[i] = px_make((int)lround(rr), (int)lround(gg), (int)lround(bb), px_a(p));
    }
}

// A 3x3 box blur, which is what one call of Bitmap#blur looks like.
void image_blur(Image *image) {
    int w = image->w, h = image->h;
    Pixel *copy = malloc((size_t)w * h * sizeof(Pixel));
    if (!copy) return;
    memcpy(copy, image->px, (size_t)w * h * sizeof(Pixel));
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            int sum[4] = {0}, n = 0;
            for (int dy = -1; dy <= 1; dy++) {
                int yy = y + dy;
                if (yy < 0 || yy >= h) continue;
                for (int dx = -1; dx <= 1; dx++) {
                    int xx = x + dx;
                    if (xx < 0 || xx >= w) continue;
                    Pixel p = copy[(size_t)yy * w + xx];
                    sum[0] += px_r(p);
                    sum[1] += px_g(p);
                    sum[2] += px_b(p);
                    sum[3] += px_a(p);
                    n++;
                }
            }
            image->px[(size_t)y * w + x] = px_make(sum[0] / n, sum[1] / n, sum[2] / n, sum[3] / n);
        }
    }
    free(copy);
}

// Averages `division` copies of the image rotated about its center across `angle` degrees.
void image_radial_blur(Image *image, int angle, int division) {
    if (division < 2) division = 2;
    if (division > 100) division = 100;
    if (angle < 0) angle = 0;
    if (angle > 360) angle = 360;
    int w = image->w, h = image->h;
    size_t count = (size_t)w * h;
    Pixel *copy = malloc(count * sizeof(Pixel));
    uint32_t *sum = calloc(count * 4, sizeof(uint32_t));
    if (!copy || !sum) {
        free(copy);
        free(sum);
        return;
    }
    memcpy(copy, image->px, count * sizeof(Pixel));
    double cx = w / 2.0, cy = h / 2.0;
    for (int k = 0; k < division; k++) {
        double theta = (angle * (k / (double)(division - 1) - 0.5)) * M_PI / 180.0;
        double c = cos(theta), s = sin(theta);
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                double dx = x + 0.5 - cx, dy = y + 0.5 - cy;
                int sx = (int)floor(cx + dx * c - dy * s), sy = (int)floor(cy + dx * s + dy * c);
                if (sx < 0 || sy < 0 || sx >= w || sy >= h) continue;
                Pixel p = copy[(size_t)sy * w + sx];
                uint32_t *acc = sum + ((size_t)y * w + x) * 4;
                acc[0] += px_r(p);
                acc[1] += px_g(p);
                acc[2] += px_b(p);
                acc[3] += px_a(p);
            }
        }
    }
    for (size_t i = 0; i < count; i++) {
        uint32_t *acc = sum + i * 4;
        image->px[i] = px_make((int)(acc[0] / division), (int)(acc[1] / division), (int)(acc[2] / division),
                               (int)(acc[3] / division));
    }
    free(copy);
    free(sum);
}
