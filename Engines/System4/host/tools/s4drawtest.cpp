// Checks the DrawGraph library's pixel operations on tiny sprites against the SDK's formulas, calling the registered
// HLL functions the way the VM does. Exits non-zero if any check fails.
// Usage: s4drawtest
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "game.h"

using namespace s4;

namespace {

int g_checks = 0, g_failed = 0;
VM *g_vm = nullptr;
int32_t g_refs = -1;  // a page whose words stand in for ref int variables

void check(bool ok, const char *what, int got, int want) {
    g_checks++;
    if (ok) return;
    g_failed++;
    std::printf("FAIL %s: got %d, want %d\n", what, got, want);
}
void expect(const char *what, int got, int want) { check(got == want, what, got, want); }

// The SDK's formulas written out in floating point, independent of the runtime's integer code.
int mulF(int a, int b) { return int(std::lround(a * b / 255.0)); }
int blendF(int d, int s, int a) { return int(std::lround((d * (255 - a) + s * a) / 255.0)); }
int satF(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

struct Arg {
    enum Kind { Int, Float, Ref, Str } kind;
    int32_t i = 0;
    float f = 0;
    std::string s;
};
Arg I(int v) { return {Arg::Int, v}; }
Arg F(float v) { return {Arg::Float, 0, v}; }
Arg R(int k) { return {Arg::Ref, k}; }
Arg S(const std::string &v) { return {Arg::Str, 0, 0, v}; }
int &ref(int k) { return g_vm->page(g_refs)->v[size_t(k)]; }

int32_t call(const std::string &name, const std::vector<Arg> &args) {
    auto it = HllRegistry::table().find(name);
    if (it == HllRegistry::table().end()) {
        g_checks++;
        g_failed++;
        std::printf("FAIL %s isn't registered\n", name.c_str());
        return 0;
    }
    HllFunction decl;
    decl.name = name;
    HllCall c{*g_vm, decl};
    for (const Arg &a : args) {
        c.argWord.push_back(int(c.words.size()));
        switch (a.kind) {
            case Arg::Int: c.words.push_back(a.i); break;
            case Arg::Float: { int32_t w; std::memcpy(&w, &a.f, 4); c.words.push_back(w); break; }
            case Arg::Ref: c.words.push_back(g_refs); c.words.push_back(a.i); break;
            case Arg::Str: c.words.push_back(g_vm->newString(a.s)); break;
        }
    }
    it->second(c);
    return c.ret;
}

// A sprite of its own pixels, filled with one RGBA value, or without an alpha map.
void sprite(int n, int w, int h, int r, int g, int b, int a, bool alpha = true) {
    Sprite &s = game().sprites.get(n);
    s = Sprite{};
    s.used = true;
    s.hasOwn = true;
    s.own.create(w, h, alpha);
    s.own.fill(0, 0, w, h, r, g, b, alpha ? a : 0);
}
uint8_t *at(int n, int x, int y) { return game().sprites.find(n)->own.at(x, y); }
void set(int n, int x, int y, int r, int g, int b, int a) {
    uint8_t *p = at(n, x, y);
    p[0] = uint8_t(r); p[1] = uint8_t(g); p[2] = uint8_t(b); p[3] = uint8_t(a);
}
// Expects one channel of one pixel; ch 3 is the alpha map.
void px(const char *what, int n, int x, int y, int ch, int want) { expect(what, at(n, x, y)[ch], want); }
void rgb(const char *what, int n, int x, int y, int r, int g, int b) {
    std::string w = what;
    px((w + " R").c_str(), n, x, y, 0, r);
    px((w + " G").c_str(), n, x, y, 1, g);
    px((w + " B").c_str(), n, x, y, 2, b);
}

// Sprite 1 is the destination, 2 the source: one pixel each, as (d) and (s, sa).
void pair1(int d, int dA, int s, int sA) {
    sprite(1, 1, 1, d, d, d, dA);
    sprite(2, 1, 1, s, s, s, sA);
}

void testBlends() {
    const int d = 60, s = 220, sa = 100, al = 180, rate = 140;
    struct Case { const char *name; std::vector<int> extra; int want; };
    Case cases[] = {
        {"Copy", {}, s},
        {"CopyBright", {rate}, mulF(s, rate)},
        {"CopyColorReverse", {}, 255 - s},
        {"Blend", {al}, blendF(d, s, al)},
        {"BlendSrcBright", {al, rate}, blendF(d, mulF(s, rate), al)},
        {"BlendAddSatur", {}, satF(d + s)},
        {"BlendAMap", {}, blendF(d, s, sa)},
        {"BlendAMapSrcOnly", {}, satF(d + mulF(s, sa))},
        {"BlendAMapAlpha", {al}, blendF(d, s, mulF(sa, al))},
        {"BlendAMapBright", {rate}, blendF(d, mulF(s, rate), sa)},
        {"BlendAMapAlphaSrcBright", {al, rate}, blendF(d, mulF(s, rate), mulF(sa, al))},
        {"BlendScreen", {}, 255 - mulF(255 - d, 255 - s)},
        {"BlendMultiply", {}, mulF(d, s)},
        {"BlendScreenAlpha", {al}, blendF(d, 255 - mulF(255 - d, 255 - s), al)},
        {"SaturDP_DPxSA", {}, satF(d + sa)},
        {"CopyUseAMapUnder", {sa + 1}, s},
        {"CopyUseAMapBorder", {sa + 1}, d},
    };
    for (auto &k : cases) {
        pair1(d, 255, s, sa);
        std::vector<Arg> a = {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1)};
        for (int v : k.extra) a.push_back(I(v));
        call("DrawGraph." + std::string(k.name), a);
        px(k.name, 1, 0, 0, 0, k.want);
        px((std::string(k.name) + " keeps the alpha map").c_str(), 1, 0, 0, 3, 255);
    }
    // The saturating add at both ends, and BlendAMapSrcOnly's clamp.
    pair1(200, 255, 200, 255);
    call("DrawGraph.BlendAMapSrcOnly", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1)});
    px("BlendAMapSrcOnly saturates", 1, 0, 0, 0, 255);

    pair1(d, 255, s, sa);
    call("DrawGraph.BlendAMapColor", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1), I(10), I(20), I(250)});
    rgb("BlendAMapColor", 1, 0, 0, blendF(d, 10, sa), blendF(d, 20, sa), blendF(d, 250, sa));
    pair1(d, 255, s, sa);
    call("DrawGraph.BlendAMapColorAlpha", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1), I(10), I(20), I(250), I(al)});
    rgb("BlendAMapColorAlpha", 1, 0, 0, blendF(d, 10, mulF(sa, al)), blendF(d, 20, mulF(sa, al)), blendF(d, 250, mulF(sa, al)));
    // BlendUseAMapColor's fourth argument is the alpha map's sprite.
    pair1(d, 255, s, sa);
    call("DrawGraph.BlendUseAMapColor", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1), I(10), I(20), I(250), I(al)});
    rgb("BlendUseAMapColor", 1, 0, 0, blendF(d, 10, mulF(sa, al)), blendF(d, 20, mulF(sa, al)), blendF(d, 250, mulF(sa, al)));
    pair1(d, 255, s, 99);
    call("StoatSpriteEngine.CG_BlendAMapBin", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1), I(100)});
    px("CG_BlendAMapBin below the border", 1, 0, 0, 0, d);
    pair1(d, 255, s, 100);
    call("StoatSpriteEngine.CG_BlendAMapBin", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1), I(100)});
    px("CG_BlendAMapBin at the border", 1, 0, 0, 0, s);
}

void testAlphaMaps() {
    const int da = 90, sa = 200;
    struct Case { const char *name; int want; };
    Case cases[] = {
        {"CopyAMap", sa},
        {"ScreenDA_DAxSA", 255 - mulF(255 - da, 255 - sa)},
        {"AddDA_DAxSA", satF(da + sa)},
        {"SubDA_DAxSA", satF(da - sa)},
        {"BlendDA_DAxSA", blendF(da, sa, sa)},
        {"CopyAMapMax", sa},
        {"CopyAMapMin", da},
    };
    for (auto &k : cases) {
        pair1(10, da, 50, sa);
        call("DrawGraph." + std::string(k.name), {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1)});
        px(k.name, 1, 0, 0, 3, k.want);
    }
    pair1(10, da, 50, sa);
    call("DrawGraph.CopyAMapMax", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1)});
    px("CopyAMapMax brings the pixel", 1, 0, 0, 0, 50);
    pair1(10, da, 50, sa);
    call("DrawGraph.CopyAMapMin", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1)});
    px("CopyAMapMin leaves the pixel", 1, 0, 0, 0, 10);
    pair1(10, da, 50, 0);
    call("DrawGraph.SpriteCopyAMap", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1), I(0)});
    px("SpriteCopyAMap skips the key", 1, 0, 0, 3, da);
    pair1(10, da, 50, 7);
    call("DrawGraph.SpriteCopyAMap", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1), I(0)});
    px("SpriteCopyAMap copies other values", 1, 0, 0, 3, 7);

    // Writing the alpha map of a picture without one: the rest of it reads as opaque.
    sprite(1, 2, 1, 10, 10, 10, 0, false);
    sprite(2, 1, 1, 50, 50, 50, 33);
    call("DrawGraph.CopyAMap", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1)});
    px("CopyAMap onto a pixel-only picture", 1, 0, 0, 3, 33);
    px("the picture's other pixels turn opaque", 1, 1, 0, 3, 255);
    expect("it has an alpha map", game().sprites.find(1)->own.hasAlpha, 1);
    // A source without an alpha map reads as alpha 255.
    sprite(1, 1, 1, 10, 10, 10, 0);
    sprite(2, 1, 1, 50, 50, 50, 0, false);
    call("DrawGraph.BlendAMap", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1)});
    px("BlendAMap from a pixel-only source", 1, 0, 0, 0, 50);
}

void testCopies() {
    // Clipping on both pictures.
    sprite(1, 3, 1, 0, 0, 0, 255);
    sprite(2, 3, 1, 0, 0, 0, 255);
    set(2, 0, 0, 10, 0, 0, 255); set(2, 1, 0, 20, 0, 0, 255); set(2, 2, 0, 30, 0, 0, 255);
    call("DrawGraph.Copy", {I(1), I(-1), I(0), I(2), I(0), I(0), I(3), I(1)});
    px("Copy clipped left 0", 1, 0, 0, 0, 20);
    px("Copy clipped left 1", 1, 1, 0, 0, 30);
    px("Copy clipped left 2", 1, 2, 0, 0, 0);
    // A picture copied onto itself reads the pixels from before the call.
    call("DrawGraph.Copy", {I(2), I(1), I(0), I(2), I(0), I(0), I(2), I(1)});
    px("Copy onto itself 0", 2, 0, 0, 0, 10);
    px("Copy onto itself 1", 2, 1, 0, 0, 10);
    px("Copy onto itself 2", 2, 2, 0, 0, 20);
    sprite(2, 3, 1, 0, 0, 0, 255);
    set(2, 0, 0, 10, 0, 0, 1); set(2, 1, 0, 20, 0, 0, 2); set(2, 2, 0, 30, 0, 0, 3);
    sprite(1, 3, 1, 0, 0, 0, 255);
    call("DrawGraph.CopyReverseLR", {I(1), I(0), I(0), I(2), I(0), I(0), I(3), I(1)});
    px("CopyReverseLR 0", 1, 0, 0, 0, 30);
    px("CopyReverseLR 2", 1, 2, 0, 0, 10);
    call("DrawGraph.CopyReverseAMapLR", {I(1), I(0), I(0), I(2), I(0), I(0), I(3), I(1)});
    px("CopyReverseAMapLR 0", 1, 0, 0, 3, 3);
    // Colour key.
    sprite(1, 2, 1, 1, 1, 1, 255);
    sprite(2, 2, 1, 255, 0, 255, 255);
    set(2, 1, 0, 40, 50, 60, 255);
    call("DrawGraph.CopySprite", {I(1), I(0), I(0), I(2), I(0), I(0), I(2), I(1), I(255), I(0), I(255)});
    px("CopySprite skips the key colour", 1, 0, 0, 1, 1);
    rgb("CopySprite copies other colours", 1, 1, 0, 40, 50, 60);
    sprite(1, 1, 1, 0, 0, 0, 255);
    sprite(2, 1, 1, 200, 100, 50, 255);
    call("DrawGraph.CopyGrayscale", {I(1), I(0), I(0), I(2), I(0), I(0), I(1), I(1)});
    int gray = int(std::lround(200 * 0.299 + 100 * 0.587 + 50 * 0.114));
    rgb("CopyGrayscale", 1, 0, 0, gray, gray, gray);
    // Box blur across, radius 1, the edges repeated.
    sprite(1, 3, 1, 0, 0, 0, 255);
    sprite(2, 3, 1, 0, 0, 0, 255);
    set(2, 1, 0, 90, 0, 0, 255); set(2, 2, 0, 180, 0, 0, 255);
    call("DrawGraph.CopyWidthBlur", {I(1), I(0), I(0), I(2), I(0), I(0), I(3), I(1), I(1)});
    px("CopyWidthBlur 0", 1, 0, 0, 0, 30);
    px("CopyWidthBlur 1", 1, 1, 0, 0, 90);
    px("CopyWidthBlur 2", 1, 2, 0, 0, 150);
    // Texture wrap starting one pixel into the source.
    sprite(1, 4, 1, 0, 0, 0, 255);
    call("DrawGraph.CopyTextureWrap", {I(1), I(0), I(0), I(4), I(1), I(2), I(0), I(0), I(3), I(1), I(1), I(0)});
    px("CopyTextureWrap 0", 1, 0, 0, 0, 90);
    px("CopyTextureWrap 1", 1, 1, 0, 0, 180);
    px("CopyTextureWrap 2", 1, 2, 0, 0, 0);
    px("CopyTextureWrap 3", 1, 3, 0, 0, 90);
}

void testFills() {
    const int d = 100;
    struct Case { const char *name; std::vector<int> extra; int want; };
    Case cases[] = {
        {"Fill", {30, 30, 30}, 30},
        {"FillAlphaColor", {30, 30, 30, 200}, blendF(d, 30, 200)},
        {"FillScreen", {30, 30, 30}, 255 - mulF(255 - d, 255 - 30)},
        {"FillMultiply", {30, 30, 30}, mulF(d, 30)},
        {"FillOverlay", {30, 30, 30}, mulF(2 * d, 30)},
        {"FillOverlayAlpha", {30, 30, 30, 128}, blendF(d, mulF(2 * d, 30), 128)},
        {"BrightDestOnly", {77}, mulF(d, 77)},
    };
    for (auto &k : cases) {
        sprite(1, 1, 1, d, d, d, 255);
        std::vector<Arg> a = {I(1), I(0), I(0), I(1), I(1)};
        for (int v : k.extra) a.push_back(I(v));
        call("DrawGraph." + std::string(k.name), a);
        px(k.name, 1, 0, 0, 0, k.want);
    }
    sprite(1, 1, 1, 200, 200, 200, 255);
    call("DrawGraph.FillOverlay", {I(1), I(0), I(0), I(1), I(1), I(30), I(30), I(30)});
    px("FillOverlay on a light base", 1, 0, 0, 0, 255 - mulF(2 * 55, 225));

    sprite(1, 2, 1, 0, 0, 0, 0);
    set(1, 0, 0, 0, 0, 0, 99); set(1, 1, 0, 0, 0, 0, 100);
    call("DrawGraph.FillAMapOverBorder", {I(1), I(0), I(0), I(2), I(1), I(7), I(100)});
    px("FillAMapOverBorder below", 1, 0, 0, 3, 99);
    px("FillAMapOverBorder at", 1, 1, 0, 3, 7);
    set(1, 0, 0, 0, 0, 0, 99); set(1, 1, 0, 0, 0, 0, 100);
    call("DrawGraph.FillAMapUnderBorder", {I(1), I(0), I(0), I(2), I(1), I(7), I(100)});
    px("FillAMapUnderBorder below", 1, 0, 0, 3, 7);
    px("FillAMapUnderBorder at", 1, 1, 0, 3, 100);
    sprite(1, 1, 3, 0, 0, 0, 0);
    call("DrawGraph.FillAMapGradationUD", {I(1), I(0), I(0), I(1), I(3), I(0), I(255)});
    px("FillAMapGradationUD top", 1, 0, 0, 3, 0);
    px("FillAMapGradationUD middle", 1, 0, 1, 3, 128);
    px("FillAMapGradationUD bottom", 1, 0, 2, 3, 255);
    sprite(1, 2, 1, 5, 5, 5, 255);
    call("DrawGraph.FillWithAlpha", {I(1), I(1), I(0), I(5), I(5), I(1), I(2), I(3), I(4)});
    px("FillWithAlpha clipped", 1, 0, 0, 3, 255);
    rgb("FillWithAlpha", 1, 1, 0, 1, 2, 3);
    px("FillWithAlpha alpha", 1, 1, 0, 3, 4);
    sprite(1, 3, 3, 0, 0, 0, 0);
    call("DrawGraph.DrawLineToAMap", {I(1), I(0), I(0), I(2), I(2), I(77)});
    px("DrawLineToAMap on the line", 1, 1, 1, 3, 77);
    px("DrawLineToAMap off the line", 1, 1, 0, 3, 0);
}

void testScaling() {
    // 2x1 to 4x1, nearest.
    sprite(2, 2, 1, 0, 0, 0, 255);
    set(2, 0, 0, 10, 0, 0, 20); set(2, 1, 0, 30, 0, 0, 40);
    sprite(1, 4, 1, 0, 0, 0, 255);
    call("DrawGraph.CopyStretch", {I(1), I(0), I(0), I(4), I(1), I(2), I(0), I(0), I(2), I(1)});
    px("CopyStretch 0", 1, 0, 0, 0, 10);
    px("CopyStretch 1", 1, 1, 0, 0, 10);
    px("CopyStretch 2", 1, 2, 0, 0, 30);
    px("CopyStretch 3", 1, 3, 0, 0, 30);
    call("DrawGraph.CopyStretchAMap", {I(1), I(0), I(0), I(4), I(1), I(2), I(0), I(0), I(2), I(1)});
    px("CopyStretchAMap 3", 1, 3, 0, 3, 40);
    // Bilinear: the destination's pixel 1 centre (1.5 of 4) is source x 0.25, a quarter of the way along.
    sprite(1, 4, 1, 0, 0, 0, 255);
    call("DrawGraph.CopyStretchInterp", {I(1), I(0), I(0), I(4), I(1), I(2), I(0), I(0), I(2), I(1)});
    px("CopyStretchInterp edge", 1, 0, 0, 0, 10);
    px("CopyStretchInterp 1", 1, 1, 0, 0, 15);
    px("CopyStretchInterp 2", 1, 2, 0, 0, 25);
    // 4x1 to 2x1, averaged.
    sprite(2, 4, 1, 0, 0, 0, 255);
    set(2, 0, 0, 10, 0, 0, 255); set(2, 1, 0, 20, 0, 0, 255); set(2, 2, 0, 100, 0, 0, 0); set(2, 3, 0, 201, 0, 0, 0);
    sprite(1, 2, 1, 0, 0, 0, 255);
    call("DrawGraph.CopyReduce", {I(1), I(0), I(0), I(2), I(1), I(2), I(0), I(0), I(4), I(1)});
    px("CopyReduce 0", 1, 0, 0, 0, 15);
    px("CopyReduce 1", 1, 1, 0, 0, 151);
    call("DrawGraph.CopyReduceAMap", {I(1), I(0), I(0), I(2), I(1), I(2), I(0), I(0), I(4), I(1)});
    px("CopyReduceAMap 1", 1, 1, 0, 3, 0);
    sprite(2, 1, 1, 200, 200, 200, 100);
    sprite(1, 2, 1, 0, 0, 0, 255);
    call("DrawGraph.CopyStretchBlend", {I(1), I(0), I(0), I(2), I(1), I(2), I(0), I(0), I(1), I(1), I(64)});
    px("CopyStretchBlend", 1, 1, 0, 0, blendF(0, 200, 64));
    sprite(1, 2, 1, 0, 0, 0, 255);
    call("DrawGraph.CopyStretchBlendAMap", {I(1), I(0), I(0), I(2), I(1), I(2), I(0), I(0), I(1), I(1)});
    px("CopyStretchBlendAMap", 1, 1, 0, 0, blendF(0, 200, 100));
    sprite(1, 2, 1, 0, 0, 0, 0, false);
    call("DrawGraph.CopyStretchWithAlphaMap", {I(1), I(0), I(0), I(2), I(1), I(2), I(0), I(0), I(1), I(1)});
    px("CopyStretchWithAlphaMap pixel", 1, 1, 0, 0, 200);
    px("CopyStretchWithAlphaMap alpha", 1, 1, 0, 3, 100);
}

void testRotation() {
    // A 3x3 source with a marked top-left corner, turned 90 degrees clockwise: the corner goes top right.
    sprite(2, 3, 3, 0, 0, 0, 255);
    set(2, 0, 0, 200, 0, 0, 255);
    sprite(1, 3, 3, 9, 9, 9, 0);
    call("DrawGraph.CopyRotZoom", {I(1), I(2), I(0), I(0), I(3), I(3), F(90), F(1)});
    px("CopyRotZoom 90 top right", 1, 2, 0, 0, 200);
    px("CopyRotZoom 90 top left", 1, 0, 0, 0, 0);
    px("CopyRotZoom keeps the alpha map", 1, 2, 0, 3, 0);
    call("DrawGraph.CopyRotZoomAMap", {I(1), I(2), I(0), I(0), I(3), I(3), F(90), F(1)});
    px("CopyRotZoomAMap", 1, 2, 0, 3, 255);
    // Zoomed out by half, the destination's corners lie outside the picture and keep their pixels.
    sprite(1, 3, 3, 9, 9, 9, 0);
    call("DrawGraph.CopyRotZoom", {I(1), I(2), I(0), I(0), I(3), I(3), F(0), F(0.5f)});
    px("CopyRotZoom 0.5 corner untouched", 1, 0, 0, 0, 9);
    px("CopyRotZoom 0.5 centre", 1, 1, 1, 0, 0);
    sprite(1, 3, 3, 9, 9, 9, 0);
    call("DrawGraph.CopyRotZoom2Bilinear", {I(1), F(1.5f), F(1.5f), I(2), F(1.5f), F(1.5f), F(-90), F(1)});
    px("CopyRotZoom2Bilinear -90 bottom left", 1, 0, 2, 0, 200);

    // Y rotation: 0 degrees is a copy, 180 mirrors, 60 halves the width.
    sprite(2, 4, 1, 0, 0, 0, 255);
    set(2, 0, 0, 10, 0, 0, 255); set(2, 1, 0, 20, 0, 0, 255); set(2, 2, 0, 30, 0, 0, 255); set(2, 3, 0, 40, 0, 0, 255);
    sprite(3, 4, 1, 1, 1, 1, 255);
    sprite(1, 4, 1, 0, 0, 0, 255);
    call("DrawGraph.CopyRotateY", {I(1), I(3), I(2), I(0), I(0), I(4), I(1), F(0), F(1)});
    px("CopyRotateY 0", 1, 0, 0, 0, 10);
    px("CopyRotateY 0 end", 1, 3, 0, 0, 40);
    call("DrawGraph.CopyRotateY", {I(1), I(3), I(2), I(0), I(0), I(4), I(1), F(180), F(1)});
    px("CopyRotateY 180 mirrors", 1, 0, 0, 0, 40);
    call("DrawGraph.CopyRotateYFixL", {I(1), I(3), I(2), I(0), I(0), I(4), I(1), F(60), F(1)});
    // Each written pixel covers two source pixels; its centre samples the second of them.
    px("CopyRotateYFixL 60 at the left", 1, 0, 0, 0, 20);
    px("CopyRotateYFixL 60 squeezed", 1, 1, 0, 0, 40);
    px("CopyRotateYFixL 60 background", 1, 2, 0, 0, 1);
    call("DrawGraph.CopyRotateYFixR", {I(1), I(3), I(2), I(0), I(0), I(4), I(1), F(60), F(1)});
    px("CopyRotateYFixR 60 background", 1, 1, 0, 0, 1);
    px("CopyRotateYFixR 60 at the right", 1, 3, 0, 0, 40);
    // X rotation, with the alpha map: a half-transparent source blends over the background.
    sprite(2, 1, 2, 200, 200, 200, 128);
    sprite(3, 1, 2, 0, 0, 0, 255);
    sprite(1, 1, 2, 50, 50, 50, 255);
    call("DrawGraph.CopyRotateXUseAMap", {I(1), I(3), I(2), I(0), I(0), I(1), I(2), F(0), F(1)});
    px("CopyRotateXUseAMap", 1, 0, 1, 0, blendF(0, 200, 128));
    call("DrawGraph.CopyRotateXFixU", {I(1), I(3), I(2), I(0), I(0), I(1), I(2), F(60), F(1)});
    px("CopyRotateXFixU 60 top", 1, 0, 0, 0, 200);
    px("CopyRotateXFixU 60 background", 1, 0, 1, 0, 0);
}

void testReadsAndShapes() {
    sprite(1, 2, 1, 11, 22, 33, 44);
    ref(0) = ref(1) = ref(2) = -1;
    expect("GetPixelColor returns true", call("DrawGraph.GetPixelColor", {I(1), I(1), I(0), R(0), R(1), R(2)}), 1);
    expect("GetPixelColor R", ref(0), 11);
    expect("GetPixelColor B", ref(2), 33);
    expect("GetAlphaColor", (call("DrawGraph.GetAlphaColor", {I(1), I(0), I(0), R(3)}), ref(3)), 44);
    ref(0) = -1;
    expect("GetPixelColor outside returns false", call("DrawGraph.GetPixelColor", {I(1), I(2), I(0), R(0), R(1), R(2)}), 0);
    expect("GetPixelColor outside leaves the reference", ref(0), -1);
    sprite(1, 1, 1, 0, 0, 0, 0, false);
    call("DrawGraph.GetAlphaColor", {I(1), I(0), I(0), R(3)});
    expect("GetAlphaColor without an alpha map", ref(3), 255);

    // A triangle covering the whole 4x4 picture in one colour, then one covering only its top-left half.
    sprite(1, 4, 4, 0, 0, 0, 0);
    call("DrawGraph.DrawColorPolygon", {I(1), F(-1), F(-1), F(0), I(10), I(20), I(30), I(40),
                                        F(20), F(-1), F(0), I(10), I(20), I(30), I(40),
                                        F(-1), F(20), F(0), I(10), I(20), I(30), I(40)});
    rgb("DrawColorPolygon", 1, 3, 3, 10, 20, 30);
    px("DrawColorPolygon alpha", 1, 3, 3, 3, 40);
    sprite(1, 4, 4, 0, 0, 0, 0);
    sprite(2, 1, 1, 70, 80, 90, 255);
    call("DrawGraph.DrawPolygon", {I(1), I(2), F(0), F(0), F(0), F(0), F(0), F(4), F(0), F(0), F(1), F(0),
                                   F(0), F(4), F(0), F(0), F(1)});
    rgb("DrawPolygon inside", 1, 0, 0, 70, 80, 90);
    px("DrawPolygon outside", 1, 3, 3, 0, 0);
}

void testFont() {
    call("DrawGraph.SetFontName", {S("\x96\xbe\x92\xa9")});
    expect("GetFontName", g_vm->str(call("DrawGraph.GetFontName", {})) == "\x96\xbe\x92\xa9", 1);
    call("DrawGraph.SetFontColor", {I(1), I(2), I(3)});
    call("DrawGraph.GetFontColor", {R(0), R(1), R(2)});
    expect("GetFontColor G", ref(1), 2);
    call("DrawGraph.SetFontSpace", {I(4)});
    expect("GetFontSpace", call("DrawGraph.GetFontSpace", {}), 4);
    call("DrawGraph.SetFontUnderline", {I(1)});
    expect("GetFontUnderline", call("DrawGraph.GetFontUnderline", {}), 1);
    call("DrawGraph.SetFontStrikeOut", {I(1)});
    expect("GetFontStrikeOut", call("DrawGraph.GetFontStrikeOut", {}), 1);
    // The underline is drawn in the font colour along the bottom of the line, over the text's width.
    call("DrawGraph.SetFontSize", {I(16)});
    call("DrawGraph.SetFontColor", {I(255), I(0), I(0)});
    sprite(1, 40, 20, 0, 0, 0, 0);
    call("DrawGraph.DrawTextToAMap", {I(1), I(0), I(0), S("AA")});
    px("DrawTextToAMap underline", 1, 1, 15, 3, 255);
    px("DrawTextToAMap below the line", 1, 1, 17, 3, 0);
}

}  // namespace

int main() {
    Game g(GameConfig{});
    VM vm{Ain{}};
    g_vm = &vm;
    g_refs = vm.newPage(PageKind::Local, -1, 8);
    testBlends();
    testAlphaMaps();
    testCopies();
    testFills();
    testScaling();
    testRotation();
    testReadsAndShapes();
    testFont();
    std::printf("%d checks, %d failed\n", g_checks, g_failed);
    return g_failed ? 1 : 0;
}
