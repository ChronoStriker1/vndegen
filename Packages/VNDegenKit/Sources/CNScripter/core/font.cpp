#include "font.h"

#include "common.h"
#include "vnfont.h"

#include <algorithm>
#include <cmath>
#include <map>

#if __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>
#endif

namespace vnns {

namespace {

GlyphCallback gCallback = nullptr;
void *gCallbackCtx = nullptr;

class CallbackRasterizer : public FontRasterizer {
public:
    bool render(uint32_t cp, int cellW, int cellH, bool bold, Glyph &out) override {
        out.w = cellW;
        out.h = cellH;
        out.coverage.assign(size_t(cellW) * size_t(cellH), 0);
        return gCallback(gCallbackCtx, cp, cellW, cellH, bold ? 1 : 0, out.coverage.data()) != 0;
    }
};

// The font pack's substitute for a Windows face name (Shift-JIS, as scripts write it), or for MS Gothic, NScripter's
// default, when the script names none or one the table lacks.
struct FaceFile {
    std::string path;
    int face = 0;
};

bool resolveFace(const std::string &sjisName, FaceFile &out) {
    vnfont_match m;
    std::string name = sjisToUtf8(sjisName);
    if (name.empty() || !vnfont_resolve(name.c_str(), 0, 0, &m)) {
        if (!vnfont_resolve("MS Gothic", 0, 0, &m)) return false;
    }
    out.path = m.path;
    out.face = m.face;
    return true;
}

#if __APPLE__
// CoreText with the font pack's face (Ume Gothic for MS Gothic), then Hiragino for characters it lacks, then the
// system's fallback. Glyphs are scaled horizontally to the cell, as Windows does when a font is created with a width,
// and sit on MS Gothic's baseline (86% of the cell height down).
class CoreTextRasterizer : public FontRasterizer {
public:
    explicit CoreTextRasterizer(const std::string &sjisFace) {
        FaceFile file;
        if (!resolveFace(sjisFace, file)) return;
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, reinterpret_cast<const UInt8 *>(file.path.c_str()),
                                                               CFIndex(file.path.size()), false);
        if (!url) return;
        CFArrayRef descriptors = CTFontManagerCreateFontDescriptorsFromURL(url);
        CFRelease(url);
        if (!descriptors) return;
        CFIndex n = CFArrayGetCount(descriptors);
        if (n > 0) {
            descriptor_ = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(descriptors, std::min<CFIndex>(file.face, n - 1)));
            CFRetain(descriptor_);
        }
        CFRelease(descriptors);
    }
    ~CoreTextRasterizer() override {
        for (auto &[k, f] : fonts_) CFRelease(f);
        for (auto &[k, f] : fallbacks_) CFRelease(f);
        if (descriptor_) CFRelease(descriptor_);
    }
    bool render(uint32_t cp, int cellW, int cellH, bool bold, Glyph &out) override {
        out.w = cellW;
        out.h = cellH;
        out.coverage.assign(size_t(cellW) * size_t(cellH), 0);
        if (cellW <= 0 || cellH <= 0) return false;
        CTFontRef font = fontFor(cellH, bold);
        if (!font) return false;
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
        CTFontRef use = font;
        CTFontRef fallback = nullptr;
        if (!CTFontGetGlyphsForCharacters(font, chars, glyphs, n) || !glyphs[0]) {
            CTFontRef hiragino = hiraginoFor(cellH);
            if (hiragino && CTFontGetGlyphsForCharacters(hiragino, chars, glyphs, n) && glyphs[0]) {
                font = hiragino;
                use = hiragino;
            }
        }
        if (!glyphs[0]) {
            CFStringRef s = CFStringCreateWithCharacters(nullptr, chars, n);
            fallback = CTFontCreateForString(font, s, CFRangeMake(0, n));
            CFRelease(s);
            if (!fallback || !CTFontGetGlyphsForCharacters(fallback, chars, glyphs, n)) {
                if (fallback) CFRelease(fallback);
                return false;
            }
            use = fallback;
        }
        CGColorSpaceRef gray = CGColorSpaceCreateDeviceGray();
        CGContextRef ctx = CGBitmapContextCreate(out.coverage.data(), size_t(cellW), size_t(cellH), 8, size_t(cellW),
                                                 gray, kCGImageAlphaNone);
        CGColorSpaceRelease(gray);
        if (ctx) {
            bool half = cp < 0x100 || (cp >= 0xFF61 && cp <= 0xFF9F);
            double fullW = half ? cellW * 2.0 : cellW;
            CGContextSetGrayFillColor(ctx, 1, 1);
            CGContextSetGrayStrokeColor(ctx, 1, 1);
            CGContextSetShouldAntialias(ctx, true);
            if (bold) {
                CGContextSetTextDrawingMode(ctx, kCGTextFillStroke);
                CGContextSetLineWidth(ctx, std::max(0.5, cellH / 28.0));
            }
            // Centre the glyph's advance in the cell, then stretch to the requested width.
            CGSize adv;
            CTFontGetAdvancesForGlyphs(use, kCTFontOrientationHorizontal, glyphs, &adv, 1);
            double sx = fullW / cellH;
            double natural = half ? cellH / 2.0 : double(cellH);
            CGContextScaleCTM(ctx, sx, 1);
            CGPoint pos = CGPointMake((natural - adv.width) / 2, cellH * 0.14);
            CTFontDrawGlyphs(use, glyphs, &pos, 1, ctx);
            CGContextRelease(ctx);
        }
        // A bitmap context's memory starts with the top row, so the coverage is already top-down.
        if (fallback) CFRelease(fallback);
        return true;
    }

private:
    // Bold is drawn by stroking, as Windows emboldens fonts with no bold face, so one font serves both.
    CTFontRef fontFor(int size, bool) {
        if (auto it = fonts_.find(size); it != fonts_.end()) return it->second;
        CTFontRef f = descriptor_ ? CTFontCreateWithFontDescriptor(descriptor_, size, nullptr) : hiraginoFor(size);
        if (f && !descriptor_) CFRetain(f);
        fonts_[size] = f;
        return f;
    }
    CTFontRef hiraginoFor(int size) {
        if (auto it = fallbacks_.find(size); it != fallbacks_.end()) return it->second;
        CTFontRef f = CTFontCreateWithName(CFSTR("HiraginoSans-W3"), size, nullptr);
        fallbacks_[size] = f;
        return f;
    }
    CTFontDescriptorRef descriptor_ = nullptr;
    std::map<int, CTFontRef> fonts_, fallbacks_;
};
#endif

}  // namespace

void setGlyphCallback(GlyphCallback fn, void *ctx) {
    gCallback = fn;
    gCallbackCtx = ctx;
}

std::unique_ptr<FontRasterizer> makeRasterizer(const std::string &faceName) {
    if (gCallback) return std::make_unique<CallbackRasterizer>();
#if __APPLE__
    return std::make_unique<CoreTextRasterizer>(faceName);
#else
    (void)faceName;
    return nullptr;
#endif
}

}  // namespace vnns
