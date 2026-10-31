#include "gfx.h"

#include <algorithm>
#include <cstring>

#include "font.h"
#include "sjis.h"

namespace s4 {

void Surface::create(int width, int height, bool alpha) {
    w = std::max(0, width);
    h = std::max(0, height);
    hasAlpha = alpha;
    px.assign(size_t(w) * h * 4, 0);
}

void Surface::fill(int x, int y, int fw, int fh, int r, int g, int b, int a) {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(w, x + fw), y1 = std::min(h, y + fh);
    for (int yy = y0; yy < y1; yy++)
        for (int xx = x0; xx < x1; xx++) {
            uint8_t *p = at(xx, yy);
            if (r >= 0) { p[0] = uint8_t(r); p[1] = uint8_t(g); p[2] = uint8_t(b); }
            if (a >= 0) p[3] = uint8_t(a);
        }
}

Surface &Sprite::writable() {
    if (!hasOwn) {
        if (cg) {
            own.create(cg->w, cg->h, cg->hasAlpha);
            own.px = cg->rgba;
            if (!cg->hasAlpha)
                for (size_t i = 3; i < own.px.size(); i += 4) own.px[i] = 255;
        }
        hasOwn = true;
        cg.reset();
    }
    return own;
}

int Sprites::unused(int min) const {
    int n = std::max(1, min);
    while (true) {
        auto it = list_.find(n);
        if (it == list_.end() || !it->second.used) return n;
        n++;
    }
}

void blit(uint8_t *dst, int dw, int dh, int dx, int dy, const uint8_t *src, int sw, int sh, bool srcAlpha, int rate,
          int method, int brightness) {
    if (!src || rate <= 0) return;
    int x0 = std::max(0, dx), y0 = std::max(0, dy), x1 = std::min(dw, dx + sw), y1 = std::min(dh, dy + sh);
    for (int y = y0; y < y1; y++) {
        const uint8_t *s = src + (size_t(y - dy) * sw + (x0 - dx)) * 4;
        uint8_t *d = dst + (size_t(y) * dw + x0) * 4;
        for (int x = x0; x < x1; x++, s += 4, d += 4) {
            int a = srcAlpha ? s[3] * rate / 255 : rate;
            if (a <= 0) continue;
            int sr = s[0], sg = s[1], sb = s[2];
            if (brightness != 255) {
                sr = sr * brightness / 255;
                sg = sg * brightness / 255;
                sb = sb * brightness / 255;
            }
            if (method == 1) {  // screen
                sr = d[0] + sr - d[0] * sr / 255;
                sg = d[1] + sg - d[1] * sg / 255;
                sb = d[2] + sb - d[2] * sb / 255;
                a = rate;
            } else if (method == 2) {  // multiply
                sr = d[0] * sr / 255;
                sg = d[1] * sg / 255;
                sb = d[2] * sb / 255;
                a = rate;
            }
            if (a >= 255) {
                d[0] = uint8_t(sr); d[1] = uint8_t(sg); d[2] = uint8_t(sb);
            } else {
                d[0] = uint8_t(d[0] + (sr - d[0]) * a / 255);
                d[1] = uint8_t(d[1] + (sg - d[1]) * a / 255);
                d[2] = uint8_t(d[2] + (sb - d[2]) * a / 255);
            }
        }
    }
}

void Sprites::compose(std::vector<uint8_t> &frame, int w, int h, int offX, int offY) {
    std::fill(frame.begin(), frame.end(), 0);
    for (size_t i = 3; i < frame.size(); i += 4) frame[i] = 255;
    std::vector<std::pair<std::pair<int, int>, Sprite *>> order;
    for (auto &[n, s] : list_)
        if (s.used && s.show) order.push_back({{s.z, n}, &s});
    std::sort(order.begin(), order.end(), [](auto &a, auto &b) { return a.first < b.first; });
    for (auto &[k, s] : order) {
        blit(frame.data(), w, h, s->x + offX, s->y + offY, s->pixels(), s->width(), s->height(), s->hasAlpha(),
             s->blendRate, s->drawMethod, s->brightness);
        if (s->text.w > 0)
            blit(frame.data(), w, h, s->x + offX, s->y + offY, s->text.px.data(), s->text.w, s->text.h, true, s->blendRate, 0);
    }
}

namespace {

// Lays coverage over dst in a colour, keeping straight alpha: the text layer ends up with the text's own alpha.
void putGlyph(Surface &dst, int x, int y, const GlyphImage &g, int r, int gg, int b) {
    for (int yy = 0; yy < g.h; yy++) {
        int py = y + yy;
        if (py < 0 || py >= dst.h) continue;
        for (int xx = 0; xx < g.w; xx++) {
            int px = x + xx;
            if (px < 0 || px >= dst.w) continue;
            int a = g.coverage[size_t(yy) * g.w + xx];
            if (!a) continue;
            uint8_t *d = dst.at(px, py);
            int da = d[3];
            int oa = a + da * (255 - a) / 255;
            if (oa <= 0) continue;
            d[0] = uint8_t((r * a + d[0] * da * (255 - a) / 255) / oa);
            d[1] = uint8_t((gg * a + d[1] * da * (255 - a) / 255) / oa);
            d[2] = uint8_t((b * a + d[2] * da * (255 - a) / 255) / oa);
            d[3] = uint8_t(oa);
        }
    }
}

}  // namespace

// A character's glyph: the game's font library first when the style asks for it, then VNDegen's fonts.
static bool glyphFor(const TextStyle &st, const std::string &s, size_t at, int len, char32_t cp, bool bold, GlyphImage &g) {
    if (st.gameFont) {
        uint16_t code = len == 2 ? uint16_t(uint8_t(s[at]) << 8 | uint8_t(s[at + 1])) : uint8_t(s[at]);
        if (gameGlyph(st.face, code, st.size, g)) return true;
    }
    return glyph(st.gameFont ? 0 : st.face, cp, st.size, bold, g);
}

int drawText(Surface &dst, int x, int y, const std::string &s, const TextStyle &st, int charSpace) {
    bool bold = st.weight >= 600 || st.boldWeight > 0;
    int ascent = st.gameFont ? gameFontAscent(st.face, st.size) : fontAscent(st.face, st.size);
    int pen = x;
    for (size_t i = 0; i < s.size();) {
        int len;
        size_t at = i;
        char32_t cp = sjis::decodeAt(s, i, &len);
        i += len;
        GlyphImage g;
        int adv = len == 2 ? st.size : st.size / 2;
        if (glyphFor(st, s, at, len, cp, bold, g)) {
            adv = g.advance > 0 ? g.advance : adv;
            int gx = pen + g.left, gy = y + ascent - g.top;
            int edge = int(st.edgeWeight + 0.5f);
            for (int oy = -std::max(st.shadowU, edge); oy <= std::max(st.shadowD, edge); oy++)
                for (int ox = -std::max(st.shadowL, edge); ox <= std::max(st.shadowR, edge); ox++) {
                    if (!ox && !oy) continue;
                    if (edge > 0) putGlyph(dst, gx + ox, gy + oy, g, st.er, st.eg, st.eb);
                    else putGlyph(dst, gx + ox, gy + oy, g, st.sr, st.sg, st.sb);
                }
            putGlyph(dst, gx, gy, g, st.r, st.g, st.b);
        }
        pen += adv + charSpace;
    }
    return pen - x;
}

int textWidth(const std::string &s, const TextStyle &st, int charSpace) {
    bool bold = st.weight >= 600 || st.boldWeight > 0;
    int w = 0;
    for (size_t i = 0; i < s.size();) {
        int len;
        size_t at = i;
        char32_t cp = sjis::decodeAt(s, i, &len);
        i += len;
        GlyphImage g;
        int adv = len == 2 ? st.size : st.size / 2;
        if (glyphFor(st, s, at, len, cp, bold, g) && g.advance > 0) adv = g.advance;
        w += adv + charSpace;
    }
    return w;
}

}  // namespace s4
