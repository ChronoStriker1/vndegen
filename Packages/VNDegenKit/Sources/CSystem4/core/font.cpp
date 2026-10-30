#include "font.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>

#include "fnl.h"
#include "vnfont.h"

#if __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#endif

namespace s4 {
namespace {

std::mutex mu;
std::string packDir;
bool packReady = false;

std::vector<uint8_t> readFile(const std::string &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

struct FontSource {
    std::vector<uint8_t> data;
    int face = 0;
};

// The Windows names System 4 games draw with: face 0 and 1 of sact_text_metrics_t.
bool packFont(int face, FontSource &out) {
    if (!packReady && !packDir.empty()) {
        const char *dirs[] = {packDir.c_str()};
        vnfont_init((packDir + "/fonts.tsv").c_str(), dirs, 1);
        packReady = true;
    }
    vnfont_match m;
    const char *name = face == 1 ? "MS Mincho" : "MS Gothic";
    if (!vnfont_resolve(name, 0, 0, &m)) return false;
    out.data = readFile(m.path);
    out.face = m.face;
    return !out.data.empty();
}

#if __APPLE__

class Face {
public:
    explicit Face(const FontSource *src) {
        if (!src) return;
        CFDataRef data = CFDataCreate(nullptr, src->data.data(), CFIndex(src->data.size()));
        CFArrayRef list = CTFontManagerCreateFontDescriptorsFromData(data);
        CFRelease(data);
        if (!list) return;
        CFIndex n = CFArrayGetCount(list);
        if (n > 0) {
            descriptor_ = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(list, std::min<CFIndex>(src->face, n - 1)));
            CFRetain(descriptor_);
        }
        CFRelease(list);
    }
    bool glyph(uint32_t cp, int px, bool bold, GlyphImage &out) {
        UniChar chars[2];
        CFIndex n = 1;
        if (cp >= 0x10000) {
            chars[0] = UniChar(0xD800 + ((cp - 0x10000) >> 10));
            chars[1] = UniChar(0xDC00 + ((cp - 0x10000) & 0x3FF));
            n = 2;
        } else {
            chars[0] = UniChar(cp);
        }
        CGGlyph glyphs[2] = {0, 0};
        CTFontRef font = fontFor(px), use = nullptr, fallback = nullptr;
        if (font && CTFontGetGlyphsForCharacters(font, chars, glyphs, n) && glyphs[0]) use = font;
        if (!use) {
            CTFontRef h = hiraginoFor(px);
            if (h && CTFontGetGlyphsForCharacters(h, chars, glyphs, n) && glyphs[0]) use = h;
        }
        if (!use) {
            CTFontRef base = font ? font : hiraginoFor(px);
            CFStringRef s = CFStringCreateWithCharacters(nullptr, chars, n);
            fallback = CTFontCreateForString(base, s, CFRangeMake(0, n));
            CFRelease(s);
            if (fallback && CTFontGetGlyphsForCharacters(fallback, chars, glyphs, n) && glyphs[0]) use = fallback;
        }
        if (!use) {
            if (fallback) CFRelease(fallback);
            return false;
        }
        CGSize adv;
        CTFontGetAdvancesForGlyphs(use, kCTFontOrientationHorizontal, glyphs, &adv, 1);
        CGRect box = CTFontGetBoundingRectsForGlyphs(use, kCTFontOrientationHorizontal, glyphs, nullptr, 1);
        int pad = bold ? 2 : 1;
        out.left = int(std::floor(box.origin.x)) - pad;
        int bottom = int(std::floor(box.origin.y)) - pad;
        out.w = int(std::ceil(box.origin.x + box.size.width)) + pad - out.left;
        out.top = int(std::ceil(box.origin.y + box.size.height)) + pad;
        out.h = out.top - bottom;
        out.advance = int(std::lround(adv.width));
        out.coverage.assign(size_t(std::max(0, out.w)) * std::max(0, out.h), 0);
        if (out.w > 0 && out.h > 0) {
            CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
            CGContextRef ctx = CGBitmapContextCreate(out.coverage.data(), size_t(out.w), size_t(out.h), 8, size_t(out.w),
                                                     gray, kCGImageAlphaNone);
            CGColorSpaceRelease(gray);
            CGContextSetGrayFillColor(ctx, 1, 1);
            if (bold) {
                CGContextSetTextDrawingMode(ctx, kCGTextFillStroke);
                CGContextSetGrayStrokeColor(ctx, 1, 1);
                CGContextSetLineWidth(ctx, std::max(1.0, px / 24.0));
            }
            CGPoint pos = CGPointMake(-out.left, -bottom);
            CTFontDrawGlyphs(use, glyphs, &pos, 1, ctx);
            CGContextRelease(ctx);
        }
        if (fallback) CFRelease(fallback);
        return true;
    }
    int ascent(int px) {
        CTFontRef f = fontFor(px);
        if (!f) f = hiraginoFor(px);
        return f ? int(std::lround(CTFontGetAscent(f))) : px * 86 / 100;
    }

private:
    CTFontRef fontFor(int px) {
        if (!descriptor_) return nullptr;
        auto it = fonts_.find(px);
        if (it != fonts_.end()) return it->second;
        return fonts_[px] = CTFontCreateWithFontDescriptor(descriptor_, px, nullptr);
    }
    CTFontRef hiraginoFor(int px) {
        auto it = hiragino_.find(px);
        if (it != hiragino_.end()) return it->second;
        return hiragino_[px] = CTFontCreateWithName(CFSTR("HiraginoSans-W3"), px, nullptr);
    }
    CTFontDescriptorRef descriptor_ = nullptr;
    std::map<int, CTFontRef> fonts_, hiragino_;
};

#else

FT_Library library() {
    static FT_Library lib = [] {
        FT_Library l = nullptr;
        FT_Init_FreeType(&l);
        return l;
    }();
    return lib;
}

class Face {
public:
    explicit Face(const FontSource *src) {
        if (src) {
            src_ = *src;
            if (FT_New_Memory_Face(library(), src_.data.data(), FT_Long(src_.data.size()), src_.face, &face_)) face_ = nullptr;
        }
    }
    bool glyph(uint32_t cp, int px, bool bold, GlyphImage &out) {
        if (!face_) return false;
        FT_UInt index = FT_Get_Char_Index(face_, cp);
        if (!index) return false;
        FT_Set_Pixel_Sizes(face_, 0, FT_UInt(px));
        if (FT_Load_Glyph(face_, index, FT_LOAD_NO_BITMAP)) return false;
        if (bold) FT_GlyphSlot_Embolden(face_->glyph);
        if (FT_Render_Glyph(face_->glyph, FT_RENDER_MODE_NORMAL)) return false;
        const FT_Bitmap &b = face_->glyph->bitmap;
        if (b.pixel_mode != FT_PIXEL_MODE_GRAY) return false;
        out.w = int(b.width);
        out.h = int(b.rows);
        out.left = face_->glyph->bitmap_left;
        out.top = face_->glyph->bitmap_top;
        out.advance = int(face_->glyph->advance.x >> 6);
        out.coverage.resize(size_t(out.w) * out.h);
        for (int y = 0; y < out.h; y++)
            std::memcpy(out.coverage.data() + size_t(y) * out.w, b.buffer + ptrdiff_t(y) * b.pitch, size_t(out.w));
        return true;
    }
    int ascent(int px) {
        if (!face_) return px * 86 / 100;
        FT_Set_Pixel_Sizes(face_, 0, FT_UInt(px));
        return int(face_->size->metrics.ascender >> 6);
    }

private:
    FontSource src_;
    FT_Face face_ = nullptr;
};

#endif

Face &faceFor(int f) {
    static std::map<int, std::unique_ptr<Face>> faces;
    auto &p = faces[f];
    if (!p) {
        FontSource src;
        p = std::make_unique<Face>(packFont(f, src) ? &src : nullptr);
    }
    return *p;
}

}  // namespace

namespace {
std::unique_ptr<FnlFont> g_fnl;
std::map<std::tuple<int, uint16_t, int>, GlyphImage> g_fnlCache;
// The stored size to scale down from for a pixel size: the smallest at least px tall, else the largest.
int fnlSize(int face, int px) {
    std::vector<int> sizes = g_fnl->sizes(face);
    if (sizes.empty()) return -1;
    int best = -1;
    for (int z : sizes)
        if (z >= px && (best < 0 || z < best)) best = z;
    return best < 0 ? *std::max_element(sizes.begin(), sizes.end()) : best;
}
}  // namespace

void setGameFontLibrary(const std::string &path) {
    std::lock_guard<std::mutex> lock(mu);
    g_fnlCache.clear();
    g_fnl.reset();
    if (path.empty()) return;
    auto f = std::make_unique<FnlFont>();
    if (f->open(path)) g_fnl = std::move(f);
}

// Box-filters the stored 1-bit glyph to the pixel size, which also gives it anti-aliased edges.
bool gameGlyph(int face, uint16_t code, int px, GlyphImage &out) {
    auto &cache = g_fnlCache;
    std::lock_guard<std::mutex> lock(mu);
    if (!g_fnl || face < 0 || face >= g_fnl->faceCount() || px <= 0) return false;
    std::tuple<int, uint16_t, int> k{face, code, px};
    auto it = cache.find(k);
    if (it != cache.end()) { out = it->second; return true; }
    int size = fnlSize(face, px);
    FnlGlyph g;
    if (size < 0 || !g_fnl->glyph(face, size, code, g, 1)) return false;
    double f = double(px) / double(size);
    int ow = std::max(1, int(std::lround(g.w * f))), oh = std::max(1, int(std::lround(g.h * f)));
    out.w = ow;
    out.h = oh;
    out.left = 0;
    out.top = int(std::lround(g.top * f));
    out.advance = std::max(1, int(std::lround(g.advance * f)));
    out.coverage.assign(size_t(ow) * oh, 0);
    for (int y = 0; y < oh; y++) {
        int y0 = int(y / f), y1 = std::max(y0 + 1, std::min(g.h, int((y + 1) / f)));
        for (int x = 0; x < ow; x++) {
            int x0 = int(x / f), x1 = std::max(x0 + 1, std::min(g.w, int((x + 1) / f)));
            int sum = 0, n = 0;
            for (int yy = y0; yy < y1 && yy < g.h; yy++)
                for (int xx = x0; xx < x1 && xx < g.w; xx++, n++) sum += g.coverage[size_t(yy) * g.w + xx];
            out.coverage[size_t(y) * ow + x] = uint8_t(n ? sum / n : 0);
        }
    }
    if (cache.size() > 20000) cache.clear();
    cache[k] = out;
    return true;
}

int gameFontAscent(int face, int px) {
    std::lock_guard<std::mutex> lock(mu);
    if (!g_fnl || face < 0 || face >= g_fnl->faceCount()) return px * 86 / 100;
    int size = fnlSize(face, px);
    return size < 0 ? px * 86 / 100 : int(std::lround(g_fnl->ascent(face, size, 1) * double(px) / size));
}

void setFontPackDir(const std::string &dir) {
    std::lock_guard<std::mutex> lock(mu);
    packDir = dir;
}

bool glyph(int face, uint32_t cp, int px, bool bold, GlyphImage &out) {
    using Key = std::tuple<int, uint32_t, int, bool>;
    static std::map<Key, GlyphImage> cache;
    std::lock_guard<std::mutex> lock(mu);
    Key k{face, cp, px, bold};
    auto it = cache.find(k);
    if (it != cache.end()) { out = it->second; return true; }
    if (!faceFor(face == 1 ? 1 : 0).glyph(cp, px, bold, out)) return false;
    if (cache.size() > 20000) cache.clear();
    cache[k] = out;
    return true;
}

int fontAscent(int face, int px) {
    std::lock_guard<std::mutex> lock(mu);
    return faceFor(face == 1 ? 1 : 0).ascent(px);
}

}  // namespace s4
