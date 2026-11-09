// The `scroll` game part: the older engine's scrolling pictures (90 and 91) and its second image load (92), measured
// under Wine on Test Games 83 and 86. All three load a picture into a scene slot through PartHost::loadImage, so the
// next screen update shows it, 17 and 22 take it away, and saves keep it as any other picture.
#include <cstring>
#include <map>

#include "parts.h"
#include "save.h"

namespace neko {
namespace {

// The number the PC's save holds after the slot of command 90's picture (Test Game 83's fog, in a PC save). Command
// 91's is taken to be the same; no PC save of one has been read.
constexpr int kScrollKind = 6;
// Where that record keeps the command's numbers, in its fixed part with its three names empty: the speed as pixels a
// frame, and parameters 11 and 12 twice. With the speed left at 0 the PC loads the picture and holds it still, as it
// did for a save VNDegen wrote without it.
constexpr int kAtSpeed = 45, kAtP11 = 225, kAtP12 = 229, kAtP11Again = 333, kAtP12Again = 337;

class ScrollPart : public GamePart {
    static float perSecond(int thousandthsAFrame) { return float(thousandthsAFrame) * 60 / 1000; }

public:
    bool command(PartHost &host, const Node &n) override {
        switch (n.opcode) {
        case 90:
            // Across: slot p0, its top at y p2, tiled over the width and moving p3 thousandths of a pixel a frame, at
            // the games' 60 frames a second (negative to the left). Test Game 83's fog, p3 1000, crosses 106 pixels in
            // 1.76 seconds on the PC, fitted frame by frame over 12 seconds, and its PC save keeps 1.0 for it; Test
            // Game 86's lace strips, -500 along the top and +500 along the bottom, each move 15 pixels in half a
            // second.
            // A picture the game doesn't have loads nothing: what the slot showed stays, and its numbers with it.
            if (host.loadImage(n.p[0], n.s[1], 0, n.p[2], false, perSecond(n.p[3]), 0, kScrollKind))
                across_[n.p[0]] = {float(n.p[3]) / 1000, n.p[11], n.p[12]};
            return true;
        case 91:
            // Down: slot p0, its left at x p1, tiled over the height and moving p3 at the same rate (negative moves it
            // up). Set 1 measured it on Test Game 90's PC, frame by frame in a recording at 60 frames a second: the
            // picture sits at x p1 (-110 in all four uses, with p2 0), and p3 -35000 moved it 7,340 pixels in 210
            // frames. Neither of set 3's games uses it.
            // Where the PC's save keeps this one's speed isn't known (no PC save of one has been read), so its record
            // is written with none, and not with what command 90 left for the slot.
            if (host.loadImage(n.p[0], n.s[1], n.p[1], 0, false, 0, perSecond(n.p[3]), kScrollKind))
                across_.erase(n.p[0]);
            return true;
        case 92:
            // A picture at p1, p2 in slot p3, added to the scene when p4 is 1: Test Game 83's sword strokes, four
            // copies of one picture painted on black, show over each other and over the scene on the PC.
            if (host.loadImage(n.p[3], n.s[1], n.p[1], n.p[2], n.p[4] == 1)) across_.erase(n.p[3]);
            return true;
        default:
            return false;
        }
    }

    // The PC's record of each picture command 90 loaded, which the engine writes as a picture's: its speed and
    // parameters 11 and 12 go where the PC keeps them.
    void savePC(SaveData &d) const override {
        for (SavedSprite &s : d.sprites) {
            auto it = across_.find(s.slot);
            if (s.kind != kScrollKind || it == across_.end()) continue;
            float speed = it->second.speed;
            int32_t p11 = it->second.p11, p12 = it->second.p12;
            s.fixedBytes = {
                {kAtSpeed, bytes(&speed)},   {kAtP11, bytes(&p11)},      {kAtP12, bytes(&p12)},
                {kAtP11Again, bytes(&p11)}, {kAtP12Again, bytes(&p12)},
            };
        }
    }
    // A load brings the picture back, and its numbers with it: the next save writes them again. Without them a save
    // made in VNDegen after loading a PC save held speed 0, and the PC then showed the fog standing still. The speed
    // is kept as the float the save held, whatever it is, so a PC save's record comes through a load and a save with
    // the bytes it had: one too small for a command's thousandths, and one the engine won't play (it alone decides
    // that, in engine.cpp's restore, and draws such a picture standing still).
    void loadPC(const SaveData &d) override {
        across_.clear();
        for (const SavedSprite &s : d.sprites) {
            if (s.kind != kScrollKind || s.rawFixed.size() < size_t(kAtP12Again) + 4) continue;
            float speed;
            int32_t p11, p12;
            std::memcpy(&speed, &s.rawFixed[size_t(kAtSpeed)], 4);
            std::memcpy(&p11, &s.rawFixed[size_t(kAtP11)], 4);
            std::memcpy(&p12, &s.rawFixed[size_t(kAtP12)], 4);
            across_[s.slot] = {speed, p11, p12};
        }
    }
    void reset() override { across_.clear(); }

private:
    template <typename T> static std::vector<uint8_t> bytes(const T *value) {
        std::vector<uint8_t> b(sizeof(T));
        std::memcpy(b.data(), value, sizeof(T));
        return b;
    }
    // A scrolling picture's saved numbers: its speed in pixels a frame, as the PC's record holds it, and the command's
    // parameters 11 and 12.
    struct Across {
        float speed = 0;
        int p11 = 0, p12 = 0;
    };
    std::map<int, Across> across_;
};

}  // namespace

std::unique_ptr<GamePart> makeScrollPart() { return std::make_unique<ScrollPart>(); }

}  // namespace neko
