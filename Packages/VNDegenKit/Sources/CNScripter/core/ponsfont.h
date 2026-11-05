// A Ponscripter game's own font files (h_mapfont), rasterized with stb_truetype the way the Umineko games' engine draws
// them (docs/nscripter-cleanroom.md): no hinting, at twice the font size, with whole-pixel advances at that size, and
// no kerning. Callers draw at twice the screen's resolution and halve the result.
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "common.h"

namespace vnns {

class PonsFace {
public:
    PonsFace();
    ~PonsFace();
    PonsFace(const PonsFace &) = delete;
    PonsFace &operator=(const PonsFace &) = delete;

    // Takes the font file's bytes. False when they aren't a font stb_truetype reads.
    bool load(Bytes data);

    struct Glyph {
        int advance = 0;          // whole pixels at the size asked for
        int left = 0, top = 0;    // bitmap's offset from the pen: right of it, and above the baseline
        int w = 0, h = 0;
        std::vector<uint8_t> coverage;  // w * h
    };
    // The glyph for cp at an em of px pixels. A character the font lacks gets the font's missing-glyph shape, as the
    // engine draws it; there is no fallback font.
    const Glyph &glyph(uint32_t cp, int px);
    // hhea ascender, and the height from ascender to descender plus the line gap, at an em of px pixels, rounded up.
    int ascent(int px) const;
    int height(int px) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace vnns
