#include "text.h"

#include "game.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace wolf {

namespace {

uint32_t decodeUtf8(const std::string &s, size_t &i) {
    auto c = static_cast<unsigned char>(s[i]);
    int n = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    uint32_t cp = n == 1 ? c : n == 2 ? c & 0x1F : n == 3 ? c & 0x0F : c & 0x07;
    for (int k = 1; k < n && i + k < s.size(); k++) cp = cp << 6 | (static_cast<unsigned char>(s[i + k]) & 0x3F);
    i += n;
    return cp;
}

// Reads "name[...]" at i (just after the backslash). Returns the bracket's contents, or nullopt.
bool codeWithArg(const std::string &s, size_t i, const char *name, std::string &arg, size_t &next) {
    size_t n = std::strlen(name);
    if (s.compare(i, n, name) != 0 || i + n >= s.size() || s[i + n] != '[') return false;
    size_t close = s.find(']', i + n);
    if (close == std::string::npos) return false;
    arg = s.substr(i + n + 1, close - i - n - 1);
    next = close + 1;
    return true;
}

}  // namespace

std::vector<TextToken> parseText(const std::string &s) {
    std::vector<TextToken> out;
    size_t i = 0;
    while (i < s.size()) {
        char c = s[i];
        if (c == '\r') {
            i++;
            continue;
        }
        if (c == '\n') {
            out.push_back({TextToken::Newline});
            i++;
            continue;
        }
        if (c == '<' && i + 2 < s.size() && s[i + 2] == '>' && (s[i + 1] == 'L' || s[i + 1] == 'C' || s[i + 1] == 'R')) {
            out.push_back({TextToken::Align, 0, s[i + 1] == 'L' ? 0 : s[i + 1] == 'C' ? 1 : 2});
            i += 3;
            continue;
        }
        if (c != '\\' || i + 1 >= s.size()) {
            out.push_back({TextToken::Char, decodeUtf8(s, i)});
            continue;
        }
        size_t at = i + 1, next = 0;
        std::string arg;
        struct Code {
            const char *name;
            TextToken::Kind kind;
        };
        static const Code numeric[] = {
            {"sp", TextToken::Speed},  {"ax", TextToken::AbsX},      {"ay", TextToken::AbsY},
            {"mx", TextToken::MoveX},  {"my", TextToken::MoveY},     {"c", TextToken::Color},
            {"f", TextToken::Size},    {"i", TextToken::Icon},       {"space", TextToken::LineSpace},
            {"-", TextToken::Tighten}, {"cE", TextToken::EdgeColor}, {"font", TextToken::Face},
            {"m", TextToken::LineMax},
        };
        bool matched = false;
        if (codeWithArg(s, at, "r", arg, next)) {
            size_t comma = arg.find(',');
            TextToken t{TextToken::Ruby};
            t.base = arg.substr(0, comma);
            t.ruby = comma == std::string::npos ? "" : arg.substr(comma + 1);
            out.push_back(t);
            i = next;
            continue;
        }
        for (auto &code : numeric) {
            if (codeWithArg(s, at, code.name, arg, next)) {
                TextToken t{code.kind};
                t.value = std::atoi(arg.c_str());
                out.push_back(t);
                i = next;
                matched = true;
                break;
            }
        }
        if (matched) continue;
        switch (s[at]) {
            case '\\': out.push_back({TextToken::Char, '\\'}); i = at + 1; continue;
            case '.': out.push_back({TextToken::Wait, 0, 15}); i = at + 1; continue;
            case '|': out.push_back({TextToken::Wait, 0, 60}); i = at + 1; continue;
            case '^': out.push_back({TextToken::AutoClose}); i = at + 1; continue;
            case '>': out.push_back({TextToken::Instant, 0, 1}); i = at + 1; continue;
            case '<': out.push_back({TextToken::Instant, 0, 0}); i = at + 1; continue;
            case 'n': out.push_back({TextToken::Newline}); i = at + 1; continue;
            case 'E': out.push_back({TextToken::Edge, 0, 1}); i = at + 1; continue;
            case 'N': out.push_back({TextToken::Edge, 0, 0}); i = at + 1; continue;
            default: break;
        }
        // Codes this runtime draws nothing for, such as \A+ or \cS[n]: skip the name and any bracket.
        size_t j = at;
        while (j < s.size() && std::isalpha(static_cast<unsigned char>(s[j]))) j++;
        if (j == at) j++;
        if (j < s.size() && s[j] == '[') {
            size_t close = s.find(']', j);
            j = close == std::string::npos ? j : close + 1;
        }
        i = j;
    }
    return out;
}

size_t visibleChars(const std::string &text) {
    size_t n = 0;
    for (auto &t : parseText(text)) {
        if (t.kind == TextToken::Char || t.kind == TextToken::Icon) n++;
        else if (t.kind == TextToken::Ruby) n += visibleChars(t.base);
    }
    return n;
}

TextStyle baseTextStyle(const Game &game) {
    TextStyle s;
    // Wolf sizes message text to the screen: 18 pixels at 640 by 480.
    s.size = std::max(12, int(std::lround(game.screenHeight() * 18.0 / 480)));
    const auto &settings = game.gameDat().settings;
    if (settings.size() > 11) s.lineSpacing = settings[11];
    if (settings.size() > 10) s.letterSpacing = 0;
    if (game.screenWidth() == 320 && game.screenHeight() == 240) s.unit = 2;
    return s;
}

TextRenderer::TextRenderer(const Game &game, ImageCache *images) : game_(game), images_(images) {}

Font &TextRenderer::font(int face) {
    const auto &names = game_.gameDat().fonts;
    // A sub font the game leaves unnamed draws with the base font.
    if (face < 0 || face >= int(names.size()) || (face > 0 && names[face].empty())) face = 0;
    if (!fonts_[face]) fonts_[face] = openFont(game_.files(), names[face]);
    return *fonts_[face];
}

const GlyphImage *TextRenderer::glyph(uint32_t cp, int size, int face) {
    auto key = std::make_tuple(cp, size, face);
    auto it = glyphs_.find(key);
    if (it != glyphs_.end()) return &it->second;
    GlyphImage g;
    if (!font(face).glyph(cp, size, false, g)) {
        g = GlyphImage{};
        g.advance = cp < 0x80 ? size / 2 : size;
    }
    return &glyphs_.emplace(key, std::move(g)).first->second;
}

struct TextRenderer::Layout {
    struct Item {
        float x;
        int y;  // the line's top
        int size, face, segment;
        std::array<uint8_t, 3> color, edgeColor;
        bool edge;
        const GlyphImage *g = nullptr;
        BitmapPtr icon;
    };
    // A run of text with one alignment on one line. <C> and <R> place a segment within the widest line.
    struct Segment {
        int align;
        float start, end;
    };
    std::vector<Item> items;
    std::vector<Segment> segments;
    int w = 0, h = 0;
    size_t chars = 0;
};

TextRenderer::Layout TextRenderer::layout(const std::string &text, const TextStyle &style, size_t visible) {
    Layout out;
    const float unit = float(std::max(1, style.unit));
    float x = 0, spacing = float(style.letterSpacing), width = 0;
    int lineTop = 0, size = style.size, lineSize = size, lineSpacing = style.lineSpacing, height = 0, face = 0;
    auto color = style.color;
    std::array<uint8_t, 3> edgeColor{0, 0, 0};
    bool edge = false;
    size_t shown = 0;
    out.segments.push_back({0, 0, 0});

    auto extend = [&] {
        Layout::Segment &seg = out.segments.back();
        seg.end = std::max(seg.end, x);
        width = std::max(width, x);
    };
    auto place = [&](uint32_t cp, int sz) {
        const GlyphImage *g = glyph(cp, sz, face);
        if (shown < visible)
            out.items.push_back({x, lineTop, sz, face, int(out.segments.size()) - 1, color, edgeColor, edge, g, nullptr});
        shown++;
        x += g->advance + spacing;
        lineSize = std::max(lineSize, sz);
        extend();
        height = std::max(height, lineTop + lineSize);
    };

    for (auto &t : parseText(text)) {
        switch (t.kind) {
            case TextToken::Char: place(t.cp, size); break;
            case TextToken::Newline:
                lineTop += lineSize + lineSpacing;
                lineSize = size;
                x = 0;
                out.segments.push_back({out.segments.back().align, 0, 0});
                break;
            case TextToken::Align:
                out.segments.push_back({t.value, x, x});
                break;
            case TextToken::Color: color = game_.textColor(t.value); break;
            case TextToken::Size: size = std::max(4, t.value); break;
            case TextToken::LineSpace: lineSpacing = t.value; break;
            case TextToken::Tighten: spacing = float(style.letterSpacing) - t.value / unit; break;
            case TextToken::Edge: edge = t.value != 0; break;
            case TextToken::EdgeColor: edgeColor = game_.textColor(t.value); break;
            case TextToken::Face: face = std::clamp(t.value, 0, 3); break;
            case TextToken::LineMax:
                lineSize = std::max(lineSize, t.value);
                height = std::max(height, lineTop + lineSize);
                break;
            // \ax[n] also widens the text to n, which the standard systems use to right-align a later line.
            case TextToken::AbsX: x = t.value / unit; extend(); break;
            case TextToken::AbsY: lineTop = int(std::lround(t.value / unit)); break;
            case TextToken::MoveX: x += t.value / unit; extend(); break;
            case TextToken::MoveY: lineTop += int(std::lround(t.value / unit)); break;
            case TextToken::Icon: {
                BitmapPtr icon;
                if (images_) {
                    char path[64];
                    std::snprintf(path, sizeof path, "BasicData/icon%03d.png", t.value);
                    icon = images_->get(path);
                }
                if (shown < visible)
                    out.items.push_back({x, lineTop, size, face, int(out.segments.size()) - 1, color, edgeColor, edge,
                                         nullptr, icon});
                shown++;
                x += size + spacing;
                extend();
                height = std::max(height, lineTop + size);
                break;
            }
            case TextToken::Ruby: {
                float start = x;
                size_t i = 0;
                while (i < t.base.size()) place(decodeUtf8(t.base, i), size);
                // The reading sits above the base text, at half size, centred over it.
                int rubySize = std::max(4, size / 2), rubyWidth = 0;
                std::vector<uint32_t> cps;
                for (size_t k = 0; k < t.ruby.size();) cps.push_back(decodeUtf8(t.ruby, k));
                for (uint32_t cp : cps) rubyWidth += glyph(cp, rubySize, face)->advance;
                float rx = start + (x - start - rubyWidth) / 2;
                for (uint32_t cp : cps) {
                    const GlyphImage *g = glyph(cp, rubySize, face);
                    if (shown <= visible)
                        out.items.push_back({rx, lineTop - rubySize, rubySize, face, int(out.segments.size()) - 1,
                                             color, edgeColor, edge, g, nullptr});
                    rx += g->advance;
                }
                break;
            }
            default: break;
        }
    }
    out.w = int(std::ceil(width));
    out.h = height;
    out.chars = shown;
    return out;
}

std::array<int, 2> TextRenderer::measure(const std::string &text, const TextStyle &style) {
    Layout l = layout(text, style, 0);
    return {l.w, l.h};
}

TextBlock TextRenderer::render(const std::string &text, const TextStyle &style, size_t visible) {
    Layout l = layout(text, style, visible);
    TextBlock block;
    block.chars = l.chars;
    // Shadows and outlines reach a pixel past the text; ruby may sit above the first line.
    int pad = block.origin;
    int minY = 0;
    for (auto &it : l.items) minY = std::min(minY, it.y);
    int w = std::max(1, l.w + pad * 2 + 2), h = std::max(1, l.h - minY + pad * 2);
    block.bitmap = Bitmap(w, h);
    Canvas canvas(block.bitmap);

    auto blendCoverage = [&](const GlyphImage &g, int gx, int gy, std::array<uint8_t, 3> c, int scale) {
        for (int yy = 0; yy < g.h; yy++) {
            int ty = gy + yy;
            if (ty < 0 || ty >= h) continue;
            uint8_t *row = block.bitmap.row(ty);
            for (int xx = 0; xx < g.w; xx++) {
                int tx = gx + xx;
                if (tx < 0 || tx >= w) continue;
                int a = g.coverage[size_t(yy) * g.w + xx] * scale / 256;
                if (!a) continue;
                uint8_t *d = row + tx * 4;
                int da = d[3], outA = a + da * (255 - a) / 255;
                if (!outA) continue;
                for (int ch = 0; ch < 3; ch++) d[ch] = uint8_t((c[ch] * a + d[ch] * da * (255 - a) / 255) / outA);
                d[3] = uint8_t(outA);
            }
        }
    };

    for (auto &it : l.items) {
        const Layout::Segment &seg = l.segments[it.segment];
        float shift = seg.align == 2 ? l.w - seg.end : seg.align == 1 ? (l.w - (seg.end - seg.start)) / 2 - seg.start : 0;
        int left = int(std::lround(it.x + shift)) + pad, top = it.y - minY + pad;
        if (it.icon) {
            if (!it.icon->empty())
                canvas.draw(*it.icon, {0, 0, it.icon->w, it.icon->h},
                            {float(left), float(top), float(it.size), float(it.size)});
            continue;
        }
        if (!it.g || it.g->coverage.empty()) continue;
        int gx = left + it.g->left, gy = top + font(it.face).ascent(it.size) - it.g->top;
        if (it.edge) {
            // \E outlines each character: the edge colour one pixel out in every direction, then the character.
            for (int oy = -1; oy <= 1; oy++)
                for (int ox = -1; ox <= 1; ox++)
                    if (ox || oy) blendCoverage(*it.g, gx + ox, gy + oy, it.edgeColor, 256);
        } else if (style.shadow) {
            blendCoverage(*it.g, gx + 1, gy + 1, {0, 0, 0}, 192);
        }
        blendCoverage(*it.g, gx, gy, it.color, 256);
    }
    return block;
}

}  // namespace wolf
