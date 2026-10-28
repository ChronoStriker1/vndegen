// CPU images and drawing. Pixels are 32-bit premultiplied RGBA, stored as uint32_t 0xAABBGGRR (R in the lowest byte),
// so the final frame is RGBA8 in memory order.
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "common.h"

namespace vnns {

struct Image {
    int w = 0, h = 0;
    std::vector<uint32_t> px;
    Image() = default;
    Image(int w_, int h_, uint32_t fill = 0) : w(std::max(0, w_)), h(std::max(0, h_)), px(size_t(w) * size_t(h), fill) {}
    uint32_t *row(int y) { return px.data() + size_t(y) * w; }
    const uint32_t *row(int y) const { return px.data() + size_t(y) * w; }
    bool empty() const { return w <= 0 || h <= 0; }
};
using ImageRef = std::shared_ptr<const Image>;

inline uint32_t rgba(int r, int g, int b, int a = 255) {
    return uint32_t(r) | uint32_t(g) << 8 | uint32_t(b) << 16 | uint32_t(a) << 24;
}
inline int chR(uint32_t p) { return p & 0xFF; }
inline int chG(uint32_t p) { return p >> 8 & 0xFF; }
inline int chB(uint32_t p) { return p >> 16 & 0xFF; }
inline int chA(uint32_t p) { return p >> 24; }
// x * a / 255, rounded, for 0..255 inputs.
inline uint32_t mul255(uint32_t x, uint32_t a) {
    uint32_t t = x * a + 128;
    return (t + (t >> 8)) >> 8;
}
// Scales every channel of a premultiplied pixel by a/255.
inline uint32_t scalePx(uint32_t p, uint32_t a) {
    if (a >= 255) return p;
    uint32_t rb = (p & 0x00FF00FF) * a + 0x00800080;
    rb = ((rb + ((rb >> 8) & 0x00FF00FF)) >> 8) & 0x00FF00FF;
    uint32_t ga = ((p >> 8) & 0x00FF00FF) * a + 0x00800080;
    ga = ((ga + ((ga >> 8) & 0x00FF00FF)) >> 8) & 0x00FF00FF;
    return rb | ga << 8;
}
// Source-over for premultiplied pixels.
inline uint32_t over(uint32_t dst, uint32_t src) {
    uint32_t sa = src >> 24;
    if (sa == 255) return src;
    if (sa == 0) return dst;
    return src + scalePx(dst, 255 - sa);
}
// Linear blend of two premultiplied pixels, t of 0..256.
inline uint32_t lerpPx(uint32_t a, uint32_t b, int t) {
    uint32_t ia = uint32_t(256 - t), ib = uint32_t(t);
    uint32_t rb = ((a & 0x00FF00FF) * ia + (b & 0x00FF00FF) * ib) >> 8 & 0x00FF00FF;
    uint32_t ga = (((a >> 8) & 0x00FF00FF) * ia + ((b >> 8) & 0x00FF00FF) * ib) >> 8 & 0x00FF00FF;
    return rb | ga << 8;
}

enum class Blend { Normal, Add, Sub };

// Draws src's rectangle (sx, sy, w, h) at (dx, dy) with an extra opacity of 0..255.
void blit(Image &dst, const Image &src, int dx, int dy, int sx, int sy, int w, int h, int alpha = 255,
          Blend blend = Blend::Normal);
// Draws src transformed by a 2x2 matrix around its centre, placed with its centre at (cx, cy). Nearest neighbour.
void blitTransformed(Image &dst, const Image &src, int sx, int sy, int sw, int sh, double cx, double cy,
                     const double m[4], int alpha, Blend blend);
void fillRect(Image &dst, int x, int y, int w, int h, uint32_t color);
// Darkens or tints the rectangle the way NScripter's color-key text window does: each channel multiplied by color/255.
void multiplyRect(Image &dst, int x, int y, int w, int h, uint32_t color);

// Decodes BMP, PNG or JPEG data to straight-alpha RGBA, then premultiplies.
bool decodeImage(const Bytes &data, Image &out, bool *hasAlpha);

}  // namespace vnns
