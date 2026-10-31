// AliceSoft ".fnl" font libraries (header "FNA\0"). A library holds faces, each face holds pixel sizes, and each
// size holds a glyph table indexed by character. Glyphs are 1-bit bitmaps, zlib-compressed, drawn at large sizes
// (64 to 272 rows); the runtime box-filters them down (`downscale`) to get anti-aliased coverage. The format is
// described in prose in the clean-room notes (docs/system4-cleanroom.md).
#pragma once
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace s4 {

struct FnlGlyph {
    int w = 0, h = 0;
    int left = 0;     // from the pen position to the coverage's left edge (always 0: bitmaps start at the pen)
    int top = 0;      // from the baseline up to the coverage's top row (the cell top, so equal to ascent())
    int advance = 0;  // pen advance, rounded to output pixels
    std::vector<uint8_t> coverage;  // w * h, 0..255
};

class FnlFont {
public:
    // Reads the face, size and glyph tables. Glyph bitmaps are read from the file on demand.
    bool open(const std::string &path);
    int faceCount() const { return int(faces_.size()); }
    // The stored sizes of a face: the bitmap cell height in rows before downscaling.
    std::vector<int> sizes(int face) const;
    int glyphCount(int face, int size) const;
    // Whether the size has a bitmap for the character, without reading it.
    bool has(int face, int size, uint16_t sjisCode) const;
    // sjisCode: a single byte (0x20-0x7E, 0xA1-0xDF) or a CP932 lead byte << 8 | trail byte. downscale 1 returns
    // the stored bitmap as 0/255; 4 averages each 4x4 block (1 to 15). Returns false for a character the size does not have,
    // with out.advance still set when the table reserves a slot for it.
    bool glyph(int face, int size, uint16_t sjisCode, FnlGlyph &out, int downscale = 1);
    // Baseline distance from the cell top, in output pixels. The file has no reliable field for it, so it is
    // measured from the bottom of the "H" glyph.
    int ascent(int face, int size, int downscale = 1);

private:
    struct Record { uint16_t advance; uint32_t offset, length; };
    struct Size {
        int size;
        uint32_t unknown;  // second size field; tracks the descent in some faces, 0 in others
        std::vector<Record> glyphs;
        mutable int ascent = -1;  // measured on first use
    };
    const Size *findSize(int face, int size) const;
    bool readBitmap(const Size &s, const Record &r, std::vector<uint8_t> &bits, int &stride);

    std::ifstream file_;
    std::vector<std::vector<Size>> faces_;
};

// Glyph table index of a Shift-JIS code, or -1 when the code has no slot: ASCII 0x20-0x7E, then half-width kana
// 0xA1-0xDF, then JIS X 0208 rows of 94 cells each.
int fnlGlyphIndex(uint16_t sjisCode);

}  // namespace s4
