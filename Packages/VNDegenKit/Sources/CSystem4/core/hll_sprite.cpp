// The sprite engine library of SACT2 and its successor StoatSpriteEngine: sprites, CG, text on sprites, input,
// music and sound. Function behaviour follows the SACT2 reference in AliceSoft's System 4.0 SDK; the Stoat
// names that SACT2 lacks were read from how the games call them.
#include <algorithm>
#include <chrono>
#include <ctime>

#include "game.h"
#include "sjis.h"

namespace s4 {
namespace {

Game &G() { return game(); }

TextStyle styleFrom(const Page *tm) {
    TextStyle st;
    if (!tm || tm->v.size() < 13) return st;
    const auto &v = tm->v;
    st.r = v[0]; st.g = v[1]; st.b = v[2];
    st.size = v[3] > 0 ? v[3] : 16;
    st.weight = v[4];
    st.face = v[5];
    st.shadowL = v[6]; st.shadowU = v[7]; st.shadowR = v[8]; st.shadowD = v[9];
    st.sr = v[10]; st.sg = v[11]; st.sb = v[12];
    return st;
}

// Text drawn on a sprite goes to its text layer, which covers the sprite.
void textDraw(int n, const std::string &text, const TextStyle &st) {
    Sprite *s = G().sprites.find(n);
    if (!s) return;
    if (s->text.w != s->width() || s->text.h != s->height()) s->text.create(s->width(), s->height(), true);
    int w = drawText(s->text, s->textX, s->textY, text, st, s->charSpace);
    s->textX += w;
    s->lineHeight = std::max(s->lineHeight, st.size);
}

// ---- engine ----

S4_HLL(StoatSpriteEngine, Init) { c.retInt(1); }
S4_HLL(StoatSpriteEngine, GetScreenWidth) { c.retInt(G().screenW); }
S4_HLL(StoatSpriteEngine, GetScreenHeight) { c.retInt(G().screenH); }
S4_HLL(StoatSpriteEngine, Update) {
    G().present();
    G().yieldFrame();
    c.retInt(1);
}
S4_HLL(StoatSpriteEngine, Error) { G().log("game error: " + sjis::toUtf8(c.s(0))); c.retInt(1); }

// ---- sprites ----

S4_HLL(StoatSpriteEngine, SP_GetUnuseNum) { c.retInt(G().sprites.unused(c.i(0))); }
S4_HLL(StoatSpriteEngine, SP_Count) {
    int n = 0;
    for (auto &[k, s] : G().sprites.all()) n += s.used;
    c.retInt(n);
}
S4_HLL(StoatSpriteEngine, SP_SetCG) {
    auto img = G().cgCached(c.i(1));
    if (!img) {
        G().log("SP_SetCG: no CG " + std::to_string(c.i(1)));
        c.retInt(0);
        return;
    }
    Sprite &s = G().sprites.get(c.i(0));
    s.used = true;
    s.cg = img;
    s.hasOwn = false;
    s.own = {};
    c.retInt(1);
}
S4_HLL(StoatSpriteEngine, SP_Create) {
    Sprite &s = G().sprites.get(c.i(0));
    s.used = true;
    s.cg.reset();
    s.hasOwn = true;
    s.own.create(c.i(1), c.i(2), true);
    s.own.fill(0, 0, s.own.w, s.own.h, c.i(3), c.i(4), c.i(5), c.i(6));
    c.retInt(1);
}
S4_HLL(StoatSpriteEngine, SP_CreatePixelOnly) {
    Sprite &s = G().sprites.get(c.i(0));
    s.used = true;
    s.cg.reset();
    s.hasOwn = true;
    s.own.create(c.i(1), c.i(2), false);
    c.retInt(1);
}
S4_HLL(StoatSpriteEngine, SP_CreateCopy) {
    Sprite *src = G().sprites.find(c.i(1));
    if (!src) { c.retInt(0); return; }
    Sprite copy = *src;
    G().sprites.get(c.i(0)) = copy;
    c.retInt(1);
}
S4_HLL(StoatSpriteEngine, SP_Delete) { G().sprites.remove(c.i(0)); c.retInt(1); }
S4_HLL(StoatSpriteEngine, SP_DeleteAll) { G().sprites.clear(); c.retBool(true); }
S4_HLL(StoatSpriteEngine, SP_IsUsing) { c.retBool(G().sprites.find(c.i(0)) != nullptr); }

#define SPRITE_SET(fn, body)                         \
    S4_HLL(StoatSpriteEngine, fn) {                  \
        Sprite *s = G().sprites.find(c.i(0));        \
        if (s) { body; }                             \
        c.retInt(s ? 1 : 0);                         \
    }
#define SPRITE_GET(fn, expr, dflt)                   \
    S4_HLL(StoatSpriteEngine, fn) {                  \
        Sprite *s = G().sprites.find(c.i(0));        \
        c.retInt(s ? int32_t(expr) : (dflt));        \
    }

SPRITE_SET(SP_SetPos, s->x = c.i(1); s->y = c.i(2))
SPRITE_SET(SP_SetX, s->x = c.i(1))
SPRITE_SET(SP_SetY, s->y = c.i(1))
SPRITE_SET(SP_SetZ, s->z = c.i(1))
SPRITE_SET(SP_SetBlendRate, s->blendRate = std::clamp(c.i(1), 0, 255))
SPRITE_SET(SP_SetShow, s->show = c.i(1) != 0)
SPRITE_SET(SP_SetDrawMethod, s->drawMethod = c.i(1))
SPRITE_SET(SP_SetBrightness, s->brightness = std::clamp(c.i(1), 0, 255))
SPRITE_GET(SP_ExistAlpha, s->hasAlpha(), 0)
SPRITE_GET(SP_GetPosX, s->x, 0)
SPRITE_GET(SP_GetPosY, s->y, 0)
SPRITE_GET(SP_GetWidth, s->width(), 0)
SPRITE_GET(SP_GetHeight, s->height(), 0)
SPRITE_GET(SP_GetZ, s->z, 0)
SPRITE_GET(SP_GetBlendRate, s->blendRate, 0)
SPRITE_GET(SP_GetShow, s->show, 0)
SPRITE_GET(SP_GetDrawMethod, s->drawMethod, 0)
SPRITE_GET(SP_GetBrightness, s->brightness, 0)

// Hit tests use the sprite's position; SP_IsPtIn also asks the alpha map.
S4_HLL(StoatSpriteEngine, SP_IsPtInRect) {
    Sprite *s = G().sprites.find(c.i(0));
    int x = c.i(1), y = c.i(2);
    c.retInt(s && x >= s->x && y >= s->y && x < s->x + s->width() && y < s->y + s->height());
}
S4_HLL(StoatSpriteEngine, SP_IsPtIn) {
    Sprite *s = G().sprites.find(c.i(0));
    int x = c.i(1), y = c.i(2);
    bool in = s && x >= s->x && y >= s->y && x < s->x + s->width() && y < s->y + s->height();
    if (in && s->hasAlpha()) in = s->pixels()[(size_t(y - s->y) * s->width() + (x - s->x)) * 4 + 3] > 0;
    c.retInt(in);
}
S4_HLL(StoatSpriteEngine, SP_GetAMapValue) {
    Sprite *s = G().sprites.find(c.i(0));
    int x = c.i(1), y = c.i(2);
    if (!s || x < 0 || y < 0 || x >= s->width() || y >= s->height() || !s->pixels()) { c.retInt(-1); return; }
    c.retInt(s->hasAlpha() ? s->pixels()[(size_t(y) * s->width() + x) * 4 + 3] : 255);
}
S4_HLL(StoatSpriteEngine, SP_GetPixelValue) {
    Sprite *s = G().sprites.find(c.i(0));
    int x = c.i(1), y = c.i(2);
    if (!s || x < 0 || y < 0 || x >= s->width() || y >= s->height() || !s->pixels()) { c.retBool(false); return; }
    const uint8_t *p = s->pixels() + (size_t(y) * s->width() + x) * 4;
    c.ref(3) = p[0];
    c.ref(4) = p[1];
    c.ref(5) = p[2];
    c.retBool(true);
}

// ---- text on sprites ----

SPRITE_SET(SP_SetTextHome, s->textHomeX = c.i(1); s->textHomeY = c.i(2))
SPRITE_SET(SP_SetTextLineSpace, s->lineSpace = c.i(1))
SPRITE_SET(SP_SetTextCharSpace, s->charSpace = c.i(1))
SPRITE_SET(SP_SetTextPos, s->textX = c.i(1); s->textY = c.i(2))
SPRITE_GET(SP_GetTextHomeX, s->textHomeX, 0)
SPRITE_GET(SP_GetTextHomeY, s->textHomeY, 0)
SPRITE_GET(SP_GetTextCharSpace, s->charSpace, 0)
SPRITE_GET(SP_GetTextPosX, s->textX, 0)
SPRITE_GET(SP_GetTextPosY, s->textY, 0)
SPRITE_GET(SP_GetTextLineSpace, s->lineSpace, 0)
S4_HLL(StoatSpriteEngine, SP_TextDraw) {
    textDraw(c.i(0), c.s(1), styleFrom(c.page(2)));
    c.retInt(1);
}
SPRITE_SET(SP_TextClear, s->text = {})
S4_HLL(StoatSpriteEngine, SP_TextHome) {
    Sprite *s = G().sprites.find(c.i(0));
    if (s) { s->textX = s->textHomeX; s->textY = s->textHomeY; s->lineHeight = 0; }
    c.retInt(s ? 1 : 0);
}
S4_HLL(StoatSpriteEngine, SP_TextNewLine) {
    Sprite *s = G().sprites.find(c.i(0));
    if (s) {
        s->textX = s->textHomeX;
        s->textY += std::max(c.i(1), s->lineHeight) + s->lineSpace;
        s->lineHeight = 0;
    }
    c.retInt(s ? 1 : 0);
}
S4_HLL(StoatSpriteEngine, SP_TextCopy) {
    Sprite *d = G().sprites.find(c.i(0)), *s = G().sprites.find(c.i(1));
    if (d && s) { d->text = s->text; d->textX = s->textX; d->textY = s->textY; }
    c.retInt(d && s);
}

// A sprite of its own holding one line of text, in the style the SP_SetTextSprite* calls set.
TextStyle g_textSpriteStyle;
S4_HLL(StoatSpriteEngine, SP_SetTextSpriteType) { g_textSpriteStyle.face = c.i(0); }
S4_HLL(StoatSpriteEngine, SP_SetTextSpriteSize) { g_textSpriteStyle.size = c.i(0); }
S4_HLL(StoatSpriteEngine, SP_SetTextSpriteColor) { g_textSpriteStyle.r = c.i(0); g_textSpriteStyle.g = c.i(1); g_textSpriteStyle.b = c.i(2); }
S4_HLL(StoatSpriteEngine, SP_SetTextSpriteBoldWeight) { g_textSpriteStyle.boldWeight = c.f(0); }
S4_HLL(StoatSpriteEngine, SP_SetTextSpriteEdgeWeight) { g_textSpriteStyle.edgeWeight = c.f(0); }
S4_HLL(StoatSpriteEngine, SP_SetTextSpriteEdgeColor) { g_textSpriteStyle.er = c.i(0); g_textSpriteStyle.eg = c.i(1); g_textSpriteStyle.eb = c.i(2); }
S4_HLL(StoatSpriteEngine, SP_SetTextSprite) {
    const TextStyle &st = g_textSpriteStyle;
    int edge = int(st.edgeWeight + 0.5f);
    int w = std::max(1, textWidth(c.s(1), st, 0) + edge * 2), h = st.size + edge * 2;
    Sprite &s = G().sprites.get(c.i(0));
    s.used = true;
    s.cg.reset();
    s.hasOwn = true;
    s.own.create(w, h, true);
    drawText(s.own, edge, edge, c.s(1), st, 0);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, SP_SetDashTextSprite) {
    // A horizontal rule the size given, in the text sprite colour, standing in for a long dash.
    const TextStyle &st = g_textSpriteStyle;
    Sprite &s = G().sprites.get(c.i(0));
    s.used = true;
    s.cg.reset();
    s.hasOwn = true;
    s.own.create(c.i(1), c.i(2), true);
    int t = std::max(1, c.i(2) / 12);
    s.own.fill(0, (c.i(2) - t) / 2, c.i(1), t, st.r, st.g, st.b, 255);
    c.retBool(true);
}

// ---- CG ----

S4_HLL(StoatSpriteEngine, CG_IsExist) { c.retInt(G().cgExists(c.i(0))); }
S4_HLL(StoatSpriteEngine, CG_GetMetrics) {
    auto img = G().cgCached(c.i(0));
    Page *m = c.page(1);
    if (!img || !m || m->v.size() < 7) { c.retInt(0); return; }
    m->v[0] = img->w;
    m->v[1] = img->h;
    m->v[2] = 24;
    m->v[3] = 1;
    m->v[4] = img->hasAlpha;
    m->v[5] = img->w * 3;
    m->v[6] = img->hasAlpha ? img->w : 0;
    c.retInt(1);
}

// ---- input ----

S4_HLL(StoatSpriteEngine, Mouse_GetPos) {
    c.ref(0) = G().mouseX();
    c.ref(1) = G().mouseY();
    c.retInt(1);
}
int g_wheelForward = 0, g_wheelBack = 0;
S4_HLL(StoatSpriteEngine, Mouse_ClearWheel) { G().takeWheel(); g_wheelForward = g_wheelBack = 0; }
S4_HLL(StoatSpriteEngine, Mouse_GetWheel) {
    int d = G().takeWheel();
    if (d > 0) g_wheelForward += d; else g_wheelBack -= d;
    c.ref(0) = g_wheelForward;
    c.ref(1) = g_wheelBack;
}
S4_HLL(StoatSpriteEngine, Key_ClearFlag) { c.retInt(1); }
S4_HLL(StoatSpriteEngine, Key_ClearFlagNoCtrl) {}
S4_HLL(StoatSpriteEngine, Key_ClearFlagOne) {}
S4_HLL(StoatSpriteEngine, Key_IsDown) { c.retInt(G().keyDown(c.i(0))); }
S4_HLL(StoatSpriteEngine, KEY_GetState) { c.retBool(G().keyDown(c.i(0))); }
S4_HLL(StoatSpriteEngine, Timer_Get) { c.retInt(int32_t(G().now())); }

// ---- music and sound ----
// Channels map to Audio players. Music data comes from the BGM archive, sounds from the wave archive.

struct Channel { int handle = 0; int number = 0; };
std::map<int, Channel> g_music, g_sound;

std::shared_ptr<const std::vector<uint8_t>> archiveData(AldArchive *a, int number) {
    const ArchiveEntry *e = a ? a->find(number) : nullptr;
    if (!e) return nullptr;
    auto d = std::make_shared<std::vector<uint8_t>>();
    if (!a->read(*e, *d)) return nullptr;
    return d;
}

int prepare(std::map<int, Channel> &chans, int ch, AldArchive *a, int number, int group) {
    auto &c = chans[ch];
    if (c.handle) G().audio.close(c.handle);
    c = {};
    auto d = archiveData(a, number);
    if (!d) return 0;
    c.handle = G().audio.open(d);
    c.number = number;
    if (c.handle) G().audio.setGroup(c.handle, group);
    return c.handle ? 1 : 0;
}
int handleOf(std::map<int, Channel> &chans, int ch) {
    auto it = chans.find(ch);
    return it == chans.end() ? 0 : it->second.handle;
}

S4_HLL(StoatSpriteEngine, Music_IsExist) { c.retInt(G().bgmArchive && G().bgmArchive->find(c.i(0)) ? 1 : 0); }
S4_HLL(StoatSpriteEngine, Music_Prepare) {
    int ok = prepare(g_music, c.i(0), G().bgmArchive.get(), c.i(1), 1);
    if (ok) G().audio.setLoops(handleOf(g_music, c.i(0)), 0);
    c.retInt(ok);
}
S4_HLL(StoatSpriteEngine, Music_Unprepare) {
    int h = handleOf(g_music, c.i(0));
    if (h) G().audio.close(h);
    g_music.erase(c.i(0));
    c.retInt(1);
}
S4_HLL(StoatSpriteEngine, Music_Play) { G().audio.play(handleOf(g_music, c.i(0))); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Music_Stop) { G().audio.stop(handleOf(g_music, c.i(0))); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Music_IsPlay) { c.retInt(G().audio.playing(handleOf(g_music, c.i(0)))); }
S4_HLL(StoatSpriteEngine, Music_SetLoopCount) { G().audio.setLoops(handleOf(g_music, c.i(0)), c.i(1)); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Music_GetLoopCount) { c.retInt(G().audio.loops(handleOf(g_music, c.i(0)))); }
S4_HLL(StoatSpriteEngine, Music_Fade) { G().audio.fade(handleOf(g_music, c.i(0)), c.i(1), c.i(2), c.i(3)); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Music_StopFade) { G().audio.stopFade(handleOf(g_music, c.i(0))); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Music_IsFade) { c.retInt(G().audio.fading(handleOf(g_music, c.i(0)))); }
S4_HLL(StoatSpriteEngine, Music_Pause) { G().audio.pause(handleOf(g_music, c.i(0)), true); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Music_Restart) { G().audio.pause(handleOf(g_music, c.i(0)), false); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Music_IsPause) { c.retInt(G().audio.paused(handleOf(g_music, c.i(0)))); }
S4_HLL(StoatSpriteEngine, Music_GetPos) { c.retInt(int32_t(G().audio.positionMs(handleOf(g_music, c.i(0))))); }
S4_HLL(StoatSpriteEngine, Music_GetLength) { c.retInt(int32_t(G().audio.lengthMs(handleOf(g_music, c.i(0))))); }
S4_HLL(StoatSpriteEngine, Music_GetSamplePos) { c.retInt(int32_t(G().audio.positionSamples(handleOf(g_music, c.i(0))))); }
S4_HLL(StoatSpriteEngine, Music_GetSampleLength) { c.retInt(int32_t(G().audio.lengthSamples(handleOf(g_music, c.i(0))))); }
S4_HLL(StoatSpriteEngine, Music_Seek) { G().audio.seekMs(handleOf(g_music, c.i(0)), c.i(1)); c.retInt(1); }

S4_HLL(StoatSpriteEngine, Sound_IsExist) { c.retInt(G().waveArchive && G().waveArchive->find(c.i(0)) ? 1 : 0); }
S4_HLL(StoatSpriteEngine, Sound_GetUnuseChannel) {
    int ch = 1;
    while (g_sound.count(ch) && G().audio.playing(g_sound[ch].handle)) ch++;
    c.retInt(ch);
}
S4_HLL(StoatSpriteEngine, Sound_Prepare) { c.retInt(prepare(g_sound, c.i(0), G().waveArchive.get(), c.i(1), 2)); }
S4_HLL(StoatSpriteEngine, Sound_Unprepare) {
    int h = handleOf(g_sound, c.i(0));
    if (h) G().audio.close(h);
    g_sound.erase(c.i(0));
    c.retInt(1);
}
S4_HLL(StoatSpriteEngine, Sound_Play) { G().audio.play(handleOf(g_sound, c.i(0))); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Sound_Stop) { G().audio.stop(handleOf(g_sound, c.i(0))); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Sound_IsPlay) { c.retInt(G().audio.playing(handleOf(g_sound, c.i(0)))); }
S4_HLL(StoatSpriteEngine, Sound_Fade) { G().audio.fade(handleOf(g_sound, c.i(0)), c.i(1), c.i(2), c.i(3)); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Sound_StopFade) { G().audio.stopFade(handleOf(g_sound, c.i(0))); c.retInt(1); }
S4_HLL(StoatSpriteEngine, Sound_IsFade) { c.retInt(G().audio.fading(handleOf(g_sound, c.i(0)))); }
S4_HLL(StoatSpriteEngine, Sound_GetTimeLength) { c.retInt(int32_t(G().audio.lengthMs(handleOf(g_sound, c.i(0))))); }
S4_HLL(StoatSpriteEngine, Sound_GetGroupNum) { c.retInt(0); }
S4_HLL(SACT2, Sound_SetLoopCount) { G().audio.setLoops(handleOf(g_sound, c.i(0)), c.i(1)); c.retInt(1); }
S4_HLL(SACT2, Sound_GetLoopCount) { c.retInt(G().audio.loops(handleOf(g_sound, c.i(0)))); }
S4_HLL(SACT2, Sound_GetPos) { c.retInt(int32_t(G().audio.positionMs(handleOf(g_sound, c.i(0))))); }
S4_HLL(SACT2, Sound_GetLength) { c.retInt(int32_t(G().audio.lengthMs(handleOf(g_sound, c.i(0))))); }
S4_HLL(SACT2, Sound_ReverseLR) { c.retInt(1); }
S4_HLL(SACT2, Sound_GetVolume) { c.retInt(100); }
S4_HLL(StoatSpriteEngine, Sound_GetGroupNumFromDataNum) { c.retInt(0); }

// ---- transitions: the screen as it was blends into the screen as it is ----

std::vector<uint8_t> g_transFrom;
S4_HLL(StoatSpriteEngine, TRANS_Begin) {
    g_transFrom.assign(size_t(G().screenW) * G().screenH * 4, 0);
    G().sprites.compose(g_transFrom, G().screenW, G().screenH, 0, 0);
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, TRANS_Update) {
    float t = std::clamp(c.f(0), 0.0f, 1.0f);
    std::vector<uint8_t> now(g_transFrom.size());
    G().sprites.compose(now, G().screenW, G().screenH, 0, 0);
    if (g_transFrom.size() == now.size()) {
        int a = int(t * 256);
        for (size_t i = 0; i < now.size(); i++) now[i] = uint8_t(g_transFrom[i] + ((now[i] - g_transFrom[i]) * a >> 8));
    }
    G().showFrame(std::move(now));
    c.retBool(true);
}
S4_HLL(StoatSpriteEngine, TRANS_End) { g_transFrom.clear(); c.retBool(true); }

// ---- SACT2's screen effects, wallpaper and quake (the SACT2 reference) ----

// Runs a transition from the screen as last shown to the screen as the sprites are now, over ms of game time. Keys
// cancel it when key is set. Kinds 2 and 3 fade through black, 4 and 5 through white; every other kind crossfades
// (the games mostly use 1, the crossfade).
void runEffect(int kind, int ms, bool key) {
    std::vector<uint8_t> from, to(size_t(G().screenW) * G().screenH * 4);
    int w, h;
    G().takeFrame(from, w, h);
    G().sprites.compose(to, G().screenW, G().screenH, 0, 0);
    if (from.size() != to.size() || ms <= 0) { G().showFrame(std::move(to)); return; }
    uint32_t start = G().now();
    std::vector<uint8_t> out(to.size());
    for (;;) {
        uint32_t t = G().now() - start;
        if (t >= uint32_t(ms)) break;
        if (key && (G().keyDown(1) || G().keyDown(13) || G().keyDown(32))) break;
        float r = float(t) / float(ms);
        int mid = kind == 4 || kind == 5 ? 255 : 0;
        for (size_t i = 0; i < out.size(); i++) {
            int a = from[i], b = to[i], v;
            if (kind >= 2 && kind <= 5) v = r < 0.5f ? a + int((mid - a) * r * 2) : mid + int((b - mid) * (r - 0.5f) * 2);
            else v = a + int((b - a) * r);
            out[i] = uint8_t(v);
        }
        G().showFrame(out);
        G().yieldFrame();
    }
    G().showFrame(std::move(to));
}

S4_HLL(SACT2, Effect) { runEffect(c.i(0), c.i(1), c.i(2) != 0); c.retInt(1); }
S4_HLL(SACT2, EffectSetMask) { c.retInt(1); }
S4_HLL(SACT2, EffectSetMaskSP) { c.retInt(1); }

// Shakes the screen by moving every sprite, back and forth, for ms.
S4_HLL(SACT2, QuakeScreen) {
    int ax = c.i(0), ay = c.i(1), ms = c.i(2);
    uint32_t start = G().now();
    std::vector<uint8_t> f(size_t(G().screenW) * G().screenH * 4);
    for (int k = 0; G().now() - start < uint32_t(std::max(0, ms)); k++) {
        int dx = ax ? (k % 2 ? ax : -ax) * int(ms - (G().now() - start)) / std::max(1, ms) : 0;
        int dy = ay ? (k % 2 ? ay : -ay) * int(ms - (G().now() - start)) / std::max(1, ms) : 0;
        G().sprites.compose(f, G().screenW, G().screenH, dx, dy);
        G().showFrame(f);
        G().yieldFrame();
    }
    G().present();
}
S4_HLL(SACT2, QUAKE_SET_CROSS) {}
S4_HLL(SACT2, QUAKE_SET_ROTATION) {}

// The wallpaper is the picture behind every sprite: sprite 0 here, which no game creates itself.
S4_HLL(SACT2, SetWP) {
    auto img = G().cgCached(c.i(0));
    Sprite &s = G().sprites.get(0);
    s = Sprite{};
    s.used = img != nullptr;
    s.cg = img;
    s.z = INT32_MIN;
    c.retInt(img ? 1 : 0);
}
S4_HLL(StoatSpriteEngine, SetWP_Color) {
    Sprite &s = G().sprites.get(0);
    s = Sprite{};
    s.used = true;
    s.hasOwn = true;
    s.own.create(G().screenW, G().screenH, false);
    s.own.fill(0, 0, G().screenW, G().screenH, c.i(0), c.i(1), c.i(2), 255);
    s.z = INT32_MIN;
    c.retInt(1);
}
S4_HLL(SACT2, WP_GetSP) { c.retInt(0); }
S4_HLL(SACT2, WP_SetSP) { c.retInt(1); }
S4_HLL(SACT2, GetMainSurfaceNumber) { c.retInt(-1); }
S4_HLL(SACT2, SP_GetMaxZ) {
    int z = 0;
    for (auto &[n, sp] : G().sprites.all())
        if (sp.used && n != 0) z = std::max(z, sp.z);
    c.retInt(z);
}
S4_HLL(SACT2, Mouse_SetPos) { G().mouseMove(c.i(0), c.i(1)); c.retInt(1); }
S4_HLL(SACT2, Joypad_ClearKeyDownFlag) {}
S4_HLL(SACT2, Joypad_IsKeyDown) { c.retInt(0); }
S4_HLL(SACT2, Joypad_GetNumof) { c.retInt(0); }
S4_HLL(SACT2, JoypadQuake_Set) {}
S4_HLL(SACT2, Joypad_GetAnalogStickStatus) { c.refF(2) = 0; c.refF(3) = 0; c.retBool(false); }
S4_HLL(SACT2, Joypad_GetDigitalStickStatus) {
    for (int k = 2; k < 6; k++) c.ref(k) = 0;
    c.retBool(false);
}

// ---- clock ----

S4_HLL(StoatSpriteEngine, System_GetDate) {
    std::time_t t = std::time(nullptr);
    std::tm lt = *std::localtime(&t);
    c.ref(0) = lt.tm_year + 1900;
    c.ref(1) = lt.tm_mon + 1;
    c.ref(2) = lt.tm_mday;
    c.ref(3) = lt.tm_wday;
}
S4_HLL(StoatSpriteEngine, System_GetTime) {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm lt = *std::localtime(&t);
    c.ref(0) = lt.tm_hour;
    c.ref(1) = lt.tm_min;
    c.ref(2) = lt.tm_sec;
    c.ref(3) = int(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);
}

// ---- strings ----

S4_HLL(StoatSpriteEngine, IntToZenkaku) {
    std::string s = std::to_string(c.i(1));
    int fig = c.i(2);
    if (fig > 0 && int(s.size()) < fig) s = std::string(fig - s.size(), c.i(3) ? '0' : ' ') + s;
    std::string out;
    for (char ch : s) {
        if (ch >= '0' && ch <= '9') { out += char(0x82); out += char(0x4F + (ch - '0')); }
        else if (ch == '-') { out += char(0x81); out += char(0x7C); }
        else { out += char(0x81); out += char(0x40); }
    }
    c.refS(0) = out;
}
S4_HLL(StoatSpriteEngine, IntToHankaku) {
    std::string s = std::to_string(c.i(1));
    int fig = c.i(2);
    if (fig > 0 && int(s.size()) < fig) s = std::string(fig - s.size(), c.i(3) ? '0' : ' ') + s;
    c.refS(0) = s;
}

// ---- view and system settings the games set; VNDegen has no use for them ----

S4_HLL(StoatSpriteEngine, FPS_SetShow) {}
S4_HLL(StoatSpriteEngine, FPS_GetShow) { c.retBool(false); }
S4_HLL(StoatSpriteEngine, FPS_Get) { c.retFloat(60.0f); }
int g_viewMode = 0;
bool g_keepPrevious = false;
S4_HLL(StoatSpriteEngine, VIEW_SetMode) { g_viewMode = c.i(0); c.retBool(true); }
S4_HLL(StoatSpriteEngine, VIEW_GetMode) { c.retInt(g_viewMode); }
S4_HLL(StoatSpriteEngine, VIEW_SetOffsetPos) {}
S4_HLL(StoatSpriteEngine, KeepPreviousView_SetMode) { g_keepPrevious = c.b(0); c.retBool(true); }
S4_HLL(StoatSpriteEngine, KeepPreviousView_GetMode) { c.retBool(g_keepPrevious); }
S4_HLL(StoatSpriteEngine, DX_GetUsePower2Texture) { c.retBool(false); }
S4_HLL(StoatSpriteEngine, DX_SetUsePower2Texture) {}
bool g_sysFlags[8] = {};
S4_HLL(StoatSpriteEngine, SYSTEM_IsResetOnce) { c.retBool(false); }
S4_HLL(StoatSpriteEngine, SYSTEM_SetConfigOverFrameRateSleep) { g_sysFlags[0] = c.b(0); }
S4_HLL(StoatSpriteEngine, SYSTEM_GetConfigOverFrameRateSleep) { c.retBool(g_sysFlags[0]); }
S4_HLL(StoatSpriteEngine, SYSTEM_SetConfigSleepByInactiveWindow) { g_sysFlags[1] = c.b(0); }
S4_HLL(StoatSpriteEngine, SYSTEM_GetConfigSleepByInactiveWindow) { c.retBool(g_sysFlags[1]); }
S4_HLL(StoatSpriteEngine, SYSTEM_SetReadMessageSkipping) { g_sysFlags[2] = c.b(0); }
S4_HLL(StoatSpriteEngine, SYSTEM_GetReadMessageSkipping) { c.retBool(g_sysFlags[2]); }
S4_HLL(StoatSpriteEngine, SYSTEM_SetConfigFrameSkipWhileMessageSkip) { g_sysFlags[3] = c.b(0); }
S4_HLL(StoatSpriteEngine, SYSTEM_GetConfigFrameSkipWhileMessageSkip) { c.retBool(g_sysFlags[3]); }
S4_HLL(StoatSpriteEngine, SYSTEM_SetInvalidateFrameSkipWhileMessageSkip) { g_sysFlags[4] = c.b(0); }
S4_HLL(StoatSpriteEngine, SYSTEM_GetInvalidateFrameSkipWhileMessageSkip) { c.retBool(g_sysFlags[4]); }
S4_HLL(StoatSpriteEngine, Debug_GetCurrentAllocatedMemorySize) { c.retInt(0); }
S4_HLL(StoatSpriteEngine, Debug_GetMaxAllocatedMemorySize) { c.retInt(0); }
S4_HLL(StoatSpriteEngine, Debug_GetFillRate) { c.retInt(0); }

}  // namespace
}  // namespace s4
