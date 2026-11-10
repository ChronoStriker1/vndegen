// The running game: the script interpreter, the sprites, the message window and the transitions.
//
// Command meanings come from the games' own data and the PC game under Wine; docs/nekosdk-cleanroom.md says which are
// settled and which are readings still to be checked.
#pragma once
#include <tuple>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "archive.h"
#include "audio.h"
#include "canvas.h"
#include "image.h"
#include "movie.h"
#include "parts.h"
#include "save.h"
#include "script.h"
#include "ui.h"

namespace neko {

// init.ini: tab-separated keys, CP932, with // comments.
class Config {
public:
    void parse(const std::vector<uint8_t> &data);
    std::string text(const std::string &key, const std::string &fallback = "") const;
    int number(const std::string &key, int fallback = 0) const;

private:
    std::unordered_map<std::string, std::string> values_;
};

// One timed change of a sprite's property. `relative` values add to the base, the others replace it.
struct Track {
    enum Property { X, Y, ScaleX, ScaleY, Rotation, Alpha } property = X;
    bool relative = false;
    bool easeOut = false;
    // `quadratic` with a positive length, which Test Game 87's fly-outs ease in by.
    bool easeIn = false;
    float from = 0, to = 0;
    int startMs = 0, lengthMs = 0;
};

// The tracks a command started on a sprite. An EX change runs once; a named animation may repeat.
struct Animation {
    std::vector<Track> tracks;
    int64_t begunMs = 0;
    bool repeat = false;
    int periodMs = 0;
    bool hasPivot = false;
    int pivotX = -1, pivotY = -1;  // the point a named `scale` keeps still, -1 for the middle
    // A named animation's text as command 19 gave it, which the PC writes into a save as it is.
    std::string source;
};

struct Sprite {
    ImageRef image;
    std::string path;
    float x = 0, y = 0, scaleX = 1, scaleY = 1, rotation = 0, alpha = 1;
    int blurAmount = 0;
    bool blurOn = false;
    // Timed changes run side by side until each finishes, when it folds into the values above. The named animation
    // runs over them all until it's stopped.
    std::vector<Animation> changes;
    Animation loop;
    bool looping = false;
    // The point in the picture a scale keeps in place, or -1 for its middle, and whether a command has set it: the PC saves
    // 0, 0 for a picture whose anchor nothing set (Test Game 93 under Wine).
    int anchorX = -1, anchorY = -1;
    bool anchorSet = false;
    // A flipbook: numbered pictures shown in turn.
    int flipCount = 0, flipMs = 25, flipShown = 0;
    // A movie playing as the picture, and the story time it started at.
    std::shared_ptr<Movie> movie;
    int64_t movieFrom = 0;
    int64_t flipBegun = 0;
    bool flipLoop = false, flipReverse = false;
    // Command 121's parameters 15 and 16, which saves keep for the PC.
    int flipStep = 0, flipParam15 = 0;
    bool waveOn = false;
    // An effect painted on black, which the PC game adds to the picture.
    bool additive = false;
    // A picture tiled along an axis and moving along it, in pixels a second, from scrollFrom: Test Game 86's lace strips
    // (commands 90 and 91, a game part's, through PartHost::loadImage).
    float scrollX = 0, scrollY = 0;
    int64_t scrollFrom = 0;
    // A slot a game part holds with no picture of its own (PartHost::holdSlot): the part draws it in the slot's order,
    // and it's shown and taken away as a picture is. `kind` is the number a save's record holds for a part's slot,
    // held or a picture (SavedSprite::kind), 0 for the engine's own.
    bool held = false;
    int kind = 0;
    int waveSpeed = 0, waveRows = 0, waveSize = 0;
    // The SDK's film noise (commands 51 and 52), its picture the noise frame and the story time it began.
    bool noise = false;
    int64_t noiseFrom = 0;
};

struct SpriteView {
    float x, y, scaleX, scaleY, rotation, alpha;
};

using Sprites = std::map<int, Sprite>;  // the slot is the draw order

struct Glyph {
    int x, y;  // the pen on the canvas
    uint32_t codepoint;
    bool italic;
};

// macrolist.ini's global and scenario variables, by their place as scripts count it.
struct MacroLists {
    std::vector<int> global, scenario;
};
MacroLists parseMacroList(const std::vector<uint8_t> &file);

class Engine {
public:
    bool start(const std::string &root, const std::string &dataDir, const std::string &logPath, const std::string &fontDir, std::string &error);
    // The host's movie decoder, before start. Without one, movies are logged and left out.
    void setMovies(const VNNekoMovies &movies) { movies_ = movies; hasMovies_ = true; }
    ~Engine();

    bool step(int ms);
    const uint8_t *frame(int &w, int &h, bool &changed);
    // The size of the host's view for the game, from which an automatic 4:3 frame is chosen.
    void setViewSize(float w, float h) { viewAspect_ = h > 0 ? w / h : 0; }
    Audio &audio() { return audio_; }

    void mouseMove(float x, float y);
    void setHostTaps(bool on) { hostTaps_ = on; }
    int choiceResult() const { return choiceResult_; }
    bool choiceKept() const { return choiceMade_ || !choiceImage_.empty(); }
    // For test hosts: the choice on screen, or -1, and picking one.
    int choiceCount() const { return waiting_ == Waiting::Choice ? choiceCount_ : -1; }
    void choose(int index);
    void mouseButton(int button, bool down);
    void mouseWheel(int notches);
    // Moves the pointer to the nearest control that way, for a controller or arrow keys. False when there is none here.
    bool moveFocus(int dx, int dy);
    // A held skip stopped at unread text stays stopped until it's let go.
    void setSkip(bool on) {
        skip_ = on && !skipStopped_;
        if (!on) skipStopped_ = false;
    }
    bool inMenu() const { return screen_ != nullptr; }
    // For test hosts: presses a menu button by its action, as a click on it does.
    bool pressAction(const std::string &action);
    void setPaused(bool paused);
    // The host window's state, for Config's screen mode, and the change Config asks for: 0 a window, 1 full screen, -1
    // none.
    void setWindowState(bool canFullScreen, bool fullScreen) {
        if (canFullScreen != canFullScreen_ || fullScreen != fullScreen_) dirty_ = true;
        canFullScreen_ = canFullScreen;
        fullScreen_ = fullScreen;
    }
    int takeWindowRequest() { return std::exchange(windowRequest_, -1); }
    bool pausesWhenInactive() const { return setting("pause_inactive") != 0; }
    int confirmKey() const { return setting("deckey"); }

    const char *problem() const { return problem_.empty() ? nullptr : problem_.c_str(); }
    const char *debugStatus();
    bool waitingForClick() const { return waiting_ == Waiting::Click; }
    int textCount() const { return textCount_; }
    int textAdvance(uint32_t cp, int own) const;
    int debugTextAdvance(uint32_t cp);
    // A global (scope 0) or scenario variable (1) by its number, for tests.
    // Whether a sound still plays in this channel of music (kind 0), effects (1) or voices (2).
    bool soundPlaying(int kind, int channel);
    int variable(int scope, int number) const {
        const auto &table = scope == 1 ? scenario_ : globals_;
        auto it = table.find(number);
        return it == table.end() ? 0 : it->second;
    }
    bool jump(const std::string &script, int node);
    // A click on a card's comment asks the host for one: the card's slot and its comment now, or 0 when nothing's asked.
    int takeCommentRequest(std::string &utf8);
    // The comment the player typed, written into that card's save as the PC writes it.
    bool setComment(int slot, const std::string &utf8);
    // Something the title asks the host to show, once per click: "alert" with its text (Version info), "manual" with the
    // full path of the game's own manual page, or "url" with a web page the PC opens in a browser. False when nothing's
    // asked. A press from a test host never asks for the web page.
    bool takeHostRequest(std::string &kind, std::string &payload);
    // For checks: lays out a message as the message window sets it, replacing the one shown, and gives the furthest
    // right and lowest pixel its ink reaches, and the canvas's size to hold them against.
    void messageExtent(const std::string &utf8Name, const std::string &utf8Text, int &right, int &bottom);
    int canvasWidth() const { return width_; }
    int canvasHeight() const { return height_; }
    // Saving, loading and a card's lock, which tests also call.
    bool saveToSlot(int slot);
    bool loadFromSlot(int slot);
    void toggleLock(int slot);

private:
    friend class PartHost;
    enum class Waiting { None, Time, Click, Choice, StaffRoll, Ended, PartScreen };
    struct Frame {
        std::shared_ptr<Script> script;
        const Node *node = nullptr;
    };

    void log(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void warnOnce(const std::string &what);
    // Movies: command 100 loads one into a slot like a picture, and each step shows the frame due.
    void loadMovie(const Node &n);
    // A flow's staff roll, the table's `staffroll` screen: `movie VARIABLE FILE...` plays the file the scenario
    // variable numbers, over everything, and `skipif G G...` lets a click end it once one of those globals is set.
    // Or, as Test Game 84's exe draws its own, the screen's pictures as layers over black, `length MS` long, with `music
    // FILE`, and `sets G` set as it ends. A story table's `staffroll NAME` opens `screen staffroll_NAME` instead.
    void startStaffRoll(const std::string &name);
    void stepStaffRoll();
    void endStaffRoll();
    void composeStaffRoll(Canvas &out);
    Canvas staffRollGroup_;
    bool staffRollSkips_ = false;
    const Screen *staffRollLayers_ = nullptr;
    int64_t staffRollFrom_ = 0;
    std::shared_ptr<Movie> openMovie(const std::string &path, bool loop);
    void stepMovies();
    void stopMovieSound();
    VNNekoMovies movies_{};
    bool hasMovies_ = false;
    std::shared_ptr<Movie> soundMovie_;  // the movie whose sound the mix plays
    void complain(const std::string &what);

    // Interpreter.
    std::shared_ptr<Script> loadScript(const std::string &path);
    void run();
    void runScene(const Node &n);
    void runFlow(const Node &n);
    void advance();
    void prefetchAhead();
    bool flowActive() const { return !stack_.empty(); }

    // Saves.
    // `suffix` "s" for the Previous Choice file beside the save.
    std::string savePath(int slot, const char *extension, const char *suffix = "") const;
    void deleteSlot(int slot);
    bool cardAt(int slot, int &x, int &y) const;
    bool overCardDelete(int slot, int x, int y) const { return overCardPart("slotdelete", slot, x, y); }
    bool overCardPart(const char *key, int slot, int x, int y) const;
    int newestQuickSlot() { return newestSlot(firstQuickSlot_, lastQuickSlot_); }
    int newestSlot(int first, int last);
    // A locked save can be loaded but not saved over or deleted. The lock is kept in cfg.sav, as the PC keeps it.
    bool locked(int slot) const;
    // `slotcomment X BASELINE WIDTH SIZE PROMPT...`: the comment's place on a card, and what an empty one shows.
    bool overComment(int slot, int x, int y) const;
    int commentRequest_ = 0;
    std::string root_, hostRequestKind_, hostRequestPayload_;
    bool testPress_ = false;
    std::set<std::string> testsPlayed_;
    std::string lastScene_;
    int64_t screenMs_ = 0, openingFrom_ = -1;  // a menu's own clock, and when the title's opening began
    std::unordered_map<std::string, bool> paintedOnBlack_;
    int64_t trackQuarterShown_ = -1;
    int firstQuickSlot_ = 81, lastQuickSlot_ = 88, nextQuickSlot_ = 81;
    bool barPinned_ = false;
    SaveData captureSave();
    void noteThumbnail();
    struct SlotInfo {
        bool exists = false;
        int date[6] = {};
        ImageRef thumbnail;
        std::string flow;  // the flow script the save is in, for its chapter's name
        int route = 0;  // its route mode, for the card's progress label
        std::map<int, int> variables;  // the save's scenario variables, for a card that names its chapter by one
        std::string comment;  // typed on a PC, UTF-8
    };
    SlotInfo &slotInfo(int slot);
    bool pageOff(const std::string &action) const;
    bool pageStepOff(const std::string &action) const;
    bool buttonOff(const std::string &action) const;
    bool partDisables(const std::string &action) const;
    void composeSlots(Canvas &out);
    void drawComment(Canvas &out, const std::string &utf8, int x, int baseline, int width, int px);
    int slotAt(int x, int y) const;
    void composeDialog(Canvas &out);
    void openDialog(int question, const std::function<void()> &yes);

    // Galleries.
    void loadGalleries();
    void noteSeen(const std::string &path);
    bool seen(const std::string &path) const;
    void saveSeen();
    // Flipbooks, and what anime_cfg.ini gives each: its pictures and the ms each shows.
    void startFlipbook(const Node &n);
    void advanceFlipbooks();
    void resumeFlipbook(Sprite &s);
    struct FlipModes { std::string path; bool loop, reverse; };
    // VNDegen's lines beside a save, kept in `saveNNN.vnd`.
    void writeFlipModes(int slot, const SaveData &d);
    std::string sidecarText(const SaveData &d);
    // Return to Choice (BackSelection): the story as the latest choice began waiting, as the PC's Previous Choice file
    // holds it, with VNDegen's lines for it. Taken on the step after the choice is asked, written beside each save and
    // read back with it.
    struct PreviousChoice {
        SaveData data;
        std::string side;
    };
    std::optional<PreviousChoice> previousChoice_;
    std::optional<PreviousChoice> setAside_, setAsideChoice_;
    int setAsideResult_ = 0;
    std::string dragSetting_;
    bool hostTaps_ = false;
    int64_t clockMs_ = 0, discFrom_ = 0, discSeenAt_ = -1000000;
    bool discUp_ = false, discTurning_ = false;
    void drawDisc(Canvas &out, const ImageRef &picture, int x, int y, int cols, int rows, int column, int row, int axis, float speed);
    std::vector<std::pair<const Picture *, const Image *>> darkenedOver_;
    void drawButtonDisc(Canvas &out, const Button &b, int down = 0);
    bool discUpFor(const std::string &picture);
    ButtonState messageButtonState(const Button &b);
    int windowMoved_ = 0;
    std::map<std::tuple<const Image *, int, int>, std::pair<ImageRef, ImageRef>> discCells_;
    bool resumeSetAside();
    bool previousChoiceDue_ = false;
    // The latest choice asked, as Test Games 82 and 83's saves keep it (SaveData::choiceColumns): its options, the sheet's
    // columns, and the cells shown, never cleared.
    int askedCount_ = 0, askedColumns_ = 0, askedResult_ = 0;
    std::vector<int> askedCells_;
    void notePreviousChoice();
    void writePreviousChoice(int slot);
    void readPreviousChoice(int slot);
    void returnToChoice();
    // The switch that asks a dialog question: Config's own for it, or the one `confirm` covering every question.
    std::string confirmSetting(int question) const;
    void takeScaleAnchor(Sprite &s, const Node &n);
    void anchorOf(const Sprite &s, int &x, int &y) const;
    bool paintedOnBlack(const std::string &path);
    bool fullWidthPlate() const;
    // A table picture with `turn`, at the angle the screen's clock has brought it to.
    Picture turned(const Picture &p) const;
    bool plateUp() const;
    bool partPlate() const;
    // Whether the table's `screen defaults` has this setting.
    bool defaultsHave(const char *key) const;
    bool composeTitleOpening(Canvas &out);
    // Where the pointer is, from -1 at the screen's left or top edge to 1 at the other, or the middle before it's been seen.
    void pointerToward(float &x, float &y) const;
    // Whether the player's Individual Voices settings let this voice file play.
    bool voiceAllowed(const std::string &voice) const;
    int soundGroup(int kind, const std::string &path) const;  // the volume a sound plays at
    int cardsPerPage() const;
    void cardPlace(int index, int &x, int &y) const;
    std::string mainFlowKey() const;
    bool galleryPageExists(int page) const;
    void composeTrackCards(Canvas &out);
    void composeTrackTime(Canvas &out);
    void restoreFlipbookModes(Sprite &s, int slot, int frame, const std::map<int, FlipModes> &slots, int recordRepeat = -1, int recordReverse = -1);
    // A flipbook's line of anime_cfg.ini: how many pictures, how long each shows, and the sound and volume a line can
    // name after them.
    struct FlipbookInfo {
        int count = 0, ms = 0;
        std::string sound;
        int volume = 100;
    };
    std::map<std::string, FlipbookInfo> flipbooks_;
    void applyFlipTime(Sprite &s);
    // GOOD Timing: a scene from opcode 153 to the marker at opcode 154, where Ejaculate jumps past the marker by way of a
    // short script, and Extend holds the story at the marker and plays the config's groups of scripts until Ejaculate.
    struct GoodTiming {
        bool active = false, extending = false, ejaculating = false;
        std::shared_ptr<Script> main;
        int marker = 0, count = 0, group = -1, index = 0, played = 0, shownCount = 0;
        std::string config;
        std::vector<int> texts;  // the text nodes before the marker
        std::vector<std::string> extension, ejaculate;
        std::vector<std::string> groups[3];
    } gt_;
    // change_texture_list.ini's pairs: with Config's art switch off, the second picture of a pair shows as the first.
    std::map<std::string, std::string> plainArt_;
    // Ore o Hoshigaru Tsuma to Gibo's CG groups, from the same file: whether each has a see-through protagonist and a
    // version without pubic hair, which Config's and the CG viewer's switches pick.
    std::map<std::string, std::pair<bool, bool>> artVersions_;
    std::string shownPath(const std::string &path) const;
    bool artSwitchable() const;
    std::string versionPath(const std::string &path) const;
    // Test Game 93's normal CG art: a CG variant numbered in the hundreds, the detailed one a script names, shown as its
    // base, the same number less its hundreds, when Config asks for normal art and that file exists.
    std::string plainVariant(const std::string &path) const;
    void applyArtSwitch();
    // Splash effects: droplets a burst throws out, at a slot's place in the draw order. Their motion is a guess from
    // the commands' numbers and the look of the PC game.
    struct Droplet { float x, y, vx, vy, scale; int bornMs, lifeMs; };
    // Command 200's emitter, with its picture's name and the numbers a save keeps, in the save's order.
    struct Splash { ImageRef image; std::string path; std::array<int, 12> saved{}; int perBurstScale = 1000, gravity = 0, minLife = 500, maxLife = 1000, speed = 300, cap = 100; std::vector<Droplet> droplets; };
    std::map<int, Splash> splashes_;
    void splashBurst(const Node &n);
    void composeSplashes(Canvas &out, int fromSlot, int toSlot);
    bool readGtConfig(const std::string &path);
    void gtMeasure(const Script &script, int marker);
    void gtBegin(const Node &n);
    void gtReachMarker(const Node &n);
    void gtEjaculate();
    bool gtNextScript();
    bool gtBackToStory();
    void composeGtCount(Canvas &out);
    size_t configStringLength() const;
    int nextTrack(int direction);
    void playTrack(int index);
    void stopTrack();
    void holdMusic();
    int currentTrack_ = -1;
    bool trackPaused_ = false, trackRepeat_ = false, trackShuffle_ = false;
    void composeGallery(Canvas &out);
    int galleryCellAt(int x, int y) const;
    void openGalleryCell(int index);
    struct GalleryEntry {
        int page = 0;
        std::string thumbnail, flow, bgm;
        // A scene's scripts, any one of which read to its end unlocks it.
        std::vector<std::string> keys;
        std::vector<std::string> images;
        std::vector<int> imageIds;
        // Each picture's group, the first field of its line, and its place among its group's lines in the file, for a
        // game that keeps its flags by group (`cgflagsbygroup`).
        std::vector<int> imageGroups, imagePlaces;
    };
    // Where a picture's unlock flag sits in cfg.sav: by its number, or with `cgflagsbygroup STRIDE` under `screen defaults`
    // STRIDE bytes a group and 4 a place, as Test Game 83 keeps them (set 3). -1 for none.
    int cgFlagAt(const GalleryEntry &e, size_t picture) const;
    size_t configLayout(const char *key, size_t otherwise) const;
    bool sceneUnlocked(const GalleryEntry &scene) const;
    // Test Game 92's anime gallery, from its anime_view.ini: entries of movies, unlocked whole by a scene's script as a
    // scene is, and shown in the CG viewer.
    std::vector<GalleryEntry> cgEntries_, sceneEntries_, animeEntries_;
    // The entries of the gallery on screen, and whether one of an entry's pictures is unlocked.
    const std::vector<GalleryEntry> &galleryEntries() const;
    int cellShift() const;
    int clampSetting(const std::string &name, int value) const;
    // The host window, for Config's screen mode: whether it can go full screen, whether it is, and a change asked for,
    // -1 for none.
    bool canFullScreen_ = false, fullScreen_ = false;
    int windowRequest_ = -1;
    // Whether a slot, or a picture, is an event CG, which Config's camera work moves.
    bool isCgPath(const std::string &path) const;
    static std::string withoutPatch(const std::string &key);
    static std::string readKey(const std::string &name);
    bool isCgSlot(int slot) const;
    void composeDrift(Canvas &out);
    int64_t driftFrom_ = 0;
    size_t driftFirst_ = 0;
    ImageRef galleryThumbnail(const std::string &path) const;
    bool galleryImageSeen(const GalleryEntry &entry, size_t image) const;
    bool cgPictureSeen(const GalleryEntry &entry, size_t image) const;
    bool isPictureGallery() const { return screen_ && (screen_->name == "cg" || screen_->name == "anime"); }
    std::vector<std::string> tracks_, trackTitles_;
    // Test Game 88's sprite gallery, from cfg\sc_view.ini: its backgrounds, and each heroine's faces, outfits and poses as
    // the thumbnails the pickers show. A heroine's sprites are sc\sc_HHH_OOO_PPP_SSS_FFF, HHH her number in the file names.
    struct SpriteOutfit {
        std::string thumbnail;
        int id = 0;    // the OOO of her sprites in this outfit
        int bonus = 0;  // the line's second field: 0, or which bonus it comes with
    };
    struct SpriteHeroine {
        int number = 0;
        std::vector<std::string> faces, poses;
        std::vector<SpriteOutfit> outfits;
    };
    std::vector<std::pair<std::string, int>> spriteBackgrounds_;
    std::vector<SpriteHeroine> spriteHeroines_;
    // Each heroine's chosen outfit, as its place in her list, and whether the story wears it, kept in cfg.sav as the PC does.
    std::vector<int> spriteOutfit_, spriteLocked_;
    std::string lockedSprite(const std::string &path) const;
    std::string switchedOutfit(const std::string &path) const;
    std::string outfitSetting_;
    std::map<int, std::vector<int>> outfitSwitch_;
    // The sprite gallery's screen (sprites.cpp): what's shown, and its music.
    struct SpriteGallery {
        bool explaining = true, music = true, blur = false, pinned = false, panelsHidden = false;
        int heroine = 0, background = 0, zoom = 100, list = -1, listScroll = 0;
        std::vector<int> face, pose;
        int64_t restingSince = 0, slideSince = -1000000;
    } sprite_;
    int spriteMusic_ = 0;
    bool spriteScreen() const;
    void openSprites();
    void closeSprites();
    void playSpriteMusic();
    bool spriteAvailable(const std::string &thumbnail) const;
    std::vector<int> spriteChoices(int picker) const;
    int &spriteChoice(int picker);
    int spriteChoice(int picker) const { return const_cast<Engine *>(this)->spriteChoice(picker); }
    void stepSpriteChoice(int picker, int direction);
    std::string spriteThumbnail(int picker, int index) const;
    std::string spriteFigure() const;
    std::string spriteBackground() const;
    bool spritePanelsShown() const;
    float spriteSlide() const;
    std::pair<std::string, int> spriteControlAt(int x, int y) const;
    void pressSprites(int x, int y);
    void stepSprites();
    void spritePointerMoved();
    void composeSprites(Canvas &out);
    void composeSpriteList(Canvas &out);
    void pressSpriteList(int x, int y);
    struct SpriteListEntry {
        int index = 0;      // its place in sc_view.ini's list
        bool shown = true;  // false for a bonus the copy has only the name of
    };
    std::vector<SpriteListEntry> spriteListEntries(int picker) const;
    std::string spriteListFigure(int picker, int index) const;
    std::pair<int, int> spriteListPlace(int position) const;
    int spriteListRows() const;
    int spriteListAt(int x, int y, bool *onCheck = nullptr) const;
    // The figures a list shows tinted, by file and state, as last drawn.
    std::map<std::string, ImageRef> spriteTinted_;
    // Whether each bonus patch folder the lists name is there.
    mutable std::map<std::string, bool> spritePatches_;
    std::unordered_map<std::string, bool> seen_;
    std::unordered_map<std::string, std::string> readNames_;  // a finished script as the story named it, by its seen_ key
    // The text read, by script and node id, for Config's read-only skip.
    std::unordered_map<std::string, std::set<int>> readNodes_;
    bool readChanged_ = false, skipStopped_ = false, jumping_ = false, textFresh_ = false;
    void loadReadText();
    void saveReadText();
    void noteTextRead(const Node &n);
    void writeReadSav();
    std::vector<std::string> readScripts_;  // read.sav's scripts, as the PC named them
    bool seenChanged_ = false;
    std::vector<std::string> viewing_;
    size_t viewingIndex_ = 0;
    bool viewingImage_ = false;
    // The CG viewer's panel, from the cg screen's `viewer...` lines (viewer.cpp): the entry and picture shown, counted among
    // all of the entry's pictures, the panel hidden, the slideshow, zoom in hundredths, the entry's music and the art shown.
    size_t viewEntry_ = 0, viewImage_ = 0;
    bool viewPanelHidden_ = false, viewAuto_ = false, viewMuted_ = false, replayFromViewer_ = false;
    int64_t viewPanelSeenAt_ = 0;
    void viewerPointerMoved(int y);
    // The Scene screen's tab and page as a replay began, for `replaykeeps`.
    int replayTab_ = 0, replayPage_ = -1;
    int64_t viewMs_ = 0, viewAutoFrom_ = 0;
    int viewZoom_ = 100, viewMusic_ = 0;
    int movieZoom_ = 100;  // the manual animation control's zoom, in hundredths
    std::string viewMusicPath_;
    // The anime gallery's movie in the viewer, looping from viewMovieFrom_ on the viewer's clock, and whether the mix
    // reads its sound.
    std::shared_ptr<Movie> viewMovie_;
    int64_t viewMovieFrom_ = 0;
    ImageRef viewMovieImage_;
    bool viewMovieSound_ = false;
    std::string viewerGallery_ = "cg";  // the gallery the viewer was opened from
    void stopViewerMovie();
    void stopViewerMovieSound();
    bool viewerPanel() const;
    void openViewer(size_t entry);
    void viewerShow(size_t entry, size_t image);
    void viewerMusic();
    void stopViewerMusic();
    void viewerVariant(int direction);
    void viewerCg(int direction);
    void viewerAdvance();
    void viewerZoom(int notches);
    void closeViewer();
    void viewerTab(int page);
    const GalleryEntry *viewerScene() const;
    void composeViewerPicture(Canvas &out);
    void composeViewer(Canvas &out);
    int viewerButtonRow(const std::vector<std::string> &line, bool over) const;
    const std::vector<std::string> *viewerButtonAt(int x, int y) const;
    void pressViewer(int x, int y);
    void dragViewerSlider(int x, int y);
    std::vector<std::pair<int, int>> viewerSpots() const;
    bool viewerDragging_ = false;
    void stepViewer(int ms);
    // False when the scene has no flow to play.
    bool startReplay(const GalleryEntry &scene);
    bool replaying_ = false;
    int trackPlayer_ = 0;

    // Settings, by the names the menu tables use, kept in a small file of VNDegen's beside the saves.
    void loadSettings();
    void saveSettings();
    void applySettings();
    void applyArtSet();
    std::map<std::string, int> startingSettings() const;
    int galleryTab_ = 0;
    int64_t galleryTabAt_ = 0;  // the menu clock when the gallery's tab opened
    // A cell's entry, or -1. A game with tabs lists only the tab's entries.
    int galleryEntry(int cell) const;
    int setting(const std::string &name) const;
    void changeSetting(const std::string &name, int value);
    std::map<std::string, int> settings_, settingsWhenOpened_;
    // voice_type.ini: how a voice file's name starts, lowercase, and the type of who speaks it, the longest starts first.
    std::vector<std::pair<std::string, int>> voiceTypes_;
    void composeSettingsWidgets(Canvas &out);
    bool pressSettingsWidget(int x, int y, bool dragging);
    bool leftHeld_ = false;
    int autoDelayMs_ = 0;
    bool windowAlwaysClear_ = false;

    std::vector<int> globalMacros_, scenarioMacros_;
    std::string storyFlowKey_ = "FirstFlow";

    // Global variables, in the PC's cfg.sav.
    void loadGlobals();
    void loadReadScripts();
    void saveGlobals();
    std::vector<uint8_t> configFile_;

    // Menus.
    void openScreen(const std::string &name);
    void closeScreen();
    void runAction(const std::string &action);
    void startStory();
    void composeScreen(Canvas &out);
    void composeMessageButtons(Canvas &out);
    bool innerButtonsShown() const;
    bool underButtonsShown() const;
    void openLog();
    void closeLog();
    void scrollLog(int lines);
    void composeLog(Canvas &out);
    Button *buttonAt(int x, int y, std::vector<Button> &buttons);
    // `menustages`: Test Game 94's title shows its art until a click brings up the menu, stage 2, and the right button
    // puts it away. A `submenu:NAME` button opens the buttons and pictures of that group beside it, and a group named
    // `PARENT/NAME` opens inside PARENT's.
    int menuStage_ = 2;
    std::string subMenu_;
    bool submenuOpen(const std::string &name) const;
    bool dimmedBySubmenu(const Button &b) const;
    int questionCell(int question) const;
    int64_t barMovedAt_ = -1000000;
    bool barHover() const;
    bool barHovered() const;
    // When `barhover`'s rows last came up, and how much of them shows as they fade in and out.
    int64_t barShownAt_ = -1000000;
    float barOpacity() const;
    // `submenuopen MS`: a submenu's buttons grow from their left edge as it opens and shrink back as it closes.
    std::string shownSubMenu_, squeezeGroup_;
    int64_t squeezeAt_ = -1000000;
    bool squeezeOpening_ = false;
    float squeezeOf(const Button &b) const;
    float squeezeProgress() const;
    // What the last frame drew, so a fade or squeeze gets the frame that finishes it.
    float squeezeDrawn_ = 1, barDrawnOpacity_ = 1;
    void trackSubmenu();
    Canvas barLayer_;
    void playVoiceAgain(const std::string &voice);
    bool logVoiceButton(int row, int &x, int &y, int &w, int &h, ImageRef &sheet, int &col) const;
    int autoPerCharMs_ = -1;
    int autoBaseMs_ = 0;
    // `autowithvoice`: auto's pause runs alongside the voice, not after it.
    bool autoWithVoice_ = false;
    // `inwindow` under `screen log`: the backlog shown a line at a time in the message window, as Test Game 83's (set 3).
    // windowLog_ is how many lines back is showing, 0 for the line in play.
    int windowLog_ = 0;
    bool logInWindow() const;
    void stepWindowLog(int lines);
    int plateHiddenX_ = 0;
    bool plateRests_ = false, plateRaised_ = false;
    int charFadeMs_ = 160;  // each character's fade in, `textfade` or 160  // auto's pause for each character of the line, from `autocurve`, or -1 for a fixed pause
    // The setting a wait command scales by, Config's wait time unless `waitscale SETTING` names another that scales only
    // waits, not fades, as Test Game 82's auto wait does (set 2, timed under Wine).
    std::string waitScale() const;
    bool anyGlobalSet(const std::vector<int> &numbers) const;
    bool shown(const Button &b) const;
    bool shown(const Picture &p) const;
    // Test Game 94's route buttons replay the story from the prologue with each choice made for that route, 1 to 4, which
    // its choices name in parameters 10 to 13. The route is a scenario variable, `routevar N` in the defaults table (94's
    // SENARIO_STATUS, 4000), set as the story starts: the PC's saves carry it, and a load goes on in the route.
    int pendingRoute_ = 0, routeVar_ = -1;
    // The older layout's slot for each place of a standing picture, each place's middle from the screen's, and the
    // background's and the CG's slots, as Test Game 83 has them.
    std::vector<int> layoutSlots_{200, 400, 300, 600, 500, 700}, layoutX_{0, -272, 272, -400, 400, 0};
    bool layoutPairs_ = false;
    void clearLayoutSlot(int slot);
    struct BulbStep {
        int frames = 0, level = -1;
    };
    std::vector<std::vector<BulbStep>> bulbPatterns_;
    int bulbPattern_ = -1, bulbFrom_ = 255, bulbDrawn_ = 255;
    bool bulbSeen_ = false;
    std::map<std::pair<const Picture *, int>, std::pair<ImageRef, ImageRef>> darkened_;
    ImageRef darkened(const Picture &p, const std::vector<const Picture *> &frames, int dark);
    int64_t bulbStart_ = 0;
    std::map<std::string, std::string> bulbDark_;
    void loadBulb();
    int bulbLevel();
    std::string bulbDark(const std::string &shown, const Image &lit);
    bool layoutSlotByP1_ = false;
    int layoutBgSlot_ = 100, layoutCgSlot_ = 100;
    int route() const;

    // Commands.
    void beginChoice(const Node &n);
    void updateChoice();
    void choicePlace(int &x, int &top, int &step, int &slant) const;
    static int variableIndex(const Node &n);
    int operand(const Node &n) const;
    int variableValue(int scope, int index) const;
    int variableNumber(int scope, int index) const;
    void endStory(const std::string &screen);
    void loadImage(const Node &n);
    void transform(const Node &n);
    void transformOverTime(const Node &n);
    void changeOverTime(Sprite &s, const Node &n);
    void startNamedAnimation(const Node &n);
    void namedAnimation(Sprite &s, const Node &n);
    Animation parseAnimation(const std::string &text, const Sprite &s);
    void stopNamedAnimation(int slot);
    void abortChange(int slot);
    void screenUpdate(const Node &n);
    void showText(const Node &n);
    void playSound(const Node &n, int kind);
    void stopChannel(int kind, int channel, int ms);
    void stopKind(int kind, int ms);
    std::map<int, int> &channelsOf(int kind) { return kind == 0 ? bgm_ : kind == 1 ? se_ : voice_; }
    void changeVolume(int kind, int channel, int volume, int ms);

    // Drawing.
    SpriteView view(const Sprite &s) const;
    void foldFinishedChanges(Sprite &s);
    void composeScene(Canvas &out, const Sprites &sprites);
    void composeMessage(Canvas &out);
    std::string readSwapped(const std::string &sheet) const;
    bool hoverAllows(const Picture &p) const;
    bool musicLit(const Button &b) const;
    // Test Game 92's manual animation control: while a movie shows and Config allows it, the wheel and a panel zoom the
    // movie, and the zoomed movie follows the pointer. The table's `animpanel`, `animbuttons`, `animdigits`, `animhelp`
    // and `animzoom` place it.
    bool movieShowing() const;
    bool cameraActive() const;
    bool cameraPanelShown() const;
    int cameraButtonAt(int x, int y) const;
    void cameraZoom(int hundredths);
    void composeCameraPanel(Canvas &out);
    void markRect(int &x, int &y, int &w, int &h) const;
    bool markShowing() const;
    void drawMark(Canvas &out);
    void compose();
    void layoutText(const std::string &utf8Name, const std::string &utf8Text);
    void layoutRun(const std::string &text, int x, int y, bool endsThere, std::vector<Glyph> &out);
    bool twoByteAdvance_ = false;
    // init.ini's font types: 1 a shadow, 2 a thin frame, 9 both, 13 a wide frame, 17 a wider one.
    int frameWidth() const { return fontType_ == 17 ? 3 : fontType_ == 13 ? 2 : (fontType_ == 2 || fontType_ == 9) ? 1 : 0; }
    bool hasShadow() const { return fontType_ == 1 || fontType_ == 9; }
    void drawGlyph(Canvas &out, const Glyph &g, int pass, int alpha, int r, int gr, int b, int fr, int fg, int fb);
    void drawGlyphs(Canvas &out, const std::vector<Glyph> &glyphs, int r, int g, int b, int fr, int fg, int fb);
    void drawGlyphRun(Canvas &out);
    bool anythingMoves() const;

    Files files_;
    std::unique_ptr<Images> images_;
    // Game parts the table turns on (parts.h), the one whose screen is open, and where its result goes.
    std::vector<std::pair<std::string, std::unique_ptr<GamePart>>> parts_;
    GamePart *partScreen_ = nullptr;
    int partResultScope_ = -1, partResultNumber_ = 0;
    std::unique_ptr<PartHost> partHost_;
    void loadParts();
    // The older layout's commands, 10 to 14 and 21 (oldlayout.cpp).
    void loadOldLayout();
    void runOldLayout(const Node &n);
    void resetParts();
    bool partCommand(const Node &n);
    GamePart *menuPart() const;
    bool loadPartImage(int slot, const std::string &path, int x, int y, bool additive, float scrollX, float scrollY, int kind);
    void holdPartSlot(int slot, int kind);
    void releasePartSlot(int slot);
    bool partSlotHeld(int slot) const;
    bool replayOff(const std::string &action) const;
    void drawParts(Canvas &out, Layer layer, int fromSlot = 0, int toSlot = 0);
    void finishPartScreen(int result);
    // VNDegen's own lines beside a save, read only when they were written with it: game parts' state, and in a story
    // table's game the exact place in the story, the last choice's or screen's result, and the part screen open.
    struct Sidecar {
        bool valid = false;
        std::map<int, FlipModes> flips;
        std::map<std::string, std::vector<std::string>> parts;
        std::vector<std::pair<std::string, int>> story;  // each flow frame's script and node, bottom first
        bool hasStoryTable = false;
        uint32_t storyTable = 0;
        bool hasFlowNumbered = false, flowNumbered = true;
        bool hasResult = false;
        int result = 0;
        std::string screen;
        int screenScope = -1, screenNumber = 0;
        bool atChoice = false;
        // `slotfx SLOT ADDITIVE SX SY`: a slot's additive drawing and scrolling, which the PC's record doesn't hold.
        std::map<int, std::array<float, 3>> slotFx;
        // `var NUMBER VALUE`: VNDegen's own story variables, past those the PC's save has room for.
        std::map<int, int> variables;
    };
    static uint32_t saveFingerprint(const SaveData &d);
    uint32_t storyTableHash() const;
    bool storyPlaceNumbered(const SaveData &d);
    Sidecar readSidecar(const SaveData &d, int slot);
    Sidecar parseSidecar(const SaveData &d, const std::string &text);
    void restoreSave(const SaveData &data, const Sidecar &side);
    std::map<int, FlipModes> flipModesAt(const SaveData &d, const Sidecar &side);
    void loadPartsState(const SaveData &d, const Sidecar &side);
    bool restoreStoryPlace(const Sidecar &side, bool &atChoice);
    // The story table (story.cpp): a game without flow scripts has its story order as `flow` blocks under `screen
    // story`, compiled to flow nodes. Flows are scripts named `story:NAME`; `chapter N` plays `chapter:N`.
    std::shared_ptr<Script> compileStory(const std::string &flow);
    // `complains`: false for a search through the story's chapters, where one no part plays yet is no fault.
    std::shared_ptr<Script> compileChapter(int number, bool complains = true);
    std::string firstStoryFlow() const;
    bool resumeStoryAt(const std::string &scene);
    bool storyFlowExists(const std::string &name) const;
    bool resumeStoryAtNumber(int number, int active, const std::string &scene, bool &atChoice, bool &atScreen);
    bool storySceneIs(const Node &n, const std::string &scene);
    bool resumeStoryByRule(bool &atScreen);
    // A flow condition's comparison, as Test Game 94's exe orders them: ==, !=, <=, >=, <, >.
    bool storyConditionHolds(const Node &n);
    // Opcodes the story table needs that flow scripts don't have. Flow scripts' own stop well below these.
    static constexpr int kStoryGoto = 1001, kStoryOpen = 1002, kStoryChapter = 1003, kStoryResume = 1004;
    // The parameter of a story table's `scene`, `choice` and `open` nodes that holds the PC's number for the place.
    static constexpr int kStoryPcNumber = 31;
    // A story table's choice's columns of its picture: how many at this parameter, then the columns.
    static constexpr int kChoiceCells = 14;
    static constexpr size_t kChoiceCellsMax = 5;
    Config config_;
    Audio audio_;
    FILE *log_ = nullptr;
    std::string problem_;
    std::string status_;
    std::unordered_map<std::string, bool> warned_;

    int width_ = 1280, height_ = 720, fps_ = 60;
    Canvas canvas_, from_, previous_;
    // The 4:3 frame of a game whose init.ini names a MsgWinSquare picture, as Test Game 88's PC draws it in a 1280x960
    // window: the 16:9 frame on top, the picture under it, and the message window, text and buttons moved down onto
    // it. square_ is whether it's on, from Config's `screensize` or, set to automatic, the host's view.
    ImageRef squareImage_;
    int squareY_ = 720, squareShift_ = 240;
    bool square_ = false, messageInStrip_ = false;
    float viewAspect_ = 0;
    Canvas frame_, strip_;
    bool wantsSquare() const;
    void composeStrip();
    // The pointer's y as the message window's layer has it, which the 4:3 frame moves down.
    int messagePointerY() const { return pointerY_ - (square_ && !partPlate() ? squareShift_ : 0); }
    bool dirty_ = true, frameChanged_ = false, markSaved_ = false;
    std::vector<uint8_t> underMark_;
    int64_t now_ = 0;
    bool paused_ = false, skip_ = false;

    // The flow script running scenes, and the scene it called.
    std::vector<Frame> stack_;
    Frame scene_;
    std::unordered_map<std::string, std::shared_ptr<Script>> scripts_;
    Waiting waiting_ = Waiting::None;
    int64_t waitUntil_ = 0;
    bool clicked_ = false;
    int textCount_ = 0;

    // Commands change the back set; a screen update shows it.
    Sprites back_, front_;
    // The patch holding a second set of CGs and sprites, such as `sys_5`, empty when the game has none, and the set showing.
    std::string artSet_, artShown_;
    ImageRef ruleImage_;
    int transitionType_ = 0, transitionMs_ = 0;
    int64_t transitionStart_ = 0;
    bool transitioning_ = false;

    // The story's variables: global ones (saved with the settings) and the scenario's (saved with a save).
    std::map<int, int> globals_, scenario_;

    // A choice: a sheet with one column per option and one row per state (normal, hovered, chosen, ...).
    ImageRef choiceSheet_, choiceBack_;
    int choiceCount_ = 0, choiceHover_ = -1, choiceResult_ = 0;
    // Each option's column of the choice's picture, when the story table names them, and the box closing after a press
    // (`close MS SCALE` under `screen choice`): when it began, or -1.
    std::vector<int> choiceCells_;
    int64_t choiceClosingFrom_ = -1;
    int choiceColumns() const;
    void composeChoice(Canvas &out);
    std::array<int, 4> choiceRoutes_{-1, -1, -1, -1};  // the option each of Test Game 94's routes takes, or another value
    const std::vector<std::string> *choiceTable(const std::string &key) const;
    float mouseX_ = 0, mouseY_ = 0;
    bool mouseMoved_ = false;

    // Effects.
    int shakeType_ = 0, shakeLock_ = 1 << 30;
    int64_t shakeStart_ = 0, shakeStopAt_ = -1;
    int shakeStopMs_ = 0;
    // A shake that goes on until it's stopped: the amplitudes in pixels and periods in ms each way.
    bool shakeSustained_ = false;
    float shakeAmpX_ = 0, shakeAmpY_ = 0, shakePeriodX_ = 1, shakePeriodY_ = 1;
    int flashMs_ = 0, flashR_ = 0, flashG_ = 0, flashB_ = 0;
    // The film noise as drawn: the crop of its frame for one step of its random places, kept until the step changes.
    void drawNoise(Canvas &out, const Sprite &s);
    ImageRef noiseCrop_;
    int64_t noiseCropStep_ = -1;
    int64_t flashStart_ = 0;
    bool blur_ = false, blurEnding_ = false, previousValid_ = false;
    int blurAmount_ = 0, blurEndMs_ = 0;
    int64_t blurEndStart_ = 0;

    // Saves. `thumbnail_` is the last story frame, taken when a menu opens over it.
    const SaveLayout *layout_ = nullptr;
    std::string saveDir_;
    std::map<int, SlotInfo> slots_;
    std::vector<uint8_t> thumbnail_;
    int thumbW_ = 160, thumbH_ = 90;
    int page_ = 0;
    std::string returnTo_;
    int flowCallId_ = 0;
    bool flowCallNumbered_ = false;
    std::string choiceImage_;
    // The frame behind the log with `keepwindow`: 1 for the window's picture alone, 2 with its buttons and the parts' layer.
    int windowArtOnly_ = 0;
    bool choiceMade_ = false;  // a choice has been made this game, as Test Game 87's saves hold it
    std::vector<SavedText> backlog_;
    SavedText currentText_;
    struct Playing {
        std::string path;
        int volume = 0, pan = 0;
    };
    std::map<int, Playing> bgmNow_, seNow_;
    bool dialogOpen_ = false;
    std::string dialogOpener_;  // the action of the menu button whose question is open
    int dialogQuestion_ = 0, dialogHover_ = -1;
    std::function<void()> dialogYes_;
    int testPlayer_ = 0;  // Config's test sound
    bool dialogNoAsk_ = false;  // the "don't ask again" box, ticked
    void answerDialog(bool yes);

    // Menus, and the message window's buttons.
    std::map<std::string, Screen> screens_;
    Screen *screen_ = nullptr;
    std::string folder_;
    MessageButtons messageButtons_;
    // A replay from the gallery has its own bar, msg_button_playview.ini, whose Exit leaves the replay.
    MessageButtons replayButtons_;
    bool hasReplayButtons_ = false;
    MessageButtons &bar() { return replaying_ && hasReplayButtons_ ? replayButtons_ : messageButtons_; }
    ImageRef buttonPlate_;
    int plateX_ = 0, plateY_ = 0, plateHiddenY_ = 0;
    int pointerX_ = 0, pointerY_ = 0;
    bool pointerSeen_ = false;
    std::string pressed_;
    const Node *prefetchedFrom_ = nullptr;
    bool innerWereShown_ = false, underWereShown_ = false;
    bool autoMode_ = false, skipToggle_ = false, windowHiddenByPlayer_ = false;
    std::string titleBgm_;
    int titleMusic_ = 0;
    int barTop_ = 0;
    int64_t autoReadyAt_ = -1;
    bool logOpen_ = false;
    int logTop_ = 0;
    Canvas logBase_;
    int logTextR_ = 255, logTextG_ = 128, logTextB_ = 0, logFrameR_ = 0, logFrameG_ = 0, logFrameB_ = 0;

    // The message window.
    bool windowShown_ = false;
    int64_t windowShownAt_ = -100000;
    int64_t windowDrawnAt_ = 0;
    ImageRef windowImage_, waitMark_;
    std::vector<ImageRef> bars_;
    std::vector<int> barY_;
    int windowX_ = 0, windowY_ = 0, windowShowType_ = 0;
    // The Config screen's Dialogue Window Opacity. Test Game 95's PC default is 60 of 100.
    float windowOpacity_ = 0.6f;
    int nameX_ = 0, nameY_ = 0, textX_ = 0, textY_ = 0;
    int markX_ = 0, markY_ = 0, markPatterns_ = 1;
    float markSpeed_ = 1;
    int fontFace_ = 0, fontSize_ = 28, fontType_ = 0, lineInterval_ = 0, maxChars_ = 64;
    // How the PC places text, where it isn't what the font gives: a gap after each glyph's own advance, a line pitch, and
    // an offset from init.ini's positions. 0 keeps the font's own.
    float textGap_ = 0;
    int commentFace_ = 0;
    int textLine_ = 0, textDX_ = 0, textDY_ = 0;
    int textR_ = 255, textG_ = 255, textB_ = 255, shadowR_ = 0, shadowG_ = 0, shadowB_ = 0;
    std::vector<Glyph> nameGlyphs_, textGlyphs_;
    int charMs_ = 20;
    int64_t textStart_ = 0;
    bool textDone_ = false;
    int voicePlayer_ = 0;
    std::map<int, int> bgm_, se_, voice_;
    // The file each playing voice came from, so unchecking its speaker in Individual Voices silences it at once.
    std::map<int, std::string> voiceFiles_;
    void applyVoiceChoices();
};

}  // namespace neko
