// Glyphs for NekoSDK text. init.ini's MsgFont number names a Windows font; the face comes from VNDegen's Windows font
// pack (Engines/Shared/vnfont.h), then the system. Apple systems draw glyphs with CoreText; the Linux test host uses
// FreeType. Adapted from the System 4 runtime's font.cpp.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace neko {

struct GlyphImage {
    int w = 0, h = 0;
    int left = 0;     // from the pen position to the coverage's left edge
    int top = 0;      // from the baseline up to the coverage's top row
    int advance = 0;
    std::vector<uint8_t> coverage;  // w * h
};

// face: init.ini's MsgFont number. Thread safe; glyphs are cached.
bool glyph(int face, uint32_t codepoint, int px, bool bold, GlyphImage &out);
int fontAscent(int face, int px);
// Where the font pack is: its folder holding fonts.tsv. Call before drawing text.
void setFontPackDir(const std::string &dir);

}  // namespace neko
