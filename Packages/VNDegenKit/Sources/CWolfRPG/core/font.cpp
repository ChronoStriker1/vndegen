#include "font.h"

#include "vnfont.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>

#if __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#else
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SYNTHESIS_H
#endif

namespace fs = std::filesystem;

namespace wolf {

namespace {

uint16_t be16(const std::vector<uint8_t> &d, size_t at) { return at + 2 <= d.size() ? uint16_t(d[at] << 8 | d[at + 1]) : 0; }
uint32_t be32(const std::vector<uint8_t> &d, size_t at) { return uint32_t(be16(d, at)) << 16 | be16(d, at + 2); }

void appendUtf8(std::string &out, uint32_t cp) {
    if (cp < 0x80) out += char(cp);
    else if (cp < 0x800) {
        out += char(0xC0 | cp >> 6);
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | cp >> 12);
        out += char(0x80 | (cp >> 6 & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else {
        out += char(0xF0 | cp >> 18);
        out += char(0x80 | (cp >> 12 & 0x3F));
        out += char(0x80 | (cp >> 6 & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

std::string normalized(const std::string &name) {
    std::string out(name.size() + 1, '\0');
    if (!vnfont_normalize(name.c_str(), out.data(), out.size())) return name;
    out.resize(std::strlen(out.c_str()));
    return out;
}

std::vector<uint8_t> readFile(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

// The font file to use for a Game.dat face name, and the face in it.
struct FontSource {
    std::vector<uint8_t> data;
    int face = 0;
    bool embolden = false;
};

// A font the game ships in its folder or its Data folder, matched by any of its family names.
bool findGameFont(const GameFiles &files, const std::string &want, FontSource &out) {
    if (want.empty()) return false;
    std::string key = normalized(want);
    for (const fs::path &dir : {files.root(), files.root() / "Data", files.root() / "data"}) {
        std::error_code ec;
        for (auto &entry : fs::directory_iterator(dir, ec)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            if (ext != ".ttf" && ext != ".otf" && ext != ".ttc") continue;
            auto data = readFile(entry.path());
            int faces = data.size() >= 12 && be32(data, 0) == 0x74746366 ? int(be32(data, 8)) : 1;
            for (int face = 0; face < faces && face < 16; face++) {
                auto names = fontFamilyNames(data, face);
                names.push_back(entry.path().stem().string());
                for (auto &n : names) {
                    if (normalized(n) == key) {
                        out.data = std::move(data);
                        out.face = face;
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

bool findPackFont(const std::string &name, FontSource &out) {
#ifdef WOLF_FONTS_DIR
    // The app reads the font pack's table at launch; the Linux test host reads it here.
    static std::once_flag once;
    std::call_once(once, [] {
        const char *dirs[] = {WOLF_FONTS_DIR};
        vnfont_init(WOLF_FONTS_DIR "/fonts.tsv", dirs, 1);
    });
#endif
    vnfont_match m;
    if (name.empty() || !vnfont_resolve(name.c_str(), 0, 0, &m)) return false;
    out.data = readFile(m.path);
    out.face = m.face;
    out.embolden = m.embolden;
    return !out.data.empty();
}

#if __APPLE__

CTFontDescriptorRef descriptorFor(const FontSource &src) {
    CFDataRef data = CFDataCreate(nullptr, src.data.data(), CFIndex(src.data.size()));
    CFArrayRef list = CTFontManagerCreateFontDescriptorsFromData(data);
    CFRelease(data);
    if (!list) return nullptr;
    CTFontDescriptorRef d = nullptr;
    CFIndex n = CFArrayGetCount(list);
    if (n > 0) {
        d = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(list, std::min<CFIndex>(src.face, n - 1)));
        CFRetain(d);
    }
    CFRelease(list);
    return d;
}

// CoreText with the game's or the font pack's face, then Hiragino, then the system's fallback for the character.
class CoreTextFont : public Font {
public:
    explicit CoreTextFont(const FontSource *src) {
        if (src) descriptor_ = descriptorFor(*src);
    }
    ~CoreTextFont() override {
        for (auto &[k, f] : fonts_) CFRelease(f);
        for (auto &[k, f] : hiragino_) CFRelease(f);
        if (descriptor_) CFRelease(descriptor_);
    }

    bool glyph(uint32_t cp, int px, bool bold, GlyphImage &out) override {
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
        if (!use && font) {
            CFStringRef s = CFStringCreateWithCharacters(nullptr, chars, n);
            fallback = CTFontCreateForString(font, s, CFRangeMake(0, n));
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
            CGContextRef ctx = CGBitmapContextCreate(out.coverage.data(), size_t(out.w), size_t(out.h), 8,
                                                     size_t(out.w), gray, kCGImageAlphaNone);
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

    int ascent(int px) override {
        CTFontRef f = fontFor(px);
        if (!f) f = hiraginoFor(px);
        return f ? int(std::lround(CTFontGetAscent(f))) : px * 86 / 100;
    }

private:
    CTFontRef fontFor(int px) {
        if (!descriptor_) return nullptr;
        auto it = fonts_.find(px);
        if (it != fonts_.end()) return it->second;
        CTFontRef f = CTFontCreateWithFontDescriptor(descriptor_, px, nullptr);
        fonts_[px] = f;
        return f;
    }
    CTFontRef hiraginoFor(int px) {
        auto it = hiragino_.find(px);
        if (it != hiragino_.end()) return it->second;
        CTFontRef f = CTFontCreateWithName(CFSTR("HiraginoSans-W3"), px, nullptr);
        hiragino_[px] = f;
        return f;
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

// FreeType with the game's or the font pack's face, then MS Gothic's substitute for Japanese text, then
// WOLF_FALLBACK_FONT.
class FreeTypeFont : public Font {
public:
    explicit FreeTypeFont(std::vector<FontSource> sources) : sources_(std::move(sources)) {
        for (auto &s : sources_) {
            FT_Face f = nullptr;
            if (FT_New_Memory_Face(library(), s.data.data(), FT_Long(s.data.size()), s.face, &f) == 0) faces_.push_back(f);
        }
    }
    ~FreeTypeFont() override {
        for (FT_Face f : faces_) FT_Done_Face(f);
    }

    bool glyph(uint32_t cp, int px, bool bold, GlyphImage &out) override {
        for (FT_Face f : faces_) {
            FT_UInt index = FT_Get_Char_Index(f, cp);
            if (!index) continue;
            FT_Set_Pixel_Sizes(f, 0, FT_UInt(px));
            // The font pack's MS Gothic substitute carries 1-bit bitmap strikes for small sizes; draw the outlines,
            // whose 8-bit coverage is what the text drawing expects.
            if (FT_Load_Glyph(f, index, FT_LOAD_NO_BITMAP)) continue;
            if (bold) FT_GlyphSlot_Embolden(f->glyph);
            if (FT_Render_Glyph(f->glyph, FT_RENDER_MODE_NORMAL)) continue;
            const FT_Bitmap &b = f->glyph->bitmap;
            if (b.pixel_mode != FT_PIXEL_MODE_GRAY) continue;
            out.w = int(b.width);
            out.h = int(b.rows);
            out.left = f->glyph->bitmap_left;
            out.top = f->glyph->bitmap_top;
            out.advance = int(f->glyph->advance.x >> 6);
            out.coverage.resize(size_t(out.w) * out.h);
            for (int y = 0; y < out.h; y++)
                std::memcpy(out.coverage.data() + size_t(y) * out.w, b.buffer + ptrdiff_t(y) * b.pitch, size_t(out.w));
            return true;
        }
        return false;
    }

    int ascent(int px) override {
        if (faces_.empty()) return px * 86 / 100;
        FT_Set_Pixel_Sizes(faces_.front(), 0, FT_UInt(px));
        return int(faces_.front()->size->metrics.ascender >> 6);
    }

private:
    std::vector<FontSource> sources_;  // FreeType reads the face data from these buffers
    std::vector<FT_Face> faces_;
};

#endif

}  // namespace

std::vector<std::string> fontFamilyNames(const std::vector<uint8_t> &d, int face) {
    std::vector<std::string> names;
    size_t base = 0;
    if (be32(d, 0) == 0x74746366) {  // 'ttcf', a collection
        if (face >= int(be32(d, 8))) return names;
        base = be32(d, 12 + size_t(face) * 4);
    }
    uint16_t tables = be16(d, base + 4);
    size_t nameAt = 0;
    for (uint16_t i = 0; i < tables; i++) {
        size_t rec = base + 12 + size_t(i) * 16;
        if (be32(d, rec) == 0x6E616D65) nameAt = be32(d, rec + 8);  // 'name'
    }
    if (!nameAt) return names;
    uint16_t count = be16(d, nameAt + 2);
    size_t strings = nameAt + be16(d, nameAt + 4);
    for (uint16_t i = 0; i < count; i++) {
        size_t rec = nameAt + 6 + size_t(i) * 12;
        uint16_t platform = be16(d, rec), nameId = be16(d, rec + 6);
        uint16_t len = be16(d, rec + 8), off = be16(d, rec + 10);
        if (nameId != 1 && nameId != 4 && nameId != 16) continue;
        size_t at = strings + off;
        if (at + len > d.size()) continue;
        std::string s;
        if (platform == 3 || platform == 0) {  // UTF-16BE
            for (size_t k = 0; k + 1 < len; k += 2) {
                uint32_t cp = be16(d, at + k);
                if (cp >= 0xD800 && cp < 0xDC00 && k + 3 < len) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (be16(d, at + k + 2) - 0xDC00);
                    k += 2;
                }
                appendUtf8(s, cp);
            }
        } else {
            s.assign(reinterpret_cast<const char *>(d.data() + at), len);
        }
        if (!s.empty()) names.push_back(s);
    }
    return names;
}

std::unique_ptr<Font> openFont(const GameFiles &files, const std::string &faceName) {
    FontSource primary;
    bool found = findGameFont(files, faceName, primary) || findPackFont(faceName, primary);
#if __APPLE__
    return std::make_unique<CoreTextFont>(found ? &primary : nullptr);
#else
    std::vector<FontSource> sources;
    if (found) sources.push_back(std::move(primary));
    FontSource gothic;
    if (findPackFont("MS Gothic", gothic)) sources.push_back(std::move(gothic));
    if (const char *fallback = std::getenv("WOLF_FALLBACK_FONT")) {
        FontSource f;
        f.data = readFile(fallback);
        if (!f.data.empty()) sources.push_back(std::move(f));
    }
    return std::make_unique<FreeTypeFont>(std::move(sources));
#endif
}

}  // namespace wolf
