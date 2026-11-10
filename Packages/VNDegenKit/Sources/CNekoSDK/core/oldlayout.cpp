// The older layout's commands, in the six games that keep their story in the exe (Test Games 82, 83, 86, 87, 90 and 96):
// 10 loads a background, 11 a CG, 12 deletes either, 13 loads a standing picture at one of five or six places, 14 deletes
// the one at a place, and 21 deletes them all. Each changes what the next update (command 9) shows, as command 15 does; the
// scripts follow them with one.
//
// Measured from seven PC saves of Test Game 83 (set 3, 2026-11-08), every record exact. A background sits in slot 100 at
// 0, 0. A standing picture at place P (parameter 0) sits in slot 200, 400, 300, 600, 500 or 700 for places 0 to 5, its
// middle at 0, -272, +272, -400, +400 or 0 from the screen's, and halfway down, with C's division, moved by parameters 2
// and 3. Every picture in those saves is 780 tall, so halfway down is inferred from command 15, which it matches there.
// A CG's slot is the background's there, inferred: command 12 deletes "background or CG" with no slot named.
//
// Test Game 82 (set 2, nine PC saves under Wine) keeps its CGs in slot 102, and a CG or a background loaded over the
// other, with no delete between, leaves only its own record: each takes the other away. Its places 0 to 2 match Test
// Game 83's; places 3 to 5 aren't checked there yet.
//
// Test Game 87 (set 1, 21 PC saves under Wine, every record exact) picks the slot by a second number with the place,
// `位置:P_N`: parameter 1 for a load, slot 200 + 100 * N, and 14's number is that slot's N, not a place. Its five places'
// middles are 512 and 256 to the left, the middle, and 256 and 512 to the right.
//
// A game whose exe places them otherwise says so in its table, under `screen defaults`:
//   layoutslots S0 S1 ...             the slot for each place, or with `layoutslotby p1` for each parameter 1
//   layoutslotby p1                   a load's slot by its parameter 1, and 14's number names a slot, not a place
//   layoutx X0 X1 ...                 each place's middle, from the screen's
//   layoutbgslot N                    the background's slot
//   layoutcgslot N                    the CG's, the background's unless given
#include <tuple>
#include <sstream>
#include <cstdio>
#include <cctype>
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
    if (const auto *slots = d.setting("layoutslots"); slots && !slots->empty()) {
        layoutSlots_.clear();
        for (size_t i = 0; i < slots->size(); i++) layoutSlots_.push_back(number(*slots, i));
    }
    if (const auto *x = d.setting("layoutx"); x && !x->empty()) {
        layoutX_.clear();
        for (size_t i = 0; i < x->size(); i++) layoutX_.push_back(number(*x, i));
    }
    if (const auto *by = d.setting("layoutslotby"); by && !by->empty()) layoutSlotByP1_ = (*by)[0] == "p1";
    if (const auto *slot = d.setting("layoutbgslot"); slot && !slot->empty()) layoutBgSlot_ = number(*slot, 0);
    layoutCgSlot_ = layoutBgSlot_;
    if (const auto *slot = d.setting("layoutcgslot"); slot && !slot->empty()) layoutCgSlot_ = number(*slot, 0);
    layoutPairs_ = d.setting("layoutpairs") != nullptr;
    loadBulb();
}

void Engine::loadBulb() {
    bulbPatterns_.clear();
    bulbDark_.clear();
    bulbBlends_.clear();
    bulbPattern_ = -1;
    bulbFrom_ = bulbDrawn_ = 255;
    if (!layoutPairs_) return;
    Bytes data = files_.read("cfg\\light_info.ini");
    if (!data) return;
    std::istringstream in(std::string(data->begin(), data->end()));
    bool open = false;
    for (std::string line; std::getline(in, line);) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        if (line.rfind("#p", 0) == 0) {
            bulbPatterns_.emplace_back();
            open = true;
        } else if (line == "##") {
            open = false;
        } else if (open) {
            int a = 0, b = 0;
            if (std::sscanf(line.c_str(), "light,%d,%d", &a, &b) == 2) bulbPatterns_.back().push_back({std::max(0, a), std::clamp(b, 0, 255)});
            else if (std::sscanf(line.c_str(), "wait,%d", &a) == 1) bulbPatterns_.back().push_back({std::max(0, a), -1});
        }
    }
    bulbPatterns_.erase(std::remove_if(bulbPatterns_.begin(), bulbPatterns_.end(), [](const std::vector<BulbStep> &p) {
        int frames = 0;
        for (const BulbStep &s : p) frames += s.frames;
        return frames <= 0;
    }), bulbPatterns_.end());
}

int Engine::bulbLevel() {
    if (bulbPatterns_.empty() || !setting("bulb")) return 255;
    for (int guard = 0; guard < 1000; guard++) {
        if (bulbPattern_ < 0) {
            bulbPattern_ = int(size_t(std::rand()) % bulbPatterns_.size());
            if (bulbStart_ <= 0 || bulbStart_ > now_) bulbStart_ = now_;
        }
        int64_t frame = (now_ - bulbStart_) * 60 / 1000, at = 0;
        int level = bulbFrom_;
        for (const BulbStep &step : bulbPatterns_[size_t(bulbPattern_)]) {
            if (frame < at + step.frames) {
                if (step.level < 0) return level;
                return level + int((step.level - level) * (frame - at + 1) / std::max(1, step.frames));
            }
            at += step.frames;
            if (step.level >= 0) level = step.level;
        }
        bulbFrom_ = level;
        bulbStart_ += at * 1000 / 60;
        bulbPattern_ = -1;
        if (now_ - bulbStart_ > 60000) bulbStart_ = now_;
    }
    return bulbFrom_;
}

ImageRef Engine::bulbImage(const Sprite &s, ImageRef lit) {
    if (!layoutPairs_ || bulbPatterns_.empty() || !lit || s.path.empty()) return lit;
    int level = bulbLevel();
    if (level >= 255) return lit;
    auto found = bulbDark_.find(s.path);
    if (found == bulbDark_.end()) {
        std::string name = normalizedPath(s.path), dark;
        size_t dot = name.rfind('.');
        int add = name.rfind("sc\\", 0) == 0 ? 10 : name.rfind("bg\\", 0) == 0 ? 1 : 0;
        if (add && dot != std::string::npos && dot >= 3 && std::isdigit(uint8_t(name[dot - 1])) && std::isdigit(uint8_t(name[dot - 2])) && std::isdigit(uint8_t(name[dot - 3]))) {
            int number = std::atoi(name.substr(dot - 3, 3).c_str()) + add;
            char digits[8];
            std::snprintf(digits, sizeof digits, "%03d", number % 1000);
            std::string other = name;
            other.replace(dot - 3, 3, digits);
            if (number < 1000 && files_.exists(other)) dark = other;
        }
        found = bulbDark_.emplace(s.path, dark).first;
    }
    if (found->second.empty()) return lit;
    ImageRef dark = images_->get(found->second);
    if (!dark || dark->w != lit->w || dark->h != lit->h) return lit;
    if (level <= 0) return dark;
    auto key = std::make_tuple(lit.get(), dark.get(), level / 8);
    if (auto made = bulbBlends_.find(key); made != bulbBlends_.end()) return made->second;
    auto mixed = std::make_shared<Image>(*dark);
    int a = level / 8 * 8;
    for (size_t i = 0; i < mixed->rgba.size(); i++) mixed->rgba[i] = uint8_t((dark->rgba[i] * (255 - a) + lit->rgba[i] * a + 127) / 255);
    mixed->opaque = lit->opaque && dark->opaque;
    if (bulbBlends_.size() > 96) bulbBlends_.clear();
    bulbBlends_[key] = mixed;
    return mixed;
}

// `layoutpairs` under `screen defaults`: the PC keeps each place's picture as two records, the slot's own and the slot
// after it, as Test Game 96 keeps the dark and the lit version of every standing picture and background for its
// flickering bulb (set 2, from its PC saves). The runtime draws one, in the slot itself, so whatever takes a place's
// picture away or puts another there clears the slot after it too. Without that, the lit picture a PC save brought
// stayed on screen after the story removed it.
void Engine::clearLayoutSlot(int slot) {
    back_.erase(slot);
    if (layoutPairs_) back_.erase(slot + 1);
}

void Engine::runOldLayout(const Node &n) {
    const auto &p = n.p;
    auto place = [&] { return size_t(std::clamp(p[0], 0, int(layoutX_.size()) - 1)); };
    auto slotOf = [&](int index) { return layoutSlots_[size_t(std::clamp(index, 0, int(layoutSlots_.size()) - 1))]; };
    switch (n.opcode) {
    case 10:
    case 11: {
        noteSeen(n.s[1]);
        ImageRef image = images_->get(shownPath(n.s[1]));
        if (!image) {
            complain("Missing image " + n.s[1]);
            return;
        }
        // A background and a CG in slots of their own each take the other away.
        int slot = n.opcode == 10 ? layoutBgSlot_ : layoutCgSlot_, other = n.opcode == 10 ? layoutCgSlot_ : layoutBgSlot_;
        if (other != slot) clearLayoutSlot(other);
        clearLayoutSlot(slot);
        Sprite &s = back_[slot];
        s = Sprite{};
        s.image = image;
        s.path = n.s[1];
        break;
    }
    case 12:
        clearLayoutSlot(layoutBgSlot_);
        clearLayoutSlot(layoutCgSlot_);
        break;
    case 13: {
        noteSeen(n.s[1]);
        ImageRef image = images_->get(shownPath(n.s[1]));
        if (!image) {
            complain("Missing image " + n.s[1]);
            return;
        }
        // Test Game 87 gives a second number with the place, and one load in Test Game 83 parameter 4; neither is known yet.
        if ((p[1] != 0 && !layoutSlotByP1_) || p[4] != 0) warnOnce("A standing picture's parameter 1 or 4 isn't known yet: " + sjis::toUtf8(n.s[0]));
        int slot = slotOf(layoutSlotByP1_ ? p[1] : p[0]);
        clearLayoutSlot(slot);
        Sprite &s = back_[slot];
        s = Sprite{};
        s.image = image;
        s.path = n.s[1];
        s.x = float((width_ - image->w) / 2 + layoutX_[place()] + p[2]);
        s.y = float((height_ - image->h) / 2 + p[3]);
        break;
    }
    case 14:
        clearLayoutSlot(slotOf(p[0]));
        break;
    case 21:
        for (int slot : layoutSlots_) clearLayoutSlot(slot);
        break;
    default:
        break;
    }
}

}  // namespace neko
