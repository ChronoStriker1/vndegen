// Test Game 90 (Mankitsu Happening)'s face pictures and scene-change slides, commands 130 to 145 and 160 to 163, which
// no other game uses. Measured on the PC game under Wine (set 1), from 30 fps recordings of its first scenes.
//
// Faces: 130 loads an `ac\` picture into a face slot (parameter 2, 2000), 131 removes one (parameter 0) and 138 all of
// them, and 132 shows what's loaded, fading over parameter 1 ms as a screen update does. A face sits at the message
// window's top left, (0, 512), over its plate and under its text, exactly where the PC draws it, and goes with the
// window. 136 (moving a face), 139 (a face flipbook) and 145 (a face shake) are taken and not yet drawn.
//
// Slides: 160 names two panel pictures (s1, s2), 162 starts bars of them sliding in from the left until they cover the
// screen, 161 waits until they do, while the scene changes behind them, and 163 slides them off to the right and waits
// for that. On the PC the bars' heights and speeds vary from slide to slide, as if random; these are drawn in the same
// style over the times the recording shows (in over 0.6 s, out over 0.8 s), not the PC's own bars. Unconfirmed.
#include <algorithm>
#include <map>
#include <sstream>

#include "parts.h"

namespace neko {
namespace {

constexpr int kFaceX = 0, kFaceY = 512;
constexpr int kFollowMs = 120;  // the second strip's lag behind the first, in and out

struct Bar {
    int y = 0, h = 0;
    int inDelay = 0, inMs = 0, outDelay = 0, outMs = 0;
};

class MankitsuPart : public GamePart {
public:
    bool command(PartHost &host, const Node &n) override {
        switch (n.opcode) {
        case 130: loaded_[n.p[2]] = n.s[1]; return true;
        case 131: loaded_.erase(n.p[0]); return true;
        case 138: loaded_.clear(); return true;
        case 132:
            from_ = shown_;
            shown_ = loaded_;
            fadeFrom_ = host.nowMs();
            fadeMs_ = std::max(0, int(n.p[1]));
            host.redraw();
            return true;
        case 136: case 139: case 145:
            if (!warned_) host.log("Test Game 90's face move, flipbook and shake aren't drawn yet");
            warned_ = true;
            return true;
        case 160:
            panels_[0] = n.s[1];
            panels_[1] = n.s[2];
            holdMs_ = std::max(0, int(n.p[14]));
            return true;
        case 162: startSlide(host); return true;
        case 161:
            if (int left = int(slideFrom_ + coverMs() - host.nowMs()); left > 0) host.waitMs(left);
            return true;
        case 163:
            // The bars go no sooner than parameter 14 after they started (1000 in every slide), as the PC's left about a
            // second after theirs came in; read so, unconfirmed.
            outFrom_ = std::max(host.nowMs(), slideFrom_ + holdMs_);
            host.waitMs(int(outFrom_ - host.nowMs()) + uncoverMs());
            return true;
        default: return false;
        }
    }

    bool step(PartHost &host) override {
        int64_t now = host.nowMs();
        bool fading = now - fadeFrom_ < fadeMs_;
        bool sliding = slideFrom_ >= 0 && (outFrom_ < 0 || now - outFrom_ < uncoverMs());
        if (outFrom_ >= 0 && now - outFrom_ >= uncoverMs()) slideFrom_ = outFrom_ = -1;
        return fading || sliding;
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        if (layer == Layer::Window) drawFaces(host, out);
        if (layer == Layer::OverWindow) drawSlide(host, out);
    }

    void save(std::vector<std::string> &lines) const override {
        for (const auto &[slot, path] : shown_) lines.push_back("face " + std::to_string(slot) + " " + path);
    }

    void load(const std::vector<std::string> &lines) override {
        reset();
        for (const std::string &line : lines) {
            std::istringstream in(line);
            std::string word, path;
            int slot = 0;
            if (in >> word >> slot >> path && word == "face") shown_[slot] = loaded_[slot] = path;
        }
    }

    void reset() override {
        loaded_.clear();
        shown_.clear();
        from_.clear();
        fadeMs_ = 0;
        slideFrom_ = outFrom_ = -1;
    }

private:
    void drawFaces(PartHost &host, Canvas &out) {
        float t = fadeMs_ > 0 ? std::clamp(float(host.nowMs() - fadeFrom_) / float(fadeMs_), 0.0f, 1.0f) : 1.0f;
        auto drawSet = [&](const std::map<int, std::string> &set, float alpha) {
            if (alpha <= 0) return;
            for (const auto &[slot, path] : set)
                if (ImageRef img = host.image(path)) drawPart(out, *img, kFaceX, kFaceY, 0, 0, img->w, img->h, alpha);
        };
        // A face that stays is drawn once, in full; one going fades out as one coming fades in.
        std::map<int, std::string> going, coming, staying;
        for (const auto &[slot, path] : from_) (shown_.count(slot) && shown_.at(slot) == path ? staying : going)[slot] = path;
        for (const auto &[slot, path] : shown_)
            if (!staying.count(slot)) coming[slot] = path;
        drawSet(staying, 1);
        drawSet(going, 1 - t);
        drawSet(coming, t);
    }

    void startSlide(PartHost &host) {
        bars_.clear();
        // A fresh pattern for each slide, as the PC's differ, from a seed the slide's start gives.
        uint32_t seed = uint32_t(++slides_ * 2654435761u);
        auto rnd = [&](int lo, int hi) {
            seed = seed * 1664525u + 1013904223u;
            return lo + int((seed >> 8) % uint32_t(hi - lo + 1));
        };
        for (int y = 0; y < host.height();) {
            Bar b;
            b.y = y;
            b.h = std::min(rnd(12, 64), host.height() - y);
            b.inDelay = rnd(0, 200);
            b.inMs = rnd(300, 400);
            b.outDelay = rnd(0, 300);
            b.outMs = rnd(350, 500);
            bars_.push_back(b);
            y += b.h;
        }
        slideFrom_ = host.nowMs();
        outFrom_ = -1;
        host.redraw();
    }

    int coverMs() const {
        int ms = 0;
        for (const Bar &b : bars_) ms = std::max(ms, b.inDelay + b.inMs + kFollowMs);
        return ms;
    }

    int uncoverMs() const {
        int ms = 0;
        for (const Bar &b : bars_) ms = std::max(ms, b.outDelay + b.outMs + kFollowMs);
        return ms;
    }

    void drawSlide(PartHost &host, Canvas &out) {
        if (slideFrom_ < 0) return;
        int64_t now = host.nowMs();
        int w = host.width();
        // Each bar is a strip of the second panel (orange) with one of the first (yellow) kFollowMs behind it, coming in
        // and going out, so the screen is all the first while covered, as the PC's is.
        ImageRef lead = host.image(panels_[1]), follow = host.image(panels_[0]);
        auto progress = [](int64_t ms, int length) {
            float t = std::clamp(float(ms) / float(std::max(1, length)), 0.0f, 1.0f);
            return 1 - (1 - t) * (1 - t);
        };
        for (const Bar &b : bars_) {
            for (int layer = 0; layer < 2; layer++) {
                const ImageRef &img = layer == 0 ? lead : follow;
                if (!img) continue;
                // In, the strip's right edge runs from the screen's left to its right; out, its left edge does.
                int64_t inMs = now - slideFrom_ - b.inDelay - (layer == 1 ? kFollowMs : 0);
                int64_t outMs = outFrom_ < 0 ? -1 : now - outFrom_ - b.outDelay - (layer == 0 ? kFollowMs : 0);
                int left = int(progress(outMs, b.outMs) * w), right = int(progress(inMs, b.inMs) * w);
                if (right <= left) continue;
                float sx = float(right - left) / float(img->w), sy = float(b.h) / float(img->h);
                drawImage(out, *img, (left + right) / 2.0f, b.y + b.h / 2.0f, sx, sy, 0, 1);
            }
        }
    }

    std::map<int, std::string> loaded_, shown_, from_;
    int64_t fadeFrom_ = 0;
    int fadeMs_ = 0;
    std::string panels_[2];
    int holdMs_ = 0;
    std::vector<Bar> bars_;
    int64_t slideFrom_ = -1, outFrom_ = -1;
    uint32_t slides_ = 0;
    bool warned_ = false;
};

}  // namespace

std::unique_ptr<GamePart> makeMankitsuPart() { return std::make_unique<MankitsuPart>(); }

}  // namespace neko
