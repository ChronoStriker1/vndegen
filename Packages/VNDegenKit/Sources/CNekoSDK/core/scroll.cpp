// The `scroll` game part: the older engine's scrolling pictures (90 and 91) and its second image load (92), measured
// under Wine on Test Games 83 and 86. All three load a picture into a scene slot through PartHost::loadImage, so the
// next screen update shows it, 17 and 22 take it away, and saves keep it as any other picture.
#include "parts.h"

namespace neko {
namespace {

class ScrollPart : public GamePart {
public:
    bool command(PartHost &host, const Node &n) override {
        switch (n.opcode) {
        case 90: {
            // Across: slot p0, its top at y p2, moving p3 pixels a second (negative to the left), tiled over the width.
            // Test Game 86's character cards run a lace strip at -500 along the top and +500 along the bottom, and two
            // PC shots half a second apart show each moved 15 pixels on its 116-pixel repeat, one way and the other,
            // which 500 a second gives and 30 would too; unconfirmed between them. With p11 and p12 set, Test Game
            // 83's fog (p3 1000), the picture stays still on the PC: frames 0.7, 1.4 and 2 seconds apart fit at no shift.
            // It is tiled there all the same, from a place that differed between two runs (44 and 398 pixels along), by
            // a rule not found; here it starts from the left edge. loadImage tiles only what scrolls, so a still one
            // is given a speed too small to see, a pixel in a quarter of an hour.
            bool still = n.p[11] != 0 || n.p[12] != 0;
            host.loadImage(n.p[0], n.s[1], 0, n.p[2], false, still ? 0.001f : float(n.p[3]), 0);
            return true;
        }
        case 91:
            // Down: as 90 turned, its left at x p2 and p3 pixels a second, tiled over the height. Neither of set 3's games
            // uses it; it's inferred from 90 for Test Game 90.
            host.loadImage(n.p[0], n.s[1], n.p[2], 0, false, 0, float(n.p[3]));
            return true;
        case 92:
            // A picture at p1, p2 in slot p3, added to the scene when p4 is 1: Test Game 83's sword strokes, four copies
            // of one picture painted on black, show over each other and over the scene on the PC.
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
