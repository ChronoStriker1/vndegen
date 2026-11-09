// Test Game 96's viewer comments: lines its PC plays over a scene as if typed by a live stream's viewers. The scene
// commands load a list of them and play its groups (the `yabai` part), and the vote screens play their own lists
// (set 2's part), each through a CommentStream of its own.
#pragma once
#include <string>
#include <vector>

#include "canvas.h"
#include "image.h"
#include "parts.h"

namespace neko {

// One `#p GROUP A B` block of a list file: its number, its two numbers and its lines, in the file's code page.
struct CommentGroup {
    int number = 0, a = 0, b = 0;
    std::vector<std::string> lines;
};

// The blocks of a list file's text, in the file's order.
std::vector<CommentGroup> parseCommentList(const std::string &text);
// Group N of a list: the first block numbered N, or with none, the block at place N. Test Game 96's com_11_0003 and
// _0004 number two blocks 1 and none 2, and com_31_0003 two 5 and none 8, though their scenes play 2 and 8; com_21_0003
// numbers its sixth block 8 and its ninth 5, so the numbers count where they're given. Which the PC plays is unconfirmed.
const CommentGroup *findCommentGroup(const std::vector<CommentGroup> &groups, int number);

class CommentStream {
public:
    // A list file's groups: `#p GROUP A B`, its lines, `##` (cfg\com\*.ini, cfg\CommentList*.ini). False when missing.
    bool load(PartHost &host, const std::string &path);
    // Group N of the loaded list starts now; one already playing goes on.
    void play(PartHost &host, int group);
    // Lines given inline, as command 113's: each `《text》` or `《text》,X,Y`.
    void lines(PartHost &host, const std::vector<std::string> &texts);
    // The comments end; command 111's parameter says how.
    void end(PartHost &host, int how);
    // Each frame, on host.nowMs(); true while anything moves.
    bool step(PartHost &host);
    // Over the scene, at the layer the holder chooses.
    void draw(PartHost &host, Canvas &out) const;
    // VNDegen's own lines beside a save, and a fresh start.
    void saveState(std::vector<std::string> &lines) const;
    void loadState(PartHost &host, const std::vector<std::string> &lines);
    void reset();

private:
    // A placed line waiting for its time: its text, row and start.
    struct Pending {
        std::string text;
        int y = 0;
        int64_t at = 0;
    };
    // A source of lines still to start: a group's, or the inline ones.
    struct Cursor {
        std::vector<std::string> lines;
        int a = 0, b = 0;  // the gap before the next, in frames at 60 a second, from a to b; 0 for inline lines
        size_t next = 0;
        int64_t at = 0;  // when the next starts
    };
    // A comment on its way across: its picture, its row and when it entered at the right edge.
    struct Live {
        ImageRef picture;
        int y = 0;
        int64_t born = 0;
    };
    void start(PartHost &host, const std::string &text, int y, int64_t at);
    int freeLane(PartHost &host, int64_t now) const;

    std::string path_;
    std::vector<CommentGroup> groups_;
    std::vector<int> playing_;  // the groups started, in order, for a save
    std::vector<Pending> pending_;
    std::vector<Cursor> cursors_;
    std::vector<Live> live_;
    bool ended_ = false;
};

}  // namespace neko
