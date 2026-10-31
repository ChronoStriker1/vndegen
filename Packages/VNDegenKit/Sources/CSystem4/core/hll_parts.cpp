// PartsEngine and ChipmunkSpriteEngine: the sprite and UI library later System 4 games (Test Game 56) are built on.
// A part is a numbered component with up to three states (1 normal, 2 under the pointer, 3 pressed), each a picture,
// text or a picture built by construction steps, and a position, depth, visibility and alpha. Parts may sit inside
// a parent. Every part draws as a sprite of the runtime's own sprite list, so the frame composes as for the earlier
// games. Behaviour comes from the library's function and argument names in the game's code file and from the calls
// the game makes; docs/system4-cleanroom.md has the details.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "game.h"
#include "gfx.h"
#include "sjis.h"

namespace s4 {
namespace {

Game &G() { return game(); }

constexpr int kSpriteBase = 1000000;  // parts' sprite numbers, above any the earlier sprite engines use

struct Step {  // a construction step
    enum Kind { Create, CreatePixelOnly, Fill, FillAlpha, FillBlend, CopyCG, DrawCG } kind;
    int x = 0, y = 0, w = 0, h = 0, r = 0, g = 0, b = 0, a = 255;
    std::string cg;
    int sx = 0, sy = 0, sw = 0, sh = 0;
};

struct State {
    std::string cgName;
    std::shared_ptr<const Image> cg;
    bool hasText = false;
    std::string text;
    TextStyle font;
    int charSpace = 0, lineSpace = 0;
    std::vector<Step> steps;
    Surface built;
    bool hasBuilt = false;
    // Text drawn into a fixed area: its top left and size (0 for the text's own size).
    int areaX = 0, areaY = 0, areaW = 0, areaH = 0;
    // A looping CG: <base><number>, two digits, from loopStart for loopCount pictures of loopMs each.
    std::string loopBase;
    int loopStart = 0, loopCount = 0, loopMs = 0, loopShown = -1;
    bool empty() const { return !cg && !hasBuilt && !hasText; }
};

struct Motion {
    enum Kind { Alpha, Pos } kind;
    int number;
    float from[2], to[2];
    int begin, end;
};

struct Part {
    State states[3];
    int state = 1;  // the input state, which is also the state shown: 1 normal, 2 under the pointer, 3 pressed
    int layer = 0;
    bool clickable = false;
    bool pixelDecide = false;  // clicks count only where the picture isn't transparent
    bool lockInput = false;
    float x = 0, y = 0;
    int z = 0;
    bool show = true;
    int alpha = 255;
    int originMode = 1;
    int parent = 0;
    bool messageWindowLink = false;
    int delegate = -1;
    int type = 0;
    bool dirty = true;  // the picture needs drawing again
};

// What the player did to a part, queued for the game's message loop. Types as the game's dispatcher numbers them.
struct Message {
    enum Type { MouseEnter = 1, MouseMove = 2, MouseLeave = 3, MouseClick = 5 };
    int type, number, delegate;
    std::vector<int> vars;  // all ints: the pointer's x and y, then the key for a click
};

// Activities: named groups of parts the game's UI code keeps here, with the keys and parts that close them.
struct Activity {
    std::vector<std::pair<std::string, int>> parts;  // name and number
    std::set<std::string> closeParts;
    std::set<int> endKeys;
    std::map<std::tuple<std::string, int, std::string>, std::string> funcs;  // (parts name, type, key) to function
};

// Intent data: a type and destinations under two names, which the game's UI code sets and reads back.
struct Intent {
    int type = 0;
    std::vector<std::string> destinations;
};

std::map<int, Part> g_parts;
std::vector<Motion> g_motions;
int g_motionTime = 0;
bool g_motionOn = false;
TextStyle g_textDefault;  // ChipmunkSpriteEngine's SP_SetTextSprite* defaults for new text
std::deque<Message> g_messages;
int g_hover = 0, g_pressed = 0;  // parts under the pointer and pressed; 0 for none
bool g_buttonWas = false;
int g_activeLayer = 0;
constexpr int kOverlayLayer = 1;  // the system overlay layer, drawn over the game's
std::map<std::string, Activity> g_activities;
std::map<std::pair<std::string, std::string>, Intent> g_intents;
std::map<int, std::map<int, int>> g_links;  // PartsFunc's per-part values (scroll links and the like), by ID
std::set<std::string> g_loggedOnce;
std::vector<uint8_t> g_transFrom;  // the screen a ChipmunkSpriteEngine transition starts from

void logOnce(const std::string &what) {
    if (g_loggedOnce.insert(what).second) G().log("PartsEngine: " + what);
}

S4_HLL_RESET {
    for (auto &[n, p] : g_parts) G().sprites.remove(kSpriteBase + n);
    g_parts.clear();
    g_motions.clear();
    g_motionTime = 0;
    g_motionOn = false;
    g_textDefault = TextStyle{};
    g_transFrom.clear();
    g_messages.clear();
    g_hover = g_pressed = 0;
    g_buttonWas = false;
    g_activeLayer = 0;
    g_activities.clear();
    g_links.clear();
    g_intents.clear();
    g_loggedOnce.clear();
}

Part *find(int n) {
    auto it = g_parts.find(n);
    return it == g_parts.end() ? nullptr : &it->second;
}

Part &get(int n) {
    auto [it, added] = g_parts.try_emplace(n);
    if (added) {
        for (auto &s : it->second.states) s.font = g_textDefault;
        it->second.layer = g_activeLayer;
    }
    return it->second;
}

State *stateOf(HllCall &c, int numberArg, int stateArg) {
    int s = c.i(stateArg);
    if (s < 1 || s > 3) return nullptr;
    return &get(c.i(numberArg)).states[s - 1];
}

// A state's text in lines, which break at "\n", and the size it draws at.
std::vector<std::string> textLines(const State &s) {
    std::vector<std::string> lines(1);
    for (char ch : s.text) {
        if (ch == '\n') lines.emplace_back();
        else lines.back() += ch;
    }
    return lines;
}
int lineHeight(const State &s) { return s.font.size + s.lineSpace; }
void textSize(const State &s, int &w, int &h) {
    auto lines = textLines(s);
    w = 1;
    for (const auto &l : lines) w = std::max(w, textWidth(l, s.font, s.charSpace));
    h = int(lines.size()) * lineHeight(s) + 4;
    if (s.areaW > 0) w = s.areaW;
    if (s.areaH > 0) h = s.areaH;
}

// The size a state shows at: its built picture, its CG, else its text.
void stateSize(const State &s, int &w, int &h) {
    w = h = 0;
    if (s.hasBuilt) { w = s.built.w; h = s.built.h; }
    else if (s.cg) { w = s.cg->w; h = s.cg->h; }
    else if (s.hasText && !s.text.empty()) textSize(s, w, h);
}

// The picture a state shows: what its construction steps built, its CG, or its text.
void drawState(const State &s, Sprite &sp) {
    sp.cg.reset();
    sp.hasOwn = false;
    if (s.hasBuilt) {
        sp.own = s.built;
        sp.hasOwn = true;
    } else if (s.cg) {
        sp.cg = s.cg;
    }
    if (s.hasText && !s.text.empty()) {
        // Each line is the font's size plus the line space below the one before.
        auto lines = textLines(s);
        int w, h, lineH = lineHeight(s);
        textSize(s, w, h);
        Surface t;
        t.create(w, h, true);
        for (size_t k = 0; k < lines.size(); k++) drawText(t, 0, int(k) * lineH, lines[k], s.font, s.charSpace);
        if (sp.hasOwn || sp.cg) {
            Surface &dst = sp.writable();
            blit(dst.px.data(), dst.w, dst.h, s.areaX, s.areaY, t.px.data(), t.w, t.h, true, 255, 0);
        } else {
            sp.own = std::move(t);
            sp.hasOwn = true;
        }
    }
}

// The position, depth and alpha a part shows at, through its parents.
void placement(int n, float &x, float &y, int &z, int &alpha, bool &show, int depth = 0) {
    Part *p = find(n);
    if (!p || depth > 32) return;
    x += p->x;
    y += p->y;
    z += p->z + (depth == 0 ? p->layer * 1000000 : 0);
    alpha = alpha * p->alpha / 255;
    show = show && p->show;
    if (p->parent > 0 && p->parent != n) placement(p->parent, x, y, z, alpha, show, depth + 1);
}

// Draws every part into its sprite: the picture when it changed, and the placement each time.
void syncAll() {
    uint32_t now = G().now();
    for (auto &[n, p] : g_parts) {
        State &ls = p.states[std::clamp(p.state, 1, 3) - 1];
        if (ls.loopCount > 0 && ls.loopMs > 0) {
            int k = ls.loopStart + int(now / uint32_t(ls.loopMs) % uint32_t(ls.loopCount));
            if (k != ls.loopShown) {
                char num[16];
                std::snprintf(num, sizeof num, "%02d", k);
                ls.cg = G().cgCachedNamed(ls.loopBase + num);
                ls.loopShown = k;
                p.dirty = true;
            }
        }
        Sprite &sp = G().sprites.get(kSpriteBase + n);
        sp.used = true;
        if (p.dirty) {
            // A state with nothing in it shows the normal one.
            const State &st = p.states[std::clamp(p.state, 1, 3) - 1];
            drawState(st.empty() ? p.states[0] : st, sp);
            p.dirty = false;
        }
        float x = 0, y = 0;
        int z = 0, alpha = 255;
        bool show = true;
        placement(n, x, y, z, alpha, show);
        // Origin modes in reading order over a 3x3 grid; 1 is the top left.
        int m = std::clamp(p.originMode, 1, 9) - 1;
        x -= float(sp.width()) * float(m % 3) / 2.0f;
        y -= float(sp.height()) * float(m / 3) / 2.0f;
        sp.x = int(std::lround(x));
        sp.y = int(std::lround(y));
        sp.z = z;
        sp.blendRate = alpha;
        sp.show = show && (sp.hasOwn || sp.cg);
    }
}

void applyMotions(int t) {
    for (const Motion &m : g_motions) {
        Part *p = find(m.number);
        if (!p) continue;
        float k = m.end <= m.begin ? 1.0f : std::clamp(float(t - m.begin) / float(m.end - m.begin), 0.0f, 1.0f);
        if (t < m.begin) continue;
        if (m.kind == Motion::Alpha) p->alpha = int(std::lround(m.from[0] + (m.to[0] - m.from[0]) * k));
        else {
            p->x = m.from[0] + (m.to[0] - m.from[0]) * k;
            p->y = m.from[1] + (m.to[1] - m.from[1]) * k;
        }
    }
}

// The part under the pointer that takes clicks: the frontmost shown clickable part whose rectangle holds it.
int hitTest(int mx, int my) {
    int best = 0, bestZ = 0;
    for (auto &[n, p] : g_parts) {
        if (!p.clickable || p.lockInput) continue;
        Sprite *sp = G().sprites.find(kSpriteBase + n);
        if (!sp || !sp->show || sp->blendRate == 0) continue;
        if (mx < sp->x || my < sp->y || mx >= sp->x + sp->width() || my >= sp->y + sp->height()) continue;
        if (p.pixelDecide && sp->hasAlpha() && sp->pixels()[(size_t(my - sp->y) * sp->width() + (mx - sp->x)) * 4 + 3] == 0)
            continue;
        if (!best || sp->z >= bestZ) { best = n; bestZ = sp->z; }
    }
    return best;
}

void post(int type, int number, std::vector<int> vars) {
    Part *p = find(number);
    g_messages.push_back({type, number, p ? p->delegate : -1, std::move(vars)});
}

void setInputState(int number, int state) {
    Part *p = find(number);
    if (p && p->state != state) { p->state = state; p->dirty = true; }
}

// Turns the pointer into states and messages: entering and leaving a part, pressing it, and a click when the button
// comes up over the part it went down on.
void updateInput() {
    int mx = G().mouseX(), my = G().mouseY();
    bool down = G().keyDown(1);
    int hit = hitTest(mx, my);
    if (hit != g_hover) {
        if (g_hover && find(g_hover)) {
            post(Message::MouseLeave, g_hover, {mx, my});
            setInputState(g_hover, 1);
        }
        if (hit) post(Message::MouseEnter, hit, {mx, my});
        g_hover = hit;
    }
    if (down && !g_buttonWas) g_pressed = hit;
    if (!down && g_buttonWas) {
        if (g_pressed && g_pressed == hit) post(Message::MouseClick, hit, {mx, my, 1});
        g_pressed = 0;
    }
    g_buttonWas = down;
    if (g_hover) setInputState(g_hover, g_pressed == g_hover ? 3 : 2);
}

int motionEnd() {
    int e = 0;
    for (const Motion &m : g_motions) e = std::max(e, m.end);
    return e;
}

}  // namespace

// ---- parts ----
S4_HLL(PartsEngine, GetFreeNumber) {
    int n = 1000000;
    while (g_parts.count(n)) n++;
    c.retInt(n);
}
S4_HLL(PartsEngine, IsExist) { c.retBool(find(c.i(0)) != nullptr); }
S4_HLL(PartsEngine, Release) {
    int n = c.i(0);
    g_parts.erase(n);
    G().sprites.remove(kSpriteBase + n);
    std::erase_if(g_motions, [n](const Motion &m) { return m.number == n; });
}
S4_HLL(PartsEngine, ReleaseAll) {
    for (auto &[n, p] : g_parts) G().sprites.remove(kSpriteBase + n);
    g_parts.clear();
    g_motions.clear();
}
S4_HLL(PartsEngine, SetComponentType) { get(c.i(0)).type = c.i(1); }
S4_HLL(PartsEngine, GetComponentType) { Part *p = find(c.i(0)); c.retInt(p ? p->type : 0); }
S4_HLL(PartsEngine, SetComponentPos) { Part &p = get(c.i(0)); p.x = c.f(1); p.y = c.f(2); }
S4_HLL(PartsEngine, SetComponentPosZ) { get(c.i(0)).z = c.i(1); }
S4_HLL(PartsEngine, GetComponentPosX) { Part *p = find(c.i(0)); c.retFloat(p ? p->x : 0); }
S4_HLL(PartsEngine, GetComponentPosY) { Part *p = find(c.i(0)); c.retFloat(p ? p->y : 0); }
S4_HLL(PartsEngine, GetComponentPosZ) { Part *p = find(c.i(0)); c.retInt(p ? p->z : 0); }
S4_HLL(PartsEngine, SetComponentOriginPosMode) { get(c.i(0)).originMode = c.i(1); }
S4_HLL(PartsEngine, GetComponentOriginPosMode) { Part *p = find(c.i(0)); c.retInt(p ? p->originMode : 1); }
S4_HLL(PartsEngine, SetComponentShow) { get(c.i(0)).show = c.b(1); }
S4_HLL(PartsEngine, IsComponentShow) { Part *p = find(c.i(0)); c.retBool(p && p->show); }
S4_HLL(PartsEngine, SetComponentAlpha) { get(c.i(0)).alpha = std::clamp(c.i(1), 0, 255); }
S4_HLL(PartsEngine, GetComponentAlpha) { Part *p = find(c.i(0)); c.retInt(p ? p->alpha : 255); }
S4_HLL(PartsEngine, SetComponentMessageWindowShowLink) { get(c.i(0)).messageWindowLink = c.b(1); }
S4_HLL(PartsEngine, IsComponentMessageWindowShowLink) { Part *p = find(c.i(0)); c.retBool(p && p->messageWindowLink); }
S4_HLL(PartsEngine, Parts_SetParentPartsNumber) { get(c.i(0)).parent = c.i(1); }
S4_HLL(PartsEngine, Parts_GetParentPartsNumber) { Part *p = find(c.i(0)); c.retInt(p ? p->parent : 0); }
S4_HLL(PartsEngine, SetDelegateIndex) { get(c.i(0)).delegate = c.i(1); }
S4_HLL(PartsEngine, GetDelegateIndex) { Part *p = find(c.i(0)); c.retInt(p ? p->delegate : -1); }
S4_HLL(PartsEngine, Parts_SetInputState) { get(c.i(0)); setInputState(c.i(0), std::clamp(c.i(1), 1, 3)); }
S4_HLL(PartsEngine, Parts_GetInputState) { Part *p = find(c.i(0)); c.retInt(p ? p->state : 1); }
S4_HLL(PartsEngine, Parts_SetClickable) { get(c.i(0)).clickable = c.b(1); }
S4_HLL(PartsEngine, Parts_GetPartsClickable) { Part *p = find(c.i(0)); c.retBool(p && p->clickable); }
S4_HLL(PartsEngine, SetComponentDrawFilter) {}
S4_HLL(PartsEngine, GetComponentDrawFilter) { c.retInt(0); }
S4_HLL(PartsEngine, SetComponentSpeedupRateByMessageSkip) {}
S4_HLL(PartsEngine, GetComponentSpeedupRateByMessageSkip) { c.retInt(0); }
// A state's size and top left corner, before the part's position.
S4_HLL(PartsEngine, Parts_GetPartsWidth) {
    int w = 0, h = 0;
    if (State *s = stateOf(c, 0, 1)) stateSize(*s, w, h);
    c.retInt(w);
}
S4_HLL(PartsEngine, Parts_GetPartsHeight) {
    int w = 0, h = 0;
    if (State *s = stateOf(c, 0, 1)) stateSize(*s, w, h);
    c.retInt(h);
}
S4_HLL(PartsEngine, Parts_GetPartsUpperLeftPosX) {
    Part *p = find(c.i(0));
    Sprite *sp = G().sprites.find(kSpriteBase + c.i(0));
    int m = p ? std::clamp(p->originMode, 1, 9) - 1 : 0;
    c.retFloat(p ? p->x - float(sp ? sp->width() : 0) * float(m % 3) / 2.0f : 0);
}
S4_HLL(PartsEngine, Parts_GetPartsUpperLeftPosY) {
    Part *p = find(c.i(0));
    Sprite *sp = G().sprites.find(kSpriteBase + c.i(0));
    int m = p ? std::clamp(p->originMode, 1, 9) - 1 : 0;
    c.retFloat(p ? p->y - float(sp ? sp->height() : 0) * float(m / 3) / 2.0f : 0);
}
S4_HLL(PartsEngine, GetComponentWidth) {
    Sprite *sp = G().sprites.find(kSpriteBase + c.i(0));
    c.retInt(sp ? sp->width() : 0);
}
S4_HLL(PartsEngine, GetComponentHeight) {
    Sprite *sp = G().sprites.find(kSpriteBase + c.i(0));
    c.retInt(sp ? sp->height() : 0);
}

// ---- what a state shows ----
S4_HLL(PartsEngine, Parts_SetPartsCG) {
    State *s = stateOf(c, 0, 3);
    if (!s) { c.retBool(false); return; }
    s->cgName = c.s(1);
    s->cg = s->cgName.empty() ? nullptr : G().cgCachedNamed(s->cgName);
    s->hasBuilt = false;
    get(c.i(0)).dirty = true;
    c.retBool(s->cgName.empty() || s->cg != nullptr);
}
S4_HLL(PartsEngine, Parts_GetPartsCGName) {
    State *s = stateOf(c, 0, 2);
    c.refS(1) = s ? s->cgName : std::string();
}
S4_HLL(PartsEngine, Parts_SetText) {
    State *s = stateOf(c, 0, 2);
    if (!s) { c.retBool(false); return; }
    s->text = c.s(1);
    if (std::getenv("S4_PARTSTEXT")) G().log("parts text " + std::to_string(c.i(0)) + ": " + sjis::toUtf8(s->text));
    s->hasText = true;
    get(c.i(0)).dirty = true;
    c.retBool(true);
}
S4_HLL(PartsEngine, Parts_SetPartsFontSize) {
    State *s = stateOf(c, 0, 2);
    if (s) { s->font.size = c.i(1); get(c.i(0)).dirty = true; }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_SetPartsFontColor) {
    State *s = stateOf(c, 0, 4);
    if (s) { s->font.r = c.i(1); s->font.g = c.i(2); s->font.b = c.i(3); get(c.i(0)).dirty = true; }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_SetFont) {
    State *s = stateOf(c, 0, 11);
    if (s) {
        s->font.face = c.i(1) == 1 ? 1 : 0;
        s->font.size = c.i(2);
        s->font.r = c.i(3); s->font.g = c.i(4); s->font.b = c.i(5);
        s->font.boldWeight = c.f(6);
        s->font.er = c.i(7); s->font.eg = c.i(8); s->font.eb = c.i(9);
        s->font.edgeWeight = c.f(10);
        get(c.i(0)).dirty = true;
    }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_SetPartsFontType) {
    State *s = stateOf(c, 0, 2);
    if (s) { s->font.face = c.i(1) == 1 ? 1 : 0; get(c.i(0)).dirty = true; }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_SetPartsFontBoldWeight) {
    State *s = stateOf(c, 0, 2);
    if (s) { s->font.boldWeight = c.f(1); get(c.i(0)).dirty = true; }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_AddPartsText) {
    State *s = stateOf(c, 0, 2);
    if (!s) { c.retBool(false); return; }
    s->text += c.s(1);
    if (std::getenv("S4_PARTSTEXT")) G().log("parts text+ " + std::to_string(c.i(0)) + ": " + sjis::toUtf8(c.s(1)));
    s->hasText = true;
    get(c.i(0)).dirty = true;
    c.retBool(true);
}
S4_HLL(PartsEngine, Parts_SetPartsTextSurfaceArea) {
    State *s = stateOf(c, 0, 5);
    if (s) { s->areaX = c.i(1); s->areaY = c.i(2); s->areaW = c.i(3); s->areaH = c.i(4); get(c.i(0)).dirty = true; }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_SetPartsFontEdgeWeight) {
    State *s = stateOf(c, 0, 2);
    if (s) { s->font.edgeWeight = c.f(1); get(c.i(0)).dirty = true; }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_SetPartsFontEdgeColor) {
    State *s = stateOf(c, 0, 4);
    if (s) { s->font.er = c.i(1); s->font.eg = c.i(2); s->font.eb = c.i(3); get(c.i(0)).dirty = true; }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_SetLoopCG) {
    State *s = stateOf(c, 0, 5);
    if (s) {
        s->loopBase = c.s(1);
        s->loopStart = c.i(2);
        s->loopCount = c.i(3);
        s->loopMs = c.i(4);
        s->loopShown = -1;
        s->hasBuilt = false;
        get(c.i(0)).dirty = true;
    }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_IsCursorIn) {
    Sprite *sp = G().sprites.find(kSpriteBase + c.i(0));
    int x = c.i(1), y = c.i(2);
    c.retBool(sp && sp->show && x >= sp->x && y >= sp->y && x < sp->x + sp->width() && y < sp->y + sp->height());
}
S4_HLL(PartsEngine, Parts_SetPartsPixelDecide) { get(c.i(0)).pixelDecide = c.b(1); }
// Multiplying a part's colours isn't drawn yet.
S4_HLL(PartsEngine, SetComponentMulColor) { logOnce("colour multiply is not drawn"); }
S4_HLL(PartsEngine, Parts_SetTextCharSpace) {
    State *s = stateOf(c, 0, 2);
    if (s) { s->charSpace = c.i(1); get(c.i(0)).dirty = true; }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_SetTextLineSpace) {
    State *s = stateOf(c, 0, 2);
    if (s) s->lineSpace = c.i(1);
    c.retBool(s != nullptr);
}

// ---- construction: a picture built by steps ----
S4_HLL(PartsEngine, Parts_ClearPartsConstructionProcess) {
    State *s = stateOf(c, 0, 1);
    if (s) s->steps.clear();
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_AddCreateToPartsConstructionProcess) {
    State *s = stateOf(c, 0, 3);
    if (s) s->steps.push_back({Step::Create, 0, 0, c.i(1), c.i(2)});
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_AddCreatePixelOnlyToPartsConstructionProcess) {
    State *s = stateOf(c, 0, 3);
    if (s) s->steps.push_back({Step::CreatePixelOnly, 0, 0, c.i(1), c.i(2)});
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_AddFillToPartsConstructionProcess) {
    State *s = stateOf(c, 0, 8);
    if (s) s->steps.push_back({Step::Fill, c.i(1), c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7)});
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_AddFillWithAlphaToPartsConstructionProcess) {
    State *s = stateOf(c, 0, 9);
    if (s) s->steps.push_back({Step::FillAlpha, c.i(1), c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7), c.i(8)});
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_AddFillAlphaColorToPartsConstructionProcess) {
    State *s = stateOf(c, 0, 9);
    if (s) s->steps.push_back({Step::FillBlend, c.i(1), c.i(2), c.i(3), c.i(4), c.i(5), c.i(6), c.i(7), c.i(8)});
    c.retBool(s != nullptr);
}
// A cut of a CG, scaled from its source rectangle into the destination one: copied over the pixels, or blended.
static void addCut(HllCall &c, Step::Kind kind) {
    State *s = stateOf(c, 0, 11);
    if (s) {
        Step st{kind, c.i(2), c.i(3), c.i(4), c.i(5)};
        st.cg = c.s(1);
        st.sx = c.i(6); st.sy = c.i(7); st.sw = c.i(8); st.sh = c.i(9);
        s->steps.push_back(std::move(st));
    }
    c.retBool(s != nullptr);
}
S4_HLL(PartsEngine, Parts_AddCopyCutCGToPartsConstructionProcess) { addCut(c, Step::CopyCG); }
S4_HLL(PartsEngine, Parts_AddDrawCutCGToPartsConstructionProcess) { addCut(c, Step::DrawCG); }
S4_HLL(PartsEngine, Parts_BuildPartsConstructionProcess) {
    State *s = stateOf(c, 0, 1);
    if (!s) { c.retBool(false); return; }
    Surface out;
    for (const Step &st : s->steps) {
        switch (st.kind) {
        case Step::Create: out.create(std::max(0, st.w), std::max(0, st.h), true); out.fill(0, 0, out.w, out.h, 0, 0, 0, 0); break;
        case Step::CreatePixelOnly: out.create(std::max(0, st.w), std::max(0, st.h), false); break;
        case Step::Fill: out.fill(st.x, st.y, st.w, st.h, st.r, st.g, st.b, out.hasAlpha ? 255 : -1); break;
        case Step::FillAlpha: out.fill(st.x, st.y, st.w, st.h, st.r, st.g, st.b, st.a); break;
        case Step::FillBlend: {
            Surface f;
            f.create(std::max(0, st.w), std::max(0, st.h), true);
            f.fill(0, 0, f.w, f.h, st.r, st.g, st.b, std::clamp(st.a, 0, 255));
            if (out.w > 0) blit(out.px.data(), out.w, out.h, st.x, st.y, f.px.data(), f.w, f.h, true, 255, 0);
            break;
        }
        case Step::CopyCG:
        case Step::DrawCG: {
            auto img = G().cgCachedNamed(st.cg);
            if (!img || out.w <= 0 || st.w <= 0 || st.h <= 0 || st.sw <= 0 || st.sh <= 0) {
                logOnce("construction step: no CG " + sjis::toUtf8(st.cg));
                break;
            }
            Surface cut;
            cut.create(st.w, st.h, true);
            for (int y = 0; y < st.h; y++)
                for (int x = 0; x < st.w; x++) {
                    int sx = st.sx + x * st.sw / st.w, sy = st.sy + y * st.sh / st.h;
                    if (sx < 0 || sy < 0 || sx >= img->w || sy >= img->h) continue;
                    std::memcpy(cut.px.data() + (size_t(y) * st.w + x) * 4, img->rgba.data() + (size_t(sy) * img->w + sx) * 4, 4);
                }
            if (st.kind == Step::DrawCG) {
                blit(out.px.data(), out.w, out.h, st.x, st.y, cut.px.data(), cut.w, cut.h, true, 255, 0);
                break;
            }
            for (int y = 0; y < st.h; y++) {
                int dy = st.y + y;
                if (dy < 0 || dy >= out.h) continue;
                for (int x = 0; x < st.w; x++) {
                    int dx = st.x + x;
                    if (dx >= 0 && dx < out.w)
                        std::memcpy(out.px.data() + (size_t(dy) * out.w + dx) * 4, cut.px.data() + (size_t(y) * st.w + x) * 4, 4);
                }
            }
            break;
        }
        }
    }
    s->built = std::move(out);
    s->hasBuilt = s->built.w > 0 && s->built.h > 0;
    get(c.i(0)).dirty = true;
    c.retBool(true);
}

// ---- motions: values over a shared clock the game runs with UpdateMotionTime ----
S4_HLL(PartsEngine, AddComponentMotionAlpha) {
    get(c.i(0));
    g_motions.push_back({Motion::Alpha, c.i(0), {float(c.i(1)), 0}, {float(c.i(2)), 0}, c.i(3), c.i(4)});
}
S4_HLL(PartsEngine, AddComponentMotionPos) {
    get(c.i(0));
    g_motions.push_back({Motion::Pos, c.i(0), {c.f(1), c.f(2)}, {c.f(3), c.f(4)}, c.i(5), c.i(6)});
}
S4_HLL(PartsEngine, BeginMotion) {
    g_motionOn = true;
    g_motionTime = 0;
    applyMotions(0);
}
S4_HLL(PartsEngine, UpdateMotionTime) {
    // The time is what passed since the last call, in milliseconds.
    g_motionTime = c.b(1) ? motionEnd() : g_motionTime + c.i(0);
    applyMotions(g_motionTime);
}
S4_HLL(PartsEngine, IsMotion) { c.retBool(g_motionOn && g_motionTime < motionEnd()); }
S4_HLL(PartsEngine, EndMotion) {
    applyMotions(motionEnd());
    g_motions.clear();
    g_motionOn = false;
}

// ---- each frame ----
S4_HLL(PartsEngine, UpdateComponent) {
    syncAll();
    updateInput();
}
// ---- messages: the game reads the front one, then pops it, and releases the rest at the end of its loop ----
S4_HLL(PartsEngine, GetMessageType) { c.retInt(g_messages.empty() ? 0 : g_messages.front().type); }
S4_HLL(PartsEngine, PopMessage) { if (!g_messages.empty()) g_messages.pop_front(); }
S4_HLL(PartsEngine, ReleaseMessage) { g_messages.clear(); }
S4_HLL(PartsEngine, GetMessagePartsNumber) { c.retInt(g_messages.empty() ? 0 : g_messages.front().number); }
S4_HLL(PartsEngine, GetMessageDelegateIndex) { c.retInt(g_messages.empty() ? -1 : g_messages.front().delegate); }
S4_HLL(PartsEngine, GetMessageVariableCount) { c.retInt(g_messages.empty() ? 0 : int(g_messages.front().vars.size())); }
S4_HLL(PartsEngine, GetMessageVariableType) { c.retInt(1); }  // every variable made here is an int
static int messageVar(int k) {
    if (g_messages.empty() || k < 0 || size_t(k) >= g_messages.front().vars.size()) return 0;
    return g_messages.front().vars[size_t(k)];
}
S4_HLL(PartsEngine, GetMessageVariableInt) { c.retInt(messageVar(c.i(0))); }
S4_HLL(PartsEngine, GetMessageVariableFloat) { c.retFloat(float(messageVar(c.i(0)))); }
S4_HLL(PartsEngine, GetMessageVariableBool) { c.retBool(messageVar(c.i(0)) != 0); }
S4_HLL(PartsEngine, GetMessageVariableString) { c.refS(1).clear(); }
S4_HLL(PartsEngine, GetClickNumber) { c.retInt(0); }
S4_HLL(PartsEngine, SetFocus) {}
S4_HLL(PartsEngine, IsFocus) { c.retBool(false); }
S4_HLL(PartsEngine, SetClickMissSoundNumber) {}
S4_HLL(PartsEngine, GetClickMissSoundNumber) { c.retInt(-1); }
S4_HLL(PartsEngine, SetButtonClickSoundNumber) {}
S4_HLL(PartsEngine, GetButtonClickSoundNumber) { c.retInt(-1); }

// ---- Flat animations (Flash-like, from the Flat archives). Not played yet: each reports itself finished. ----
S4_HLL(PartsEngine, Parts_SetPartsFlat) { logOnce("Flat animations are not played (" + sjis::toUtf8(c.s(1)) + ")"); c.retBool(true); }
S4_HLL(PartsEngine, Parts_SetPartsFlatAndStop) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_IsPartsFlatEnd) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_StopPartsFlat) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_StartPartsFlat) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_GetPartsFlatCurrentFrameNumber) { c.retInt(0); }
S4_HLL(PartsEngine, Parts_GetPartsFlatEndFrame) { c.retInt(0); }
S4_HLL(PartsEngine, Parts_StepPartsFlatFinalFrame) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_BackPartsFlatBeginFrame) { c.retBool(true); }

// ---- numerals: a number drawn in digits. Not drawn yet. ----
S4_HLL(PartsEngine, Parts_SetNumeralNumber) { logOnce("numerals are not drawn"); c.retBool(true); }
S4_HLL(PartsEngine, Parts_SetNumeralCG) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_SetNumeralFont) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_SetNumeralShowComma) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_SetNumeralSpace) { c.retBool(true); }
S4_HLL(PartsEngine, Parts_SetNumeralLength) { c.retBool(true); }
S4_HLL(PartsEngine, BeginInput) {}
S4_HLL(PartsEngine, EndInput) {}

// ---- PartsFunc: many small calls behind one, by number. The inputs fill the int, float and string arrays in
// order, a reference argument is read back from its slot, and the result takes the last slot of its type's array,
// which the game fills with a placeholder first. The numbers come from the game's wrapper functions. ----
namespace {

struct FuncArgs {
    HllCall &c;
    Page *ints, *floats, *strs;
    int i(size_t k) const { return ints && k < ints->v.size() ? ints->v[k] : 0; }
    std::string s(size_t k) const {
        return strs && k < strs->v.size() && c.vm.valid(strs->v[k]) ? c.vm.str(strs->v[k]) : std::string();
    }
    void setI(size_t k, int v) { if (ints && k < ints->v.size()) ints->v[k] = v; }
    void setS(size_t k, const std::string &v) {
        if (!strs || k >= strs->v.size()) return;
        if (c.vm.valid(strs->v[k])) c.vm.str(strs->v[k]) = v;
        else strs->v[k] = c.vm.newString(v);
    }
    void retInt(int v) { if (ints && !ints->v.empty()) ints->v.back() = v; }
    void retBool(bool v) { retInt(v ? 1 : 0); }
    void retFloat(float f) {
        if (!floats || floats->v.empty()) return;
        int32_t b;
        std::memcpy(&b, &f, 4);
        floats->v.back() = b;
    }
    void retString(const std::string &v) { if (strs && !strs->v.empty()) setS(strs->v.size() - 1, v); }
};

Activity *activity(const std::string &name) {
    auto it = g_activities.find(name);
    return it == g_activities.end() ? nullptr : &it->second;
}

void absolute(int n, float &x, float &y, int &z) {
    int alpha = 255;
    bool show = true;
    placement(n, x, y, z, alpha, show);
}

void partsFunc(int id, FuncArgs a) {
    switch (id) {
    case 0: g_activeLayer = a.i(0); break;  // SetActiveLayer
    case 1: a.retInt(g_activeLayer); break;
    case 2: a.retInt(kOverlayLayer); break;
    case 3: break;  // PlayMotion: the motions run from UpdateMotionTime
    case 4: g_links[id][a.i(0)] = a.i(1); break;  // SetWantSave
    case 5: a.retBool(g_links[4][a.i(0)] != 0); break;
    case 7: a.retBool(activity(a.s(0)) != nullptr); break;
    case 8: a.retBool(g_activities.try_emplace(a.s(0)).second); break;
    case 9: {  // AddActivityParts(activity, name, number)
        Activity *ac = activity(a.s(0));
        if (ac) ac->parts.emplace_back(a.s(1), a.i(0));
        a.retBool(ac != nullptr);
        break;
    }
    case 10: {
        Activity *ac = activity(a.s(0));
        std::string name = a.s(1);
        if (ac) std::erase_if(ac->parts, [&](const auto &p) { return p.first == name; });
        a.retBool(ac != nullptr);
        break;
    }
    case 11: { Activity *ac = activity(a.s(0)); if (ac) ac->parts.clear(); a.retBool(ac != nullptr); break; }
    case 12: { Activity *ac = activity(a.s(0)); a.retInt(ac ? int(ac->parts.size()) : 0); break; }
    case 13: {  // GetActivityParts(index, activity, &name, &number)
        Activity *ac = activity(a.s(0));
        int k = a.i(0);
        bool ok = ac && k >= 0 && size_t(k) < ac->parts.size();
        if (ok) { a.setS(1, ac->parts[size_t(k)].first); a.setI(1, ac->parts[size_t(k)].second); }
        a.retBool(ok);
        break;
    }
    case 14: case 16: {  // by name: whether it is there, and its number
        Activity *ac = activity(a.s(0));
        std::string name = a.s(1);
        int number = -1;
        if (ac)
            for (const auto &p : ac->parts)
                if (p.first == name) { number = p.second; break; }
        if (id == 14) a.retBool(number != -1); else a.retInt(number);
        break;
    }
    case 15: case 17: {  // by number: whether it is there, and its name
        Activity *ac = activity(a.s(0));
        const std::string *name = nullptr;
        if (ac)
            for (const auto &p : ac->parts)
                if (p.second == a.i(0)) { name = &p.first; break; }
        if (id == 15) a.retBool(name != nullptr); else a.retString(name ? *name : std::string());
        break;
    }
    case 18: case 19: logOnce("activity files are not read or written"); a.retBool(false); break;
    case 20: a.retBool(false); break;
    case 21: g_intents[{a.s(0), a.s(1)}].type = a.i(0); break;  // SetIntentData(a, b, data, type)
    case 22: g_intents[{a.s(0), a.s(1)}].destinations.push_back(a.s(2)); break;
    case 23: a.retBool(g_intents.count({a.s(0), a.s(1)}) != 0); break;
    case 24: { auto it = g_intents.find({a.s(0), a.s(1)}); a.retInt(it == g_intents.end() ? 0 : int(it->second.destinations.size())); break; }
    case 26: { auto it = g_intents.find({a.s(0), a.s(1)}); a.retInt(it == g_intents.end() ? 0 : it->second.type); break; }
    case 27: { Activity *ac = activity(a.s(0)); if (ac) ac->funcs[{a.s(1), a.i(0), a.s(2)}] = a.s(3); break; }
    case 28: {
        Activity *ac = activity(a.s(0));
        std::string f;
        if (ac)
            if (auto it = ac->funcs.find({a.s(1), a.i(0), a.s(2)}); it != ac->funcs.end()) f = it->second;
        a.retString(f);
        break;
    }
    case 29: case 30: {  // the functions of one parts name and type: their count, and one by index
        Activity *ac = activity(a.s(0));
        std::vector<std::pair<std::string, std::string>> list;
        if (ac)
            for (const auto &[k, f] : ac->funcs)
                if (std::get<0>(k) == a.s(1) && std::get<1>(k) == a.i(id == 29 ? 0 : 1)) list.emplace_back(std::get<2>(k), f);
        if (id == 29) { a.retInt(int(list.size())); break; }
        if (size_t k = size_t(a.i(0)); k < list.size()) { a.setS(2, list[k].first); a.setS(3, list[k].second); }
        break;
    }
    case 31: { Activity *ac = activity(a.s(0)); if (ac) ac->closeParts.insert(a.s(1)); break; }
    case 32: { Activity *ac = activity(a.s(0)); if (ac) ac->closeParts.erase(a.s(1)); break; }
    case 33: { Activity *ac = activity(a.s(0)); if (ac) ac->closeParts.clear(); break; }
    case 34: { Activity *ac = activity(a.s(0)); a.retBool(ac && ac->closeParts.count(a.s(1))); break; }
    case 35: { Activity *ac = activity(a.s(0)); if (ac) ac->endKeys.insert(a.i(0)); break; }
    case 36: { Activity *ac = activity(a.s(0)); if (ac) ac->endKeys.erase(a.i(0)); break; }
    case 37: { Activity *ac = activity(a.s(0)); a.retBool(ac && ac->endKeys.count(a.i(0))); break; }
    case 40: case 41: case 42: {
        float x = 0, y = 0;
        int z = 0;
        absolute(a.i(0), x, y, z);
        if (id == 42) a.retInt(z); else a.retFloat(id == 40 ? x : y);
        break;
    }
    case 43: {  // the frontmost depth among the part and the parts inside it
        int best = 0;
        for (auto &[n, p] : g_parts) {
            int m = n;
            for (int d = 0; d < 32 && m > 0 && m != a.i(0); d++) m = find(m) ? find(m)->parent : 0;
            if (m != a.i(0)) continue;
            float x = 0, y = 0;
            int z = 0;
            absolute(n, x, y, z);
            best = std::max(best, z);
        }
        a.retInt(best);
        break;
    }
    case 45: get(a.i(0)).lockInput = a.i(1) != 0; break;
    case 46: { Part *p = find(a.i(0)); a.retBool(p && p->lockInput); break; }
    case 47: case 48: case 51: case 53: g_links[id][a.i(0)] = a.i(1); break;
    case 49: case 50: case 52: case 54: {  // the getters of 47, 48, 51 and 53
        auto &m = g_links[id == 49 ? 47 : id == 50 ? 48 : id == 52 ? 51 : 53];
        auto it = m.find(a.i(0));
        a.retInt(it == m.end() ? 0 : it->second);
        break;
    }
    case 162: {  // a drawing plugin's surface on a part (number, width, height, three plugin values, state)
        int st = a.i(6);
        if (st < 1 || st > 3 || a.i(1) <= 0 || a.i(2) <= 0) { a.retBool(false); break; }
        Part &p = get(a.i(0));
        State &s = p.states[st - 1];
        s.built.create(a.i(1), a.i(2), false);
        s.hasBuilt = true;
        p.dirty = true;
        a.retBool(true);
        break;
    }
    // That surface's handle for the plugin: DrawMovie3.Draw takes the part's sprite number.
    case 163: syncAll(); a.retInt(find(a.i(0)) ? kSpriteBase + a.i(0) : -1); break;
    default: logOnce("PartsFunc " + std::to_string(id) + " is not known"); break;
    }
}

}  // namespace

S4_HLL(PartsEngine, PartsFunc) { partsFunc(c.i(0), {c, c.page(1), c.page(2), c.page(3)}); }

// ---- ChipmunkSpriteEngine: the frame, transitions and settings ----
S4_HLL(ChipmunkSpriteEngine, Update) {
    syncAll();
    G().present();
}
S4_HLL(ChipmunkSpriteEngine, Sleep) {}
S4_HLL(ChipmunkSpriteEngine, KeepPreviousView) {}
S4_HLL(ChipmunkSpriteEngine, KeepPreviousView_SetMode) {}
S4_HLL(ChipmunkSpriteEngine, KeepPreviousView_GetMode) { c.retInt(0); }
S4_HLL(ChipmunkSpriteEngine, VIEW_SetMode) {}
S4_HLL(ChipmunkSpriteEngine, VIEW_GetMode) { c.retInt(0); }
S4_HLL(ChipmunkSpriteEngine, VIEW_SetOffsetPos) {}
// A transition: the screen as it was when it began crossfades into the parts as they are, by the game's rate.
S4_HLL(ChipmunkSpriteEngine, TRANS_Begin) {
    syncAll();
    g_transFrom.assign(size_t(G().screenW) * G().screenH * 4, 0);
    G().sprites.compose(g_transFrom, G().screenW, G().screenH, 0, 0);
    c.retBool(true);
}
S4_HLL(ChipmunkSpriteEngine, TRANS_Update) {
    syncAll();
    std::vector<uint8_t> now(size_t(G().screenW) * G().screenH * 4);
    G().sprites.compose(now, G().screenW, G().screenH, 0, 0);
    if (g_transFrom.size() == now.size()) {
        int a = int(std::clamp(c.f(0), 0.0f, 1.0f) * 256);
        for (size_t i = 0; i < now.size(); i++) now[i] = uint8_t(g_transFrom[i] + ((now[i] - g_transFrom[i]) * a >> 8));
    }
    G().showFrame(std::move(now));
    c.retBool(true);
}
S4_HLL(ChipmunkSpriteEngine, TRANS_End) { g_transFrom.clear(); c.retBool(true); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteType) { g_textDefault.face = c.i(0) == 1 ? 1 : 0; }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteSize) { g_textDefault.size = c.i(0); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteColor) { g_textDefault.r = c.i(0); g_textDefault.g = c.i(1); g_textDefault.b = c.i(2); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteBoldWeight) { g_textDefault.boldWeight = c.f(0); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteEdgeWeight) { g_textDefault.edgeWeight = c.f(0); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteEdgeColor) { g_textDefault.er = c.i(0); g_textDefault.eg = c.i(1); g_textDefault.eb = c.i(2); }
// The width of text in the SP_SetTextSprite* style, which the game lays out message characters by.
S4_HLL(ChipmunkSpriteEngine, SP_GetFontWidth) {
    c.ref(1) = textWidth(c.s(0), g_textDefault, 0);
    c.retBool(true);
}
S4_HLL(ChipmunkSpriteEngine, FPS_Get) { c.retFloat(60); }
S4_HLL(ChipmunkSpriteEngine, SYSTEM_SetConfigOverFrameRateSleep) {}
S4_HLL(ChipmunkSpriteEngine, SYSTEM_GetConfigOverFrameRateSleep) { c.retBool(true); }
S4_HLL(ChipmunkSpriteEngine, SYSTEM_SetConfigSleepByInactiveWindow) {}
S4_HLL(ChipmunkSpriteEngine, SYSTEM_GetConfigSleepByInactiveWindow) { c.retBool(false); }
S4_HLL(ChipmunkSpriteEngine, SYSTEM_SetReadMessageSkipping) {}
S4_HLL(ChipmunkSpriteEngine, SYSTEM_GetReadMessageSkipping) { c.retBool(false); }
S4_HLL(ChipmunkSpriteEngine, SYSTEM_SetConfigFrameSkipWhileMessageSkip) {}
S4_HLL(ChipmunkSpriteEngine, SYSTEM_GetConfigFrameSkipWhileMessageSkip) { c.retBool(false); }
S4_HLL(ChipmunkSpriteEngine, SYSTEM_SetInvalidateFrameSkipWhileMessageSkip) {}
S4_HLL(ChipmunkSpriteEngine, SYSTEM_GetInvalidateFrameSkipWhileMessageSkip) { c.retBool(false); }

}  // namespace s4
