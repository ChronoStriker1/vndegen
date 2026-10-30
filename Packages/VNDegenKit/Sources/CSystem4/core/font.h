// Glyphs for System 4 text. Games ask for a gothic or a mincho face by number, or for a Windows font by name; the
// face comes from VNDegen's Windows font pack (Engines/Shared/vnfont.h), then the system. Apple systems draw glyphs
// with CoreText; the Linux test host uses FreeType. Adapted from the Wolf RPG runtime's font.cpp.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace s4 {

struct GlyphImage {
    int w = 0, h = 0;
    int left = 0;     // from the pen position to the coverage's left edge
    int top = 0;      // from the baseline up to the coverage's top row
    int advance = 0;
    std::vector<uint8_t> coverage;  // w * h
};

// face: 0 gothic (MS Gothic), 1 mincho (MS Mincho). Thread safe; glyphs are cached.
bool glyph(int face, uint32_t codepoint, int px, bool bold, GlyphImage &out);
int fontAscent(int face, int px);
// Where the font pack is: its folder holding fonts.tsv. Call before drawing text.
void setFontPackDir(const std::string &dir);

}  // namespace s4
