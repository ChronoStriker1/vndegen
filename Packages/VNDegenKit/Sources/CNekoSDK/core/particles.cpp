// The `particles` game part: the older engine's particles (95 starts an emitter, 96 ends one), measured under Wine on
// Test Game 83's ep_05_05_00, where one emitter fills the screen with about a hundred glowing dots.
//
// What the PC showed there, from 70 frames over 12 seconds: dots of the emitter's three pictures in equal numbers,
// all over the screen from the first frame. Each pixel is added to the scene times its own brightness, its brightest
// channel: the green dots' levels 27, 54, 88, 130 and 186 show as 3, 11, 30, 66 and 136, their squares over 255, so
// the pictures' dim edges vanish and no box shows round a spark. Each dot rises the whole height at its own 48 to 60 pixels
// a second, sways sideways some 40 to 90 pixels over tens of seconds, and brightens and dims slowly. They draw at the
// emitter's slot, parameter 0: over the standing pictures in 300 and 400, under the fog in 3333. An emitter that names
// a picture the game doesn't have makes nothing, as 83's second one (parts\p000_000_000.bmp) and Test Games 84 and 89's
// show. How many there are is a reading of one emitter (parameter 1 every parameter 2 milliseconds over parameter 7,
// times five eighths, 78 there, which matches the PC's count of dots in its frames); the PC's own rule isn't known.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

#include "parts.h"

namespace neko {
namespace {

constexpr float kTwoPi = 6.2831853f;

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
            emitters_.erase(n.p[0]);
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
        return !emitters_.empty();
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int fromSlot, int toSlot) override {
        if (layer != Layer::Slot) return;
        for (auto it = emitters_.lower_bound(fromSlot); it != emitters_.end() && it->first < toSlot; ++it) drawEmitter(host, out, it->second);
    }

    // VNDegen's own saves keep each emitter's command, and a load starts its dots afresh, as the PC's record holds no dot.
    void save(std::vector<std::string> &lines) const override {
        for (const auto &[slot, e] : emitters_) {
            std::string line = "emitter " + std::to_string(slot);
            for (int v : e.p) line += " " + std::to_string(v);
            for (const std::string &name : e.names) line += " " + name;
            lines.push_back(line);
        }
    }
    void load(const std::vector<std::string> &lines) override {
        pending_.clear();
        for (const std::string &line : lines) {
            std::istringstream in(line);
            std::string word;
            Pending e;
            if (!(in >> word >> e.slot) || word != "emitter") continue;
            for (int &v : e.p) in >> v;
            for (std::string name; in >> name;) e.names.push_back(name);
            pending_.push_back(e);
        }
    }
    void reset() override { emitters_.clear(); }

private:
    struct Pending {
        int slot = 0;
        int p[10] = {};
        std::vector<std::string> names;
    };

    void start(PartHost &host, int slot, const std::vector<std::string> &names, const int p[10], int64_t now) {
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
        int every = std::max(1, p[2]);
        int count = std::min(400, std::max(1, p[1] * p[7] / every * 5 / 8));
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

    void drawEmitter(PartHost &host, Canvas &out, const Emitter &e) {
        float t = float(host.nowMs() - e.bornMs) / 1000;
        if (t < 0) t = 0;
        // They come up over the first half second.
        float fadeIn = std::min(1.0f, t / 0.5f);
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
            drawImage(out, picture, x, y, 1, 1, 0, glow * fadeIn, nullptr, true);
        }
    }

    // After load(), the emitters to start once a host is there to read their pictures: at the next step.
    void startPending(PartHost &host) {
        for (const Pending &e : pending_) start(host, e.slot, e.names, e.p, host.nowMs());
        pending_.clear();
    }

    std::map<int, Emitter> emitters_;
    std::vector<Pending> pending_;
};

}  // namespace

std::unique_ptr<GamePart> makeParticlesPart() { return std::make_unique<ParticlesPart>(); }

}  // namespace neko
