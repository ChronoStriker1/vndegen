#include "font.h"

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

#if __APPLE__
// CoreText with the system's Japanese gothic face. Glyphs are scaled horizontally to the cell, as Windows does when
// a font is created with a width, and sit on MS Gothic's baseline (86% of the cell height down).
class CoreTextRasterizer : public FontRasterizer {
public:
    explicit CoreTextRasterizer(std::string face) : face_(std::move(face)) {}
    ~CoreTextRasterizer() override {
        for (auto &[k, f] : fonts_) CFRelease(f);
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
    CTFontRef fontFor(int size, bool bold) {
        int key = size * 2 + (bold ? 1 : 0);
        if (auto it = fonts_.find(key); it != fonts_.end()) return it->second;
        CFStringRef name = CFStringCreateWithCString(nullptr, face_.c_str(), kCFStringEncodingUTF8);
        CTFontRef f = CTFontCreateWithName(name, size, nullptr);
        CFRelease(name);
        fonts_[key] = f;
        return f;
    }
    std::string face_;
    std::map<int, CTFontRef> fonts_;
};
#endif

}  // namespace

void setGlyphCallback(GlyphCallback fn, void *ctx) {
    gCallback = fn;
    gCallbackCtx = ctx;
}

std::unique_ptr<FontRasterizer> makeRasterizer(const std::string &faceName) {
    (void)faceName;  // The font pack will map Windows face names here; until then one gothic face serves all.
    if (gCallback) return std::make_unique<CallbackRasterizer>();
#if __APPLE__
    return std::make_unique<CoreTextRasterizer>("HiraginoSans-W3");
#else
    return nullptr;
#endif
}

}  // namespace vnns
