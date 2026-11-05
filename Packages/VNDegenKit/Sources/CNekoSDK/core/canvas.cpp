#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace neko {

void Canvas::clear() { std::memset(px.data(), 0, px.size()); for (size_t i = 3; i < px.size(); i += 4) px[i] = 255; }

namespace {

inline void blendPixel(uint8_t *d, int r, int g, int b, int a256) {
    d[0] = uint8_t(d[0] + (((r - d[0]) * a256) >> 8));
    d[1] = uint8_t(d[1] + (((g - d[1]) * a256) >> 8));
    d[2] = uint8_t(d[2] + (((b - d[2]) * a256) >> 8));
}

}  // namespace

void drawPart(Canvas &dst, const Image &image, int x, int y, int partX, int partY, int partW, int partH, float alpha) {
    if (alpha <= 0) return;
    if (x < 0) { partX -= x; partW += x; x = 0; }
    if (y < 0) { partY -= y; partH += y; y = 0; }
    partW = std::min({partW, dst.w - x, image.w - partX});
    partH = std::min({partH, dst.h - y, image.h - partY});
    if (partW <= 0 || partH <= 0) return;
    int a256 = int(alpha * 256 + 0.5f);
    for (int row = 0; row < partH; row++) {
        const uint8_t *s = image.rgba.data() + (size_t(partY + row) * image.w + partX) * 4;
        uint8_t *d = dst.px.data() + (size_t(y + row) * dst.w + x) * 4;
        if (image.opaque && a256 >= 256) {
            std::memcpy(d, s, size_t(partW) * 4);
            continue;
        }
        for (int i = 0; i < partW; i++, s += 4, d += 4) {
            int a = (s[3] * a256) >> 8;
            a += a >> 7;  // 255 becomes 256, so an opaque pixel replaces
            blendPixel(d, s[0], s[1], s[2], a);
        }
    }
}

void drawImage(Canvas &dst, const Image &image, float cx, float cy, float sx, float sy, float radians, float alpha) {
    if (alpha <= 0 || sx == 0 || sy == 0 || image.w == 0 || image.h == 0) return;
    float halfW = image.w * 0.5f, halfH = image.h * 0.5f;
    if (radians == 0 && sx == 1 && sy == 1) {
        float left = cx - halfW, top = cy - halfH;
        if (left == std::floor(left) && top == std::floor(top)) {
            drawPart(dst, image, int(left), int(top), 0, 0, image.w, image.h, alpha);
            return;
        }
    }
    // The screen rectangle that holds the transformed image.
    float c = std::cos(radians), s = std::sin(radians);
    float ex = std::abs(halfW * sx * c) + std::abs(halfH * sy * s), ey = std::abs(halfW * sx * s) + std::abs(halfH * sy * c);
    int x0 = std::max(0, int(std::floor(cx - ex))), x1 = std::min(dst.w, int(std::ceil(cx + ex)) + 1);
    int y0 = std::max(0, int(std::floor(cy - ey))), y1 = std::min(dst.h, int(std::ceil(cy + ey)) + 1);
    if (x0 >= x1 || y0 >= y1) return;
    // Each destination pixel's source position, in 16.16 fixed point, stepping along the row. Sampling points sit half
    // a pixel in from each edge, so an image at scale 1 samples its own pixels.
    constexpr float kOne = 65536.0f;
    float dux = c / sx, duy = s / sx, dvx = -s / sy, dvy = c / sy;
    int a256 = int(alpha * 256 + 0.5f);
    int maxU = (image.w - 1) << 16, maxV = (image.h - 1) << 16;
    bool bilinear = !(radians == 0 && sx == 1 && sy == 1);
    const uint8_t *src = image.rgba.data();
    for (int y = y0; y < y1; y++) {
        float rx = x0 + 0.5f - cx, ry = y + 0.5f - cy;
        // Double precision would cost more than it gains: a row is at most a few thousand pixels of steps under 1.
        int32_t u = int32_t((rx * dux + ry * duy + halfW - 0.5f) * kOne), v = int32_t((rx * dvx + ry * dvy + halfH - 0.5f) * kOne);
        int32_t stepU = int32_t(dux * kOne), stepV = int32_t(dvx * kOne);
        uint8_t *d = dst.px.data() + (size_t(y) * dst.w + x0) * 4;
        for (int x = x0; x < x1; x++, u += stepU, v += stepV, d += 4) {
            // Half a source pixel beyond the edge still counts, so an edge at a fraction of a pixel isn't cut short.
            if (u < -32768 || v < -32768 || u > maxU + 32768 || v > maxV + 32768) continue;
            int ux = std::clamp(u, 0, maxU), vy = std::clamp(v, 0, maxV);
            int iu = ux >> 16, iv = vy >> 16;
            const uint8_t *p = src + (size_t(iv) * image.w + iu) * 4;
            if (!bilinear) {
                int a = (p[3] * a256) >> 8;
                a += a >> 7;
                blendPixel(d, p[0], p[1], p[2], a);
                continue;
            }
            int fu = (ux >> 8) & 255, fv = (vy >> 8) & 255;
            const uint8_t *pr = iu + 1 < image.w ? p + 4 : p;
            const uint8_t *pd = iv + 1 < image.h ? p + size_t(image.w) * 4 : p;
            const uint8_t *pdr = iu + 1 < image.w ? pd + 4 : pd;
            int w00 = (256 - fu) * (256 - fv), w10 = fu * (256 - fv), w01 = (256 - fu) * fv, w11 = fu * fv;
            int r = (p[0] * w00 + pr[0] * w10 + pd[0] * w01 + pdr[0] * w11) >> 16;
            int g = (p[1] * w00 + pr[1] * w10 + pd[1] * w01 + pdr[1] * w11) >> 16;
            int b = (p[2] * w00 + pr[2] * w10 + pd[2] * w01 + pdr[2] * w11) >> 16;
            int sa = image.opaque ? 255 : (p[3] * w00 + pr[3] * w10 + pd[3] * w01 + pdr[3] * w11) >> 16;
            int a = (sa * a256) >> 8;
            a += a >> 7;
            blendPixel(d, r, g, b, a);
        }
    }
}

void blendFrame(Canvas &dst, const Canvas &from, int amount256) {
    if (amount256 <= 0 || from.w != dst.w || from.h != dst.h) return;
    int keep = 256 - amount256;
    const uint8_t *s = from.px.data();
    uint8_t *d = dst.px.data();
    size_t n = dst.px.size();
    for (size_t i = 0; i < n; i++) d[i] = uint8_t((d[i] * keep + s[i] * amount256) >> 8);
}

void softenFrame(Canvas &dst, int divisor, int amount256) {
    if (amount256 <= 0 || divisor < 2) return;
    int sw = std::max(2, dst.w / divisor), sh = std::max(2, dst.h / divisor);
    static thread_local std::vector<uint8_t> small;
    small.resize(size_t(sw) * sh * 3);
    // Averages each block, then grows it back with bilinear filtering.
    for (int y = 0; y < sh; y++) {
        int y0 = y * dst.h / sh, y1 = (y + 1) * dst.h / sh;
        for (int x = 0; x < sw; x++) {
            int x0 = x * dst.w / sw, x1 = (x + 1) * dst.w / sw;
            int r = 0, g = 0, b = 0, n = 0;
            // Every other pixel each way is plenty for a picture that's about to be blurred.
            for (int yy = y0; yy < y1; yy += 2) {
                const uint8_t *p = dst.px.data() + (size_t(yy) * dst.w + x0) * 4;
                for (int xx = x0; xx < x1; xx += 2, p += 8, n++) {
                    r += p[0];
                    g += p[1];
                    b += p[2];
                }
            }
            uint8_t *o = small.data() + (size_t(y) * sw + x) * 3;
            o[0] = uint8_t(r / std::max(1, n));
            o[1] = uint8_t(g / std::max(1, n));
            o[2] = uint8_t(b / std::max(1, n));
        }
    }
    int keep = 256 - std::min(amount256, 256);
    for (int y = 0; y < dst.h; y++) {
        int fy = (y * (sh - 1) << 8) / std::max(1, dst.h - 1);
        int iy = fy >> 8, wy = fy & 255, iy1 = std::min(iy + 1, sh - 1);
        uint8_t *d = dst.px.data() + size_t(y) * dst.w * 4;
        for (int x = 0; x < dst.w; x++, d += 4) {
            int fx = (x * (sw - 1) << 8) / std::max(1, dst.w - 1);
            int ix = fx >> 8, wx = fx & 255, ix1 = std::min(ix + 1, sw - 1);
            const uint8_t *a = small.data() + (size_t(iy) * sw + ix) * 3, *b = small.data() + (size_t(iy) * sw + ix1) * 3;
            const uint8_t *c = small.data() + (size_t(iy1) * sw + ix) * 3, *e = small.data() + (size_t(iy1) * sw + ix1) * 3;
            for (int k = 0; k < 3; k++) {
                int top = a[k] * (256 - wx) + b[k] * wx, bottom = c[k] * (256 - wx) + e[k] * wx;
                int v = (top * (256 - wy) + bottom * wy) >> 16;
                d[k] = uint8_t((d[k] * keep + v * (256 - keep)) >> 8);
            }
        }
    }
}

void tintFrame(Canvas &dst, int r, int g, int b, int amount256) {
    if (amount256 <= 0) return;
    int keep = 256 - amount256;
    int cr = r * amount256, cg = g * amount256, cb = b * amount256;
    uint8_t *d = dst.px.data();
    size_t n = dst.px.size();
    for (size_t i = 0; i < n; i += 4) {
        d[i] = uint8_t((d[i] * keep + cr) >> 8);
        d[i + 1] = uint8_t((d[i + 1] * keep + cg) >> 8);
        d[i + 2] = uint8_t((d[i + 2] * keep + cb) >> 8);
    }
}

void drawCoverage(Canvas &dst, const uint8_t *coverage, int w, int h, int x, int y, int r, int g, int b, int alpha256) {
    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if (yy < 0 || yy >= dst.h) continue;
        const uint8_t *c = coverage + size_t(row) * w;
        for (int col = 0; col < w; col++) {
            int xx = x + col;
            if (xx < 0 || xx >= dst.w || !c[col]) continue;
            int a = (c[col] * alpha256) >> 8;
            a += a >> 7;
            blendPixel(dst.px.data() + (size_t(yy) * dst.w + xx) * 4, r, g, b, a);
        }
    }
}

}  // namespace neko
