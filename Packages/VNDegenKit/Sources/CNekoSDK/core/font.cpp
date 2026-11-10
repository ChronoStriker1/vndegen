#include "font.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>

#include "vnfont.h"

#if __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#endif

namespace neko {
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

// The Windows names behind init.ini's MsgFont numbers, as its comment lists them, 7 for Lucida Sans Unicode, which the
// official English releases ask for in place of them, and 8 for Verdana, which Test Game 82's program names for MsgFont 0
// (set 2, from Wine's font log).
const char *windowsFontName(int face) {
    static const char *const names[] = {"MS Gothic", "MS PGothic", "MS Mincho", "MS PMincho", "MS UI Gothic", "Meiryo", "Meiryo UI", "Lucida Sans Unicode", "Verdana", "Calibri"};
    return face >= 0 && face < 10 ? names[face] : names[0];
}

bool packFont(int face, FontSource &out) {
    if (!packReady && !packDir.empty()) {
        const char *dirs[] = {packDir.c_str()};
        vnfont_init((packDir + "/fonts.tsv").c_str(), dirs, 1);
        packReady = true;
    }
    vnfont_match m;
    if (!vnfont_resolve(windowsFontName(face), 0, 0, &m)) return false;
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
    // Whether the face itself has the character, before any fallback.
    bool has(uint32_t cp) {
        UniChar chars[2] = {UniChar(cp), 0};
        CFIndex n = 1;
        if (cp >= 0x10000) {
            chars[0] = UniChar(0xD800 + ((cp - 0x10000) >> 10));
            chars[1] = UniChar(0xDC00 + ((cp - 0x10000) & 0x3FF));
            n = 2;
        }
        CGGlyph glyphs[2] = {0, 0};
        CTFontRef font = fontFor(16);
        return font && CTFontGetGlyphsForCharacters(font, chars, glyphs, n) && glyphs[0];
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
    int missingAdvance(int px) {
        CTFontRef f = fontFor(px);
        if (!f) f = hiraginoFor(px);
        if (!f) return px / 2;
        CGGlyph none = 0;
        CGSize adv;
        CTFontGetAdvancesForGlyphs(f, kCTFontOrientationHorizontal, &none, &adv, 1);
        return int(std::lround(adv.width));
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
    // Whether the face itself has the character.
    bool has(uint32_t cp) { return face_ && FT_Get_Char_Index(face_, cp) != 0; }
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
    int missingAdvance(int px) {
        if (!face_) return px / 2;
        FT_Set_Pixel_Sizes(face_, 0, FT_UInt(px));
        if (FT_Load_Glyph(face_, 0, FT_LOAD_NO_BITMAP)) return px / 2;
        return int(face_->glyph->advance.x >> 6);
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

void setFontPackDir(const std::string &dir) {
    std::lock_guard<std::mutex> lock(mu);
    packDir = dir;
}

bool glyph(int face, uint32_t cp, int px, bool bold, GlyphImage &out) {
    using Key = std::tuple<int, uint32_t, int, bool>;
    static std::map<Key, GlyphImage> cache;
    std::lock_guard<std::mutex> lock(mu);
    // Windows draws what Lucida Sans Unicode lacks, such as kana and ♪, from MS UI Gothic, the first of its font link,
    // so its stand-in's missing characters come from MS UI Gothic's.
    // Verdana's stand-in does the same, which isn't measured: Test Game 82's English text has nothing it lacks.
    if ((face == 7 || face == 8 || face == 9) && !faceFor(face).has(cp)) face = 4;
    Key k{face, cp, px, bold};
    auto it = cache.find(k);
    if (it != cache.end()) { out = it->second; return true; }
    if (!faceFor(face).glyph(cp, px, bold, out)) return false;
    if (cache.size() > 20000) cache.clear();
    cache[k] = out;
    return true;
}

int fontAscent(int face, int px) {
    std::lock_guard<std::mutex> lock(mu);
    return faceFor(face).ascent(px);
}

int fontMissingAdvance(int face, int px) {
    std::lock_guard<std::mutex> lock(mu);
    return faceFor(face).missingAdvance(px);
}

}  // namespace neko
