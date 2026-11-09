// The `yabaivote` game part: Test Game 96's vote, the pick screen 嘘つきは誰だ!? and the vote that follows it. The story
// table opens it with `open vote T -> S n` for truth T; it finishes with 1 when the heroine picked is one the vote elects,
// 0 when not, and 9 when the pick screen's 自白内容確認 is pressed, for the table to open the tree and the vote again
// (set 2's hand-over). Its data is the game's own: cfg\vote_base.ini names, for each vote, its length in frames at 60 a
// second (AllTimeNN), its script (BehaviorFilenameNN, a vote_move_type file), its viewers' comments (VoteCommentNN, a
// CommentList file read by CommentStream) and who it elects (ElectedNumNN, and for vote 4 ElectedNum04_2 as well: set 2
// found a pick of either plays the right ending). Heroines are numbered as ElectedNum counts them, 0 Misuzu, 1 Ayano,
// 2 Rina, which is also the vote's columns from the left.
//
// Laid out against the PC under Wine (lead, .audit/evidence/nekosdk/lead/vote96), with the hub's bars as set 2 placed them. The pick screen was shot as a menu;
// the vote shows the heroines' standing pictures behind its coffins, so it was measured only by locating its pictures in
// shots that were never looked at, and by crops of its bars. What wasn't measured is marked so.
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iterator>
#include <map>
#include <sstream>

#include "comments.h"

namespace neko {
namespace {

// The pick screen's clock from the PC's own S 2000 (the truth, which is the day) and S 2001 (the hour), as set 2's hub
// keeps them, or the hub's S 15105 and S 15104; and S 105, the PC's mode, 8 at a pick and 4 in a scene.
constexpr int kPcDayVar = 2000, kPcHourVar = 2001, kHourVar = 15104, kDayVar = 15105;
constexpr int kModeVar = 105, kModePick = 8, kModeScene = 4;
// PartHost's sound kinds, and the vote's own channels: a voice for each heroine from 0, and her effect from 20.
constexpr int kEffectKind = 1, kVoiceKind = 2, kEffectChannel = 20;
constexpr int kResultTree = 9;

// The pick screen: each heroine's panel (390 by 340 cells, three frames of a shimmer across and its states down: 0 at
// rest, 1 under the pointer), her name from n004_003's three cells, and her statement for the truth (n005 to n007, a
// cell for each truth).
struct Panel {
    const char *sheet;
    int x, y;
    int nameCell, nameX, nameY;
    const char *statement;
    int statementX, statementY;
};
const Panel kPanels[3] = {
    {"obj\\n004_001.bmp", 56, 223, 1, 134, 238, "obj\\n006_000.bmp", 54, 436},
    {"obj\\n004_000.bmp", 441, 81, 0, 521, 88, "obj\\n005_000.bmp", 464, 282},
    {"obj\\n004_002.bmp", 806, 261, 2, 891, 268, "obj\\n007_000.bmp", 845, 468},
};
constexpr int kPanelW = 390, kPanelH = 340, kPanelFrameMs = 150;  // the shimmer's pace isn't measured
constexpr int kPickFadeMs = 500;                                   // nor is the fade in, during which presses are dropped
struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};
constexpr Rect kTreeButton{12, 581, 190, 95};
// The narrator's thoughts behind the panels (obj\n008_000 to _013): one comes up somewhere on the screen every 0.3 to
// 1.8 seconds, fades in over half a second, grows about its middle by a tenth a second, and fades out over the last
// second of its four. Tracked over 30 seconds of the PC's pick screen (lead, under Wine): widths growing 6 to 12 percent
// a second and lives of 3.5 to 4 seconds; the pace and the strength were then fitted until a run of VNDegen's showed as
// many lit rows as the PC's, as bright (116 bands of mean 55 against the PC's 125 of 54, both peaking at 195). Where
// each comes up looked random and is. Which five a truth's pick shows was read from the PC's picks of truths 1, 3, 4
// and 5; truth 2's wasn't seen and is taken as 1's.
const int kThoughts[5][5] = {{0, 1, 2, 3, 13}, {0, 1, 2, 3, 13}, {0, 1, 2, 3, 13}, {4, 5, 6, 7, 13}, {8, 9, 10, 11, 12}};
constexpr int kThoughtLifeMs = 4000, kThoughtInMs = 500, kThoughtOutMs = 1000;
constexpr int kThoughtGapMs = 300, kThoughtGapSpanMs = 1500;
constexpr float kThoughtAlpha = 0.95f, kThoughtGrowth = 0.1f;

// The vote: the script's clock starts 9.9 seconds after the pick, as the countdown does, after the doors open and
// 投票開始 shows from 4.4 to 5.9 seconds.
constexpr int64_t kVoteStartMs = 9900, kStartBannerFrom = 4400, kStartBannerTo = 5900;
// Each heroine's face, 258 pixels wide, at 119 + 400 a column, on one of three rows: H 50, M 200 and L 350. A move goes
// its number of pixels a frame.
constexpr int kFaceX = 119, kColumnStep = 400, kRowH = 50, kRowM = 200, kRowL = 350;
// Each heroine's coffin, o012_002's cell for her, hangs 69 below her face's row at 65 + 400 a column, on a rope
// (o012_001) from a wheel (o012_000) at the top of her column. The wheel sits still at 96 + 400 a column, 74, its upper
// half under the monitor's frame: its lower half fits there in every frame of a run of the PC's vote, and the rope's
// own colours were found 11 to its right (lead, under Wine, by numbers alone). How far up the rope's picture reaches
// isn't measured; it hangs from the top to the coffin. Ayano's coffin was never found whole on the PC, likely with
// something over it, and is drawn as the others are.
constexpr int kCoffinX = 65, kCoffinDY = 69, kWheelX = 96, kWheelY = 74, kRopeX = 107;
// The monitor's inside, which all of that is drawn within, on a dark red: the PC's mean colour there over the vote, in
// cells no face or bubble crosses, measured from the shots' numbers alone (lead). Which picture gives it isn't known.
constexpr Rect kInside{40, 96, 1200, 544};
// The viewers' raised hands along the monitor's bottom, the same in each column from its start: o013_001 to o013_005, each
// an animation of one hand rising in cells 144 tall. What the PC's strip measures (lead, under Wine, the silhouette's
// height in each column over 37 seconds, by numbers alone; set 2's shots agree): it stands on the bottom edge, never more
// than about 70 pixels high, in four clusters some 60 wide from 40, 132, 212 and 296 of the column, about 13 pixels high
// on average across the column, and each part of it comes round every 1.35 to 1.7 seconds. Which hands make which
// cluster couldn't be fitted, so this arrangement is a stand-in with those measures: the five hands standing 68 below
// the edge, so only their tops show, three times over, each set 18 to the right of the last and a third of a turn
// behind. A later run of shots about 90 ms apart over 12 seconds puts the strip's own repeat at 1.67 seconds (it
// comes closest to itself after 1.67, 3.3 and 5.0 seconds), with a weaker one at 1.35, so the three slower hands turn
// in 100 frames. Now and then one column's hands shrink to nothing over about two seconds and are back at once: seen in
// Misuzu's column while her coffin came down to the lowest row, so likely the coffin passing over them, which this
// draws too. The arrangement itself stays unconfirmed.
struct Hand {
    const char *sheet;
    int cells, x, periodMs;
};
const Hand kHands[] = {{"obj\\o013_001.bmp", 9, 40, 1350}, {"obj\\o013_002.bmp", 7, 132, 1667}, {"obj\\o013_003.bmp", 7, 150, 1667},
                       {"obj\\o013_004.bmp", 6, 212, 1667}, {"obj\\o013_005.bmp", 8, 296, 1350}};
constexpr int kHandsBottom = 640 + 68, kHandsCopies = 3, kHandsCopyDX = 18;
// The thin black dividers between the columns (o003_000), as set 2 placed them.
constexpr int kDividerX[2] = {437, 837}, kDividerY = 16;
constexpr uint8_t kInsideRed[3] = {55, 2, 2};
const char *const kFaceSheets[3] = {"obj\\o014_000.bmp", "obj\\o014_001.bmp", "obj\\o014_002.bmp"};
const int kFaceCells[3] = {10, 12, 10};
// The speech bubbles: a sheet for each heroine, plain (N) and spiky (S), of six cells by where the tail points, placed
// from the face by the side the script names. The cells run tail at bottom left, bottom, bottom right, top right, top,
// top left.
const char *const kBubbleSheets[3][2] = {{"obj\\o014_003.bmp", "obj\\o014_004.bmp"},
                                         {"obj\\o014_005.bmp", "obj\\o014_006.bmp"},
                                         {"obj\\o014_007.bmp", "obj\\o014_008.bmp"}};
struct BubbleSide {
    const char *name;
    int cell, dx, dy;
};
// LT is the one side not seen under Wine; it's taken as RT's mirror.
const BubbleSide kSides[] = {{"B", 4, -74, 189}, {"T", 1, -74, -171}, {"RT", 0, 166, -102},
                             {"RB", 5, 166, 139}, {"LB", 3, -356, 139}, {"LT", 2, -356, -102}};
constexpr int kBubbleW = 468, kBubbleH = 240;
constexpr int64_t kBubbleMinMs = 2000;  // a bubble stays its voice's length, or two seconds at least
// A bubble comes in over 8 frames. On the PC under Wine, in shots about 85 ms apart, Ayano's first FADE in vote 1 was
// 28% in, then 90%, then whole, a straight fade of about 130 ms. Misuzu's first POP was whole within two shots of
// starting, so it takes no longer, but its one shot between differs from the bubble more than the empty scene does:
// whether it grows from small or settles from large isn't measured, and growing from half size is a guess.
constexpr int kBubbleInMs = 133;

// The countdown: 残り時間 over minutes and seconds in o005_001's digits and the hundredths in o007_002's, red under ten
// seconds.
constexpr int kDigitsX[4] = {969, 989, 1028, 1048}, kSmallX[2] = {1085, 1103};

// One line of a vote_move_type file: `FRAME,WHO,KIND,ARGS...`.
struct Event {
    int frame = 0, who = -1;
    std::string kind;
    std::vector<std::string> args;
};

int whoOf(const std::string &s) { return s == "MIS" ? 0 : s == "AYA" ? 1 : s == "RIN" ? 2 : -1; }

std::string trimmed(std::string s) {
    size_t a = s.find_first_not_of(" \t\r"), b = s.find_last_not_of(" \t\r");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

// A path as the files write it, `obj\\o016_000.bmp`, with the doubled backslash as one.
std::string path(std::string s) {
    for (size_t d; (d = s.find("\\\\")) != std::string::npos;) s.erase(d, 1);
    return s;
}

class YabaiVotePart : public GamePart {
public:
    bool openScreen(PartHost &host, const std::string &name, const std::vector<int> &args) override {
        if (name != "vote") return false;
        truth_ = std::clamp(args.empty() ? 1 : args[0], 1, 5);
        readBase(host);
        phase_ = Phase::Pick;
        opened_ = host.nowMs();
        thoughts_.clear();
        nextThought_ = opened_;
        host.setVariable(1, kModeVar, kModePick);
        host.redraw();
        return true;
    }

    void pointer(PartHost &host, int x, int y) override {
        px_ = x;
        py_ = y;
        host.redraw();
    }

    bool press(PartHost &host, int x, int y, int button) override {
        reopen(host);
        if (phase_ == Phase::Closed) return false;
        if (phase_ == Phase::Pick) {
            if (host.nowMs() - opened_ < kPickFadeMs || button != 0) return true;
            if (kTreeButton.contains(x, y)) return finish(host, kResultTree);
            int panel = panelAt(host, x, y);
            if (panel >= 0) startVote(host, panel);
            return true;
        }
        if (skipRect().contains(x, y)) {
            endVote(host);
            return true;
        }
        if (hideRect().contains(x, y)) {
            commentsHidden_ = !commentsHidden_;
            host.redraw();
        }
        return true;
    }

    bool focusSpots(PartHost &host, std::vector<std::pair<int, int>> &spots) override {
        reopen(host);
        if (phase_ == Phase::Pick) {
            for (const Panel &p : kPanels) spots.push_back({p.x + kPanelW / 2, p.y + kPanelH / 2});
            spots.push_back({kTreeButton.x + kTreeButton.w / 2, kTreeButton.y + kTreeButton.h / 2});
        } else if (phase_ == Phase::Vote) {
            for (Rect r : {skipRect(), hideRect()}) spots.push_back({r.x + r.w / 2, r.y + r.h / 2});
        }
        return phase_ != Phase::Closed;
    }

    bool step(PartHost &host) override {
        reopen(host);
        if (phase_ == Phase::Pick) {
            // The panels shimmer, and the narrator's thoughts come and go.
            int64_t now = host.nowMs();
            auto gone = [&](const Thought &t) { return now - t.born >= kThoughtLifeMs; };
            thoughts_.erase(std::remove_if(thoughts_.begin(), thoughts_.end(), gone), thoughts_.end());
            if (now >= nextThought_) {
                thoughts_.push_back({kThoughts[truth_ - 1][std::rand() % 5], 250 + std::rand() % 780, 110 + std::rand() % 450, now});
                nextThought_ = now + kThoughtGapMs + std::rand() % kThoughtGapSpanMs;
            }
            return true;
        }
        if (phase_ != Phase::Vote) return false;
        int64_t ms = host.nowMs() - voteStart_;
        int frame = ms < kVoteStartMs ? -1 : int((ms - kVoteStartMs) * 60 / 1000);
        // Each frame's lines run before that frame moves, however many frames a step covers, so a move starts on its own
        // frame at any pace of steps.
        for (int f = lastFrame_ + 1; f <= frame; f++) {
            while (next_ < events_.size() && events_[next_].frame <= f) run(host, events_[next_++]);
            advance(host, f);
        }
        lastFrame_ = std::max(lastFrame_, frame);
        comments_.step(host);
        if (frame >= allFrames_ + kEndHoldFrames) return endVote(host), true;
        return true;
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        reopen(host);
        if (layer != Layer::Screen || phase_ == Phase::Closed) return;
        if (phase_ == Phase::Pick) drawPick(host, out);
        else drawVote(host, out);
    }

    // The message window's plate (`partplate`) shows over the pick screen, not while the vote runs (set 2).
    bool showsPlate() const override { return phase_ != Phase::Vote; }

    // A vote isn't kept with a save: one made during it comes back to the pick screen, which is where the PC's saves of a
    // vote are (set 2's pick saves).
    void save(std::vector<std::string> &lines) const override {
        if (phase_ != Phase::Closed) lines.push_back("pick " + std::to_string(truth_));
    }
    void load(const std::vector<std::string> &lines) override {
        for (const std::string &line : lines)
            if (line.rfind("pick ", 0) == 0) {
                truth_ = std::clamp(std::atoi(line.c_str() + 5), 1, 5);
                phase_ = Phase::Reopen;
            }
    }
    void reset() override {
        phase_ = Phase::Closed;
        comments_.reset();
    }

private:
    enum class Phase { Closed, Reopen, Pick, Vote };
    static constexpr int kEndHoldFrames = 120;  // 投票終了 shows two seconds before the result: not measured
    static constexpr float kFallSpeed = 14;     // rows a frame in a `fall_move`

    struct Heroine {
        float y = kRowL, target = kRowL, speed = 0;
        int face = 0;
        int shakeAmp = 0, shakeUntil = -1, shakeX = 0, shakeY = 0;
        bool shakeSideways = false;
        // The bubble showing: its sheet, side, text picture, when it came, and how.
        bool bubble = false, spiky = false, pop = false;
        int side = 0, number = 0;
        std::string text;
        int64_t bubbleAt = 0;
        int bubbleShakeAmp = 0, bubbleShakeUntil = -1, bubbleShakeX = 0, bubbleShakeY = 0;
    };

    // A load's pick screen opens on the first frame, which has the host to read the vote's files with.
    void reopen(PartHost &host) {
        if (phase_ != Phase::Reopen) return;
        readBase(host);
        phase_ = Phase::Pick;
        opened_ = host.nowMs();
        thoughts_.clear();
        nextThought_ = opened_;
    }

    // vote_base.ini: `KeyNN value`, `//` starting a comment.
    void readBase(PartHost &host) {
        allFrames_ = 6900;
        elected_.clear();
        events_.clear();
        auto bytes = host.read("cfg\\vote_base.ini");
        if (!bytes) {
            host.log("vote list missing: cfg\\vote_base.ini");
            return;
        }
        char nn[3];
        std::snprintf(nn, sizeof nn, "%02d", truth_);
        std::istringstream in(std::string(bytes->begin(), bytes->end()));
        std::string behavior, comments;
        for (std::string line; std::getline(in, line);) {
            line = trimmed(line.substr(0, line.find("//")));
            std::istringstream words(line);
            std::string key, value;
            if (!(words >> key >> value)) continue;
            if (key == std::string("AllTime") + nn) allFrames_ = std::atoi(value.c_str());
            else if (key == std::string("BehaviorFilename") + nn) behavior = path(value);
            else if (key == std::string("VoteComment") + nn) comments = path(value);
            else if (key == std::string("ElectedNum") + nn || key == std::string("ElectedNum") + nn + "_2") elected_.push_back(std::atoi(value.c_str()));
        }
        commentList_ = comments;
        if (auto script = behavior.empty() ? nullptr : host.read(behavior)) {
            std::istringstream lines(std::string(script->begin(), script->end()));
            for (std::string line; std::getline(lines, line);) {
                line = trimmed(line.substr(0, line.find("//")));
                std::vector<std::string> f;
                std::istringstream fields(line);
                for (std::string x; std::getline(fields, x, ',');) f.push_back(trimmed(x));
                // Lines the PC can't read pass, as two of vote 1's do (`AYA.face_shake`).
                if (f.size() < 3 || (whoOf(f[1]) < 0 && f[1] != "0")) continue;
                events_.push_back({std::atoi(f[0].c_str()), whoOf(f[1]), f[2], std::vector<std::string>(f.begin() + 3, f.end())});
            }
            std::stable_sort(events_.begin(), events_.end(), [](const Event &a, const Event &b) { return a.frame < b.frame; });
        } else {
            host.log("vote script missing: " + behavior);
        }
    }

    int panelAt(PartHost &host, int x, int y) {
        // The topmost panel whose picture is there, drawn last.
        for (int h = 2; h >= 0; h--) {
            const Panel &p = kPanels[h];
            if (!Rect{p.x, p.y, kPanelW, kPanelH}.contains(x, y)) continue;
            // A sheet that's missing, or too small to hold the place, as a replacement picture can be, leaves the whole
            // rectangle to press.
            ImageRef sheet = host.image(p.sheet);
            int sx = x - p.x, sy = y - p.y;
            if (!sheet || sx >= sheet->w || sy >= sheet->h) return h;
            if (sheet->rgba[(size_t(sy) * size_t(sheet->w) + size_t(sx)) * 4 + 3] > 128) return h;
        }
        return -1;
    }

    void startVote(PartHost &host, int pick) {
        pick_ = pick;
        phase_ = Phase::Vote;
        voteStart_ = host.nowMs();
        next_ = 0;
        lastFrame_ = -1;
        commentsHidden_ = false;
        heroines_ = {};
        for (Heroine &h : heroines_) h.y = h.target = kRowH;
        comments_.reset();
        if (!commentList_.empty()) comments_.load(host, commentList_);
        host.log("vote " + std::to_string(truth_) + ": picked " + std::to_string(pick));
        host.redraw();
    }

    bool endVote(PartHost &host) {
        bool right = std::find(elected_.begin(), elected_.end(), pick_) != elected_.end();
        host.log(std::string("vote ") + std::to_string(truth_) + (right ? ": the pick was right" : ": the pick was wrong"));
        return finish(host, right ? 1 : 0);
    }

    bool finish(PartHost &host, int result) {
        // The vote's voices and effects end with it, so none runs on into the story's next line; and the story is in
        // a scene again, 4 in the PC's mode, unless the tree opens and the pick comes back.
        if (phase_ == Phase::Vote)
            for (int who = 0; who < 3; who++) {
                host.stopSound(kVoiceKind, who);
                host.stopSound(kEffectKind, kEffectChannel + who);
            }
        if (result != kResultTree) host.setVariable(1, kModeVar, kModeScene);
        phase_ = Phase::Closed;
        comments_.reset();
        host.finishScreen(result);
        return true;
    }

    int row(const std::string &level) const { return level == "H" ? kRowH : level == "M" ? kRowM : kRowL; }

    void run(PartHost &host, const Event &e) {
        auto arg = [&](size_t i) { return i < e.args.size() ? e.args[i] : std::string(); };
        auto num = [&](size_t i) { return std::atoi(arg(i).c_str()); };
        if (e.kind == "comment") {
            comments_.play(host, num(0));
            return;
        }
        if (e.kind == "com_stop") {
            if (num(0) == 1) comments_.end(host, 0);
            return;
        }
        if (e.who < 0) return;
        Heroine &h = heroines_[size_t(e.who)];
        if (e.kind == "face") {
            h.face = std::clamp(num(0), 0, kFaceCells[e.who] - 1);
        } else if (e.kind == "face_shake" || e.kind == "coffin_shake") {
            h.shakeSideways = arg(0) == "S_X";
            h.shakeAmp = num(1);
            h.shakeUntil = e.frame + num(2);
        } else if (e.kind == "msg_shake") {
            // `msg_shake N AMP FRAMES`, N the message's number or S_R: the heroine's bubble showing shakes.
            h.bubbleShakeAmp = num(1);
            h.bubbleShakeUntil = e.frame + num(2);
        } else if (e.kind == "up_move" || e.kind == "down_move") {
            h.target = float(row(arg(0)));
            h.speed = std::strtof(arg(1).c_str(), nullptr);
        } else if (e.kind == "fall_move") {
            // A fall is a fast steady move. On the PC under Wine, in shots about 90 ms apart, Misuzu's coffin in vote 1
            // was 60 rows down, then 135, then at rest 150 down: about 830 rows a second, 14 a frame, with no sign of
            // gathering speed between the two samples a fall this short gives.
            h.target = float(row(arg(0)));
            h.speed = kFallSpeed;
        } else if (e.kind == "message") {
            // `message N SIDE SHAPE 0 0 HOW TEXT VOICE`.
            h.bubble = true;
            h.number = num(0);
            h.side = 0;
            for (size_t i = 0; i < std::size(kSides); i++)
                if (arg(1) == kSides[i].name) h.side = int(i);
            h.spiky = arg(2) == "S";
            h.pop = arg(5) == "POP";
            h.text = path(arg(6));
            h.bubbleAt = host.nowMs();
            if (!arg(7).empty()) host.playSound(kVoiceKind, e.who, path(arg(7)));
        } else if (e.kind == "se_shake") {
            // `se_shake PATH SINGLE MS`; one of vote 1's names no sound.
            if (arg(0).rfind("se", 0) == 0) host.playSound(kEffectKind, kEffectChannel + e.who, path(arg(0)));
        } else if (e.kind == "se_stop") {
            host.stopSound(kEffectKind, kEffectChannel + e.who);
        }
    }

    // A frame of the moves and shakes, at 60 a second.
    void advance(PartHost &host, int frame) {
        for (Heroine &h : heroines_) {
            if (h.y < h.target) h.y = std::min(h.target, h.y + h.speed);
            else if (h.y > h.target) h.y = std::max(h.target, h.y - h.speed);
            auto jitter = [](int amp) { return amp > 0 ? std::rand() % (2 * amp + 1) - amp : 0; };
            bool shaking = frame < h.shakeUntil;
            h.shakeX = shaking ? jitter(h.shakeAmp) : 0;
            h.shakeY = shaking && !h.shakeSideways ? jitter(h.shakeAmp) : 0;
            bool bubbleShaking = frame < h.bubbleShakeUntil;
            h.bubbleShakeX = bubbleShaking ? jitter(h.bubbleShakeAmp) : 0;
            h.bubbleShakeY = bubbleShaking ? jitter(h.bubbleShakeAmp) : 0;
        }
        // A bubble goes when its voice has finished, two seconds on at the least.
        for (int w = 0; w < 3; w++) {
            Heroine &h = heroines_[size_t(w)];
            if (h.bubble && host.nowMs() - h.bubbleAt >= kBubbleMinMs && !host.soundPlaying(kVoiceKind, w)) h.bubble = false;
        }
    }

    void drawPick(PartHost &host, Canvas &out) {
        int64_t ms = host.nowMs() - opened_;
        float in = std::min(1.0f, float(ms) / kPickFadeMs);
        put(host, out, "obj\\n000_000.bmp", 0, 0, in);
        // Its two blue edges over the dark ground.
        put(host, out, "obj\\n000_001.bmp", 0, 0, in);
        put(host, out, "obj\\n000_002.bmp", 0, 0, in);
        for (const Thought &t : thoughts_) {
            char name[32];
            std::snprintf(name, sizeof name, "obj\\n008_%03d.bmp", t.picture);
            ImageRef image = host.image(name);
            if (!image) continue;
            int64_t age = host.nowMs() - t.born;
            float fadeIn = std::min(1.0f, float(age) / kThoughtInMs), fadeOut = std::min(1.0f, float(kThoughtLifeMs - age) / kThoughtOutMs);
            float alpha = kThoughtAlpha * fadeIn * std::max(0.0f, fadeOut);
            float scale = 1 + kThoughtGrowth * float(age) / 1000;
            drawImage(out, *image, float(t.x), float(t.y), scale, scale, 0, alpha * in);
        }
        int frame = int(ms / kPanelFrameMs) % 3;
        int hovered = in >= 1 ? panelAt(host, px_, py_) : -1;
        for (int h = 0; h < 3; h++) {
            const Panel &p = kPanels[h];
            cell(host, out, p.sheet, 3, 5, frame, h == hovered ? 1 : 0, p.x, p.y, in);
        }
        for (const Panel &p : kPanels) {
            cell(host, out, "obj\\n004_003.bmp", 3, 1, p.nameCell, 0, p.nameX, p.nameY, in);
            cell(host, out, p.statement, 5, 1, truth_ - 1, 0, p.statementX, p.statementY, in);
        }
        // The bars the hub has too: the band and line along the top, the bar along the bottom with 真相 N on it.
        put(host, out, "obj\\l000_003.bmp", 0, 0, in);
        put(host, out, "obj\\l000_004.bmp", 244, 45, in);
        put(host, out, "obj\\n002_000.bmp", 1036, 19, in);
        put(host, out, "obj\\n003_000.bmp", 0, 592, in);
        cell(host, out, "obj\\l005_001.bmp", 5, 1, truth_ - 1, 0, 669, 625, in);
        cell(host, out, "obj\\n003_001.bmp", 5, 1, truth_ - 1, 0, 899, 607, in);
        cell(host, out, "obj\\l004_000.bmp", 1, 5, 0, kTreeButton.contains(px_, py_) ? 1 : 0, kTreeButton.x, kTreeButton.y, in);
        // The red line under it, 過去の自白内容を確認することが出来ます。 (set 2's place).
        put(host, out, "obj\\l004_001.bmp", 0, 667, in);
        drawClock(host, out, true, in);
    }

    // The day, and on the pick screen the time: o000_000's digits 32 apart, o000_001 for 日目 and the colon.
    void drawClock(PartHost &host, Canvas &out, bool time, float alpha) {
        int pcDay = host.variable(1, kPcDayVar);
        int day = std::clamp(pcDay ? pcDay : host.variable(1, kDayVar), 1, 9);
        int hour = (pcDay ? host.variable(1, kPcHourVar) : host.variable(1, kHourVar)) % 24;
        cell(host, out, "obj\\o000_000.bmp", 10, 1, day, 0, 14, 24, alpha);
        if (!time) {
            // Only 日目, the colon's part left out.
            if (ImageRef label = host.image("obj\\o000_001.bmp")) drawPart(out, *label, 46, 35, 0, 0, 60, label->h, alpha);
            return;
        }
        put(host, out, "obj\\o000_001.bmp", 46, 35, alpha);
        cell(host, out, "obj\\o000_000.bmp", 10, 1, hour / 10, 0, 104, 24, alpha);
        cell(host, out, "obj\\o000_000.bmp", 10, 1, hour % 10, 0, 136, 24, alpha);
        cell(host, out, "obj\\o000_000.bmp", 10, 1, 0, 0, 185, 24, alpha);
        cell(host, out, "obj\\o000_000.bmp", 10, 1, 0, 0, 217, 24, alpha);
    }

    void drawVote(PartHost &host, Canvas &out) {
        int64_t ms = host.nowMs() - voteStart_;
        int frame = ms < kVoteStartMs ? -1 : int((ms - kVoteStartMs) * 60 / 1000);
        fill(out, 0, 0, out.w, out.h, 0, 0, 0);
        fill(out, kInside.x, kInside.y, kInside.w, kInside.h, kInsideRed[0], kInsideRed[1], kInsideRed[2]);
        // The coffins' columns, under the monitor's glass. What's behind the faces isn't drawn: it's the heroines' standing
        // pictures, laid out in shots that weren't looked at, still to be placed.
        for (int column = 0; column < 3; column++)
            for (size_t k = 0; k < std::size(kHands); k++) {
                const Hand &hand = kHands[k];
                // Through its cells and round again, each hand a little out of step with the last, and each copy a third
                // of a turn behind the one before.
                for (int copy = 0; copy < kHandsCopies; copy++) {
                    int64_t at = (ms + int64_t(k) * 400 + copy * hand.periodMs / kHandsCopies) % hand.periodMs;
                    int cellNow = int(at * hand.cells / hand.periodMs);
                    int x = kInside.x + kColumnStep * column + hand.x + copy * kHandsCopyDX;
                    inside(host, out, hand.sheet, cellNow, hand.cells, x, kHandsBottom - 144);
                }
            }
        for (int x : kDividerX) inside(host, out, "obj\\o003_000.bmp", 0, 1, x, kDividerY);
        for (int w = 0; w < 3; w++) {
            const Heroine &h = heroines_[size_t(w)];
            int coffinX = kCoffinX + kColumnStep * w + h.shakeX, coffinY = int(h.y) + kCoffinDY + h.shakeY;
            if (ImageRef rope = host.image("obj\\o012_001.bmp"))
                inside(host, out, "obj\\o012_001.bmp", 0, 1, kRopeX + kColumnStep * w + h.shakeX, coffinY - rope->h);
            inside(host, out, "obj\\o012_000.bmp", 0, 1, kWheelX + kColumnStep * w, kWheelY);
            inside(host, out, "obj\\o012_002.bmp", w, 3, coffinX, coffinY);
            inside(host, out, kFaceSheets[w], h.face, kFaceCells[w], kFaceX + kColumnStep * w + h.shakeX, int(h.y) + h.shakeY);
        }
        for (int w = 0; w < 3; w++) drawBubble(host, out, w);
        if (!commentsHidden_) comments_.draw(host, out);
        // Bubbles and comments end at the monitor's edge too: black again outside it, as on the PC (set 2).
        fill(out, 0, 0, out.w, kInside.y, 0, 0, 0);
        fill(out, 0, kInside.y + kInside.h, out.w, out.h - kInside.y - kInside.h, 0, 0, 0);
        fill(out, 0, kInside.y, kInside.x, kInside.h, 0, 0, 0);
        fill(out, kInside.x + kInside.w, kInside.y, out.w - kInside.x - kInside.w, kInside.h, 0, 0, 0);
        put(host, out, "obj\\o002_000.bmp", 14, 66, 1);
        // The top bar: the day, the vote's title (o006_000, a cell for each truth), the countdown and スキップ.
        drawClock(host, out, false, 1);
        cell(host, out, "obj\\o006_000.bmp", 5, 1, truth_ - 1, 0, 108, 0, 1);
        put(host, out, "obj\\o007_000.bmp", 963, 5, 1);
        int left = std::max(0, allFrames_ - std::max(0, frame));
        bool red = left < 600;
        int seconds = left / 60, hundredths = left % 60 * 100 / 60;
        int big[4] = {seconds / 600 % 10, seconds / 60 % 10, seconds % 60 / 10, seconds % 10};
        for (int i = 0; i < 4; i++) digit(host, out, "obj\\o005_001.bmp", big[i], kDigitsX[i], 29, red);
        digit(host, out, "obj\\o007_002.bmp", hundredths / 10, kSmallX[0], 34, red);
        digit(host, out, "obj\\o007_002.bmp", hundredths % 10, kSmallX[1], 34, red);
        tinted(host, out, "obj\\o007_001.bmp", 1012, 32, red);
        tinted(host, out, "obj\\o007_003.bmp", 1071, 37, red);
        Rect skip = skipRect();
        cell(host, out, "obj\\o001_000.bmp", 1, 5, 0, skip.contains(px_, py_) ? 1 : 0, skip.x, skip.y, 1);
        Rect hide = hideRect();
        cell(host, out, "obj\\o200_000.bmp", 1, 5, 0, commentsHidden_ ? 2 : hide.contains(px_, py_) ? 1 : 0, hide.x, hide.y, 1);
        if (ms >= kStartBannerFrom && ms < kStartBannerTo) put(host, out, "obj\\o009_001.bmp", 305, 280, 1);
        if (frame >= allFrames_) put(host, out, "obj\\o009_002.bmp", 116, 229, 1);  // where 投票終了 sits isn't measured
    }

    void drawBubble(PartHost &host, Canvas &out, int w) {
        const Heroine &h = heroines_[size_t(w)];
        if (!h.bubble) return;
        const BubbleSide &side = kSides[size_t(h.side)];
        int x = kFaceX + kColumnStep * w + side.dx + h.bubbleShakeX, y = int(h.y) + side.dy + h.bubbleShakeY;
        float in = std::min(1.0f, float(host.nowMs() - h.bubbleAt) / kBubbleInMs);
        ImageRef sheet = host.image(kBubbleSheets[w][h.spiky ? 1 : 0]);
        ImageRef text = host.image(h.text);
        if (h.pop && in < 1) {
            // POP grows from the middle.
            float scale = 0.5f + 0.5f * in;
            ImageRef shape = sheet ? cellOf(sheet, 6, side.cell) : nullptr;
            if (shape) drawImage(out, *shape, x + kBubbleW / 2.0f, y + kBubbleH / 2.0f, scale, scale, 0, 1);
            if (text) drawImage(out, *text, x + kBubbleW / 2.0f, y + kBubbleH / 2.0f, scale, scale, 0, 1);
            return;
        }
        float alpha = h.pop ? 1 : in;
        if (sheet) drawPart(out, *sheet, x, y, side.cell * kBubbleW, 0, kBubbleW, kBubbleH, alpha);
        if (text) drawPart(out, *text, x, y, 0, 0, text->w, text->h, alpha);
    }

    static Rect skipRect() { return {1139, 25, 109, 39}; }
    static Rect hideRect() { return {1124, 648, 109, 39}; }

    // One cell of a sheet as a picture of its own, for drawing scaled; kept per sheet and cell.
    ImageRef cellOf(const ImageRef &sheet, int cols, int col) {
        auto key = std::make_pair(sheet.get(), col);
        auto it = cells_.find(key);
        if (it != cells_.end()) return it->second.made;
        int w = sheet->w / cols;
        if (w <= 0 || sheet->h <= 0) return nullptr;
        auto image = std::make_shared<Image>();
        image->w = w;
        image->h = sheet->h;
        image->opaque = false;
        image->rgba.resize(size_t(w) * size_t(sheet->h) * 4);
        for (int y = 0; y < sheet->h; y++)
            std::copy_n(&sheet->rgba[(size_t(y) * sheet->w + size_t(col * w)) * 4], size_t(w) * 4, &image->rgba[size_t(y) * w * 4]);
        cells_[key] = {sheet, image};
        return image;
    }

    // The countdown's pictures in red, their green and blue taken out, as the PC draws its last ten seconds.
    ImageRef redOf(const ImageRef &image) {
        auto it = red_.find(image.get());
        if (it != red_.end()) return it->second.made;
        auto copy = std::make_shared<Image>(*image);
        for (size_t i = 0; i < copy->rgba.size(); i += 4) copy->rgba[i + 1] = copy->rgba[i + 2] = 0;
        red_[image.get()] = {image, copy};
        return copy;
    }

    void digit(PartHost &host, Canvas &out, const char *sheet, int value, int x, int y, bool red) {
        ImageRef image = host.image(sheet);
        if (!image) return;
        if (red) image = redOf(image);
        int w = image->w / 10;
        drawPart(out, *image, x, y, value * w, 0, w, image->h, 1);
    }

    void tinted(PartHost &host, Canvas &out, const char *name, int x, int y, bool red) {
        ImageRef image = host.image(name);
        if (!image) return;
        if (red) image = redOf(image);
        drawPart(out, *image, x, y, 0, 0, image->w, image->h, 1);
    }

    static void fill(Canvas &out, int x0, int y0, int w, int h, uint8_t r, uint8_t g, uint8_t b) {
        for (int y = std::max(0, y0); y < std::min(out.h, y0 + h); y++)
            for (int x = std::max(0, x0); x < std::min(out.w, x0 + w); x++) {
                uint8_t *p = out.px.data() + (size_t(y) * size_t(out.w) + size_t(x)) * 4;
                p[0] = r, p[1] = g, p[2] = b, p[3] = 255;
            }
    }
    // A cell of a sheet of COLS across, clipped to the monitor's inside.
    void inside(PartHost &host, Canvas &out, const std::string &name, int col, int cols, int x, int y) {
        ImageRef image = host.image(name);
        if (!image) return;
        int w = image->w / cols, h = image->h, sx = col * w;
        int left = std::max(x, kInside.x), top = std::max(y, kInside.y);
        int right = std::min(x + w, kInside.x + kInside.w), bottom = std::min(y + h, kInside.y + kInside.h);
        if (right <= left || bottom <= top) return;
        drawPart(out, *image, left, top, sx + left - x, top - y, right - left, bottom - top, 1);
    }
    void put(PartHost &host, Canvas &out, const std::string &name, int x, int y, float alpha) {
        if (ImageRef image = host.image(name)) drawPart(out, *image, x, y, 0, 0, image->w, image->h, alpha);
    }
    void cell(PartHost &host, Canvas &out, const std::string &name, int cols, int rows, int col, int row, int x, int y, float alpha) {
        ImageRef image = host.image(name);
        if (!image) return;
        int w = image->w / cols, h = image->h / rows;
        drawPart(out, *image, x, y, col * w, row * h, w, h, alpha);
    }

    // One of the narrator's thoughts on the pick screen: its picture's number, its middle, and when it came.
    struct Thought {
        int picture, x, y;
        int64_t born;
    };
    std::vector<Thought> thoughts_;
    int64_t nextThought_ = 0;
    Phase phase_ = Phase::Closed;
    int truth_ = 1, pick_ = 0, allFrames_ = 6900, px_ = -1, py_ = -1, lastFrame_ = -1;
    size_t next_ = 0;
    int64_t opened_ = 0, voteStart_ = 0;
    bool commentsHidden_ = false;
    std::vector<int> elected_;
    std::vector<Event> events_;
    std::string commentList_;
    std::array<Heroine, 3> heroines_{};
    CommentStream comments_;
    // Pictures made from others, by the other's address: each keeps its source, so the address stays that picture's
    // while the entry lasts.
    struct Made {
        ImageRef source, made;
    };
    std::map<std::pair<const Image *, int>, Made> cells_;
    std::map<const Image *, Made> red_;
};

}  // namespace

std::unique_ptr<GamePart> makeYabaiVotePart() { return std::make_unique<YabaiVotePart>(); }

}  // namespace neko
