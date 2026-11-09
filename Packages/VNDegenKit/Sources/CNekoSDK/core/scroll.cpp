// The `scroll` game part: the older engine's scrolling pictures (90 and 91) and its second image load (92), measured
// under Wine on Test Games 83 and 86. All three load a picture into a scene slot through PartHost::loadImage, so the
// next screen update shows it, 17 and 22 take it away, and saves keep it as any other picture.
#include "parts.h"

namespace neko {
namespace {

// The number the PC's save holds after the slot of command 90's picture (Test Game 83's fog, in a PC save). Command
// 91's is taken to be the same.
constexpr int kScrollKind = 6;

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
            host.loadImage(n.p[0], n.s[1], 0, n.p[2], false, perSecond(n.p[3]), 0, kScrollKind);
            return true;
        case 91:
            // Down: as 90 turned, its left at x p2 and p3 at the same rate, tiled over the height. Neither of set 3's
            // games uses it; it's inferred from 90 for Test Game 90.
            host.loadImage(n.p[0], n.s[1], n.p[2], 0, false, 0, perSecond(n.p[3]), kScrollKind);
            return true;
        case 92:
            // A picture at p1, p2 in slot p3, added to the scene when p4 is 1: Test Game 83's sword strokes, four
            // copies of one picture painted on black, show over each other and over the scene on the PC.
            host.loadImage(n.p[3], n.s[1], n.p[1], n.p[2], n.p[4] == 1);
            return true;
        default:
            return false;
        }
    }
};

}  // namespace

std::unique_ptr<GamePart> makeScrollPart() { return std::make_unique<ScrollPart>(); }

}  // namespace neko
