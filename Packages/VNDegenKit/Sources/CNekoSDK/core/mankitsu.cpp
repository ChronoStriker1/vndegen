// Test Game 90 (Mankitsu Happening)'s face pictures and scene-change slides, commands 130 to 145 and 160 to 163, which
// no other game uses. Measured on the PC game under Wine (set 1), from 30 fps recordings of its first scenes.
//
// Faces: a layer of its own inside the message window, with its origin at the window's top left, (0, 512), over its
// plate and under its text, and gone with the window. 130 loads an `ac\` picture into a face slot (parameter 2: 2000,
// or 2001 for a second face beside it) at parameters 0 and 1 from that origin; 131 removes one (parameter 0) and 138 all
// of them; 132 shows what's loaded, fading over parameter 1 ms as a screen update does. 136 is the scene's timed change
// (25) for a face slot, used only to move one, a nod or a hop: by parameters 0 and 1, after parameter 24 frames and over
// parameter 25 (negative easing out), at 60 frames a second. 139 is the scene's flipbook (121) there, the emotion marks:
// slot parameter 0 (3000), the unscaled picture's top left at parameters 2 and 3, scaled about its middle by parameters
// 4 and 5 in thousandths, each picture shown for parameter 16 ms. A face's place was
// fitted exactly on PC frames, and an anger mark at -20, -20 and 0.31 sits at 46, 47 from the origin, as these rules
// give; its 41 pictures took 1.3 s, two frames each for parameter 16's 30 ms and not anime_bustup_cfg.ini's 16 (30 fps
// recordings under Wine). The hop's pace wasn't settled (10 pixels came back over about 0.27 s where 10 frames would be 0.17), and
// 130's parameters 3 to 5 (1, 1, 200 in a quarter of the loads, each still followed by a 132) show nothing; the one
// face shake (145) isn't drawn.
//
// Slides: 160 names two panel pictures (s1, s2), 162 starts bars of them sliding in from the left until they cover the
// screen, 161 waits until they do, while the scene changes behind them. On the PC the bars' heights and speeds vary from slide to slide, as if random; these are drawn in the same
// style over the times the recording shows (in over 0.6 s, out over 0.8 s), not the PC's own bars. Unconfirmed.
//
// The select screen, `open select` from the story table: ten half-weeks of four cards, each a character at a place, as
// cfg\SelectPhase.ini lists them (top, left, right, bottom). A heroine's card at place -1 stands at the place her hearts
// reach in cfg\SelectChar.ini and plays her next event, which gives her a heart; the other cards play the
// scene the PC's exe lists for that half-week and card. Which scene plays was traced under Wine from runs down each
// route and one skipping a heroine (set 1): events go by hearts, not by half-week. The screen's pieces were placed by
// fitting PC shots, the cards exactly (within 0.03 of 255 over the card area). State is in the story variables the PC's
// saves hold it in (set 1, PC saves along one path): S 3000 the week, 0 to 4, S 3001 the half, and S 3010 to
// 3012 Kururu's, Otona's and Rei's hearts. VNDegen's own, kept beside the save:
//   S 10904   the chapter the pick plays (`chapter S 10904`)
//   S 10906   the heroine whose ending follows, 1 to 3, or 0 for none, set with the last pick
//
// The title, `part mankitsu` under its table screen, under the table's buttons: one of nine backgrounds at a time, in
// order, 4.33 s each and crossfading over about 0.6 s (a 75 s recording under Wine, two rounds), the three heroines'
// silhouettes, the logo and the trims, the top one moving left 240 pixels a second. Once the shared ending is seen
// (G 83) the title has six silhouettes and blue trims, each from its own pictures, as editing cfg.sav showed. Which
// background a launch starts on wasn't the same twice under Wine; it starts on a random one here.
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <random>
#include <sstream>

#include "parts.h"
#include "save.h"

namespace neko {
namespace {

constexpr int kFaceX = 0, kFaceY = 512;
constexpr int kFollowMs = 120;  // the second strip's lag behind the first, in and out
constexpr int kCaptionInMs = 100, kCaptionBack = 15, kCaptionDrift = 30;

constexpr int kTitleBottomShift = 152;
constexpr int kTitleMs = 4333, kTitleFadeMs = 600, kTitleTrimSpeed = 240, kTitleTrimPeriod = 600;
constexpr int kMenuTrimSpeed = 240;
constexpr int kNewsX = 803, kNewsW = 414, kNewsSpeed = 120;
// The title's rising dots: how many, the strip they start along (the silhouettes' feet) and how far they rise.
constexpr int kSparkles = 9, kSparkleX = 24, kSparkleW = 640, kSparkleFrom = 690, kSparkleRise = 340;
// The lower row's liquid: how far and how long it rises as the row opens, and the gap in its drawing that makes the next
// drawing a new opening.
constexpr int kRowRise = 82, kRowRiseMs = 170, kRowGapMs = 100;
constexpr int kWeekVar = 3000, kHalfVar = 3001, kHeartVar = 3010, kChapterVar = 10904, kEndingVar = 10906;
constexpr int kPhases = 10, kMaxHearts = 4;
constexpr int kHeartCap = 5;

// A card's place on the select screen, top left of its frame: top, left, right, bottom, in the ini's order.
constexpr int kCardX[4] = {399, 35, 842, 478}, kCardY[4] = {12, 297, 82, 367};
// The PC draws the cards in this order, top, right, left, bottom.
constexpr int kDrawOrder[4] = {0, 2, 1, 3};
constexpr int kCardW = 373, kCardH = 232, kLift = 4;
constexpr int kSideX = 1172, kSideY = 457, kSideStep = 58;
constexpr int kSideColumns[4] = {0, 1, 5, 6};  // save, load, config, title in e001_000
const char *const kSideActions[4] = {"nav_save", "nav_load", "nav_config", "nav_title"};

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

// The PC's numbers: the last before each half-week's next select screen, and a fixed half-week's card's own (0 for the
// half-weeks whose cards share the last number).
constexpr int kPhaseEnd[kPhases] = {60, 84, 110, 142, 146, 171, 176, 196, 210, 220};
constexpr int kCardNumber[kPhases] = {0, 81, 0, 140, 0, 170, 0, 195, 0, 0};
constexpr int kFinalFirst = 261, kFinalStep = 5, kFinalSecond = 275;

struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// A face slot's picture, or its flipbook of emotion marks.
struct Face {
    std::string path;
    int x = 0, y = 0;
    // A flipbook (139): its pictures and time each, scale, whether it repeats, and when the update that showed it ended.
    int flipCount = 0, flipMs = 0;
    int64_t flipFrom = 0;
    float scale = 1;
    bool flipLoop = false, scaleAboutMiddle = true;
    int flipTimes = 1;
    bool again = false;
    bool same(const Face &o) const { return path == o.path && x == o.x && y == o.y && flipCount == o.flipCount; }
};

// A timed move of a face slot (136).
struct FaceMove {
    int slot = 0, dx = 0, dy = 0;
    int64_t from = 0;
    int ms = 0;
    bool easeOut = false;
};

struct Bar {
    int y = 0, h = 0;
    int inDelay = 0, inMs = 0, outDelay = 0, outMs = 0;
};

class MankitsuPart : public GamePart {
public:
    bool command(PartHost &host, const Node &n) override {
        switch (n.opcode) {
        case 130: {
            Face f;
            f.path = n.s[1];
            f.x = n.p[0];
            f.y = n.p[1];
            loaded_[n.p[2]] = f;
            dropMoves(n.p[2]);
            return true;
        }
        case 131:
            loaded_.erase(n.p[0]);
            return true;
        case 138:
            loaded_.clear();
            return true;
        case 132:
            from_ = shown_;
            shown_ = loaded_;
            fadeFrom_ = host.nowMs();
            fadeMs_ = std::max(0, int(n.p[1]));
            // A flipbook plays from halfway through the update that first shows it, as the PC's anger mark was on its tenth
            // picture two frames after a 500 ms update ended (one mark, so unconfirmed), and goes on through later ones.
            for (auto &[slot, f] : shown_) {
                auto before = from_.find(slot);
                bool fresh = f.again || before == from_.end() || !before->second.same(f);
                f.again = false;
                f.flipFrom = fresh ? fadeFrom_ + fadeMs_ / 2 : before->second.flipFrom;
                // Its sound, in the effects channel after the scene's flipbooks' (180), so both can sound at once.
                if (auto sound = flipSounds_.find(lower(f.path)); fresh && f.flipCount > 0 && sound != flipSounds_.end())
                    host.playSound(1, 181, sound->second.first, sound->second.second);
                loaded_[slot].flipFrom = f.flipFrom;
                loaded_[slot].again = false;
            }
            host.redraw();
            return true;
        case 136: {
            FaceMove m;
            m.slot = n.p[2];
            m.dx = n.p[0];
            m.dy = n.p[1];
            m.from = host.nowMs() + int64_t(n.p[24]) * 1000 / 60;
            m.ms = std::abs(int(n.p[25])) * 1000 / 60;
            m.easeOut = n.p[25] < 0;
            moves_.push_back(m);
            return true;
        }
        case 139: {
            readFlipbooks(host);
            Face f;
            f.path = n.s[1];
            f.x = n.p[2];
            f.y = n.p[3];
            f.scale = n.p[4] > 0 ? float(n.p[4]) / 1000 : 1;
            f.scaleAboutMiddle = n.p[8] != 0;
            f.flipLoop = n.p[14] != 0;
            f.flipTimes = f.flipLoop ? std::max(1, int(n.p[15])) : 1;
            auto it = flipbooks_.find(lower(n.s[1]));
            f.flipCount = it == flipbooks_.end() ? 1 : it->second.first;
            // Parameter 16's ms a picture, in whole frames of 60 a second: 30 ms held each of an anger mark's pictures two
            // frames on the PC.
            int ms = n.p[16] > 0 ? n.p[16] : it == flipbooks_.end() ? 16 : it->second.second;
            f.flipMs = std::max(1, (ms * 60 + 999) / 1000) * 1000 / 60;
            f.again = true;
            loaded_[n.p[0]] = f;
            return true;
        }
        case 145:
            return true;  // a face shake, once in the game: not drawn
        case 160:
            panels_[0] = n.s[1];
            panels_[1] = n.s[2];
            holdMs_ = std::max(0, int(n.p[14]));
            slideIn_ = std::clamp(int(n.p[4]), 1, 600);
            slideOut_ = std::clamp(int(n.p[7]), 1, 600);
            captionIn_ = std::clamp(int(n.p[5]), 0, 600);
            captionStay_ = std::clamp(int(n.p[6]), 0, 6000);
            caption_ = captionIn_ + captionStay_ > 0 ? n.s[3] : std::string();
            captionCols_ = n.p[0] == 2 ? 2 : 1;
            captionCol_ = std::clamp(int(n.p[2]), 0, captionCols_ - 1);
            return true;
        case 162: startSlide(host); return true;
        case 161:
            if (int left = int(slideFrom_ + coverMs() - host.nowMs()); left > 0) host.waitMs(left);
            return true;
        case 163:
            if (slideFrom_ >= 0)
                if (int left = int(outFrom_ + uncoverMs() - host.nowMs()); left > 0) host.waitMs(left);
            return true;
        default: return false;
        }
    }

    bool step(PartHost &host) override {
        if (menu_ == "title") return true;  // the backgrounds and the trim move
        // A menu's trims move while it's drawn: the frame after its last drawing is the last one asked for.
        if (std::exchange(frameDrawn_, false)) return true;
        if (std::exchange(rowDrawn_, false)) return true;  // and so the lower row's liquid
        int64_t now = host.nowMs();
        bool fading = now - fadeFrom_ < fadeMs_;
        // Moves that have ended stay, as where the face now is; a flipbook runs from the end of the update that showed it.
        for (const FaceMove &m : moves_)
            if (now < m.from + m.ms) fading = true;
        for (const auto &[slot, f] : shown_)
            if (f.flipCount > 1 && now - f.flipFrom < int64_t(f.flipCount) * f.flipTimes * f.flipMs) fading = true;
        bool sliding = slideFrom_ >= 0 && now - outFrom_ < uncoverMs();
        if (slideFrom_ >= 0 && !sliding) slideFrom_ = outFrom_ = -1;
        return fading || sliding;
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        if (layer == Layer::Menu && menu_ == "title") drawTitle(host, out);
        if (layer == Layer::UnderWindow) menuFrame_ = false;
        if (layer == Layer::MenuUnder) menuFrame_ = true;
        if (layer == Layer::MenuUnder && framed()) drawMenuTrims(host, out);
        if (layer == Layer::Menu && framed()) put(host, out, "obj\\e000_001.bmp", 4, 6, 6, 1, menuPlate(), 0);
        if (layer == Layer::ButtonRow) drawLiquid(host, out);
        if (layer == Layer::Window && !selecting_) {
            // The drink gauge at the window's right, a still picture on the PC at rest: msg_button.ini's `#openmenu` panel,
            // 94 left of the screen's right edge and 2 above the window's top (set 1, fitted on a PC shot).
            put(host, out, "obj\\h000_002.bmp", 1186, 510);
            drawFaces(host, out);
        }
        if (layer == Layer::OverWindow) drawSlide(host, out);
        if (layer == Layer::Screen) {
            bool menu = std::exchange(menuFrame_, false);
            menuOver_ = menu || menuAsked_ > 0;
            menuAsked_ = menu ? 0 : std::max(0, menuAsked_ - 1);
            if (selecting_ && !menuOver_) drawSelect(host, out);
        }
    }

    bool openScreen(PartHost &host, const std::string &name, const std::vector<int> &args) override {
        menu_ = name;
        if (name == "title") {
            titleFrom_ = host.screenMs();
            titleFirst_ = int(std::random_device{}() % 9);
            return true;
        }
        if (name != "select") return false;
        if (!args.empty()) {
            int ph = std::clamp(args[0], 0, kPhases - 1);
            host.setVariable(1, kWeekVar, ph / 2);
            host.setVariable(1, kHalfVar, ph % 2);
        }
        readInis(host);
        selecting_ = true;
        confirming_ = -1;
        host.redraw();
        return true;
    }

    void pointer(PartHost &host, int x, int y) override {
        px_ = x;
        py_ = y;
        if (selecting_ && !menuOver_) host.redraw();
    }

    bool press(PartHost &host, int x, int y, int button) override {
        if (!selecting_ || menuOver_) return false;
        readInis(host);
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
            if (sideRect(k).contains(x, y)) {
                if (k < 3) {
                    menuOver_ = true;
                    menuAsked_ = 2;
                }
                host.openMenu(kSideActions[k]);
                return true;
            }
        return true;
    }

    // Controller focus: the four cards, the side buttons, and yes and no while asking.
    bool focusSpots(PartHost &, std::vector<std::pair<int, int>> &spots) override {
        if (!selecting_ || menuOver_) return false;
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

    // The PC's saves at the select screen hold 6 where its saves in a scene hold 1 (flowActive); given 1 there, its PC
    // went on into the scenes of card 0 instead of opening the select screen (set 1, under Wine).
    void savePC(SaveData &d) const override {
        if (selecting_) d.flowActive = 6;
        if (lastCard_ >= 0) d.lastChoice = lastCard_;
        d.windowSprites.clear();
        for (const auto &[slot, f] : shown_) {
            if (f.flipCount || f.path.empty()) continue;
            SavedSprite record;
            record.slot = slot;
            record.path = f.path;
            record.x = float(kFaceX + f.x);
            record.y = float(kFaceY + f.y);
            d.windowSprites.push_back(record);
        }
    }

    // A PC save in a pick's scenes holds no chapter number, only what the PC keeps: the half-week (S 3000 and 3001, still
    // the one picked at), the card (the last result), and the hearts, a heroine's already counting the event she was
    // picked for (set 1, PC saves in every scene of the first four half-weeks' picks). The chapter is worked out from
    // those here and played when the story asks for chapter 0, which is what such a save's S 10904 holds.
    void loadPC(const SaveData &d) override {
        pendingChapter_ = 0;
        lastCard_ = d.lastChoice >= 0 && d.lastChoice <= 3 ? d.lastChoice : -1;
        bool faces = false;
        for (const SavedSprite &record : d.windowSprites) {
            if (record.path.empty() || record.flipFrame >= 0) continue;
            if (!faces) {
                shown_.clear();
                loaded_.clear();
                faces = true;
            }
            Face f;
            f.path = record.path;
            if (!std::isfinite(record.x) || !std::isfinite(record.y)) continue;
            f.x = int(std::clamp(record.x, -8192.0f, 8192.0f)) - kFaceX;
            f.y = int(std::clamp(record.y, -8192.0f, 8192.0f)) - kFaceY;
            shown_[record.slot] = loaded_[record.slot] = f;
        }
        auto var = [&](int n) {
            auto it = d.variables.find(n);
            return it == d.variables.end() ? 0 : it->second;
        };
        int ph = 2 * std::clamp(var(kWeekVar), -1, kPhases) + std::clamp(var(kHalfVar), -1, kPhases), card = d.lastChoice;
        if (d.flowActive != 1 || ph < 0 || ph >= kPhases || card < 0 || card > 3) return;
        const CardScenes &cs = kCards[ph][card];
        int before = 0;
        if (cs.heroine >= 0) {
            int after = std::clamp(var(kHeartVar + cs.heroine), 0, kHeartCap);
            before = after > kMaxHearts ? kMaxHearts : std::max(0, after - 1);
            for (int k = 0; k < 2; k++)
                if (normalizedName(d.scene) == std::string("scr\\") + kLate[cs.heroine][k] + ".txt") before = kMaxHearts + k;
        }
        int ending = 0;
        if (ph == kPhases - 1)
            for (int heroine = 0; heroine < 3; heroine++)
                if (var(kHeartVar + heroine) >= kMaxHearts) ending = heroine + 1;
        pendingChapter_ = chapterNumber(ph, card, before, ending);
    }

    // `chapter S 10904`: the scenes a pick plays, from the number choose() put there, or for 0, the one a PC save's
    // state gave (loadPC), which is then kept in S 10904 so VNDegen's own saves after it hold it.
    std::vector<std::string> chapter(PartHost &host, int number) override {
        if (number == 0 && pendingChapter_) {
            number = pendingChapter_;
            host.setVariable(1, kChapterVar, number);
            host.setVariable(1, kEndingVar, number % 10);
        }
        int phase = number / 1000 - 1, card = number / 100 % 10, hearts = number / 10 % 10, ending = number % 10;
        if (phase < 0 || phase >= kPhases || card > 3) return {};
        std::vector<std::string> out;
        // Each scene with the PC's number for it, which its saves there hold (set 1, a PC save in each scene of every
        // card's pick): numbers are given as `next` counts on, and 0 leaves a scene without one.
        int next = 0;
        auto add = [&](const std::vector<const char *> &list) {
            for (const char *scene : list) {
                std::string line = std::string("scr\\") + scene + ".txt";
                if (next > 0) line += " @" + std::to_string(next);
                out.push_back(line);
            }
        };
        auto addNumbered = [&](const std::vector<const char *> &list, int first, int rest) {
            for (size_t i = 0; i < list.size(); i++) {
                next = i == 0 ? first : rest;
                add({list[i]});
            }
        };
        const CardScenes &cs = kCards[phase][card];
        int end = kPhaseEnd[phase];
        if (cs.heroine < 0) {
            // A card of the fixed half-weeks has its own number and the common scenes count on from it; in the others a
            // card's scene takes the number just before the next select screen.
            next = kCardNumber[phase] ? kCardNumber[phase] : end;
            add(cs.scenes);
        } else if (hearts < kMaxHearts) {
            // A heroine's event: week 1's own numbers, or 801, 851 or 901 and 2 for each event before it, and the
            // rest of the event the number before the next select screen.
            int first = phase == 0 ? 21 + 10 * cs.heroine : 801 + 50 * cs.heroine + 2 * hearts;
            addNumbered(kEvents[cs.heroine][hearts], first, end);
        } else {
            next = end;
            add({kLate[cs.heroine][std::clamp(hearts - kMaxHearts, 0, 1)]});
        }
        if (kCardNumber[phase]) next = kCardNumber[phase] + 1;
        // The last pick: the heroine's last event and two common scenes, or with none at four hearts 029 alone, after
        // which the PC goes back to the title (set 1, a run picking no heroine).
        if (phase == kPhases - 1) {
            if (ending <= 0) {
                next = 9999;
                add(kFinal[0]);
                return out;
            }
            addNumbered(kFinal[std::min(ending, 3)], kFinalFirst + kFinalStep * (ending - 1), kFinalSecond);
            next = kFinalSecond;
            for (const char *scene : kAfter[phase]) {
                next++;
                add({scene});
            }
            return out;
        }
        for (const char *scene : kAfter[phase]) {
            add({scene});
            next++;
        }
        return out;
    }

    void save(std::vector<std::string> &lines) const override {
        lines.push_back("card " + std::to_string(lastCard_));
        if (selecting_) lines.push_back("select");
        auto keep = [&](const char *word, int slot, const Face &f) {
            if (f.flipCount) return;
            std::ostringstream line;
            line << word << ' ' << slot << ' ' << f.x << ' ' << f.y << ' ' << int(std::lround(f.scale * 1000)) << ' ' << int(f.scaleAboutMiddle) << ' ' << f.flipCount << ' ' << f.flipMs << ' ' << int(f.flipLoop) << ' ' << f.path;
            lines.push_back(line.str());
        };
        for (const auto &[slot, f] : shown_) keep("shown", slot, f);
        for (const auto &[slot, f] : loaded_) keep("loaded", slot, f);
    }

    void load(const std::vector<std::string> &lines) override {
        if (lines.empty()) return;
        loaded_.clear();
        shown_.clear();
        from_.clear();
        moves_.clear();
        fadeMs_ = 0;
        selecting_ = false;
        confirming_ = -1;
        for (const std::string &line : lines) {
            std::istringstream in(line);
            std::string word;
            Face f;
            int slot = 0;
            in >> word;
            if (word == "select") {
                selecting_ = true;
            } else if (word == "card") {
                in >> lastCard_;
                if (lastCard_ < 0 || lastCard_ > 3) lastCard_ = -1;
            } else if (word == "face") {
                if (in >> slot >> f.x >> f.y >> f.path) shown_[slot] = loaded_[slot] = f;
            } else if (word == "shown" || word == "loaded") {
                int scale = 1000, about = 1, loop = 0;
                if (in >> slot >> f.x >> f.y >> scale >> about >> f.flipCount >> f.flipMs >> loop >> f.path) {
                    f.scale = float(scale) / 1000;
                    f.scaleAboutMiddle = about != 0;
                    f.flipLoop = loop != 0;
                    f.flipCount = std::max(0, f.flipCount);
                    f.flipMs = std::max(0, f.flipMs);
                    (word == "shown" ? shown_ : loaded_)[slot] = f;
                }
            }
        }
    }

    void reset() override {
        loaded_.clear();
        shown_.clear();
        from_.clear();
        moves_.clear();
        fadeMs_ = 0;
        slideFrom_ = outFrom_ = -1;
        selecting_ = false;
        confirming_ = -1;
        pendingChapter_ = 0;
        lastCard_ = -1;
        menuFrame_ = menuOver_ = false;
        menuAsked_ = 0;
        menu_.clear();
    }

private:
    // The menus' shared frame: which name plate of obj\\e000_001 a table screen shows, or -1 for a screen without one.
    bool framed() const { return menuPlate() >= 0 && menuPlate() != 2 && menuPlate() != 3; }
    int menuPlate() const {
        static const char *const kNames[] = {"save", "load", "cg", "scene", "music", "config"};
        for (int i = 0; i < 6; i++)
            if (menu_.rfind(kNames[i], 0) == 0) return i;
        return -1;
    }

    // A menu's trims along the top and bottom, under its cards and buttons, and the screen's name plate at 4, 6 (set 1,
    // fitted over the screen's paper on PC shots and a 30 fps recording, within 0.01 of 255): the top trim at y -36,
    // and at 0 on Save and Load, where the page buttons hang from it, moving left; the bottom one at y 662, moving
    // right; both 8 pixels a frame of the recording, 240 a second. Where along their strips the trims start isn't
    // measured.
    void drawMenuTrims(PartHost &host, Canvas &out) {
        frameDrawn_ = true;
        int moved = int(std::max<int64_t>(0, host.screenMs()) * kMenuTrimSpeed / 1000);
        if (ImageRef top = host.image("obj\\e004_000.bmp"))
            for (int x = -(moved % top->w); x < out.w; x += top->w) drawPart(out, *top, x, menuPlate() < 2 ? 0 : -36, 0, 0, top->w, top->h, 1);
        if (ImageRef bottom = host.image("obj\\e004_001.bmp"))
            for (int x = moved % bottom->w - bottom->w; x < out.w; x += bottom->w) drawPart(out, *bottom, x, 662, 0, 0, bottom->w, bottom->h, 1);
        // Config's first tab has the Questionnaire Privilege panel over the bottom trim, under its buttons.
        if (menu_ == "config") put(host, out, "sys_3\\j200_000.bmp", 53, 653);
    }

    // The red liquid behind the lower row of message buttons (set 1, from a 60 fps recording under Wine): three wave
    // strips, each repeating across the screen with its foot on the screen's bottom edge, the dark one under two
    // translucent ones, moving left 2, 6 and 10 pixels a frame of the recording (120, 360 and 600 a second); a frame
    // drawn so is within 1 of 255 of the PC's. It rises from the bottom edge in about a sixth of a second as the row
    // opens; that rise is drawn evenly, which the PC's isn't quite. The PC's bubbles in it aren't drawn. It passes in
    // front of the gauge's foot, as on the PC.
    void drawLiquid(PartHost &host, Canvas &out) {
        static const struct { const char *path; int speed; } kWaves[] = {{"obj\\h001_000.bmp", 120}, {"obj\\h001_001.bmp", 360}, {"obj\\h001_002.bmp", 600}};
        int64_t now = host.nowMs();
        if (now - rowSeen_ > kRowGapMs || now < rowSeen_) rowFrom_ = now;
        rowSeen_ = now;
        rowDrawn_ = true;
        int sunk = int(std::max<int64_t>(0, kRowRiseMs - (now - rowFrom_)) * kRowRise / kRowRiseMs);
        for (const auto &wave : kWaves)
            if (ImageRef strip = host.image(wave.path)) {
                int moved = int(now * wave.speed / 1000 % strip->w);
                for (int x = -moved; x < out.w; x += strip->w) drawPart(out, *strip, x, out.h - strip->h + sunk, 0, 0, strip->w, strip->h, 1);
            }
    }

    void drawTitle(PartHost &host, Canvas &out) {
        // The bonus story disc has the game's title with three silhouettes and its own logo over the game's.
        bool disc = host.exists("obj\\d300_001.bmp");
        bool six = !disc && host.variable(0, 83) != 0;
        int64_t t = std::max<int64_t>(0, host.screenMs() - titleFrom_);
        int shown = int(t / kTitleMs), into = int(t % kTitleMs);
        auto bg = [&](int k) {
            char path[32];
            std::snprintf(path, sizeof path, "obj\\%s_%03d.bmp", six ? "d008" : "d004", (titleFirst_ + k) % 9);
            return host.image(path);
        };
        // The pink under it all, which the backgrounds' holes show as the silhouettes (within 0.24 of 255).
        put(host, out, "obj\\d000_000.bmp", 0, 0);
        drawSparkles(host, out, t);
        if (ImageRef now = bg(shown)) drawPart(out, *now, 0, 0, 0, 0, now->w, now->h, 1);
        if (shown > 0 && into < kTitleFadeMs)
            if (ImageRef before = bg(shown - 1)) drawPart(out, *before, 0, 0, 0, 0, before->w, before->h, 1 - float(into) / kTitleFadeMs);
        put(host, out, six ? "obj\\d009_000.bmp" : "obj\\d005_000.bmp", 0, 0);
        put(host, out, six ? "obj\\d011_000.bmp" : "obj\\d007_000.bmp", 24, 363);
        put(host, out, six ? "obj\\d999_003.bmp" : "obj\\d999_002.bmp", six ? 45 : 36, 662);
        // The top trim, a strip that repeats every 600 pixels, moving left; the bottom one still.
        if (ImageRef top = host.image(six ? "obj\\d010_000.bmp" : "obj\\d006_000.bmp")) {
            int shift = int(t * kTitleTrimSpeed / 1000 % kTitleTrimPeriod);
            for (int x = -shift; x < out.w; x += top->w) drawPart(out, *top, x, 0, 0, 0, top->w, top->h, 1);
        }
        if (ImageRef bottom = host.image(six ? "obj\\d010_001.bmp" : "obj\\d006_001.bmp"))
            for (int x = -kTitleBottomShift; x < out.w; x += bottom->w) drawPart(out, *bottom, x, 720 - bottom->h, 0, 0, bottom->w, bottom->h, 1);
        put(host, out, "obj\\d000_001.bmp", 16, 5);
        if (disc) put(host, out, "obj\\d300_001.bmp", 16, 5);
        put(host, out, six ? "obj\\d010_002.bmp" : "obj\\d006_002.bmp", 842, 694);
        drawNews(host, out, t);
    }

    // Light dots rising inside the silhouettes, drawn on the pink under the backgrounds, whose holes cut them to the
    // silhouettes' shapes. In the PC's style and not its numbers (set 1, tracked in a 10 fps recording under Wine):
    // about nine at a time, the seven sizes of d003, each rising about three times its width a second (the PC's small
    // ones 26 to 38 pixels a second, its large ones 60 to 88) with a slight drift, at 30 to 70 percent. Where and when
    // the PC starts each one isn't known, so they come round on fixed turns here.
    void drawSparkles(PartHost &host, Canvas &out, int64_t t) {
        for (int k = 0; k < kSparkles; k++) {
            char path[32];
            std::snprintf(path, sizeof path, "obj\\d003_%03d.bmp", k % 7);
            ImageRef dot = host.image(path);
            if (!dot) continue;
            int speed = 3 * dot->w, turnMs = kSparkleRise * 1000 / speed;
            int64_t at = t + int64_t(k) * 7919 % turnMs;
            uint32_t turn = uint32_t(at / turnMs) * 2654435761u + uint32_t(k) * 40503u;
            turn ^= turn >> 15;
            float into = float(at % turnMs) / 1000;
            int x = kSparkleX + int(turn % kSparkleW) + int((int(turn >> 8 & 31) - 15) * into * 0.6f);
            float alpha = 0.3f + float(turn >> 16 & 255) / 255 * 0.4f;
            drawPart(out, *dot, x, kSparkleFrom - int(into * speed), 0, 0, dot->w, dot->h, alpha);
        }
    }

    // The additional edition's Information box, from sys_2.pak (set 1, fitted on shots and a 30 fps recording of the
    // release's second exe under Wine): the box at 770, 57, and in its window, 414 wide from x 803, one line of text
    // at y 91 moving left 4 pixels a frame of the recording (120 a second) and coming round every 414, under the
    // window's shade. Where along its round the text starts isn't measured.
    void drawNews(PartHost &host, Canvas &out, int64_t t) {
        ImageRef box = host.image("sys_2\\obj\\d401_000.bmp"), text = host.image("sys_2\\obj\\d401_001.bmp");
        if (!box || !text) return;
        drawPart(out, *box, 770, 57, 0, 0, box->w, box->h, 1);
        int moved = int(t * kNewsSpeed / 1000 % kNewsW);
        for (int x = kNewsX + kNewsW - moved - kNewsW; x < kNewsX + kNewsW; x += kNewsW) {
            int from = std::max(x, kNewsX), to = std::min(x + text->w, kNewsX + kNewsW);
            if (to > from) drawPart(out, *text, from, 91, from - x, 0, to - from, text->h, 1);
        }
        put(host, out, "sys_2\\obj\\d401_002.bmp", kNewsX, 84);
    }

    static std::string lower(std::string text) {
        for (char &c : text) c = char(std::tolower(uint8_t(c)));
        return text;
    }

    void dropMoves(int slot) {
        moves_.erase(std::remove_if(moves_.begin(), moves_.end(), [&](const FaceMove &m) { return m.slot == slot; }), moves_.end());
    }

    // cfg\anime_bustup_cfg.ini: `#anime PICTURES FIRST MS ...` for each flipbook the face layer plays.
    void readFlipbooks(PartHost &host) {
        if (flipbooksRead_) return;
        flipbooksRead_ = true;
        auto bytes = host.read("cfg\\anime_bustup_cfg.ini");
        if (!bytes) return;
        std::istringstream in(std::string(bytes->begin(), bytes->end()));
        for (std::string line; std::getline(in, line);) {
            std::istringstream words(line);
            std::string word, path;
            int count = 0, ms = 0;
            if (!(words >> word >> count >> path >> ms) || word != "#anime" || count <= 0 || ms <= 0) continue;
            flipbooks_[lower(path)] = {count, ms};
            // `... N SOUND VOLUME`: the sound a mark plays as it shows, as the scene's flipbooks do (the PC reads it with
            // the first picture, in a file trace under Wine).
            int n = 0, volume = 100;
            std::string sound;
            if (words >> n >> sound >> volume && sound.rfind("//", 0) != 0) flipSounds_[lower(path)] = {sound, volume};
        }
    }

    // A flipbook's picture: the first's name with its last number counted on.
    static std::string flipPicture(const std::string &first, int index) {
        size_t dot = first.rfind('.');
        if (dot == std::string::npos || dot < 3) return first;
        char digits[8];
        std::snprintf(digits, sizeof digits, "%03d", index);
        return first.substr(0, dot - 3) + digits + first.substr(dot);
    }

    void drawFaces(PartHost &host, Canvas &out) {
        int64_t now = host.nowMs();
        float t = fadeMs_ > 0 ? std::clamp(float(now - fadeFrom_) / float(fadeMs_), 0.0f, 1.0f) : 1.0f;
        auto drawSet = [&](const std::map<int, Face> &set, float alpha) {
            if (alpha <= 0) return;
            for (const auto &[slot, f] : set) {
                float x = float(kFaceX + f.x), y = float(kFaceY + f.y);
                for (const FaceMove &m : moves_) {
                    if (m.slot != slot || now <= m.from) continue;
                    float p = m.ms > 0 ? std::min(1.0f, float(now - m.from) / float(m.ms)) : 1.0f;
                    if (m.easeOut) p = 1 - (1 - p) * (1 - p);
                    x += float(m.dx) * p;
                    y += float(m.dy) * p;
                }
                std::string path = f.path;
                if (f.flipCount > 0) {
                    if (now < f.flipFrom) continue;
                    int64_t step = (now - f.flipFrom) / std::max(1, f.flipMs);
                    if (step >= int64_t(f.flipCount) * f.flipTimes) continue;  // played: nothing is left of it
                    path = flipPicture(f.path, int(step % f.flipCount));
                }
                ImageRef img = host.image(f.flipCount > 0 ? path : host.shownPath(path));
                if (!img) continue;
                if (f.scale == 1) {
                    drawPart(out, *img, int(std::lround(x)), int(std::lround(y)), 0, 0, img->w, img->h, alpha);
                } else {
                    // The unscaled picture's top left is the place; scaling keeps its middle there.
                    float cx = x + float(img->w) / 2, cy = y + float(img->h) / 2;
                    if (!f.scaleAboutMiddle) {
                        cx = x + float(img->w) * f.scale / 2;
                        cy = y + float(img->h) * f.scale / 2;
                    }
                    drawImage(out, *img, cx, cy, f.scale, f.scale, 0, alpha);
                }
            }
        };
        // A face that stays is drawn once, in full; one going fades out as one coming fades in.
        std::map<int, Face> going, coming, staying;
        for (const auto &[slot, f] : from_) (shown_.count(slot) && shown_.at(slot).same(f) ? staying : going)[slot] = f;
        for (const auto &[slot, f] : shown_)
            if (!staying.count(slot)) coming[slot] = f;
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
            int in = slideIn_ * 1000 / 60, away = slideOut_ * 1000 / 60;
            b.inDelay = rnd(0, in * 30 / 100);
            b.inMs = rnd(in * 40 / 100, in * 50 / 100);
            b.outDelay = rnd(0, away * 30 / 100);
            b.outMs = rnd(away * 50 / 100, away * 70 / 100);
            bars_.push_back(b);
            y += b.h;
        }
        slideFrom_ = host.nowMs();
        outFrom_ = caption_.empty() ? slideFrom_ + coverMs() + holdMs_ : slideFrom_ + int64_t(slideIn_ + captionIn_ + captionStay_ + slideOut_) * 1000 / 60;
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
                int64_t outMs = now - outFrom_ - b.outDelay - (layer == 0 ? kFollowMs : 0);
                int left = int(progress(outMs, b.outMs) * w), right = int(progress(inMs, b.inMs) * w);
                if (right <= left) continue;
                float sx = float(right - left) / float(img->w), sy = float(b.h) / float(img->h);
                drawImage(out, *img, (left + right) / 2.0f, b.y + b.h / 2.0f, sx, sy, 0, 1);
            }
        }
        ImageRef caption = caption_.empty() ? nullptr : host.image(caption_);
        if (!caption) return;
        int cw = caption->w / captionCols_;
        int64_t t = now - slideFrom_, arrive = int64_t(slideIn_ + captionIn_) * 1000 / 60 + kCaptionInMs;
        int64_t leave = arrive + int64_t(captionStay_) * 1000 / 60 - kCaptionInMs, away = int64_t(slideOut_) * 1000 / 60;
        float rest = float(w - cw) / 2 - kCaptionBack, x = 0;
        if (t < arrive - kCaptionInMs || t >= leave + away) return;
        if (t < arrive) {
            float u = 1 - float(arrive - t) / float(kCaptionInMs);
            x = float(-cw) + (rest + float(cw)) * (1 - (1 - u) * (1 - u));
        } else if (t < leave) {
            float u = float(t - arrive) / float(std::max<int64_t>(1, leave - arrive));
            x = rest + float(kCaptionDrift) * u * u;
        } else {
            float u = float(t - leave) / float(std::max<int64_t>(1, away));
            x = rest + float(kCaptionDrift) + (float(w) - rest - float(kCaptionDrift)) * u * u;
        }
        drawPart(out, *caption, int(std::lround(x)), (host.height() - caption->h) / 2, captionCol_ * cw, 0, cw, caption->h, 1);
    }

    // The select screen.
    struct Card {
        int character = 7, place = 0;  // n005_000's column; PosNameIndex, -1 for the heroine's own by her hearts
    };

    static Rect cardRect(int c) { return {kCardX[c], kCardY[c], kCardW, kCardH}; }
    static Rect sideRect(int k) { return {kSideX, kSideY + kSideStep * k, 108, 52}; }
    static Rect yesRect() { return {339, 293, 592, 72}; }
    static Rect noRect() { return {339, 380, 592, 72}; }

    static int chapterNumber(int phase, int card, int heartsBefore, int ending) { return 1000 * (phase + 1) + 100 * card + 10 * heartsBefore + ending; }
    static std::string normalizedName(const std::string &cp932) {
        std::string out = cp932;
        for (char &c : out) c = char(std::tolower(uint8_t(c)));
        return out;
    }

    int phase(PartHost &host) const { return std::clamp(2 * std::clamp(host.variable(1, kWeekVar), 0, kPhases) + std::clamp(host.variable(1, kHalfVar), 0, kPhases), 0, kPhases - 1); }
    int hearts(PartHost &host, int heroine) const { return std::clamp(host.variable(1, kHeartVar + heroine), 0, kHeartCap); }

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
        if (cs.heroine >= 0 && h < kHeartCap) host.setVariable(1, kHeartVar + cs.heroine, h + 1);
        int ending = 0;
        if (ph == kPhases - 1)
            for (int heroine = 0; heroine < 3; heroine++)
                if (hearts(host, heroine) >= kMaxHearts) ending = heroine + 1;
        host.setVariable(1, kChapterVar, chapterNumber(ph, c, h, ending));
        host.setVariable(1, kEndingVar, ending);
        lastCard_ = c;
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
        readInis(host);
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
                for (int k = 0; k < std::min(hearts(host, card.character), kMaxHearts); k++) put(host, out, "obj\\n006_001.bmp", x + 250 + 44 * k, y + 161);
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

    bool selecting_ = false, inisRead_ = false;
    int pendingChapter_ = 0, titleFirst_ = 0;
    int64_t titleFrom_ = 0;
    std::string menu_;
    bool frameDrawn_ = false;
    bool menuFrame_ = false, menuOver_ = false;
    int menuAsked_ = 0;
    int lastCard_ = -1;
    bool rowDrawn_ = false;
    int64_t rowFrom_ = 0, rowSeen_ = -1000000;
    int confirming_ = -1, px_ = -1, py_ = -1;
    std::array<std::array<Card, 4>, kPhases> phaseCards_{};
    std::array<std::vector<int>, 3> charPlaces_;

    std::map<int, Face> loaded_, shown_, from_;
    std::vector<FaceMove> moves_;
    std::map<std::string, std::pair<int, int>> flipbooks_;
    std::map<std::string, std::pair<std::string, int>> flipSounds_;
    bool flipbooksRead_ = false;
    int64_t fadeFrom_ = 0;
    int fadeMs_ = 0;
    std::string panels_[2];
    std::string caption_;
    int slideIn_ = 30, slideOut_ = 30, captionIn_ = 0, captionStay_ = 0, captionCols_ = 1, captionCol_ = 0;
    int holdMs_ = 0;
    std::vector<Bar> bars_;
    int64_t slideFrom_ = -1, outFrom_ = -1;
    uint32_t slides_ = 0;
};

}  // namespace

std::unique_ptr<GamePart> makeMankitsuPart() { return std::make_unique<MankitsuPart>(); }

}  // namespace neko
