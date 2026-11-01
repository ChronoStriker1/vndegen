// StoatSpriteEngine's adventure layer, which the SDK predates: MultiSprite (backgrounds, faces, pictures and text
// sprites the scenario sets by kind and number), CharSprite (message text drawn as one sprite per character), the
// backlog (ADVLogList) and saved scenes (ADVSceneKeeper). Their behaviour was read from how the games call them;
// docs/system4-cleanroom.md lists what each call was taken to mean.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

#include "font.h"
#include "game.h"
#include "serial.h"
#include "sjis.h"

namespace s4 {
namespace {

Game &G() { return game(); }

// Engine sprite numbers for MultiSprite and CharSprite start here, above any number a game uses.
int g_nextInternal = 1000000;

struct CharProps {
    int type = 0, size = 24;
    int r = 255, g = 255, b = 255, a = 255;
    float bold = 0, edge = 0;
    int er = 0, eg = 0, eb = 0, ea = 255;
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
    int origin = 1;  // reading order: 1 top left, 5 centre, 9 bottom right
    std::string text;
    int charSpace = 0;
    CharProps props = {.size = 16};
    // Fields the PC keeps whose meaning is unknown, carried through saves with the PC's values.
    int unknown8 = 1, unknown18 = 0, unknown22 = 190, unknown23 = 0;
    int sprite = 0;
    bool moving = false;
    Move move{};
    int shownCg = -1;
    std::string shownText;
};

std::map<std::pair<int, int>, Multi> g_multi;
bool g_messageFrameShown = true;

std::vector<std::pair<int, int>> g_multiOrder;  // creation order, which the PC's encoding keeps

Multi &multi(int type, int num) {
    auto [it, made] = g_multi.try_emplace({type, num});
    if (made) g_multiOrder.push_back({type, num});
    return it->second;
}

// The point a position names, by origin mode, in reading order: 1 top left (the PC's default, which every picture
// uses), 5 centre, 9 bottom right. Other values are top left.
void anchor(int mode, int w, int h, int &dx, int &dy) {
    dx = dy = 0;
    if (mode < 1 || mode > 9) return;
    int col = (mode - 1) % 3, row = (mode - 1) / 3;
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

// Encode and Decode keep the whole MultiSprite state in an int array the game stores in its saves, in the PC's
// layout: a byte stream copied into the ints four bytes at a time, padded with zeros. Taken from the routine in Test
// Game 19's StoatSpriteEngine.dll that writes it, and checked against a save the PC wrote:
//   "MST\0", u32 version 0
//   u32 7, then for each kind 0 to 6: u32 n, n sprite numbers, u32 n, n record indices (creation order)
//   u32 records, then each: u32 0 for none, or u32 1 and the sprite: kind, CG, x, y, z, alpha, default x and y,
//     u32 (1), origin mode, text and NUL, font type, font size, colour RGBA as 4 bytes, edge colour RGBA, bold and
//     edge weights as floats, character space, u32 (0), u32 linked to the message frame, parent frame, transition
//     number and time, u32 (190), u32 (0). The parenthesised fields are unknown; their values are carried through.
// Moves in progress aren't kept.
constexpr int kMultiKinds = 7;
struct Bytes {
    std::vector<uint8_t> b;
    void u32(int32_t v) { for (int k = 0; k < 4; k++) b.push_back(uint8_t(uint32_t(v) >> (8 * k))); }
    void u8(int v) { b.push_back(uint8_t(v)); }
    void f32(float f) { int32_t v; std::memcpy(&v, &f, 4); u32(v); }
    void str(const std::string &s) { b.insert(b.end(), s.begin(), s.end()); b.push_back(0); }
};
struct ByteReader {
    std::vector<uint8_t> b;
    size_t i = 0;
    bool bad = false;
    int32_t u32() {
        if (i + 4 > b.size()) { bad = true; i = b.size(); return 0; }
        int32_t v = int32_t(uint32_t(b[i]) | uint32_t(b[i + 1]) << 8 | uint32_t(b[i + 2]) << 16 | uint32_t(b[i + 3]) << 24);
        i += 4;
        return v;
    }
    int u8() {
        if (i >= b.size()) { bad = true; return 0; }
        return b[i++];
    }
    float f32() { int32_t v = u32(); float f; std::memcpy(&f, &v, 4); return f; }
    std::string str() {
        size_t j = i;
        while (j < b.size() && b[j]) j++;
        if (j >= b.size()) { bad = true; i = b.size(); return {}; }
        std::string s(reinterpret_cast<const char *>(b.data() + i), j - i);
        i = j + 1;
        return s;
    }
};

S4_HLL(StoatSpriteEngine, MultiSprite_Encode) {
    if (std::getenv("S4_MSDUMP"))
        for (auto &[k, m] : g_multi)
            G().log("ms " + std::to_string(k.first) + " " + std::to_string(k.second) + " cg " + std::to_string(m.cg) +
                    " pos " + std::to_string(m.x) + " " + std::to_string(m.y) + " z " + std::to_string(m.z) + " origin " +
                    std::to_string(m.origin) + " linked " + std::to_string(m.linked) + " text " + std::to_string(m.text.size()));
    Bytes o;
    o.str("MST");
    o.u32(0);
    o.u32(kMultiKinds);
    for (int kind = 0; kind < kMultiKinds; kind++) {
        std::vector<int32_t> nums, recs;
        for (size_t r = 0; r < g_multiOrder.size(); r++)
            if (g_multiOrder[r].first == kind) { nums.push_back(g_multiOrder[r].second); recs.push_back(int32_t(r)); }
        o.u32(int32_t(nums.size()));
        for (int32_t n : nums) o.u32(n);
        o.u32(int32_t(recs.size()));
        for (int32_t r : recs) o.u32(r);
    }
    o.u32(int32_t(g_multiOrder.size()));
    for (auto &k : g_multiOrder) {
        const Multi &m = g_multi[k];
        const CharProps &p = m.props;
        o.u32(1);
        for (int32_t v : {k.first, m.cg, m.x, m.y, m.z, m.alpha, m.defX, m.defY, m.unknown8, m.origin}) o.u32(v);
        o.str(m.text);
        o.u32(p.type);
        o.u32(p.size);
        for (int v : {p.r, p.g, p.b, p.a, p.er, p.eg, p.eb, p.ea}) o.u8(v);
        o.f32(p.bold);
        o.f32(p.edge);
        for (int32_t v : {m.charSpace, m.unknown18, int32_t(m.linked), m.parentFrame, m.transNum, m.transTime, m.unknown22,
                          m.unknown23})
            o.u32(v);
    }
    while (o.b.size() % 4) o.b.push_back(0);
    Page *a = c.page(0);
    if (a) {
        a->v.resize(o.b.size() / 4);
        std::memcpy(a->v.data(), o.b.data(), o.b.size());
    }
    c.retBool(a != nullptr);
}
S4_HLL(StoatSpriteEngine, MultiSprite_Decode) {
    Page *a = c.page(0);
    if (!a) { c.retBool(false); return; }
    ByteReader in;
    in.b.resize(a->v.size() * 4);
    std::memcpy(in.b.data(), a->v.data(), in.b.size());
    if (in.str() != "MST" || in.u32() != 0) { c.retBool(false); return; }
    // Which sprite each record is.
    std::map<int32_t, std::pair<int, int>> owner;
    int kinds = in.u32();
    for (int kind = 0; kind < kinds && !in.bad; kind++) {
        std::vector<int32_t> nums(size_t(std::max(0, in.u32())));
        for (auto &n : nums) n = in.u32();
        int32_t nrec = in.u32();
        for (int32_t k = 0; k < nrec && !in.bad; k++) {
            int32_t r = in.u32();
            if (size_t(k) < nums.size()) owner[r] = {kind, nums[size_t(k)]};
        }
    }
    std::map<std::pair<int, int>, Multi> multis;
    std::vector<std::pair<int, int>> order;
    int32_t nrec = in.u32();
    for (int32_t r = 0; r < nrec && !in.bad; r++) {
        if (in.u32() == 0) continue;
        Multi m;
        CharProps &p = m.props;
        int kind = in.u32();
        int32_t *ints[] = {&m.cg, &m.x, &m.y, &m.z, &m.alpha, &m.defX, &m.defY, &m.unknown8, &m.origin};
        for (int32_t *v : ints) *v = in.u32();
        m.text = in.str();
        p.type = in.u32();
        p.size = in.u32();
        int *bytes[] = {&p.r, &p.g, &p.b, &p.a, &p.er, &p.eg, &p.eb, &p.ea};
        for (int *v : bytes) *v = in.u8();
        p.bold = in.f32();
        p.edge = in.f32();
        m.charSpace = in.u32();
        m.unknown18 = in.u32();
        m.linked = in.u32() != 0;
        m.parentFrame = in.u32();
        m.transNum = in.u32();
        m.transTime = in.u32();
        m.unknown22 = in.u32();
        m.unknown23 = in.u32();
        auto it = owner.find(r);
        if (it == owner.end() || it->second.first != kind) continue;
        multis[it->second] = std::move(m);
        order.push_back(it->second);
    }
    if (in.bad) { c.retBool(false); return; }
    for (auto &[k, m] : g_multi)
        if (m.sprite) G().sprites.remove(m.sprite);
    g_multi = std::move(multis);
    g_multiOrder = std::move(order);
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
    if (Page *col = G().vm->page(p->v[2]); col && col->v.size() >= 4) { cp.r = col->v[0]; cp.g = col->v[1]; cp.b = col->v[2]; cp.a = col->v[3]; }
    std::memcpy(&cp.bold, &p->v[3], 4);
    std::memcpy(&cp.edge, &p->v[4], 4);
    if (Page *col = G().vm->page(p->v[5]); col && col->v.size() >= 4) { cp.er = col->v[0]; cp.eg = col->v[1]; cp.eb = col->v[2]; cp.ea = col->v[3]; }
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
// The saved form, as the PC writes it: "CSM", version 0, the number of handles ever made, then for each handle 0
// when it is free, or 1, the character, the CASCharSpriteProperty struct's fields (type, size, colour RGBA, bold and
// edge weights as float bits, edge colour RGBA), x, y, z, alpha rate, shown, and the engine's sprite number. The PC
// numbered those sprites 2000 plus the handle.
constexpr int kCharSpriteBase = 2000;
S4_HLL(StoatSpriteEngine, CharSpriteManager_Save) {
    std::vector<int32_t> out;
    serial::putTag(out, "CSM");
    out.push_back(0);
    out.push_back(g_nextCharHandle);
    for (int h = 0; h < g_nextCharHandle; h++) {
        auto it = g_chars.find(h);
        if (it == g_chars.end()) { out.push_back(0); continue; }
        const CharSpr &cs = it->second;
        const CharProps &p = cs.props;
        Sprite *s = G().sprites.find(cs.sprite);
        int32_t fb, fe;
        std::memcpy(&fb, &p.bold, 4);
        std::memcpy(&fe, &p.edge, 4);
        out.push_back(1);
        serial::putString(out, cs.ch);
        for (int32_t v : {p.type, p.size, p.r, p.g, p.b, p.a, fb, fe, p.er, p.eg, p.eb, p.ea, s ? s->x : 0, s ? s->y : 0,
                          s ? s->z : 0, cs.alpha, s ? int32_t(s->show) : 0, kCharSpriteBase + h})
            out.push_back(v);
    }
    if (Page *a = c.page(0)) a->v = std::move(out);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, CharSpriteManager_Load) {
    // Every record is read before anything changes, so damaged data leaves the current sprites alone.
    Page *a = c.page(0);
    size_t i = 0;
    if (!a || !serial::getTag(a->v, i, "CSM") || i + 2 > a->v.size() || a->v[i] != 0 || a->v[i + 1] < 0) {
        c.retBool(false);
        return;
    }
    const auto &in = a->v;
    int n = in[i + 1];
    i += 2;
    struct Rec { int handle; std::string ch; const int32_t *v; };
    std::vector<Rec> recs;
    for (int h = 0; h < n; h++) {
        if (i >= in.size()) { c.retBool(false); return; }
        if (in[i++] == 0) continue;
        std::string ch;
        if (!serial::getString(in, i, ch) || in.size() - i < 18) { c.retBool(false); return; }
        recs.push_back({h, std::move(ch), &in[i]});
        i += 18;
    }
    for (auto &[h, cs] : g_chars) G().sprites.remove(cs.sprite);
    g_chars.clear();
    for (const Rec &r : recs) {
        const int32_t *v = r.v;
        CharSpr &cs = g_chars[r.handle];
        cs.ch = r.ch;
        cs.sprite = g_nextInternal++;
        CharProps &p = cs.props;
        p.type = v[0]; p.size = v[1];
        p.r = v[2]; p.g = v[3]; p.b = v[4]; p.a = v[5];
        std::memcpy(&p.bold, &v[6], 4);
        std::memcpy(&p.edge, &v[7], 4);
        p.er = v[8]; p.eg = v[9]; p.eb = v[10]; p.ea = v[11];
        cs.alpha = v[15];
        // v[17] is the PC's sprite number; this engine numbers its own.
        Sprite &s = G().sprites.get(cs.sprite);
        s.x = v[12]; s.y = v[13]; s.z = v[14];
        drawChar(cs);
        s.show = v[16] != 0;
    }
    g_nextCharHandle = std::max(n, 1);
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
    auto &lines = g_log.back().lines;
    if (lines.empty()) lines.emplace_back();  // a page loaded from a save can have no lines
    lines.back() += c.s(0);
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
// The saved form, as the PC writes it: "ADL", version 0, the number of pages, each page's line count, lines and
// voice, then whether the log is enabled.
S4_HLL(StoatSpriteEngine, ADVLogList_Save) {
    std::vector<int32_t> out;
    serial::putTag(out, "ADL");
    out.push_back(0);
    out.push_back(int32_t(g_log.size()));
    for (auto &p : g_log) {
        out.push_back(int32_t(p.lines.size()));
        for (auto &l : p.lines) serial::putString(out, l);
        out.push_back(p.voice);
    }
    out.push_back(g_logEnabled);
    if (Page *a = c.page(0)) a->v = std::move(out);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, ADVLogList_Load) {
    Page *a = c.page(0);
    g_log.clear();
    size_t i = 0;
    if (!a || !serial::getTag(a->v, i, "ADL") || i + 2 > a->v.size() || a->v[i] != 0) { c.retBool(false); return; }
    const auto &in = a->v;
    int n = in[i + 1];
    i += 2;
    for (int k = 0; k < n; k++) {
        // Each line takes at least its NUL, so a count beyond what's left is damage.
        if (i >= in.size() || in[i] < 0 || size_t(in[i]) > in.size() - i - 1) { g_log.clear(); c.retBool(false); return; }
        LogPage p;
        p.lines.assign(size_t(in[i++]), "");
        for (auto &l : p.lines)
            if (!serial::getString(in, i, l)) { g_log.clear(); c.retBool(false); return; }
        if (i >= in.size()) { g_log.clear(); c.retBool(false); return; }
        p.voice = in[i++];
        g_log.push_back(std::move(p));
    }
    if (i < in.size()) g_logEnabled = in[i] != 0;
    c.retBool(true);
}

// ---- MsgLogManager and MsgLogViewer: Test Game 46's backlog ----
// The game adds typed entries to the manager as it shows a message: 1001 text, 1101 a new line, 1102 the wait for
// a click that ends a message, 1211 a voice, and style codes (13xx and others) the backlog doesn't need. The viewer
// turns them into sact_log_data_t lines (text, voice, separator); a separator is a line of its own between messages,
// which the game's backlog walks back to.

struct MsgLogEntry { int type; int value; std::string text; };
std::vector<MsgLogEntry> g_msgLog;
size_t g_msgLogMax = 10000;
struct ViewerLine { std::string text; int voice = 0; bool separator = false; };
std::vector<ViewerLine> g_viewer;

void msgLogAdd(MsgLogEntry e) {
    g_msgLog.push_back(std::move(e));
    if (g_msgLog.size() > g_msgLogMax) g_msgLog.erase(g_msgLog.begin(), g_msgLog.begin() + ptrdiff_t(g_msgLog.size() - g_msgLogMax));
}
const MsgLogEntry *msgLogAt(int n) { return n >= 0 && size_t(n) < g_msgLog.size() ? &g_msgLog[size_t(n)] : nullptr; }

S4_HLL(MsgLogManager, Numof) { c.retInt(int32_t(g_msgLog.size())); }
S4_HLL(MsgLogManager, AddInt) { msgLogAdd({c.i(0), c.i(1), {}}); }
S4_HLL(MsgLogManager, AddString) { msgLogAdd({c.i(0), 0, c.s(1)}); }
S4_HLL(MsgLogManager, GetType) { auto e = msgLogAt(c.i(0)); c.retInt(e ? e->type : 0); }
S4_HLL(MsgLogManager, GetInt) { auto e = msgLogAt(c.i(0)); c.retInt(e ? e->value : 0); }
S4_HLL(MsgLogManager, GetString) { auto e = msgLogAt(c.i(0)); c.refS(1) = e ? e->text : ""; }
S4_HLL(MsgLogManager, SetLineMax) { g_msgLogMax = size_t(std::max(1, c.i(0))); }
S4_HLL(MsgLogManager, GetInterface) { c.retInt(1); }  // the handle the viewer converts from; there is one manager

S4_HLL(MsgLogViewer, Numof) { c.retInt(int32_t(g_viewer.size())); }
S4_HLL(MsgLogViewer, Clear) { g_viewer.clear(); }
S4_HLL(MsgLogViewer, Add) {
    Page *p = c.page(0);
    if (!p || p->v.size() < 3) return;
    g_viewer.push_back({c.vm.valid(p->v[0]) ? c.vm.str(p->v[0]) : "", p->v[1], p->v[2] != 0});
}
S4_HLL(MsgLogViewer, Get) {
    Page *p = c.page(1);
    int n = c.i(0);
    if (!p || p->v.size() < 3) return;
    ViewerLine l = n >= 0 && size_t(n) < g_viewer.size() ? g_viewer[size_t(n)] : ViewerLine{};
    if (c.vm.valid(p->v[0])) c.vm.str(p->v[0]) = l.text;
    else p->v[0] = c.vm.newString(l.text);
    p->v[1] = l.voice;
    p->v[2] = l.separator;
}
S4_HLL(MsgLogViewer, ConvertFromMsgLogManager) {
    g_viewer.clear();
    ViewerLine line;
    bool any = false;  // the current message has a line in it
    auto endLine = [&] {
        g_viewer.push_back(line);
        line = {};
        any = true;
    };
    for (auto &e : g_msgLog) {
        switch (e.type) {
        case 1001: line.text += e.text; break;
        case 1101: endLine(); break;
        case 1211: line.voice = e.value; break;
        case 1102:
            if (!line.text.empty()) endLine();
            if (any) g_viewer.push_back({{}, 0, true});
            any = false;
            break;
        default: break;
        }
    }
    if (!line.text.empty()) g_viewer.push_back(line);
}

// ---- ADVSceneKeeper: copies of a scene struct the game hands over, for the scene replay list ----
// Each scene is kept in the PC's encoding (serial.h). The saved form is "ADS", version 0, the number of scenes and
// the scenes one after another, with nothing marking where one ends: a loaded list is split when the game first
// hands over a struct, whose type gives each scene's length.

struct SceneKeeper {
    std::vector<std::vector<int32_t>> scenes;
    std::vector<int32_t> pending;  // loaded and not yet split
    int pendingCount = 0;

    int count() const { return int(scenes.size()) + pendingCount; }
    bool split(int32_t structType) {
        if (!pendingCount) return true;
        size_t i = 0;
        for (int k = 0; k < pendingCount; k++) {
            size_t start = i;
            if (!serial::skipStruct(*G().vm, pending, i, structType)) {
                G().log("ADVSceneKeeper: the saved scenes don't fit their struct");
                pending.clear();
                pendingCount = 0;
                return false;
            }
            scenes.emplace_back(pending.begin() + ptrdiff_t(start), pending.begin() + ptrdiff_t(i));
        }
        pending.clear();
        pendingCount = 0;
        return true;
    }
};
SceneKeeper g_scenes;

S4_HLL(StoatSpriteEngine, ADVSceneKeeper_AddADVScene) {
    Page *p = c.page(0);
    if (!p || !g_scenes.split(p->index)) { c.retBool(false); return; }
    std::vector<int32_t> out;
    serial::putStruct(*G().vm, out, c.i(0));
    g_scenes.scenes.push_back(std::move(out));
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_GetNumofADVScene) { c.retInt(g_scenes.count()); }
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_GetADVScene) {
    int n = c.i(0);
    Page *p = c.page(1);
    if (!p || !g_scenes.split(p->index) || n < 0 || size_t(n) >= g_scenes.scenes.size()) { c.retBool(false); return; }
    size_t i = 0;
    c.retBool(serial::getStruct(*G().vm, g_scenes.scenes[size_t(n)], i, c.i(1)));
}
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_Clear) { g_scenes = {}; }
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_Save) {
    std::vector<int32_t> out;
    serial::putTag(out, "ADS");
    out.push_back(0);
    out.push_back(g_scenes.count());
    for (auto &sc : g_scenes.scenes) out.insert(out.end(), sc.begin(), sc.end());
    out.insert(out.end(), g_scenes.pending.begin(), g_scenes.pending.end());
    if (Page *a = c.page(0)) a->v = std::move(out);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, ADVSceneKeeper_Load) {
    Page *a = c.page(0);
    g_scenes = {};
    size_t i = 0;
    if (!a || !serial::getTag(a->v, i, "ADS") || i + 2 > a->v.size() || a->v[i] != 0 || a->v[i + 1] < 0) {
        c.retBool(false);
        return;
    }
    g_scenes.pendingCount = a->v[i + 1];
    g_scenes.pending.assign(a->v.begin() + ptrdiff_t(i + 2), a->v.end());
    c.retBool(true);
}

// ---- AnteaterADVEngine (Test Game 56): the same backlog and scene keeper. Its AddText also names the message
// window, which the backlog doesn't need, and a page may list voices; VNDegen's pages keep one. ----
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_Clear, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_AddText, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_AddNewLine, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_AddNewPage, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_AddVoice, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_SetEnable, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_IsEnable, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_GetNumofADVLog, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_GetNumofADVLogText, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_GetADVLogText, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_Save, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVLogList_Load, StoatSpriteEngine);
S4_HLL(AnteaterADVEngine, ADVLogList_GetNumofADVLogVoice) {
    int n = c.i(0);
    c.retInt(n >= 0 && size_t(n) < g_log.size() && g_log[size_t(n)].voice ? 1 : 0);
}
S4_HLL(AnteaterADVEngine, ADVLogList_GetADVLogVoice) {
    int n = c.i(0);
    c.retInt(n >= 0 && size_t(n) < g_log.size() && c.i(1) == 0 ? g_log[size_t(n)].voice : 0);
}
S4_HLL_ALIAS(AnteaterADVEngine, ADVSceneKeeper_AddADVScene, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVSceneKeeper_GetNumofADVScene, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVSceneKeeper_GetADVScene, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVSceneKeeper_Clear, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVSceneKeeper_Save, StoatSpriteEngine);
S4_HLL_ALIAS(AnteaterADVEngine, ADVSceneKeeper_Load, StoatSpriteEngine);

S4_HLL_RESET {
    g_nextInternal = 1000000;
    g_multi.clear();
    g_multiOrder.clear();
    g_messageFrameShown = true;
    g_chars.clear();
    g_nextCharHandle = 1;
    g_log.clear();
    g_logEnabled = true;
    g_scenes = {};
    g_msgLog.clear();
    g_msgLogMax = 10000;
    g_viewer.clear();
}

}  // namespace
}  // namespace s4
