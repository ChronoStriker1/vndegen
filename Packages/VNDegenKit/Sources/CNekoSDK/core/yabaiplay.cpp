// The `yabaiplay` game part: Test Game 96's story pieces that its PC keeps in the exe (set 2).
//   chapter N   the scene scripts cfg\chapter_file.ini lists under `#chapter N`, in order
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <random>
#include <sstream>

#include "font.h"
#include "parts.h"
#include "sjis.h"

namespace neko {
namespace {

// `#chapter N`, then one `scr\\FILE.txt` a line (the file's own doubled backslash), then `##`. `//` starts a comment.
std::map<int, std::vector<std::string>> parseChapters(const std::string &text) {
    std::map<int, std::vector<std::string>> chapters;
    std::istringstream in(text);
    std::vector<std::string> *open = nullptr;
    for (std::string line; std::getline(in, line);) {
        if (size_t cut = line.find("//"); cut != std::string::npos) line.erase(cut);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line.erase(0, start);
        if (line.rfind("#chapter", 0) == 0) {
            open = &chapters[std::atoi(line.c_str() + 8)];
        } else if (line == "##") {
            open = nullptr;
        } else if (open) {
            // A file may be followed by `,0` or `,1` (whether the swimsuit choice changes it); 96 writes none.
            // The name is kept as written, doubled backslash and all: the PC's saves and read.sav name scenes that way,
            // and its scene gallery unlocks by those names.
            open->push_back(line.substr(0, line.find(',')));
        }
    }
    return chapters;
}

// `Key<tab>value` lines, as question_0N.ini has them; `//` starts a comment.
std::map<std::string, std::string> parseKeys(const std::string &text) {
    std::map<std::string, std::string> keys;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) {
        if (size_t cut = line.find("//"); cut != std::string::npos) line.erase(cut);
        std::istringstream words(line);
        std::string key, value;
        if (words >> key >> value) keys[key] = value;
    }
    return keys;
}

enum : int {
    kTruth = 15100,      // 1..5
    kStage = 15101,      // + heroine: 1..3, 4 when she's done
    kHour = 15104,
    kDay = 15105,
    kSolved = 15106,     // right answers this truth
    kHoursLeft = 15107,
    kShown = 15108,      // the heroine on screen, 0 Ayano, 1 Misuzu, 2 Rina
    kEventDone = 15109,  // truth 1's 12:00 scene has played
    kTreeChapter = 15110,
    kTreeAction = 15111,  // from the tree: 1 replay, 2 ask
    kAsked = 15130,      // + 9 * (truth - 1) + 3 * heroine + (stage - 1): bit q when question q was asked
};
const char *const kNames[3] = {"Ayano", "Misuzu", "Rina"};

// The PC's own variables for the same state, read from its saves (decoded with the lead's kYabai layout): the truth, the
// clock's hour, the hours left, the truths done, each heroine's stages solved in each truth (0 to 3), her total solved,
// and a flag for each question asked. The hub keeps them in step with its own, so its saves load on the PC and the PC's
// load here.
enum : int {
    kPCTruth = 2000, kPCHour = 2001, kPCHoursLeft = 2002, kPCTruthsDone = 2003,
    kPCSolved = 2005,      // + 5 * heroine + (truth - 1)
    kPCTotal = 2070,       // + heroine
    kPCAsked = 3000,       // + 500 * heroine + 100 * (truth - 1) + 10 * (stage - 1) + question
    kPCChapter = 1000,     // the chapter playing
    kPCMode = 105,         // 2 at the hub, 4 in a scene, 8 at a pick, 11 in the tutorial
    kPCLastAsked = 2030,   // the asked flag's number of the question last asked, set as it's asked
    kPCTalked = 2045,      // 1 once a truth's questions have all been answered
    kPCTruthDone = 2100,   // + (truth - 1): 1 once that truth's questions have all been answered
};

// What the hub's screen gives the story table.
enum HubResult { kPlay = 1, kAllSolved = 2, kTimeUp = 3, kTimeEvent = 4, kOpenTree = 9 };

class YabaiPlayPart : public GamePart {
public:
    bool openScreen(PartHost &host, const std::string &name, const std::vector<int> &args) override {
        // `open hubstart T`: truth T begins, with the hours, start and order its `truth` line under `screen hub` gives.
        if (name == "hubstart") {
            startTruth(host, args.empty() ? 1 : args[0]);
            host.finishScreen(0);
            return true;
        }
        if (name == "staff") {
            staff_ = std::max(1, std::min(3, args.empty() ? 1 : args[0]));
            staffFrom_ = host.nowMs();
            staffResumeMs_ = -1;
            loadStaff(host);
            if (steps_.empty()) {
                staff_ = 0;
                host.finishScreen(0);
            }
            host.redraw();
            return true;
        }
        // `open ask N`: one of the PC's own questions on a band (1 the interrogation tutorial, 2 the vote's, 3 save
        // before the ending), はい 1 and いいえ 0; 4 the ending's three ways, 0 to 2. Placed as on the PC (Wine).
        if (name == "ask") {
            ask_ = args.empty() ? 1 : args[0];
            hoverAsk_ = -1;
            host.redraw();
            return true;
        }
        // `open hubsync`: a PC save, which holds only the PC's variables, gives the hub's own, for the tree and the vote
        // when the story goes on at a pick.
        if (name == "hubsync") {
            loadQuestions(host);
            if (!host.variable(1, kTruth) && host.variable(1, kPCTruth)) fromPC(host);
            host.finishScreen(0);
            return true;
        }
        if (name != "hub") return false;
        loadQuestions(host);
        if (!host.variable(1, kTruth) && host.variable(1, kPCTruth)) fromPC(host);
        // Back from a question's scene, or a save made inside one: the PC counts a question once its scene is over (its
        // save inside the scene still has the hour and the stage from before), so it's counted here. A replay's
        // question is already marked asked, and isn't counted again.
        if (int c = host.variable(1, kPCChapter); host.variable(1, kPCMode) == 4 && c >= 1000 && c < 4000 && (c % 1000) / 100 + 1 == truth(host)) {
            int heroine = c / 1000 - 1, stage = (c % 100) / 10 + 1, question = c % 10;
            if (heroine < 3 && question < 3 && stage == host.variable(1, kStage + heroine) && !(asked(host, heroine) & (1 << question)))
                account(host, heroine, question);
        }
        host.setVariable(1, kPCMode, 2);
        int action = host.variable(1, kTreeAction), picked = host.variable(1, kTreeChapter);
        host.setVariable(1, kTreeAction, 0);
        host.setVariable(1, kTreeChapter, 0);
        if (action == 2 && picked > 0) {
            int heroine = picked / 1000 - 1, question = picked % 10, stage = (picked % 100) / 10 + 1;
            bool here = heroine >= 0 && heroine < 3 && question < 3 && (picked % 1000) / 100 + 1 == truth(host) && stage == host.variable(1, kStage + heroine);
            if (here && stage <= 3 && askable(host, heroine, question)) return ask(host, heroine, question);
        }
        if (pendingDevil_) {
            int devil = pendingDevil_;
            pendingDevil_ = 0;
            return finishWith(host, kPlay, devil);
        }
        if (solvedAll(host)) return finishWith(host, kAllSolved, 0);
        if (host.variable(1, kHoursLeft) <= 0) return finishWith(host, kTimeUp, 0);
        // Truth 1's time event, once its clock has reached 12:00 (seen under Wine after the question that took it there).
        if (truth(host) == 1 && host.variable(1, kHour) >= 12 && !host.variable(1, kEventDone)) {
            host.setVariable(1, kEventDone, 1);
            return finishWith(host, kTimeEvent, 0);
        }
        if (int shown = shownHeroine(host); host.variable(1, kStage + shown) > 3) host.setVariable(1, kShown, openFrom(host, shown));
        bool anyLeft = false;
        for (int h = 0; h < 3; h++)
            for (int q = 0; q < 3; q++) anyLeft = anyLeft || askable(host, h, q);
        if (!anyLeft) {
            host.log("the hub has no question left to ask, and not every answer: its question files may be missing");
            return finishWith(host, kTimeUp, 0);
        }
        open_ = true;
        hoverQ_ = -1;
        bubblesFor_ = -1;
        pointerSeen_ = false;
        moveBubbles(host);
        refreshSpots(host);
        host.redraw();
        return true;
    }

    // The bubbles move on the story's clock, so a menu over the hub holds them still.
    bool step(PartHost &host) override {
        if (orphan_) {
            orphan_ = false;
            host.finishScreen(0);
            return true;
        }
        if (staff_) {
            int64_t ms = staffMs(host);
            if (steps_.empty() || ms >= staffEndMs()) {
                staff_ = 0;
                host.finishScreen(0);
            }
            return true;
        }
        if (!open_ || ask_) return false;
        moveBubbles(host);
        hover(host);
        refreshSpots(host);
        return true;
    }

    bool press(PartHost &host, int x, int y, int button) override {
        if (staff_) return true;
        if (ask_) {
            if (button != 0) return true;
            int hit = askOption(x, y);
            if (hit >= 0) {
                int result = ask_ == 4 ? hit : hit == 0 ? 1 : 0;
                bool save = ask_ == 3 && result == 1;
                ask_ = 0;
                host.finishScreen(result);
                // はい to saving opens the save menu, and the story goes on once it closes, as on the PC.
                if (save) host.openMenu("nav_save");
            }
            return true;
        }
        if (!open_ || button != 0) return open_;
        if (inside(x, y, kTreeButton)) return finishWith(host, kOpenTree, 0);
        for (int h = 0; h < 3; h++)
            if (inside(x, y, kTab[h]) && host.variable(1, kStage + h) <= 3) {
                host.setVariable(1, kShown, h);
                host.redraw();
                return true;
            }
        int shown = shownHeroine(host);
        if (int q = bubbleAt(host, x, y); q >= 0) return ask(host, shown, q);
        return true;
    }

    // Controller and keyboard: the questions still to ask, the heroines' tabs and the tree's button; on a question band,
    // its answers.
    bool focusSpots(PartHost &host, std::vector<std::pair<int, int>> &spots) override {
        if (staff_) return true;
        if (ask_) {
            for (int i = 0; i < (ask_ == 4 ? 3 : 2); i++) {
                if (ask_ == 4) spots.push_back({640, 298 + 55 * i});
                else spots.push_back({576 + 128 * i, 409});
            }
            return true;
        }
        if (!open_) return false;
        moveBubbles(host);
        int shown = shownHeroine(host);
        for (int q = 0; q < 3; q++) {
            int x = 0, y = 0;
            if (pressPoint(host, shown, q, x, y)) spots.push_back({x, y});
        }
        for (int h = 0; h < 3; h++)
            if (host.variable(1, kStage + h) <= 3) spots.push_back({kTab[h][0] + kTab[h][2] / 2, kTab[h][1] + kTab[h][3] / 2});
        spots.push_back({kTreeButton[0] + kTreeButton[2] / 2, kTreeButton[1] + kTreeButton[3] / 2});
        return true;
    }

    void pointer(PartHost &host, int x, int y) override {
        if (ask_) {
            int hit = askOption(x, y);
            if (hit != hoverAsk_) hoverAsk_ = hit, host.redraw();
            return;
        }
        if (!open_) return;
        pointerX_ = x, pointerY_ = y, pointerSeen_ = true;
        hover(host);
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        if (layer != Layer::Screen) return;
        if (staff_) {
            drawStaff(host, out);
            return;
        }
        if (ask_) {
            drawAsk(host, out);
            return;
        }
        if (!open_) return;
        loadQuestions(host);
        int shown = shownHeroine(host), stage = host.variable(1, kStage + shown);
        put(host, out, "obj\\l000_000.bmp", 0, 0);
        // The red rim and the dark vignette over it, fitted to the PC's hub at three quarters. The rim is wider than the
        // screen and may drift on the PC; it's held still here.
        if (ImageRef rim = host.image("obj\\l000_001.bmp")) drawPart(out, *rim, -125, 0, 0, 0, rim->w, rim->h, 0.75f);
        if (ImageRef dark = host.image("obj\\l000_002.bmp")) drawPart(out, *dark, 0, 0, 0, 0, dark->w, dark->h, 0.75f);
        // The dark band behind the clock and the line under it, as on the PC's hub.
        put(host, out, "obj\\l000_003.bmp", 0, 0);
        put(host, out, "obj\\l000_004.bmp", 244, 45);
        // The heroine shown, l006 sheet 2 * heroine: 16 frames 525 wide, frame 3 * (truth - 1) + (stage - 1), at x 0 (matched
        // on the PC's hub). The sheet after it is the same frames with her eyes closed; when the PC uses it isn't measured.
        cell(host, out, "obj\\l006_00" + std::to_string(2 * shown) + ".bmp", 0, 0,
             525 * (3 * (truth(host) - 1) + std::min(3, std::max(1, stage)) - 1), 0, 525, 720);
        if (stage <= 3) {
            moveBubbles(host);
            drawThoughts(host, out, shown, stage);
        }
        put(host, out, "obj\\l001_000.bmp", 256, 21);
        put(host, out, "obj\\l005_000.bmp", 0, 592);
        put(host, out, "obj\\l004_001.bmp", 0, 667);  // 過去の自白内容を確認することが出来ます。
        cell(host, out, "obj\\l004_000.bmp", kTreeButton[0], kTreeButton[1], 0, 0, 190, 95);
        drawClock(host, out);
        // The heroine tabs: row 2 for the one shown, 4 (grey) once she's done, else row 0.
        for (int h = 0; h < 3; h++) {
            int row = h == shown ? 2 : host.variable(1, kStage + h) > 3 ? 4 : 0;
            cell(host, out, "obj\\l002_003.bmp", kTab[h][0], kTab[h][1], 118 * h, 58 * row, 118, 58);
        }
        drawMiniTree(host, out, shown);
        put(host, out, "obj\\l002_002.bmp", 1002, 19);
        cell(host, out, "obj\\l001_001.bmp", 465, 21, 123 * (truth(host) - 1), 0, 123, 25);
        cell(host, out, "obj\\l005_001.bmp", 669, 625, 216 * (truth(host) - 1), 0, 216, 73);
        cell(host, out, "obj\\l005_002.bmp", 899, 607, 374 * (truth(host) - 1), 0, 374, 91);
        // A locked stage's questions are all dark, as asked ones are.
        bool shut = locked(host, shown);
        int bits = shut ? 7 : asked(host, shown);
        if (stage <= 3) moveBubbles(host);
        float seconds = float(host.nowMs() - bubblesFrom_) / 1000;
        for (int q = 0; q < 3 && stage <= 3; q++) {
            // Frames: lit for a question still to ask, dark once asked (5 states down). Under the pointer, the frame's
            // glowing row 1 and the panel's lit row 1 (hover state, as seen on the PC).
            int state = bits & (1 << q) ? 4 : q == hoverQ_ ? 1 : 0;
            const Bubble &b = bubbles_[q];
            float degrees = kRock * std::sin(2 * kPi * seconds / kRockSeconds[q] + rockPhase_[q]);
            // The PC's tilt is measured anticlockwise; drawImage turns clockwise.
            if (ImageRef picture = bubblePicture(host, shown, stage, q, state))
                drawImage(out, *picture, b.x, b.y, 1, 1, -degrees * kPi / 180, 1);
        }
        if (shut) put(host, out, "obj\\l015_001.bmp", 326, 260);
    }

    // The plate's Auto and Skip are drawn disabled over the hub, as the PC draws them there (a PC shot under Wine with
    // the plate up: both in their sheet's last row).
    int actionLit(const std::string &name) const override { return open_ && !ask_ && (name == "Auto" || name == "Skip") ? 2 : -1; }
    bool showsPlate() const override { return !staff_; }
    bool coversFrame() const override { return !ask_; }

    std::string debugStatus() const override {
        if (staff_) return "staff " + std::to_string(staff_);
        if (ask_) return "ask " + std::to_string(ask_);
        if (!open_) return {};
        std::string words = "hub";
        for (int q = 0; q < 3; q++) words += " " + std::to_string(spotX_[q]) + "," + std::to_string(spotY_[q]);
        return words;
    }

    void save(std::vector<std::string> &lines) const override {
        if (staff_) lines.push_back("staff " + std::to_string(staff_) + " " + std::to_string(std::max<int64_t>(0, staffSeenMs_)));
        else if (ask_) lines.push_back("ask " + std::to_string(ask_));
        else if (open_) lines.push_back("hub");
    }
    void load(const std::vector<std::string> &lines) override {
        for (const std::string &line : lines) {
            std::istringstream words(line);
            std::string kind;
            int number = 0;
            words >> kind >> number;
            if (kind == "hub") {
                open_ = true;
            } else if (kind == "ask" && number >= 1 && number <= 4) {
                ask_ = number;
            } else if (kind == "staff" && number >= 1 && number <= 3) {
                int64_t ms = 0;
                words >> ms;
                staff_ = number;
                staffResumeMs_ = std::max<int64_t>(0, ms);
                staffLoaded_ = 0;
                steps_.clear();
            } else {
                orphan_ = true;
            }
        }
        hoverAsk_ = hoverQ_ = -1;
        pointerSeen_ = false;
        for (int q = 0; q < 3; q++) spotX_[q] = spotY_[q] = -1;
    }

    void reset() override {
        open_ = false, ask_ = 0;
        for (int q = 0; q < 3; q++) spotX_[q] = spotY_[q] = -1;
        staff_ = 0;
        orphan_ = false;
        bubblesFor_ = -1;
        pictures_.clear();
    }

    std::vector<std::string> chapter(PartHost &host, int number) override {
        if (!loaded_) {
            loaded_ = true;
            if (auto bytes = host.read("cfg\\chapter_file.ini")) chapters_ = parseChapters(std::string(bytes->begin(), bytes->end()));
            else host.log("chapter list missing: cfg\\chapter_file.ini");
        }
        auto it = chapters_.find(number);
        return it == chapters_.end() ? std::vector<std::string>{} : it->second;
    }

private:
    // Places measured on the PC's hub (1280x720): the 自白内容確認 button and the three heroine tabs.
    static constexpr int kTreeButton[4] = {12, 581, 190, 95};
    static constexpr int kTab[3][4] = {{752, 50, 118, 58}, {870, 50, 118, 58}, {988, 50, 118, 58}};

    // The bubbles drift and rock. Tracked on the PC under Wine through 76 s of one hub, 300 frames (set 2):
    // - Each moves in a straight line at a steady speed and turns back at the edges of the area: its frame's top at 160,
    //   bottom at 600, right side at 1280, left side at 355.
    // - Each rocks 10 degrees either way as a sine, over 50.0, 19.3 and 26.1 s.
    struct Bubble {
        float x = 0, y = 0;    // its middle
        float vx = 0, vy = 0;  // pixels a second
        int w = 337, h = 202;  // its frame: 337 by 202, or 512 by 165 for a long question
    };
    static constexpr float kBubbleSpeed = 12;
    static constexpr float kAreaLeft = 355, kAreaTop = 160, kAreaRight = 1280, kAreaBottom = 600;
    static constexpr float kTouchY = 51, kTouchX = 60;
    static constexpr float kPi = 3.14159265f, kRock = 10;
    static constexpr float kRockSeconds[3] = {50.0f, 19.3f, 26.1f};
    static constexpr int kMoveMs = 16;

    static bool inside(int x, int y, const int (&r)[4]) { return x >= r[0] && y >= r[1] && x < r[0] + r[2] && y < r[1] + r[3]; }

    bool askable(PartHost &host, int heroine, int q) {
        int stage = host.variable(1, kStage + heroine);
        return stage >= 1 && stage <= 3 && !locked(host, heroine) && !(asked(host, heroine) & (1 << q));
    }

    bool touch(int i, int j) const {
        return std::fabs(bubbles_[i].y - bubbles_[j].y) < float(bubbles_[i].h + bubbles_[j].h) / 2 - kTouchY &&
               std::fabs(bubbles_[i].x - bubbles_[j].x) < float(bubbles_[i].w + bubbles_[j].w) / 2 - kTouchX;
    }

    // The bubbles where the clock has them now, laid out afresh for another heroine, stage or truth.
    void moveBubbles(PartHost &host) {
        loadQuestions(host);
        int shown = shownHeroine(host), stage = std::min(3, std::max(1, host.variable(1, kStage + shown)));
        int64_t now = host.nowMs();
        if (int key = truth(host) * 100 + shown * 10 + stage; key != bubblesFor_) {
            bubblesFor_ = key;
            bubblesFrom_ = bubblesAt_ = now;
            pictures_.clear();
            for (int attempt = 0; attempt < 40; attempt++) {
                layoutSeed_ = mixed(chance_++ * 2654435761u + uint32_t(key) * 97u);
                for (int q = 0; q < 3; q++) {
                    Bubble &b = bubbles_[q];
                    b = Bubble{};
                    if (ImageRef text = host.image(panel(host, shown, stage, q)); text && text->w > b.w) b.w = text->w, b.h = text->h / 5;
                    uint32_t seed = mixed(layoutSeed_ + uint32_t(q) * 7919u);
                    auto unit = [&](uint32_t salt) { return float(mixed(seed + salt) % 10000) / 10000; };
                    b.x = kAreaLeft + float(b.w) / 2 + unit(1) * (kAreaRight - kAreaLeft - float(b.w));
                    b.y = kAreaTop + float(b.h) / 2 + unit(2) * (kAreaBottom - kAreaTop - float(b.h));
                    float heading = unit(3) * 2 * kPi;
                    b.vx = kBubbleSpeed * std::cos(heading), b.vy = kBubbleSpeed * std::sin(heading);
                    rockPhase_[q] = unit(4) * 2 * kPi;
                }
                bool touching = false;
                for (int pair = 0; pair < 3; pair++) touching = touching || touch(kPairs[pair][0], kPairs[pair][1]);
                if (!touching) break;
            }
            for (int pair = 0; pair < 3; pair++) wasNear_[pair] = touch(kPairs[pair][0], kPairs[pair][1]);
        }
        // A long gap, such as the app waking, isn't caught up.
        if (now - bubblesAt_ > 2000 || now < bubblesAt_) bubblesAt_ = now;
        for (; bubblesAt_ + kMoveMs <= now; bubblesAt_ += kMoveMs) {
            float dt = float(kMoveMs) / 1000;
            for (Bubble &b : bubbles_) b.x += b.vx * dt, b.y += b.vy * dt;
            for (int pair = 0; pair < 3; pair++) {
                Bubble &a = bubbles_[kPairs[pair][0]], &b = bubbles_[kPairs[pair][1]];
                bool touching = touch(kPairs[pair][0], kPairs[pair][1]);
                if (touching && !wasNear_[pair]) {
                    float deepY = float(a.h + b.h) / 2 - kTouchY - std::fabs(a.y - b.y), deepX = float(a.w + b.w) / 2 - kTouchX - std::fabs(a.x - b.x);
                    if (deepX < deepY) {
                        float away = a.x < b.x ? -1.0f : 1.0f;
                        a.vx = away * std::fabs(a.vx), b.vx = -away * std::fabs(b.vx);
                    } else {
                        float away = a.y < b.y ? -1.0f : 1.0f;
                        a.vy = away * std::fabs(a.vy), b.vy = -away * std::fabs(b.vy);
                    }
                }
                wasNear_[pair] = touching;
            }
            for (Bubble &b : bubbles_) {
                if ((b.y - float(b.h) / 2 < kAreaTop && b.vy < 0) || (b.y + float(b.h) / 2 > kAreaBottom && b.vy > 0)) b.vy = -b.vy;
                if ((b.x - float(b.w) / 2 < kAreaLeft && b.vx < 0) || (b.x + float(b.w) / 2 > kAreaRight && b.vx > 0)) b.vx = -b.vx;
            }
        }
    }

    // The question whose bubble is at x, y and can still be asked, the one drawn last first, or -1. A bubble takes
    // presses on its frame's inside, the frame less the margin its art leaves.
    int bubbleAt(PartHost &host, int x, int y) {
        int shown = shownHeroine(host);
        if (host.variable(1, kStage + shown) > 3) return -1;
        moveBubbles(host);
        int nearest = -1;
        float best = 0;
        for (int q = 0; q < 3; q++) {
            const Bubble &b = bubbles_[q];
            if (!bubblePicture(host, shown, std::min(3, std::max(1, host.variable(1, kStage + shown))), q, 0)) continue;
            float dx = std::fabs(float(x) - b.x) / (float(b.w) / 2 - 20), dy = std::fabs(float(y) - b.y) / (float(b.h) / 2 - 26);
            float far = std::max(dx, dy);
            if (far < 1 && (nearest < 0 || far < best)) nearest = q, best = far;
        }
        return nearest >= 0 && askable(host, shown, nearest) ? nearest : -1;
    }

    bool pressPoint(PartHost &host, int shown, int q, int &x, int &y) {
        if (!askable(host, shown, q)) return false;
        const Bubble &b = bubbles_[q];
        static constexpr int kOffsets[9][2] = {{0, 0}, {-1, 0}, {1, 0}, {0, -1}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        for (const auto &offset : kOffsets) {
            x = int(b.x) + offset[0] * b.w / 4, y = int(b.y) + offset[1] * b.h / 5;
            if (bubbleAt(host, x, y) == q) return true;
        }
        return false;
    }

    void refreshSpots(PartHost &host) {
        for (int q = 0; q < 3; q++)
            if (!pressPoint(host, shownHeroine(host), q, spotX_[q], spotY_[q])) spotX_[q] = spotY_[q] = -1;
    }

    void hover(PartHost &host) {
        int hit = pointerSeen_ ? bubbleAt(host, pointerX_, pointerY_) : -1;
        if (hit != hoverQ_) hoverQ_ = hit, host.redraw();
    }

    // A bubble as one picture, so it can be drawn tilted: its frame (l010_000 to _002 for stages 1 to 3, blue, green and
    // pink, three shapes across; l010_003 to _005 the wide ones a 512-wide panel takes) with the question's panel over
    // it, both in the same one of their five states.
    ImageRef bubblePicture(PartHost &host, int heroine, int stage, int q, int state) {
        int at = q * 5 + state;
        if (auto it = pictures_.find(at); it != pictures_.end()) return it->second;
        const Bubble &b = bubbles_[q];
        int colour = std::min(3, std::max(1, stage)) - 1 + (b.w > 337 ? 3 : 0);
        ImageRef frame = host.image("obj\\l010_00" + std::to_string(colour) + ".bmp"), text = host.image(panel(host, heroine, stage, q));
        if (!frame || (q + 1) * b.w > frame->w || (state + 1) * b.h > frame->h) return pictures_[at] = nullptr;
        auto picture = std::make_shared<Image>();
        picture->w = b.w, picture->h = b.h, picture->opaque = false;
        picture->rgba.resize(size_t(b.w) * size_t(b.h) * 4);
        for (int y = 0; y < b.h; y++)
            std::copy_n(&frame->rgba[(size_t(state * b.h + y) * size_t(frame->w) + size_t(q * b.w)) * 4], size_t(b.w) * 4, &picture->rgba[size_t(y) * size_t(b.w) * 4]);
        if (text && text->w <= b.w && text->h / 5 <= b.h) {
            int tw = text->w, th = text->h / 5, left = (b.w - tw) / 2, top = (b.h - th) / 2;
            for (int y = 0; y < th; y++)
                for (int x = 0; x < tw; x++) {
                    const uint8_t *src = &text->rgba[(size_t(state * th + y) * size_t(tw) + size_t(x)) * 4];
                    uint8_t *dst = &picture->rgba[(size_t(top + y) * size_t(b.w) + size_t(left + x)) * 4];
                    // Straight alpha, the panel over the frame.
                    int sa = src[3], da = dst[3] * (255 - sa) / 255, a = sa + da;
                    if (!a) continue;
                    for (int c = 0; c < 3; c++) dst[c] = uint8_t((src[c] * sa + dst[c] * da) / a);
                    dst[3] = uint8_t(a);
                }
        }
        return pictures_[at] = picture;
    }

    static int truth(PartHost &host) { return std::max(1, std::min(5, host.variable(1, kTruth))); }
    static int shownHeroine(PartHost &host) { return std::max(0, std::min(2, host.variable(1, kShown))); }
    static int askedVar(PartHost &host, int heroine) {
        int stage = std::max(1, std::min(3, host.variable(1, kStage + heroine)));
        return kAsked + 9 * (truth(host) - 1) + 3 * heroine + (stage - 1);
    }
    static int asked(PartHost &host, int heroine) { return host.variable(1, askedVar(host, heroine)); }
    static bool solvedAll(PartHost &host) {
        for (int h = 0; h < 3; h++)
            if (host.variable(1, kStage + h) <= 3) return false;
        return true;
    }

    void startTruth(PartHost &host, int t) {
        host.setVariable(1, kTruth, t);
        for (int h = 0; h < 3; h++) host.setVariable(1, kStage + h, 1);
        for (int i = 0; i < 9; i++) host.setVariable(1, kAsked + 9 * (t - 1) + i, 0);
        host.setVariable(1, kSolved, 0);
        host.setVariable(1, kPCLastAsked, 0);
        auto line = truthLine(host, t);
        host.setVariable(1, kHour, line.size() > 3 ? std::atoi(line[3].c_str()) : 8);
        host.setVariable(1, kDay, t);
        host.setVariable(1, kHoursLeft, hours(host, t));
        host.setVariable(1, kShown, std::max(0, std::min(2, firstShown(host, t))));
        toPC(host);
    }

    // `truth T HOURS FIRST [START]` under `screen hub`: the hours truth T gives, whom the hub shows first (A, M or R) and
    // the hour it starts at (8 unless given), and
    // `lock T HEROINE STAGE NEEDS...`: that stage opens once each heroine it needs is done, as seen under Wine.
    std::vector<std::string> truthLine(PartHost &host, int t) {
        for (auto &line : host.setting("hub", "truth"))
            if (!line.empty() && std::atoi(line[0].c_str()) == t) return line;
        return {};
    }
    int hours(PartHost &host, int t) {
        auto line = truthLine(host, t);
        return line.size() > 1 ? std::atoi(line[1].c_str()) : 18;
    }
    static int heroineOf(const std::string &word) { return word == "M" ? 1 : word == "R" ? 2 : word == "A" ? 0 : -1; }
    int firstShown(PartHost &host, int t) {
        auto line = truthLine(host, t);
        return line.size() > 2 ? heroineOf(line[2]) : 0;
    }
    bool locked(PartHost &host, int heroine) {
        int t = truth(host), stage = host.variable(1, kStage + heroine);
        for (auto &line : host.setting("hub", "lock")) {
            if (line.size() < 4 || std::atoi(line[0].c_str()) != t || heroineOf(line[1]) != heroine || std::atoi(line[2].c_str()) != stage)
                continue;
            for (size_t i = 3; i < line.size(); i++)
                if (int need = heroineOf(line[i]); need >= 0 && host.variable(1, kStage + need) <= 3) return true;
        }
        return false;
    }

    // Whom the hub shows after a right answer: the same heroine while her next stage is open, else the first of Ayano,
    // Misuzu and Rina with an open stage. All five truths' orders under Wine fit it (truth 2 goes from Misuzu back to
    // Ayano, not on to Rina).
    int openFrom(PartHost &host, int heroine) {
        auto open = [&](int h) { return host.variable(1, kStage + h) <= 3 && !locked(host, h); };
        if (open(heroine)) return heroine;
        for (int h = 0; h < 3; h++)
            if (open(h)) return h;
        return heroine;
    }

    bool ask(PartHost &host, int heroine, int question) {
        int t = truth(host), stage = host.variable(1, kStage + heroine);
        if (stage < 1 || stage > 3 || question < 0 || question > 2) return true;
        host.setVariable(1, kPCLastAsked, kPCAsked + 500 * heroine + 100 * (t - 1) + 10 * (stage - 1) + question);
        return finishWith(host, kPlay, 1000 * (heroine + 1) + 100 * (t - 1) + 10 * (stage - 1) + question);
    }

    // A question asked: an hour gone, its flag set, and a right answer's next stage, as the PC counts them.
    void account(PartHost &host, int heroine, int question) {
        int t = truth(host), stage = host.variable(1, kStage + heroine);
        host.setVariable(1, askedVar(host, heroine), asked(host, heroine) | (1 << question));
        if (question == answer(host, heroine, stage)) {
            host.setVariable(1, kStage + heroine, stage + 1);
            host.setVariable(1, kSolved, host.variable(1, kSolved) + 1);
            host.setVariable(1, kShown, openFrom(host, heroine));
        }
        // The answer that finishes the truth costs no hour: the PC's saves at a pick hold the clock from before it
        // (16:00 with 7 hours left after truth 3's nine right answers from 08:00 with 15).
        if (solvedAll(host)) {
            host.setVariable(1, kPCTruthDone + t - 1, 1);
            host.setVariable(1, kPCTalked, 1);
        } else {
            host.setVariable(1, kHoursLeft, host.variable(1, kHoursLeft) - 1);
            host.setVariable(1, kHour, host.variable(1, kHour) + 1);
            host.setVariable(1, kPCLastAsked, 0);  // the PC's hub saves hold 0 here, its pick saves the last question
        }
        // Truth 5: a heroine's last right answer is followed at once by her devil scene, 910, 920 or 930 (Wine).
        if (t == 5 && host.variable(1, kStage + heroine) == 4 && stage == 3) pendingDevil_ = 910 + 10 * heroine;
        toPC(host);
    }

    void toPC(PartHost &host) {
        int t = truth(host);
        host.setVariable(1, kPCTruth, t);
        host.setVariable(1, kPCHour, host.variable(1, kHour));
        host.setVariable(1, kPCHoursLeft, host.variable(1, kHoursLeft));
        host.setVariable(1, kPCTruthsDone, t - 1);
        for (int h = 0; h < 3; h++) {
            int before = host.variable(1, kPCSolved + 5 * h + (t - 1)), now = std::min(3, host.variable(1, kStage + h) - 1);
            host.setVariable(1, kPCSolved + 5 * h + (t - 1), now);
            host.setVariable(1, kPCTotal + h, host.variable(1, kPCTotal + h) + now - before);
            for (int stage = 1; stage <= 3; stage++) {
                int bits = host.variable(1, kAsked + 9 * (t - 1) + 3 * h + (stage - 1));
                for (int q = 0; q < 3; q++)
                    if (bits & (1 << q)) host.setVariable(1, kPCAsked + 500 * h + 100 * (t - 1) + 10 * (stage - 1) + q, 1);
            }
        }
    }

    void fromPC(PartHost &host) {
        int t = std::max(1, std::min(5, host.variable(1, kPCTruth)));
        host.setVariable(1, kTruth, t);
        host.setVariable(1, kDay, t);
        host.setVariable(1, kHour, host.variable(1, kPCHour));
        host.setVariable(1, kHoursLeft, host.variable(1, kPCHoursLeft));
        int solved = 0;
        for (int h = 0; h < 3; h++) {
            int done = std::max(0, std::min(3, host.variable(1, kPCSolved + 5 * h + (t - 1))));
            host.setVariable(1, kStage + h, done + 1);
            solved += done;
            for (int tt = 1; tt <= 5; tt++)
                for (int stage = 1; stage <= 3; stage++) {
                    int bits = 0;
                    for (int q = 0; q < 3; q++)
                        if (host.variable(1, kPCAsked + 500 * h + 100 * (tt - 1) + 10 * (stage - 1) + q)) bits |= 1 << q;
                    host.setVariable(1, kAsked + 9 * (tt - 1) + 3 * h + (stage - 1), bits);
                }
        }
        host.setVariable(1, kSolved, solved);
        // Truth 1's 12:00 scene has played once its clock is past noon, or at noon when it's the chapter last played.
        int last = host.variable(1, kPCChapter), hour = host.variable(1, kHour);
        host.setVariable(1, kEventDone, t > 1 || hour >= 12);
        // Whom to show: the heroine of the question last asked in this truth, as the PC's hub opens on her (a PC save at
        // truth 3's hub after Ayano's question, loaded under Wine), else the truth's first; then on to an open stage.
        int shown = last >= 1000 && last < 4000 && (last % 1000) / 100 + 1 == t ? last / 1000 - 1 : std::max(0, std::min(2, firstShown(host, t)));
        host.setVariable(1, kShown, openFrom(host, shown));
    }

    bool finishWith(PartHost &host, int result, int chapter) {
        // The chapter to play goes where the PC keeps it, S 1000, with its mode for a scene, so the story table's
        // `chapter S 1000` plays it and a save made inside it is the PC's; the tree's own hand-back stays in S 15110.
        if (chapter) {
            host.setVariable(1, kPCChapter, chapter);
            host.setVariable(1, kPCMode, 4);
        }
        open_ = false;
        host.finishScreen(result);
        return true;
    }

    void loadQuestions(PartHost &host) {
        if (questionsLoaded_) return;
        questionsLoaded_ = true;
        for (int t = 1; t <= 5; t++)
            if (auto bytes = host.read("cfg\\question_0" + std::to_string(t) + ".ini"))
                for (auto &[k, v] : parseKeys(std::string(bytes->begin(), bytes->end()))) questions_[k] = v;
    }
    std::string key(PartHost &host, int heroine, int stage, const std::string &what) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%sLevel%02dRank%02d%s", kNames[heroine], truth(host), stage, what.c_str());
        return buf;
    }
    int answer(PartHost &host, int heroine, int stage) {
        auto it = questions_.find(key(host, heroine, stage, "AnsNum"));
        return it == questions_.end() ? -1 : std::atoi(it->second.c_str());
    }
    std::string panel(PartHost &host, int heroine, int stage, int q) {
        auto it = questions_.find(key(host, heroine, stage, "Panel" + std::to_string(q + 1)));
        if (it == questions_.end()) return {};
        std::string path;
        for (size_t i = 0; i < it->second.size(); i++)
            if (!(it->second[i] == '\\' && i + 1 < it->second.size() && it->second[i + 1] == '\\')) path += it->second[i];
        return path;
    }

    static void put(PartHost &host, Canvas &out, const std::string &path, int x, int y) {
        if (ImageRef image = host.image(path)) drawPart(out, *image, x, y, 0, 0, image->w, image->h, 1);
    }
    static void cell(PartHost &host, Canvas &out, const std::string &path, int x, int y, int cx, int cy, int w, int h) {
        if (path.empty()) return;
        if (ImageRef image = host.image(path); image && cx + w <= image->w && cy + h <= image->h)
            drawPart(out, *image, x, y, cx, cy, w, h, 1);
    }

    // The ask band's places: band and question at y 316 (choice: 205), はい and いいえ at 522 and 650, y 390, 109x39;
    // the ending's three ways at x 390, y 279 + 55 each, 501x39.
    int askOption(int x, int y) const {
        if (ask_ == 4) {
            for (int i = 0; i < 3; i++)
                if (x >= 390 && x < 891 && y >= 279 + 55 * i && y < 318 + 55 * i) return i;
            return -1;
        }
        for (int i = 0; i < 2; i++)
            if (x >= 522 + 128 * i && x < 631 + 128 * i && y >= 390 && y < 429) return i;
        return -1;
    }
    // The clock and the hours left in o000's digits (32 wide), the day's "日目 :" in o000_001, and the limit under the
    // band in l002's small red digits (19 wide), all placed on the PC's hub shots. A one-digit count of hours left is
    // taken as centred over the two places; not yet seen on the PC.
    static void digits(PartHost &host, Canvas &out, const char *sheet, int w, int h, int x, int y, int value, int places) {
        value = std::max(0, value);
        for (int i = places - 1; i >= 0; i--, value /= 10) cell(host, out, sheet, x + w * i, y, w * (value % 10), 0, w, h);
    }
    static void drawClock(PartHost &host, Canvas &out) {
        int day = host.variable(1, kDay), hour = host.variable(1, kHour), left = host.variable(1, kHoursLeft);
        digits(host, out, "obj\\o000_000.bmp", 32, 40, 14, 24, day, 1);
        put(host, out, "obj\\o000_001.bmp", 46, 35);
        digits(host, out, "obj\\o000_000.bmp", 32, 40, 104, 24, hour, 2);
        digits(host, out, "obj\\o000_000.bmp", 32, 40, 185, 24, 0, 2);
        if (left >= 10) digits(host, out, "obj\\o000_000.bmp", 32, 40, 297, 7, left, 2);
        else digits(host, out, "obj\\o000_000.bmp", 32, 40, 313, 7, std::max(0, left), 1);
        // "タイムリミット D日目 HH時": the limit is the truth's start plus its hours.
        put(host, out, "obj\\l002_000.bmp", 257, 53);
        int limit = hour + left;
        digits(host, out, "obj\\l002_001.bmp", 19, 21, 350, 48, day, 1);
        digits(host, out, "obj\\l002_001.bmp", 19, 21, 396, 48, limit, 2);
    }

    // The small 質問進行ツリー at the top right, for the heroine shown, as on the PC's hub (truth 1 at 20:00): a row per
    // stage reached, 28 apart from y 100, three cells 42 apart from x 1146 in that stage's colour (l003_002: blue, green,
    // pink). An asked question carries the red X (l003_006). A solved stage's unasked ones are dimmed, and its right
    // answer joins the row below by a stem (l003_003) and a bar over the three (l003_005). How dim isn't measured.
    void drawMiniTree(PartHost &host, Canvas &out, int heroine) {
        put(host, out, "obj\\l003_000.bmp", 1073, 0);
        put(host, out, "obj\\l003_001.bmp", 1116, 76);
        int t = truth(host), current = std::min(3, host.variable(1, kStage + heroine));
        for (int stage = 1; stage <= current; stage++) {
            int y = 100 + 28 * (stage - 1);
            bool solved = stage < host.variable(1, kStage + heroine);
            int bits = host.variable(1, kAsked + 9 * (t - 1) + 3 * heroine + (stage - 1));
            for (int q = 0; q < 3; q++) {
                int x = 1146 + 42 * q;
                bool asked = bits & (1 << q);
                // The question under the pointer shows red (cell 4) in the current row, as on the PC.
                int colour = !solved && q == hoverQ_ && heroine == shownHeroine(host) ? 4 : stage - 1;
                if (ImageRef cells = host.image("obj\\l003_002.bmp"))
                    drawPart(out, *cells, x, y, 38 * colour, 0, 38, 18, solved && !asked ? 0.35f : 1);
                if (asked) put(host, out, "obj\\l003_006.bmp", x + 9, y - 1);
            }
            if (solved && stage < 3) {
                int right = answer(host, heroine, stage);
                if (right >= 0) put(host, out, "obj\\l003_003.bmp", 1146 + 42 * right + 18, y + 15);
                put(host, out, "obj\\l003_005.bmp", 1164, y + 22);
            }
        }
    }

    void drawAsk(PartHost &host, Canvas &out) const {
        int bandY = ask_ == 4 ? 205 : 316;
        put(host, out, "obj\\a002_000.bmp", 0, bandY);
        cell(host, out, "obj\\t000_000.bmp", 438, bandY, 404 * (ask_ - 1), 0, 404, 38);
        if (ask_ == 4)
            for (int i = 0; i < 3; i++) cell(host, out, "obj\\t001_000.bmp", 390, 279 + 55 * i, 501 * i, (hoverAsk_ == i ? 1 : 0) * 39, 501, 39);
        else
            for (int i = 0; i < 2; i++) cell(host, out, "obj\\a003_001.bmp", 522 + 128 * i, 390, 109 * i, (hoverAsk_ == i ? 1 : 0) * 39, 109, 39);
    }

    int ask_ = 0, hoverAsk_ = -1, hoverQ_ = -1, pendingDevil_ = 0;
    bool open_ = false;
    struct Post {
        std::string head, name, date;
        std::vector<std::string> lines;
        std::string image;
        int height = 0;
    };
    struct StaffStep {
        int first = 0, count = 0, slid = 0;
        bool appended = false;
    };
    int staff_ = 0, staffLoaded_ = 0;
    bool orphan_ = false;
    int64_t staffFrom_ = 0, staffResumeMs_ = -1, staffSeenMs_ = 0;
    std::vector<Post> posts_;
    std::vector<StaffStep> steps_;
    static constexpr int kStaffStepMs = 4083, kStaffSlideMs = 250, kStaffFadeMs = 4000, kStaffBlackMs = 1100;
    static constexpr int kStaffTailMs[3] = {3900, 3850, 3100};
    static constexpr int kPanelMs = 10000, kPanelFadeMs = 1667, kPanelHoldMs = 6000, kPanels = 11;
    static constexpr int kPageX = 42, kPageY = 98, kScanY = 34, kPanelX = 880, kPanelY = 96, kPageBottom = 670;
    static constexpr int kPostTop = 98, kPostLine = 27, kImageLines = 7, kHeadX = 106, kBodyX = 156;
    static constexpr int kStaffFace = 1, kStaffSize = 24, kHeadDrop = 2, kBodyDrop = 1;

    int64_t staffMs(PartHost &host) {
        if (staffResumeMs_ >= 0) {
            staffFrom_ = host.nowMs() - staffResumeMs_;
            staffResumeMs_ = -1;
        }
        loadStaff(host);
        staffSeenMs_ = std::max<int64_t>(0, host.nowMs() - staffFrom_);
        return staffSeenMs_;
    }
    int staffTail() const { return kStaffTailMs[std::max(1, std::min(3, staff_)) - 1]; }
    int64_t staffEndMs() const {
        return int64_t(std::max<size_t>(1, steps_.size()) - 1) * kStaffStepMs + staffTail() + kStaffFadeMs + kStaffBlackMs;
    }

    void loadStaff(PartHost &host) {
        if (staffLoaded_ == staff_) return;
        staffLoaded_ = staff_;
        posts_.clear();
        steps_.clear();
        auto bytes = host.read("cfg\\staff" + std::to_string(staff_) + ".ini");
        if (!bytes) {
            host.log("staff thread missing: cfg\\staff" + std::to_string(staff_) + ".ini");
            return;
        }
        std::istringstream in(std::string(bytes->begin(), bytes->end()));
        std::vector<std::string> lines;
        for (std::string line; std::getline(in, line);) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
            if (!line.empty()) lines.push_back(line);
        }
        for (size_t i = 0; i < lines.size(); i++) {
            if (lines[i] != "#thread" || i + 3 >= lines.size()) continue;
            Post post;
            post.head = lines[i + 1];
            post.name = lines[i + 2];
            post.date = lines[i + 3];
            for (i += 4; i < lines.size() && lines[i] != "##"; i++) {
                if (lines[i].rfind("msg,", 0) == 0) post.lines.push_back(lines[i].substr(4));
                if (lines[i].rfind("img,", 0) == 0) {
                    const std::string raw = lines[i].substr(4);
                    post.image.clear();
                    for (size_t c = 0; c < raw.size(); c++)
                        if (!(raw[c] == '\\' && c + 1 < raw.size() && raw[c + 1] == '\\')) post.image += raw[c];
                }
            }
            post.height = kPostLine * (1 + int(post.lines.size()) + (post.image.empty() ? 0 : kImageLines));
            posts_.push_back(post);
        }
        int first = 0, count = 0, total = int(posts_.size());
        auto bottom = [&] {
            int y = kPostTop;
            for (int i = first; i <= count && i < total; i++) y += posts_[size_t(i)].height;
            return y;
        };
        while (count < total && steps_.size() < size_t(4 * total + 4)) {
            StaffStep step;
            if (bottom() > kPageBottom && first < count) step.slid = posts_[size_t(first++)].height;
            if (bottom() <= kPageBottom || first == count) count++, step.appended = true;
            step.first = first, step.count = count;
            steps_.push_back(step);
        }
    }

    static int staffText(Canvas &out, const std::string &text, int x, int y, int r, int g, int b) {
        int ascent = fontAscent(kStaffFace, kStaffSize);
        for (size_t i = 0; i < text.size();) {
            int length = 1;
            char32_t code = sjis::decodeAt(text, i, &length);
            i += size_t(std::max(1, length));
            GlyphImage glyphImage;
            if (!glyph(kStaffFace, uint32_t(code), kStaffSize, false, glyphImage)) continue;
            if (glyphImage.w > 0 && glyphImage.h > 0)
                drawCoverage(out, glyphImage.coverage.data(), glyphImage.w, glyphImage.h, x + glyphImage.left, y + ascent - glyphImage.top, r, g, b, 256);
            x += glyphImage.advance;
        }
        return x;
    }

    void drawPost(PartHost &host, Canvas &out, const Post &post, int y) {
        static const std::string kColon = " \x81\x46 ";
        int x = staffText(out, post.head + kColon, kHeadX, y + kHeadDrop, 0, 0, 0);
        x = staffText(out, post.name, x, y + kHeadDrop, 0, 0, 255);
        staffText(out, kColon + post.date, x, y + kHeadDrop, 0, 0, 0);
        y += kPostLine;
        for (const std::string &line : post.lines) {
            staffText(out, line, kBodyX, y + kBodyDrop, 0, 0, 0);
            y += kPostLine;
        }
        if (!post.image.empty()) put(host, out, post.image, kBodyX, y);
    }

    void drawStaff(PartHost &host, Canvas &out) {
        int64_t ms = staffMs(host);
        tintFrame(out, 0, 0, 0, 256);
        if (steps_.empty()) return;
        put(host, out, "obj\\s000_002.bmp", kPageX, kPageY);
        size_t at = std::min(size_t(ms / kStaffStepMs), steps_.size() - 1);
        const StaffStep &step = steps_[at];
        int64_t into = ms - int64_t(at) * kStaffStepMs;
        bool sliding = step.slid > 0 && into < kStaffSlideMs;
        int first = sliding ? step.first - 1 : step.first, last = sliding && step.appended ? step.count - 1 : step.count;
        int y = kPostTop + (sliding ? -int(int64_t(step.slid) * into / kStaffSlideMs) : 0);
        for (int i = first; i < last; i++) {
            drawPost(host, out, posts_[size_t(i)], y);
            y += posts_[size_t(i)].height;
        }
        int panel = int(std::min<int64_t>(ms / kPanelMs, kPanels - 1));
        int64_t rel = ms - int64_t(panel) * kPanelMs;
        float shown = rel < kPanelFadeMs ? float(rel) / kPanelFadeMs
                      : rel < kPanelFadeMs + kPanelHoldMs ? 1
                      : rel < 2 * kPanelFadeMs + kPanelHoldMs ? 1 - float(rel - kPanelFadeMs - kPanelHoldMs) / kPanelFadeMs : 0;
        char name[32];
        std::snprintf(name, sizeof name, "obj\\s001_%03d.bmp", panel);
        if (ImageRef image = host.image(name); image && shown > 0) drawPart(out, *image, kPanelX, kPanelY, 0, 0, image->w, image->h, shown * shown);
        put(host, out, "obj\\s000_000.bmp", 0, 0);
        put(host, out, "obj\\s000_001.bmp", kPageX, kScanY);
        int64_t fadeFrom = int64_t(steps_.size() - 1) * kStaffStepMs + staffTail();
        if (ms > fadeFrom) tintFrame(out, 0, 0, 0, int(std::min<int64_t>(256, (ms - fadeFrom) * 256 / kStaffFadeMs)));
    }
    // The hub's bubbles: where they are, the clock they've been moved up to and the one their rocking counts from, what
    // they were laid out for (truth, heroine and stage), each pair's side of its touching line, and their pictures by
    // question and state.
    static constexpr int kPairs[3][2] = {{0, 1}, {0, 2}, {1, 2}};
    std::array<Bubble, 3> bubbles_;
    int64_t bubblesAt_ = 0, bubblesFrom_ = 0;
    int bubblesFor_ = -1;
    bool wasNear_[3] = {false, false, false};
    uint32_t chance_ = std::random_device{}(), layoutSeed_ = 0;
    float rockPhase_[3] = {0, 0, 0};
    int spotX_[3] = {-1, -1, -1}, spotY_[3] = {-1, -1, -1};
    std::map<int, ImageRef> pictures_;
    int pointerX_ = 0, pointerY_ = 0;
    bool pointerSeen_ = false;
    bool loaded_ = false;
    bool questionsLoaded_ = false;
    struct Thought {
        int truth = 0, who = 0, stage = 0;
        std::string picture;
    };
    bool thoughtsLoaded_ = false;
    std::vector<Thought> thoughts_;
    static constexpr int kThoughtEveryMs = 2400, kThoughtInMs = 500, kThoughtHoldMs = 2300, kThoughtOutMs = 1300;
    static constexpr float kThoughtGrow = 0.34f, kThoughtShown = 0.8f;
    static constexpr int kThoughtLeft = 400, kThoughtWide = 500, kThoughtTop = 165, kThoughtTall = 425;

    void loadThoughts(PartHost &host) {
        if (thoughtsLoaded_) return;
        thoughtsLoaded_ = true;
        auto bytes = host.read("cfg\\mind_syu.ini");
        if (!bytes) return;
        std::istringstream in(std::string(bytes->begin(), bytes->end()));
        int truth = 0;
        for (std::string line; std::getline(in, line);) {
            if (size_t cut = line.find("//"); cut != std::string::npos) line.erase(cut);
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
            if (line.rfind("#p", 0) == 0) truth = std::atoi(line.c_str() + 2) + 1;
            size_t first = line.find(','), second = first == std::string::npos ? first : line.find(',', first + 1);
            if (line.empty() || line[0] == '#' || second == std::string::npos) continue;
            Thought thought;
            thought.truth = truth;
            thought.who = std::atoi(line.c_str());
            thought.stage = std::atoi(line.c_str() + first + 1) + 1;
            for (size_t c = second + 1; c < line.size(); c++)
                if (!(line[c] == '\\' && c + 1 < line.size() && line[c + 1] == '\\')) thought.picture += line[c];
            thoughts_.push_back(thought);
        }
    }

    static uint32_t mixed(uint32_t value) {
        value ^= value >> 16;
        value *= 0x7feb352du;
        value ^= value >> 15;
        value *= 0x846ca68bu;
        value ^= value >> 16;
        return value;
    }

    void drawThoughts(PartHost &host, Canvas &out, int heroine, int stage) {
        loadThoughts(host);
        std::vector<const Thought *> pool;
        for (const Thought &thought : thoughts_)
            if (thought.truth == truth(host) && (thought.who == 0 || (thought.who == heroine + 1 && thought.stage == stage))) pool.push_back(&thought);
        if (pool.empty()) return;
        int64_t ms = std::max<int64_t>(0, host.nowMs() - bubblesFrom_);
        const int life = kThoughtInMs + kThoughtHoldMs + kThoughtOutMs;
        for (int64_t index = std::max<int64_t>(0, (ms - life) / kThoughtEveryMs); index * kThoughtEveryMs <= ms; index++) {
            int64_t age = ms - index * kThoughtEveryMs;
            if (age >= life) continue;
            uint32_t seed = mixed(layoutSeed_ + uint32_t(index) * 40503u);
            const Thought &thought = *pool[seed % pool.size()];
            ImageRef image = host.image(thought.picture);
            if (!image) continue;
            float shown = age < kThoughtInMs ? float(age) / kThoughtInMs : age < kThoughtInMs + kThoughtHoldMs ? 1 : 1 - float(age - kThoughtInMs - kThoughtHoldMs) / kThoughtOutMs;
            float scale = 1 + kThoughtGrow * float(age) / float(life);
            float x = float(kThoughtLeft + int(mixed(seed + 1) % kThoughtWide)), y = float(kThoughtTop + int(mixed(seed + 2) % kThoughtTall));
            drawImage(out, *image, x, y, scale, scale, 0, shown * kThoughtShown);
        }
    }
    std::map<int, std::vector<std::string>> chapters_;
    std::map<std::string, std::string> questions_;
};

}  // namespace

std::unique_ptr<GamePart> makeYabaiPlayPart() { return std::make_unique<YabaiPlayPart>(); }

}  // namespace neko
