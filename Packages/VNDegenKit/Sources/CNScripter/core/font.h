// Glyph rendering. NScripter draws every character in a fixed cell: a full-width character fills the font width and
// height that setwindow names, a half-width one half the width. A rasterizer fills such a cell with 8-bit coverage.
//
// Faces come from VNDegen's font pack (Engines/Shared/vnfont.h, docs/fonts.md): a script's `font` or `defaultfont`
// name through its table, and MS Gothic's substitute, NScripter's default, otherwise. The app initializes the table
// at launch through FontPack. Every lookup goes through makeRasterizer().
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace vnns {

struct Glyph {
    int w = 0, h = 0;
    std::vector<uint8_t> coverage;  // w * h, row-major
};

class FontRasterizer {
public:
    virtual ~FontRasterizer() = default;
    // Renders cp into a cellW x cellH cell, baseline and size as MS Gothic would place it at a cellH font height.
    virtual bool render(uint32_t cp, int cellW, int cellH, bool bold, Glyph &out) = 0;
};

using GlyphCallback = int (*)(void *ctx, uint32_t codepoint, int cellWidth, int cellHeight, int bold,
                              uint8_t *coverage);
// The host's rasterizer, if it set one (the Linux test host does); otherwise the platform's.
void setGlyphCallback(GlyphCallback fn, void *ctx);
std::unique_ptr<FontRasterizer> makeRasterizer(const std::string &faceName);

}  // namespace vnns
