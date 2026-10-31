// DrawGraph: drawing on sprite pictures, which every call names by sprite number. The operations and their formulas
// follow the SDK's IGraph and IGraph2 interface comments and the SACT2 reference's draw_graph page; the calls the SDK
// doesn't list were read from their names and argument names. Where the SDK is silent: colours, rates and alpha
// values are 0 to 255 and clamped to that range, a product of two of them divides by 255 rounding to the nearest,
// Satur clamps to 0..255, and rectangles are clipped to both pictures. A picture without an alpha map reads as
// opaque and gets an opaque one before a call writes to its alpha map. A source that is also the destination is read
// from a copy taken before the call.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <string>
#include <vector>

#include "game.h"

namespace s4 {
namespace {

Game &G() { return game(); }

uint8_t sat(int v) { return uint8_t(std::clamp(v, 0, 255)); }
int mul(int a, int b) { return (a * b + 127) / 255; }
uint8_t blend(int d, int s, int a) { return uint8_t((d * (255 - a) + s * a + 127) / 255); }
int screen(int d, int s) { return d + s - mul(d, s); }
int overlay(int d, int s) { return d < 128 ? mul(2 * d, s) : 255 - mul(2 * (255 - d), 255 - s); }
int arg(HllCall &c, int k) { return std::clamp(c.i(k), 0, 255); }
constexpr float kDeg = std::numbers::pi_v<float> / 180;

Surface *surf(int sp) {
    Sprite *s = G().sprites.find(sp);
    return s ? &s->writable() : nullptr;
}

void needAlpha(Surface &d) {
    if (d.hasAlpha) return;
    for (size_t i = 3; i < d.px.size(); i += 4) d.px[i] = 255;
    d.hasAlpha = true;
}

// A picture to read from, without copying a shared CG.
struct Src {
    const uint8_t *px = nullptr;
    int w = 0, h = 0;
    bool alpha = false;
    std::vector<uint8_t> copy;

    explicit operator bool() const { return px || !copy.empty(); }
    const uint8_t *at(int x, int y) const { return (copy.empty() ? px : copy.data()) + (size_t(y) * w + x) * 4; }
    int a(int x, int y) const { return alpha ? at(x, y)[3] : 255; }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
};

// Call surf() on the destination first, so a sprite showing a shared CG has its own copy to compare against.
Src source(int sp, const Surface *dst) {
    Src s;
    Sprite *sprite = G().sprites.find(sp);
    if (!sprite || !sprite->pixels() || sprite->width() <= 0 || sprite->height() <= 0) return s;
    s.w = sprite->width();
    s.h = sprite->height();
    s.alpha = sprite->hasAlpha();
    if (sprite->hasOwn && &sprite->own == dst) s.copy = sprite->own.px;
    else s.px = sprite->pixels();
    return s;
}

// ---- Rectangles: f(dest pixel, source pixel, source alpha) for each pixel pair ----

template <class F>
void eachPair(Surface &d, int dx, int dy, const Src &s, int sx, int sy, int w, int h, bool flipX, bool flipY, F f) {
    for (int y = 0; y < h; y++) {
        int ty = dy + y, fy = sy + (flipY ? h - 1 - y : y);
        if (ty < 0 || ty >= d.h || fy < 0 || fy >= s.h) continue;
        for (int x = 0; x < w; x++) {
            int tx = dx + x, fx = sx + (flipX ? w - 1 - x : x);
            if (tx < 0 || tx >= d.w || fx < 0 || fx >= s.w) continue;
            f(d.at(tx, ty), s.at(fx, fy), s.a(fx, fy));
        }
    }
}
// The usual layout: (nDest, nDx, nDy, nSrc, nSx, nSy, nWidth, nHeight, ...).
template <class F> void pairOp(HllCall &c, bool writesAlpha, F f, bool flipX = false, bool flipY = false) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Src s = source(c.i(3), d);
    if (!s) return;
    if (writesAlpha) needAlpha(*d);
    eachPair(*d, c.i(1), c.i(2), s, c.i(4), c.i(5), c.i(6), c.i(7), flipX, flipY, f);
}
// (nDest, nX, nY, nWidth, nHeight, ...): f(pixel, x, y) with x and y counted from the rectangle's corner.
template <class F> void fillOp(HllCall &c, bool writesAlpha, F f) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    if (writesAlpha) needAlpha(*d);
    int X = c.i(1), Y = c.i(2);
    int x0 = std::max(0, X), y0 = std::max(0, Y), x1 = std::min(d->w, X + c.i(3)), y1 = std::min(d->h, Y + c.i(4));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) f(d->at(x, y), x - X, y - Y);
}
void copy3(uint8_t *p, const uint8_t *s) { std::memcpy(p, s, 3); }
// Per colour channel: f(dest, source, source alpha) gives the new value.
template <class F> void pairRGB(HllCall &c, F f) {
    pairOp(c, false, [&f](uint8_t *p, const uint8_t *s, int a) {
        for (int k = 0; k < 3; k++) p[k] = sat(f(p[k], s[k], a));
    });
}
// Per colour channel with the colour at arguments 5 to 7: f(dest, colour) gives the new value.
template <class F> void fillRGB(HllCall &c, F f) {
    int col[3] = {arg(c, 5), arg(c, 6), arg(c, 7)};
    fillOp(c, false, [&](uint8_t *p, int, int) { for (int k = 0; k < 3; k++) p[k] = sat(f(p[k], col[k])); });
}
void putAlpha(uint8_t *p, const uint8_t *, int a) { p[3] = uint8_t(a); }
void putPixel(uint8_t *p, const uint8_t *s, int) { copy3(p, s); }

S4_HLL(DrawGraph, Copy) { pairOp(c, false, putPixel); }
S4_HLL(DrawGraph, CopyAMap) { pairOp(c, true, putAlpha); }
S4_HLL(DrawGraph, CopyWithAlphaMap) {
    pairOp(c, true, [](uint8_t *p, const uint8_t *s, int a) { copy3(p, s); p[3] = uint8_t(a); });
}
S4_HLL(DrawGraph, CopyBright) {
    int r = arg(c, 8);
    pairRGB(c, [r](int, int s, int) { return mul(s, r); });
}
S4_HLL(DrawGraph, CopySprite) {
    int r = arg(c, 8), g = arg(c, 9), b = arg(c, 10);
    pairOp(c, false, [=](uint8_t *p, const uint8_t *s, int) { if (s[0] != r || s[1] != g || s[2] != b) copy3(p, s); });
}
S4_HLL(DrawGraph, CopyColorReverse) { pairRGB(c, [](int, int s, int) { return 255 - s; }); }
// Not in the SDK. Luma with the ITU-R BT.601 weights.
S4_HLL(DrawGraph, CopyGrayscale) {
    pairOp(c, false, [](uint8_t *p, const uint8_t *s, int) {
        p[0] = p[1] = p[2] = uint8_t((s[0] * 299 + s[1] * 587 + s[2] * 114 + 500) / 1000);
    });
}
S4_HLL(DrawGraph, CopyUseAMapUnder) {
    int n = c.i(8);
    pairOp(c, false, [n](uint8_t *p, const uint8_t *s, int a) { if (a < n) copy3(p, s); });
}
S4_HLL(DrawGraph, CopyUseAMapBorder) {
    int n = c.i(8);
    pairOp(c, false, [n](uint8_t *p, const uint8_t *s, int a) { if (a >= n) copy3(p, s); });
}
// Where the source's alpha is the greater (or the lesser), the pixel and its alpha both come over.
S4_HLL(DrawGraph, CopyAMapMax) {
    pairOp(c, true, [](uint8_t *p, const uint8_t *s, int a) { if (a > p[3]) { copy3(p, s); p[3] = uint8_t(a); } });
}
S4_HLL(DrawGraph, CopyAMapMin) {
    pairOp(c, true, [](uint8_t *p, const uint8_t *s, int a) { if (a < p[3]) { copy3(p, s); p[3] = uint8_t(a); } });
}
S4_HLL(DrawGraph, CopyReverseLR) { pairOp(c, false, putPixel, true, false); }
S4_HLL(DrawGraph, CopyReverseUD) { pairOp(c, false, putPixel, false, true); }
S4_HLL(DrawGraph, CopyReverseAMapLR) { pairOp(c, true, putAlpha, true, false); }
S4_HLL(DrawGraph, CopyReverseAMapUD) { pairOp(c, true, putAlpha, false, true); }

// Blends by a rate, not the alpha map.
S4_HLL(DrawGraph, Blend) {
    int al = arg(c, 8);
    pairRGB(c, [al](int d, int s, int) { return blend(d, s, al); });
}
S4_HLL(DrawGraph, BlendSrcBright) {
    int al = arg(c, 8), r = arg(c, 9);
    pairRGB(c, [=](int d, int s, int) { return blend(d, mul(s, r), al); });
}
S4_HLL(DrawGraph, BlendAddSatur) { pairRGB(c, [](int d, int s, int) { return d + s; }); }
// DestPixel = Blend(DestPixel, SrcPixel, SrcAlphaMap) and its variants.
S4_HLL(DrawGraph, BlendAMap) { pairRGB(c, [](int d, int s, int a) { return blend(d, s, a); }); }
S4_HLL(DrawGraph, BlendAMapSrcOnly) { pairRGB(c, [](int d, int s, int a) { return d + mul(s, a); }); }
S4_HLL(DrawGraph, BlendAMapColor) {
    int col[3] = {arg(c, 8), arg(c, 9), arg(c, 10)};
    pairOp(c, false, [&col](uint8_t *p, const uint8_t *, int a) {
        for (int k = 0; k < 3; k++) p[k] = blend(p[k], col[k], a);
    });
}
void blendAMapColorAlpha(HllCall &c) {
    int col[3] = {arg(c, 8), arg(c, 9), arg(c, 10)}, al = arg(c, 11);
    pairOp(c, false, [&col, al](uint8_t *p, const uint8_t *, int a) {
        for (int k = 0; k < 3; k++) p[k] = blend(p[k], col[k], mul(a, al));
    });
}
S4_HLL(DrawGraph, BlendAMapColorAlpha) { blendAMapColorAlpha(c); }
// (nDest, nDx, nDy, nAlpha, nAx, nAy, nWidth, nHeight, nR, nG, nB, nRate): nAlpha is the sprite whose alpha map is
// used, so this is BlendAMapColorAlpha with the same argument positions.
S4_HLL(DrawGraph, BlendUseAMapColor) { blendAMapColorAlpha(c); }
S4_HLL(DrawGraph, BlendAMapAlpha) {
    int al = arg(c, 8);
    pairRGB(c, [al](int d, int s, int a) { return blend(d, s, mul(a, al)); });
}
S4_HLL(DrawGraph, BlendAMapBright) {
    int r = arg(c, 8);
    pairRGB(c, [r](int d, int s, int a) { return blend(d, mul(s, r), a); });
}
S4_HLL(DrawGraph, BlendAMapAlphaSrcBright) {
    int al = arg(c, 8), r = arg(c, 9);
    pairRGB(c, [=](int d, int s, int a) { return blend(d, mul(s, r), mul(a, al)); });
}
// The sprite engine's, which SACT2 games reach too: BlendAMap with the alpha map read as two values, none of the
// source below nBorder and all of it from nBorder up.
S4_HLL(StoatSpriteEngine, CG_BlendAMapBin) {
    int border = c.i(8);
    pairOp(c, false, [border](uint8_t *p, const uint8_t *s, int a) { if (a >= border) copy3(p, s); });
}
// The Photoshop filters use the source's pixels only, not its alpha map.
S4_HLL(DrawGraph, BlendScreen) { pairRGB(c, [](int d, int s, int) { return screen(d, s); }); }
S4_HLL(DrawGraph, BlendMultiply) { pairRGB(c, [](int d, int s, int) { return mul(d, s); }); }
S4_HLL(DrawGraph, BlendScreenAlpha) {
    int al = arg(c, 8);
    pairRGB(c, [al](int d, int s, int) { return blend(d, screen(d, s), al); });
}
// DestPixel = Satur(DestPixel + SrcAlpha), the source's alpha added to each colour channel.
S4_HLL(DrawGraph, SaturDP_DPxSA) { pairRGB(c, [](int d, int, int a) { return d + a; }); }
S4_HLL(DrawGraph, ScreenDA_DAxSA) {
    pairOp(c, true, [](uint8_t *p, const uint8_t *, int a) { p[3] = uint8_t(screen(p[3], a)); });
}
S4_HLL(DrawGraph, AddDA_DAxSA) { pairOp(c, true, [](uint8_t *p, const uint8_t *, int a) { p[3] = sat(p[3] + a); }); }
S4_HLL(DrawGraph, SubDA_DAxSA) { pairOp(c, true, [](uint8_t *p, const uint8_t *, int a) { p[3] = sat(p[3] - a); }); }
// The SDK gives no formula. Read like BlendAMap on the alpha map: DestAlpha = Blend(DestAlpha, SrcAlpha, SrcAlpha).
S4_HLL(DrawGraph, BlendDA_DAxSA) { pairOp(c, true, [](uint8_t *p, const uint8_t *, int a) { p[3] = blend(p[3], a, a); }); }
// The colour key is an alpha value: source alpha equal to it leaves the destination's alpha.
S4_HLL(DrawGraph, SpriteCopyAMap) {
    int key = c.i(8);
    pairOp(c, true, [key](uint8_t *p, const uint8_t *, int a) { if (a != key) p[3] = uint8_t(a); });
}

// ---- Fills ----

S4_HLL(DrawGraph, Fill) { fillRGB(c, [](int, int col) { return col; }); }
S4_HLL(DrawGraph, FillAMap) {
    uint8_t a = uint8_t(arg(c, 5));
    fillOp(c, true, [a](uint8_t *p, int, int) { p[3] = a; });
}
S4_HLL(DrawGraph, FillWithAlpha) {
    uint8_t col[4] = {uint8_t(arg(c, 5)), uint8_t(arg(c, 6)), uint8_t(arg(c, 7)), uint8_t(arg(c, 8))};
    fillOp(c, true, [&col](uint8_t *p, int, int) { std::memcpy(p, col, 4); });
}
S4_HLL(DrawGraph, FillAlphaColor) {
    int r = arg(c, 8);
    fillRGB(c, [r](int d, int col) { return blend(d, col, r); });
}
// Over takes the border value itself and Under doesn't, so the two split every alpha value between them.
S4_HLL(DrawGraph, FillAMapOverBorder) {
    uint8_t a = uint8_t(arg(c, 5));
    int border = c.i(6);
    fillOp(c, true, [=](uint8_t *p, int, int) { if (p[3] >= border) p[3] = a; });
}
S4_HLL(DrawGraph, FillAMapUnderBorder) {
    uint8_t a = uint8_t(arg(c, 5));
    int border = c.i(6);
    fillOp(c, true, [=](uint8_t *p, int, int) { if (p[3] < border) p[3] = a; });
}
// nUpA on the top row to nDownA on the bottom row of the whole rectangle, before clipping.
S4_HLL(DrawGraph, FillAMapGradationUD) {
    int up = arg(c, 5), down = arg(c, 6), n = std::max(1, c.i(4) - 1);
    fillOp(c, true, [=](uint8_t *p, int, int y) { p[3] = uint8_t((up * (n - y) + down * y + n / 2) / n); });
}
S4_HLL(DrawGraph, FillScreen) { fillRGB(c, [](int d, int col) { return screen(d, col); }); }
S4_HLL(DrawGraph, FillMultiply) { fillRGB(c, [](int d, int col) { return mul(d, col); }); }
// Not in the SDK. The Photoshop overlay with the picture as the base layer.
S4_HLL(DrawGraph, FillOverlay) { fillRGB(c, [](int d, int col) { return overlay(d, col); }); }
S4_HLL(DrawGraph, FillOverlayAlpha) {
    int al = arg(c, 8);
    fillRGB(c, [al](int d, int col) { return blend(d, overlay(d, col), al); });
}
S4_HLL(DrawGraph, BrightDestOnly) {
    int r = arg(c, 5);
    fillOp(c, false, [r](uint8_t *p, int, int) { for (int k = 0; k < 3; k++) p[k] = uint8_t(mul(p[k], r)); });
}

// ---- Scaling: (nDest, nDx, nDy, nDWidth, nDHeight, nSrc, nSx, nSy, nSWidth, nSHeight, ...) ----

enum class Filter { Nearest, Bilinear, Box };

// Writes f(dest pixel, value) for each pixel of the destination rectangle, the value being the source rectangle
// scaled to its size: RGBA, with alpha 255 for a source without an alpha map. Samples stay inside the source
// rectangle. Box averages the source pixels each destination pixel covers, for reducing.
template <class F> void stretchOp(HllCall &c, Filter filter, bool writesAlpha, F f) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Src s = source(c.i(5), d);
    if (!s) return;
    int dx = c.i(1), dy = c.i(2), dw = c.i(3), dh = c.i(4), sx = c.i(6), sy = c.i(7), sw = c.i(8), sh = c.i(9);
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    int cx0 = std::max(0, sx), cy0 = std::max(0, sy), cx1 = std::min(s.w, sx + sw), cy1 = std::min(s.h, sy + sh);
    if (cx0 >= cx1 || cy0 >= cy1) return;
    if (writesAlpha) needAlpha(*d);
    auto px = [&](int x, int y, int ch) { return ch == 3 ? s.a(x, y) : s.at(x, y)[ch]; };
    for (int y = std::max(0, -dy); y < dh && dy + y < d->h; y++)
        for (int x = std::max(0, -dx); x < dw && dx + x < d->w; x++) {
            int v[4];
            if (filter == Filter::Box) {
                int u0 = std::clamp(sx + int(int64_t(x) * sw / dw), cx0, cx1 - 1);
                int u1 = std::clamp(sx + int((int64_t(x + 1) * sw + dw - 1) / dw), u0 + 1, cx1);
                int v0 = std::clamp(sy + int(int64_t(y) * sh / dh), cy0, cy1 - 1);
                int v1 = std::clamp(sy + int((int64_t(y + 1) * sh + dh - 1) / dh), v0 + 1, cy1);
                int n = (u1 - u0) * (v1 - v0);
                for (int ch = 0; ch < 4; ch++) {
                    int sum = 0;
                    for (int yy = v0; yy < v1; yy++)
                        for (int xx = u0; xx < u1; xx++) sum += px(xx, yy, ch);
                    v[ch] = (sum + n / 2) / n;
                }
            } else if (filter == Filter::Nearest) {
                int u = std::clamp(sx + int(int64_t(2 * x + 1) * sw / (2 * dw)), cx0, cx1 - 1);
                int w = std::clamp(sy + int(int64_t(2 * y + 1) * sh / (2 * dh)), cy0, cy1 - 1);
                for (int ch = 0; ch < 4; ch++) v[ch] = px(u, w, ch);
            } else {
                float fx = sx + (x + 0.5f) * sw / dw - 0.5f, fy = sy + (y + 0.5f) * sh / dh - 0.5f;
                int x0 = std::clamp(int(std::floor(fx)), cx0, cx1 - 1), x1 = std::min(x0 + 1, cx1 - 1);
                int y0 = std::clamp(int(std::floor(fy)), cy0, cy1 - 1), y1 = std::min(y0 + 1, cy1 - 1);
                float wx = std::clamp(fx - x0, 0.0f, 1.0f), wy = std::clamp(fy - y0, 0.0f, 1.0f);
                for (int ch = 0; ch < 4; ch++) {
                    float t = px(x0, y0, ch) * (1 - wx) + px(x1, y0, ch) * wx;
                    float b = px(x0, y1, ch) * (1 - wx) + px(x1, y1, ch) * wx;
                    v[ch] = int(t * (1 - wy) + b * wy + 0.5f);
                }
            }
            f(d->at(dx + x, dy + y), v);
        }
}
void putRGB(uint8_t *p, const int *v) { for (int k = 0; k < 3; k++) p[k] = uint8_t(v[k]); }
void putA(uint8_t *p, const int *v) { p[3] = uint8_t(v[3]); }
void putRGBA(uint8_t *p, const int *v) { for (int k = 0; k < 4; k++) p[k] = uint8_t(v[k]); }
void blendRGB(uint8_t *p, const int *v) { for (int k = 0; k < 3; k++) p[k] = blend(p[k], v[k], v[3]); }

S4_HLL(DrawGraph, CopyStretch) { stretchOp(c, Filter::Nearest, false, putRGB); }
S4_HLL(DrawGraph, CopyStretchAMap) { stretchOp(c, Filter::Nearest, true, putA); }
S4_HLL(DrawGraph, CopyStretchInterp) { stretchOp(c, Filter::Bilinear, false, putRGB); }
S4_HLL(DrawGraph, CopyStretchAMapInterp) { stretchOp(c, Filter::Bilinear, true, putA); }
S4_HLL(DrawGraph, CopyReduce) { stretchOp(c, Filter::Box, false, putRGB); }
S4_HLL(DrawGraph, CopyReduceAMap) { stretchOp(c, Filter::Box, true, putA); }
S4_HLL(DrawGraph, CopyStretchWithAlphaMap) { stretchOp(c, Filter::Nearest, true, putRGBA); }
S4_HLL(DrawGraph, CopyStretchBilinearWithAlphaMap) { stretchOp(c, Filter::Bilinear, true, putRGBA); }
S4_HLL(DrawGraph, CopyStretchBlend) {
    int al = arg(c, 10);
    stretchOp(c, Filter::Nearest, false, [al](uint8_t *p, const int *v) {
        for (int k = 0; k < 3; k++) p[k] = blend(p[k], v[k], al);
    });
}
S4_HLL(DrawGraph, CopyStretchBlendAMap) { stretchOp(c, Filter::Nearest, false, blendRGB); }

// ---- Texture wrap: the source rectangle tiled over the destination rectangle, starting (nU, nV) into it ----

template <class F> void wrapOp(HllCall &c, F f) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Src s = source(c.i(5), d);
    if (!s) return;
    int dx = c.i(1), dy = c.i(2), dw = c.i(3), dh = c.i(4);
    int sx = std::max(0, c.i(6)), sy = std::max(0, c.i(7));
    int sw = std::min(s.w, c.i(6) + c.i(8)) - sx, sh = std::min(s.h, c.i(7) + c.i(9)) - sy;
    if (sw <= 0 || sh <= 0) return;
    int u = c.i(10), v = c.i(11);
    auto wrap = [](int a, int n) { return (a % n + n) % n; };
    for (int y = std::max(0, -dy); y < dh && dy + y < d->h; y++)
        for (int x = std::max(0, -dx); x < dw && dx + x < d->w; x++)
            f(d->at(dx + x, dy + y), s.at(sx + wrap(x + u, sw), sy + wrap(y + v, sh)));
}
S4_HLL(DrawGraph, CopyTextureWrap) { wrapOp(c, copy3); }
S4_HLL(DrawGraph, CopyTextureWrapAlpha) {
    int al = arg(c, 12);
    wrapOp(c, [al](uint8_t *p, const uint8_t *s) { for (int k = 0; k < 3; k++) p[k] = blend(p[k], s[k], al); });
}

// ---- Rotation ----

// Fills each pixel of d whose centre maps, through inv, inside the source rectangle (sx, sy, w, h) clipped to the
// source: f(dest pixel, RGBA value). Nearest sampling, or bilinear inside the rectangle.
template <class Inv, class F>
void mapBack(Surface &d, const Src &s, int sx, int sy, int w, int h, bool bilinear, Inv inv, F f) {
    int x0 = std::max(0, sx), y0 = std::max(0, sy), x1 = std::min(s.w, sx + w), y1 = std::min(s.h, sy + h);
    if (x0 >= x1 || y0 >= y1) return;
    auto px = [&](int x, int y, int ch) { return ch == 3 ? s.a(x, y) : s.at(x, y)[ch]; };
    for (int y = 0; y < d.h; y++)
        for (int x = 0; x < d.w; x++) {
            float u, v;
            inv(x + 0.5f, y + 0.5f, u, v);
            if (!(u >= x0 && u < x1 && v >= y0 && v < y1)) continue;
            int val[4];
            if (bilinear) {
                float fx = u - 0.5f, fy = v - 0.5f;
                int ax = std::clamp(int(std::floor(fx)), x0, x1 - 1), bx = std::min(ax + 1, x1 - 1);
                int ay = std::clamp(int(std::floor(fy)), y0, y1 - 1), by = std::min(ay + 1, y1 - 1);
                float wx = std::clamp(fx - ax, 0.0f, 1.0f), wy = std::clamp(fy - ay, 0.0f, 1.0f);
                for (int ch = 0; ch < 4; ch++) {
                    float t = px(ax, ay, ch) * (1 - wx) + px(bx, ay, ch) * wx;
                    float b = px(ax, by, ch) * (1 - wx) + px(bx, by, ch) * wx;
                    val[ch] = int(t * (1 - wy) + b * wy + 0.5f);
                }
            } else {
                for (int ch = 0; ch < 4; ch++) val[ch] = px(int(u), int(v), ch);
            }
            f(d.at(x, y), val);
        }
}

// fRotate is in degrees: Test Game 46 turns a damage number by -180 * t / 200 as it fades. A positive angle turns
// clockwise on screen. The source rectangle's centre lands on the destination's centre, and the destination's other
// pixels stay as they were.
template <class F> void rotZoom(HllCall &c, bool writesAlpha, F f) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Src s = source(c.i(1), d);
    float mag = c.f(7);
    if (!s || !(mag > 0)) return;
    int sx = c.i(2), sy = c.i(3), w = c.i(4), h = c.i(5);
    float rad = c.f(6) * kDeg, cs = std::cos(rad), sn = std::sin(rad);
    float cx = d->w / 2.0f, cy = d->h / 2.0f, ucx = sx + w / 2.0f, ucy = sy + h / 2.0f;
    if (writesAlpha) needAlpha(*d);
    mapBack(*d, s, sx, sy, w, h, false, [=](float x, float y, float &u, float &v) {
        float rx = x - cx, ry = y - cy;
        u = ucx + (cs * rx + sn * ry) / mag;
        v = ucy + (-sn * rx + cs * ry) / mag;
    }, f);
}
S4_HLL(DrawGraph, CopyRotZoom) { rotZoom(c, false, putRGB); }
S4_HLL(DrawGraph, CopyRotZoomAMap) { rotZoom(c, true, putA); }
S4_HLL(DrawGraph, CopyRotZoomUseAMap) { rotZoom(c, false, blendRGB); }
// Not in the SDK. (nDest, fCx, fCy, nSrc, fSrcCx, fSrcCy, fRotate, fMag): the whole source, its point (fSrcCx,
// fSrcCy) on the destination's (fCx, fCy), sampled bilinearly. Pixels only, as CopyRotZoom.
S4_HLL(DrawGraph, CopyRotZoom2Bilinear) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Src s = source(c.i(3), d);
    float mag = c.f(7);
    if (!s || !(mag > 0)) return;
    float cx = c.f(1), cy = c.f(2), ucx = c.f(4), ucy = c.f(5);
    float rad = c.f(6) * kDeg, cs = std::cos(rad), sn = std::sin(rad);
    mapBack(*d, s, 0, 0, s.w, s.h, true, [=](float x, float y, float &u, float &v) {
        float rx = x - cx, ry = y - cy;
        u = ucx + (cs * rx + sn * ry) / mag;
        v = ucy + (-sn * rx + cs * ry) / mag;
    }, putRGB);
}

// (nWrite, nDest, nSrc, nSx, nSy, nWidth, nHeight, fRotate, fMag): nWrite gets nDest's pixels with the source
// rectangle drawn over them, turned fRotate degrees about a vertical (Y) or horizontal (X) axis. Drawn without
// perspective: the picture narrows by cos(fRotate) and shows mirrored past 90 degrees. The unturned picture sits
// centred on nWrite; the axis runs through its centre, or along the edge the Fix variants name. fMag scales it about
// the same axis. Only the pixels change, not nWrite's alpha map.
void rotateAxis(HllCall &c, bool yAxis, int fix, bool useAMap) {
    Surface *wr = surf(c.i(0));
    if (!wr) return;
    if (c.i(1) != c.i(0)) {
        Src back = source(c.i(1), wr);
        if (!back) return;
        for (int y = 0; y < std::min(wr->h, back.h); y++)
            for (int x = 0; x < std::min(wr->w, back.w); x++) copy3(wr->at(x, y), back.at(x, y));
    }
    Src s = source(c.i(2), wr);
    float mag = c.f(8);
    if (!s || !(mag > 0)) return;
    int sx = c.i(3), sy = c.i(4), w = c.i(5), h = c.i(6);
    float k = std::cos(c.f(7) * kDeg) * mag;
    float kx = yAxis ? k : mag, ky = yAxis ? mag : k;
    if (std::fabs(k) < 1e-4f) return;  // edge on
    float pu = w / 2.0f, pv = h / 2.0f;  // the axis, in the rectangle
    if (yAxis && fix) pu = fix < 0 ? 0 : float(w);
    if (!yAxis && fix) pv = fix < 0 ? 0 : float(h);
    float px = (wr->w - w) / 2.0f + pu, py = (wr->h - h) / 2.0f + pv;
    auto inv = [=](float x, float y, float &u, float &v) {
        u = sx + pu + (x - px) / kx;
        v = sy + pv + (y - py) / ky;
    };
    if (useAMap) mapBack(*wr, s, sx, sy, w, h, false, inv, blendRGB);
    else mapBack(*wr, s, sx, sy, w, h, false, inv, putRGB);
}
S4_HLL(DrawGraph, CopyRotateY) { rotateAxis(c, true, 0, false); }
S4_HLL(DrawGraph, CopyRotateYUseAMap) { rotateAxis(c, true, 0, true); }
S4_HLL(DrawGraph, CopyRotateYFixL) { rotateAxis(c, true, -1, false); }
S4_HLL(DrawGraph, CopyRotateYFixR) { rotateAxis(c, true, 1, false); }
S4_HLL(DrawGraph, CopyRotateYFixLUseAMap) { rotateAxis(c, true, -1, true); }
S4_HLL(DrawGraph, CopyRotateYFixRUseAMap) { rotateAxis(c, true, 1, true); }
S4_HLL(DrawGraph, CopyRotateX) { rotateAxis(c, false, 0, false); }
S4_HLL(DrawGraph, CopyRotateXUseAMap) { rotateAxis(c, false, 0, true); }
S4_HLL(DrawGraph, CopyRotateXFixU) { rotateAxis(c, false, -1, false); }
S4_HLL(DrawGraph, CopyRotateXFixD) { rotateAxis(c, false, 1, false); }
S4_HLL(DrawGraph, CopyRotateXFixUUseAMap) { rotateAxis(c, false, -1, true); }
S4_HLL(DrawGraph, CopyRotateXFixDUseAMap) { rotateAxis(c, false, 1, true); }

// ---- Blur: a box of 2 * nBlur + 1 source pixels across or down, the edge pixels of the source rectangle repeated ----

void blurOp(HllCall &c, bool across, bool alpha) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Src s = source(c.i(3), d);
    if (!s) return;
    int dx = c.i(1), dy = c.i(2), sx = c.i(4), sy = c.i(5), w = c.i(6), h = c.i(7), r = std::max(0, c.i(8));
    int x0 = std::max(0, sx), y0 = std::max(0, sy), x1 = std::min(s.w, sx + w), y1 = std::min(s.h, sy + h);
    if (x0 >= x1 || y0 >= y1) return;
    if (alpha) needAlpha(*d);
    int n = 2 * r + 1;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int tx = dx + x, ty = dy + y, fx = sx + x, fy = sy + y;
            if (tx < 0 || ty < 0 || tx >= d->w || ty >= d->h || fx < x0 || fy < y0 || fx >= x1 || fy >= y1) continue;
            int sum[4] = {};
            for (int k = -r; k <= r; k++) {
                int u = across ? std::clamp(fx + k, x0, x1 - 1) : fx, v = across ? fy : std::clamp(fy + k, y0, y1 - 1);
                for (int ch = 0; ch < 3; ch++) sum[ch] += s.at(u, v)[ch];
                sum[3] += s.a(u, v);
            }
            uint8_t *p = d->at(tx, ty);
            if (alpha) p[3] = uint8_t((sum[3] + n / 2) / n);
            else for (int ch = 0; ch < 3; ch++) p[ch] = uint8_t((sum[ch] + n / 2) / n);
        }
}
S4_HLL(DrawGraph, CopyWidthBlur) { blurOp(c, true, false); }
S4_HLL(DrawGraph, CopyHeightBlur) { blurOp(c, false, false); }
S4_HLL(DrawGraph, CopyAMapWidthBlur) { blurOp(c, true, true); }
S4_HLL(DrawGraph, CopyAMapHeightBlur) { blurOp(c, false, true); }

// ---- Lines, points and polygons ----

template <class F> void line(HllCall &c, bool writesAlpha, F f) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    if (writesAlpha) needAlpha(*d);
    int x0 = c.i(1), y0 = c.i(2), x1 = c.i(3), y1 = c.i(4);
    int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int k = 0; k <= n; k++) {
        int x = n ? x0 + (x1 - x0) * k / n : x0, y = n ? y0 + (y1 - y0) * k / n : y0;
        if (x >= 0 && y >= 0 && x < d->w && y < d->h) f(d->at(x, y));
    }
}
S4_HLL(DrawGraph, DrawLine) {
    uint8_t col[3] = {uint8_t(arg(c, 5)), uint8_t(arg(c, 6)), uint8_t(arg(c, 7))};
    line(c, false, [&col](uint8_t *p) { copy3(p, col); });
}
S4_HLL(DrawGraph, DrawLineToAMap) {
    uint8_t a = uint8_t(arg(c, 5));
    line(c, true, [a](uint8_t *p) { p[3] = a; });
}
// False, with the references left alone, for a missing sprite or a point outside it.
S4_HLL(DrawGraph, GetPixelColor) {
    Src s = source(c.i(0), nullptr);
    int x = c.i(1), y = c.i(2);
    if (!s || !s.inside(x, y)) { c.retBool(false); return; }
    c.ref(3) = s.at(x, y)[0];
    c.ref(4) = s.at(x, y)[1];
    c.ref(5) = s.at(x, y)[2];
    c.retBool(true);
}
S4_HLL(DrawGraph, GetAlphaColor) {
    Src s = source(c.i(0), nullptr);
    int x = c.i(1), y = c.i(2);
    if (!s || !s.inside(x, y)) { c.retBool(false); return; }
    c.ref(3) = s.a(x, y);
    c.retBool(true);
}

// Not in the SDK. Fills a triangle's pixels whose centres fall inside it (edges included), either winding, with
// f(pixel, b0, b1, b2) given the centre's barycentric weights. Z is ignored: the triangle is drawn flat.
template <class F> void triangle(Surface &d, const float (&px)[3], const float (&py)[3], F f) {
    float area = (px[1] - px[0]) * (py[2] - py[0]) - (px[2] - px[0]) * (py[1] - py[0]);
    if (std::fabs(area) < 1e-6f) return;
    auto [xmin, xmax] = std::minmax({px[0], px[1], px[2]});
    auto [ymin, ymax] = std::minmax({py[0], py[1], py[2]});
    int x0 = std::max(0, int(std::floor(xmin))), x1 = std::min(d.w - 1, int(std::ceil(xmax)));
    int y0 = std::max(0, int(std::floor(ymin))), y1 = std::min(d.h - 1, int(std::ceil(ymax)));
    auto edge = [&](int a, int b, float x, float y) {
        return ((px[b] - px[a]) * (y - py[a]) - (x - px[a]) * (py[b] - py[a])) / area;
    };
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            float cx = x + 0.5f, cy = y + 0.5f;
            float b0 = edge(1, 2, cx, cy), b1 = edge(2, 0, cx, cy), b2 = edge(0, 1, cx, cy);
            if (b0 >= 0 && b1 >= 0 && b2 >= 0) f(d.at(x, y), b0, b1, b2);
        }
}
// (nDest, nTex, then fX, fY, fZ, fU, fV for each vertex). U and V run 0 to 1 across the texture, which repeats past
// that. The texture's pixels are copied, nearest sampled, without its alpha map.
S4_HLL(DrawGraph, DrawPolygon) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Src t = source(c.i(1), d);
    if (!t) return;
    float px[3], py[3], pu[3], pv[3];
    for (int k = 0; k < 3; k++) {
        px[k] = c.f(2 + 5 * k);
        py[k] = c.f(3 + 5 * k);
        pu[k] = c.f(5 + 5 * k);
        pv[k] = c.f(6 + 5 * k);
    }
    triangle(*d, px, py, [&](uint8_t *p, float b0, float b1, float b2) {
        float u = (b0 * pu[0] + b1 * pu[1] + b2 * pu[2]) * t.w, v = (b0 * pv[0] + b1 * pv[1] + b2 * pv[2]) * t.h;
        int tx = (int(std::floor(u)) % t.w + t.w) % t.w, ty = (int(std::floor(v)) % t.h + t.h) % t.h;
        copy3(p, t.at(tx, ty));
    });
}
// (nDest, then fX, fY, fZ, nR, nG, nB, nA for each vertex): colour and alpha shaded across the triangle and written
// to the pixels and the alpha map, as FillWithAlpha writes both.
S4_HLL(DrawGraph, DrawColorPolygon) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    needAlpha(*d);
    float px[3], py[3];
    int col[3][4];
    for (int k = 0; k < 3; k++) {
        px[k] = c.f(1 + 7 * k);
        py[k] = c.f(2 + 7 * k);
        for (int ch = 0; ch < 4; ch++) col[k][ch] = arg(c, 4 + 7 * k + ch);
    }
    triangle(*d, px, py, [&](uint8_t *p, float b0, float b1, float b2) {
        for (int ch = 0; ch < 4; ch++) p[ch] = sat(int(b0 * col[0][ch] + b1 * col[1][ch] + b2 * col[2][ch] + 0.5f));
    });
}

// ---- Text ----

struct DrawFont {
    TextStyle st;
    std::string name = "\x82\x6c\x82\x72 \x83\x53\x83\x56\x83\x62\x83\x4e";  // ＭＳ ゴシック, the SDK's default
    int underline = 0, strikeOut = 0, space = 0;
};
DrawFont g_font;

S4_HLL_RESET { g_font = {}; }
S4_HLL(DrawGraph, SetFontSize) { g_font.st.size = c.i(0); }
S4_HLL(DrawGraph, SetFontName) {
    g_font.name = c.s(0);
    g_font.st.face = g_font.name.find("\x96\xbe\x92\xa9") != std::string::npos ? 1 : 0;  // 明朝
}
S4_HLL(DrawGraph, SetFontWeight) { g_font.st.weight = c.i(0); }
S4_HLL(DrawGraph, SetFontUnderline) { g_font.underline = c.i(0); }
S4_HLL(DrawGraph, SetFontStrikeOut) { g_font.strikeOut = c.i(0); }
S4_HLL(DrawGraph, SetFontSpace) { g_font.space = c.i(0); }
S4_HLL(DrawGraph, SetFontColor) { g_font.st.r = c.i(0); g_font.st.g = c.i(1); g_font.st.b = c.i(2); }
S4_HLL(DrawGraph, GetFontSize) { c.retInt(g_font.st.size); }
S4_HLL(DrawGraph, GetFontName) { c.retString(g_font.name); }
S4_HLL(DrawGraph, GetFontWeight) { c.retInt(g_font.st.weight); }
S4_HLL(DrawGraph, GetFontUnderline) { c.retInt(g_font.underline); }
S4_HLL(DrawGraph, GetFontStrikeOut) { c.retInt(g_font.strikeOut); }
S4_HLL(DrawGraph, GetFontSpace) { c.retInt(g_font.space); }
S4_HLL(DrawGraph, GetFontColor) { c.ref(0) = g_font.st.r; c.ref(1) = g_font.st.g; c.ref(2) = g_font.st.b; }

// The text on a clear layer the size of the picture. Underline and strike-out are lines size / 16 thick (at least
// one pixel), along the bottom of the line and through its middle.
Surface textLayer(const Surface &d, int x, int y, const std::string &text) {
    Surface layer;
    layer.create(d.w, d.h, true);
    int w = drawText(layer, x, y, text, g_font.st, g_font.space);
    int t = std::max(1, g_font.st.size / 16);
    auto bar = [&](int top) { layer.fill(x, top, w, t, g_font.st.r, g_font.st.g, g_font.st.b, 255); };
    if (g_font.underline) bar(y + g_font.st.size - t);
    if (g_font.strikeOut) bar(y + (g_font.st.size - t) / 2);
    return layer;
}
// Text drawn into the pixels keeps the alpha map; text drawn into the alpha map writes the coverage there.
S4_HLL(DrawGraph, DrawTextToPMap) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Surface layer = textLayer(*d, c.i(1), c.i(2), c.s(3));
    for (size_t i = 0; i < layer.px.size(); i += 4)
        if (int a = layer.px[i + 3])
            for (int k = 0; k < 3; k++) d->px[i + k] = blend(d->px[i + k], layer.px[i + k], a);
}
S4_HLL(DrawGraph, DrawTextToAMap) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Surface layer = textLayer(*d, c.i(1), c.i(2), c.s(3));
    needAlpha(*d);
    for (size_t i = 3; i < layer.px.size(); i += 4) d->px[i] = std::max(d->px[i], layer.px[i]);
}

}  // namespace
}  // namespace s4
