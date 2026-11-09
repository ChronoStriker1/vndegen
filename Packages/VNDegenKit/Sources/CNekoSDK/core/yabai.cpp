// The `yabai` game part: Test Game 96's viewer comments over its scenes, scene commands 110 to 113.
//   110 [コメントリスト読み込み]  loads the comment list s1 (cfg\com\com_XX_NNNN.ini)
//   111 [コメント終了]          ends the comments, with p0 (0 or 6 to 10)
//   112 [コメントグループ再生]   plays group p0 of the list
//   113 [強調文再生]            plays the lines in s1, one to a line, some as `《text》,X,Y`
// and the message window's Comment button, which hides and shows them.
// The first uses are in scr\sc_001_000_000_001.txt, from node 974.
#include <sstream>

#include "comments.h"

namespace neko {
namespace {

class YabaiPart : public GamePart {
public:
    bool command(PartHost &host, const Node &n) override {
        switch (n.opcode) {
        case 110:
            comments_.load(host, n.s[1]);
            return true;
        case 111:
            comments_.end(host, n.p[0]);
            return true;
        case 112:
            comments_.play(host, n.p[0]);
            return true;
        case 113: {
            std::vector<std::string> texts;
            std::istringstream in(n.s[1]);
            for (std::string line; std::getline(in, line);) texts.push_back(line);
            comments_.lines(host, texts);
            return true;
        }
        default:
            return false;
        }
    }
    bool step(PartHost &host) override {
        // A load has no host to read the list with, so its lines wait for the first step.
        if (!pending_.empty()) {
            comments_.loadState(host, pending_);
            pending_.clear();
        }
        return comments_.step(host);
    }
    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        if (layer == Layer::UnderWindow && !hidden_) comments_.draw(host, out);
    }
    // The message window's Comment button (msg_button.ini's `#inner Comment`, a toggle) hides the comments and shows
    // them again; they go on running hidden. With no comments on screen or to come it's drawn disabled, as the PC draws
    // it on a line that has none (set 2, a PC shot under Wine), and does nothing. It's drawn lit while they're hidden,
    // which hasn't been checked on the PC.
    bool action(PartHost &host, const std::string &name) override {
        if (name != "Comment") return false;
        if (comments_.active()) hidden_ = !hidden_;
        host.redraw();
        return true;
    }
    int actionLit(const std::string &name) const override {
        if (name != "Comment") return -1;
        return !comments_.active() ? 2 : hidden_ ? 1 : 0;
    }
    void save(std::vector<std::string> &lines) const override { comments_.saveState(lines); }
    void load(const std::vector<std::string> &lines) override { pending_ = lines; }
    void reset() override {
        comments_.reset();
        pending_.clear();
    }

private:
    CommentStream comments_;
    std::vector<std::string> pending_;
    bool hidden_ = false;  // the player's choice, kept across scenes and loads
};

}  // namespace

std::unique_ptr<GamePart> makeYabaiPart() { return std::make_unique<YabaiPart>(); }

}  // namespace neko
