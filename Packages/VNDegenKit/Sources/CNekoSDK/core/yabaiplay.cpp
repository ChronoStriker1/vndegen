// The `yabaiplay` game part: Test Game 96's story pieces that its PC keeps in the exe (set 2).
//   chapter N   the scene scripts cfg\chapter_file.ini lists under `#chapter N`, in order
// Its chapters number the interrogation as 1000 * heroine (1 Ayano, 2 Misuzu, 3 Rina) + 100 * (truth - 1)
// + 10 * (stage - 1) + question, with the prologue, the truths' openings, the votes and endings below 1000.
#include <algorithm>
#include <array>
#include <cstdio>
#include <map>
#include <sstream>

#include "parts.h"

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
            line = line.substr(0, line.find(','));
            std::string path;
            for (size_t i = 0; i < line.size(); i++)
                if (!(line[i] == '\\' && i + 1 < line.size() && line[i + 1] == '\\')) path += line[i];
            open->push_back(path);
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

// The hub's variables, VNDegen's own, in a range 96's PC saves leave empty (theirs end at 14021, then 19999). The lead's
// tree part reads the same numbers.
enum : int {
    kTruth = 15100,      // 1..5
    kStage = 15101,      // + heroine: 1..3, 4 when she's done
    kHour = 15104,       // the clock's hour, 8 at a truth's start, counting past 24 as the PC's limit does
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
};

// What the hub's screen gives the story table.
enum HubResult { kPlay = 1, kAllSolved = 2, kTimeUp = 3, kTimeEvent = 4, kDevil = 5, kOpenTree = 9 };

class YabaiPlayPart : public GamePart {
public:
    bool openScreen(PartHost &host, const std::string &name, const std::vector<int> &args) override {
        // `open hubstart T`: truth T begins, with the hours, start and order its `truth` line under `screen hub` gives.
        if (name == "hubstart") {
            startTruth(host, args.empty() ? 1 : args[0]);
            host.finishScreen(0);
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
        if (name != "hub") return false;
        loadQuestions(host);
        // A PC save, which holds only the PC's variables: take the hub's state from them.
        if (!host.variable(1, kTruth) && host.variable(1, kPCTruth)) {
            fromPC(host);
            // Saved inside a question's scene: the PC counts the question once its scene is over, so count it now.
            int c = host.variable(1, kPCChapter);
            if (host.variable(1, kPCMode) == 4 && c >= 1000 && c < 4000 && (c % 1000) / 100 + 1 == truth(host)) {
                int heroine = c / 1000 - 1, stage = (c % 100) / 10 + 1, question = c % 10;
                if (stage == host.variable(1, kStage + heroine) && !(asked(host, heroine) & (1 << question))) account(host, heroine, question);
            }
        }
        host.setVariable(1, kPCMode, 2);
        // Back from the tree: a replay plays at no cost; an ask is a question like a bubble's.
        int action = host.variable(1, kTreeAction), picked = host.variable(1, kTreeChapter);
        host.setVariable(1, kTreeAction, 0);
        host.setVariable(1, kTreeChapter, 0);
        if (action == 1 && picked > 0) return finishWith(host, kPlay, picked);
        if (action == 2 && picked > 0) {
            int heroine = picked / 1000 - 1, question = picked % 10;
            if (heroine >= 0 && heroine < 3) return ask(host, heroine, question);
        }
        if (pendingDevil_) {
            int devil = pendingDevil_;
            pendingDevil_ = 0;
            return finishWith(host, kDevil, devil);
        }
        if (solvedAll(host)) return finishWith(host, kAllSolved, 0);
        if (host.variable(1, kHoursLeft) <= 0) return finishWith(host, kTimeUp, 0);
        // Truth 1's time event, once its clock has reached 12:00 (seen under Wine after the question that took it there).
        if (truth(host) == 1 && host.variable(1, kHour) >= 12 && !host.variable(1, kEventDone)) {
            host.setVariable(1, kEventDone, 1);
            return finishWith(host, kTimeEvent, 0);
        }
        open_ = true;
        hoverQ_ = -1;
        host.redraw();
        return true;
    }

    bool press(PartHost &host, int x, int y, int button) override {
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
        int shown = host.variable(1, kShown);
        for (int q = 0; q < 3; q++)
            if (inside(x, y, bubbleRect(q)) && !locked(host, shown) && !(asked(host, shown) & (1 << q))) return ask(host, shown, q);
        return true;
    }

    void pointer(PartHost &host, int x, int y) override {
        if (ask_) {
            int hit = askOption(x, y);
            if (hit != hoverAsk_) hoverAsk_ = hit, host.redraw();
            return;
        }
        if (!open_) return;
        int shown = host.variable(1, kShown), hit = -1;
        for (int q = 0; q < 3 && hit < 0; q++)
            if (inside(x, y, bubbleRect(q)) && !locked(host, shown) && !(asked(host, shown) & (1 << q))) hit = q;
        if (hit != hoverQ_) hoverQ_ = hit, host.redraw();
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        if (layer != Layer::Screen) return;
        if (ask_) {
            drawAsk(host, out);
            return;
        }
        if (!open_) return;
        int shown = host.variable(1, kShown), stage = host.variable(1, kStage + shown);
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
        for (int q = 0; q < 3 && stage <= 3; q++) {
            auto r = bubbleRect(q);
            // Frames: blue for a question still to ask, dark once asked (l010, 3 shapes across, 5 states down).
            // Under the pointer, the frame's glowing row 1 and the panel's lit row 1 (hover state, as seen on the PC).
            int state = bits & (1 << q) ? 4 : q == hoverQ_ ? 1 : 0;
            // Each stage's own frames: l010_000 blue, _001 green, _002 pink.
            cell(host, out, "obj\\l010_00" + std::to_string(std::min(3, std::max(1, stage)) - 1) + ".bmp", r[0], r[1], q * 337,
                 state * 202, 337, 202);
            // Each panel is its question's text in five states down, centred in its frame.
            if (ImageRef text = host.image(panel(host, shown, stage, q))) {
                int w = text->w, h = text->h / 5;
                drawPart(out, *text, r[0] + (337 - w) / 2, r[1] + (202 - h) / 2, 0, state * h, w, h, 1);
            }
        }
        if (shut) put(host, out, "obj\\l015_001.bmp", 326, 260);
    }

    void reset() override { open_ = false, ask_ = 0; }

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
    // Places measured on the PC's hub (1280x720): the 自白内容確認 button and the three heroine tabs. The bubbles drift on
    // the PC; until that's measured they sit at the middle of where they were seen.
    static constexpr int kTreeButton[4] = {12, 581, 190, 95};
    static constexpr int kTab[3][4] = {{752, 50, 118, 58}, {870, 50, 118, 58}, {988, 50, 118, 58}};
    static constexpr int kBubble[3][2] = {{480, 160}, {760, 250}, {500, 370}};

    static bool inside(int x, int y, const int (&r)[4]) { return x >= r[0] && y >= r[1] && x < r[0] + r[2] && y < r[1] + r[3]; }
    static std::array<int, 4> bubbleRect(int q) { return {kBubble[q][0], kBubble[q][1], 337, 202}; }
    static bool inside(int x, int y, const std::array<int, 4> &r) { return x >= r[0] && y >= r[1] && x < r[0] + r[2] && y < r[1] + r[3]; }

    static int truth(PartHost &host) { return std::max(1, std::min(5, host.variable(1, kTruth))); }
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

    bool ask(PartHost &host, int heroine, int question) {
        int t = truth(host), stage = host.variable(1, kStage + heroine);
        if (stage < 1 || stage > 3 || question < 0 || question > 2) return true;
        int chapter = 1000 * (heroine + 1) + 100 * (t - 1) + 10 * (stage - 1) + question;
        account(host, heroine, question);
        host.setVariable(1, kPCMode, 4);
        return finishWith(host, kPlay, chapter);
    }

    // A question asked: an hour gone, its flag set, and a right answer's next stage, as the PC counts them.
    void account(PartHost &host, int heroine, int question) {
        int t = truth(host), stage = host.variable(1, kStage + heroine);
        host.setVariable(1, askedVar(host, heroine), asked(host, heroine) | (1 << question));
        host.setVariable(1, kHoursLeft, host.variable(1, kHoursLeft) - 1);
        host.setVariable(1, kHour, host.variable(1, kHour) + 1);
        if (question == answer(host, heroine, stage)) {
            host.setVariable(1, kStage + heroine, stage + 1);
            host.setVariable(1, kSolved, host.variable(1, kSolved) + 1);
            // She stays on screen while her next stage is open; else the next of Ayano, Misuzu, Rina who has one (Wine).
            int next = heroine;
            for (int i = 0; i < 3 && (host.variable(1, kStage + next) > 3 || locked(host, next)); i++) next = (next + 1) % 3;
            host.setVariable(1, kShown, next);
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
        // Truth 1's 12:00 scene has played once its clock is past noon; whom to show: the first with an open stage.
        host.setVariable(1, kEventDone, t > 1 || host.variable(1, kHour) > 12);
        int shown = std::max(0, std::min(2, firstShown(host, t)));
        for (int i = 0; i < 3 && (host.variable(1, kStage + shown) > 3 || locked(host, shown)); i++) shown = (shown + 1) % 3;
        host.setVariable(1, kShown, shown);
    }

    bool finishWith(PartHost &host, int result, int chapter) {
        // The chapter to play goes where the PC keeps it, S 1000, so the story table's `chapter S 1000` plays it and a PC
        // save made inside it is found there; the tree's own hand-back stays in S 15110.
        if (chapter) host.setVariable(1, kPCChapter, chapter);
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
                int colour = !solved && q == hoverQ_ && heroine == host.variable(1, kShown) ? 4 : stage - 1;
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
    bool loaded_ = false;
    bool questionsLoaded_ = false;
    std::map<int, std::vector<std::string>> chapters_;
    std::map<std::string, std::string> questions_;
};

}  // namespace

std::unique_ptr<GamePart> makeYabaiPlayPart() { return std::make_unique<YabaiPlayPart>(); }

}  // namespace neko
