// The Wolf RPG Editor game state and event interpreter. Rendering and sound are separate, so this part runs and is
// tested without a display.
#pragma once

#include "data.h"
#include "files.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace wolf {

// Wolf's own keys. The host maps keyboards, controllers and touch onto them.
enum Key : uint32_t {
    KeyDown = 1 << 0,
    KeyLeft = 1 << 1,
    KeyRight = 1 << 2,
    KeyUp = 1 << 3,
    KeyConfirm = 1 << 4,
    KeyCancel = 1 << 5,
    KeySub = 1 << 6,
};

struct Input {
    uint32_t held = 0;
    uint32_t pressed = 0;  // went down this frame
};

// What a picture shows: an image file, text, or a window skin stretched to a size.
enum class PictureKind { None, File, Text, Window };

struct Tween {
    double from = 0, to = 0;
    int frames = 0, elapsed = 0;
    double value() const { return frames <= 0 || elapsed >= frames ? to : from + (to - from) * elapsed / frames; }
    void set(double v, int over) {
        from = value();
        to = v;
        frames = over;
        elapsed = 0;
    }
    void step() { if (elapsed < frames) elapsed++; }
};

struct Picture {
    PictureKind kind = PictureKind::None;
    std::string source;  // file path or text
    int anchor = 0;      // 0 top left, 1 centre, 2 bottom left, 3 bottom right? (Wolf's anchor codes divided by 16)
    int blend = 0;       // 0 normal, 1 add, 2 subtract, 3 multiply
    int divX = 1, divY = 1, pattern = 1;
    int windowW = 0, windowH = 0;
    Tween x, y, opacity, zoomX, zoomY, angle, r, g, b;
    uint64_t generation = 0;  // changes whenever the source changes, so the renderer reloads it
};

struct ChoiceState {
    bool active = false;
    std::vector<std::string> items;  // empty strings are hidden
    std::vector<int> visible;        // indices of the shown items
    int cursor = 0;
    int cancelCase = -1;  // case to take on cancel, or -1 when cancel does nothing
    int result = -1;
};

struct MessageState {
    bool active = false;
    std::string text;
};

struct Frame;

// One running event: a map event page or a common event, with its call stack.
struct Thread {
    struct Call {
        const Commands *commands = nullptr;
        size_t pc = 0;
        int commonEvent = -1;  // common event id, or -1 for a map event
        int mapEvent = -1;
    };
    std::vector<Call> stack;
    int wait = 0;
    bool waitingMessage = false, waitingChoice = false;
    int choiceIndent = 0;
    std::map<size_t, int> loopCounts;  // remaining passes of counted loops, by the loop command's index
    bool done() const { return stack.empty(); }
};

struct MapEventState {
    const MapEvent *data = nullptr;
    int page = -1;  // active page, or -1
    std::array<int32_t, 10> self{};
    bool erased = false;
    bool ranAutorun = false;
};

class Game {
public:
    explicit Game(std::filesystem::path root);

    bool load(std::string *error);
    // Advances one frame.
    void update(const Input &input);

    const GameDat &gameDat() const { return gameDat_; }
    const GameFiles &files() const { return files_; }
    const std::map<int, Picture> &pictures() const { return pictures_; }
    const ChoiceState &choice() const { return choice_; }
    const MessageState &message() const { return message_; }
    const Map *map() const { return map_.get(); }
    int screenWidth() const { return gameDat_.screenWidth; }
    int screenHeight() const { return gameDat_.screenHeight; }
    // The tint over the whole screen, as Wolf's change screen colour sets it: 100 is unchanged per channel.
    std::array<double, 3> screenColor() const { return {tintR_.value(), tintG_.value(), tintB_.value()}; }
    std::string windowSkin() const;
    std::string cursorSkin() const;
    std::string systemString(int i) const;
    std::array<uint8_t, 3> textColor(int i) const;
    uint64_t frameCount() const { return frame_; }

    // Commands the interpreter has met but does not run yet, with how often. For the logs.
    const std::map<int, int> &unsupported() const { return unsupported_; }
    std::function<void(const std::string &)> log = [](const std::string &) {};

    // Expands Wolf's text codes that the interpreter resolves: \v[n], \s[n], \self[n], \cself[n], \sys[n].
    std::string expandText(const std::string &text, const Thread *t) const;

private:
    // Variables.
    int32_t value(int32_t v, const Thread &t);  // a literal, or the variable it names when 1,000,000 or above
    int32_t readVar(int32_t id, const Thread &t);
    void writeVar(int32_t id, int32_t v, const Thread &t);
    bool isStringVar(int32_t id) const;
    std::string readStr(int32_t id, const Thread &t) const;
    void writeStr(int32_t id, const std::string &s, const Thread &t);
    int32_t *selfSlot(int32_t id, const Thread &t);
    int32_t dbNumber(const Database &db, int type, int row, int field) const;
    std::string dbString(const Database &db, int type, int row, int field) const;

    // Interpreter.
    void run(Thread &t);
    bool step(Thread &t);  // runs one command; false when the thread must wait
    void jumpToBranchEnd(Thread::Call &c, int indent);
    bool jumpToCase(Thread::Call &c, int indent, int caseValue);
    void callCommon(Thread &t, int id, const Command &cmd, int argBase);
    int commonByName(const std::string &name) const;
    void pictureCommand(Thread &t, const Command &c);
    void setVariable(Thread &t, const Command &c);
    void setString(Thread &t, const Command &c);
    bool condition(Thread &t, const Command &c);
    bool stringCondition(Thread &t, const Command &c);
    void unsupported(const Command &c);

    // Maps and events.
    bool loadMap(int id);
    int activePage(const MapEventState &e, const Thread &ctx);
    void refreshPages();
    void startPage(int eventIndex, Thread &t);
    bool commonConditionMet(const CommonEvent &ce);

    std::filesystem::path root_;
    GameFiles files_;
    GameDat gameDat_;
    std::vector<CommonEvent> commons_;
    Database udb_, cdb_, sdb_;
    std::vector<Tileset> tilesets_;

    std::vector<int32_t> normal_;
    std::array<std::vector<int32_t>, 9> spare_;
    std::vector<int32_t> system_;
    std::vector<std::string> strings_, systemStrings_;
    std::vector<std::array<int32_t, 100>> cself_;
    std::vector<std::array<std::string, 100>> cselfStr_;

    int mapId_ = -1;
    std::unique_ptr<Map> map_;
    std::vector<MapEventState> events_;
    int heroX_ = 0, heroY_ = 0, heroDir_ = 2;

    std::vector<Thread> threads_;      // autorun and called events, run in order
    std::vector<Thread> parallel_;     // parallel common events, one per event
    std::vector<Thread> mapParallel_;  // parallel map event pages
    std::map<int, Picture> pictures_;
    ChoiceState choice_;
    MessageState message_;
    Tween tintR_, tintG_, tintB_;
    Input input_;
    uint64_t frame_ = 0, generation_ = 0;
    std::mt19937 rng_{12345};
    std::map<int, int> unsupported_;
};

}  // namespace wolf
