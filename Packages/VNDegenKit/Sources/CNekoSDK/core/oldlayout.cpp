// The older layout's commands, in the six games that keep their story in the exe (Test Games 82, 83, 86, 87, 90 and 96):
// 10 loads a background, 11 a CG, 12 deletes either, 13 loads a standing picture at one of six places, 14 deletes the one
// at a place, and 21 deletes them all. Each changes what the next update (command 9) shows, as command 15 does; the
// scripts follow them with one.
//
// Measured from seven PC saves of Test Game 83 (set 3, 2026-11-08), every record exact. A background sits in slot 100 at
// 0, 0. A standing picture at place P (parameter 0) sits in slot 200, 400, 300, 600, 500 or 700 for places 0 to 5, its
// middle at 0, -272, +272, -400, +400 or 0 from the screen's, and halfway down, with C's division, moved by parameters 2
// and 3. Every picture in those saves is 780 tall, so halfway down is inferred from command 15, which it matches there.
// A CG's slot is the background's, inferred: command 12 deletes "background or CG" with no slot named.
//
// A game whose exe places them otherwise says so in its table, under `screen defaults`:
//   layoutslots S0 S1 S2 S3 S4 S5     the slot for each place
//   layoutx X0 X1 X2 X3 X4 X5         each place's middle, from the screen's
//   layoutbgslot N                    the background's and CG's slot
#include <algorithm>
#include <cstdlib>

#include "engine.h"
#include "sjis.h"

namespace neko {
namespace {

int number(const std::vector<std::string> &line, size_t i) { return i < line.size() ? std::atoi(line[i].c_str()) : 0; }

}  // namespace

void Engine::loadOldLayout() {
    auto it = screens_.find("defaults");
    if (it == screens_.end()) return;
    const Screen &d = it->second;
    if (const auto *slots = d.setting("layoutslots"); slots && slots->size() >= 6)
        for (size_t i = 0; i < 6; i++) layoutSlots_[i] = number(*slots, i);
    if (const auto *x = d.setting("layoutx"); x && x->size() >= 6)
        for (size_t i = 0; i < 6; i++) layoutX_[i] = number(*x, i);
    if (const auto *slot = d.setting("layoutbgslot"); slot && !slot->empty()) layoutBgSlot_ = number(*slot, 0);
}

void Engine::runOldLayout(const Node &n) {
    const auto &p = n.p;
    auto place = [&] { return std::clamp(p[0], 0, 5); };
    switch (n.opcode) {
    case 10:
    case 11: {
        noteSeen(n.s[1]);
        ImageRef image = images_->get(shownPath(n.s[1]));
        if (!image) {
            complain("Missing image " + n.s[1]);
            return;
        }
        Sprite &s = back_[layoutBgSlot_];
        s = Sprite{};
        s.image = image;
        s.path = n.s[1];
        break;
    }
    case 12:
        back_.erase(layoutBgSlot_);
        break;
    case 13: {
        noteSeen(n.s[1]);
        ImageRef image = images_->get(shownPath(n.s[1]));
        if (!image) {
            complain("Missing image " + n.s[1]);
            return;
        }
        // Test Game 87 gives a second number with the place, and one load in Test Game 83 parameter 4; neither is known yet.
        if (p[1] != 0 || p[4] != 0) warnOnce("A standing picture's parameter 1 or 4 isn't known yet: " + sjis::toUtf8(n.s[0]));
        Sprite &s = back_[layoutSlots_[size_t(place())]];
        s = Sprite{};
        s.image = image;
        s.path = n.s[1];
        s.x = float((width_ - image->w) / 2 + layoutX_[size_t(place())] + p[2]);
        s.y = float((height_ - image->h) / 2 + p[3]);
        break;
    }
    case 14:
        back_.erase(layoutSlots_[size_t(place())]);
        break;
    case 21:
        for (int slot : layoutSlots_) back_.erase(slot);
        break;
    default:
        break;
    }
}

}  // namespace neko
