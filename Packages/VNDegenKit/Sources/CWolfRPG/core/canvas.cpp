#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace wolf {

namespace {

inline uint8_t clamp8(int v) { return uint8_t(v < 0 ? 0 : v > 255 ? 255 : v); }

// Blends one source colour (already multiplied by the colour options) with coverage `a` (0 to 255) into dst.
inline void blendPixel(uint8_t *d, int r, int g, int b, int a, Blend blend) {
    if (a <= 0) return;
    switch (blend) {
        case Blend::Normal:
            if (a >= 255) {
                d[0] = clamp8(r);
                d[1] = clamp8(g);
                d[2] = clamp8(b);
                d[3] = 255;
            } else {
                int inv = 255 - a;
                int da = d[3];
                int outA = a + da * inv / 255;
                if (outA <= 0) return;
                // Straight alpha: weight the destination by its own alpha.
                d[0] = clamp8((r * a + d[0] * da * inv / 255) / outA);
                d[1] = clamp8((g * a + d[1] * da * inv / 255) / outA);
                d[2] = clamp8((b * a + d[2] * da * inv / 255) / outA);
                d[3] = clamp8(outA);
            }
            break;
        case Blend::Add:
            d[0] = clamp8(d[0] + r * a / 255);
            d[1] = clamp8(d[1] + g * a / 255);
            d[2] = clamp8(d[2] + b * a / 255);
            break;
        case Blend::Subtract:
            d[0] = clamp8(d[0] - r * a / 255);
            d[1] = clamp8(d[1] - g * a / 255);
            d[2] = clamp8(d[2] - b * a / 255);
            break;
        case Blend::Multiply:
            d[0] = clamp8(d[0] * (255 * 255 - a * (255 - std::min(r, 255))) / (255 * 255));
            d[1] = clamp8(d[1] * (255 * 255 - a * (255 - std::min(g, 255))) / (255 * 255));
            d[2] = clamp8(d[2] * (255 * 255 - a * (255 - std::min(b, 255))) / (255 * 255));
            break;
    }
}

}  // namespace

void Canvas::clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    for (int y = 0; y < t_.h; y++) {
        uint8_t *d = t_.row(y);
        for (int x = 0; x < t_.w; x++, d += 4) {
            d[0] = r;
            d[1] = g;
            d[2] = b;
            d[3] = a;
        }
    }
}

void Canvas::draw(const Bitmap &src, RectI from, RectF to, const DrawOptions &o) {
    if (src.empty() || to.w <= 0 || to.h <= 0 || o.opacity <= 0) return;
    from.x = std::clamp(from.x, 0, src.w);
    from.y = std::clamp(from.y, 0, src.h);
    from.w = std::clamp(from.w, 0, src.w - from.x);
    from.h = std::clamp(from.h, 0, src.h - from.y);
    if (from.w <= 0 || from.h <= 0) return;

    const int opacity = int(std::lround(std::clamp(o.opacity, 0.0f, 1.0f) * 255));
    const int mr = int(std::lround(std::clamp(o.r, 0.0f, 2.0f) * 256));
    const int mg = int(std::lround(std::clamp(o.g, 0.0f, 2.0f) * 256));
    const int mb = int(std::lround(std::clamp(o.b, 0.0f, 2.0f) * 256));
    const bool plainColour = mr == 256 && mg == 256 && mb == 256;
    const double sx = from.w / double(to.w), sy = from.h / double(to.h);

    // Most drawing is unrotated and nearest-neighbour: tiles, characters, pictures. That path maps each destination
    // column and row to a source one once, and copies opaque pixels without blending.
    if (std::fmod(std::abs(o.angle), 360.0) <= 1e-6 && !o.smooth) {
        int x0 = std::max(0, int(std::ceil(to.x - 0.5f))), x1 = std::min(t_.w, int(std::ceil(to.x + to.w - 0.5f)));
        int y0 = std::max(0, int(std::ceil(to.y - 0.5f))), y1 = std::min(t_.h, int(std::ceil(to.y + to.h - 0.5f)));
        if (x0 >= x1 || y0 >= y1) return;
        thread_local std::vector<int> columns;
        columns.resize(size_t(x1 - x0));
        for (int x = x0; x < x1; x++)
            columns[x - x0] = (from.x + std::min(from.w - 1, std::max(0, int((x + 0.5 - to.x) * sx)))) * 4;
        bool plain = opacity == 255 && plainColour && o.blend == Blend::Normal;
        for (int y = y0; y < y1; y++) {
            int v = std::min(from.h - 1, std::max(0, int((y + 0.5 - to.y) * sy)));
            const uint8_t *srow = src.row(from.y + v);
            uint8_t *d = t_.row(y) + x0 * 4;
            for (int i = 0; i < x1 - x0; i++, d += 4) {
                const uint8_t *p = srow + columns[i];
                int a = p[3];
                if (a == 0) continue;
                if (plain && a == 255) {
                    std::memcpy(d, p, 4);
                    continue;
                }
                int r = p[0], g = p[1], b = p[2];
                if (!plainColour) {
                    r = r * mr >> 8;
                    g = g * mg >> 8;
                    b = b * mb >> 8;
                }
                blendPixel(d, r, g, b, a * opacity / 255, o.blend);
            }
        }
        return;
    }

    // The destination pixels to visit: the rectangle, or the bounding box of the rotated rectangle.
    double cx = to.x + to.w / 2.0, cy = to.y + to.h / 2.0;
    double rad = o.angle * M_PI / 180.0, cs = std::cos(rad), sn = std::sin(rad);
    bool rotated = std::fmod(std::abs(o.angle), 360.0) > 1e-6;
    double minX = to.x, minY = to.y, maxX = to.x + to.w, maxY = to.y + to.h;
    if (rotated) {
        double hw = to.w / 2.0, hh = to.h / 2.0;
        double ex = std::abs(hw * cs) + std::abs(hh * sn), ey = std::abs(hw * sn) + std::abs(hh * cs);
        minX = cx - ex;
        maxX = cx + ex;
        minY = cy - ey;
        maxY = cy + ey;
    }
    int x0 = std::max(0, int(std::floor(minX))), x1 = std::min(t_.w, int(std::ceil(maxX)));
    int y0 = std::max(0, int(std::floor(minY))), y1 = std::min(t_.h, int(std::ceil(maxY)));

    for (int y = y0; y < y1; y++) {
        uint8_t *d = t_.row(y) + x0 * 4;
        for (int x = x0; x < x1; x++, d += 4) {
            // The point in the unrotated destination rectangle this pixel's centre shows.
            double px = x + 0.5, py = y + 0.5;
            if (rotated) {
                double dx = px - cx, dy = py - cy;
                px = cx + dx * cs + dy * sn;
                py = cy - dx * sn + dy * cs;
            }
            double u = (px - to.x) * sx, v = (py - to.y) * sy;
            if (u < 0 || v < 0 || u >= from.w || v >= from.h) continue;
            int r, g, b, a;
            if (o.smooth) {
                double fu = std::max(0.0, u - 0.5), fv = std::max(0.0, v - 0.5);
                int iu = std::min(int(fu), from.w - 1), iv = std::min(int(fv), from.h - 1);
                int iu1 = std::min(iu + 1, from.w - 1), iv1 = std::min(iv + 1, from.h - 1);
                double wu = fu - iu, wv = fv - iv;
                const uint8_t *p00 = src.row(from.y + iv) + (from.x + iu) * 4;
                const uint8_t *p10 = src.row(from.y + iv) + (from.x + iu1) * 4;
                const uint8_t *p01 = src.row(from.y + iv1) + (from.x + iu) * 4;
                const uint8_t *p11 = src.row(from.y + iv1) + (from.x + iu1) * 4;
                double w00 = (1 - wu) * (1 - wv) * p00[3], w10 = wu * (1 - wv) * p10[3];
                double w01 = (1 - wu) * wv * p01[3], w11 = wu * wv * p11[3];
                double wa = w00 + w10 + w01 + w11;
                if (wa <= 0) continue;
                r = int((p00[0] * w00 + p10[0] * w10 + p01[0] * w01 + p11[0] * w11) / wa);
                g = int((p00[1] * w00 + p10[1] * w10 + p01[1] * w01 + p11[1] * w11) / wa);
                b = int((p00[2] * w00 + p10[2] * w10 + p01[2] * w01 + p11[2] * w11) / wa);
                a = int(wa + 0.5);
            } else {
                const uint8_t *s = src.row(from.y + int(v)) + (from.x + int(u)) * 4;
                r = s[0];
                g = s[1];
                b = s[2];
                a = s[3];
            }
            if (!plainColour) {
                r = r * mr >> 8;
                g = g * mg >> 8;
                b = b * mb >> 8;
            }
            blendPixel(d, r, g, b, a * opacity / 255, o.blend);
        }
    }
}

void Canvas::fill(RectF to, uint8_t r, uint8_t g, uint8_t b, uint8_t a, Blend blend) {
    int x0 = std::max(0, int(std::floor(to.x))), x1 = std::min(t_.w, int(std::ceil(to.x + to.w)));
    int y0 = std::max(0, int(std::floor(to.y))), y1 = std::min(t_.h, int(std::ceil(to.y + to.h)));
    for (int y = y0; y < y1; y++) {
        uint8_t *d = t_.row(y) + x0 * 4;
        for (int x = x0; x < x1; x++, d += 4) blendPixel(d, r, g, b, a, blend);
    }
}

void Canvas::tint(float r, float g, float b) {
    int mr = int(std::lround(std::clamp(r, 0.0f, 2.0f) * 256));
    int mg = int(std::lround(std::clamp(g, 0.0f, 2.0f) * 256));
    int mb = int(std::lround(std::clamp(b, 0.0f, 2.0f) * 256));
    if (mr == 256 && mg == 256 && mb == 256) return;
    for (int y = 0; y < t_.h; y++) {
        uint8_t *d = t_.row(y);
        for (int x = 0; x < t_.w; x++, d += 4) {
            d[0] = clamp8(d[0] * mr >> 8);
            d[1] = clamp8(d[1] * mg >> 8);
            d[2] = clamp8(d[2] * mb >> 8);
        }
    }
}

}  // namespace wolf
