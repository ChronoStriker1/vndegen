#include "ponsfont.h"

#include <cmath>
#include <cstring>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
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

// The font's table directory (the first font of a collection) lies in the file, and so does every table it lists.
static bool tablesInside(const Bytes &d) {
    if (d.size() < 12) return false;
    uint64_t at = 0;
    if (memcmp(d.data(), "ttcf", 4) == 0) {
        if (d.size() < 16) return false;
        at = be32(&d[12]);
        if (at + 12 > d.size()) return false;
    }
    uint64_t tables = be16(&d[at + 4]);
    if (at + 12 + tables * 16 > d.size()) return false;
    for (uint64_t t = 0; t < tables; t++) {
        const uint8_t *rec = &d[at + 12 + t * 16];
        if (uint64_t(be32(rec + 8)) + be32(rec + 12) > d.size()) return false;
    }
    return true;
}

bool PonsFace::load(Bytes data) {
    impl_->data = std::move(data);
    if (!tablesInside(impl_->data)) return false;
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
