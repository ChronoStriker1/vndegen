// Ponscripter's additions to NScripter, as the Umineko When They Cry games use them. Ponscripter games have a UTF-8
// script (0.utf) holding the text in two languages, each line marked with langjp or langen. Everything here was
// settled as a black box, with lab scripts run on the games' own engine under Wine; docs/nscripter-cleanroom.md lists
// what each lab showed.
#include <algorithm>
#include <cstdint>

#include "engine.h"

namespace vnns {

namespace {
using E = Engine;

// langen, langjp, langall: the language of the text that follows, until the next of them. The rest of the line is
// usually that text (langen^Hello^@).
void cmdLang(E &e, const std::string &name) {
    e.textLang_ = name == "langen" ? 0 : name == "langjp" ? 1 : -1;
    e.continueLine_ = true;
}

void cmdShowLang(E &e, const std::string &name) { e.shownLang_ = name == "showlangjp" ? 1 : 0; }

// getreadlang: the language of the text being read: -1 for all, 0 English, 1 Japanese. The games' textgosub routine
// skips the waits of the language that isn't shown with it.
void cmdGetReadLang(E &e, const std::string &) { e.setVar(e.readVar(), e.textLang_); }

// language english|japanese, in the define block. It changed neither the language shown nor the text's layout in any
// lab, so it's read and ignored.
void cmdLanguage(E &e, const std::string &) { e.readStr(); }

// h_mapfont n,"file": face n is the game's font file.
void cmdMapFont(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    e.fontFiles_[n] = e.readStr();
}

// h_rendering hinting, positioning: how glyphs are rasterized. The games ask for no hinting and whole-pixel positions,
// which is how the runtime draws them, so the settings are read and kept no further.
void cmdRendering(E &e, const std::string &) {
    e.readStr();
    if (e.readComma()) e.readStr();
}

// steamsetachieve "name": a Steam achievement, which VNDegen has no place for.
void cmdSteamAchievement(E &e, const std::string &) { logf("Steam achievement %s", e.readStr().c_str()); }

// tachistate %l,%c,%r: 1 for each standing image (left, centre, right) on screen, else 0. The games read it to draw
// them again after switching sprite sets.
void cmdTachiState(E &e, const std::string &) {
    for (int i = 0; i < 3; i++) {
        if (i) e.readComma();
        const Sprite &s = e.scene_.ld[size_t(i)];
        e.setVar(e.readVar(), s.loaded && s.visible ? 1 : 0);
    }
}

// endroll dx,dy,dw,dh,sx,sy,sw,sh,mode,speed,distance: a credits roll. The btndef image's rectangle (sx, sy, sw, sh)
// is drawn into (dx, dy, dw, dh), moving up a pixel every speed / 10 ms until it has moved distance pixels; the script
// waits that long, and a click doesn't cut it short. The games use mode 2 only, the one measured.
void cmdEndRoll(E &e, const std::string &) {
    int64_t v[11];
    for (int i = 0; i < 11; i++) {
        if (i) e.readComma();
        v[i] = e.readInt();
    }
    if (v[8] != 2) logf("endroll mode %lld isn't measured; it scrolls as mode 2", static_cast<long long>(v[8]));
    E::EndRoll &r = e.endRoll_;
    std::copy(v, v + 8, r.rect);
    r.distance = std::clamp<int64_t>(v[10], 0, 1 << 20);
    r.speed = std::clamp<int64_t>(v[9], 1, 100000);
    r.start = e.now_;
    // The wait and the scroll end together, at the whole millisecond the last pixel is reached.
    int64_t duration = (r.distance * r.speed + 9) / 10;
    r.end = r.start + duration;
    r.pos = e.cur_.pos;
    r.on = true;
    if (duration > 0) {
        e.waitTimer(int(std::min<int64_t>(duration, INT32_MAX)), false);
        e.waitUntil_ = r.end;
    }
    e.stepEndRoll();
}

// drawclear: the draw picture becomes black.
void cmdDrawClear(E &e, const std::string &) { e.drawBuffer_ = Image(e.width(), e.height(), rgba(0, 0, 0)); }

// drawsp n,cell,alpha,x,y: draws sprite n's cell onto the draw picture, its top left at (x, y), whether or not the
// sprite is shown. A cell past the sprite's last draws its whole image, as on the PC.
void cmdDrawSp(E &e, const std::string &) {
    int v[5];
    for (int i = 0; i < 5; i++) {
        if (i) e.readComma();
        v[i] = e.readInt();
    }
    if (e.drawBuffer_.empty()) e.drawBuffer_ = Image(e.width(), e.height(), rgba(0, 0, 0));
    const Sprite *s = e.spriteRef(v[0]);
    if (!s || !s->img.image) return;
    const Image &img = *s->img.image;
    int cells = std::max(1, s->img.cells), cw = s->img.cellWidth;
    int sx = v[1] >= 0 && v[1] < cells ? v[1] * cw : 0, w = v[1] >= 0 && v[1] < cells ? cw : img.w;
    blit(e.drawBuffer_, img, v[3], v[4], sx, 0, w, img.h, std::clamp(v[2], 0, 255));
}

// draw: the draw picture goes on the screen, over the scene, until the scene is next brought up to date.
void cmdDraw(E &e, const std::string &) {
    if (e.drawBuffer_.empty()) e.drawBuffer_ = Image(e.width(), e.height(), rgba(0, 0, 0));
    e.bltLayer_ = e.drawBuffer_;
    e.bltState_ = 1;
    e.composeFrame();
}

// logsp n,text,x,y[,colour...]: a sprite of a page's text, as getlog or gettext gave it, for a backlog screen. In
// Ponscripter its position is on the doubled screen, it breaks at any character to fit the window's width, and the
// page's line breaks are dropped (measured on the PC engine).
void cmdLogSp(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    std::string text = e.readStr();
    e.readComma();
    int x = e.readInt();
    e.readComma();
    int y = e.readInt();
    std::vector<uint32_t> colors;
    while (e.readComma()) colors.push_back(parseHexColor(e.readStr()));
    if (colors.empty()) colors.push_back(rgba(255, 255, 255));
    if (colors.size() > 64) colors.resize(64);
    text.erase(std::remove(text.begin(), text.end(), '\n'), text.end());
    Sprite *s = e.spriteRef(n);
    if (!s) return;
    int cells = 1;
    ImageRef img = e.renderPonsString(text, 2 * e.ponsSize(), 0, colors, e.win_.shadow, cells, e.ponsLineWidth2());
    *s = Sprite();
    // Like strsp's, a logsp sprite saves with no tag and alpha 255, so it doesn't come back on load.
    s->alpha = 255;
    s->img.image = img;
    s->img.cells = cells;
    s->img.cellWidth = img->w / cells;
    s->img.loop = 3;
    s->loaded = s->visible = true;
    s->x = x / 2;
    s->y = y / 2;
    e.sceneDirty_ = true;
}
}  // namespace

void Engine::stepEndRoll() {
    EndRoll &r = endRoll_;
    // A roll whose wait ended early (a trap, a load) stops where it is. A dialog a callback opens over it keeps the
    // roll's wait to put back, and the roll goes on.
    bool ours = cur_.pos == r.pos && ((wait_ == WaitKind::Timer && waitUntil_ == r.end) ||
                                      (wait_ == WaitKind::Dialog && ui_ && ui_->prevWait == WaitKind::Timer &&
                                       ui_->prevWaitUntil == r.end));
    if (r.end > r.start && !ours) {
        r.on = false;
        return;
    }
    int64_t moved = now_ >= r.end ? r.distance : std::min(r.distance, (now_ - r.start) * 10 / r.speed);
    int64_t v[8];
    std::copy(r.rect, r.rect + 8, v);
    v[5] += moved;
    bltLayer_ = Image(width(), height(), 0);
    bltState_ = 1;
    bltRect(v);
    if (moved >= r.distance) r.on = false;
}

const std::unordered_map<std::string, CommandFn> &ponscripterCommandTable() {
    static const std::unordered_map<std::string, CommandFn> table = {
        {"langen", cmdLang},          {"langjp", cmdLang},       {"langall", cmdLang},
        {"showlangen", cmdShowLang},  {"showlangjp", cmdShowLang}, {"getreadlang", cmdGetReadLang},
        {"language", cmdLanguage},    {"h_mapfont", cmdMapFont}, {"h_rendering", cmdRendering},
        {"steamsetachieve", cmdSteamAchievement}, {"tachistate", cmdTachiState}, {"endroll", cmdEndRoll},
        {"drawclear", cmdDrawClear},  {"drawsp", cmdDrawSp},     {"draw", cmdDraw},
        {"logsp", cmdLogSp},
    };
    return table;
}

}  // namespace vnns
