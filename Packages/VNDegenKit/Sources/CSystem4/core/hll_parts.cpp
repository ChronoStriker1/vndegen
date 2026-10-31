// PartsEngine and ChipmunkSpriteEngine: the sprite and UI library later System 4 games (Test Game 56) are built on.
// A part is a numbered component with up to three states (1 normal, 2 under the pointer, 3 pressed), each a picture,
// text or a picture built by construction steps, and a position, depth, visibility and alpha. Parts may sit inside
// a parent. Every part draws as a sprite of the runtime's own sprite list, so the frame composes as for the earlier
// games. Behaviour comes from the library's function and argument names in the game's code file and from the calls
// the game makes; docs/system4-cleanroom.md has the details.
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "game.h"
#include "gfx.h"
#include "sjis.h"

namespace s4 {
namespace {

Game &G() { return game(); }

constexpr int kSpriteBase = 1000000;  // parts' sprite numbers, above any the earlier sprite engines use

struct Step {  // a construction step
    enum Kind { Create, CreatePixelOnly, Fill, FillAlpha } kind;
    int x = 0, y = 0, w = 0, h = 0, r = 0, g = 0, b = 0, a = 255;
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
};

struct Motion {
    enum Kind { Alpha, Pos } kind;
    int number;
    float from[2], to[2];
    int begin, end;
};

struct Part {
    State states[3];
    int state = 1;
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

std::map<int, Part> g_parts;
std::vector<Motion> g_motions;
int g_motionTime = 0;
bool g_motionOn = false;
TextStyle g_textDefault;  // ChipmunkSpriteEngine's SP_SetTextSprite* defaults for new text

S4_HLL_RESET {
    for (auto &[n, p] : g_parts) G().sprites.remove(kSpriteBase + n);
    g_parts.clear();
    g_motions.clear();
    g_motionTime = 0;
    g_motionOn = false;
    g_textDefault = TextStyle{};
}

Part *find(int n) {
    auto it = g_parts.find(n);
    return it == g_parts.end() ? nullptr : &it->second;
}

Part &get(int n) {
    auto [it, added] = g_parts.try_emplace(n);
    if (added)
        for (auto &s : it->second.states) s.font = g_textDefault;
    return it->second;
}

State *stateOf(HllCall &c, int numberArg, int stateArg) {
    int s = c.i(stateArg);
    if (s < 1 || s > 3) return nullptr;
    return &get(c.i(numberArg)).states[s - 1];
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
        int w = std::max(1, textWidth(s.text, s.font, s.charSpace));
        int h = s.font.size + 4;
        Surface t;
        t.create(w, h, true);
        drawText(t, 0, 0, s.text, s.font, s.charSpace);
        if (sp.hasOwn || sp.cg) {
            Surface &dst = sp.writable();
            blit(dst.px.data(), dst.w, dst.h, 0, 0, t.px.data(), t.w, t.h, true, 255, 0);
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
    z += p->z;
    alpha = alpha * p->alpha / 255;
    show = show && p->show;
    if (p->parent > 0 && p->parent != n) placement(p->parent, x, y, z, alpha, show, depth + 1);
}

// Draws every part into its sprite: the picture when it changed, and the placement each time.
void syncAll() {
    for (auto &[n, p] : g_parts) {
        Sprite &sp = G().sprites.get(kSpriteBase + n);
        sp.used = true;
        if (p.dirty) {
            drawState(p.states[std::clamp(p.state, 1, 3) - 1], sp);
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
S4_HLL(PartsEngine, Parts_SetPartsState) {
    Part &p = get(c.i(0));
    if (p.state != c.i(1)) { p.state = c.i(1); p.dirty = true; }
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
S4_HLL(PartsEngine, UpdateComponent) { syncAll(); }
// Messages are what the player did to parts (clicks and the like); none are made yet.
S4_HLL(PartsEngine, GetMessageType) { c.retInt(0); }
S4_HLL(PartsEngine, ReleaseMessage) {}
S4_HLL(PartsEngine, BeginInput) {}
S4_HLL(PartsEngine, EndInput) {}

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
S4_HLL(ChipmunkSpriteEngine, TRANS_Begin) { c.retBool(true); }
S4_HLL(ChipmunkSpriteEngine, TRANS_Update) { syncAll(); G().present(); }
S4_HLL(ChipmunkSpriteEngine, TRANS_End) {}
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteType) { g_textDefault.face = c.i(0) == 1 ? 1 : 0; }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteSize) { g_textDefault.size = c.i(0); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteColor) { g_textDefault.r = c.i(0); g_textDefault.g = c.i(1); g_textDefault.b = c.i(2); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteBoldWeight) { g_textDefault.boldWeight = c.f(0); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteEdgeWeight) { g_textDefault.edgeWeight = c.f(0); }
S4_HLL(ChipmunkSpriteEngine, SP_SetTextSpriteEdgeColor) { g_textDefault.er = c.i(0); g_textDefault.eg = c.i(1); g_textDefault.eb = c.i(2); }
S4_HLL(ChipmunkSpriteEngine, SP_GetFontWidth) { c.retInt(c.i(1) / 2); }
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
