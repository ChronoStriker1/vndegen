// SengokuRanceFont: Test Game 46's text library, which draws its messages and menus onto sprites with the game's own
// font file (.fnl) and keeps a text cursor per sprite. Behaviour read from how the game calls it: the argument
// names, the TextStyle struct it passes, and the game functions that wrap the calls.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "game.h"
#include "sjis.h"

namespace s4 {
namespace {

Game &G() { return game(); }

struct SpriteText {
    int type = 0;
    float size = 16, bold = 0, edge = 0, scaleX = 1, spaceScaleX = 1, spacing = 0;
    int r = 255, g = 255, b = 255, er = 0, eg = 0, eb = 0;
    float homeX = 0, x = 0;
    int homeY = 0, y = 0, lineSpace = 0;
    int lastChar = 0;
    int reduceDescender = 0;
};

std::map<int, SpriteText> g_state;
SpriteText g_global;  // the style SetFontType and SetFontSize set, for GetTextWidth
S4_HLL_RESET {
    g_state.clear();
    g_global = {};
}

TextStyle styleOf(const SpriteText &t) {
    TextStyle st;
    st.gameFont = true;
    // The game uses font types 0 and 256: the high byte picks the library's face when it is set.
    st.face = t.type >= 256 ? t.type >> 8 : t.type;
    st.size = std::max(1, int(std::lround(t.size)));
    st.r = t.r; st.g = t.g; st.b = t.b;
    st.boldWeight = t.bold;
    st.edgeWeight = t.edge;
    st.er = t.er; st.eg = t.eg; st.eb = t.eb;
    return st;
}

void draw(int sp, const std::string &text) {
    Sprite *s = G().sprites.find(sp);
    if (!s) return;
    SpriteText &t = g_state[sp];
    if (s->text.w != s->width() || s->text.h != s->height()) s->text.create(s->width(), s->height(), true);
    int w = drawText(s->text, int(std::lround(t.x)), t.y, text, styleOf(t), int(std::lround(t.spacing)));
    t.x += float(w);
    if (!text.empty()) {
        int len;
        size_t last = 0;
        for (size_t i = 0; i < text.size(); i += len) {
            sjis::decodeAt(text, i, &len);
            last = i;
        }
        t.lastChar = len == 2 && last + 1 < text.size() ? (uint8_t(text[last]) | uint8_t(text[last + 1]) << 8) : uint8_t(text[last]);
    }
}

S4_HLL(SengokuRanceFont, SetFontType) { g_global.type = c.i(0); }
S4_HLL(SengokuRanceFont, SetFontSize) { g_global.size = c.f(0); }
S4_HLL(SengokuRanceFont, GetTextWidth) { c.retFloat(float(textWidth(c.s(0), styleOf(g_global), 0))); }
S4_HLL(SengokuRanceFont, GetActualFontSize) { c.retFloat(c.f(1)); }
S4_HLL(SengokuRanceFont, GetActualFontSizeRoundDown) { c.retFloat(std::floor(c.f(1))); }

S4_HLL(SengokuRanceFont, SP_ClearState) { g_state.erase(c.i(0)); }
S4_HLL(SengokuRanceFont, SP_GetTextCharSpace) { c.retFloat(g_state[c.i(0)].spacing); }
S4_HLL(SengokuRanceFont, SP_GetTextHomeX) { c.retFloat(g_state[c.i(0)].homeX); }
S4_HLL(SengokuRanceFont, SP_GetTextHomeY) { c.retInt(g_state[c.i(0)].homeY); }
S4_HLL(SengokuRanceFont, SP_GetTextLineSpace) { c.retInt(g_state[c.i(0)].lineSpace); }
S4_HLL(SengokuRanceFont, SP_GetTextPosX) { c.retFloat(g_state[c.i(0)].x); }
S4_HLL(SengokuRanceFont, SP_GetTextPosY) { c.retInt(g_state[c.i(0)].y); }
S4_HLL(SengokuRanceFont, SP_SetTextCharSpace) { g_state[c.i(0)].spacing = c.f(1); }
S4_HLL(SengokuRanceFont, SP_SetTextHome) { auto &t = g_state[c.i(0)]; t.homeX = c.f(1); t.homeY = c.i(2); }
S4_HLL(SengokuRanceFont, SP_SetTextLineSpace) { g_state[c.i(0)].lineSpace = c.i(1); }
S4_HLL(SengokuRanceFont, SP_SetTextPos) { auto &t = g_state[c.i(0)]; t.x = c.f(1); t.y = c.i(2); }
S4_HLL(SengokuRanceFont, SP_TextClear) {
    if (Sprite *s = G().sprites.find(c.i(0))) s->text = {};
}
S4_HLL(SengokuRanceFont, SP_TextCopy) {
    Sprite *d = G().sprites.find(c.i(0)), *s = G().sprites.find(c.i(1));
    if (d && s) d->text = s->text;
    g_state[c.i(0)] = g_state[c.i(1)];
}
S4_HLL(SengokuRanceFont, SP_TextHome) { auto &t = g_state[c.i(0)]; t.x = t.homeX; t.y = t.homeY; }
S4_HLL(SengokuRanceFont, SP_TextNewLine) {
    auto &t = g_state[c.i(0)];
    t.x = t.homeX;
    t.y += int(std::lround(c.f(1))) + t.lineSpace;
}
// The classic metrics are sact_text_metrics_t: colour, size, weight and face.
S4_HLL(SengokuRanceFont, SP_SetTextMetricsClassic) {
    Page *tm = c.page(1);
    if (!tm || tm->v.size() < 6) return;
    auto &t = g_state[c.i(0)];
    t.r = tm->v[0]; t.g = tm->v[1]; t.b = tm->v[2];
    t.size = float(tm->v[3]);
    t.bold = tm->v[4] >= 600 ? 1.0f : 0.0f;
    t.type = tm->v[5];
}
// TextStyle: FontType, FontSize, BoldWidth, FontR, FontG, FontB, EdgeWidth, EdgeR, EdgeG, EdgeB, ScaleX, SpaceScaleX,
// FontSpacing.
S4_HLL(SengokuRanceFont, SP_SetTextStyle) {
    Page *p = c.page(1);
    if (!p || p->v.size() < 13) return;
    auto f = [&](int k) { float v; std::memcpy(&v, &p->v[k], 4); return v; };
    auto &t = g_state[c.i(0)];
    t.type = p->v[0];
    t.size = f(1);
    t.bold = f(2);
    t.r = p->v[3]; t.g = p->v[4]; t.b = p->v[5];
    t.edge = f(6);
    t.er = p->v[7]; t.eg = p->v[8]; t.eb = p->v[9];
    t.scaleX = f(10);
    t.spaceScaleX = f(11);
    t.spacing = f(12);
}
S4_HLL(SengokuRanceFont, SP_TextDrawClassic) { draw(c.i(0), c.s(1)); }
S4_HLL(SengokuRanceFont, SP_TextDraw) {
    if (std::getenv("S4_TEXTLOG")) {
        std::string hex;
        for (unsigned char ch : c.s(1)) { char b[4]; std::snprintf(b, 4, "%02x ", ch); hex += b; }
        auto &t = g_state[c.i(0)];
        Sprite *sp = G().sprites.find(c.i(0));
        G().log("SP_TextDraw sp " + std::to_string(c.i(0)) + " at " + std::to_string(t.x) + "," + std::to_string(t.y) +
                " size " + std::to_string(t.size) + " type " + std::to_string(t.type) + " rgb " + std::to_string(t.r) + "," +
                std::to_string(t.g) + "," + std::to_string(t.b) + " sprite " + (sp ? std::to_string(sp->width()) + "x" + std::to_string(sp->height()) : "none") + " : " + hex);
    }
    draw(c.i(0), c.s(1));
}
S4_HLL(SengokuRanceFont, SP_TextDrawPreload) {}
S4_HLL(SengokuRanceFont, SP_GetTextLastCharCode) { c.retInt(g_state[c.i(0)].lastChar); }
S4_HLL(SengokuRanceFont, SP_SetTextLastCharCode) { g_state[c.i(0)].lastChar = c.i(1); }
S4_HLL(SengokuRanceFont, SP_SetReduceDescender) { g_state[c.i(0)].reduceDescender = c.i(1); }
S4_HLL(SengokuRanceFont, SP_GetReduceDescender) { c.retInt(g_state[c.i(0)].reduceDescender); }

// SystemServiceEx: presentation details the PC engine reports. VNDegen presents every frame.
S4_HLL(SystemServiceEx, GetUpdateResult) { c.retInt(1); }
S4_HLL(SystemServiceEx, GetOverlayPresent) { c.retInt(0); }
S4_HLL(SystemServiceEx, GetOverlayActive) { c.retInt(0); }
S4_HLL(SystemServiceEx, UpdateMainSurface) {}
S4_HLL(SystemServiceEx, SetWaitVSync) {}
S4_HLL(SystemServiceEx, GetHasPresented) { c.retInt(1); }
S4_HLL(SystemServiceEx, ClearHasPresented) {}

// AliceLogo3: the AliceSoft logo animation at startup. VNDegen skips it.
S4_HLL(AliceLogo3, Init) { c.retInt(1); }
S4_HLL(AliceLogo3, SetWaveNum) {}
S4_HLL(AliceLogo3, Run) {}

}  // namespace
}  // namespace s4
