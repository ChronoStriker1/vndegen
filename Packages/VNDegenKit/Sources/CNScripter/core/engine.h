// VNDegen's NScripter interpreter. One Engine runs one game. The host calls step() once per frame with the elapsed
// time; the engine runs the script until it has to wait (for a click, a timer, an effect), then draws the frame.
//
// Display model, measured on the original engine: image commands change a pending scene. The screen shows the
// committed scene, which `print`, image commands given an effect, and text output bring up to date. `wait` and
// `click` leave pending changes hidden.
#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../include/vnns.h"
#include "audio.h"
#include "font.h"
#include "fs.h"
#include "gfx.h"
#include "ponsfont.h"
#include "script.h"

struct lua_State;

namespace vnns {

constexpr int kSpriteCount = 1000;
constexpr int kSprite2Count = 256;
// The PC Ponscripter engine's monocro color before a game sets one, as its saves show.
constexpr uint32_t kPonsMonocroDefault = 0xFF7AF688;  // red 136, green 246, blue 122, as rgba() packs them

// An image as a script names it, with its tag: ":a;file", ":c/3,100,0;file", ":s/20,20,0;#FFFFFFtext", ...
struct LoadedImage {
    std::string tag;     // the full string the script gave, for saves and the file log
    ImageRef image;      // all cells side by side
    int cells = 1;
    int cellWidth = 0;
    std::vector<int> cellTimes;  // milliseconds per cell; empty or 0 means no animation
    int loop = 0;                // 0 repeat, 1 stop at last, 2 back and forth, 3 none
    int layer = -1;              // "*n": the sprite shows plugin layer n instead of an image
};

struct Sprite {
    LoadedImage img;
    bool loaded = false;
    bool visible = false;
    int x = 0, y = 0;
    int alpha = -1;  // -1 when the script gave none, drawn opaque; saves keep the distinction
    int drawAlpha() const { return alpha < 0 ? 255 : alpha; }
    int cell = 0;
    int animDir = 1;
    int64_t animClock = 0;  // engine time of the last cell change
};

// lsp2 sprites: placed by their centre, scaled, rotated, and blended normally, additively or subtractively.
struct Sprite2 {
    LoadedImage img;
    bool loaded = false;
    bool visible = false;
    int cx = 0, cy = 0;
    int scaleX = 0, scaleY = 0, rot = 0;
    int alpha = -1;
    int cell = 0;
    Blend blend = Blend::Normal;
};

struct Scene {
    std::string bgTag = "black";
    LoadedImage bg;
    uint32_t bgColor = rgba(0, 0, 0);
    std::array<Sprite, 3> ld;  // left, centre, right
    std::array<Sprite, kSpriteCount> sp;
    std::array<Sprite2, kSprite2Count> sp2;
    bool allSpHidden = false, allSp2Hidden = false;
    bool monocro = false;
    uint32_t monocroColor = 0;  // kept while monocro is off, as saves show
    int nega = 0;
    struct Bar {
        bool on = false;
        int value = 0, x = 0, y = 0, w = 0, h = 0, max = 0;
        uint32_t color = 0;
    };
    std::array<Bar, 100> bars;
    struct Prnum {
        bool on = false;
        int value = 0, x = 0, y = 0, w = 0, h = 0;
        uint32_t color = 0;
    };
    std::array<Prnum, 100> prnums;
};

// Save fields whose meaning the lab didn't settle. A loaded PC save's values are kept and written back; a new game
// starts from the values the PC engine writes for a fresh one.
struct SaveExtras {
    int h12 = 0;
    std::string h32, h33;
    int h100[6] = {1, 1, 0, 0, 0, 0};
    int ldQ[3] = {0, 0, 0};
    std::string pStr1;
    int waveA = -1, waveB = 0, waveC = 0;
    int pInt4 = 1;
    int qInt1 = 0, qInt2 = 1, qInt3 = 0, qInt4 = 0, qInt5 = 0;
    int tail[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // [4] mirrors allsphide, [5] allsp2hide
    std::string tStr;
    // The PC Ponscripter format's own fields, of unknown meaning: always these in the saves measured.
    int pnsHead[3] = {1, 1, 0};
    int pnsWindow = 0;
    int pnsAllSp = 0;  // where NScripter keeps allsphide
    uint32_t pnsColor = 0xFFFFFFFF;  // where NScripter keeps the text color; textcolor doesn't change it
    int pnsTail[10] = {};
};

struct Effect {
    int type = 1;
    int duration = 0;
    std::string mask;
};

struct TextChar {
    std::string ch;  // one character, in the script's encoding
    int x, y;        // top-left, screen pixels
    uint32_t color;
    int w, h;
    bool ruby = false;
    std::string gaiji;  // NSFont gaiji: an image drawn in place of the character
    int link = -1;      // text button number
    // Ponscripter text, drawn at twice the screen's resolution: the face, and the pen's x and the baseline's y on that
    // doubled screen. face is -1 for NScripter's fixed cells.
    int face = -1, x2 = 0, y2 = 0;
    int px2 = 0;  // the em it was laid out at, on the doubled screen
    bool shadow = false;
};

struct Button {
    int number;
    enum Kind { Region, Sprite, TextLink } kind;
    int sprite = -1;
    int x = 0, y = 0, w = 0, h = 0;  // region buttons, and text links
    int ox = 0, oy = 0;              // btn: hover image's position in the btndef image
    std::string hoverCtrl;           // exbtn control string run while hovered
    // bsp buttons: control strings for not hovered, hovered and pressed; empty means cells 0, 1 and 2.
    bool bsp = false;
    std::string offCtrl, onCtrl, pressCtrl;
};

enum class WaitKind { None, Timer, Click, Text, Effect, Button, Select, Quake, Movie, Dialog };

// One of NScripter's built-in screens, open over the game.
struct SystemUi {
    enum Kind { Menu, YesNo, Lookback, HideWindow, TextInput } kind = Menu;
    std::string title, message;
    std::vector<std::string> items;  // Shift-JIS
    std::vector<bool> enabled;
    std::function<void(int)> onChoose;  // -1 when cancelled
    int hover = -1;
    bool keyboard = false;
    int page = 0;  // lookback
    std::string input;
    int maxLen = 64;
    // input's window and field size, in the units its arguments use (4/3 of a pixel); 0 for the PC's default.
    int dlgW = 0, dlgH = 0, fieldW = 0, fieldH = 0;
    int rowTop = 0, rowH = 0, boxX = 0, boxW = 0;
    std::vector<std::array<int, 4>> buttons;  // where the dialogs drew each item, x y w h, for the pointer
    // The wait the screen interrupted, restored when it closes.
    WaitKind prevWait = WaitKind::None;
    std::function<void()> prevDone;
    int64_t prevWaitUntil = 0;
};

class Engine {
public:
    struct Config {
        std::string gameRoot;
        std::string saveRoot;  // where saves go when the script names no savedir; normally gameRoot
        bool englishWordWrap = false;
    };

    explicit Engine(Config cfg);
    ~Engine();

    bool start(std::string *error);
    // Advances the engine clock by ms and runs the script. Returns false once the game has ended.
    bool step(int ms);
    const Image &frame() const { return frame_; }
    bool frameChanged() const { return frameChanged_; }

    // Input. Coordinates are in game pixels.
    void mouseMove(int x, int y);
    void mouseButton(int button, bool down);  // 1 left, 2 middle, 3 right
    void wheel(int notches);                  // positive away from the player
    void key(int code, bool down);            // VNNS_KEY_* codes
    void typeText(const std::string &utf8);

    void requestQuit() { quitRequested_ = true; }
    void setPaused(bool p) {
        paused_ = p;
        audio_.setPaused(p);
    }
    void saveSystemData();
    // A load from outside the script: runs the define block first if the game hasn't left it yet.
    bool loadSlot(int slot);
    AudioMixer &audio() { return audio_; }
    const std::string &problem() const { return problem_; }
    // Ends the game with a problem the runtime can't go on from, such as a script that asks for more memory than exists.
    void fail(const std::string &why) {
        problem_ = why;
        ended_ = true;
    }
    int width() const { return width_; }
    int height() const { return height_; }
    std::string caption() const { return caption_; }
    std::string debugStatus() const;
    // The window menu bar the game builds with insertmenu, which the runtime never draws, as vnns_menu describes it.
    std::string windowMenuJson() const;
    // Runs the menu bar item at index as the PC engine does when the player picks it. False when no item there runs.
    bool chooseWindowMenuItem(size_t index);
    const Script &script() const { return script_; }

    // ---- everything below is the interpreter's own state and helpers, shared by the command files ----

    // Script cursor and parsing (interp.cpp)
    struct Cursor {
        size_t pos = 0;
        int line = 0;
        int stmt = 0;  // statement index within the line, as saves record it
    };
    Cursor cur_;
    bool defineMode_ = true;
    void jumpToLine(int line);
    bool jumpToLabel(std::string_view label);
    void skipSpaces(bool wide = true);  // wide: full-width spaces too
    char peek() const;
    bool atStatementEnd() const;
    bool readComma();  // consumes a ',' if present
    std::string readWord();
    std::string readLabel();
    int readInt();
    std::string readStr();
    std::string readColorOrStr();
    bool peekIsStringArg();
    // A variable reference for commands that store results.
    struct VarRef {
        enum Kind { Num, Str, Array, None } kind = None;
        int index = 0;
        std::vector<int> subs;
    };
    VarRef readVar();
    void setVar(const VarRef &v, int value);
    void setVar(const VarRef &v, const std::string &value);
    int getNumVar(int index) const;
    void setNumVar(int index, int value);
    std::string &strVar(int index);
    int *arrayElement(int index, const std::vector<int> &subs);
    bool readCondition();
    void skipToLineEnd();
    void skipStatement();
    void runUntilBlocked();
    void executeStatement();
    void executeCommand(const std::string &name);
    int jumps_ = 0;               // counts cursor jumps, so a command that jumped isn't checked for leftovers
    bool continueLine_ = false;   // if/notif leave the cursor on the next command of the same line
    void gosub(size_t returnPos, int line, int stmt, std::string_view label);
    // `return [*label]`: leaves the innermost gosub, defsub or textgosub frame. Returns false with no frame to leave.
    bool returnFromSub(const std::string &label);
    void errorf(const char *fmt, ...) __attribute__((format(printf, 2, 3)));
    int quietErrors_ = 0;  // while above 0, errorf logs nothing

    // Ponscripter's text style. ~cN~ picks face N, plain; ~i~ and ~b~ toggle italic and bold, which add 1 and 2 to it. A
    // style lasts from line to line until a tag changes it.
    struct PonsStyle {
        int base = 0;
        bool italic = false, bold = false;
        int face() const { return base + (italic ? 1 : 0) + (bold ? 2 : 0); }
    };
    struct Frame {
        enum Kind { Gosub, For, TextGosub } kind = Gosub;
        size_t pos = 0;  // return position, or the loop body's start
        int line = 0, stmt = 0;
        int var = 0, to = 0, step = 1;  // for loops
        // A defsub call returns to the arguments it hasn't read yet, as the PC engine does (and saves): getparam reads
        // them from pos and moves it on, and return skips any left.
        bool hasParams = false;
        bool paramsRead = false;  // getparam has read some of them
        // textgosub calls resume text output afterwards
        bool resumeText = false;
        // Ponscripter: the text line's caret, language and style, which the routine's own text may change.
        bool caret = false;
        int textLang = -1;
        PonsStyle style;
    };
    std::vector<Frame> stack_;

    // Variables
    std::vector<int> num_;
    std::vector<std::string> str_;
    struct ArrayVar {
        std::vector<int> dims;
        std::vector<int> data;
    };
    std::map<int, ArrayVar> arrays_;
    std::unordered_map<std::string, int> numAlias_;
    std::unordered_map<std::string, std::string> strAlias_;
    std::unordered_set<std::string> defsubs_;
    std::map<int, std::pair<int, int>> intLimits_;
    int globalBorder_ = 200;

    // Scene and display (scene.cpp)
    Scene scene_;          // pending
    Scene shown_;          // committed
    Image frame_;          // what the host shows
    bool frameChanged_ = true;
    bool sceneDirty_ = true;
    int width_ = 640, height_ = 480;
    int humanz_ = 499;
    std::string humanOrder_ = "lcr";
    bool windowBack_ = false;
    int underline_ = 479;
    std::map<int, Effect> effects_;
    Effect windowEffect_{1, 0, ""};
    int effectBlank_ = 0;
    bool effectSkip_ = true;
    bool effectCut_ = false;
    std::string transMode_ = "l";
    std::unordered_set<int> windowChips_;
    bool loadImage(const std::string &tag, LoadedImage &out);
    // A background image. Backgrounds are opaque: the default transparency mode (top-left colour) doesn't apply.
    bool loadBackground(const std::string &tag, LoadedImage &out) {
        return loadImage(!tag.empty() && tag[0] == ':' ? tag : ":c;" + tag, out);
    }
    ImageRef loadFileImage(const std::string &file, bool *hasAlpha = nullptr);
    // An effect's mask file. The Umineko games write theirs as image tags (":c;Bmp\Efe\left.bmp"), which their engine
    // reads as the file after the tag; in NScripter the string is the file name.
    ImageRef loadMask(const std::string &spec) {
        size_t semi = spec.find(';');
        bool tagged = ponscripter_ && !spec.empty() && spec[0] == ':' && semi != std::string::npos;
        return loadFileImage(tagged ? spec.substr(semi + 1) : spec);
    }
    std::unordered_map<std::string, ImageRef> imageCache_;
    std::vector<std::string> imageCacheOrder_;
    std::unordered_set<std::string> imageAlpha_;  // cached images that have an alpha channel
    std::shared_ptr<Image> screenshot_;
    void forgetImage(const std::string &file);
    void commitScene();
    // Starts an effect from the shown frame to the pending scene. effectNo 0 commits nothing, 1 is instant.
    void printWithEffect(int effectNo);
    void printWithEffect(const Effect &e);
    // After an image command: ",effect" or ",type,duration[,mask]". False, with an instant effect, when absent.
    bool readEffectArgs(Effect &e);
    void renderScene(const Scene &s, Image &out, bool withWindow);
    void composeFrame();
    struct RunningEffect {
        Effect e;
        Image from;
        Image to;
        ImageRef mask;
        int64_t start = 0;
        bool active = false;
        // Effect 99: the DLL's file name, lowercased, its parameter string, and the rectangle that changes.
        std::string dll, param;
        int rx = 0, ry = 0, rw = 0, rh = 0;
        std::vector<uint8_t> dirs;  // breakup.dll: each cell's direction, 0..7
    } effect_;
    void finishEffect();
    void drawEffect(Image &out, double t);
    // blt draws the btndef image straight onto the screen, past the scene (a credits roll, say). The drawing is a layer
    // over the committed scene and under the built-in menus. It lasts until the next commit of the scene; after ofscpy,
    // which makes it the offscreen image, until the commit after that, so a print or an effect still ends on it. It
    // isn't saved, and a load or reset drops it.
    Image bltLayer_;
    LoadedImage bltSource_;  // the btndef image, loaded without its transparency for blt
    int bltState_ = 0;  // 0 none, 1 drawn, 2 drawn and ofscpy has run
    // Draws the btndef image's rectangle (sx, sy, sw, sh), scaled to (dx, dy, dw, dh), into the blt layer: v holds
    // the eight in that order, as blt takes them.
    void bltRect(const int64_t v[8]);
    void clearBlt() {
        endRoll_.on = false;  // a credits roll draws into the blt layer; whatever drops that ends the roll
        bltState_ = 0;
        bltLayer_ = Image();
    }
    void drawBltLayer(Image &out) const {
        if (bltState_ > 0 && bltLayer_.w == out.w && bltLayer_.h == out.h) blit(out, bltLayer_, 0, 0, 0, 0, out.w, out.h);
    }
    // Effect 99, the effect DLLs (dlleffects.cpp). startDllEffect returns false for a DLL that isn't emulated.
    bool startDllEffect(const Image &to);
    void drawDllEffect(Image &out, double t);
    int quakeAmp_ = 0, quakeX_ = 0, quakeY_ = 0, quakeKind_ = 0;  // kind: 0 quake, 1 quakex, 2 quakey
    void drawOverlays(Image &out);  // button hover images, the click cursor, system menus
    int64_t quakeEnd_ = 0, quakeDuration_ = 0;
    void animateSprites();
    void applySpStr(const std::string &ctrl);
    Sprite *spriteRef(int n);

    // Text window (text.cpp)
    struct Window {
        // The PC engine's defaults, read from its saves (its manual says x 8; the engine uses 21).
        int tx = 21, ty = 16, cols = 23, rows = 16, fw = 26, fh = 26, px = 0, py = 2, speed = 20;
        bool bold = true, shadow = true;
        bool imageWindow = false;
        uint32_t color = rgba(0x99, 0x99, 0x99);
        int x1 = 0, y1 = 0, x2 = 639, y2 = 479;
        LoadedImage image;
        std::string imageTag;
    } win_;
    std::string fontName_;
    bool windowShown_ = false;
    bool windowHiddenByEffect_ = false;
    bool textOff_ = false;  // textoff / systemcall windowerase
    bool eraseOnEffect_ = true;
    bool textHidden_ = false;   // texthide
    std::vector<TextChar> page_;
    std::string pageText_;      // the page's text, for gettext and the log
    int penX_ = 0, penY_ = 0;   // next character's cell, in pixels relative to the text origin
    int textSpeed_ = 20;
    int defaultSpeeds_[3] = {40, 20, 10};
    int speedChoice_ = 1;
    uint32_t textColor_ = rgba(255, 255, 255);
    uint32_t defaultTextColor_ = rgba(255, 255, 255);
    int shadeX_ = 1, shadeY_ = 1;
    bool english_ = false;
    bool linePage_ = false;
    int indent_ = 0;
    bool rubyOn_ = false;
    int rubyW_ = 0, rubyH_ = 0;
    std::string clickStr_;
    int clickStrLines_ = 0;
    std::string kinsokuStart_, kinsokuEnd_;
    std::string textGosub_, preTextGosub_, loadGosub_;
    bool inTextLine_ = false;      // the cursor is inside a text line
    bool textFast_ = false;        // a click asked for the rest of the text up to the next wait
    bool pendingPageClear_ = false;
    bool isPageWait_ = false;      // what the textgosub call was for, for ispage
    std::string pretextTag_;       // [tag] of the text line, for gettag
    bool pretextDone_ = false;     // the line's head (tag, pretextgosub) has been handled
    bool lineHadText_ = false;
    bool noNewlineAtEnd_ = false;  // the line ended in '/', or a page wait right before its end
    bool halfWidthText_ = false;   // inside a backquoted stretch of a text line: English, wrapped between words
    bool ignoreClickStr_ = false;
    bool clickCursorPage_ = false;
    std::vector<std::string> pendingChars_;  // characters of a variable being shown
    // Ponscripter (ponscripter.cpp, docs/nscripter.md). Settled as a black box on the Umineko games' own engine.
    bool ponscripter_ = false;
    bool caretText_ = false;  // inside ^...^ on a text line: English text, where $, %, {, <, ( and [ are plain
    // The language of the text that follows (langen 0, langjp 1, langall or none -1), and the one shown (showlangen,
    // showlangjp). Text of another language isn't drawn, but its waits run.
    int textLang_ = -1;
    int shownLang_ = 0;
    TextEncoding encoding() const { return ponscripter_ ? TextEncoding::Utf8 : TextEncoding::ShiftJis; }
    bool textHiddenByLang() const { return ponscripter_ && textLang_ >= 0 && textLang_ != shownLang_; }
    std::map<int, std::string> fontFiles_;  // h_mapfont: the game's font files, by face number
    // endroll: the btndef image scrolls up through a rectangle, a pixel every speed / 10 ms, for distance pixels,
    // while the script waits until end. Drawn into the blt layer each step; it stops early if that wait ends early.
    struct EndRoll {
        bool on = false;
        int64_t start = 0, end = 0;
        uint64_t wait = 0;  // the timer wait it started, so a later one with the same end isn't taken for it
        int64_t rect[8] = {};
        int64_t distance = 0, speed = 1;
    } endRoll_;
    void stepEndRoll();
    // drawclear, drawsp and draw build a picture of their own, which draw puts on the screen as blt does.
    Image drawBuffer_;

    // Ponscripter's proportional text (ponstext.cpp). Positions are in pixels of a screen twice the game's size, where
    // the engine draws; the text is halved onto the frame.
    std::map<int, std::unique_ptr<PonsFace>> ponsFaces_;  // loaded on first use; null when the file can't be read
    PonsFace *ponsFace(int n);
    PonsStyle ponsStyle_;
    static void applyPonsTag(PonsStyle &style, std::string_view tag);
    // The character at t[p] as Ponscripter draws it: ` and ' become curly quotes, `` and '' double ones. Returns the
    // character, and its length in the script in *used.
    static std::string ponsChar(std::string_view t, size_t p, size_t *used);
    bool ponsText() { return ponscripter_ && ponsFace(ponsStyle_.face()); }
    int penX2_ = 0, penY2_ = 0;  // the pen on the doubled screen, from the text origin
    void resetPen();
    int ponsSize() const;
    int ponsLineWidth2() const;
    int ponsIndent2() const;
    int ponsLineHeight2();
    void ponsPutChar(const std::string &ch);
    void ponsNewLine();
    void ponsBreakParagraph();  // br
    // The width on the doubled screen of the word starting at t[p], up to a space, wait or line end, skipping markup.
    // literal: t is a string putString puts (only carets and tags are markup), not a text line.
    int ponsWordWidth(std::string_view t, size_t p, bool literal = false);
    // Whether the word after the space before t[p] won't fit on the line, so the line breaks there.
    bool ponsWrapsBefore(std::string_view t, size_t p, bool literal = false);
    // Puts a string's characters, reading its carets, ~tags~ and quotes (select items, text buttons, puttext).
    void putString(const std::string &s);
    struct PonsLayer {
        Image doubled, frame;  // the page's text on the doubled screen, and halved
        // Everything a character's pixels depend on, so a cached page is drawn again when any of it changes.
        struct Placed {
            std::string ch;
            int face, px, x2, y2;
            uint32_t color;
            bool shadow;
            int shadeX, shadeY;
            bool operator==(const Placed &o) const {
                return ch == o.ch && face == o.face && px == o.px && x2 == o.x2 && y2 == o.y2 && color == o.color &&
                       shadow == o.shadow && shadeX == o.shadeX && shadeY == o.shadeY;
            }
        };
        std::vector<Placed> placed;
    } ponsLayer_;
    void drawPonsText(Image &out);
    // A ":s" string sprite's text, laid out with an em of px and pitch pixels between characters on the doubled
    // screen, and halved, one cell per colour. With a wrap width, a character that would pass it starts a line.
    ImageRef renderPonsString(const std::string &text, int px, int pitch, const std::vector<uint32_t> &colors, bool shadow,
                              int &cells, int wrap = 0);
    void noteTextLineStart();      // savepoint and read-text bookkeeping at the head of a text line
    bool textBtnOff_ = false;
    int textBtnStart_ = 1;
    uint32_t linkColor_[2] = {rgba(255, 255, 0x88), rgba(0x88, 255, 255)};
    std::vector<std::pair<int, std::string>> textButtons_;  // number, text
    int lastCharX_ = 0, lastCharY_ = 0;
    std::vector<std::string> log_;  // lookback pages, oldest first
    std::vector<std::string> logTags_;  // each page's [tag]
    std::string pageTag_;               // the current page's [tag]
    int maxLogPages_ = 50;
    std::unique_ptr<FontRasterizer> fonts_;
    struct SpFont {
        bool set = false;
        std::string name;
        int fw = 0, fh = 0, px = 0, py = 0;
        bool bold = false, shadow = false;
    } spFont_;
    void textStep();
    bool beginTextLine();
    void putChar(const std::string &ch, bool isRuby = false);
    void newLine();
    void clearPage();
    void showWindow();
    void drawWindow(Image &out);
    bool glyphFor(uint32_t cp, int w, int h, bool bold, const Glyph *&out);
    void drawGlyphs(Image &out, const std::string &str, int x, int y, int fw, int fh, int pitchX, uint32_t color,
                    bool bold, bool shadow, int maxCols, int pitchY, bool aa = true);
    ImageRef renderStringImage(const std::string &spec, int &cells);
    int halfAdvance() const;
    int fullAdvance() const;
    void textClickWait(bool page);
    bool textClickStr(const std::string &ch);
    // setcursor / abssetcursor: the click-wait cursors (0 click, 1 page), shown during the built-in click waits.
    struct ClickCursor {
        Sprite sprite;
        int dx = 0, dy = 0;
        bool absolute = false;
        bool set = false;
    } cursors_[2];
    void drawClickCursor(Image &out);
    void pushLog();

    // Buttons (buttons.cpp)
    std::vector<Button> buttons_;
    LoadedImage btnImage_;
    std::string btnDefault_;  // exbtn_d
    int hoverButton_ = -1;
    int btnTimeout_ = 0;
    int64_t btnStart_ = 0;
    int lastBtnTime_ = 0;
    bool btnDown_ = false;
    bool transBtn_ = false;
    std::set<std::string> btnKeys_;  // getcursor, getenter, ... for the next wait
    bool useWheel_ = false, useEscSpc_ = false;
    int btnArea_ = 0;
    void startButtonWait(const VarRef &result, bool clearAfter, bool textButtons);
    void updateHover();
    int hitButton(int x, int y);
    std::vector<std::pair<std::string, std::string>> cselItems_;  // text, label
    bool btnNoWindowErase_ = false;
    bool btnTimeoutWaitsVoice_ = false;
    VarRef btnResult_;
    bool btnClearAfter_ = false;
    std::map<int, std::string> textBtnCtrl_;
    bool pollButtonWait();
    // bexec, the newer button wait: results are strings ("S12", "LCLICK", "UP", ...).
    bool bexec_ = false, bCursor_ = false, bDown_ = false, bTrans_ = false;
    VarRef bexecNum_;
    int64_t bTimeStart_ = -1;  // btime's clock keeps running across bexec calls until the next btime
    int keyboardButton_ = -1;  // the button the arrow keys moved to, without bcursor
    void applyBspHover(int hit);
    bool finishBexec(const std::string &result, int sprite);
    bool pollBexec();
    enum class SelectMode { Goto, Gosub, Number };
    SelectMode selectMode_ = SelectMode::Goto;
    void startSelect(SelectMode mode);
    bool pollSelect();
    uint32_t selectColor(bool hover) const;
    VarRef selectVar_;
    std::string selectColorHex_[2] = {"FFFFFF", "888888"};
    std::vector<int> cselSprites_;
    Cursor selectReturn_;
    size_t selectFirst_ = 0;
    std::vector<int> selectRows_;
    // Ponscripter: each choice's characters on the page and the bottom of its last line, since a choice may wrap.
    std::vector<std::pair<size_t, size_t>> selectChars_;
    std::vector<int> selectBottoms_;
    int selectHover_ = -1;
    bool keyboardSelect_ = false;
    void drawSystemUi(Image &out);
    void drawDialog(Image &out, SystemUi &ui);
    std::unique_ptr<SystemUi> ui_;
    void openUi(SystemUi ui);
    void closeUi(int choice);
    void pollSystemUi();
    bool textGosubActive() const;
    void openRightClickMenu();
    void runSystemFunction(const std::string &fn);
    void yesNo(const std::string &message, std::function<void(bool)> done, bool okCancel = false);
    void openSaveLoad(bool save);
    void openLookback();
    std::string typed_, lastInput_;
    struct SaveInfo {
        int month = 0, day = 0, hour = 0, minute = 0;
        std::string note;
    };
    bool readSaveInfo(int slot, SaveInfo &info);
    std::string globalsName() const;
    std::string saveFilePath(const std::string &file) const;  // a file in the save folder, on disk

    // Timing, waits and input
    int64_t now_ = 0;
    int64_t timerBase_ = 0;
    // NSLua's NSTimer: milliseconds of wall time since the engine started. Monotonic, and it advances while a Lua
    // function runs, as the PC's tick count does; the engine clock now_ only moves between steps.
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    int64_t wallMs() const;
    WaitKind wait_ = WaitKind::None;
    int64_t waitUntil_ = 0;
    // Counts timer waits, so the command that started one can tell it from a later wait with the same end.
    uint64_t timerWaits_ = 0;
    int64_t waitStart_ = 0;
    bool paused_ = false;
    bool checkTrap();
    void processWait();
    bool waitSkippable_ = false;
    std::function<void()> onWaitDone_;
    void waitTimer(int ms, bool skippable);
    void waitClick(std::function<void()> done);
    void finishWait();
    bool waitRightToo_ = false;  // lrclick: a right click also ends the wait
    int autoClick_ = 0;          // autoclick: click waits end by themselves after this many ms
    void resetGame(bool fromDefine = false);
    struct Input {
        bool click = false, clickUp = false, rclick = false, mclick = false;
        int wheel = 0;
        std::vector<int> keys;  // pressed this frame
    } in_;
    int mouseX_ = 0, mouseY_ = 0;
    bool mouseDown_ = false;
    std::set<int> keysDown_;
    bool skipMode_ = false, autoMode_ = false;
    int autoModeTime_ = 1000;
    bool ctrlSkip() const;
    int lastClickX_ = 0, lastClickY_ = 0;
    int trapMode_ = 0;  // 0 off, 1 left, 2 left+right
    bool trapStopped_ = false;
    std::string trapLabel_;
    bool trapPending_ = false;
    bool rmode_ = true;
    std::vector<std::pair<std::string, std::string>> rmenu_;
    // insertmenu's items in order. An item whose function is "sub" opens a submenu, which holds the items one level
    // down that follow it.
    struct WindowMenuItem {
        std::string text, fn;
        int level = 0;
    };
    std::vector<WindowMenuItem> windowMenu_;
    std::string windowMenuItemsJson(size_t &next, int level) const;
    bool lookbackOnRight_ = false;
    int lastRet_ = 0;

    // Saves and system data (save.cpp)
    std::string saveDir_;
    bool saveOn_ = true;
    bool autoSaveOff_ = false;
    SaveExtras extras_;
    // The savepoint: what a save writes. Live while saveon runs top-level statements; frozen at a text page's head,
    // at saveoff, and at the savepoint command.
    bool savepointLive_ = true;
    Bytes savepoint_;
    // A Ponscripter game with a textgosub moves its savepoint only at a text line's head, before pretextgosub, while
    // saveon is in effect: to the state at the head of the page, the first text line since the page was cleared.
    // saveon and saveoff only turn that on and off (measured on the PC).
    Bytes pageHead_;
    // For each call frame a Ponscripter save holds, whether it is a user command's (1) and whether getparam has read
    // its arguments (2): the PC format leaves them to be worked out from the script, which can't always tell. Kept with
    // each saved state and written beside the save; loadedCalls_ is the record of the save being loaded.
    std::vector<uint8_t> callFlags() const;
    std::vector<uint8_t> savepointCalls_, pageHeadCalls_, loadedCalls_;
    int pageHeadLine_ = 0, pageHeadStmt_ = 0;
    bool pageHeadTaken_ = false;
    bool ponsSaves() const { return ponscripter_ && !textGosub_.empty(); }
    // Ponscripter bounds the window's text origin as it bounds sizes, so every drawing path's doubled positions fit.
    static int ponsOrigin(int v) { return std::clamp(v, -8192, 8192); }
    Cursor stmtStart_;  // where the statement being executed began
    void serializeState(Bytes &out);
    void serializeTail(Bytes &out, int line, int stmt);
    int savepointLine_ = 0, savepointStmt_ = 0;
    bool restoreState(const Bytes &data, std::string *error);
    void freezeSavepoint(bool atTextHead = false);
    void noteStatementStart(bool text);
    bool insideTextGosub() const;
    std::vector<int> arrayOrder_;  // dim order, which saves follow
    std::vector<std::string> fileLogOrder_;
    std::vector<int> labelLogOrder_;
    std::vector<uint8_t> kidoku_;
    bool kidokuOn_ = false;
    void markRead(size_t pos);
    bool isRead(size_t pos) const;
    bool savegame2_ = false;
    void noteLabelVisited(int labelIndex);
    std::string rubyFont_;
    bool globalOn_ = false;
    bool fileLogOn_ = false, labelLogOn_ = false;
    bool kidokuSkip_ = false;
    std::set<std::string> fileLog_;
    std::vector<bool> labelLog_;
    int saveNumber_ = 9;
    std::string saveNames_[3];
    std::string caption_;
    bool saveGame(int slot, const std::string &note);
    bool loadGame(int slot);
    std::string savePath(int slot) const;
    void loadGlobals();
    void importPcSaves();
    // gloval.sav's contents: each global variable as an int32 and a NUL-terminated string.
    Bytes globalsFile() const;
    Bytes globalsOnDisk_;  // what gloval.sav holds, as far as the engine knows
    // Writes gloval.sav if a global variable changed since it was last written.
    void saveGlobalsIfChanged();
    void noteFileUsed(const std::string &tag);

    // Audio (audio.cpp holds the mixer; sound commands live in commands_sound.cpp)
    AudioMixer audio_;
    std::string bgmName_;
    bool bgmLoop_ = true;
    int bgmFadeOut_ = 0, bgmFadeIn_ = 0, cdFadeOut_ = 0;
    int volBgm_ = 100, volSe_ = 100, volVoice_ = 100;
    std::string waveName_;
    bool waveLoop_ = false;
    bool waveDuringSkip_ = false;  // mode_wave_demo
    std::array<int, 50> chVol_ = [] { std::array<int, 50> a{}; a.fill(-1); return a; }();
    std::array<std::string, 50> dwaveLoaded_, dwaveLoopName_;
    std::string loopBgmHead_, loopBgmTail_;
    bool loopBgmPending_ = false;
    void playBgm(const std::string &name, bool loop);
    void playSound(int channel, const std::string &name, bool loop);
    bool readSound(const std::string &name, Bytes &out);
    std::string cdTrackFile(int track) const;

    // Movies (video.cpp)
    struct Movie;
    std::unique_ptr<Movie> movie_;
    void playMovie(const std::string &file, bool clickStops, bool loop, bool async, int x, int y, int w, int h);
    void stepMovie(int ms);
    void stopMovie();
    void drawMovie(Image &out);

    // Plugins emulated in place of the game's DLLs (plugins.cpp)
    struct Layer {
        std::string plugin;
        int interval = 0;  // ms between the plugin's Timer calls
        int64_t nextTick = 0;
    };
    std::map<int, Layer> layers_;
    void tickLayers();
    void drawLayer(int layer, Image &out);
    void layerMessage(int layer, const std::string &msg);
    void execDll(const std::string &call);
    struct GaijiRule {
        std::string ch;
        std::string image;
    };
    std::vector<GaijiRule> gaiji_;
    struct MSpDrawItem;
    std::vector<std::shared_ptr<MSpDrawItem>> mspdraw_;

    // NSLua (nslua.cpp): the game's system.lua run by Lua 5.1, as the PC engine's nslua.dll runs it. luasub names
    // become commands that call NSCOM_name; luacall names are callbacks (NSCALL_name) the engine makes.
    lua_State *lua_ = nullptr;
    std::unordered_set<std::string> luaCommands_;
    std::unordered_set<std::string> luaCallbacks_;
    bool luaAnimOn_ = false;
    int luaAnimInterval_ = 15;
    int64_t luaAnimNext_ = 0;
    Cursor luaReturn_;  // where NSGosub returns to: after the Lua command's statement, or where a callback was made
    std::chrono::steady_clock::time_point luaCallStart_;  // when the outermost running Lua call began
    bool inLua_ = false;
    void luaStart();
    void luaStop();
    bool luaCommand(const std::string &name);
    // Calls NSCALL_name if the script asked for it with luacall. arg is passed when it isn't negative; *result gets
    // a boolean return value.
    bool luaCallback(const std::string &name, int arg = -1, bool *result = nullptr);
    // Runs one statement given as text, in place: NSExec.
    void execString(const std::string &statement);

    std::string problem_;
    bool ended_ = false;
    bool quitRequested_ = false;
    int64_t rngState_ = 1;
    int nextRandom(int n);

    Config cfg_;
    FileSystem fs_;
    Script script_;
};

using CommandFn = void (*)(Engine &, const std::string &name);
// Every command, by lowercase name. Defined across the command files.
const std::unordered_map<std::string, CommandFn> &commandTable();
// Ponscripter's own commands, known only in a Ponscripter game, so an NScripter game's script reads as before.
const std::unordered_map<std::string, CommandFn> &ponscripterCommandTable();
void registerCommands(std::unordered_map<std::string, CommandFn> &table);

}  // namespace vnns
