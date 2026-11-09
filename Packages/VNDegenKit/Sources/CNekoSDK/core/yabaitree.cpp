// The `yabaitree` game part: Test Game 96's confession tree, 自白内容確認, the screen its interrogation hub opens to look
// back over each heroine's questions, replay a confession or ask a question straight from it. The story table opens it
// with `open tree` (set 2's hub hands over), and it reads and writes VNDegen's own story variables, which the hub keeps:
//   S 15100           the truth level, 1 to 5
//   S 15101..15103    each heroine's stage, 1 to 3, or 4 once done (Ayano, Misuzu, Rina)
//   S 15108           the heroine the hub shows, 0 to 2, which the tree opens on
//   S 15130 + 9*(truth-1) + 3*heroine + (stage-1)   the questions asked at that stage, a bit each
// and on leaving S 15110, the chapter to play (0 for none), and S 15111, 1 to replay it or 2 to ask it.
// Laid out against the PC's tree under Wine, from set 2's saves at truth 1 and 2 (lead,
// .audit/evidence/nekosdk/lead/tree96): rows 135 apart from y 126, one to a stage up to the heroine's current one,
// questions 274 apart from x 234; a solved stage's asked questions carry the 自白済み stamp and a ▶ replay, its others are
// drawn at 0.313; the current stage shows its questions to ask; the red line joins the right answers, the picture and
// column each heroine's *_tree.ini names.
#include <algorithm>
#include <array>
#include <cstdlib>
#include <sstream>

#include "parts.h"

namespace neko {
namespace {

constexpr int kTruthVar = 15100, kStageVar = 15101, kShownVar = 15108, kChapterVar = 15110, kHowVar = 15111, kAskedVar = 15130;
constexpr int kRowTop = 126, kRowStep = 135, kColLeft = 234, kColStep = 274;
const char *const kTreeFiles[3] = {"cfg\\ayano_tree.ini", "cfg\\misuzu_tree.ini", "cfg\\rina_tree.ini"};

// One stage of one truth for one heroine, as her tree file has it.
struct Stage {
    int questions = 3, answer = 0;
    std::string ask, after;  // the question sheets before and after asking
    std::string line;        // the red line from this stage's right answer to the next's
    int linePos = 0;
};

// A rectangle on the screen.
struct Rect {
    int x, y, w, h;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

class YabaiTreePart : public GamePart {
public:
    bool openScreen(PartHost &host, const std::string &name, const std::vector<int> &) override {
        if (name != "tree") return false;
        if (!loaded_) readTrees(host);
        open_ = true;
        truth_ = viewed_ = std::clamp(host.variable(1, kTruthVar), 1, 5);
        heroine_ = std::clamp(host.variable(1, kShownVar), 0, 2);
        host.redraw();
        return true;
    }

    void pointer(PartHost &host, int x, int y) override {
        px_ = x;
        py_ = y;
        host.redraw();
    }

    bool press(PartHost &host, int x, int y, int button) override {
        if (!open_) return false;
        if (!loaded_) readTrees(host);
        if (button == 1 || kClose.contains(x, y)) return leave(host, 0, 0);
        for (int h = 0; h < 3; h++)
            if (tab(h).contains(x, y)) {
                heroine_ = h;
                host.redraw();
                return true;
            }
        for (int t = 1; t <= truth_; t++)
            if (plate(t).contains(x, y)) {
                viewed_ = t;
                host.redraw();
                return true;
            }
        int shown = shownStages(host);
        for (int s = 1; s <= shown; s++) {
            const Stage &st = stage(viewed_, s);
            for (int q = 0; q < st.questions; q++) {
                bool asked = isAsked(host, viewed_, s, q);
                if (asked && replay(st, s, q).contains(x, y)) return leave(host, chapter(s, q), 1);
                if (!asked && askable(host, s) && panel(st, s, q, true).contains(x, y)) return leave(host, chapter(s, q), 2);
            }
        }
        return true;
    }

    // Controller focus: the close button, the tabs and plates, each ▶ replay and each question to ask.
    bool focusSpots(PartHost &host, std::vector<std::pair<int, int>> &spots) override {
        if (!open_) return false;
        if (!loaded_) readTrees(host);
        auto middle = [&](const Rect &r) { spots.push_back({r.x + r.w / 2, r.y + r.h / 2}); };
        middle(kClose);
        for (int h = 0; h < 3; h++) middle(tab(h));
        for (int t = 1; t <= truth_; t++) middle(plate(t));
        int shown = shownStages(host);
        for (int s = 1; s <= shown; s++) {
            const Stage &st = stage(viewed_, s);
            for (int q = 0; q < st.questions; q++) {
                bool asked = isAsked(host, viewed_, s, q);
                if (asked) middle(replay(st, s, q));
                else if (askable(host, s)) middle(panel(st, s, q, true));
            }
        }
        return true;
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        if (!open_ || layer != Layer::Screen) return;
        if (!loaded_) readTrees(host);
        put(host, out, "obj\\m000_000.bmp", 0, 8);
        put(host, out, "obj\\m000_003.bmp", 21, 34);
        put(host, out, "obj\\m000_004.bmp", 880, 92);
        cell(host, out, "obj\\m000_001.bmp", 1, 5, 0, kClose.contains(px_, py_) ? 1 : 0, 1222, 0);
        put(host, out, "obj\\m000_002.bmp", 1222, 52);
        put(host, out, "obj\\m000_006.bmp", 161, 677);
        for (int h = 0; h < 3; h++) {
            Rect r = tab(h);
            cell(host, out, "obj\\l002_003.bmp", 3, 5, h, h == heroine_ ? 2 : r.contains(px_, py_) ? 1 : 0, r.x, r.y);
        }
        for (int t = 1; t <= truth_; t++) {
            Rect r = plate(t);
            cell(host, out, "obj\\m000_005.bmp", 5, 5, t - 1, t == viewed_ ? 2 : r.contains(px_, py_) ? 1 : 0, r.x, r.y);
        }
        cell(host, out, "obj\\m001_000.bmp", 3, 1, heroine_, 0, 842, 598);
        cell(host, out, "obj\\m001_001.bmp", 3, 1, heroine_, 0, 810, 639);

        int shown = shownStages(host);
        bool done = !askable(host, shown);
        for (int s = 1; s <= shown; s++) {
            const Stage &st = stage(viewed_, s);
            int top = rowTop(s);
            // The grey joint from the stage above to this one's questions.
            if (s > 1) put(host, out, st.questions == 2 ? "obj\\m010_001.bmp" : st.questions >= 4 ? "obj\\m010_003.bmp" : "obj\\m010_002.bmp", 353, top - 22);
            // The red line from this stage's right answer to the next's, once the next is solved too.
            bool nextSolved = s + 1 < shown || (s + 1 == shown && done);
            if (nextSolved && !st.line.empty()) put(host, out, st.line, 353 + kColStep * st.linePos, top + 55);
        }
        for (int s = 1; s <= shown; s++) {
            const Stage &st = stage(viewed_, s);
            bool solved = s < shown || done;
            for (int q = 0; q < st.questions; q++) {
                bool asked = isAsked(host, viewed_, s, q);
                Rect r = panel(st, s, q, !solved && !asked);
                if (!solved && !asked) {
                    cell(host, out, st.ask, 4, 5, q, r.contains(px_, py_) ? 1 : 0, r.x, r.y);
                    continue;
                }
                cell(host, out, st.after, 4, 1, q, 0, r.x, r.y, asked ? 1.0f : 0.313f);
                if (!asked) continue;
                put(host, out, "obj\\m012_000.bmp", r.x + 57, r.y - 13);
                Rect play = replay(st, s, q);
                cell(host, out, "obj\\m009_000.bmp", 1, 5, 0, play.contains(px_, py_) ? 1 : 0, play.x, play.y);
            }
        }
        if (done) {
            cell(host, out, "obj\\m012_001.bmp", 5, 1, viewed_ - 1, 0, 1012, 603);
        } else {
            // The hint under the questions to ask, 5 rows below them.
            put(host, out, "obj\\m002_000.bmp", 30, rowTop(shown) + 89 + 5);
        }
    }

    // An open tree is kept with a save, `open TRUTH VIEWED HEROINE`, so a load brings it back as it was.
    void save(std::vector<std::string> &lines) const override {
        if (open_) lines.push_back("open " + std::to_string(truth_) + " " + std::to_string(viewed_) + " " + std::to_string(heroine_));
    }
    void load(const std::vector<std::string> &lines) override {
        for (const std::string &line : lines) {
            std::istringstream words(line);
            std::string kind;
            int truth = 0, viewed = 0, heroine = 0;
            if (words >> kind >> truth >> viewed >> heroine && kind == "open") {
                open_ = true;
                truth_ = std::clamp(truth, 1, 5);
                viewed_ = std::clamp(viewed, 1, truth_);
                heroine_ = std::clamp(heroine, 0, 2);
            }
        }
    }

    void reset() override {
        open_ = false;
        px_ = py_ = -1;
    }

private:
    static constexpr Rect kClose{1222, 0, 58, 58};

    static Rect tab(int h) { return {842 + 120 * h + (h == 2 ? 2 : 0), 35, 118, 58}; }
    static Rect plate(int t) { return {29 + 143 * (t - 1), 623, 138, 53}; }
    static int rowTop(int s) { return kRowTop + kRowStep * (s - 1); }

    // A question's panel: a stage of fewer than three questions is centred under the three columns.
    static Rect panel(const Stage &st, int s, int q, bool asking) {
        int x = kColLeft + kColStep * q + (3 - st.questions) * kColStep / 2;
        return {x, rowTop(s), asking ? 270 : 264, asking ? 89 : 72};
    }
    static Rect replay(const Stage &st, int s, int q) {
        Rect r = panel(st, s, q, false);
        return {r.x + 217, r.y + 23, 33, 32};
    }

    int chapter(int s, int q) const { return 1000 * (heroine_ + 1) + 100 * (viewed_ - 1) + 10 * (s - 1) + q; }

    // The stages a heroine shows at the viewed truth: up to her current one, or all of a past truth's.
    int shownStages(PartHost &host) const {
        int count = stageCount(viewed_), current = host.variable(1, kStageVar + heroine_);
        return viewed_ < truth_ || current > count ? count : std::max(1, current);
    }
    // A stage can be asked when it's the heroine's current one at the current truth.
    bool askable(PartHost &host, int s) const {
        return viewed_ == truth_ && s == host.variable(1, kStageVar + heroine_) && s <= stageCount(viewed_);
    }
    bool isAsked(PartHost &host, int t, int s, int q) const {
        return (host.variable(1, kAskedVar + 9 * (t - 1) + 3 * heroine_ + (s - 1)) >> q) & 1;
    }
    int stageCount(int t) const {
        int count = 0;
        while (count < 8 && !tree_[size_t(heroine_)][size_t(t - 1)][size_t(count)].ask.empty()) count++;
        return std::max(1, count);
    }
    const Stage &stage(int t, int s) const { return tree_[size_t(heroine_)][size_t(t - 1)][size_t(std::clamp(s, 1, 8) - 1)]; }

    bool leave(PartHost &host, int chapterNumber, int how) {
        host.setVariable(1, kChapterVar, chapterNumber);
        host.setVariable(1, kHowVar, how);
        host.setVariable(1, kShownVar, heroine_);
        open_ = false;
        host.finishScreen(chapterNumber ? 1 : 0);
        return true;
    }

    // Each heroine's tree file: `LevelNNRankMMNum`, `...Ans`, and the question pictures in lines the file keeps as
    // comments (`//LevelNNRankMMFilename` and `AfterFilename`), with the red lines `LevelNNRedLineFileK` and `PosK`.
    void readTrees(PartHost &host) {
        loaded_ = true;
        for (int h = 0; h < 3; h++) {
            auto bytes = host.read(kTreeFiles[h]);
            if (!bytes) continue;
            std::istringstream in(std::string(bytes->begin(), bytes->end()));
            for (std::string line; std::getline(in, line);) {
                size_t at = line.find("Level");
                if (at == std::string::npos || line.size() < at + 7) continue;
                std::istringstream words(line.substr(at));
                std::string key, value;
                words >> key >> value;
                int level = std::atoi(key.substr(5, 2).c_str());
                if (level < 1 || level > 5) continue;
                auto &stages = tree_[size_t(h)][size_t(level - 1)];
                std::string rest = key.substr(7);
                std::string path = value;
                for (size_t d; (d = path.find("\\\\")) != std::string::npos;) path.erase(d, 1);
                if (rest.rfind("Rank", 0) == 0 && rest.size() > 6) {
                    int rank = std::atoi(rest.substr(4, 2).c_str());
                    if (rank < 1 || rank > 8) continue;
                    Stage &st = stages[size_t(rank - 1)];
                    std::string field = rest.substr(6);
                    if (field == "Num") st.questions = std::clamp(std::atoi(value.c_str()), 1, 4);
                    else if (field == "Ans") st.answer = std::atoi(value.c_str());
                    else if (field == "Filename") st.ask = path;
                    else if (field == "AfterFilename") st.after = path;
                } else if (rest.rfind("RedLineFile", 0) == 0) {
                    int k = std::atoi(rest.substr(11).c_str());
                    if (k >= 1 && k <= 8) stages[size_t(k - 1)].line = path;
                } else if (rest.rfind("RedLinePos", 0) == 0) {
                    int k = std::atoi(rest.substr(10).c_str());
                    if (k >= 1 && k <= 8) stages[size_t(k - 1)].linePos = std::atoi(value.c_str());
                }
            }
        }
    }

    void put(PartHost &host, Canvas &out, const std::string &path, int x, int y, float alpha = 1) {
        if (ImageRef image = host.image(path)) drawPart(out, *image, x, y, 0, 0, image->w, image->h, alpha);
    }
    void cell(PartHost &host, Canvas &out, const std::string &path, int cols, int rows, int col, int row, int x, int y, float alpha = 1) {
        ImageRef image = path.empty() ? nullptr : host.image(path);
        if (!image) return;
        int w = image->w / cols, h = image->h / rows;
        drawPart(out, *image, x, y, col * w, row * h, w, h, alpha);
    }

    bool loaded_ = false, open_ = false;
    int truth_ = 1, viewed_ = 1, heroine_ = 0, px_ = -1, py_ = -1;
    std::array<std::array<std::array<Stage, 8>, 5>, 3> tree_{};
};

}  // namespace

std::unique_ptr<GamePart> makeYabaiTreePart() { return std::make_unique<YabaiTreePart>(); }

}  // namespace neko
