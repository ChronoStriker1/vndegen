// Movie libraries: DrawMovie2 (Test Game 19) and DrawMovie3 (Test Game 56) draw a movie's frames into a sprite the
// game made, one Draw call per frame; PlayMovie (Test Game 46) plays a movie over the whole screen while the game
// polls IsPlay. Behaviour read from how the games call them: the argument names and the game functions that wrap
// the calls. The movie's sound is a stream in the mixer's music group, so the game's music volume applies to it.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <memory>

#include "game.h"
#include "movie.h"
#include "save.h"

namespace s4 {
namespace {

namespace fs = std::filesystem;

Game &G() { return game(); }

constexpr int kMusicGroup = 1;

// Some copies of the games carry their movie under a romanized name (Test Game 19 asks for "ランス０２.alm" and has
// Rance02.alm). When the named file is missing and its folder holds exactly one movie, that is the one.
std::string moviePath(const std::string &name) {
    std::string path = save::hostPath(*G().vm, name, false);
    std::error_code ec;
    if (fs::exists(path, ec)) return path;
    fs::path dir = fs::path(path).parent_path();
    std::string found;
    for (auto &e : fs::directory_iterator(dir, ec)) {
        std::string ext = e.path().extension().string();
        for (char &ch : ext) ch = char(std::tolower(static_cast<unsigned char>(ch)));
        if (ext != ".alm") continue;
        if (!found.empty()) return path;
        found = e.path().string();
    }
    if (found.empty()) return path;
    G().log("movie: " + path + " is missing; playing " + found);
    return found;
}

struct Playback {
    std::unique_ptr<Movie> movie;
    uint32_t startMs = 0;
    bool running = false;
    int volume = 100;
    int count = 0;  // frames shown, which the game reads to measure the frame rate
    uint32_t lastShownMs = 0, longestGapMs = 0;  // game time between new frames, for the close log

    bool load(const std::string &name) {
        release();
        auto m = std::make_unique<Movie>();
        std::string path = moviePath(name);
        if (!m->open(path)) {
            G().log("movie: can't open " + path);
            return false;
        }
        movie = std::move(m);
        return true;
    }
    bool run() {
        if (!movie) return false;
        startMs = G().now();
        running = true;
        G().log("movie: playing");
        Movie *m = movie.get();
        G().audio.setStream([m](float *out, int frames) { return m->readAudio(out, frames); }, kMusicGroup, volume);
        return true;
    }
    // Decodes up to the game clock. True when the frame changed.
    bool advance() {
        if (!movie || !running) return false;
        bool changed = movie->advanceTo((G().now() - startMs) / 1000.0);
        if (changed) {
            uint32_t at = G().now();
            if (count > 0) longestGapMs = std::max(longestGapMs, at - lastShownMs);
            lastShownMs = at;
            count++;
        }
        return changed;
    }
    bool ended() { return !movie || (running && movie->ended()); }
    void setVolume(int v) {
        volume = std::clamp(v, 0, 100);
        if (running) G().audio.setStreamVolume(volume);
    }
    void release() {
        if (running) G().audio.clearStream();
        if (movie && running) {
            Movie::Dry d = movie->dry();
            double seconds = (G().now() - startMs) / 1000.0;
            char rate[160];
            std::snprintf(rate, sizeof rate, "showed %d frames, %.2f a second against the movie's %.2f, longest gap %u ms", count,
                          seconds > 0 ? count / seconds : 0.0, movie->frameRate(), longestGapMs);
            G().log("movie: closed after " + std::to_string(int(seconds)) + " s of game time; " + rate + "; its sound ran dry " +
                    std::to_string(d.reads) + " times, " + std::to_string(d.frames * 1000 / Movie::kAudioRate) + " ms in all");
        }
        running = false;
        movie.reset();
        count = 0;
        lastShownMs = longestGapMs = 0;
    }
};

Playback g_draw, g_play;
S4_HLL_RESET {
    g_draw.release();
    g_play.release();
}

// Scales the movie frame into an RGBA destination, bilinear.
void scaleInto(uint8_t *dst, int dw, int dh, const Movie &m) {
    const uint8_t *src = m.frame().data();
    int sw = m.width(), sh = m.height();
    if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
    if (sw == dw && sh == dh) {
        std::copy(src, src + size_t(sw) * sh * 4, dst);
        return;
    }
    for (int y = 0; y < dh; y++) {
        float fy = std::max(0.0f, (y + 0.5f) * sh / dh - 0.5f);
        int y0 = std::min(int(fy), sh - 1), y1 = std::min(y0 + 1, sh - 1);
        int wy = int((fy - y0) * 256);
        for (int x = 0; x < dw; x++) {
            float fx = std::max(0.0f, (x + 0.5f) * sw / dw - 0.5f);
            int x0 = std::min(int(fx), sw - 1), x1 = std::min(x0 + 1, sw - 1);
            int wx = int((fx - x0) * 256);
            const uint8_t *a = src + (size_t(y0) * sw + x0) * 4, *b = src + (size_t(y0) * sw + x1) * 4;
            const uint8_t *c = src + (size_t(y1) * sw + x0) * 4, *d = src + (size_t(y1) * sw + x1) * 4;
            uint8_t *o = dst + (size_t(y) * dw + x) * 4;
            for (int k = 0; k < 3; k++) {
                int top = a[k] * (256 - wx) + b[k] * wx, bottom = c[k] * (256 - wx) + d[k] * wx;
                o[k] = uint8_t((top * (256 - wy) + bottom * wy) >> 16);
            }
            o[3] = 255;
        }
    }
}

// DrawMovie2 and DrawMovie3 share their calls; DrawMovie3's UpdateVolume takes a mute-when-inactive flag that
// VNDegen has no use for.
#define DRAW_MOVIE(lib)                                                                          \
    S4_HLL(lib, Load) { c.retBool(g_draw.load(c.s(0))); }                                        \
    S4_HLL(lib, Run) { c.retBool(g_draw.run()); }                                                \
    S4_HLL(lib, Draw) {                                                                          \
        Sprite *s = G().sprites.find(c.i(0));                                                    \
        if (!s || !g_draw.movie) { c.retBool(false); return; }                                   \
        if (g_draw.advance()) {                                                                  \
            Surface &d = s->writable();                                                          \
            scaleInto(d.px.data(), d.w, d.h, *g_draw.movie);                                     \
        }                                                                                        \
        c.retBool(true);                                                                         \
    }                                                                                            \
    S4_HLL(lib, IsEnd) { c.retBool(g_draw.ended()); }                                            \
    S4_HLL(lib, GetCount) { c.retInt(g_draw.count); }                                            \
    S4_HLL(lib, SetInnerVolume) { g_draw.setVolume(c.i(0)); }                                    \
    S4_HLL(lib, UpdateVolume) {}                                                                 \
    S4_HLL(lib, Release) { g_draw.release(); }

DRAW_MOVIE(DrawMovie2)
DRAW_MOVIE(DrawMovie3)

S4_HLL(PlayMovie, Init) { c.retInt(1); }
S4_HLL(PlayMovie, Load) { c.retInt(g_play.load(c.s(0)) ? 1 : 0); }
S4_HLL(PlayMovie, Play) { c.retInt(g_play.run() ? 1 : 0); }
S4_HLL(PlayMovie, Stop) { g_play.release(); }
// The game polls this every frame; each poll shows the movie's current frame over the screen, letterboxed.
S4_HLL(PlayMovie, IsPlay) {
    if (g_play.ended()) { c.retInt(0); return; }
    if (g_play.advance()) {
        const Movie &m = *g_play.movie;
        int sw = G().screenW, sh = G().screenH;
        int w = sw, h = int(int64_t(sw) * m.height() / std::max(1, m.width()));
        if (h > sh) { h = sh; w = int(int64_t(sh) * m.width() / std::max(1, m.height())); }
        std::vector<uint8_t> scaled(size_t(w) * h * 4);
        scaleInto(scaled.data(), w, h, m);
        std::vector<uint8_t> f(size_t(sw) * sh * 4, 0);
        for (size_t i = 3; i < f.size(); i += 4) f[i] = 255;
        int ox = (sw - w) / 2, oy = (sh - h) / 2;
        for (int y = 0; y < h; y++)
            std::copy_n(scaled.data() + size_t(y) * w * 4, size_t(w) * 4, f.data() + (size_t(y + oy) * sw + ox) * 4);
        G().showFrame(std::move(f));
    }
    c.retInt(1);
}
S4_HLL(PlayMovie, Release) { g_play.release(); }

}  // namespace
}  // namespace s4
