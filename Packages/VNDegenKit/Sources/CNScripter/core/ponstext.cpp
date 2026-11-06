// Ponscripter's proportional text: the game's own fonts, laid out and drawn as the Umineko games' engine draws them.
// That engine works on a screen twice the game's size: glyphs are rasterized at twice the font size, placed at whole
// pixels of that screen, and the result is halved. Line height, paragraph breaks, word wrap, shadows and the faces the
// ~tags~ pick were all measured on it (docs/nscripter-cleanroom.md).
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>

#include "engine.h"

namespace vnns {

PonsFace *Engine::ponsFace(int n) {
    auto it = ponsFaces_.find(n);
    if (it != ponsFaces_.end()) return it->second.get();
    // A face h_mapfont didn't name is fonts/face<n>.ttf, as on the PC engine.
    auto mapped = fontFiles_.find(n);
    std::string file = mapped != fontFiles_.end() ? mapped->second : "fonts/face" + std::to_string(n) + ".ttf";
    std::unique_ptr<PonsFace> face;
    Bytes data;
    if (fs_.read(file, data)) {
        face = std::make_unique<PonsFace>();
        if (!face->load(std::move(data))) {
            logf("font %s can't be read", toUtf8(file).c_str());
            face.reset();
        }
    }
    return ponsFaces_.emplace(n, std::move(face)).first->second.get();
}

void Engine::applyPonsTag(PonsStyle &style, std::string_view tag) {
    for (size_t i = 0; i < tag.size(); i++) {
        char c = tag[i];
        if (c == 'i') {
            style.italic = !style.italic;
        } else if (c == 'b') {
            style.bold = !style.bold;
        } else if (c == 'c' && i + 1 < tag.size() && isdigit(uint8_t(tag[i + 1]))) {
            int n = 0;
            while (i + 1 < tag.size() && isdigit(uint8_t(tag[i + 1]))) n = n * 10 + (tag[++i] - '0');
            style = PonsStyle();
            style.base = n;
        }
    }
}

std::string Engine::ponsChar(std::string_view t, size_t p, size_t *used) {
    char c = t[p];
    bool twice = p + 1 < t.size() && t[p + 1] == c;
    if (c == '`') {
        *used = twice ? 2 : 1;
        return twice ? "“" : "‘";
    }
    if (c == '\'') {
        *used = twice ? 2 : 1;
        return twice ? "”" : "’";
    }
    *used = charLen(t, p);
    return std::string(t.substr(p, *used));
}

void Engine::resetPen() {
    penX_ = penY_ = 0;
    penX2_ = penY2_ = 0;
}

// The window's font size and width, bounded: sizes past any screen are a broken script, and doubling them mustn't
// overflow.
int Engine::ponsSize() const { return std::clamp(win_.fh, 1, 512); }
int Engine::ponsLineWidth2() const {
    return 2 * int(std::clamp<int64_t>(int64_t(win_.cols) * win_.fw, 1, 8192));
}

int Engine::ponsIndent2() const {
    return 2 * int(std::clamp<int64_t>(int64_t(indent_) * (int64_t(win_.fw) + win_.px), 0, 8192));
}

int Engine::ponsLineHeight2() {
    PonsFace *f = ponsFace(ponsStyle_.face());
    return (f ? f->height(2 * ponsSize()) : 2 * ponsSize()) + 2 * std::clamp(win_.py, -512, 512);
}

void Engine::ponsNewLine() {
    penX2_ = ponsIndent2();
    penY2_ += ponsLineHeight2();
    penX_ = penX2_ / 2;
    penY_ = penY2_ / 2;
    pageText_ += '\n';
}

void Engine::ponsBreakParagraph() {
    PonsFace *f = ponsFace(ponsStyle_.face());
    int height = f ? f->height(2 * ponsSize()) : 2 * ponsSize();
    penX2_ = ponsIndent2();
    penY2_ += (height + 1) / 2 + 2 * std::clamp(win_.py, -512, 512);
    penX_ = penX2_ / 2;
    penY_ = penY2_ / 2;
    pageText_ += '\n';
}

void Engine::ponsPutChar(const std::string &ch) {
    showWindow();
    int faceNo = ponsStyle_.face();
    PonsFace *face = ponsFace(faceNo);
    size_t n;
    uint32_t cp = decodeChar(ch, 0, &n);
    int px = 2 * ponsSize();
    int advance = face->glyph(cp, px)->advance;
    int lineWidth = ponsLineWidth2();
    // A character that won't fit starts a line, unless it's one that may not start a line (kinsoku), which hangs past
    // the edge. Words break at spaces before this, so this is a word wider than the line, or Japanese text.
    if (penX2_ + advance > lineWidth && penX2_ > ponsIndent2() && kinsokuStart_.find(ch) == std::string::npos)
        ponsNewLine();
    TextChar c;
    c.ch = ch;
    c.face = faceNo;
    // The origin is bounded as sizes are, so a script's or a save's huge one can't overflow the doubled positions.
    int tx = std::clamp(win_.tx, -8192, 8192), ty = std::clamp(win_.ty, -8192, 8192);
    c.x2 = 2 * tx + penX2_;
    c.y2 = 2 * ty + penY2_ + 2 * face->ascent(ponsSize());
    c.px2 = px;
    c.x = c.x2 / 2;
    c.y = ty + penY2_ / 2;
    c.w = (advance + 1) / 2;
    c.h = (ponsLineHeight2() + 1) / 2;
    c.color = textColor_;
    c.shadow = win_.shadow;
    for (const GaijiRule &g : gaiji_)
        if (g.ch == ch) c.gaiji = g.image;
    page_.push_back(c);
    pageText_ += ch;
    lastCharX_ = c.x;
    lastCharY_ = c.y;
    penX2_ += advance;
    penX_ = penX2_ / 2;
    sceneDirty_ = true;
}

int Engine::ponsWordWidth(std::string_view t, size_t p, bool literal) {
    PonsStyle style = ponsStyle_;
    int px = 2 * ponsSize(), width = 0;
    while (p < t.size()) {
        char c = t[p];
        if (c == ' ' || c == '\n') break;
        if (!literal && (c == '@' || c == '\\' || (c == '/' && p + 1 < t.size() && t[p + 1] == '\n'))) break;
        if (c == '^' || (c == '_' && !literal)) {
            p++;
            continue;
        }
        if (c == '~') {
            size_t close = t.find('~', p + 1);
            if (close != std::string_view::npos && close < t.find('\n', p)) {
                applyPonsTag(style, t.substr(p + 1, close - p - 1));
                p = close + 1;
                continue;
            }
        }
        // A text line's colours, and !sd, !sN, !dN and !wN, read as the text step reads them. A string putString puts
        // has only carets and tags for markup.
        if (!literal && c == '#' && p + 7 <= t.size() &&
            std::all_of(t.begin() + long(p) + 1, t.begin() + long(p) + 7, [](char h) { return isxdigit(uint8_t(h)); })) {
            p += 7;
            continue;
        }
        if (!literal && c == '!' && p + 2 < t.size() && tolower(uint8_t(t[p + 1])) == 's' &&
            tolower(uint8_t(t[p + 2])) == 'd') {
            p += 3;
            continue;
        }
        if (!literal && c == '!' && p + 2 < t.size() && strchr("sdwSDW", t[p + 1]) && isdigit(uint8_t(t[p + 2]))) {
            p += 2;
            while (p < t.size() && isdigit(uint8_t(t[p]))) p++;
            continue;
        }
        size_t used;
        std::string ch = ponsChar(t, p, &used);
        p += used;
        PonsFace *face = ponsFace(style.face());
        if (!face) break;
        size_t n;
        width += face->glyph(decodeChar(ch, 0, &n), px)->advance;
    }
    return width;
}

bool Engine::ponsWrapsBefore(std::string_view t, size_t p, bool literal) {
    size_t nl = t.find('\n', p);
    int width = ponsWordWidth(t.substr(0, nl == std::string_view::npos ? t.size() : nl), p, literal);
    int space = ponsFace(ponsStyle_.face())->glyph(' ', 2 * ponsSize())->advance;
    return width > 0 && penX2_ + space + width > ponsLineWidth2() && penX2_ > 0;
}

void Engine::putString(const std::string &s) {
    if (!ponscripter_) {
        for (size_t i = 0; i < s.size(); i += charLen(s, i)) putChar(s.substr(i, charLen(s, i)));
        return;
    }
    PonsStyle saved = ponsStyle_;
    ponsStyle_ = PonsStyle();
    for (size_t i = 0; i < s.size();) {
        if (s[i] == '^') {
            i++;
            continue;
        }
        if (s[i] == '~') {
            size_t close = s.find('~', i + 1);
            if (close != std::string::npos) {
                applyPonsTag(ponsStyle_, std::string_view(s).substr(i + 1, close - i - 1));
                i = close + 1;
                continue;
            }
        }
        size_t used;
        std::string ch = ponsChar(s, i, &used);
        i += used;
        // Words break between lines as a text line's do.
        if (ch == " " && ponsText() && ponsWrapsBefore(s, i, true)) {
            newLine();
            continue;
        }
        putChar(ch);
    }
    ponsStyle_ = saved;
}

// The page's Ponscripter characters are drawn onto the doubled screen as they appear, shadow first, and the changed
// part is halved into a layer the frame shows. A page that changed other than by gaining characters is drawn again.
void Engine::drawPonsText(Image &out) {
    PonsLayer &l = ponsLayer_;
    std::vector<PonsLayer::Placed> now;
    for (const TextChar &c : page_)
        if (c.face >= 0 && c.gaiji.empty())
            now.push_back({c.ch, c.face, c.px2, c.x2, c.y2, c.color, c.shadow, shadeX_, shadeY_});
    bool grows = now.size() >= l.placed.size() && std::equal(l.placed.begin(), l.placed.end(), now.begin());
    if (l.frame.w != out.w || l.frame.h != out.h || !grows) {
        l.doubled = Image(out.w * 2, out.h * 2);
        l.frame = Image(out.w, out.h);
        l.placed.clear();
    }
    int x0 = l.doubled.w, y0 = l.doubled.h, x1 = 0, y1 = 0;
    auto draw = [&](const PonsFace::Glyph &g, int gx, int gy, uint32_t color) {
        for (int y = 0; y < g.h; y++) {
            int yy = gy + y;
            if (yy < 0 || yy >= l.doubled.h) continue;
            uint32_t *row = l.doubled.row(yy);
            const uint8_t *cov = &g.coverage[size_t(y) * size_t(g.w)];
            for (int x = 0; x < g.w; x++) {
                int xx = gx + x;
                if (xx < 0 || xx >= l.doubled.w || !cov[x]) continue;
                row[xx] = over(row[xx], scalePx(color | 0xFF000000u, cov[x]));
            }
        }
        x0 = std::min(x0, gx);
        y0 = std::min(y0, gy);
        x1 = std::max(x1, gx + g.w);
        y1 = std::max(y1, gy + g.h);
    };
    for (size_t i = l.placed.size(); i < now.size(); i++) {
        const PonsLayer::Placed &c = now[i];
        PonsFace *face = ponsFace(c.face);
        if (!face) continue;
        size_t n;
        std::shared_ptr<const PonsFace::Glyph> held = face->glyph(decodeChar(c.ch, 0, &n), c.px);
        const PonsFace::Glyph &g = *held;
        if (g.w == 0) continue;
        if (c.shadow) draw(g, c.x2 + g.left + shadeX_, c.y2 - g.top + shadeY_, rgba(0, 0, 0));
        draw(g, c.x2 + g.left, c.y2 - g.top, c.color);
    }
    l.placed = std::move(now);
    // Halve what changed: each frame pixel the average of its 2x2 block.
    for (int y = std::max(0, y0 / 2); y < std::min(l.frame.h, (y1 + 1) / 2); y++)
        for (int x = std::max(0, x0 / 2); x < std::min(l.frame.w, (x1 + 1) / 2); x++) {
            uint32_t sum[4] = {0, 0, 0, 0};
            for (int dy = 0; dy < 2; dy++)
                for (int dx = 0; dx < 2; dx++) {
                    uint32_t p = l.doubled.row(2 * y + dy)[2 * x + dx];
                    for (int k = 0; k < 4; k++) sum[k] += p >> (8 * k) & 255;
                }
            l.frame.row(y)[x] = sum[0] / 4 | (sum[1] / 4) << 8 | (sum[2] / 4) << 16 | (sum[3] / 4) << 24;
        }
    blit(out, l.frame, 0, 0, 0, 0, out.w, out.h);
}

ImageRef Engine::renderPonsString(const std::string &text, int px, int pitch, const std::vector<uint32_t> &colors,
                                  bool shadow, int &cells, int wrap) {
    px = std::clamp(px, 1, 1024);
    pitch = std::clamp(pitch, -px, 1024);
    // Lay the string out once, then draw it in each colour. Each glyph hangs from its own face's ascent.
    struct Placed {
        std::shared_ptr<const PonsFace::Glyph> g;
        int x, y, ascent;
    };
    std::vector<Placed> glyphs;
    PonsStyle style;
    int pen = 0, height = 0, widest = 0, lineTop = 0;
    auto ascentOf = [px](PonsFace *f) { return px % 2 ? f->ascent(px) : 2 * f->ascent(px / 2); };
    for (size_t i = 0; i < text.size();) {
        if (text[i] == '^') {
            i++;
            continue;
        }
        if (text[i] == '~') {
            size_t close = text.find('~', i + 1);
            if (close != std::string::npos) {
                applyPonsTag(style, std::string_view(text).substr(i + 1, close - i - 1));
                i = close + 1;
                continue;
            }
        }
        size_t used, n;
        std::string ch = ponsChar(text, i, &used);
        i += used;
        PonsFace *face = ponsFace(style.face());
        if (!face) continue;
        std::shared_ptr<const PonsFace::Glyph> g = face->glyph(decodeChar(ch, 0, &n), px);
        if (wrap > 0 && pen > 0 && pen + g->advance > wrap) {
            pen = 0;
            lineTop += face->height(px) + 2 * std::clamp(win_.py, -512, 512);
        }
        glyphs.push_back({g, pen, lineTop, ascentOf(face)});
        pen += g->advance + pitch;
        widest = std::max(widest, pen);
        height = std::max(height, lineTop + face->height(px));
    }
    // Even sizes, so each cell halves to whole pixels. At most 8192 x 4096 a cell, and 16M doubled pixels (64 MB) in
    // all, whatever a script asks for: the cells past that budget are dropped, as NScripter's string sprites do.
    int w2 = (std::clamp(widest, 2, 8192) + 1) & ~1;
    int h2 = (std::clamp(height + (shadow ? shadeY_ : 0), 2, std::min(4096, (1 << 24) / w2) & ~1) + 1) & ~1;
    cells = int(std::clamp<int64_t>(int64_t(colors.size()), 1, std::max<int64_t>(1, (int64_t(1) << 24) / (int64_t(w2) * h2))));
    Image doubled(w2 * cells, h2);
    for (int c = 0; c < cells; c++)
        for (const Placed &p : glyphs) {
            for (int pass = shadow ? 0 : 1; pass < 2; pass++) {
                uint32_t color = pass == 0 ? rgba(0, 0, 0) : colors[size_t(c)];
                int gx = c * w2 + p.x + p.g->left + (pass == 0 ? shadeX_ : 0);
                int gy = p.y + p.ascent - p.g->top + (pass == 0 ? shadeY_ : 0);
                for (int y = 0; y < p.g->h; y++) {
                    if (gy + y < 0 || gy + y >= h2) continue;
                    uint32_t *row = doubled.row(gy + y);
                    for (int x = 0; x < p.g->w; x++) {
                        int xx = gx + x;
                        uint8_t a = p.g->coverage[size_t(y) * size_t(p.g->w) + size_t(x)];
                        if (a && xx >= c * w2 && xx < (c + 1) * w2) row[xx] = over(row[xx], scalePx(color | 0xFF000000u, a));
                    }
                }
            }
        }
    return std::make_shared<const Image>(halveImage(doubled));
}

}  // namespace vnns
