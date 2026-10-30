// StoatSpriteEngine's adventure layer, which the SDK predates: MultiSprite (backgrounds, faces, pictures and text
// sprites the scenario sets by kind and number), CharSprite (message text drawn as one sprite per character), the
// backlog (ADVLogList) and saved scenes (ADVSceneKeeper). Their behaviour was read from how the games call them;
// docs/system4-cleanroom.md lists what each call was taken to mean. Also the DrawGraph calls on sprite pictures.
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

#include "font.h"
#include "game.h"
#include "sjis.h"

namespace s4 {
namespace {

Game &G() { return game(); }

// Engine sprite numbers for MultiSprite and CharSprite start here, above any number a game uses.
int g_nextInternal = 1000000;

struct CharProps {
    int type = 0, size = 24;
    int r = 255, g = 255, b = 255;
    float bold = 0, edge = 0;
    int er = 0, eg = 0, eb = 0;
};

TextStyle styleOf(const CharProps &p) {
    TextStyle st;
    st.face = p.type;
    st.size = p.size;
    st.r = p.r; st.g = p.g; st.b = p.b;
    st.boldWeight = p.bold;
    st.edgeWeight = p.edge;
    st.er = p.er; st.eg = p.eg; st.eb = p.eb;
    return st;
}

// ---- MultiSprite ----
// Kinds seen in the scenario code: 0 face, 1 (unused by the test games), 2 picture, 3 background, 4 wallpaper,
// 5 text.

struct Move { int x0, y0, a0, x1, y1, a1, time, wait; float accel; bool alpha; };

struct Multi {
    int cg = 0;
    int x = 0, y = 0, defX = 0, defY = 0, z = 0, alpha = 255;
    int transNum = 0, transTime = 0;
    bool linked = false;
    int parentFrame = -1;
    int origin = 0;
    std::string text;
    int charSpace = 0;
    CharProps props;
    int sprite = 0;
    bool moving = false;
    Move move{};
    int shownCg = -1;
    std::string shownText;
};

std::map<std::pair<int, int>, Multi> g_multi;
bool g_messageFrameShown = true;

Multi &multi(int type, int num) { return g_multi[{type, num}]; }

// The point a position names, by origin mode: the numeric keypad's layout (7 top left, 5 centre, 3 bottom right),
// and 0 also top left.
void anchor(int mode, int w, int h, int &dx, int &dy) {
    dx = dy = 0;
    if (mode < 1 || mode > 9) return;
    int col = (mode - 1) % 3, row = 2 - (mode - 1) / 3;
    dx = -w * col / 2;
    dy = -h * row / 2;
}

void applyMulti(Multi &m) {
    bool has = m.cg > 0 || !m.text.empty();
    if (!has) {
        if (m.sprite) G().sprites.remove(m.sprite);
        m.sprite = 0;
        m.shownCg = -1;
        m.shownText.clear();
        return;
    }
    if (!m.sprite) m.sprite = g_nextInternal++;
    Sprite &s = G().sprites.get(m.sprite);
    if (m.cg > 0 && m.shownCg != m.cg) {
        auto img = G().cgCached(m.cg);
        s.used = img != nullptr;
        s.cg = img;
        s.hasOwn = false;
        s.own = {};
        m.shownCg = m.cg;
    } else if (m.cg <= 0 && m.shownText != m.text) {
        TextStyle st = styleOf(m.props);
        int edge = int(st.edgeWeight + 0.5f);
        s.used = true;
        s.cg.reset();
        s.hasOwn = true;
        s.own.create(std::max(1, textWidth(m.text, st, m.charSpace) + edge * 2), st.size + edge * 2, true);
        drawText(s.own, edge, edge, m.text, st, m.charSpace);
        m.shownText = m.text;
    }
    int dx, dy;
    anchor(m.origin, s.width(), s.height(), dx, dy);
    s.x = m.x + dx;
    s.y = m.y + dy;
    s.z = m.z;
    s.blendRate = std::clamp(m.alpha, 0, 255);
    s.show = !m.linked || g_messageFrameShown;
}

#define MS_SET(fn, body)                              \
    S4_HLL(StoatSpriteEngine, MultiSprite_##fn) {     \
        Multi &m = multi(c.i(0), c.i(1));             \
        body;                                         \
        c.retBool(true);                              \
    }
MS_SET(SetCG, m.cg = c.i(2))
MS_SET(SetPos, m.x = c.i(2); m.y = c.i(3))
MS_SET(SetDefaultPos, m.defX = c.i(2); m.defY = c.i(3))
MS_SET(SetZ, m.z = c.i(2))
MS_SET(SetAlpha, m.alpha = c.i(2))
MS_SET(SetTransitionInfo, m.transNum = c.i(2); m.transTime = c.i(3))
MS_SET(SetLinkedMessageFrame, m.linked = c.b(2))
MS_SET(SetParentMessageFrameNum, m.parentFrame = c.i(2))
MS_SET(SetOriginPosMode, m.origin = c.i(2))
MS_SET(SetText, m.text = c.s(2))
MS_SET(SetCharSpace, m.charSpace = c.i(2))
MS_SET(CharSpriteProperty_SetType, m.props.type = c.i(2))
MS_SET(CharSpriteProperty_SetSize, m.props.size = c.i(2))
MS_SET(CharSpriteProperty_SetColor, m.props.r = c.i(2); m.props.g = c.i(3); m.props.b = c.i(4))
MS_SET(CharSpriteProperty_SetBoldWeight, m.props.bold = c.f(2))
MS_SET(CharSpriteProperty_SetEdgeWeight, m.props.edge = c.f(2))
MS_SET(CharSpriteProperty_SetEdgeColor, m.props.er = c.i(2); m.props.eg = c.i(3); m.props.eb = c.i(4))
MS_SET(ResetDefaultPos, m.x = m.defX; m.y = m.defY)
S4_HLL(StoatSpriteEngine, MultiSprite_GetTransitionInfo) {
    Multi &m = multi(c.i(0), c.i(1));
    c.ref(2) = m.transNum;
    c.ref(3) = m.transTime;
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, MultiSprite_GetCG) { c.retInt(multi(c.i(0), c.i(1)).cg); }
S4_HLL(StoatSpriteEngine, MultiSprite_GetPos) {
    Multi &m = multi(c.i(0), c.i(1));
    c.ref(2) = m.x;
    c.ref(3) = m.y;
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, MultiSprite_GetAlpha) { c.ref(2) = multi(c.i(0), c.i(1)).alpha; c.retBool(true); }
S4_HLL(StoatSpriteEngine, MultiSprite_ResetAllDefaultPos) {
    for (auto &[k, m] : g_multi)
        if (k.first == c.i(0)) { m.x = m.defX; m.y = m.defY; }
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, MultiSprite_ResetAllCG) {
    for (auto &[k, m] : g_multi) { m.cg = 0; m.text.clear(); }
}
S4_HLL(StoatSpriteEngine, MultiSprite_SetAllShowMessageFrame) { g_messageFrameShown = c.b(0); }
S4_HLL(StoatSpriteEngine, MultiSprite_UpdateView) {
    for (auto &[k, m] : g_multi) applyMulti(m);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, MultiSprite_Rebuild) {
    for (auto &[k, m] : g_multi) { m.shownCg = -1; m.shownText.clear(); applyMulti(m); }
}

// Moves run on a clock the game drives: it asks for the longest move, then sets every move's elapsed time.
S4_HLL(StoatSpriteEngine, MultiSprite_BeginMove) {
    Multi &m = multi(c.i(0), c.i(1));
    m.moving = true;
    m.move = Move{c.i(2), c.i(3), m.alpha, c.i(4), c.i(5), m.alpha, c.i(6), c.i(7), c.f(8), false};
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, MultiSprite_BeginMoveWithAlpha) {
    Multi &m = multi(c.i(0), c.i(1));
    m.moving = true;
    m.move = Move{c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7), c.i(8), c.i(9), c.f(10), true};
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, MultiSprite_GetMaxMoveTotalTime) {
    int t = 0;
    for (auto &[k, m] : g_multi)
        if (m.moving) t = std::max(t, m.move.time + m.move.wait);
    c.retInt(t);
}
S4_HLL(StoatSpriteEngine, MultiSprite_SetAllMoveCurrentTime) {
    int now = c.i(0);
    for (auto &[k, m] : g_multi) {
        if (!m.moving) continue;
        const Move &mv = m.move;
        float t = mv.time > 0 ? std::clamp(float(now - mv.wait) / float(mv.time), 0.0f, 1.0f) : 1.0f;
        // Acceleration bends the curve: positive starts slow, negative starts fast.
        if (mv.accel > 0) t = std::pow(t, 1.0f + mv.accel);
        else if (mv.accel < 0) t = 1.0f - std::pow(1.0f - t, 1.0f - mv.accel);
        m.x = mv.x0 + int(std::lround((mv.x1 - mv.x0) * t));
        m.y = mv.y0 + int(std::lround((mv.y1 - mv.y0) * t));
        if (mv.alpha) m.alpha = mv.a0 + int(std::lround((mv.a1 - mv.a0) * t));
    }
}
S4_HLL(StoatSpriteEngine, MultiSprite_EndAllMove) {
    for (auto &[k, m] : g_multi) {
        if (!m.moving) continue;
        m.x = m.move.x1;
        m.y = m.move.y1;
        if (m.move.alpha) m.alpha = m.move.a1;
        m.moving = false;
    }
}

// Encode and Decode keep the whole MultiSprite state in an int array the game stores in its saves.
void putString(std::vector<int32_t> &out, const std::string &s) {
    out.push_back(int32_t(s.size()));
    for (unsigned char ch : s) out.push_back(ch);
}
std::string getString(const std::vector<int32_t> &in, size_t &i) {
    std::string s;
    if (i >= in.size()) return s;
    size_t n = size_t(std::max(0, in[i++]));
    for (size_t k = 0; k < n && i < in.size(); k++) s += char(in[i++]);
    return s;
}
S4_HLL(StoatSpriteEngine, MultiSprite_Encode) {
    std::vector<int32_t> out{1, int32_t(g_multi.size()), g_messageFrameShown};
    for (auto &[k, m] : g_multi) {
        for (int32_t v : {k.first, k.second, m.cg, m.x, m.y, m.defX, m.defY, m.z, m.alpha, m.transNum, m.transTime,
                          int32_t(m.linked), m.parentFrame, m.origin, m.charSpace, m.props.type, m.props.size,
                          m.props.r, m.props.g, m.props.b, m.props.er, m.props.eg, m.props.eb})
            out.push_back(v);
        int32_t fb, fe;
        std::memcpy(&fb, &m.props.bold, 4);
        std::memcpy(&fe, &m.props.edge, 4);
        out.push_back(fb);
        out.push_back(fe);
        putString(out, m.text);
    }
    Page *a = c.page(0);
    if (a) a->v = std::move(out);
    c.retBool(a != nullptr);
}
S4_HLL(StoatSpriteEngine, MultiSprite_Decode) {
    Page *a = c.page(0);
    if (!a || a->v.size() < 3 || a->v[0] != 1) { c.retBool(false); return; }
    const auto &in = a->v;
    for (auto &[k, m] : g_multi)
        if (m.sprite) G().sprites.remove(m.sprite);
    g_multi.clear();
    size_t i = 1;
    int n = in[i++];
    g_messageFrameShown = in[i++];
    for (int k = 0; k < n && i + 25 <= in.size(); k++) {
        int type = in[i++], num = in[i++];
        Multi &m = multi(type, num);
        int32_t *f[] = {&m.cg, &m.x, &m.y, &m.defX, &m.defY, &m.z, &m.alpha, &m.transNum, &m.transTime};
        for (int32_t *p : f) *p = in[i++];
        m.linked = in[i++];
        m.parentFrame = in[i++];
        m.origin = in[i++];
        m.charSpace = in[i++];
        int32_t *pp[] = {&m.props.type, &m.props.size, &m.props.r, &m.props.g, &m.props.b, &m.props.er, &m.props.eg, &m.props.eb};
        for (int32_t *p : pp) *p = in[i++];
        std::memcpy(&m.props.bold, &in[i++], 4);
        std::memcpy(&m.props.edge, &in[i++], 4);
        m.text = getString(in, i);
    }
    c.retBool(true);
}

// ---- CharSprite: one sprite per character of message text ----

struct CharSpr {
    int sprite = 0;
    std::string ch;
    int alpha = 255;
    CharProps props;
};
std::map<int, CharSpr> g_chars;
int g_nextCharHandle = 1;

CharProps propsFrom(Page *p) {
    CharProps cp;
    if (!p || p->v.size() < 6) return cp;
    cp.type = p->v[0];
    cp.size = p->v[1];
    if (Page *col = G().vm->page(p->v[2]); col && col->v.size() >= 3) { cp.r = col->v[0]; cp.g = col->v[1]; cp.b = col->v[2]; }
    std::memcpy(&cp.bold, &p->v[3], 4);
    std::memcpy(&cp.edge, &p->v[4], 4);
    if (Page *col = G().vm->page(p->v[5]); col && col->v.size() >= 3) { cp.er = col->v[0]; cp.eg = col->v[1]; cp.eb = col->v[2]; }
    return cp;
}

S4_HLL(StoatSpriteEngine, CharSpriteManager_CreateHandle) {
    int h = g_nextCharHandle++;
    g_chars[h].sprite = g_nextInternal++;
    c.retInt(h);
}
S4_HLL(StoatSpriteEngine, CharSpriteManager_ReleaseHandle) {
    auto it = g_chars.find(c.i(0));
    if (it != g_chars.end()) { G().sprites.remove(it->second.sprite); g_chars.erase(it); }
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, CharSpriteManager_Clear) {
    for (auto &[h, cs] : g_chars) G().sprites.remove(cs.sprite);
    g_chars.clear();
}
S4_HLL(StoatSpriteEngine, CharSpriteManager_Rebuild) {}
// Save and Load keep every character sprite in an int array the game stores in its saves.
void drawChar(CharSpr &cs) {
    TextStyle st = styleOf(cs.props);
    int edge = int(st.edgeWeight + 0.5f);
    Sprite &s = G().sprites.get(cs.sprite);
    int x = s.x, y = s.y, z = s.z;
    bool show = s.used ? s.show : true;
    s.used = true;
    s.cg.reset();
    s.hasOwn = true;
    s.own.create(std::max(1, textWidth(cs.ch, st, 0) + edge * 2), st.size + edge * 2, true);
    drawText(s.own, edge, edge, cs.ch, st, 0);
    s.x = x; s.y = y; s.z = z; s.show = show;
    s.blendRate = cs.alpha;
}
S4_HLL(StoatSpriteEngine, CharSpriteManager_Save) {
    std::vector<int32_t> out{1, int32_t(g_chars.size()), g_nextCharHandle};
    for (auto &[h, cs] : g_chars) {
        Sprite *s = G().sprites.find(cs.sprite);
        int32_t fb, fe;
        std::memcpy(&fb, &cs.props.bold, 4);
        std::memcpy(&fe, &cs.props.edge, 4);
        for (int32_t v : {h, cs.alpha, s ? s->x : 0, s ? s->y : 0, s ? s->z : 0, s ? int32_t(s->show) : 0, cs.props.type,
                          cs.props.size, cs.props.r, cs.props.g, cs.props.b, fb, fe, cs.props.er, cs.props.eg, cs.props.eb})
            out.push_back(v);
        putString(out, cs.ch);
    }
    if (Page *a = c.page(0)) a->v = std::move(out);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, CharSpriteManager_Load) {
    Page *a = c.page(0);
    for (auto &[h, cs] : g_chars) G().sprites.remove(cs.sprite);
    g_chars.clear();
    if (!a || a->v.size() < 3 || a->v[0] != 1) { c.retBool(false); return; }
    const auto &in = a->v;
    size_t i = 1;
    int n = in[i++];
    g_nextCharHandle = in[i++];
    for (int k = 0; k < n && i + 16 <= in.size(); k++) {
        int h = in[i++];
        CharSpr &cs = g_chars[h];
        cs.sprite = g_nextInternal++;
        cs.alpha = in[i++];
        int x = in[i++], y = in[i++], z = in[i++], show = in[i++];
        cs.props.type = in[i++];
        cs.props.size = in[i++];
        cs.props.r = in[i++]; cs.props.g = in[i++]; cs.props.b = in[i++];
        std::memcpy(&cs.props.bold, &in[i++], 4);
        std::memcpy(&cs.props.edge, &in[i++], 4);
        cs.props.er = in[i++]; cs.props.eg = in[i++]; cs.props.eb = in[i++];
        cs.ch = getString(in, i);
        Sprite &s = G().sprites.get(cs.sprite);
        s.x = x; s.y = y; s.z = z;
        drawChar(cs);
        s.show = show;
    }
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, CharSprite_Release) {
    auto it = g_chars.find(c.i(0));
    if (it != g_chars.end()) { G().sprites.remove(it->second.sprite); g_chars.erase(it); }
}
S4_HLL(StoatSpriteEngine, CharSprite_SetChar) {
    auto it = g_chars.find(c.i(0));
    if (it == g_chars.end()) return;
    CharSpr &cs = it->second;
    cs.ch = c.s(1);
    cs.props = propsFrom(c.page(2));
    drawChar(cs);
}
#define CS(h) auto it = g_chars.find(h); if (it == g_chars.end()) return; Sprite &s = G().sprites.get(it->second.sprite)
S4_HLL(StoatSpriteEngine, CharSprite_SetPos) { CS(c.i(0)); s.x = c.i(1); s.y = c.i(2); }
S4_HLL(StoatSpriteEngine, CharSprite_SetZ) { CS(c.i(0)); s.z = c.i(1); }
S4_HLL(StoatSpriteEngine, CharSprite_SetAlphaRate) { CS(c.i(0)); it->second.alpha = s.blendRate = std::clamp(c.i(1), 0, 255); }
S4_HLL(StoatSpriteEngine, CharSprite_SetShow) { CS(c.i(0)); s.show = c.b(1); }
#undef CS
S4_HLL(StoatSpriteEngine, CharSprite_GetChar) {
    auto it = g_chars.find(c.i(0));
    c.refS(1) = it == g_chars.end() ? "" : it->second.ch;
}
S4_HLL(StoatSpriteEngine, CharSprite_GetAlphaRate) {
    auto it = g_chars.find(c.i(0));
    c.retInt(it == g_chars.end() ? 0 : it->second.alpha);
}

// ---- ADVLogList: the backlog, pages of text lines with an optional voice each ----

struct LogPage { std::vector<std::string> lines{""}; int voice = 0; };
std::vector<LogPage> g_log;
bool g_logEnabled = true;

S4_HLL(StoatSpriteEngine, ADVLogList_Clear) { g_log.clear(); }
S4_HLL(StoatSpriteEngine, ADVLogList_AddText) {
    if (!g_logEnabled) return;
    if (g_log.empty()) g_log.emplace_back();
    g_log.back().lines.back() += c.s(0);
}
S4_HLL(StoatSpriteEngine, ADVLogList_AddNewLine) {
    if (!g_logEnabled) return;
    if (g_log.empty()) g_log.emplace_back();
    g_log.back().lines.emplace_back();
}
S4_HLL(StoatSpriteEngine, ADVLogList_AddNewPage) {
    if (!g_logEnabled) return;
    g_log.emplace_back();
    if (g_log.size() > 1000) g_log.erase(g_log.begin());
}
S4_HLL(StoatSpriteEngine, ADVLogList_AddVoice) {
    if (!g_logEnabled) return;
    if (g_log.empty()) g_log.emplace_back();
    g_log.back().voice = c.i(0);
}
S4_HLL(StoatSpriteEngine, ADVLogList_SetEnable) { g_logEnabled = c.b(0); }
S4_HLL(StoatSpriteEngine, ADVLogList_IsEnable) { c.retBool(g_logEnabled); }
S4_HLL(StoatSpriteEngine, ADVLogList_GetNumofADVLog) { c.retInt(int32_t(g_log.size())); }
S4_HLL(StoatSpriteEngine, ADVLogList_GetNumofADVLogText) {
    int n = c.i(0);
    c.retInt(n >= 0 && size_t(n) < g_log.size() ? int32_t(g_log[n].lines.size()) : 0);
}
S4_HLL(StoatSpriteEngine, ADVLogList_GetADVLogText) {
    int n = c.i(0), t = c.i(1);
    c.refS(2) = n >= 0 && size_t(n) < g_log.size() && t >= 0 && size_t(t) < g_log[n].lines.size() ? g_log[n].lines[t] : "";
}
S4_HLL(StoatSpriteEngine, ADVLogList_GetADVLogVoice) {
    int n = c.i(0);
    c.retInt(n >= 0 && size_t(n) < g_log.size() ? g_log[n].voice : 0);
}
S4_HLL(StoatSpriteEngine, ADVLogList_Save) {
    std::vector<int32_t> out{1, int32_t(g_log.size())};
    for (auto &p : g_log) {
        out.push_back(p.voice);
        out.push_back(int32_t(p.lines.size()));
        for (auto &l : p.lines) putString(out, l);
    }
    if (Page *a = c.page(0)) a->v = std::move(out);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, ADVLogList_Load) {
    Page *a = c.page(0);
    g_log.clear();
    if (!a || a->v.size() < 2 || a->v[0] != 1) { c.retBool(false); return; }
    size_t i = 1;
    int n = a->v[i++];
    for (int k = 0; k < n && i + 2 <= a->v.size(); k++) {
        LogPage p;
        p.voice = a->v[i++];
        int lines = a->v[i++];
        p.lines.clear();
        for (int l = 0; l < lines; l++) p.lines.push_back(getString(a->v, i));
        g_log.push_back(std::move(p));
    }
    c.retBool(true);
}

// ---- ADVSceneKeeper: copies of a scene struct the game hands over, for the scene replay list ----

std::vector<std::vector<int32_t>> g_scenes;  // the struct's int members
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_AddADVScene) {
    Page *p = c.page(0);
    if (p) g_scenes.push_back(p->v);
    c.retBool(p != nullptr);
}
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_GetNumofADVScene) { c.retInt(int32_t(g_scenes.size())); }
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_GetADVScene) {
    int n = c.i(0);
    Page *p = c.page(1);
    if (!p || n < 0 || size_t(n) >= g_scenes.size()) { c.retBool(false); return; }
    for (size_t k = 0; k < p->v.size() && k < g_scenes[n].size(); k++) {
        VarType t = G().vm->varType(*p, k);
        if (t.type == T_INT || t.type == T_BOOL || t.type == T_FLOAT) p->v[k] = g_scenes[n][k];
    }
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_Clear) { g_scenes.clear(); }
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_Save) {
    std::vector<int32_t> out{1, int32_t(g_scenes.size())};
    for (auto &s : g_scenes) {
        out.push_back(int32_t(s.size()));
        out.insert(out.end(), s.begin(), s.end());
    }
    if (Page *a = c.page(0)) a->v = std::move(out);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_Load) {
    Page *a = c.page(0);
    g_scenes.clear();
    if (!a || a->v.size() < 2 || a->v[0] != 1) { c.retBool(false); return; }
    size_t i = 1;
    int n = a->v[i++];
    for (int k = 0; k < n && i < a->v.size(); k++) {
        size_t len = size_t(std::max(0, a->v[i++]));
        len = std::min(len, a->v.size() - i);
        g_scenes.emplace_back(a->v.begin() + i, a->v.begin() + i + len);
        i += len;
    }
    c.retBool(true);
}

// ---- DrawGraph: drawing on sprite pictures, which the call names by sprite number ----

Surface *surf(int sp) {
    Sprite *s = G().sprites.find(sp);
    return s ? &s->writable() : nullptr;
}

S4_HLL(DrawGraph, Fill) {
    if (Surface *d = surf(c.i(0))) d->fill(c.i(1), c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7), -1);
}
S4_HLL(DrawGraph, FillAMap) {
    if (Surface *d = surf(c.i(0))) { d->hasAlpha = true; d->fill(c.i(1), c.i(2), c.i(3), c.i(4), -1, 0, 0, c.i(5)); }
}
S4_HLL(DrawGraph, FillWithAlpha) {
    if (Surface *d = surf(c.i(0))) { d->hasAlpha = true; d->fill(c.i(1), c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7), c.i(8)); }
}
S4_HLL(DrawGraph, FillAlphaColor) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    int x0 = std::max(0, c.i(1)), y0 = std::max(0, c.i(2)), x1 = std::min(d->w, c.i(1) + c.i(3)), y1 = std::min(d->h, c.i(2) + c.i(4));
    int r = c.i(5), g = c.i(6), b = c.i(7), a = c.i(8);
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++) {
            uint8_t *p = d->at(x, y);
            p[0] = uint8_t(p[0] + (r - p[0]) * a / 255);
            p[1] = uint8_t(p[1] + (g - p[1]) * a / 255);
            p[2] = uint8_t(p[2] + (b - p[2]) * a / 255);
        }
}
// Copies a rectangle of pixels, or of the alpha map, between sprite pictures.
void copyRect(int dst, int dx, int dy, int src, int sx, int sy, int w, int h, bool pixels, bool alpha) {
    Sprite *ss = G().sprites.find(src);
    Surface *d = surf(dst);
    if (!ss || !d || !ss->pixels()) return;
    const uint8_t *sp = ss->pixels();
    int sw = ss->width(), sh = ss->height();
    bool sAlpha = ss->hasAlpha();
    if (alpha) d->hasAlpha = true;
    for (int y = 0; y < h; y++) {
        int ty = dy + y, fy = sy + y;
        if (ty < 0 || ty >= d->h || fy < 0 || fy >= sh) continue;
        for (int x = 0; x < w; x++) {
            int tx = dx + x, fx = sx + x;
            if (tx < 0 || tx >= d->w || fx < 0 || fx >= sw) continue;
            const uint8_t *s = sp + (size_t(fy) * sw + fx) * 4;
            uint8_t *p = d->at(tx, ty);
            if (pixels) { p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; }
            if (alpha) p[3] = sAlpha ? s[3] : 255;
        }
    }
}
S4_HLL(DrawGraph, Copy) { copyRect(c.i(0), c.i(1), c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7), true, false); }
S4_HLL(DrawGraph, CopyAMap) { copyRect(c.i(0), c.i(1), c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7), false, true); }
S4_HLL(DrawGraph, CopyWithAlphaMap) { copyRect(c.i(0), c.i(1), c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7), true, true); }
// Bilinear scaling of pixels or the alpha map.
void stretch(HllCall &c, bool pixels, bool alpha) {
    Sprite *ss = G().sprites.find(c.i(5));
    Surface *d = surf(c.i(0));
    if (!ss || !d || !ss->pixels()) return;
    int dx = c.i(1), dy = c.i(2), dw = c.i(3), dh = c.i(4), sx = c.i(6), sy = c.i(7), sw = c.i(8), sh = c.i(9);
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    const uint8_t *sp = ss->pixels();
    int W = ss->width(), H = ss->height();
    bool sAlpha = ss->hasAlpha();
    if (alpha) d->hasAlpha = true;
    for (int y = 0; y < dh; y++) {
        int ty = dy + y;
        if (ty < 0 || ty >= d->h) continue;
        float fy = sy + (y + 0.5f) * sh / dh - 0.5f;
        int y0 = std::clamp(int(std::floor(fy)), 0, H - 1), y1 = std::min(y0 + 1, H - 1);
        float wy = std::clamp(fy - std::floor(fy), 0.0f, 1.0f);
        for (int x = 0; x < dw; x++) {
            int tx = dx + x;
            if (tx < 0 || tx >= d->w) continue;
            float fx = sx + (x + 0.5f) * sw / dw - 0.5f;
            int x0 = std::clamp(int(std::floor(fx)), 0, W - 1), x1 = std::min(x0 + 1, W - 1);
            float wx = std::clamp(fx - std::floor(fx), 0.0f, 1.0f);
            uint8_t *p = d->at(tx, ty);
            for (int ch = 0; ch < 4; ch++) {
                if (ch < 3 && !pixels) continue;
                if (ch == 3 && !alpha) continue;
                auto at = [&](int xx, int yy) { return ch == 3 && !sAlpha ? 255.0f : float(sp[(size_t(yy) * W + xx) * 4 + ch]); };
                float v = (at(x0, y0) * (1 - wx) + at(x1, y0) * wx) * (1 - wy) + (at(x0, y1) * (1 - wx) + at(x1, y1) * wx) * wy;
                p[ch] = uint8_t(std::clamp(v + 0.5f, 0.0f, 255.0f));
            }
        }
    }
}
S4_HLL(DrawGraph, CopyStretchInterp) { stretch(c, true, false); }
S4_HLL(DrawGraph, CopyStretchAMapInterp) { stretch(c, false, true); }
S4_HLL(DrawGraph, CopyStretch) { stretch(c, true, false); }
S4_HLL(DrawGraph, CopyStretchAMap) { stretch(c, false, true); }
S4_HLL(DrawGraph, CopyReduce) { stretch(c, true, false); }
S4_HLL(DrawGraph, CopyReduceAMap) { stretch(c, false, true); }
S4_HLL(DrawGraph, DrawLine) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    int x0 = c.i(1), y0 = c.i(2), x1 = c.i(3), y1 = c.i(4);
    int n = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int k = 0; k <= n; k++) {
        int x = n ? x0 + (x1 - x0) * k / n : x0, y = n ? y0 + (y1 - y0) * k / n : y0;
        if (x >= 0 && y >= 0 && x < d->w && y < d->h) { uint8_t *p = d->at(x, y); p[0] = uint8_t(c.i(5)); p[1] = uint8_t(c.i(6)); p[2] = uint8_t(c.i(7)); }
    }
}

TextStyle g_drawFont;
S4_HLL(DrawGraph, SetFontSize) { g_drawFont.size = c.i(0); }
S4_HLL(DrawGraph, SetFontName) { g_drawFont.face = c.s(0).find("\x96\xbe\x92\xa9") != std::string::npos ? 1 : 0; }  // 明朝
S4_HLL(DrawGraph, SetFontWeight) { g_drawFont.weight = c.i(0); }
S4_HLL(DrawGraph, SetFontUnderline) {}
S4_HLL(DrawGraph, SetFontStrikeOut) {}
int g_drawFontSpace = 0;

S4_HLL_RESET {
    g_nextInternal = 1000000;
    g_multi.clear();
    g_messageFrameShown = true;
    g_chars.clear();
    g_nextCharHandle = 1;
    g_log.clear();
    g_logEnabled = true;
    g_scenes.clear();
    g_drawFont = {};
    g_drawFontSpace = 0;
}
S4_HLL(DrawGraph, SetFontSpace) { g_drawFontSpace = c.i(0); }
S4_HLL(DrawGraph, SetFontColor) { g_drawFont.r = c.i(0); g_drawFont.g = c.i(1); g_drawFont.b = c.i(2); }
S4_HLL(DrawGraph, GetFontSize) { c.retInt(g_drawFont.size); }
S4_HLL(DrawGraph, GetFontWeight) { c.retInt(g_drawFont.weight); }
// Text drawn into the pixels keeps the alpha map; text drawn into the alpha map writes the coverage there.
S4_HLL(DrawGraph, DrawTextToPMap) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Surface layer;
    layer.create(d->w, d->h, true);
    drawText(layer, c.i(1), c.i(2), c.s(3), g_drawFont, g_drawFontSpace);
    for (size_t i = 0; i < layer.px.size(); i += 4) {
        int a = layer.px[i + 3];
        if (!a) continue;
        for (int k = 0; k < 3; k++) d->px[i + k] = uint8_t(d->px[i + k] + (layer.px[i + k] - d->px[i + k]) * a / 255);
    }
}
S4_HLL(DrawGraph, DrawTextToAMap) {
    Surface *d = surf(c.i(0));
    if (!d) return;
    Surface layer;
    layer.create(d->w, d->h, true);
    drawText(layer, c.i(1), c.i(2), c.s(3), g_drawFont, g_drawFontSpace);
    d->hasAlpha = true;
    for (size_t i = 3; i < layer.px.size(); i += 4) d->px[i] = std::max(d->px[i], layer.px[i]);
}

}  // namespace
}  // namespace s4
