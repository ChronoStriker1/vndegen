// The running game: the script interpreter, the sprites, the message window and the transitions.
//
// Command meanings come from the games' own data and the PC game under Wine; docs/nekosdk-cleanroom.md says which are
// settled and which are readings still to be checked.
#pragma once
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "archive.h"
#include "audio.h"
#include "canvas.h"
#include "image.h"
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
};

struct Sprite {
    ImageRef image;
    std::string path;
    float x = 0, y = 0, scaleX = 1, scaleY = 1, rotation = 0, alpha = 1;
    int blurAmount = 0;
    // The point in the picture a scale keeps in place, or -1 for its middle.
    int anchorX = -1, anchorY = -1;
    bool blurOn = false;
    // Timed changes run side by side until each finishes, when it folds into the values above. The named animation
    // runs over them all until it's stopped.
    std::vector<Animation> changes;
    Animation loop;
    bool looping = false;
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
    ~Engine();

    bool step(int ms);
    const uint8_t *frame(int &w, int &h, bool &changed);
    Audio &audio() { return audio_; }

    void mouseMove(float x, float y);
    // For test hosts: the choice on screen, or -1, and picking one.
    int choiceCount() const { return waiting_ == Waiting::Choice ? choiceCount_ : -1; }
    void choose(int index);
    void mouseButton(int button, bool down);
    void mouseWheel(int notches);
    // Moves the pointer to the nearest control that way, for a controller or arrow keys. False when there is none here.
    bool moveFocus(int dx, int dy);
    void setSkip(bool on) { skip_ = on; }
    bool inMenu() const { return screen_ != nullptr; }
    // For test hosts: presses a menu button by its action, as a click on it does.
    bool pressAction(const std::string &action);
    void setPaused(bool paused);

    const char *problem() const { return problem_.empty() ? nullptr : problem_.c_str(); }
    const char *debugStatus();
    bool waitingForClick() const { return waiting_ == Waiting::Click; }
    int textCount() const { return textCount_; }
    // A global (scope 0) or scenario variable (1) by its number, for tests.
    int variable(int scope, int number) const {
        const auto &table = scope == 1 ? scenario_ : globals_;
        auto it = table.find(number);
        return it == table.end() ? 0 : it->second;
    }
    bool jump(const std::string &script, int node);
    // Saving, loading and a card's lock, which tests also call.
    bool saveToSlot(int slot);
    bool loadFromSlot(int slot);
    void toggleLock(int slot);

private:
    enum class Waiting { None, Time, Click, Choice, Ended };
    struct Frame {
        std::shared_ptr<Script> script;
        const Node *node = nullptr;
    };

    void log(const char *format, ...) __attribute__((format(printf, 2, 3)));
    void warnOnce(const std::string &what);
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
    std::string savePath(int slot, const char *extension) const;
    void deleteSlot(int slot);
    bool cardAt(int slot, int &x, int &y) const;
    bool overCardDelete(int slot, int x, int y) const { return overCardPart("slotdelete", slot, x, y); }
    bool overCardPart(const char *key, int slot, int x, int y) const;
    int newestQuickSlot();
    // A locked save can be loaded but not saved over or deleted. The lock is kept in cfg.sav, as the PC keeps it.
    bool locked(int slot) const;
    std::string lastScene_;
    int nextQuickSlot_ = 81;
    bool barPinned_ = false;
    SaveData captureSave();
    void restoreSave(const SaveData &data);
    void noteThumbnail();
    struct SlotInfo {
        bool exists = false;
        int date[6] = {};
        ImageRef thumbnail;
        std::string comment;  // typed on a PC, UTF-8
    };
    SlotInfo &slotInfo(int slot);
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
        std::string thumbnail, flow, key, bgm;
        std::vector<std::string> images;
        std::vector<int> imageIds;
    };
    std::vector<GalleryEntry> cgEntries_, sceneEntries_;
    std::vector<std::string> tracks_;
    std::unordered_map<std::string, bool> seen_;
    bool seenChanged_ = false;
    std::vector<std::string> viewing_;
    size_t viewingIndex_ = 0;
    bool viewingImage_ = false;
    // The CG viewer's panel, from the cg screen's `viewer...` lines (viewer.cpp): the entry and picture shown, counted among
    // all of the entry's pictures, the panel hidden, the slideshow, zoom in hundredths, the entry's music and the art shown.
    size_t viewEntry_ = 0, viewImage_ = 0;
    bool viewPanelHidden_ = false, viewAuto_ = false, viewMuted_ = false, replayFromViewer_ = false;
    int64_t viewMs_ = 0, viewAutoFrom_ = 0;
    int viewZoom_ = 100, viewMusic_ = 0;
    std::string viewMusicPath_;
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
    // What Config starts at for this game: the shared defaults, and every Individual Voices box checked.
    std::map<std::string, int> startingSettings() const;
    // Whether the player's Individual Voices settings let this voice file play.
    bool voiceAllowed(const std::string &voice) const;
    void saveSettings();
    void applySettings();
    void applyArtSet();
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

    // Commands.
    void beginChoice(const Node &n);
    void updateChoice();
    void choicePlace(int &x, int &top, int &step) const;
    int variableValue(int scope, int index) const;
    int variableNumber(int scope, int index) const;
    void endStory(const std::string &screen);
    void loadImage(const Node &n);
    void transform(const Node &n);
    void transformOverTime(const Node &n);
    void takeScaleAnchor(Sprite &s, const Node &n);
    void anchorOf(const Sprite &s, int &x, int &y) const;
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
    void markRect(int &x, int &y, int &w, int &h) const;
    bool markShowing() const;
    void drawMark(Canvas &out);
    void compose();
    void layoutText(const std::string &utf8Name, const std::string &utf8Text);
    void layoutRun(const std::string &text, int x, int y, bool endsThere, std::vector<Glyph> &out);
    void drawGlyph(Canvas &out, const Glyph &g, int pass, int alpha, int r, int gr, int b, int fr, int fg, int fb);
    void drawGlyphs(Canvas &out, const std::vector<Glyph> &glyphs, int r, int g, int b, int fr, int fg, int fb);
    void drawGlyphRun(Canvas &out);
    bool anythingMoves() const;

    Files files_;
    std::unique_ptr<Images> images_;
    Config config_;
    Audio audio_;
    FILE *log_ = nullptr;
    std::string problem_;
    std::string status_;
    std::unordered_map<std::string, bool> warned_;

    int width_ = 1280, height_ = 720, fps_ = 60;
    Canvas canvas_, from_, previous_;
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
    std::string choiceImage_;
    std::vector<SavedText> backlog_;
    SavedText currentText_;
    struct Playing {
        std::string path;
        int volume = 0, pan = 0;
    };
    std::map<int, Playing> bgmNow_, seNow_;
    bool dialogOpen_ = false;
    int dialogQuestion_ = 0, dialogHover_ = -1;
    std::function<void()> dialogYes_;
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
