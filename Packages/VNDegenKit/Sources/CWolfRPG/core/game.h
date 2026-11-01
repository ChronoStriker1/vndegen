// The Wolf RPG Editor game state and event interpreter. Rendering and sound are separate, so this part runs and is
// tested without a display.
#pragma once

#include "audio.h"
#include "data.h"
#include "files.h"

#include <array>
#include <bitset>
#include <deque>
#include <cstdlib>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <set>
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

// Mouse buttons, as bits of Input's mouse fields.
enum MouseButton : uint32_t {
    MouseLeft = 1 << 0,
    MouseRight = 1 << 1,
    MouseMiddle = 1 << 2,
};

// Keyboard keys by Wolf's own key code, 100 and up: 100 plus the DirectInput scan code of the key (the manual's key
// input page and WodiLib's KeyboardCode).
constexpr int kKeyCodeCount = 356;
using KeyCodes = std::bitset<kKeyCodeCount>;

struct Input {
    uint32_t held = 0;
    uint32_t pressed = 0;  // went down this frame
    KeyCodes keysHeld, keysPressed;
    int mouseX = 0, mouseY = 0;  // in the game's pixels
    uint32_t mouseHeld = 0, mousePressed = 0;
    int wheel = 0;  // notches this frame, positive away from the player
};

// An XY array (Wolf RPG Editor 3's command 255): numbers by X, then by Y.
using XYArray = std::vector<std::vector<int32_t>>;

// A picture named <XY_IMAGE/A=../R=../G=../B=../XSIZE=../YSIZE=..> or <XYN_IMAGE/...>, the second going by array names:
// copies of the four arrays, null where one is missing or left empty, and the image's size. The picture shows them as
// they were when shown. The manual's picture page: an empty alpha means 255 and an empty colour 0, and without XSIZE or
// YSIZE the largest array gives the size.
struct XYImage {
    std::array<std::shared_ptr<const XYArray>, 4> channels{};  // A, R, G, B
    int w = 0, h = 0;
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
    int anchor = 0;      // Wolf's anchor code divided by 16: 0 top left, 1 centre, 2 bottom left, 3 top right,
                         // 4 bottom right, 5 top centre
    int blend = 0;       // 0 normal, 1 add, 2 subtract, 3 multiply
    int divX = 1, divY = 1, pattern = 1;
    int imageW = 0, imageH = 0;  // size of the image file, the text or the window, for the variable command
    // <CUT/Xa-b/Yc-d> before a file name shows that part of each pattern, in pixels, inclusive. cutW is 0 without one.
    int cutX = 0, cutY = 0, cutW = 0, cutH = 0;
    std::shared_ptr<const XYImage> xyImage;  // for a picture made from XY arrays
    int windowW = 0, windowH = 0;  // size of a window picture: a window skin stretched, or a shape
    Tween x, y, opacity, zoomX, zoomY, angle, r, g, b;
    uint64_t generation = 0;  // changes whenever the source changes, so the renderer reloads it
    // Effects from the effect command.
    int autoPatternMode = 0;  // 0 none, 1 once, 2 loop, 3 back and forth
    int autoFrom = 1, autoTo = 1, autoInterval = 1, autoTimer = 0, autoDir = 1;
    int shakePower = 0, shakeSpeed = 0, shakeFrames = 0;
    double shakeOffset = 0;
    int shiftX = 0, shiftY = 0;  // the effect command's draw position shift, added to the position when drawing
    // The effect command's zoom: the picture scales around a screen point by this rate, on top of its own zoom.
    Tween effectZoom{100, 100}, effectZoomX, effectZoomY;
    // The effect command's pulse (点滅A): the colour swings to its value plus these offsets and back, once every two
    // intervals. An interval of 0 stops it.
    std::array<int, 3> pulse{0, 0, 0};
    int pulseInterval = 0;
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
    bool heldForChoice = false;  // stays up until the choice after it is answered
};

// The string command's keyboard mode while the player types.
struct TextEntry {
    bool active = false;
    int32_t target = 0;
    int maxLength = 0;
    bool canCancel = false;
    std::string text;
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
        // Remaining passes of this call's counted loops, by the loop command's index. A loop reached other than from
        // its own end or a "continue" (`repeating`) starts its count afresh, so one left early by a break, a label
        // jump or the end of the event doesn't carry its count into the next time.
        std::map<size_t, int> loopCounts;
        bool repeating = false;
    };
    std::vector<Call> stack;
    int wait = 0;
    bool waitingMessage = false, waitingChoice = false, waitingText = false;
    int choiceIndent = 0;
    const struct Actor *waitingRoute = nullptr;
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
    bool pageChanged = false;  // since its parallel script last started
};

class Game {
public:
    explicit Game(std::filesystem::path root);

    bool load(std::string *error);
    // The General MIDI SoundFont, and the one to load when it can't be read. Set before load, so the start map's MIDI
    // music has it.
    void setSoundFont(std::string path, std::string fallback);
    // For test hosts: goes back to the title at the next frame, as the event command does.
    void debugReturnToTitle() { pendingTitle_ = true; }
    // Advances one frame.
    void update(const Input &input);

    const GameDat &gameDat() const { return gameDat_; }
    const GameFiles &files() const { return files_; }
    const std::map<int, Picture> &pictures() const { return pictures_; }
    const ChoiceState &choice() const { return choice_; }
    void setChoiceBox(float x, float y, float w, float lineH) const { choiceBox_ = {x, y, w, lineH}; }
    const MessageState &message() const { return message_; }
    const TextEntry &textEntry() const { return textEntry_; }
    const XYArray *xyArray(int number) const;
    const XYArray *xyArray(const std::string &name) const;
    // Text the player typed, for the string command's keyboard mode. Does nothing when it isn't waiting.
    void typeText(const std::string &utf8);
    const Map *map() const { return map_.get(); }
    int mapId() const { return mapId_; }
    // The hero's tile and facing, for tests and the debug status.
    std::array<int, 3> heroTile() const { return {hero_.tx, hero_.ty, hero_.chara.direction}; }
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
    int32_t systemNumber(int i) const;
    std::array<uint8_t, 3> textColor(int i) const;
    uint64_t frameCount() const { return frame_; }
    const ShakeState &shake() const { return shake_; }
    // The effect command's map zoom: the rate in percent and the screen point it zooms around. The map, its
    // characters, the background and fog zoom; pictures don't.
    std::array<double, 3> mapZoom() const { return {mapZoom_.value(), mapZoomX_.value(), mapZoomY_.value()}; }
    const TransitionState &transition() const { return transition_; }
    double scrollX() const { return scrollX_; }
    double scrollY() const { return scrollY_; }
    bool quitRequested() const { return quitRequested_; }
    // The game's sound. The host pulls its mix from the audio thread with Audio::render.
    Audio *audio() const { return audio_.get(); }

    // Commands the interpreter has met but does not run yet, with how often. For the logs.
    const std::map<int, int> &unsupported() const { return unsupported_; }
    std::function<void(const std::string &)> log = [](const std::string &) {};
    bool trace_ = std::getenv("WOLF_TRACE") != nullptr;  // logs every command run

    // Expands Wolf's text codes that the interpreter resolves: \v[n], \s[n], \self[n], \cself[n], \sys[n].
    std::string expandText(const std::string &text, const Thread *t) const;

    // Cheats (cheats.cpp), for VNDegen's Cheats page. Each changes the game's own state, so a save made afterwards is
    // an ordinary save.
    // Whether the game keeps gold, items and the party where Wolf RPG Editor's standard systems keep them.
    bool hasStandardParty() const;
    // The state as JSON, in the format VNCore's CheatState(json:) reads.
    std::string cheatState() const;
    // Each returns false when the game doesn't have what it changes.
    bool cheatAddGold(int amount);
    bool cheatAddItem(int kind, int id, int count);  // kind 0 item, 1 weapon, 2 armor
    bool cheatRestoreParty();
    bool cheatSetVariable(int id, int value);
    void setWalkThroughWalls(bool on) { walkThroughWalls_ = on; }
    // The host runs `speed` of the game's frames for each one of real time, so fades started now take that much less.
    void setSpeed(int speed);

private:
    bool walkThroughWalls_ = false;
    // Variables.
    int32_t value(int32_t v, const Thread &t);  // a literal, or the variable it names when 1,000,000 or above
    int32_t readVar(int32_t id, const Thread &t);
    void writeVar(int32_t id, int32_t v, const Thread &t);
    bool isStringVar(int32_t id) const;
    std::string readStr(int32_t id, const Thread &t) const;
    void writeStr(int32_t id, const std::string &s, const Thread &t);
    int32_t *selfSlot(int32_t id, const Thread &t);
    // The database cell a 1 X TT DDDD FF address names, or null when the database has no such cell or it holds the
    // other kind of value.
    int32_t *dbNumberCell(int32_t id);
    std::string *dbStringCell(int32_t id);
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
    void extendedString(Thread &t, const Command &c, int32_t target, int op);
    std::string expandRegexCodes(const std::string &pattern, const Thread &t) const;
    std::string hiddenCode(const std::string &name, const std::string &arg);
    std::string makeEvent(const std::string &options, const std::string &image);
    std::string findPath(const std::string &options);
    void startTextEntry(Thread &t, const Command &c);
    void updateTextEntry();
    std::string textAt(int32_t address, const Thread &t);
    void showMessage(std::string text);
    bool condition(Thread &t, const Command &c);
    bool stringCondition(Thread &t, const Command &c);
    void unsupported(const Command &c);
    const CommonEvent *commonById(int id) const;
    void databaseCommand(Thread &t, const Command &c);
    void setVariablePlus(Thread &t, const Command &c);
    void csvCommand(Thread &t, const Command &c);
    void dbRowCommand(Thread &t, const Command &c);  // 252: copying and sorting rows
    void xyCommand(Thread &t, const Command &c);     // 255
    void xySortCommand(Thread &t, const Command &c);  // 257
    XYArray *xyTarget(Thread &t, const Command &c, bool create);
    bool keyInput(Thread &t, const Command &c);
    void partyCommand(Thread &t, const Command &c);
    void effectCommand(Thread &t, const Command &c);
    void soundCommand(Thread &t, const Command &c);
    void playSoundEffect(int id);  // a sound from the system database's SE list
    void chipCommand(Thread &t, const Command &c);
    // Saves (savestate.cpp): the save and load commands, and reading and writing Save/SaveDataNN.sav as the PC
    // engine does.
    void saveCommand(Thread &t, const Command &c);
    // --- savestate.cpp: declarations for saving and loading go here ---
public:
    // Writes the game as it stands to Save/SaveDataNN.sav (slot 1 is SaveData01.sav), in the PC engine's format.
    bool saveSlot(int slot, std::string *error = nullptr);
    // Reads Save/SaveDataNN.sav. The game goes on from it at the start of the next frame, and every running event
    // ends. False, with the game unchanged, when the slot is empty or unreadable.
    bool loadSlot(int slot, std::string *error = nullptr);

private:
    std::vector<uint8_t> readData(const std::string &path) const;
    void readDatabases();
    bool startNewGame(std::string *error);
    void returnToTitle();
    std::filesystem::path saveFilePath(const std::string &name) const;  // a file under the game folder, any case
    std::shared_ptr<struct SaveFile> readSave(const std::filesystem::path &path, std::string *error) const;
    bool writeSave(const std::filesystem::path &path, const SaveFile &save, std::string *error);
    SaveFile snapshot();       // the current state, unknown fields from saveBase_
    SaveFile blankSave();      // what the engine writes when a variable goes into a save that doesn't exist yet
    void applyPendingLoad();   // Game::update calls it first thing while pendingLoad_ is set
    void applySave(const SaveFile &s);
    // Reads or writes the variable at `address` in a save, for commands 221 and 222.
    void readSavedVariable(const SaveFile *s, int32_t address, int32_t target, Thread &t);
    void writeSavedVariable(SaveFile &s, int32_t address, int32_t source, Thread &t);
    std::filesystem::path saveTarget(int32_t slotArg, const Thread &t);
    int playSeconds() const;
    int playFrames() const;

    std::shared_ptr<SaveFile> pendingLoad_;
    // The save this session last read or wrote. New saves copy the fields VNDegen doesn't model from it, so the PC
    // engine finds them as it left them.
    std::shared_ptr<SaveFile> saveBase_;
    bool wolf35_ = false;  // the game's data is in Wolf RPG Editor 3.5's format
    int32_t loadedPlaySeconds_ = 0, loadedPlayFrames_ = 0;
    uint64_t frameAtLoad_ = 0;
    // --- end of savestate.cpp declarations ---
    Database *databaseKind(int kind);
    uint32_t keysNow() const;
    void gatherInput(const Input &input);  // this frame's input: the host's, with auto input and the game's key rules
    void keyControl(Thread &t, const Command &c);  // command 126
    void autoInput(Thread &t, const Command &c);   // command 125
    bool pictureUnderMouse(const Picture &p) const;
    int eventUnderMouse() const;
    void chooseWithMouse(int n);

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
    Actor *actorAtAddress(int32_t id, const Thread &t);
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
    // A deque, so an event a hidden code adds moves no other: moves in progress point at their characters.
    std::deque<MapEventState> events_;
    // Events hidden codes made. They leave events_ with the map, as the manual says, but stay here until the title,
    // since a call that outlives the map change may still be running one's commands.
    std::deque<MapEvent> madeEvents_;
    std::map<int, std::string> eventNames_;  // names hidden codes gave events, by id
    // Self variables of map events, kept per map and event id for as long as the game runs.
    std::map<std::pair<int, int>, std::array<int32_t, 10>> eventSelf_;
    Actor hero_;
    std::array<Actor, 5> party_;
    double cameraX_ = 0, cameraY_ = 0;

    // Autorun and called events, run in order. A deque, so an event that queues another (command 211) while it runs
    // keeps its own place in memory.
    std::deque<Thread> threads_;
    std::vector<Thread> parallel_;     // parallel common events, one per event
    std::vector<Thread> mapParallel_;  // parallel map event pages
    std::map<int, Picture> pictures_;
    std::map<int32_t, XYArray> xyNumbered_;
    std::map<std::string, XYArray> xyNamed_;
    ChoiceState choice_;
    MessageState message_;
    TextEntry textEntry_;
    Thread textEntryScope_;  // a copy of the call that asked, whose self variables the target may name
    std::set<std::string> hiddenCodesLogged_;  // what has been logged once: hidden codes and missing event names
    Tween tintR_, tintG_, tintB_;
    Input input_;
    std::array<int, 7> heldFrames_{};  // frames each Wolf key has been held, by bit of Key
    uint64_t frame_ = 0, generation_ = 0;
    std::shared_ptr<class TextRenderer> textMeasure_;  // sizes text pictures (pictureCommandOne; text.h)
    std::mt19937 rng_{12345};
    std::map<int, int> unsupported_;
    std::unique_ptr<Audio> audio_;
    void playListed(SoundKind kind, int id, int fadeFrames, int startMs = 0);
    void playMapMusic();

    bool moveDuringEvents_ = false, haltNonPicture_ = false, scrollLocked_ = false;
    bool pendingTitle_ = false, quitRequested_ = false;
    std::string soundFont_, soundFontFallback_;
    // What command 125 presses, which counts as input on the next frame.
    uint32_t autoKeys_ = 0, autoMouse_ = 0;
    KeyCodes autoKeyCodes_;
    int autoWheel_ = 0;
    // Command 126: for each Wolf key, 0 accepts it, 1 stops it moving the hero or starting events, and 2 stops it
    // altogether. Keys the game turned off by device.
    std::array<int, 7> basicControl_{};
    bool keyboardOff_ = false, mouseOff_ = false;
    KeyCodes keyCodesOff_;
    // Last frame's state, for the edges of pressing and letting go.
    uint32_t lastHeld_ = 0, lastMouseHeld_ = 0;
    KeyCodes lastKeysHeld_;
    std::array<int, 3> mouseHeldFrames_{};
    std::array<int, kKeyCodeCount> keyHeldFrames_{};
    int mouseX_ = 0, mouseY_ = 0, lastMouseX_ = 0, lastMouseY_ = 0, hostMouseX_ = -1, hostMouseY_ = -1;
    // Where the renderer drew the choice window, for the mouse. Zero height until it first draws.
    struct ChoiceBox { float x = 0, y = 0, w = 0, lineH = 0; };
    mutable ChoiceBox choiceBox_;
    ShakeState shake_;
    Tween mapZoom_{100, 100}, mapZoomX_, mapZoomY_;
    TransitionState transition_;
    int transitionFrames_ = 0;
    double scrollX_ = 0, scrollY_ = 0;
};

class Game;
XYImage resolveXYImage(const std::string &source, const Game &game);

}  // namespace wolf
