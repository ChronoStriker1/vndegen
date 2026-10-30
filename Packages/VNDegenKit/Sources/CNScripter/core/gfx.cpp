#include "gfx.h"

#include <algorithm>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#define STBI_NO_FAILURE_STRINGS
#include "../third_party/stb_image.h"

namespace vnns {

void blit(Image &dst, const Image &src, int dx, int dy, int sx, int sy, int w, int h, int alpha, Blend blend) {
    if (alpha <= 0) return;
    if (dx < 0) { sx -= dx; w += dx; dx = 0; }
    if (dy < 0) { sy -= dy; h += dy; dy = 0; }
    if (sx < 0) { dx -= sx; w += sx; sx = 0; }
    if (sy < 0) { dy -= sy; h += sy; sy = 0; }
    w = std::min({w, dst.w - dx, src.w - sx});
    h = std::min({h, dst.h - dy, src.h - sy});
    if (w <= 0 || h <= 0) return;
    uint32_t a = uint32_t(std::min(alpha, 255));
    for (int y = 0; y < h; y++) {
        uint32_t *d = dst.row(dy + y) + dx;
        const uint32_t *s = src.row(sy + y) + sx;
        for (int x = 0; x < w; x++) {
            uint32_t p = scalePx(s[x], a);
            if (blend == Blend::Normal) {
                d[x] = over(d[x], p);
            } else {
                int sign = blend == Blend::Add ? 1 : -1;
                int r = std::clamp(chR(d[x]) + sign * chR(p), 0, 255);
                int g = std::clamp(chG(d[x]) + sign * chG(p), 0, 255);
                int b = std::clamp(chB(d[x]) + sign * chB(p), 0, 255);
                d[x] = rgba(r, g, b, chA(d[x]));
            }
        }
    }
}

void blitTransformed(Image &dst, const Image &src, int sx, int sy, int sw, int sh, double cx, double cy,
                     const double m[4], int alpha, Blend blend) {
    double det = m[0] * m[3] - m[1] * m[2];
    if (std::fabs(det) < 1e-9 || alpha <= 0) return;
    // Inverse maps destination offsets back into the source, whose centre is the origin.
    double i0 = m[3] / det, i1 = -m[1] / det, i2 = -m[2] / det, i3 = m[0] / det;
    double hw = sw / 2.0, hh = sh / 2.0;
    double xs[4], ys[4];
    double corners[4][2] = {{-hw, -hh}, {hw, -hh}, {-hw, hh}, {hw, hh}};
    for (int k = 0; k < 4; k++) {
        xs[k] = cx + m[0] * corners[k][0] + m[1] * corners[k][1];
        ys[k] = cy + m[2] * corners[k][0] + m[3] * corners[k][1];
    }
    int x0 = std::max(0, int(std::floor(*std::min_element(xs, xs + 4))));
    int x1 = std::min(dst.w, int(std::ceil(*std::max_element(xs, xs + 4))));
    int y0 = std::max(0, int(std::floor(*std::min_element(ys, ys + 4))));
    int y1 = std::min(dst.h, int(std::ceil(*std::max_element(ys, ys + 4))));
    uint32_t a = uint32_t(std::min(alpha, 255));
    for (int y = y0; y < y1; y++) {
        uint32_t *d = dst.row(y);
        double ry = y + 0.5 - cy;
        for (int x = x0; x < x1; x++) {
            double rx = x + 0.5 - cx;
            double u = i0 * rx + i1 * ry + hw, v = i2 * rx + i3 * ry + hh;
            if (u < 0 || v < 0 || u >= sw || v >= sh) continue;
            uint32_t p = scalePx(src.row(sy + int(v))[sx + int(u)], a);
            if (blend == Blend::Normal) {
                d[x] = over(d[x], p);
            } else {
                int sign = blend == Blend::Add ? 1 : -1;
                d[x] = rgba(std::clamp(chR(d[x]) + sign * chR(p), 0, 255), std::clamp(chG(d[x]) + sign * chG(p), 0, 255),
                            std::clamp(chB(d[x]) + sign * chB(p), 0, 255), chA(d[x]));
            }
        }
    }
}

void fillRect(Image &dst, int x, int y, int w, int h, uint32_t color) {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(dst.w, x + w), y1 = std::min(dst.h, y + h);
    for (int yy = y0; yy < y1; yy++) std::fill(dst.row(yy) + x0, dst.row(yy) + std::max(x0, x1), color);
}

void multiplyRect(Image &dst, int x, int y, int w, int h, uint32_t color) {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(dst.w, x + w), y1 = std::min(dst.h, y + h);
    int cr = chR(color), cg = chG(color), cb = chB(color);
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *d = dst.row(yy);
        for (int xx = x0; xx < x1; xx++) {
            uint32_t p = d[xx];
            d[xx] = rgba(int(mul255(chR(p), cr)), int(mul255(chG(p), cg)), int(mul255(chB(p), cb)), chA(p));
        }
    }
}

bool decodeImage(const Bytes &data, Image &out, bool *hasAlpha) {
    int w, h, comp;
    if (data.empty()) return false;
    unsigned char *pixels = stbi_load_from_memory(data.data(), int(data.size()), &w, &h, &comp, 4);
    if (!pixels) return false;
    out = Image(w, h);
    bool alpha = comp == 2 || comp == 4;
    const unsigned char *s = pixels;
    for (size_t i = 0; i < out.px.size(); i++, s += 4) {
        uint32_t a = alpha ? s[3] : 255;
        out.px[i] = rgba(int(mul255(s[0], a)), int(mul255(s[1], a)), int(mul255(s[2], a)), int(a));
    }
    stbi_image_free(pixels);
    if (hasAlpha) *hasAlpha = alpha;
    return true;
}

}  // namespace vnns
