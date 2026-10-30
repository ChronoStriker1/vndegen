// Glyphs for Wolf's text. A game names its font in Game.dat. The font comes from a font file the game ships whose
// family name matches, then from VNDegen's Windows font pack (Engines/Shared/vnfont.h) for Windows names such as
// MS Gothic, then from the system. Apple systems draw glyphs with CoreText; the Linux test host uses FreeType.
#pragma once

#include "files.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wolf {

struct GlyphImage {
    int w = 0, h = 0;      // coverage size
    int left = 0;          // from the pen position to the coverage's left edge
    int top = 0;           // from the baseline up to the coverage's top row
    int advance = 0;       // how far the pen moves
    std::vector<uint8_t> coverage;  // w * h, 0 to 255
};

class Font {
public:
    virtual ~Font() = default;
    // The glyph for a character at a pixel size (the em height). False when no face has it.
    virtual bool glyph(uint32_t codepoint, int px, bool bold, GlyphImage &out) = 0;
    // The distance from the top of a line to its baseline at a pixel size.
    virtual int ascent(int px) = 0;
};

// The font for a face name from Game.dat. Never null: a face that can't be found draws with the system's fonts.
std::unique_ptr<Font> openFont(const GameFiles &files, const std::string &faceName);

// The family names a TrueType or OpenType file's name table gives, in every language, UTF-8. For matching a game's
// own fonts by the name Game.dat uses.
std::vector<std::string> fontFamilyNames(const std::vector<uint8_t> &file, int face = 0);

}  // namespace wolf
