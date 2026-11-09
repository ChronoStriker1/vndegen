#include "comments.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

#include "font.h"
#include "sjis.h"

namespace neko {

namespace {

// As Test Game 96's PC draws them (lead, under Wine, .audit/evidence/nekosdk/lead/comments96): white 24-pixel MS Gothic
// with a black edge about 2 pixels wide, crossing the screen from right to left in 6.05 seconds whatever their length, in
// twelve rows 40 pixels apart from the top.
constexpr int kFace = 0, kSize = 24, kEdge = 2;
constexpr int64_t kCrossMs = 6050;
constexpr int kLaneTop = 10, kLanePitch = 40, kLanes = 12;
// Command 113's lines start one about every 2.1 seconds. A placed line's text stands about 10 pixels below its row,
// where a picture's own text starts about 3 below its top.
constexpr int64_t kInlineGapMs = 2100;
constexpr int kPlacedDrop = 7;

int64_t framesMs(int frames) { return int64_t(frames) * 1000 / 60; }

// A line without its carriage return and its trailing spaces.
std::string trimmed(std::string line) {
    while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
    return line;
}

// A comment's picture: its text in white over a black edge, on nothing.
ImageRef commentPicture(const std::string &text) {
    std::vector<GlyphImage> glyphs;
    int width = 0;
    for (size_t i = 0; i < text.size();) {
        int length = 1;
        char32_t code = sjis::decodeAt(text, i, &length);
        i += size_t(std::max(1, length));
        GlyphImage g;
        if (!glyph(kFace, uint32_t(code), kSize, false, g)) continue;
        width += g.advance;
        glyphs.push_back(std::move(g));
    }
    int ascent = fontAscent(kFace, kSize);
    int w = width + 2 * kEdge, h = kSize + 2 * kEdge + 4;
    if (width <= 0) return nullptr;
    std::vector<uint8_t> ink(size_t(w) * h, 0), edge(size_t(w) * h, 0);
    int pen = kEdge;
    for (const GlyphImage &g : glyphs) {
        for (int y = 0; y < g.h; y++)
            for (int x = 0; x < g.w; x++) {
                int tx = pen + g.left + x, ty = kEdge + ascent - g.top + y;
                if (tx < 0 || ty < 0 || tx >= w || ty >= h) continue;
                uint8_t &at = ink[size_t(ty) * w + tx];
                at = std::max(at, g.coverage[size_t(y) * g.w + x]);
            }
        pen += g.advance;
    }
    // The edge: the ink spread kEdge pixels every way.
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t most = 0;
            for (int dy = -kEdge; dy <= kEdge; dy++)
                for (int dx = -kEdge; dx <= kEdge; dx++) {
                    int sx = x + dx, sy = y + dy;
                    if (sx >= 0 && sy >= 0 && sx < w && sy < h) most = std::max(most, ink[size_t(sy) * w + sx]);
                }
            edge[size_t(y) * w + x] = most;
        }
    auto picture = std::make_shared<Image>();
    picture->w = w;
    picture->h = h;
    picture->opaque = false;
    picture->rgba.resize(size_t(w) * h * 4);
    for (size_t i = 0; i < size_t(w) * h; i++) {
        uint8_t white = ink[i];
        picture->rgba[i * 4 + 0] = picture->rgba[i * 4 + 1] = picture->rgba[i * 4 + 2] = white;
        picture->rgba[i * 4 + 3] = std::max(edge[i], white);
    }
    return picture;
}

}  // namespace

// `//` starts a comment line. A group's lines run from its `#p` line to `##`.
std::map<int, CommentGroup> parseCommentList(const std::string &text) {
    std::map<int, CommentGroup> groups;
    std::istringstream in(text);
    std::string line;
    CommentGroup *open = nullptr;
    while (std::getline(in, line)) {
        line = trimmed(line);
        if (line.rfind("//", 0) == 0) continue;
        if (line.rfind("#p", 0) == 0) {
            std::istringstream words(line.substr(2));
            int number = 0;
            CommentGroup group;
            words >> number >> group.a >> group.b;
            open = &(groups[number] = group);
        } else if (line == "##") {
            open = nullptr;
        } else if (open && !line.empty()) {
            open->lines.push_back(line);
        }
    }
    return groups;
}

bool CommentStream::load(PartHost &host, const std::string &path) {
    auto bytes = host.read(path);
    if (!bytes) {
        host.log("comment list missing: " + path);
        return false;
    }
    path_ = path;
    groups_ = parseCommentList(std::string(bytes->begin(), bytes->end()));
    playing_.clear();
    cursors_.clear();
    ended_ = false;
    return true;
}

// A group's lines start in the file's order, the first at once and each next after A to B frames at 60 a second, and
// the group ends with its last: group 0 of Test Game 96's first list, 12 lines at 40 to 80, gave about 12 comments 0.4
// to 1.2 seconds apart, then none, and group 10's 150 to 300 gave 2.3 to 4.5 seconds. The spread within A to B is taken
// as even.
void CommentStream::play(PartHost &host, int group) {
    playing_.push_back(group);
    ended_ = false;
    auto it = groups_.find(group);
    if (it == groups_.end()) {
        host.log("comment group missing: " + std::to_string(group));
        return;
    }
    Cursor cursor;
    cursor.lines = it->second.lines;
    cursor.a = it->second.a;
    cursor.b = std::max(it->second.a, it->second.b);
    cursor.at = host.nowMs();
    cursors_.push_back(std::move(cursor));
}

// Command 113's lines start one every 2.1 seconds; one placed as `《text》,X,Y` starts X frames on at row Y, as the
// PC's do: seven at 0 entered together at rows 20 to 440, and those at 90 about 1.5 seconds later (lead, under Wine).
void CommentStream::lines(PartHost &host, const std::vector<std::string> &texts) {
    Cursor cursor;
    int64_t now = host.nowMs();
    for (const std::string &raw : texts) {
        std::string text = trimmed(raw);
        if (text.empty()) continue;
        size_t comma = text.rfind(',');
        size_t before = comma == std::string::npos || comma == 0 ? std::string::npos : text.rfind(',', comma - 1);
        // 》 is 81 74 in the scripts' code page, and a comma is never a second byte there.
        if (before != std::string::npos && before >= 2 && text.compare(before - 2, 2, "\x81\x74") == 0) {
            char *end = nullptr;
            long x = std::strtol(text.c_str() + before + 1, &end, 10);
            if (end && *end == ',') {
                pending_.push_back({text.substr(0, before), std::atoi(text.c_str() + comma + 1) + kPlacedDrop, now + framesMs(int(x))});
                continue;
            }
        }
        cursor.lines.push_back(text);
    }
    if (cursor.lines.empty()) return;
    cursor.at = now;
    cursors_.push_back(std::move(cursor));
}

// The comments go, those on their way too, and no more start. The parameter is the group that last played (111 7 after
// group 7, 111 10 after group 10): on Test Game 96's PC, 111 10 came while group 10 still sent comments, and none was
// left three seconds on, where those on their way would have taken six (lead, under Wine).
void CommentStream::end(PartHost &, int) {
    ended_ = true;
    playing_.clear();
    cursors_.clear();
    pending_.clear();
    live_.clear();
}

void CommentStream::start(PartHost &host, const std::string &text, int y, int64_t at) {
    ImageRef picture = commentPicture(text);
    if (!picture) return;
    live_.push_back({picture, y < 0 ? kLaneTop + freeLane(host, at) * kLanePitch : y, at});
}

// A row whose last comment has all come in, at random; any row when none has.
int CommentStream::freeLane(PartHost &host, int64_t now) const {
    std::vector<int> open;
    for (int lane = 0; lane < kLanes; lane++) {
        bool busy = false;
        for (const Live &c : live_) {
            if (c.y != kLaneTop + lane * kLanePitch) continue;
            int64_t in = (int64_t(host.width()) + c.picture->w) * (now - c.born) / kCrossMs;
            if (in < c.picture->w + kLanePitch) busy = true;
        }
        if (!busy) open.push_back(lane);
    }
    return open.empty() ? std::rand() % kLanes : open[size_t(std::rand()) % open.size()];
}

bool CommentStream::step(PartHost &host) {
    int64_t now = host.nowMs();
    for (Cursor &cursor : cursors_)
        while (cursor.next < cursor.lines.size() && now >= cursor.at) {
            start(host, cursor.lines[cursor.next++], -1, cursor.at);
            int gap = cursor.a + (cursor.b > cursor.a ? std::rand() % (cursor.b - cursor.a + 1) : 0);
            cursor.at += cursor.a || cursor.b ? framesMs(gap) : kInlineGapMs;
        }
    cursors_.erase(std::remove_if(cursors_.begin(), cursors_.end(), [](const Cursor &c) { return c.next >= c.lines.size(); }), cursors_.end());
    for (auto it = pending_.begin(); it != pending_.end();) {
        if (now < it->at) {
            ++it;
            continue;
        }
        start(host, it->text, it->y, it->at);
        it = pending_.erase(it);
    }
    live_.erase(std::remove_if(live_.begin(), live_.end(), [&](const Live &c) { return now - c.born >= kCrossMs; }), live_.end());
    return !live_.empty();
}

void CommentStream::draw(PartHost &host, Canvas &out) const {
    int64_t now = host.nowMs();
    int width = host.width();
    for (const Live &c : live_) {
        int x = width - int((int64_t(width) + c.picture->w) * (now - c.born) / kCrossMs);
        drawPart(out, *c.picture, x, c.y, 0, 0, c.picture->w, c.picture->h, 1);
    }
}

// `list PATH`, `group N` for each group started, `ended`. Those on their way and placed lines aren't kept, as a save
// comes between a scene's lines and the PC's saves hold none of them.
void CommentStream::saveState(std::vector<std::string> &lines) const {
    if (!path_.empty()) lines.push_back("list " + path_);
    for (int group : playing_) lines.push_back("group " + std::to_string(group));
    if (ended_) lines.push_back("ended");
}

void CommentStream::loadState(PartHost &host, const std::vector<std::string> &lines) {
    reset();
    for (const std::string &line : lines) {
        if (line.rfind("list ", 0) == 0) load(host, line.substr(5));
        else if (line.rfind("group ", 0) == 0) playing_.push_back(std::atoi(line.c_str() + 6));
        else if (line == "ended") ended_ = true;
    }
}

void CommentStream::reset() {
    path_.clear();
    groups_.clear();
    playing_.clear();
    cursors_.clear();
    pending_.clear();
    live_.clear();
    ended_ = false;
}

}  // namespace neko
