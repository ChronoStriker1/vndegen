// Inspects and renders AliceSoft .fnl font libraries.
// Usage: s4fnl <file.fnl> info
//        s4fnl <file.fnl> render <face> <size> <utf8 text> <out.png> [downscale, default 4]
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "fnl.h"
#include "sjis.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

namespace {

// Counts the characters a size has among double-byte codes with lead bytes [first, last].
int countRange(const s4::FnlFont &font, int face, int size, int first, int last) {
    int n = 0;
    for (int lead = first; lead <= last; lead++)
        for (int trail = 0x40; trail <= 0xFC; trail++) n += font.has(face, size, uint16_t(lead << 8 | trail));
    return n;
}

int info(s4::FnlFont &font) {
    std::printf("%d faces\n", font.faceCount());
    for (int f = 0; f < font.faceCount(); f++) {
        std::printf("face %d:\n", f);
        for (int size : font.sizes(f)) {
            int ascii = 0, halfKana = 0;
            for (int c = 0x20; c <= 0x7E; c++) ascii += font.has(f, size, uint16_t(c));
            for (int c = 0xA1; c <= 0xDF; c++) halfKana += font.has(f, size, uint16_t(c));
            // Lead 0x82-0x83 holds JIS rows 3-6 (digits, Latin, hiragana, katakana, Greek); kanji start at 0x88.
            std::printf("  size %3d: %4d glyphs, ascent %3d | ascii %d, half-width kana %d, 0x82-0x83 %d, kanji %d\n",
                        size, font.glyphCount(f, size), font.ascent(f, size), ascii, halfKana,
                        countRange(font, f, size, 0x82, 0x83),
                        countRange(font, f, size, 0x88, 0x9F) + countRange(font, f, size, 0xE0, 0xEA));
        }
    }
    return 0;
}

int render(s4::FnlFont &font, int face, int size, const std::string &utf8, const char *out, int downscale) {
    const std::string text = s4::sjis::fromUtf8(utf8);
    struct Placed { s4::FnlGlyph g; int x; };
    std::vector<Placed> glyphs;
    int pen = 0, width = 0;
    for (size_t i = 0; i < text.size();) {
        const int len = s4::sjis::charLength(text, i);
        const auto *b = reinterpret_cast<const unsigned char *>(text.data() + i);
        const uint16_t code = len == 2 ? uint16_t(b[0] << 8 | b[1]) : b[0];
        int cpLen;
        const char32_t cp = s4::sjis::decodeAt(text, i, &cpLen);
        i += size_t(len);
        s4::FnlGlyph g;
        if (!font.glyph(face, size, code, g, downscale)) {
            std::fprintf(stderr, "no glyph for U+%04X (SJIS %04X)\n", unsigned(cp), code);
            pen += g.advance;
            continue;
        }
        width = std::max(width, pen + g.left + g.w);
        glyphs.push_back({g, pen});
        pen += g.advance;
    }
    width = std::max(width, pen);
    const int height = (size + downscale - 1) / downscale;
    if (width <= 0) {
        std::fprintf(stderr, "nothing to draw\n");
        return 1;
    }
    // Black text on white; glyph cells are full height, so each one starts at the line's top row.
    std::vector<uint8_t> gray(size_t(width) * height, 255);
    for (const Placed &p : glyphs)
        for (int y = 0; y < p.g.h && y < height; y++)
            for (int x = 0; x < p.g.w; x++) {
                uint8_t &px = gray[size_t(y) * width + p.x + p.g.left + x];
                px = uint8_t(std::min<int>(px, 255 - p.g.coverage[size_t(y) * p.g.w + x]));
            }
    std::printf("%dx%d, ascent %d\n", width, height, font.ascent(face, size, downscale));
    return stbi_write_png(out, width, height, 1, gray.data(), width) ? 0 : 1;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: s4fnl <file.fnl> info | render <face> <size> <text> <out.png> [downscale]\n");
        return 2;
    }
    s4::FnlFont font;
    if (!font.open(argv[1])) {
        std::fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }
    const std::string cmd = argv[2];
    if (cmd == "info") return info(font);
    if (cmd == "render" && argc >= 7)
        return render(font, std::atoi(argv[3]), std::atoi(argv[4]), argv[5], argv[6], argc >= 8 ? std::atoi(argv[7]) : 4);
    std::fprintf(stderr, "unknown command\n");
    return 2;
}
