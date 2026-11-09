// The `particles` game part: the older engine's particles (95 starts an emitter, 96 ends one), measured under Wine on
// Test Game 83's ep_05_05_00, where one emitter fills the screen with about a hundred glowing dots.
//
// What the PC showed there, from 70 frames over 12 seconds: dots of the emitter's three pictures in equal numbers,
// all over the screen from the first frame. Each pixel is added to the scene times its own brightness, its brightest
// channel: the green dots' levels 27, 54, 88, 130 and 186 show as 3, 11, 30, 66 and 136, their squares over 255, so
// the pictures' dim edges vanish and no box shows round a spark. Each dot rises the whole height at its own 48 to 60
// pixels a second, sways sideways some 40 to 90 pixels over tens of seconds, and brightens and dims slowly. The emitter
// is a record in its slot, parameter 0, as the PC's save holds it (kind 15, its three pictures and its numbers): the
// dots draw there, over the standing pictures in 300 and 400 and under the fog in 3333, and what takes a picture away
// takes it away. A save holds no dot, so a load starts them afresh. An emitter that names a picture the game doesn't
// have makes nothing, as 83's second one (parts\p000_000_000.bmp) and Test Games 84 and 89's show. How many there are
// is a reading of one emitter (parameter 1 every parameter 2 milliseconds over parameter 7, times five eighths, 78
// there, which matches the PC's count of dots in its frames); the PC's own rule isn't known.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

#include "parts.h"
#include "save.h"

namespace neko {
namespace {

constexpr float kTwoPi = 6.2831853f;
// The number the PC's save holds after an emitter's slot.
constexpr int kEmitterKind = 15;
// Where the record keeps the command's numbers, in its fixed part with the three names taken out (Test Game 83's PC
// save): parameter 1, then parameters 2 and 7 as seconds, 8, 9 and 3 as they are, and 4 and 5 as floats.
constexpr int kAtCount = 53, kAtEvery = 77, kAtLife = 81, kAtP8 = 85, kAtP9 = 89, kAtP3 = 93, kAtP4 = 97, kAtP5 = 101;

struct Dot {
    size_t picture = 0;
    float x = 0, y = 0;                    // where it started
    float rise = 0;                        // pixels a second, upward
    float sway = 0, swayPeriod = 1, swayPhase = 0;
    float glowPeriod = 1, glowPhase = 0;
};

struct Emitter {
    std::vector<std::string> names;
    std::vector<ImageRef> pictures;
    int p[10] = {};
    int64_t bornMs = 0;
    std::vector<Dot> dots;
};

// A small generator of its own, so an emitter's dots come out the same on every run and every host.
struct Random {
    uint32_t state;
    float next() {  // 0 to 1
        state = state * 1664525u + 1013904223u;
        return float(state >> 8) / float(1u << 24);
    }
    float between(float low, float high) { return low + (high - low) * next(); }
};

class ParticlesPart : public GamePart {
public:
    bool command(PartHost &host, const Node &n) override {
        if (n.opcode == 96) {
            // Its dots go with it at once; how the PC lets them die away hasn't been seen (Test Game 86 uses it). Its
            // slot is let go too, so a save made after this holds no record of an emitter that isn't there.
            if (emitters_.erase(n.p[0])) host.releaseSlot(n.p[0]);
            host.redraw();
            return true;
        }
        if (n.opcode != 95) return false;
        std::vector<std::string> names;
        for (int i = 1; i <= 3; i++)
            if (!n.s[size_t(i)].empty()) names.push_back(n.s[size_t(i)]);
        int p[10];
        for (int i = 0; i < 10; i++) p[i] = n.p[i];
        start(host, n.p[0], names, p, host.nowMs());
        return true;
    }

    bool step(PartHost &host) override {
        if (!pending_.empty()) startPending(host);
        // An emitter whose slot the scene has cleared is over.
        for (auto it = emitters_.begin(); it != emitters_.end();)
            it = host.slotHeld(it->first) ? std::next(it) : emitters_.erase(it);
        return !emitters_.empty();
    }

    void drawSlot(PartHost &host, Canvas &out, int slot, float alpha) override {
        // With Config's Effects off the PC shows no dot at all; the emitter stays, and its record in a save.
        auto it = emitters_.find(slot);
        if (it != emitters_.end() && alpha > 0 && host.effectsOn()) drawEmitter(host, out, it->second, alpha);
    }

    // The PC's record of each emitter: the engine writes the slot's own, and this fills in the pictures and the
    // numbers.
    void savePC(SaveData &d) const override {
        for (SavedSprite &s : d.sprites) {
            if (s.kind != kEmitterKind || !s.path.empty()) continue;
            if (auto it = emitters_.find(s.slot); it != emitters_.end()) {
                fill(s, it->second.names, it->second.p);
                continue;
            }
            // Straight after a load the emitters still wait for their first step, and a save made then keeps them.
            for (const Pending &e : pending_)
                if (e.slot == s.slot) fill(s, e.names, e.p);
        }
    }
    void loadPC(const SaveData &d) override {
        emitters_.clear();
        pending_.clear();
        for (const SavedSprite &s : d.sprites) {
            if (s.kind != kEmitterKind || !s.path.empty() || s.rawFixed.size() < size_t(kAtP5) + 4) continue;
            Pending e;
            e.slot = s.slot;
            for (const std::string &name : s.names)
                if (!name.empty()) e.names.push_back(name);
            auto i = [&](int at) { int32_t v; std::memcpy(&v, &s.rawFixed[size_t(at)], 4); return int(v); };
            auto f = [&](int at) { float v; std::memcpy(&v, &s.rawFixed[size_t(at)], 4); return v; };
            e.p[0] = s.slot;
            e.p[1] = i(kAtCount);
            e.p[2] = whole(f(kAtEvery) * 1000);
            e.p[7] = whole(f(kAtLife) * 1000);
            e.p[8] = i(kAtP8);
            e.p[9] = i(kAtP9);
            e.p[3] = i(kAtP3);
            e.p[4] = whole(f(kAtP4));
            e.p[5] = whole(f(kAtP5));
            pending_.push_back(e);
        }
    }

    void reset() override {
        emitters_.clear();
        pending_.clear();
    }

private:
    static void fill(SavedSprite &s, const std::vector<std::string> &names, const int p[10]) {
        for (size_t i = 0; i < 3; i++) s.names[i] = i < names.size() ? names[i] : "";
        s.fixedBytes = {
            {kAtCount, int32(p[1])},
            {kAtEvery, float32(float(p[2]) / 1000)},
            {kAtLife, float32(float(p[7]) / 1000)},
            {kAtP8, int32(p[8])},
            {kAtP9, int32(p[9])},
            {kAtP3, int32(p[3])},
            {kAtP4, float32(float(p[4]))},
            {kAtP5, float32(float(p[5]))},
        };
    }
    // A float from a save as a whole number: a save's bytes may be anything.
    static int whole(float v) {
        if (!std::isfinite(v)) return 0;
        return int(std::lround(std::clamp(v, -1.0e9f, 1.0e9f)));
    }
    static std::vector<uint8_t> int32(int32_t v) {
        std::vector<uint8_t> b(4);
        std::memcpy(b.data(), &v, 4);
        return b;
    }
    static std::vector<uint8_t> float32(float v) {
        std::vector<uint8_t> b(4);
        std::memcpy(b.data(), &v, 4);
        return b;
    }

    struct Pending {
        int slot = 0;
        int p[10] = {};
        std::vector<std::string> names;
    };

    void start(PartHost &host, int slot, const std::vector<std::string> &names, const int p[10], int64_t now,
               bool hold = true) {
        emitters_.erase(slot);
        Emitter e;
        e.names = names;
        for (const std::string &name : names) {
            ImageRef picture = host.image(name);
            if (!picture) {
                host.log("particles: no " + name + ", so this emitter makes nothing");
                return;
            }
            e.pictures.push_back(glowing(*picture));
        }
        if (e.pictures.empty()) return;
        for (int i = 0; i < 10; i++) e.p[i] = p[i];
        e.bornMs = now;
        // A script's numbers may be anything, so the sum is done wide and cut at 640 before the five eighths: 1 to 400
        // dots.
        int64_t made = int64_t(std::max(0, p[1])) * std::max(0, p[7]) / std::max(1, p[2]);
        int count = std::max(1, int(std::min<int64_t>(made, 640)) * 5 / 8);
        Random random{uint32_t(slot) * 2654435761u + 12345u};
        float w = float(host.width()), h = float(host.height());
        for (int i = 0; i < count; i++) {
            Dot d;
            d.picture = size_t(i) % e.pictures.size();
            d.x = random.between(0, w);
            d.y = random.between(0, h);
            d.rise = random.between(48, 60);
            d.sway = random.between(40, 90);
            d.swayPeriod = random.between(16, 32);
            d.swayPhase = random.between(0, kTwoPi);
            d.glowPeriod = random.between(4, 9);
            d.glowPhase = random.between(0, kTwoPi);
            e.dots.push_back(d);
        }
        // From a command the slot is taken now; after a load the engine has brought it back already.
        if (hold) host.holdSlot(slot, kEmitterKind);
        emitters_[slot] = std::move(e);
        host.redraw();
    }

    // The picture as the PC adds it: each pixel times its brightest channel over 255.
    static ImageRef glowing(const Image &picture) {
        auto out = std::make_shared<Image>(picture);
        for (size_t i = 0; i + 3 < out->rgba.size(); i += 4) {
            uint8_t *px = &out->rgba[i];
            int brightest = std::max(px[0], std::max(px[1], px[2]));
            for (int c = 0; c < 3; c++) px[c] = uint8_t((px[c] * brightest + 127) / 255);
        }
        return out;
    }

    void drawEmitter(PartHost &host, Canvas &out, const Emitter &e, float alpha) {
        float t = float(host.nowMs() - e.bornMs) / 1000;
        if (t < 0) t = 0;
        float h = float(host.height());
        for (const Dot &d : e.dots) {
            const Image &picture = *e.pictures[d.picture];
            // Off the top, a dot comes round again from below the bottom.
            float span = h + float(picture.h);
            float y = std::fmod(d.y - d.rise * t + float(picture.h) / 2, span);
            if (y < 0) y += span;
            y -= float(picture.h) / 2;
            float x = d.x + d.sway * std::sin(kTwoPi * t / d.swayPeriod + d.swayPhase);
            float glow = 0.78f + 0.22f * std::sin(kTwoPi * t / d.glowPeriod + d.glowPhase);
            drawImage(out, picture, x, y, 1, 1, 0, glow * alpha, nullptr, true);
        }
    }

    // After load(), the emitters to start once a host is there to read their pictures: at the next step.
    void startPending(PartHost &host) {
        for (const Pending &e : pending_) start(host, e.slot, e.names, e.p, host.nowMs(), false);
        pending_.clear();
    }

    std::map<int, Emitter> emitters_;
    std::vector<Pending> pending_;
};

}  // namespace

std::unique_ptr<GamePart> makeParticlesPart() { return std::make_unique<ParticlesPart>(); }

}  // namespace neko
