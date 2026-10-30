// The Wolf RPG Editor game state and event interpreter. Rendering and sound are separate, so this part runs and is
// tested without a display.
#pragma once

#include "data.h"
#include "files.h"

#include <array>
#include <cstdlib>
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
    int imageW = 0, imageH = 0;  // size of the image file, for the variable command
    int windowW = 0, windowH = 0;  // size of a window picture
    Tween x, y, opacity, zoomX, zoomY, angle, r, g, b;
    uint64_t generation = 0;  // changes whenever the source changes, so the renderer reloads it
    // Effects from the effect command.
    int autoPatternMode = 0;  // 0 none, 1 once, 2 loop, 3 back and forth
    int autoFrom = 1, autoTo = 1, autoInterval = 1, autoTimer = 0, autoDir = 1;
    int shakePower = 0, shakeSpeed = 0, shakeFrames = 0;
    double shakeOffset = 0;
    int flickerFrames = 0, flickerInterval = 0;
    bool erasing = false;
};

struct ShakeState {
    int power = 0, speed = 0, type = 0, frames = 0;
    double offset = 0;
};

struct TransitionState {
    bool prepared = false;  // the screen is frozen
    uint64_t generation = 0;
    int frames = 0, elapsed = 0;  // a crossfade from the frozen screen in progress
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
    std::string text;        // without a leading face code
    int face = -1;           // face graphic from "@n" at the start of the text, or -1
    size_t shown = 0, total = 0;  // characters typed so far, and in all
    bool instant = false;
};

// A character on the map: the hero, a party member following the hero, or a map event.
struct Character {
    std::string graphic;  // character image file; nothing is drawn when it is empty and chip is negative
    int chip = -1;        // a tile of the map's tileset, drawn instead of an image
    double x = 0, y = 0;  // position in tiles, fractional while moving
    int direction = 2;    // numeric keypad: 2 down, 4 left, 6 right, 8 up, 1 3 7 9 the diagonals
    int pattern = 1;      // animation frame, from 0; 1 is standing in three-frame sets
    int opacity = 255;
    int blend = 0;          // 0 normal, 1 add, 2 subtract, 3 multiply
    int lift = 0;           // pixels the character is drawn above its tile, for jumps and height settings
    bool visible = true;    // false for erased events and hidden party members
    bool above = false;     // drawn over everything else on the map
    int shadow = 0;         // shadow graphic number from the system database, 0 for none
};

// One running event: a map event page or a common event, with its call stack.
struct Thread {
    struct Call {
        const Commands *commands = nullptr;
        size_t pc = 0;
        int commonEvent = -1;  // common event id, or -1 for a map event
        int mapEvent = -1;
        int32_t returnTo = 0;  // where a called common event's return value goes, or 0
        // A map event's page keeps its map alive, since the event runs on after moving the hero elsewhere.
        std::shared_ptr<const Map> map;
        int mapId = -1;
    };
    std::vector<Call> stack;
    int wait = 0;
    bool waitingMessage = false, waitingChoice = false;
    int choiceIndent = 0;
    const struct Actor *waitingRoute = nullptr;
    std::map<size_t, int> loopCounts;  // remaining passes of counted loops, by the loop command's index
    bool done() const { return stack.empty(); }
};

// A character that moves on the map, with its movement state.
struct Actor {
    Character chara;
    int tx = 0, ty = 0;  // the tile it stands on, or is moving to
    double fromX = 0, fromY = 0;
    int moveFrame = 0, moveFrames = 0;  // a step in progress
    int jumpHeight = 0;
    int speed = 3;      // 0 slowest to 6 fastest
    int frequency = 3;  // how often autonomous movement steps, 0 every frame to 5 rarely
    int animSpeed = 3;
    bool through = false, fixedDirection = false, walkAnimation = true, idleAnimation = false;
    int animTimer = 0, animStep = 0;
    // A move route from the event's page or a move command.
    std::shared_ptr<const MoveRoute> route;  // shares ownership of the map or command that holds it
    size_t routeStep = 0;
    int routeWait = 0;
    bool routeFromCommand = false;
    int idleTimer = 0;
    bool moving() const { return moveFrame < moveFrames; }
    void placeAt(int x, int y) {
        tx = x;
        ty = y;
        chara.x = x;
        chara.y = y;
        moveFrame = moveFrames = 0;
    }
};

struct MapEventState {
    const MapEvent *data = nullptr;
    Actor actor;
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
    int mapId() const { return mapId_; }
    const std::vector<Tileset> &tilesets() const { return tilesets_; }
    // The hero, party members and map events, in no particular order. The map renderer sorts them.
    std::vector<const Character *> characters() const;
    // The top left of the screen on the map, in pixels.
    double cameraX() const { return cameraX_; }
    double cameraY() const { return cameraY_; }
    int tileSize() const { return gameDat_.tileSize(); }
    // Moves the hero to a map position at once, for tests.
    bool debugTeleport(int mapId, int x, int y);
    int screenWidth() const { return gameDat_.screenWidth; }
    int screenHeight() const { return gameDat_.screenHeight; }
    // The tint over the whole screen, as Wolf's change screen colour sets it: 100 is unchanged per channel.
    std::array<double, 3> screenColor() const { return {tintR_.value(), tintG_.value(), tintB_.value()}; }
    std::string windowSkin() const;
    std::string cursorSkin() const;
    std::string pauseIcon() const;
    int pauseIconFrames() const;
    std::string faceGraphic(int n) const;
    int choiceTop() const { return system_.size() > 4 ? system_[4] : -1; }
    std::string systemString(int i) const;
    std::array<uint8_t, 3> textColor(int i) const;
    uint64_t frameCount() const { return frame_; }
    const ShakeState &shake() const { return shake_; }
    const TransitionState &transition() const { return transition_; }
    double scrollX() const { return scrollX_; }
    double scrollY() const { return scrollY_; }
    bool quitRequested() const { return quitRequested_; }

    // Commands the interpreter has met but does not run yet, with how often. For the logs.
    const std::map<int, int> &unsupported() const { return unsupported_; }
    std::function<void(const std::string &)> log = [](const std::string &) {};
    bool trace_ = std::getenv("WOLF_TRACE") != nullptr;  // logs every command run

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
    void pictureCommandOne(Thread &t, const Command &c, int number);
    void setVariable(Thread &t, const Command &c);
    void setString(Thread &t, const Command &c);
    std::string textAt(int32_t address, const Thread &t);
    void showMessage(std::string text);
    bool condition(Thread &t, const Command &c);
    bool stringCondition(Thread &t, const Command &c);
    void unsupported(const Command &c);
    const CommonEvent *commonById(int id) const;
    void databaseCommand(Thread &t, const Command &c);
    void setVariablePlus(Thread &t, const Command &c);
    bool keyInput(Thread &t, const Command &c);
    void partyCommand(Thread &t, const Command &c);
    void effectCommand(Thread &t, const Command &c);
    void soundCommand(Thread &t, const Command &c);
    void playSoundEffect(int id);  // a sound from the system database's SE list
    void chipCommand(Thread &t, const Command &c);
    void saveCommand(Thread &t, const Command &c);
    Database *databaseKind(int kind);
    uint32_t keysNow() const;

    // Maps and events.
    bool loadMap(int id);
    int activePage(const MapEventState &e, const Thread &ctx);
    void refreshPages();
    void startPage(int eventIndex, Thread &t);
    bool commonConditionMet(const CommonEvent &ce);
    void updateCamera();
    void updatePictures();
    void syncPartyGraphics();

    // Characters (characters.cpp).
    Actor *actorFor(int target, const Thread &t);  // -1 this event, -2 the hero, 0 and up a map event id
    uint32_t tileFlags(int x, int y) const;         // passability of the uppermost tile that decides it
    bool canMove(const Actor &a, int dir) const;
    bool step(Actor &a, int dir);                  // starts a step; false when blocked
    void turn(Actor &a, int dir);
    void updateActor(Actor &a, bool isHero);
    void runRoute(Actor &a, bool isHero);
    void heroControl();
    void checkTouch(int x, int y, bool heroMoved);
    bool eventAt(int x, int y, int trigger, bool startIt);
    int framesPerStep(const Actor &a) const;
    void autonomousMove(MapEventState &e);
    void updateCharacters();
    bool anyRouteRunning() const;

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

    int mapId_ = -1, loadedMap_ = -1;
    std::shared_ptr<Map> map_;
    std::vector<MapEventState> events_;
    // Self variables of map events, kept per map and event id for as long as the game runs.
    std::map<std::pair<int, int>, std::array<int32_t, 10>> eventSelf_;
    Actor hero_;
    std::array<Actor, 5> party_;
    double cameraX_ = 0, cameraY_ = 0;

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

    bool moveDuringEvents_ = false, haltNonPicture_ = false, scrollLocked_ = false;
    bool pendingTitle_ = false, quitRequested_ = false;
    uint32_t autoKeys_ = 0;
    int keyControl_ = 0;
    ShakeState shake_;
    TransitionState transition_;
    int transitionFrames_ = 0;
    double scrollX_ = 0, scrollY_ = 0;
};

}  // namespace wolf
