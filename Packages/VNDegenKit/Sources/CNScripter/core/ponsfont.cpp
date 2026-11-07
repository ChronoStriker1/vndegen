#include "ponsfont.h"

#include <cmath>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
// stb_truetype clamps a read that would leave its CFF buffer, after asserting it can't; a game's font may make it.
#define STBTT_assert(x) ((void)0)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "../third_party/stb_truetype.h"
#pragma GCC diagnostic pop

namespace vnns {

struct PonsFace::Impl {
    Bytes data;
    stbtt_fontinfo info{};
    int ascender = 0, descender = 0, lineGap = 0, unitsPerEm = 1000;
    std::map<std::pair<uint32_t, int>, std::shared_ptr<const Glyph>> glyphs;
};

PonsFace::PonsFace() : impl_(std::make_unique<Impl>()) {}
PonsFace::~PonsFace() = default;

// stb_truetype reads a font's tables without checking their lengths, so before it sees a game's file: the table
// directory (the first font of a collection) and every table it lists lie in the file, the tables stb reads hold what
// it reads from them (head, hhea, maxp, hmtx, cmap and its subtables, loca), every loca entry points inside glyf, and
// composite glyphs nest at most 32 deep, without a cycle (stb follows them recursively). Reads that start there but
// run on (a glyph's points, a cmap's segment search and glyph array) end in the padding load adds. stb reads a CFF
// font's outlines with bounds of its own, the CFF table's length (a local change to stb).
static bool compositesNest(const uint8_t *glyf, uint64_t glyfSize, const std::function<uint64_t(uint64_t)> &loca,
                           uint64_t glyphs) {
    std::vector<uint8_t> depth(glyphs, 0);  // 0 not seen, 255 being walked, else 1 + the deepest component's depth
    std::function<int(uint64_t, int)> walk = [&](uint64_t g, int level) -> int {
        if (g >= glyphs) return 1;  // stb draws a glyph past the last as nothing
        if (depth[g] == 255 || level > 32) return -1;
        if (depth[g]) return depth[g];
        uint64_t start = loca(g), end = loca(g + 1);
        int deepest = 0;
        if (end > start && start + 10 <= glyfSize && int16_t(be16(glyf + start)) < 0) {
            depth[g] = 255;
            for (uint64_t at = start + 10; at + 4 <= glyfSize;) {
                uint16_t flags = be16(glyf + at), part = be16(glyf + at + 2);
                int d = walk(part, level + 1);
                if (d < 0) return -1;
                deepest = std::max(deepest, d);
                at += 4 + ((flags & 1) ? 4 : 2) + ((flags & 8) ? 2 : (flags & 0x40) ? 4 : (flags & 0x80) ? 8 : 0);
                if (!(flags & 0x20)) break;
            }
        }
        if (deepest >= 32) return -1;
        depth[g] = uint8_t(deepest + 1);
        return depth[g];
    };
    for (uint64_t g = 0; g < glyphs; g++)
        if (walk(g, 0) < 0) return false;
    return true;
}

static bool tablesInside(const Bytes &d) {
    if (d.size() < 12) return false;
    uint64_t at = 0;
    if (memcmp(d.data(), "ttcf", 4) == 0) {
        if (d.size() < 16) return false;
        at = be32(&d[12]);
        if (at + 12 > d.size()) return false;
    }
    uint64_t count = be16(&d[at + 4]);
    if (at + 12 + count * 16 > d.size()) return false;
    struct Table {
        const uint8_t *p = nullptr;
        uint64_t size = 0;
    };
    std::map<std::string, Table> tables;
    for (uint64_t t = 0; t < count; t++) {
        const uint8_t *rec = &d[at + 12 + t * 16];
        uint64_t off = be32(rec + 8), size = be32(rec + 12);
        if (off + size > d.size()) return false;
        tables[std::string(reinterpret_cast<const char *>(rec), 4)] = {d.data() + off, size};
    }
    auto has = [&](const char *tag, uint64_t least) { return tables.count(tag) && tables[tag].size >= least; };
    if (!has("head", 54) || !has("hhea", 36) || !has("maxp", 6) || !has("cmap", 4)) return false;
    uint64_t glyphs = be16(tables["maxp"].p + 4), metrics = be16(tables["hhea"].p + 34);
    if (metrics == 0 || metrics > glyphs) return false;
    if (!has("hmtx", metrics * 4 + (glyphs - metrics) * 2)) return false;
    const Table &cmap = tables["cmap"];
    uint64_t subtables = be16(cmap.p + 2);
    if (4 + subtables * 8 > cmap.size) return false;
    for (uint64_t i = 0; i < subtables; i++) {
        uint64_t off = be32(cmap.p + 4 + i * 8 + 4);
        if (off + 8 > cmap.size) return false;
        const uint8_t *sub = cmap.p + off;
        uint16_t format = be16(sub);
        // Formats 8 to 13 keep a 32-bit length after a reserved field, 14 right after the format, the rest 16 bits.
        uint64_t length = format == 14 ? be32(sub + 2) : format >= 8 ? be32(sub + 4) : be16(sub + 2);
        if (off + length > cmap.size) return false;
        if (format == 4 && (length < 14 || 16 + uint64_t(be16(sub + 6)) * 4 > length)) return false;
        if ((format == 12 || format == 13) && (length < 16 || 16 + uint64_t(be32(sub + 12)) * 12 > length)) return false;
        if (format == 6 && (length < 10 || 10 + uint64_t(be16(sub + 8)) * 2 > length)) return false;
        if (format == 0 && length < 262) return false;
    }
    if (tables.count("glyf")) {
        bool longLoca = be16(tables["head"].p + 50) != 0;
        if (!has("loca", (glyphs + 1) * (longLoca ? 4 : 2))) return false;
        const uint8_t *locaTable = tables["loca"].p;
        auto loca = [&](uint64_t g) {
            return longLoca ? uint64_t(be32(locaTable + g * 4)) : uint64_t(be16(locaTable + g * 2)) * 2;
        };
        for (uint64_t g = 0; g <= glyphs; g++)
            if (loca(g) > tables["glyf"].size) return false;
        if (!compositesNest(tables["glyf"].p, tables["glyf"].size, loca, glyphs)) return false;
    } else if (!tables.count("CFF ") || tables["CFF "].size >= 0x40000000) {
        return false;
    }
    return true;
}

// More than any read stb starts inside a checked table can run on: a simple glyph's contours, instructions, flags and
// points (under 460 KB), or a format 4 cmap's search and glyph array (under 400 KB).
static constexpr size_t kReadPadding = 1 << 20;

bool PonsFace::load(Bytes data) {
    impl_->data = std::move(data);
    if (!tablesInside(impl_->data)) return false;
    impl_->data.resize(impl_->data.size() + kReadPadding, 0);
    const unsigned char *d = impl_->data.data();
    int offset = stbtt_GetFontOffsetForIndex(d, 0);
    if (offset < 0 || !stbtt_InitFont(&impl_->info, d, offset)) return false;
    stbtt_GetFontVMetrics(&impl_->info, &impl_->ascender, &impl_->descender, &impl_->lineGap);
    // The em scale at one pixel is 1 / unitsPerEm.
    impl_->unitsPerEm = std::max(1, int(std::lround(1.0 / stbtt_ScaleForMappingEmToPixels(&impl_->info, 1.0f))));
    return true;
}

int PonsFace::ascent(int px) const { return int(std::ceil(double(impl_->ascender) * px / impl_->unitsPerEm)); }

int PonsFace::height(int px) const {
    return int(std::ceil(double(impl_->ascender - impl_->descender + impl_->lineGap) * px / impl_->unitsPerEm));
}

std::shared_ptr<const PonsFace::Glyph> PonsFace::glyph(uint32_t cp, int px) {
    auto key = std::make_pair(cp, px);
    if (auto it = impl_->glyphs.find(key); it != impl_->glyphs.end()) return it->second;
    if (impl_->glyphs.size() > 8192) impl_->glyphs.clear();
    auto made = std::make_shared<Glyph>();
    Glyph &g = *made;
    const stbtt_fontinfo *f = &impl_->info;
    int index = stbtt_FindGlyphIndex(f, int(cp));
    if (index < 0 || index >= f->numGlyphs) index = 0;  // a broken cmap's index past the font's glyphs
    float scale = stbtt_ScaleForMappingEmToPixels(f, float(px));
    int advance = 0, lsb = 0;
    stbtt_GetGlyphHMetrics(f, index, &advance, &lsb);
    // The advance in 26.6 fixed point, as a font engine scales it, then whole pixels, dropping the fraction.
    g.advance = int(std::lround(double(advance) * scale * 64)) >> 6;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(f, index, scale, scale, &x0, &y0, &x1, &y1);
    g.w = std::max(0, x1 - x0);
    g.h = std::max(0, y1 - y0);
    g.left = x0;
    g.top = -y0;
    if (g.w > 0 && g.h > 0 && g.w <= 4096 && g.h <= 4096) {
        g.coverage.assign(size_t(g.w) * size_t(g.h), 0);
        stbtt_MakeGlyphBitmap(f, g.coverage.data(), g.w, g.h, g.w, scale, scale, index);
    } else {
        g.w = g.h = 0;
    }
    impl_->glyphs.emplace(key, made);
    return made;
}

}  // namespace vnns
