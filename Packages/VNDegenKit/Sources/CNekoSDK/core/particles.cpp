// The `particles` game part: the older engine's particles (95 starts an emitter, 96 ends one), measured under Wine on
// Test Game 83's ep_05_05_00.
//
// Each pixel is added to the scene times its own brightness, its brightest
// channel: the green dots' levels 27, 54, 88, 130 and 186 show as 3, 11, 30, 66 and 136, their squares over 255, so
// the pictures' dim edges vanish and no box shows round a spark. The emitter
// is a record in its slot, parameter 0, as the PC's save holds it (kind 15, its three pictures and its numbers).
// A save holds no dot, so a load starts them afresh. An emitter that names a picture the game doesn't
// have makes nothing, as 83's second one (parts\p000_000_000.bmp) and Test Games 84 and 89's show.
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
constexpr float kShare = 0.625f;
// The number the PC's save holds after an emitter's slot.
constexpr int kEmitterKind = 15;
// Where the record keeps the command's numbers, in its fixed part with the three names taken out (Test Game 83's PC
// save): parameter 1, then parameters 2 and 7 as seconds, 8, 9 and 3 as they are, and 4 and 5 as floats.
constexpr int kAtCount = 53, kAtEvery = 77, kAtLife = 81, kAtP8 = 85, kAtP9 = 89, kAtP3 = 93, kAtP4 = 97, kAtP5 = 101;

struct Emitter {
    std::vector<std::string> names;
    std::vector<ImageRef> pictures;
    int p[10] = {};
    int64_t bornMs = 0;
    int64_t stoppedMs = -1;
    uint32_t seed = 0;

    int perBatch() const { return std::clamp(p[1], 1, 64); }
    int lifeMs() const { return std::clamp(p[7], 100, 30000); }
    int everyMs() const {
        int64_t every = std::clamp(p[2], 16, 60000);
        return int(std::max<int64_t>(every, int64_t(perBatch()) * lifeMs() / 400));
    }
    bool over(int64_t now) const { return stoppedMs >= 0 && now - stoppedMs >= lifeMs(); }
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
            if (auto it = emitters_.find(n.p[0]); it != emitters_.end() && it->second.stoppedMs < 0)
                it->second.stoppedMs = host.nowMs();
            host.releaseSlot(n.p[0]);
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
        int64_t now = host.nowMs();
        for (auto it = emitters_.begin(); it != emitters_.end();) {
            if (host.slotHeld(it->first)) {
                ++it;
                continue;
            }
            if (!it->second.pictures.empty()) retire(std::move(it->second), now);
            it = emitters_.erase(it);
        }
        auto dead = std::remove_if(ending_.begin(), ending_.end(), [&](const Emitter &e) { return e.over(now); });
        ending_.erase(dead, ending_.end());
        return !emitters_.empty() || !ending_.empty();
    }

    void drawSlot(PartHost &host, Canvas &out, int slot, float alpha) override {
        // With Config's Effects off the PC shows no dot at all; the emitter stays, and its record in a save.
        auto it = emitters_.find(slot);
        if (it == emitters_.end() || alpha <= 0 || !host.effectsOn() || it->second.stoppedMs >= 0) return;
        drawEmitter(host, out, it->second, alpha);
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        if (layer != Layer::UnderWindow || !host.effectsOn()) return;
        for (const auto &[slot, e] : emitters_)
            if (e.stoppedMs >= 0) drawEmitter(host, out, e, 1);
        for (const Emitter &e : ending_) drawEmitter(host, out, e, 1);
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
        ending_.clear();
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
        ending_.clear();
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
        Emitter e;
        e.names = names;
        for (int i = 0; i < 10; i++) e.p[i] = p[i];
        e.bornMs = now;
        bool whole = !names.empty();
        for (const std::string &name : names) {
            ImageRef picture = host.image(name);
            if (!picture) {
                host.log("particles: no " + name + ", so this emitter makes nothing");
                whole = false;
                break;
            }
            e.pictures.push_back(glowing(*picture));
        }
        if (!whole) {
            // From a command nothing changes: no slot is taken, and an emitter already in the slot goes on. From a
            // load the engine has brought the slot back, so its names and numbers are kept, drawing nothing, and the
            // next save writes the record as it was read.
            if (!hold) {
                e.pictures.clear();
                emitters_[slot] = std::move(e);
            }
            return;
        }
        e.seed = uint32_t(slot) * 2654435761u + uint32_t(now) * 40503u + 12345u;
        if (auto old = emitters_.find(slot); old != emitters_.end() && !old->second.pictures.empty())
            retire(std::move(old->second), now);
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

    void retire(Emitter &&e, int64_t now) {
        if (e.stoppedMs < 0) e.stoppedMs = now;
        if (!e.over(now)) ending_.push_back(std::move(e));
    }

    void drawEmitter(PartHost &host, Canvas &out, const Emitter &e, float alpha) {
        if (e.pictures.empty()) return;
        int64_t now = host.nowMs();
        int64_t t = std::max<int64_t>(0, now - e.bornMs);
        int64_t emitted = e.stoppedMs >= 0 ? std::max<int64_t>(0, e.stoppedMs - e.bornMs) : t;
        int every = e.everyMs(), life = e.lifeMs(), perBatch = e.perBatch();
        int64_t first = t >= life ? (t - life) / every + 1 : 0, last = std::min(t, emitted) / every;
        float w = float(host.width()), h = float(host.height());
        float seconds = float(t) / 1000;
        for (int64_t batch = first; batch <= last; batch++) {
            int64_t age = t - batch * every;
            if (age < 0 || age >= life) continue;
            float u = float(age) / float(life);
            float shown = std::min(1.0f, u / 0.05f) * std::min(1.0f, 0.7f + 0.6f * u) * std::min(1.0f, (1 - u) / 0.1f);
            for (int i = 0; i < perBatch; i++) {
                Random random{e.seed ^ (uint32_t(batch) * 0x9E3779B1u) ^ (uint32_t(i + 1) * 0x85EBCA6Bu)};
                if (random.next() >= kShare) continue;
                const Image &picture = *e.pictures[size_t(batch * perBatch + i) % e.pictures.size()];
                float x = random.between(0, w), y = random.between(0, h);
                float rise = random.between(48, 60);
                float sway = random.between(40, 90), swayPeriod = random.between(16, 32);
                float swayPhase = random.between(0, kTwoPi);
                y -= rise * float(age) / 1000;
                x += sway * std::sin(kTwoPi * seconds / swayPeriod + swayPhase);
                drawImage(out, picture, x, y, 1, 1, 0, shown * alpha, nullptr, true);
            }
        }
    }

    // After load(), the emitters to start once a host is there to read their pictures: at the next step.
    void startPending(PartHost &host) {
        for (const Pending &e : pending_) start(host, e.slot, e.names, e.p, host.nowMs(), false);
        pending_.clear();
    }

    std::map<int, Emitter> emitters_;
    std::vector<Emitter> ending_;
    std::vector<Pending> pending_;
};

}  // namespace

std::unique_ptr<GamePart> makeParticlesPart() { return std::make_unique<ParticlesPart>(); }

}  // namespace neko
