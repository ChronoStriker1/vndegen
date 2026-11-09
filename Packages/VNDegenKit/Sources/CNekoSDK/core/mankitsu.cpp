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
//
// The select screen, `open select` from the story table: ten half-weeks of four cards, each a character at a place, as
// cfg\SelectPhase.ini lists them (top, left, right, bottom). A heroine's card at place -1 stands at the place her hearts
// reach in cfg\SelectChar.ini and plays her next event, which gives her a heart, up to four; the other cards play the
// scene the PC's exe lists for that half-week and card. Which scene plays was traced under Wine from runs down each
// route and one skipping a heroine (set 1): events go by hearts, not by half-week. The screen's pieces were placed by
// fitting PC shots, the cards exactly (within 0.03 of 255 over the card area). State is in the story variables the PC's
// saves hold it in (set 1, nine PC saves along one path): S 3000 the week, 0 to 4, and 5 once the last is chosen, S 3001
// the half, S 3010 to 3012 Kururu's, Otona's and Rei's hearts. VNDegen's own, kept beside the save:
//   S 10904   the chapter the pick plays (`chapter S 10904`)
//   S 10906   the heroine whose ending follows, 1 to 3, or 0 for none, set with the last pick
#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <sstream>

#include "parts.h"

namespace neko {
namespace {

constexpr int kFaceX = 0, kFaceY = 512;
constexpr int kFollowMs = 120;  // the second strip's lag behind the first, in and out

constexpr int kWeekVar = 3000, kHalfVar = 3001, kHeartVar = 3010, kChapterVar = 10904, kEndingVar = 10906;
constexpr int kPhases = 10, kMaxHearts = 4;

// A card's place on the select screen, top left of its frame: top, left, right, bottom, in the ini's order.
constexpr int kCardX[4] = {399, 35, 842, 478}, kCardY[4] = {12, 297, 82, 367};
// The PC draws the cards in this order, top, right, left, bottom.
constexpr int kDrawOrder[4] = {0, 2, 1, 3};
constexpr int kCardW = 373, kCardH = 232, kLift = 4;
constexpr int kSideX = 1172, kSideY = 457, kSideStep = 58;
constexpr int kSideColumns[4] = {0, 1, 5, 6};  // save, load, config, title in e001_000

// What a card plays: a heroine's event (by her hearts) or a fixed scene list.
struct CardScenes {
    int heroine = -1;  // 0 to 2: her next event, or her late short scene at four hearts
    std::vector<const char *> scenes;
};

// Each half-week's cards, top, left, right, bottom, as the exe lists them (set 1, under Wine).
const CardScenes kCards[kPhases][4] = {
    {{0, {}}, {1, {}}, {2, {}}, {-1, {"ep_006_000_000"}}},
    {{-1, {"ep_007_001_000"}}, {-1, {"ep_007_002_000"}}, {-1, {"ep_007_003_000"}}, {-1, {"ep_007_000_003"}}},
    {{0, {}}, {1, {}}, {2, {}}, {-1, {"ep_006_002_000"}}},
    {{-1, {"ep_006_001_000"}}, {-1, {"ep_007_001_001"}}, {-1, {"ep_007_000_002"}}, {-1, {"ep_006_003_000"}}},
    {{0, {}}, {1, {}}, {2, {}}, {-1, {"ep_006_004_000"}}},
    {{-1, {"ep_007_001_002"}}, {-1, {"ep_007_002_001"}}, {-1, {"ep_007_003_001"}}, {-1, {"ep_007_000_000"}}},
    {{0, {}}, {1, {}}, {2, {}}, {-1, {"ep_007_000_001"}}},
    {{-1, {"ep_007_000_004"}}, {-1, {"ep_006_005_000"}}, {-1, {"ep_006_006_000"}}, {-1, {"ep_006_007_000"}}},
    {{0, {}}, {1, {}}, {2, {}}, {-1, {"ep_007_000_005"}}},
    {{0, {}}, {1, {}}, {2, {}}, {-1, {"ep_006_008_000"}}},
};
// Each heroine's four events, by the hearts she has, each with its second part; Otona's third goes on into 019.
const std::vector<const char *> kEvents[3][kMaxHearts] = {
    {{"ep_001_007_000", "ep_001_007_001"}, {"ep_001_012_000", "ep_001_012_001"}, {"ep_001_017_000", "ep_001_017_001"}, {"ep_001_022_000", "ep_001_022_001"}},
    {{"ep_001_008_000", "ep_001_008_001"}, {"ep_001_013_000", "ep_001_013_001"},
     {"ep_001_018_000", "ep_001_018_001", "ep_001_019_000", "ep_001_019_001"}, {"ep_001_023_000", "ep_001_023_001"}},
    {{"ep_001_009_000", "ep_001_009_001"}, {"ep_001_014_000", "ep_001_014_001"}, {"ep_001_020_000", "ep_001_020_001"}, {"ep_001_024_000", "ep_001_024_001"}},
};
// A heroine with four hearts in the last two half-weeks: a short scene of hers.
const char *const kLate[3][2] = {{"ep_007_001_003", "ep_007_001_004"}, {"ep_007_002_002", "ep_007_002_003"}, {"ep_007_003_003", "ep_007_003_004"}};
// The common scenes after a half-week's pick, and at the end the heroine's last event and two more (none: 029 alone).
const std::vector<const char *> kAfter[kPhases] = {
    {}, {"ep_001_010_000", "ep_001_011_000", "ep_001_011_001"}, {}, {"ep_001_015_000", "ep_001_016_000"}, {},
    {"ep_001_021_000"}, {}, {"ep_001_025_000"}, {}, {"ep_001_030_000", "ep_001_031_000"},
};
const std::vector<const char *> kFinal[4] = {
    {"ep_001_029_000"}, {"ep_001_026_000", "ep_001_026_001"}, {"ep_001_027_000", "ep_001_027_001"}, {"ep_001_028_000", "ep_001_028_001"},
};

struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

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
        if (layer == Layer::Screen && selecting_) drawSelect(host, out);
    }

    bool openScreen(PartHost &host, const std::string &name, const std::vector<int> &) override {
        if (name != "select") return false;
        readInis(host);
        selecting_ = true;
        confirming_ = -1;
        host.redraw();
        return true;
    }

    void pointer(PartHost &host, int x, int y) override {
        px_ = x;
        py_ = y;
        if (selecting_) host.redraw();
    }

    bool press(PartHost &host, int x, int y, int button) override {
        if (!selecting_) return false;
        host.redraw();
        if (confirming_ >= 0) {
            if (button == 0 && yesRect().contains(x, y)) return choose(host, confirming_);
            if (button == 1 || (button == 0 && noRect().contains(x, y))) confirming_ = -1;
            return true;
        }
        if (button != 0) return true;
        for (int c = 0; c < 4; c++)
            if (cardRect(c).contains(x, y)) {
                confirming_ = c;
                return true;
            }
        for (int k = 0; k < 4; k++)
            if (sideRect(k).contains(x, y) && !warnedSide_) {
                // Save, load, config and title from here need a way into the menus that PartHost doesn't give yet.
                host.log("Test Game 90's select screen buttons aren't wired to the menus yet");
                warnedSide_ = true;
            }
        return true;
    }

    // Controller focus: the four cards, the side buttons, and yes and no while asking.
    bool focusSpots(PartHost &, std::vector<std::pair<int, int>> &spots) override {
        if (!selecting_) return false;
        auto middle = [&](const Rect &r) { spots.push_back({r.x + r.w / 2, r.y + r.h / 2}); };
        if (confirming_ >= 0) {
            middle(yesRect());
            middle(noRect());
            return true;
        }
        for (int c = 0; c < 4; c++) middle(cardRect(c));
        for (int k = 0; k < 4; k++) middle(sideRect(k));
        return true;
    }

    // `chapter S 10904`: the scenes a pick plays, from the number choose() put there.
    std::vector<std::string> chapter(PartHost &, int number) override {
        int phase = number / 1000 - 1, card = number / 100 % 10, hearts = number / 10 % 10, ending = number % 10;
        if (phase < 0 || phase >= kPhases || card > 3) return {};
        std::vector<std::string> out;
        auto add = [&](const std::vector<const char *> &list) {
            for (const char *scene : list) out.push_back(std::string("scr\\") + scene + ".txt");
        };
        const CardScenes &cs = kCards[phase][card];
        if (cs.heroine < 0) add(cs.scenes);
        else if (hearts < kMaxHearts) add(kEvents[cs.heroine][hearts]);
        else add({kLate[cs.heroine][phase == kPhases - 1 ? 1 : 0]});
        // The last pick: the heroine's last event and two common scenes, or with none at four hearts 029 alone, after
        // which the PC goes back to the title (set 1, a run picking no heroine).
        if (phase == kPhases - 1) add(kFinal[std::clamp(ending, 0, 3)]);
        if (phase < kPhases - 1 || ending > 0) add(kAfter[phase]);
        return out;
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
        selecting_ = false;
        confirming_ = -1;
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

    // The select screen.
    struct Card {
        int character = 7, place = 0;  // n005_000's column; PosNameIndex, -1 for the heroine's own by her hearts
    };

    static Rect cardRect(int c) { return {kCardX[c], kCardY[c], kCardW, kCardH}; }
    static Rect sideRect(int k) { return {kSideX, kSideY + kSideStep * k, 108, 52}; }
    static Rect yesRect() { return {339, 293, 592, 72}; }
    static Rect noRect() { return {339, 380, 592, 72}; }

    int phase(PartHost &host) const { return std::clamp(2 * host.variable(1, kWeekVar) + host.variable(1, kHalfVar), 0, kPhases - 1); }
    int hearts(PartHost &host, int heroine) const { return std::clamp(host.variable(1, kHeartVar + heroine), 0, kMaxHearts); }

    // The place a card stands at: its own, or for a heroine at -1, the next in her SelectChar list.
    int placeOf(PartHost &host, const Card &card) const {
        if (card.place >= 0 || card.character > 2) return std::clamp(card.place, 0, 11);
        const auto &list = charPlaces_[size_t(card.character)];
        int h = hearts(host, card.character);
        return list.empty() ? 0 : std::clamp(list[size_t(std::min(h, int(list.size()) - 1))], 0, 11);
    }

    bool choose(PartHost &host, int c) {
        int ph = phase(host);
        const CardScenes &cs = kCards[ph][c];
        int h = cs.heroine >= 0 ? hearts(host, cs.heroine) : 0;
        if (cs.heroine >= 0 && h < kMaxHearts) host.setVariable(1, kHeartVar + cs.heroine, h + 1);
        int ending = 0;
        if (ph == kPhases - 1)
            for (int heroine = 0; heroine < 3; heroine++)
                if (hearts(host, heroine) >= kMaxHearts) ending = heroine + 1;
        host.setVariable(1, kChapterVar, 1000 * (ph + 1) + 100 * c + 10 * h + ending);
        host.setVariable(1, kEndingVar, ending);
        host.setVariable(1, kWeekVar, (ph + 1) / 2);
        host.setVariable(1, kHalfVar, (ph + 1) % 2);
        selecting_ = false;
        confirming_ = -1;
        host.finishScreen(c);
        return true;
    }

    // cfg\SelectPhase.ini's `#p WEEK HALF` blocks of CharIndex and PosNameIndex, and SelectChar.ini's `#p HEROINE N`
    // PosIndex lists.
    void readInis(PartHost &host) {
        if (inisRead_) return;
        inisRead_ = true;
        auto lines = [&](const char *path) {
            std::vector<std::vector<std::string>> out;
            auto bytes = host.read(path);
            if (!bytes) return out;
            std::istringstream in(std::string(bytes->begin(), bytes->end()));
            for (std::string line; std::getline(in, line);) {
                if (size_t c = line.find("//"); c != std::string::npos) line.erase(c);
                std::istringstream words(line);
                std::vector<std::string> w;
                for (std::string word; words >> word;) w.push_back(word);
                if (!w.empty()) out.push_back(w);
            }
            return out;
        };
        int block = -1, n = 0;
        for (const auto &w : lines("cfg\\SelectPhase.ini")) {
            if (w[0] == "#p" && w.size() >= 3) {
                block = std::atoi(w[1].c_str()) * 2 + std::atoi(w[2].c_str());
                n = 0;
            } else if (block >= 0 && block < kPhases && n < 4) {
                if (w[0] == "CharIndex" && w.size() >= 2) phaseCards_[size_t(block)][size_t(n)].character = std::clamp(std::atoi(w[1].c_str()), 0, 7);
                else if (w[0] == "PosNameIndex" && w.size() >= 2) phaseCards_[size_t(block)][size_t(n)].place = std::atoi(w[1].c_str());
                else if (w[0] == "AddArray") n++;
            }
        }
        int heroine = -1;
        for (const auto &w : lines("cfg\\SelectChar.ini")) {
            if (w[0] == "#p" && w.size() >= 3) heroine = std::atoi(w[1].c_str());
            else if (w[0] == "PosIndex" && w.size() >= 2 && heroine >= 0 && heroine < 3) charPlaces_[size_t(heroine)].push_back(std::atoi(w[1].c_str()));
        }
    }

    static void put(PartHost &host, Canvas &out, const std::string &path, int x, int y, int cols = 1, int rows = 1, int col = 0, int row = 0) {
        ImageRef img = host.image(path);
        if (!img) return;
        int w = img->w / cols, h = img->h / rows;
        drawPart(out, *img, x, y, col * w, row * h, w, h, 1);
    }

    void drawSelect(PartHost &host, Canvas &out) {
        int ph = phase(host);
        int hover = -1;
        if (confirming_ < 0)
            for (int c = 0; c < 4; c++)
                if (cardRect(c).contains(px_, py_)) hover = c;
        put(host, out, "obj\\n000_000.bmp", 0, 0);
        for (int c : kDrawOrder) {
            put(host, out, "obj\\n003_002.bmp", kCardX[c] - 16, kCardY[c] + 155);
            put(host, out, "obj\\n003_001.bmp", kCardX[c] + 66, kCardY[c] + 207);
        }
        for (int c : kDrawOrder) {
            const Card &card = phaseCards_[size_t(ph)][size_t(c)];
            bool lifted = c == hover;
            int x = kCardX[c], y = kCardY[c] - (lifted ? kLift : 0);
            char place[32];
            std::snprintf(place, sizeof place, "obj\\n004_%03d.bmp", placeOf(host, card));
            put(host, out, place, x + 23, y + 28);
            put(host, out, "obj\\n003_000.bmp", x, y, 1, 5, 0, c == confirming_ ? 2 : lifted ? 1 : 0);
            put(host, out, "obj\\n005_000.bmp", x + 257, y - 41, 8, 5, card.character, lifted ? 1 : 0);
            if (card.character <= 2) {
                put(host, out, "obj\\n006_000.bmp", x + 246, y + 157);
                for (int k = 0; k < hearts(host, card.character); k++) put(host, out, "obj\\n006_001.bmp", x + 250 + 44 * k, y + 161);
            }
        }
        put(host, out, "obj\\n001_000.bmp", 0, 30);
        put(host, out, "obj\\n001_001.bmp", 32, 29, 10, 1, (ph / 2 + 1) % 10, 0);
        put(host, out, "obj\\n001_002.bmp", 183, 29, 2, 1, ph % 2, 0);
        put(host, out, "obj\\n002_000.bmp", 0, 104);
        if (hover >= 0) {
            int x = kCardX[hover] + 37, y = kCardY[hover] + 261;
            put(host, out, "obj\\n007_000.bmp", x, y);
            put(host, out, "obj\\n007_001.bmp", x + 63, y + 8, 12, 1, placeOf(host, phaseCards_[size_t(ph)][size_t(hover)]), 0);
        }
        for (int k = 0; k < 4; k++) {
            bool over = confirming_ < 0 && sideRect(k).contains(px_, py_);
            put(host, out, "obj\\e001_000.bmp", kSideX, kSideY + kSideStep * k, 9, 5, kSideColumns[k], over ? 1 : 0);
            if (over) put(host, out, "obj\\e001_002.bmp", 1026, kSideY + kSideStep * k - 104, 9, 1, kSideColumns[k], 0);
        }
        if (confirming_ >= 0) {
            tintFrame(out, 0, 0, 0, 155);
            put(host, out, "obj\\a003_001.bmp", 325, 228);
            put(host, out, "obj\\a004_000.bmp", 339, 293, 2, 5, 0, yesRect().contains(px_, py_) ? 1 : 0);
            put(host, out, "obj\\a004_000.bmp", 339, 380, 2, 5, 1, noRect().contains(px_, py_) ? 1 : 0);
        }
    }

    bool selecting_ = false, inisRead_ = false, warnedSide_ = false;
    int confirming_ = -1, px_ = -1, py_ = -1;
    std::array<std::array<Card, 4>, kPhases> phaseCards_{};
    std::array<std::vector<int>, 3> charPlaces_;

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
