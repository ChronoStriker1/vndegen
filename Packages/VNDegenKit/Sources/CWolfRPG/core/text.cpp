#include "text.h"

#include "game.h"

#include <algorithm>
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
            {"sp", TextToken::Speed}, {"ax", TextToken::AbsX}, {"ay", TextToken::AbsY}, {"mx", TextToken::MoveX},
            {"my", TextToken::MoveY}, {"c", TextToken::Color}, {"f", TextToken::Size}, {"i", TextToken::Icon},
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
            default: break;
        }
        // Codes this runtime draws nothing for, such as \font[n] or \E: skip the name and any bracket.
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

TextRenderer::TextRenderer(const Game &game, ImageCache &images) : game_(game), images_(images) {}

Font &TextRenderer::font() {
    if (!font_) font_ = openFont(game_.files(), game_.gameDat().fonts[0]);
    return *font_;
}

const GlyphImage *TextRenderer::glyph(uint32_t cp, int size) {
    auto key = std::make_pair(cp, size);
    auto it = glyphs_.find(key);
    if (it != glyphs_.end()) return &it->second;
    GlyphImage g;
    if (!font().glyph(cp, size, false, g)) {
        g = GlyphImage{};
        g.advance = cp < 0x80 ? size / 2 : size;
    }
    return &glyphs_.emplace(key, std::move(g)).first->second;
}

TextBlock TextRenderer::render(const std::string &text, const TextStyle &style, size_t visible) {
    struct Item {
        int x, y;            // pen position: x, and the line's top
        int size;
        std::array<uint8_t, 3> color;
        const GlyphImage *g = nullptr;
        BitmapPtr icon;
    };
    std::vector<Item> items;
    auto tokens = parseText(text);
    int x = 0, lineTop = 0, size = style.size, maxW = 0, maxH = 0;
    auto color = style.color;
    size_t shown = 0;
    int lineSize = size;

    auto place = [&](uint32_t cp, int sz) {
        const GlyphImage *g = glyph(cp, sz);
        if (shown < visible) items.push_back({x, lineTop, sz, color, g, nullptr});
        shown++;
        x += g->advance + style.letterSpacing;
        lineSize = std::max(lineSize, sz);
        maxW = std::max(maxW, x);
        maxH = std::max(maxH, lineTop + lineSize + style.lineSpacing);
    };

    for (auto &t : tokens) {
        switch (t.kind) {
            case TextToken::Char: place(t.cp, size); break;
            case TextToken::Newline:
                lineTop += lineSize + style.lineSpacing;
                lineSize = size;
                x = 0;
                break;
            case TextToken::Color: color = game_.textColor(t.value); break;
            case TextToken::Size: size = std::max(4, t.value); break;
            case TextToken::AbsX: x = t.value; break;
            case TextToken::AbsY: lineTop = t.value; break;
            case TextToken::MoveX: x += t.value; break;
            case TextToken::MoveY: lineTop += t.value; break;
            case TextToken::Icon: {
                char path[64];
                std::snprintf(path, sizeof path, "BasicData/icon%03d.png", t.value);
                BitmapPtr icon = images_.get(path);
                if (shown < visible) items.push_back({x, lineTop, size, color, nullptr, icon});
                shown++;
                x += size + style.letterSpacing;
                maxW = std::max(maxW, x);
                maxH = std::max(maxH, lineTop + size + style.lineSpacing);
                break;
            }
            case TextToken::Ruby: {
                int start = x;
                size_t i = 0;
                while (i < t.base.size()) place(decodeUtf8(t.base, i), size);
                // The reading sits above the base text, at half size, centred over it.
                int rubySize = std::max(4, size / 2), width = 0;
                std::vector<uint32_t> cps;
                for (size_t k = 0; k < t.ruby.size();) cps.push_back(decodeUtf8(t.ruby, k));
                for (uint32_t cp : cps) width += glyph(cp, rubySize)->advance;
                int rx = start + (x - start - width) / 2;
                for (uint32_t cp : cps) {
                    const GlyphImage *g = glyph(cp, rubySize);
                    if (shown <= visible) items.push_back({rx, lineTop - rubySize, rubySize, color, g, nullptr});
                    rx += g->advance;
                }
                break;
            }
            default: break;
        }
    }

    TextBlock block;
    block.chars = shown;
    int pad = style.shadow ? 1 : 0;
    // Ruby may sit above the first line; the bitmap starts at the highest point drawn.
    int minY = 0;
    for (auto &it : items) minY = std::min(minY, it.y);
    int w = std::max(1, maxW + pad + 2), h = std::max(1, maxH - minY + pad);
    block.bitmap = Bitmap(w, h);
    Canvas canvas(block.bitmap);
    for (auto &it : items) {
        int top = it.y - minY;
        if (it.icon && !it.icon->empty()) {
            canvas.draw(*it.icon, {0, 0, it.icon->w, it.icon->h}, {float(it.x), float(top), float(it.size), float(it.size)});
            continue;
        }
        if (!it.g || it.g->coverage.empty()) continue;
        int baseline = top + font().ascent(it.size);
        int gx = it.x + it.g->left, gy = baseline - it.g->top;
        for (int pass = style.shadow ? 0 : 1; pass < 2; pass++) {
            int ox = pass == 0 ? 1 : 0, oy = pass == 0 ? 1 : 0;
            auto c = pass == 0 ? std::array<uint8_t, 3>{0, 0, 0} : it.color;
            for (int yy = 0; yy < it.g->h; yy++) {
                int ty = gy + yy + oy;
                if (ty < 0 || ty >= h) continue;
                uint8_t *row = block.bitmap.row(ty);
                for (int xx = 0; xx < it.g->w; xx++) {
                    int tx = gx + xx + ox;
                    if (tx < 0 || tx >= w) continue;
                    int a = it.g->coverage[size_t(yy) * it.g->w + xx];
                    if (pass == 0) a = a * 3 / 4;
                    if (!a) continue;
                    uint8_t *d = row + tx * 4;
                    int da = d[3], outA = a + da * (255 - a) / 255;
                    if (!outA) continue;
                    for (int ch = 0; ch < 3; ch++)
                        d[ch] = uint8_t((c[ch] * a + d[ch] * da * (255 - a) / 255) / outA);
                    d[3] = uint8_t(outA);
                }
            }
        }
    }
    return block;
}

}  // namespace wolf
