#include "fnl.h"

#include <zlib.h>

#include <algorithm>
#include <string_view>

namespace s4 {

namespace {

uint32_t u32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }

// Glyph rows are padded to whole 32-bit words.
int rowStride(int advance) { return (advance + 31) / 32 * 4; }

}  // namespace

int fnlGlyphIndex(uint16_t code) {
    if (code < 0x100) {
        if (code >= 0x20 && code <= 0x7E) return code - 0x20;
        if (code >= 0xA1 && code <= 0xDF) return 95 + code - 0xA1;
        return -1;
    }
    int lead = code >> 8, trail = code & 0xFF;
    if (lead < 0x81 || (lead > 0x9F && lead < 0xE0) || lead > 0xFC) return -1;
    if (trail < 0x40 || trail == 0x7F || trail > 0xFC) return -1;
    if (lead >= 0xE0) lead -= 0x40;
    // Each lead byte covers two JIS rows: trail 0x40-0x9E is the odd row, 0x9F-0xFC the even one.
    int row = (lead - 0x81) * 2, cell;
    if (trail >= 0x9F) {
        row += 1;
        cell = trail - 0x9F;
    } else {
        cell = trail - 0x40 - (trail > 0x7F);
    }
    return 158 + row * 94 + cell;
}

bool FnlFont::open(const std::string &path) {
    faces_.clear();
    file_.close();
    file_.open(path, std::ios::binary);
    if (!file_) return false;
    file_.seekg(0, std::ios::end);
    const uint64_t fileSize = uint64_t(file_.tellg());
    file_.seekg(0);
    uint64_t pos = 0;
    auto read = [&](void *dst, size_t n) {
        if (pos + n > fileSize || !file_.read(static_cast<char *>(dst), std::streamsize(n))) return false;
        pos += n;
        return true;
    };
    auto readU32 = [&](uint32_t &v) {
        uint8_t b[4];
        if (!read(b, 4)) return false;
        v = u32(b);
        return true;
    };

    // "FNA\0", u32 0, u32 file size, u32 index size (not relied on), u32 face count.
    uint8_t header[20];
    if (!read(header, sizeof header) || std::string_view(reinterpret_cast<char *>(header), 4) != std::string_view("FNA\0", 4))
        return false;
    const uint32_t faceCount = u32(header + 16);
    if (faceCount > 256) return false;
    faces_.resize(faceCount);
    for (auto &face : faces_) {
        uint32_t sizeCount;
        if (!readU32(sizeCount) || sizeCount > 256) return false;
        face.resize(sizeCount);
        for (auto &s : face) {
            uint8_t sh[12];
            if (!read(sh, sizeof sh)) return false;
            const uint32_t height = u32(sh), glyphs = u32(sh + 8);
            s.size = int(height);
            s.unknown = u32(sh + 4);
            if (height == 0 || height > 4096 || uint64_t(glyphs) * 10 > fileSize - pos) return false;
            std::vector<uint8_t> raw(size_t(glyphs) * 10);
            if (!read(raw.data(), raw.size())) return false;
            s.glyphs.resize(glyphs);
            for (uint32_t i = 0; i < glyphs; i++) {
                const uint8_t *r = &raw[i * 10];
                Record &g = s.glyphs[i];
                g.advance = uint16_t(r[0] | r[1] << 8);
                g.offset = u32(r + 2);
                g.length = u32(r + 6);
                if (uint64_t(g.offset) + g.length > fileSize) return false;
            }
        }
    }
    return true;
}

std::vector<int> FnlFont::sizes(int face) const {
    std::vector<int> out;
    if (face >= 0 && face < faceCount())
        for (const Size &s : faces_[face]) out.push_back(s.size);
    return out;
}

int FnlFont::glyphCount(int face, int size) const {
    const Size *s = findSize(face, size);
    if (!s) return 0;
    return int(std::count_if(s->glyphs.begin(), s->glyphs.end(), [](const Record &r) { return r.length > 0; }));
}

bool FnlFont::has(int face, int size, uint16_t sjisCode) const {
    const Size *s = findSize(face, size);
    const int index = fnlGlyphIndex(sjisCode);
    return s && index >= 0 && index < int(s->glyphs.size()) && s->glyphs[index].length > 0;
}

const FnlFont::Size *FnlFont::findSize(int face, int size) const {
    if (face < 0 || face >= faceCount()) return nullptr;
    for (const Size &s : faces_[face])
        if (s.size == size) return &s;
    return nullptr;
}

// Inflates a glyph into rows ordered top to bottom (the file stores them bottom to top), one bit per pixel, most
// significant bit first.
bool FnlFont::readBitmap(const Size &s, const Record &r, std::vector<uint8_t> &bits, int &stride) {
    std::vector<uint8_t> packed(r.length);
    file_.clear();
    file_.seekg(std::streamoff(r.offset));
    if (!file_.read(reinterpret_cast<char *>(packed.data()), std::streamsize(packed.size()))) return false;
    stride = rowStride(r.advance);
    const size_t expected = size_t(stride) * size_t(s.size);
    std::vector<uint8_t> flipped(expected);
    uLongf outLen = uLongf(expected);
    if (uncompress(flipped.data(), &outLen, packed.data(), uLong(packed.size())) != Z_OK || outLen != expected)
        return false;
    bits.resize(expected);
    for (int y = 0; y < s.size; y++)
        std::copy_n(&flipped[size_t(s.size - 1 - y) * stride], stride, &bits[size_t(y) * stride]);
    return true;
}

bool FnlFont::glyph(int face, int size, uint16_t sjisCode, FnlGlyph &out, int downscale) {
    out = FnlGlyph();
    const Size *s = findSize(face, size);
    const int index = fnlGlyphIndex(sjisCode);
    if (!s || index < 0 || index >= int(s->glyphs.size()) || downscale < 1 || downscale > 15)
        return false;
    const int d = downscale;
    const Record &r = s->glyphs[index];
    out.advance = (r.advance + d / 2) / d;
    if (r.length == 0) return false;

    std::vector<uint8_t> bits;
    int stride;
    if (!readBitmap(*s, r, bits, stride)) return false;
    auto bit = [&](int x, int y) { return (bits[size_t(y) * stride + (x >> 3)] >> (7 - (x & 7))) & 1; };

    // A few glyphs draw past their advance into the row padding, so the bitmap is as wide as its ink requires.
    int width = r.advance;
    for (int y = 0; y < s->size; y++)
        for (int x = stride * 8 - 1; x >= width; x--)
            if (bit(x, y)) {
                width = x + 1;
                break;
            }

    out.w = (width + d - 1) / d;
    out.h = (s->size + d - 1) / d;
    out.top = ascent(face, size, d);
    out.coverage.assign(size_t(out.w) * out.h, 0);
    for (int y = 0; y < s->size; y++)
        for (int x = 0; x < width; x++)
            if (bit(x, y)) out.coverage[size_t(y / d) * out.w + x / d]++;
    // Each output pixel averages a d x d block; blocks past the bitmap's edge count the missing pixels as empty.
    for (uint8_t &c : out.coverage) c = uint8_t(c * 255 / (d * d));
    return true;
}

int FnlFont::ascent(int face, int size, int downscale) {
    const Size *found = findSize(face, size);
    if (!found || downscale < 1) return 0;
    const Size &s = *found;
    if (s.ascent < 0) {
        // The row under the lowest ink of "H" is the baseline. A face without "H" falls back to 80% of the cell.
        s.ascent = s.size * 4 / 5;
        const int index = fnlGlyphIndex('H');
        std::vector<uint8_t> bits;
        int stride;
        if (index < int(s.glyphs.size()) && s.glyphs[index].length && readBitmap(s, s.glyphs[index], bits, stride)) {
            for (int y = s.size - 1; y >= 0; y--)
                if (const uint8_t *row = bits.data() + size_t(y) * stride;
                    std::any_of(row, row + stride, [](uint8_t b) { return b != 0; })) {
                    s.ascent = y + 1;
                    break;
                }
        }
    }
    return (s.ascent + downscale / 2) / downscale;
}

}  // namespace s4
