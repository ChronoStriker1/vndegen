#include "engine.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <cerrno>
#include <ctime>
#include <iterator>
#include <sstream>
#include <dirent.h>
#include <strings.h>
#include <sys/stat.h>

#include "font.h"
#include "sjis.h"
#include "tables.h"

namespace neko {
namespace {

constexpr int kKindBgm = 0, kKindSe = 1, kKindVoice = 2;
// A group kept at no volume, for voices whose speaker the player has unchecked while they play.
constexpr int kMutedVoices = 4;
// Test Game 94's Config gives H sound effects (files named hse_) and background voices (voice_type.ini's types from 50)
// volumes of their own.
constexpr int kGroupHse = 5, kGroupBackVoice = 6;
// How long the message window takes to slide in, read from the PC game's footage.
constexpr int kWindowSlideMs = 200;
// The film noise's slot, and how many frames each random place of its crop lasts, as The Alchemist's exe has them.
constexpr int kNoiseSlot = 10000, kNoiseFramesPerPlace = 4;
// How long each character of text takes to fade in, measured from the PC game's footage.
constexpr int kCharFadeMs = 160;
// How far the backlog dims the frame behind it, from the PC game's footage: about 0.29 of the brightness.
constexpr int kLogDim = 182;
// Eight cards to a page and ten pages of saves, and then the Q-Save page.

// Scripts name a voice `voice\\x.ogg`, or in places in Test Game 84 just `x`, which plays all the same on the PC.
static std::string voicePath(const std::string &name) { return name.find('\\') == std::string::npos ? "voice\\" + name + ".ogg" : name; }
constexpr int kCountEdgeR = 200, kCountEdgeG = 30, kCountEdgeB = 30;
// cfg.sav's tables of what's been unlocked: a row of ints for each CG, a picture to an int, and an int for each track.
// They sit after a string at byte 44020, so they move with its length.
constexpr size_t kConfigSize = 104000, kConfigStringAt = 44020, kCgFlagsAt = 48000, kTrackFlagsAt = 100000, kSpriteOutfitAt = 1212, kSpriteLockAt = 1224;
// How long Auto waits after the text and voice are done.
constexpr int kAutoDelayMs = 1000;
constexpr float kPi = 3.14159265f;
constexpr float kShakeMs = 300, kShakePixelsX = 24, kShakePixelsY = 28;

int field(const std::vector<std::string> &line, size_t index) { return index < line.size() ? std::atoi(line[index].c_str()) : 0; }
bool inRect(int x, int y, int left, int top, int w, int h) { return x >= left && y >= top && x < left + w && y < top + h; }
// A flipbook's picture: the first one's name with its last three digits counting up.
std::string flipPicture(const std::string &first, int index) {
    size_t dot = first.rfind('.');
    if (dot == std::string::npos || dot < 3) return first;
    char digits[8];
    std::snprintf(digits, sizeof digits, "%03d", index);
    return first.substr(0, dot - 3) + digits + first.substr(dot);
}
std::string nameInFolder(const std::string &folder, const std::string &name);
std::string matchCase(const std::string &base, const std::string &relative);
std::vector<uint8_t> readWhole(const std::string &path);

// What Config shows before the player changes anything, which is what the PC's shows.
const std::map<std::string, int> &defaultSettings() {
    static const std::map<std::string, int> defaults = {
        {"master", 100}, {"bgm", 80}, {"se", 80}, {"voice", 80}, {"hse", 80}, {"backvoice", 80},
        {"mute_master", 0}, {"mute_bgm", 0}, {"mute_se", 0}, {"mute_voice", 0}, {"mute_hse", 0}, {"mute_backvoice", 0},
        // Whether a click that moves the text on stops its voice, as the PC games without the setting always do.
        {"voice_stop", 1},
        {"text", 1}, {"auto", 1}, {"rightclick", 0}, {"opacity", 60}, {"music", 60}, {"count_color", 1}, {"gt_off", 0}, {"express", 1}, {"bar_always", 0}, {"cg_anim", 1}, {"anim_manual", 0},
        {"confirm_saveload", 1}, {"confirm_overwrite", 1}, {"confirm_delete", 1}, {"confirm_quick", 1}, {"confirm_title", 1}, {"confirm_exit", 1},
        // Config's effects, waits, clicking past waits and the H scenes' camera work, as the PC starts them.
        {"effect", 1}, {"wait", 1}, {"waitskip", 0}, {"camera", 1}, {"tutorial", 1},
        // How long waits and fades last, in hundredths: Config's wait time runs from ×0.70 to ×1.30.
        {"waittime", 100},
        // Whether the game stops while VNDegen isn't the active app, off as the PC starts it.
        {"pause_inactive", 0},
        // The keyboard's confirm key, 0 Space and 1 Enter; the other does what the right button does.
        {"deckey", 0},
        // Skip passes all text, or with 0 only what's been read, as the PC starts it with all.
        {"skipall", 1},
    };
    return defaults;
}

std::string trim(const std::string &s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}

std::vector<std::string> split(const std::string &s, char separator) {
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t end = s.find(separator, start);
        parts.push_back(s.substr(start, end == std::string::npos ? end : end - start));
        if (end == std::string::npos) return parts;
        start = end + 1;
    }
}

float progressOf(const Track &t, int ms) {
    if (ms <= t.startMs) return 0;
    if (t.lengthMs <= 0 || ms >= t.startMs + t.lengthMs) return 1;
    float linear = float(ms - t.startMs) / float(t.lengthMs);
    if (!t.easeOut) return linear;
    float left = 1 - linear;
    return 1 - left * left;
}

}  // namespace

// MARK: Config

void Config::parse(const std::vector<uint8_t> &data) {
    std::string text = sjis::toUtf8(std::string(data.begin(), data.end()));
    for (const std::string &raw : split(text, '\n')) {
        std::string line = raw.substr(0, raw.find("//"));
        size_t tab = line.find_first_of(" \t");
        if (tab == std::string::npos) continue;
        std::string key = trim(line.substr(0, tab)), value = trim(line.substr(tab));
        if (!key.empty() && !value.empty() && key[0] != '#') values_[key] = value;
    }
}

std::string Config::text(const std::string &key, const std::string &fallback) const {
    auto it = values_.find(key);
    return it == values_.end() ? fallback : it->second;
}

int Config::number(const std::string &key, int fallback) const {
    auto it = values_.find(key);
    return it == values_.end() ? fallback : std::atoi(it->second.c_str());
}

// MARK: Start and stop

Engine::~Engine() {
    // The audio thread may be reading a movie's sound, which goes with the sprites.
    audio_.clearStream();
    saveSeen();
    // Quitting from the sprite gallery keeps its outfits and locks, as leaving it does.
    if (spriteScreen()) saveGlobals();
    saveReadText();
    if (log_ && log_ != stderr) std::fclose(log_);
}

void Engine::log(const char *format, ...) {
    va_list args;
    va_start(args, format);
    std::vfprintf(log_, format, args);
    va_end(args);
    std::fputc('\n', log_);
    std::fflush(log_);
}

void Engine::warnOnce(const std::string &what) {
    if (warned_[what]) return;
    warned_[what] = true;
    log("%s", what.c_str());
}

void Engine::complain(const std::string &what) {
    problem_ = what;
    warnOnce(what);
}

bool Engine::start(const std::string &root, const std::string &dataDir, const std::string &logPath, const std::string &fontDir, std::string &error) {
    log_ = logPath.empty() ? stderr : std::fopen(logPath.c_str(), "a");
    if (!log_) log_ = stderr;
    root_ = root;
    if (!files_.open(root)) {
        error = "No NekoSDK archives were found in this folder.";
        return false;
    }
    Bytes init = files_.read("cfg\\init.ini");
    if (!init) {
        error = "This game has no cfg\\init.ini.";
        return false;
    }
    config_.parse(*init);
    images_ = std::make_unique<Images>(files_);
    setFontPackDir(fontDir);

    fps_ = config_.number("FPS", 60);
    windowImage_ = images_->get(config_.text("MsgWinFilename"));
    windowX_ = config_.number("MsgWinPosX");
    windowY_ = config_.number("MsgWinPosY");
    width_ = config_.number("WindowWidth", 1280);
    // Test Game 95 says 1020 but draws on 1080: its message window and buttons reach the bottom of that. The message
    // windows of Haha Sannin to Ana Asobi and Ore o Hoshigaru Tsuma to Gibo hang a pixel past their 720, which the PC cuts
    // off, so the canvas grows no further than 16:9.
    height_ = std::max(config_.number("WindowHeight", 720), windowImage_ ? std::min(windowY_ + windowImage_->h, width_ * 9 / 16) : 0);
    canvas_.resize(width_, height_);
    canvas_.clear();
    if (std::string square = config_.text("MsgWinSquareFilename"); !square.empty()) {
        squareImage_ = images_->get(square);
        squareY_ = config_.number("MsgWinSquarePosY", height_);
        squareShift_ = config_.number("MsgWinSquareMovement", 240);
    }

    windowShowType_ = config_.number("MsgWinShowType");
    waitMark_ = images_->get(config_.text("MsgWaitMarkFilename"));
    markX_ = config_.number("MsgWaitMarkPosX");
    markY_ = config_.number("MsgWaitMarkPosY");
    markPatterns_ = std::max(1, config_.number("MsgWaitMarkPattern", 1));
    markSpeed_ = float(std::atof(config_.text("MsgWaitMarkSpeed", "1").c_str()));
    nameX_ = config_.number("MsgNamePosX");
    nameY_ = config_.number("MsgNamePosY");
    textX_ = config_.number("MsgStringPosX");
    textY_ = config_.number("MsgStringPosY");
    fontFace_ = config_.number("MsgFont");
    fontSize_ = config_.number("MsgFontSize", 28);
    fontType_ = config_.number("MsgFontType");
    lineInterval_ = config_.number("MsgLineInterval");
    maxChars_ = config_.number("MsgMaxCharNum", 64);
    textR_ = config_.number("MsgColorR", 255);
    textG_ = config_.number("MsgColorG", 255);
    textB_ = config_.number("MsgColorB", 255);
    shadowR_ = config_.number("MsgShadowColorR");
    shadowG_ = config_.number("MsgShadowColorG");
    shadowB_ = config_.number("MsgShadowColorB");
    // The backlog's "shadow" colour fills its letters in the PC game, and its plain colour frames them.
    logFrameR_ = config_.number("MsgLogShadowColorR", 255);
    logFrameG_ = config_.number("MsgLogShadowColorG", 128);
    logFrameB_ = config_.number("MsgLogShadowColorB");
    logTextR_ = config_.number("MsgLogColorR");
    logTextG_ = config_.number("MsgLogColorG");
    logTextB_ = config_.number("MsgLogColorB");

    // What a game's exe draws on its own, which no data file names.
    folder_ = normalizedPath(config_.text("UserDataDirectory"));
    layout_ = saveLayout(folder_);
    thumbW_ = config_.number("SaveSmallImageSizeW", 160);
    thumbH_ = config_.number("SaveSmallImageSizeH", 90);
    // Where Windows keeps them, under the data folder, so a backup from a PC restores to the same place.
    if (!dataDir.empty()) {
        std::string relative = config_.text("UserDataDirectory");
        std::replace(relative.begin(), relative.end(), '\\', '/');
        saveDir_ = matchCase(dataDir, "AppData/Roaming/" + relative + "/save");
    }
    if (folder_ == "waffle\\x2019_retry") {
        bars_ = {images_->get("obj\\l000_000.bmp"), images_->get("obj\\l000_001.bmp")};
        barY_ = {0, 618};
        // The PC game shows no dimming from its message window here, at its Config defaults and at 90.
        windowOpacity_ = 0;
        windowAlwaysClear_ = true;
    }
    // The Alchemist's sys_5.pak holds Masami Chie's original CGs and sprites. The PC starts on Waffle's arranged ones in
    // the base archives, and its message window's art button and Config's Change Illustration switch between them.
    if (folder_ == "waffle\\alchemy") artSet_ = "sys_5";

    if (config_.number("MsgWinButtonPlateDisable") == 0) buttonPlate_ = images_->get(config_.text("MsgWinButtonPlateFilename"));
    barTop_ = bars_.size() > 1 ? barY_[1] : height_ - 102;
    plateX_ = config_.number("MsgWinButtonPlatePosX");
    plateY_ = config_.number("MsgWinButtonPlatePosY");
    if (buttonPlate_ && buttonPlate_->w >= width_) barTop_ = plateY_;
    plateHiddenY_ = config_.number("MsgWinButtonPlateBottomPosY", height_);
    if (Bytes ini = files_.read(config_.text("MsgButtonDeclFilename", "cfg\\msg_button.ini"))) messageButtons_ = parseMessageButtons(*ini, *images_);
    if (Bytes ini = files_.read(config_.text("MsgButtonPlayViewDeclFilename", "cfg\\msg_button_playview.ini"))) {
        replayButtons_ = parseMessageButtons(*ini, *images_);
        hasReplayButtons_ = true;
    }
    titleBgm_ = config_.text("TitleBGM");

    if (const char *table = menuTable(folder_))
        screens_ = parseScreens((menuTablePrefix() + table).c_str(), *images_, [this](const std::string &what) { warnOnce(what); });
    commentFace_ = fontFace_;
    // `textfont N`, `textsize N`, `textgap G`, `textline N` and `textshift DX DY`: the message window's text as the PC sets
    // it. The face is a number as init.ini's MsgFont gives one, 7 for Lucida Sans Unicode, which the official English
    // releases ask for (Wine's font log). Measured under Wine with VNDegen's stand-in fonts in place of the Windows ones,
    // and for Lucida Sans Unicode from the publisher's Windows screenshots.
    if (auto it = screens_.find("defaults"); it != screens_.end()) {
        const Screen &d = it->second;
        if (const auto *f = d.setting("textfont"); f && !f->empty()) fontFace_ = field(*f, 0);
        commentFace_ = fontFace_;
        // `commentfont N`: the face of a save card's comment, which the PC asks for apart from the text's.
        if (const auto *f = d.setting("commentfont"); f && !f->empty()) commentFace_ = field(*f, 0);
        if (const auto *f = d.setting("textsize"); f && !f->empty()) fontSize_ = field(*f, 0);
        if (const auto *f = d.setting("textgap"); f && !f->empty()) textGap_ = float(std::atof((*f)[0].c_str()));
        if (const auto *f = d.setting("textline"); f && !f->empty()) textLine_ = field(*f, 0);
        if (const auto *f = d.setting("textshift"); f && f->size() >= 2) {
            textDX_ = field(*f, 0);
            textDY_ = field(*f, 1);
        }
    }
    if (auto it = screens_.find("defaults"); it != screens_.end())
        if (const auto *quick = it->second.setting("quickslots"); quick && quick->size() >= 2) {
            firstQuickSlot_ = nextQuickSlot_ = field(*quick, 0);
            lastQuickSlot_ = field(*quick, 1);
        }
    if (auto it = screens_.find("defaults"); it != screens_.end())
        if (const auto *line = it->second.setting("routevar"); line && !line->empty()) routeVar_ = field(*line, 0);
    log("started: %dx%d, %d scripts in scr, %s, movies %s", width_, height_, int(files_.list("scr").size()), screens_.empty() ? "no menus yet" : "menus",
        hasMovies_ ? "through the host's decoder" : "left out, with no decoder");
    loadGalleries();
    loadParts();
    loadOldLayout();
    loadGlobals();
    loadSettings();
    if (screens_.count("title")) openScreen("title");
    else startStory();
    return true;
}

// MARK: Menus

void Engine::openScreen(const std::string &name) {
    auto it = screens_.find(name);
    if (it == screens_.end()) {
        warnOnce("No menu table for " + name);
        return;
    }
    bool fromStory = !screen_ && name != "title";
    // A ♪ stays lit once played while Config is open, as on the PC; a later visit starts them unlit.
    if (name.rfind("config", 0) != 0) testsPlayed_.clear();
    // Test Game 84's title opens with its art revealed in four tiles and its menu fading in, when the game starts or the
    // story returns to it. From another menu it's simply there.
    openingFrom_ = name == "title" && !screen_ && it->second.setting("opening") ? screenMs_ : -1;
    if (fromStory) returnTo_.clear();
    else if (screen_ && screen_->name == "title") returnTo_ = "title";
    else if (name == "title") returnTo_.clear();
    // Leaving the sprite gallery keeps its outfits and locks in cfg.sav and stops its music.
    if (spriteScreen() && name != "sprites") closeSprites();
    screen_ = &it->second;
    if (spriteScreen()) openSprites();
    pressed_.clear();
    dirty_ = true;
    page_ = 0;
    menuStage_ = screen_->setting("menustages") ? 1 : 2;
    subMenu_.clear();
    // The drifting line art starts from a sketch picked at random, as the PC's does.
    driftFrom_ = screenMs_;
    galleryTabAt_ = screenMs_;
    driftFirst_ = screen_->settings.count("drift") ? size_t(std::rand()) % screen_->settings.count("drift") : 0;
    for (Picture &p : screen_->pictures) {
        if (p.variants.empty()) continue;
        size_t pick = 0;
        if (!p.randomIf.empty() && anyGlobalSet(p.randomIf)) pick = size_t(std::rand()) % p.variants.size();
        else if (auto it = globals_.find(p.pickBy); p.pickBy >= 0 && it != globals_.end() && it->second >= 0) pick = size_t(it->second);
        // Past the last, the PC keeps its first.
        p.image = pick < p.variants.size() && p.variants[pick] ? p.variants[pick] : p.variants[0];
    }
    // `gallerytabs TAB`: the tab a gallery opens on, the middle one of three in Test Game 92.
    if (const auto *tabs = screen_->setting("gallerytabs"); tabs && !tabs->empty()) galleryTab_ = field(*tabs, 0);
    if (name == "title" && !titleBgm_.empty() && !titleMusic_) {
        if (Bytes data = files_.read(titleBgm_)) {
            titleMusic_ = audio_.open(data);
            if (titleMusic_) {
                audio_.setLoops(titleMusic_, 0);
                audio_.play(titleMusic_);
                holdMusic();
            }
        }
    }
}

void Engine::closeScreen() {
    if (spriteScreen()) closeSprites();
    screen_ = nullptr;
    dirty_ = true;
}

// Back from the story to a menu, with its sound and sprites cleared.
void Engine::endStory(const std::string &screen) {
    stopTrack();  // first, since a held player's fade would never finish
    stopKind(kKindBgm, 500);
    stopKind(kKindSe, 500);
    stopKind(kKindVoice, 0);
    if (voicePlayer_) {
        audio_.close(voicePlayer_);
        voicePlayer_ = 0;
    }
    skipToggle_ = false;
    autoMode_ = false;
    windowHiddenByPlayer_ = false;
    staffRollLayers_ = nullptr;
    gt_ = {};
    splashes_.clear();
    resetParts();
    stack_.clear();
    scene_ = {};
    back_.clear();
    front_.clear();
    stopMovieSound();
    windowShown_ = false;
    replaying_ = false;
    waiting_ = Waiting::None;
    saveSeen();
    saveReadText();
    saveGlobals();
    // A replay started from the viewer comes back to it, in the gallery it was opened from, at the picture it left.
    bool toViewer = replayFromViewer_ && screen == "scene";
    replayFromViewer_ = false;
    std::string to = toViewer ? viewerGallery_ : screen;
    bool gallery = to == "scene" || to == "cg" || to == "anime";
    returnTo_ = gallery ? "title" : "";
    screen_ = nullptr;
    openScreen(to);
    if (gallery) returnTo_ = "title";
    if (toViewer && viewEntry_ < galleryEntries().size() && viewImage_ < galleryEntries()[viewEntry_].images.size()) {
        closeViewer();
        viewerShow(viewEntry_, viewImage_);
    }
}

void Engine::startStory() {
    stopTrack();
    if (titleMusic_) {
        audio_.fade(titleMusic_, 500, 0, true);
        audio_.retire(titleMusic_);
        titleMusic_ = 0;
    }
    screen_ = nullptr;
    stack_.clear();
    scene_ = {};
    back_.clear();
    front_.clear();
    scenario_.clear();
    backlog_.clear();
    choiceImage_.clear();
    choiceMade_ = false;
    replaying_ = false;
    waiting_ = Waiting::None;
    if (routeVar_ >= 0 && pendingRoute_) scenario_[routeVar_] = pendingRoute_;
    pendingRoute_ = 0;
    staffRollLayers_ = nullptr;
    resetParts();
    std::string first = config_.text(storyFlowKey_.c_str());
    // A game with no flow scripts starts at its story table's first flow, or past the prologue at the story flow `screen
    // start` names.
    if (first.empty() && !firstStoryFlow().empty()) first = "story:" + (storyFlowKey_ == "FirstFlow" ? firstStoryFlow() : storyFlowKey_);
    auto flow = first.empty() ? nullptr : loadScript(first);
    if (!flow || !flow->first()) {
        complain("This game has no flow script to start from.");
        waiting_ = Waiting::Ended;
        return;
    }
    // A new run has no scene of its own yet, so a save before its first names none.
    lastScene_.clear();
    flowCallId_ = 0;
    stack_.push_back({flow, flow->first()});
    log("story from %s", first.c_str());
    dirty_ = true;
}

// The key in init.ini of the story past the prologue. Test Game 84's table names its own.
std::string Engine::mainFlowKey() const {
    auto it = screens_.find("start");
    const auto *flow = it == screens_.end() ? nullptr : it->second.setting("flow");
    return flow && !flow->empty() ? (*flow)[0] : "MainFlow";
}

void Engine::runAction(const std::string &action) {
    log("action %s", action.c_str());
    dirty_ = true;
    // The PC doesn't save during a staff roll: a save there holds the scene after it (set 1, Test Game 87).
    if ((action == "nav_save" || action == "Save" || action == "QuickSave") && waiting_ == Waiting::StaffRoll) return;
    if (action.rfind("submenu:", 0) == 0) {
        // Pressed again, a submenu closes, and any inside it, back to the one it opened in.
        std::string name = action.substr(8);
        size_t slash = name.rfind('/');
        subMenu_ = submenuOpen(name) ? (slash == std::string::npos ? "" : name.substr(0, slash)) : name;
        return;
    }
    if (action == "start") {
        // A game whose init.ini names no FirstFlow starts at its table's `flow`, as Test Game 89's New Game starts its
        // first chapter.
        if (config_.text("FirstFlow").empty() && firstStoryFlow().empty()) {
            storyFlowKey_ = mainFlowKey();
            startStory();
            return;
        }
        // `startmenu GLOBAL`: past the prologue, Test Game 94's Start opens its submenu of where to start from.
        if (const auto *menu = screen_ ? screen_->setting("startmenu") : nullptr; menu && anyGlobalSet({field(*menu, 0)})) {
            runAction("submenu:start");
            return;
        }
        // Past the prologue a game may offer to skip it, when its table has that question.
        if (auto it = screens_.find("start"); it != screens_.end()) {
            const auto *after = it->second.setting("after");
            bool hasMain = !config_.text(mainFlowKey()).empty() || storyFlowExists(mainFlowKey());
            if (hasMain && variableValue(0, after ? field(*after, 0) : 1)) {
                openScreen("start");
                return;
            }
        }
        storyFlowKey_ = "FirstFlow";
        startStory();
        return;
    }
    if (action == "start_prologue") { storyFlowKey_ = "FirstFlow"; startStory(); return; }
    if (action == "start_main") { storyFlowKey_ = mainFlowKey(); startStory(); return; }
    if (action == "start_sub") { storyFlowKey_ = "SubFlow1"; startStory(); return; }
    if (action.rfind("route", 0) == 0) {
        pendingRoute_ = std::atoi(action.c_str() + 5);
        storyFlowKey_ = "FirstFlow";
        startStory();
        return;
    }
    if (action.rfind("track", 0) == 0) {
        playTrack(std::atoi(action.c_str() + 5));
        return;
    }
    if (action == "music_stop") { stopTrack(); return; }
    if (action == "music_pause") {
        if (trackPlayer_) {
            trackPaused_ = !trackPaused_;
            audio_.pause(trackPlayer_, trackPaused_);
        }
        return;
    }
    if (action == "music_play") {
        if (trackPlayer_ && trackPaused_) {
            trackPaused_ = false;
            audio_.pause(trackPlayer_, false);
        } else if (!trackPlayer_) {
            playTrack(currentTrack_ < 0 ? nextTrack(1) : currentTrack_);
        }
        return;
    }
    // One button for both, in Test Game 84. It shows what pressing it would do.
    if (action == "music_toggle") {
        runAction(trackPlayer_ && !trackPaused_ ? "music_pause" : "music_play");
        return;
    }
    if (action == "music_next") { playTrack(nextTrack(1)); return; }
    if (action == "music_prev") { playTrack(nextTrack(-1)); return; }
    if (action == "music_repeat") {
        trackRepeat_ = !trackRepeat_;
        if (trackPlayer_) audio_.setLoops(trackPlayer_, trackRepeat_ ? 0 : 1);
        return;
    }
    if (action == "music_shuffle") { trackShuffle_ = !trackShuffle_; return; }
    // `music_one`, `music_all` and `music_random`: Test Game 84's three modes, of which one is always on: the track
    // over again, the list round from the top after its last, or tracks at random. Repeating the list is the default.
    if (action == "music_one" || action == "music_all" || action == "music_random") {
        trackRepeat_ = action == "music_one";
        trackShuffle_ = action == "music_random";
        if (trackPlayer_) audio_.setLoops(trackPlayer_, trackRepeat_ ? 0 : 1);
        return;
    }
    if (action.rfind("tab", 0) == 0) {
        std::string keep = returnTo_;
        openScreen("config" + action.substr(3));
        returnTo_ = keep;
        return;
    }
    // The bar's Exit in a replay ends the replay at once, back to the gallery, as the PC game does without asking.
    if (action == "Exit" && replaying_) {
        endStory("scene");
        return;
    }
    if (action == "quit" || action == "Exit") { openDialog(7, [this] { waiting_ = Waiting::Ended; }); return; }
    if (action == "Title") { openDialog(6, [this] { endStory("title"); }); return; }
    if (action == "Skip") { skipToggle_ = !skipToggle_; return; }
    if (action == "Auto") { autoMode_ = !autoMode_; return; }
    if (action == "ModeChange") {
        changeSetting("art", setting("art") ? 0 : 1);
        saveSettings();
        return;
    }
    if (action == "HideWin") { windowHiddenByPlayer_ = true; return; }
    if (action == "System") { barPinned_ = !barPinned_; return; }
    if (action == "EXpress") {
        if (!artSwitchable()) return;
        changeSetting("express", setting("express") ? 0 : 1);
        saveSettings();
        return;
    }
    // Test Game 94's galleries choose 拘り表現 (`express1`) or 通常表現 (`express0`), Config's こだわり表現 setting.
    if (action == "express1" || action == "express0") {
        changeSetting("express", action == "express1" ? 1 : 0);
        saveSettings();
        return;
    }
    if (action == "Ejaculation") { gtEjaculate(); return; }
    if (action == "Extension") {
        if (gt_.active && !gt_.extending) gt_.extend = !gt_.extend;
        return;
    }
    if (action == "GOODTiming") {
        changeSetting("gt_off", setting("gt_off") ? 0 : 1);
        saveSettings();
        return;
    }
    if (action == "LogMode") { openLog(); return; }
    // `versioninfo`: the PC's Version info box, its lines in the table's `versionline` settings, a blank line for one with
    // none.
    if (action == "versioninfo") {
        std::string text;
        if (auto it = screens_.find("defaults"); it != screens_.end())
            for (auto [line, end] = it->second.settings.equal_range("versionline"); line != end; ++line) {
                if (!text.empty()) text += "\n";
                for (size_t i = 0; i < line->second.size(); i++) text += (i ? " " : "") + line->second[i];
            }
        hostRequestKind_ = "alert";
        hostRequestPayload_ = text;
        return;
    }
    // `manual` and `websurvey`: the title's Manual opens the game's own manual page, and its web survey the page the
    // game's .url file names, which the PC opens in a browser. The table lists the files to look for, a patch's first.
    // A test host's press never asks for the survey, a page on the web.
    if (action == "manual" || (action == "websurvey" && !testPress_)) {
        auto it = screens_.find("defaults");
        const auto *files = it != screens_.end() ? it->second.setting(action) : nullptr;
        for (size_t i = 0; files && i < files->size(); i++) {
            std::string relative = (*files)[i];
            std::replace(relative.begin(), relative.end(), '\\', '/');
            std::string path = matchCase(root_, relative);
            std::vector<uint8_t> bytes = readWhole(path);
            if (bytes.empty()) continue;
            if (action == "manual") {
                hostRequestKind_ = "manual";
                hostRequestPayload_ = path;
                return;
            }
            std::string text(bytes.begin(), bytes.end());
            size_t at = text.find("URL=");
            if (at == std::string::npos) continue;
            size_t end = text.find_first_of("\r\n", at);
            hostRequestKind_ = "url";
            hostRequestPayload_ = text.substr(at + 4, end == std::string::npos ? std::string::npos : end - at - 4);
            return;
        }
        return;
    }
    // `test:se:FILE` or `test:voice:FILE`: Config's ♪ plays a sample at that group's volume, the one Test Game 94's PC reads
    // for it under Wine.
    if (action.rfind("test:", 0) == 0) {
        std::string rest = action.substr(5);
        size_t colon = rest.find(':');
        if (colon == std::string::npos) return;
        if (testPlayer_) audio_.close(testPlayer_);
        testPlayer_ = 0;
        testsPlayed_.insert(action);
        if (Bytes data = files_.read(rest.substr(colon + 1))) {
            testPlayer_ = audio_.open(data);
            if (testPlayer_) {
                // At its own group's volume: HSE samples at HSE's, background voices at theirs.
                audio_.setGroup(testPlayer_, soundGroup(rest.compare(0, colon, "voice") == 0 ? kKindVoice : kKindSe, rest.substr(colon + 1)));
                audio_.setLoops(testPlayer_, 1);
                audio_.play(testPlayer_);
            }
        }
        return;
    }
    if (action == "Voice") {
        if (!currentText_.voice.empty() && voiceAllowed(currentText_.voice)) {
            if (voicePlayer_) audio_.close(voicePlayer_);
            voicePlayer_ = 0;
            if (Bytes data = files_.read(voicePath(currentText_.voice))) {
                voicePlayer_ = audio_.open(data);
                if (voicePlayer_) {
                    voiceFiles_[voicePlayer_] = currentText_.voice;
                    audio_.setGroup(voicePlayer_, soundGroup(kKindVoice, currentText_.voice));
                    audio_.setLoops(voicePlayer_, 1);
                    audio_.play(voicePlayer_);
                }
            }
        }
        return;
    }
    if (action == "QuickSave") {
        noteThumbnail();
        // Each session starts at the first of the Q-Save page's cards and moves along one a save, as the PC does.
        openDialog(4, [this] {
            auto step = [this] { nextQuickSlot_ = nextQuickSlot_ == lastQuickSlot_ ? firstQuickSlot_ : nextQuickSlot_ + 1; };
            // A locked card is passed over, and with every one locked there's nowhere to save.
            for (int tries = firstQuickSlot_; tries <= lastQuickSlot_ && locked(nextQuickSlot_); tries++) step();
            if (locked(nextQuickSlot_)) return;
            saveToSlot(nextQuickSlot_);
            step();
        });
        return;
    }
    if (action == "QuickLoad" || action == "quickload") {
        int newest = newestQuickSlot();
        if (newest) openDialog(5, [this, newest] { loadFromSlot(newest); });
        return;
    }
    // Test Game 89's arrows step through a gallery tab's pages and stop at its ends.
    if (action == "pageprev" || action == "pagenext") {
        if (!pageStepOff(action)) page_ += action == "pagenext" ? 1 : -1;
        return;
    }
    if (action.rfind("gtab", 0) == 0) {
        galleryTab_ = std::atoi(action.c_str() + 4);
        galleryTabAt_ = screenMs_;
        page_ = 0;
        return;
    }
    if (action == "cfg_defaults") {
        settings_ = startingSettings();
        applySettings();
        return;
    }
    if (action == "cfg_undo") {
        settings_ = settingsWhenOpened_;
        applySettings();
        return;
    }
    if (action == "cfg_apply") { runAction("back"); return; }
    if (action == "log_close") { closeLog(); return; }
    if (action == "log_up") { scrollLog(1); return; }
    if (action == "log_down") { scrollLog(-1); return; }
    if (action == "log_top") { scrollLog(int(backlog_.size())); return; }
    if (action == "log_bottom") { scrollLog(-int(backlog_.size())); return; }
    if (action == "back") {
        viewingImage_ = false;
        stopTrack();
        saveSettings();
        // Closed first either way, so a screen that keeps something on leaving, such as the sprite gallery, does.
        std::string to = returnTo_;
        closeScreen();
        if (!to.empty()) openScreen(to);
        return;
    }
    if (action.rfind("page", 0) == 0) {
        if (galleryPageExists(std::atoi(action.c_str() + 4))) page_ = std::atoi(action.c_str() + 4);
        return;
    }
    // Moving between menus keeps where Back returns to.
    auto go = [&](const char *name) {
        noteThumbnail();
        std::string keep = returnTo_;
        bool within = screen_ != nullptr && screen_->name != "title";
        openScreen(name);
        if (within) returnTo_ = keep;
    };
    if (action == "nav_save" || action == "Save") { go("save"); return; }
    if (action == "nav_load" || action == "Load" || action == "load") { go("load"); return; }
    if (action == "nav_config" || action == "Config" || action == "config") {
        if (!screen_ || screen_->name.rfind("config", 0) != 0) settingsWhenOpened_ = settings_;
        go("config");
        return;
    }
    if (action == "nav_cg" || action == "cg") { go("cg"); return; }
    if (action == "nav_scene" || action == "scene") { go("scene"); return; }
    if ((action == "nav_anime" || action == "anime") && screens_.count("anime")) { go("anime"); return; }
    if (action == "nav_music" || action == "music") { go("music"); return; }
    if ((action == "nav_sprites" || action == "sprites") && screens_.count("sprites")) { go("sprites"); return; }
    if (action == "nav_title") { runAction("Title"); return; }
    if (action == "nav_quit") { runAction("quit"); return; }
    warnOnce("Button not built yet: " + action);
}

void Engine::openLog() {
    if (backlog_.empty() || !screens_.count("log")) return;
    // The frame behind the log has no message on it.
    bool hidden = windowHiddenByPlayer_;
    windowHiddenByPlayer_ = true;
    compose();
    windowHiddenByPlayer_ = hidden;
    logBase_ = canvas_;
    logTop_ = 0;
    logOpen_ = true;
    dirty_ = true;
}

void Engine::closeLog() {
    logOpen_ = false;
    dirty_ = true;
}

void Engine::scrollLog(int lines) {
    int top = std::clamp(logTop_ + lines, 0, std::max(0, int(backlog_.size()) - 1));
    if (top == logTop_ && lines < 0) closeLog();  // past the newest line, as the PC does
    logTop_ = top;
    dirty_ = true;
}

bool Engine::moveFocus(int dx, int dy) {
    // Every control on screen, as the rectangle a press lands in.
    struct Spot { int x, y; };
    std::vector<Spot> spots;
    auto addButtons = [&](const std::vector<Button> &buttons) {
        for (const Button &b : buttons)
            if (b.sheet && shown(b)) spots.push_back({b.x + b.cellW() / 2, b.y + b.cellH() / 2});
    };
    if (dialogOpen_) {
        addButtons(screens_.at("dialog").buttons);
    } else if (logOpen_) {
        addButtons(screens_.at("log").buttons);
    } else if (waiting_ == Waiting::Choice && choiceSheet_ && !screen_) {
        // A choice waiting under a menu, such as Config opened from the story, isn't the one on screen.
        int cellH = choiceSheet_->h / 5, x, top, step;
        int slant;
        choicePlace(x, top, step, slant);
        for (int i = 0; i < choiceCount_; i++) spots.push_back({width_ / 2, top + i * step + cellH / 2});
    } else if (screen_ && viewingImage_ && viewerPanel()) {
        // The CG viewer's panel, or nothing while it's hidden.
        for (auto [x, y] : viewerSpots()) spots.push_back({x, y});
    } else if (screen_) {
        addButtons(screen_->buttons);
        if (int perPage = cardsPerPage()) {
            if (ImageRef sheet = images_->get((*screen_->setting("slots"))[0])) {
                const auto *rows = screen_->setting("slotrows");
                for (int i = 0; i < perPage; i++) {
                    int cx, cy;
                    cardPlace(i, cx, cy);
                    spots.push_back({cx + sheet->w / 2, cy + sheet->h / (rows ? field(*rows, 0) : 5) / 2});
                    // A card with a save has its lock and Delete to reach too, without a pointer.
                    if (!slotInfo(page_ * perPage + i + 1).exists) continue;
                    for (const char *key : {"slotlock", "slotdelete"})
                        if (const auto *line = screen_->setting(key); line && line->size() >= 3)
                            if (ImageRef icon = images_->get((*line)[0]))
                                spots.push_back({cx + field(*line, 1) + icon->w / 2, cy + field(*line, 2) + icon->h / 10});
                    if (const auto *line = screen_->setting("slotcomment"); line && line->size() >= 4)
                        spots.push_back({cx + field(*line, 0) + field(*line, 2) / 2, cy + field(*line, 1) - field(*line, 3) / 2});
                }
            }
        }
        auto addWidgets = [&](const char *kind, size_t at) {
            for (auto [it, end] = screen_->settings.equal_range(kind); it != end; ++it) {
                ImageRef widget = it->second.size() > at + 4 ? images_->get(it->second[at]) : nullptr;
                if (widget) spots.push_back({field(it->second, at + 1) + widget->w / field(it->second, at + 3) / 2, field(it->second, at + 2) + widget->h / field(it->second, at + 4) / 2});
            }
        };
        addWidgets("radio", 2);
        addWidgets("check", 1);
        addWidgets("step", 2);
        const auto *size = screen_->setting("cellsize");
        if (size && size->size() >= 2)
            for (auto it = screen_->settings.lower_bound("cell"); it != screen_->settings.upper_bound("cell"); ++it)
                spots.push_back({std::atoi(it->second[0].c_str()) + cellShift() + std::atoi((*size)[0].c_str()) / 2, std::atoi(it->second[1].c_str()) + std::atoi((*size)[1].c_str()) / 2});
    } else {
        return false;
    }
    if (spots.empty()) return false;
    int fromX = pointerSeen_ ? pointerX_ : width_ / 2, fromY = pointerSeen_ ? pointerY_ : height_ / 2;
    const Spot *best = nullptr;
    long bestScore = 0;
    for (const Spot &s : spots) {
        int ax = s.x - fromX, ay = s.y - fromY;
        int along = ax * dx + ay * dy;
        if (along <= 0) continue;
        // Straight ahead counts for far more than off to the side.
        int across = std::abs(ax * dy) + std::abs(ay * dx);
        long score = long(along) + long(across) * 3;
        if (!best || score < bestScore) {
            best = &s;
            bestScore = score;
        }
    }
    if (!best) return true;
    mouseMove((float(best->x) + 0.5f) / float(width_), (float(best->y) + 0.5f) / float(height_));
    return true;
}

void Engine::mouseWheel(int notches) {
    // In the CG viewer the wheel zooms, a twentieth a notch.
    if (screen_ && viewingImage_ && viewerPanel() && !dialogOpen_) {
        viewerZoom(notches);
        return;
    }
    if (screen_ || dialogOpen_) return;
    // With manual animation control on, the wheel zooms a movie showing in the story instead of opening the log.
    if (!logOpen_ && cameraActive()) {
        const auto *zoom = screens_.at("defaults").setting("animzoom");
        cameraZoom(notches * (zoom && zoom->size() >= 3 ? field(*zoom, 2) : 5));
        return;
    }
    if (logOpen_) scrollLog(notches);
    else if (notches > 0) openLog();
}

// A `rise` picture's offset that long after its tab opened, in a straight line between its points, and none past the last.
static int riseOffset(const std::vector<std::pair<int, int>> &points, int64_t ms) {
    if (points.empty() || ms >= points.back().first) return 0;
    if (ms <= points.front().first) return points.front().second;
    for (size_t i = 1; i < points.size(); i++)
        if (ms <= points[i].first) {
            auto [t0, y0] = points[i - 1];
            auto [t1, y1] = points[i];
            return y0 + int(std::lround(double(y1 - y0) * double(ms - t0) / double(std::max(1, t1 - t0))));
        }
    return 0;
}

bool Engine::movieShowing() const {
    for (const auto &[slot, s] : front_)
        if (s.movie && s.image) return true;
    return false;
}

bool Engine::cameraActive() const {
    auto it = screens_.find("defaults");
    return it != screens_.end() && it->second.setting("animpanel") && setting("anim_manual") && !screen_ && movieShowing();
}

bool Engine::cameraPanelShown() const { return cameraActive() && windowShown_ && !windowHiddenByPlayer_ && !logOpen_; }

void Engine::cameraZoom(int hundredths) {
    const auto *zoom = screens_.at("defaults").setting("animzoom");
    int low = zoom && zoom->size() >= 2 ? field(*zoom, 0) : 100, high = zoom && zoom->size() >= 2 ? field(*zoom, 1) : 100;
    movieZoom_ = std::clamp(movieZoom_ + hundredths, low, high);
    dirty_ = true;
}

// The panel's button under a point: 0 zooms out, 1 resets, 2 zooms in; -1 for none.
int Engine::cameraButtonAt(int x, int y) const {
    if (!cameraPanelShown()) return -1;
    const auto *b = screens_.at("defaults").setting("animbuttons");
    if (!b || b->size() < 5) return -1;
    ImageRef sheet = images_->get((*b)[0]);
    if (!sheet) return -1;
    int w = sheet->w / 3, h = sheet->h / 5, bx = field(*b, 1);
    for (int i = 0; i < 3; i++) {
        int by = field(*b, 2 + i);
        if (x >= bx && y >= by && x < bx + w && y < by + h) return i;
    }
    return -1;
}

void Engine::composeCameraPanel(Canvas &out) {
    if (!cameraPanelShown()) return;
    const Screen &d = screens_.at("defaults");
    const auto *panel = d.setting("animpanel"), *buttons = d.setting("animbuttons"), *digits = d.setting("animdigits"), *help = d.setting("animhelp"),
               *zoom = d.setting("animzoom");
    int low = zoom && zoom->size() >= 2 ? field(*zoom, 0) : 100, high = zoom && zoom->size() >= 2 ? field(*zoom, 1) : 100;
    if (ImageRef art = images_->get((*panel)[0]); art && panel->size() >= 3) drawPart(out, *art, field(*panel, 1), field(*panel, 2), 0, 0, art->w, art->h, 1);
    // The zoom as three digits: units, then tenths and hundredths after the panel's own point.
    if (digits && digits->size() >= 5)
        if (ImageRef art = images_->get((*digits)[0])) {
            int w = art->w / 10, value[3] = {movieZoom_ / 100, movieZoom_ / 10 % 10, movieZoom_ % 10};
            for (int i = 0; i < 3; i++) drawPart(out, *art, field(*digits, 1 + i), field(*digits, 4), value[i] * w, 0, w, art->h, 1);
        }
    int hovered = pointerSeen_ ? cameraButtonAt(pointerX_, pointerY_) : -1;
    if (buttons && buttons->size() >= 5)
        if (ImageRef sheet = images_->get((*buttons)[0])) {
            int w = sheet->w / 3, h = sheet->h / 5;
            for (int i = 0; i < 3; i++) {
                // Zooming out or resetting at the smallest zoom, and zooming in at the largest, are drawn disabled.
                bool off = (i < 2 && movieZoom_ <= low) || (i == 2 && movieZoom_ >= high);
                int row = off ? 4 : hovered == i ? 1 : 0;
                drawPart(out, *sheet, field(*buttons, 1), field(*buttons, 2 + i), i * w, row * h, w, h, 1);
            }
        }
    if (hovered >= 0 && help && help->size() >= 3)
        if (ImageRef art = images_->get((*help)[0])) {
            int h = art->h / 3;
            drawPart(out, *art, field(*help, 1), field(*help, 2), 0, hovered * h, art->w, h, 1);
        }
}

namespace {

bool makeFolders(const std::string &path) {
    for (size_t at = 1; at < path.size(); at++) {
        if (path[at] != '/') continue;
        std::string part = path.substr(0, at);
        if (mkdir(part.c_str(), 0777) != 0 && errno != EEXIST) return false;
    }
    return mkdir(path.c_str(), 0777) == 0 || errno == EEXIST;
}

// The name in the folder that matches apart from case, or the name as given. A backup restored from a PC can hold
// CFG.SAV where the game asks for cfg.sav, and an iPad's disk tells them apart.
std::string nameInFolder(const std::string &folder, const std::string &name) {
    struct stat info;
    if (stat((folder + "/" + name).c_str(), &info) == 0) return name;
    DIR *dir = opendir(folder.c_str());
    if (!dir) return name;
    std::string found = name;
    while (dirent *entry = readdir(dir))
        if (strcasecmp(entry->d_name, name.c_str()) == 0) {
            found = entry->d_name;
            break;
        }
    closedir(dir);
    return found;
}

std::string matchCase(const std::string &base, const std::string &relative) {
    std::string path = base;
    for (const std::string &part : split(relative, '/'))
        if (!part.empty()) path += "/" + nameInFolder(path, part);
    return path;
}

std::vector<uint8_t> readWhole(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

// Written beside the target and renamed over it, so a crash can't leave half a save.
bool writeWhole(const std::string &path, const std::vector<uint8_t> &data) {
    std::string temporary = path + ".part";
    {
        std::ofstream f(temporary, std::ios::binary | std::ios::trunc);
        f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
        // A small file stays in the stream's buffer until here, so a full disk only shows now.
        f.flush();
        f.close();
        if (!f) {
            std::remove(temporary.c_str());
            return false;
        }
    }
    return std::rename(temporary.c_str(), path.c_str()) == 0;
}

}  // namespace

// The PC keeps each list by name, so a script's place in it counts in name order, not the file's: The Alchemist's file
// lists GLB_PROLOGUE_OVER (1000) before GLB_ENDING (1050), and its flow's プロローグ完了 names place 1, which is 1000 by
// name, the number a save the PC made after the prologue holds. Test Game 84's flows mark chapter 1A at 2, between
// GLB_01_OVER and GLB_02_OVER, and 94's put GLB_PROLOGUE_OVER, first in its file, after the four GLB_ENDING_s.
MacroLists parseMacroList(const std::vector<uint8_t> &file) {
    std::map<std::string, int> global, scenario, *list = nullptr;
    for (const std::string &raw : split(sjis::toUtf8(std::string(file.begin(), file.end())), '\n')) {
        std::string line = trim(raw.substr(0, raw.find("//")));
        if (line.rfind("#p", 0) == 0) list = line.find("Global") != std::string::npos ? &global : &scenario;
        else if (line == "##") list = nullptr;
        else if (list && !line.empty()) {
            size_t gap = line.find_first_of(" \t");
            if (gap != std::string::npos) list->emplace(line.substr(0, gap), std::atoi(line.c_str() + gap));
        }
    }
    MacroLists out;
    for (const auto &[name, number] : global) out.global.push_back(number);
    for (const auto &[name, number] : scenario) out.scenario.push_back(number);
    return out;
}

// MARK: Galleries

void Engine::loadGalleries() {
    if (Bytes data = files_.read(config_.text("MacroList", "cfg\\macrolist.ini"))) {
        auto lists = parseMacroList(*data);
        globalMacros_ = lists.global;
        scenarioMacros_ = lists.scenario;
    }
    auto lines = [&](const char *path) {
        std::vector<std::string> out;
        Bytes data = files_.read(path);
        if (!data) return out;
        for (const std::string &raw : split(sjis::toUtf8(std::string(data->begin(), data->end())), '\n')) {
            std::string line = trim(raw.substr(0, raw.find("//")));
            if (!line.empty()) out.push_back(line);
        }
        return out;
    };
    auto fields = [](const std::string &line) {
        std::vector<std::string> out;
        std::string word;
        for (char c : line + " ") {
            if (c == ' ' || c == '\t') {
                if (!word.empty()) out.push_back(word);
                word.clear();
            } else {
                word += c;
            }
        }
        return out;
    };
    // `#p page thumbnail 0 bgm flow`, then `0,n,picture` lines, then `##`.
    for (const std::string &line : lines("cfg\\cg_view.ini")) {
        auto f = fields(line);
        if (f.empty()) continue;
        if (f[0] == "#p" && f.size() >= 3) {
            GalleryEntry e;
            e.page = std::atoi(f[1].c_str());
            e.thumbnail = f[2];
            if (f.size() > 4) e.bgm = f[4];
            if (f.size() > 5) e.flow = f[5];  // the replay of the scene it's from
            // Test Game 89's header has no type field: the BGM comes third, and each picture line names the replay.
            if (f.size() == 4 && normalizedPath(f[3]).find(".ogg") != std::string::npos) e.bgm = f[3];
            cgEntries_.push_back(e);
        } else if (f[0] != "##" && !cgEntries_.empty()) {
            auto parts = split(f[0], ',');
            if (parts.size() >= 3) {
                cgEntries_.back().imageIds.push_back(std::atoi(parts[1].c_str()));
                if (parts.size() >= 4 && cgEntries_.back().flow.empty()) cgEntries_.back().flow = trim(parts[3]);
                cgEntries_.back().images.push_back(parts[2]);
            }
        }
    }
    // `#p page thumbnail key bgm flow`, then a `0,0,movie` line for each of the entry's movies, then `##`.
    for (const std::string &line : lines("cfg\\anime_view.ini")) {
        auto f = fields(line);
        if (f.empty()) continue;
        if (f[0] == "#p" && f.size() >= 6) {
            GalleryEntry e;
            e.page = std::atoi(f[1].c_str());
            e.thumbnail = f[2];
            e.keys = {f[3]};
            e.bgm = f[4];
            e.flow = f[5];
            animeEntries_.push_back(e);
        } else if (f[0] != "##" && !animeEntries_.empty()) {
            auto parts = split(f[0], ',');
            if (parts.size() >= 3) animeEntries_.back().images.push_back(parts[2]);
        }
    }
    // `page thumbnail 0 0 flow key,0,0`, with up to three keys and 0 for none. Test Game 84's second scene names two, and
    // on the PC either one read unlocks it.
    for (const std::string &line : lines("cfg\\play_view.ini")) {
        auto f = fields(line);
        if (f.size() < 6) continue;
        GalleryEntry e;
        e.page = std::atoi(f[0].c_str());
        e.thumbnail = f[1];
        e.flow = f[4];
        for (const std::string &key : split(f[5], ','))
            if (!key.empty() && key != "0") e.keys.push_back(key);
        sceneEntries_.push_back(e);
    }
    // cfg\sc_view.ini: `#p 100` lists backgrounds, `#p N face|clothes|pose` heroine N's thumbnails, each section ending
    // `##`. Paths there have their backslashes doubled, and a line's second field marks what comes with a bonus.
    {
        int heroine = -1;
        std::string kind;
        for (const std::string &line : lines("cfg\\sc_view.ini")) {
            auto f = fields(line);
            if (f.empty()) continue;
            if (f[0] == "##") { heroine = -1; continue; }
            if (f[0] == "#p" && f.size() >= 2) {
                heroine = std::atoi(f[1].c_str());
                kind = f.size() >= 3 ? f[2] : "";
                if (heroine >= 0 && heroine < 100 && int(spriteHeroines_.size()) <= heroine) spriteHeroines_.resize(size_t(heroine) + 1);
                continue;
            }
            std::string path = f[0];
            for (size_t at; (at = path.find("\\\\")) != std::string::npos;) path.erase(at, 1);
            int bonus = f.size() >= 2 ? std::atoi(f[1].c_str()) : 0;
            if (heroine == 100) spriteBackgrounds_.push_back({path, bonus});
            else if (heroine >= 0 && heroine < 100) {
                SpriteHeroine &h = spriteHeroines_[size_t(heroine)];
                std::string name = path.substr(path.find_last_of('\\') + 1);
                // clothes_HHH_OOO_000_000_000.bmp: her number and the outfit's.
                if (kind == "clothes" && name.size() >= 15) {
                    h.number = std::atoi(name.substr(8, 3).c_str());
                    h.outfits.push_back({path, std::atoi(name.substr(12, 3).c_str()), bonus});
                } else if (kind == "face") h.faces.push_back(path);
                else if (kind == "pose") h.poses.push_back(path);
            }
        }
        spriteOutfit_.assign(spriteHeroines_.size(), 0);
        spriteLocked_.assign(spriteHeroines_.size(), 0);
    }
    // A track's file, then in Test Game 84 the picture of its title.
    for (const std::string &line : lines("cfg\\sound_view.ini")) {
        auto f = fields(line);
        if (f.empty()) continue;
        tracks_.push_back(f[0]);
        trackTitles_.push_back(f.size() > 1 ? f[1] : "");
    }
    for (const std::string &line : lines("cfg\\change_texture_list.ini")) {
        std::istringstream in(line);
        std::string plain, styled;
        if (in >> plain >> styled && styled.find('.') != std::string::npos) plainArt_[normalizedPath(styled)] = plain;
        // Ore o Hoshigaru Tsuma to Gibo's lines instead name a CG group and whether it has a version with the protagonist
        // see-through and one without pubic hair.
        else if (!plain.empty() && plain.rfind("//", 0) != 0 && !styled.empty() && std::isdigit(uint8_t(styled[0]))) {
            int hair = 0;
            in >> hair;
            artVersions_[normalizedPath(plain)] = {std::atoi(styled.c_str()) != 0, hair != 0};
        }
    }
    // `#type N`, then how the names of that type's voice files start, then `##`. Types from 50 are the background voices
    // of the character 50 below.
    int voiceType = -1;
    for (const std::string &line : lines("cfg\\voice_type.ini")) {
        if (line.rfind("#type", 0) == 0) voiceType = std::atoi(line.c_str() + 5);
        else if (line == "##") voiceType = -1;
        else if (voiceType >= 0) voiceTypes_.emplace_back(normalizedPath(fields(line)[0]), voiceType);
    }
    std::stable_sort(voiceTypes_.begin(), voiceTypes_.end(), [](const auto &a, const auto &b) { return a.first.size() > b.first.size(); });
    // `#anime PICTURES FIRST MS`. Each number is read as ms because an effect of 39 pictures at 16 lasts 0.6 s that way.
    for (const std::string &line : lines("cfg\\anime_cfg.ini")) {
        std::istringstream in(line);
        std::string word, path;
        int count = 0, ms = 0;
        if (in >> word >> count >> path >> ms && word == "#anime" && count > 0 && ms > 0) flipbooks_[normalizedPath(path)] = {count, ms};
    }
    if (!saveDir_.empty()) {
        std::ifstream f(saveDir_ + "/vndegen_seen.txt");
        std::string line;
        while (std::getline(f, line))
            if (!line.empty()) seen_[line] = true;
        loadReadScripts();
        loadReadText();
    }
}

// read.sav lists the scripts the PC has read to their end: a count, then for each its name's length, the name, and a count
// of node ranges read, 8 bytes each. A scene unlocks in the PC's Scene gallery once its script is there, so a PC backup's
// scenes come along. VNDegen doesn't write the file.
void Engine::loadReadScripts() {
    auto packed = readWhole(saveDir_ + "/" + nameInFolder(saveDir_, "read.sav"));
    if (packed.empty()) return;
    auto d = unpackEntry(packed.data(), packed.size());
    size_t pos = 0;
    auto next = [&](int32_t &v) {
        if (pos + 4 > d.size()) return false;
        std::memcpy(&v, &d[pos], 4);
        pos += 4;
        return true;
    };
    int32_t count = 0, length = 0, ranges = 0;
    if (!next(count)) return;
    for (int32_t i = 0; i < count; i++) {
        if (!next(length) || length <= 0 || pos + size_t(length) > d.size()) return;
        std::string name(reinterpret_cast<const char *>(&d[pos]), strnlen(reinterpret_cast<const char *>(&d[pos]), size_t(length)));
        pos += size_t(length);
        if (!next(ranges) || ranges < 0 || pos + size_t(ranges) * 8 > d.size()) return;
        // Each range, first and last node id, is text the PC has read, which Config's read-only skip passes; the list
        // ends (0, 0) then (-2, -2).
        auto &read = readNodes_[readKey(name)];
        // Kept as the PC named it, so writing the file back never drops or renames one of its entries.
        if (!name.empty() && std::find(readScripts_.begin(), readScripts_.end(), name) == readScripts_.end()) readScripts_.push_back(name);
        for (int32_t r = 0; r < ranges; r++) {
            int32_t first = 0, last = 0;
            next(first);
            next(last);
            if (first > 0 && last >= first && last - first < 100000)
                for (int32_t id = first; id <= last; id++) read.insert(id);
        }
        if (!name.empty()) seen_[normalizedPath(name)] = true;
    }
}

// A script's read marks go by its name without a patch's folder: the PC's read.sav can name a patched script
// `sys_5\scr\x.txt` where the flow opens it as `scr\x.txt`, and both are the same file.
std::string Engine::readKey(const std::string &name) { return withoutPatch(normalizedPath(name)); }

// VNDegen's own record of the text read, a script to a line with its node ids, kept beside the PC's read.sav.
void Engine::loadReadText() {
    std::ifstream f(saveDir_ + "/vndegen_read.txt");
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string script;
        int id;
        if (!(in >> script)) continue;
        auto &read = readNodes_[readKey(script)];
        while (in >> id) read.insert(id);
    }
}

void Engine::saveReadText() {
    if (!readChanged_ || saveDir_.empty() || !makeFolders(saveDir_)) return;
    std::string text;
    for (const auto &[script, ids] : readNodes_) {
        text += script;
        for (int id : ids) text += " " + std::to_string(id);
        text += "\n";
    }
    writeWhole(saveDir_ + "/vndegen_read.txt", std::vector<uint8_t>(text.begin(), text.end()));
    writeReadSav();
    readChanged_ = false;
}

// read.sav as the PC writes it, so a backup carries VNDegen's reading to the PC: every script the PC listed, with its
// ranges and any text read here since, then each script read to its end here, which is what unlocks its scene. A
// script's ranges are runs of node ids, ending (0, 0) then (-2, -2).
void Engine::writeReadSav() {
    std::vector<std::string> scripts = readScripts_;
    std::vector<std::string> finished;
    for (const auto &[path, on] : seen_)
        if (on && path.size() > 4 && path.compare(path.size() - 4, 4, ".txt") == 0) finished.push_back(path);
    std::sort(finished.begin(), finished.end());
    for (const std::string &path : finished)
        if (std::none_of(scripts.begin(), scripts.end(), [&](const std::string &s) { return readKey(s) == readKey(path); })) scripts.push_back(path);
    if (scripts.empty()) return;
    std::vector<uint8_t> out;
    auto put = [&](int32_t v) { out.insert(out.end(), reinterpret_cast<uint8_t *>(&v), reinterpret_cast<uint8_t *>(&v) + 4); };
    put(int32_t(scripts.size()));
    for (const std::string &script : scripts) {
        put(int32_t(script.size()));
        out.insert(out.end(), script.begin(), script.end());
        std::vector<std::pair<int32_t, int32_t>> runs;
        if (auto it = readNodes_.find(readKey(script)); it != readNodes_.end())
            for (int id : it->second) {
                if (!runs.empty() && runs.back().second == id - 1) runs.back().second = id;
                else runs.push_back({id, id});
            }
        runs.push_back({0, 0});
        runs.push_back({-2, -2});
        put(int32_t(runs.size()));
        for (auto [first, last] : runs) {
            put(first);
            put(last);
        }
    }
    auto packed = packEntry(out);
    if (!packed.empty()) writeWhole(saveDir_ + "/" + nameInFolder(saveDir_, "read.sav"), packed);
}

// Config's read-only skip stops skipping at text not read before, as the PC's does; then the text is read.
void Engine::noteTextRead(const Node &n) {
    if (!scene_.script) return;
    // run() has just marked it, so it was fresh when that mark was new.
    auto &read = readNodes_[readKey(scene_.script->name)];
    bool fresh = textFresh_ || read.insert(n.id).second;
    textFresh_ = false;
    if (fresh) readChanged_ = true;
    if (!fresh || setting("skipall") || jumping_) return;
    skipToggle_ = false;
    if (skip_) {
        skip_ = false;
        skipStopped_ = true;
    }
}

// The next track the player has heard, going either way along the list, or any of them on shuffle. -1 when there's none.
int Engine::nextTrack(int direction) {
    std::vector<int> heard;
    for (int i = 0; i < int(tracks_.size()); i++)
        if (seen(tracks_[size_t(i)])) heard.push_back(i);
    if (heard.empty()) return -1;
    if (trackShuffle_) return heard[size_t(std::rand()) % heard.size()];
    if (currentTrack_ < 0) return direction > 0 ? heard.front() : heard.back();
    auto at = std::lower_bound(heard.begin(), heard.end(), currentTrack_);
    if (direction > 0) {
        if (at != heard.end() && *at == currentTrack_) ++at;
        return at == heard.end() ? heard.front() : *at;
    }
    return at == heard.begin() ? heard.back() : *(at - 1);
}

// The title's or the story's music waits while the player plays, and while the host has the game paused.
// The title's and the story's music wait while the session is paused or the music screen or CG viewer plays its own,
// and the viewer's waits while the session is paused.
void Engine::holdMusic() {
    bool hold = paused_ || trackPlayer_ || viewMusic_ || spriteMusic_;
    if (titleMusic_) audio_.pause(titleMusic_, hold);
    for (auto &[channel, player] : bgm_) audio_.pause(player, hold);
    if (viewMusic_) audio_.pause(viewMusic_, paused_);
}

void Engine::stopTrack() {
    if (!trackPlayer_) return;
    audio_.close(trackPlayer_);
    trackPlayer_ = 0;
    trackPaused_ = false;
    holdMusic();
}

void Engine::playTrack(int index) {
    if (index < 0 || size_t(index) >= tracks_.size() || !seen(tracks_[size_t(index)])) return;
    // Straight to the next track, with the other music still held.
    if (trackPlayer_) audio_.close(trackPlayer_);
    trackPlayer_ = 0;
    trackPaused_ = false;
    Bytes data = files_.read(tracks_[size_t(index)]);
    trackPlayer_ = data ? audio_.open(data) : 0;
    holdMusic();
    if (!trackPlayer_) return;
    currentTrack_ = index;
    // Its own group, for the music screen's own volume.
    audio_.setGroup(trackPlayer_, 3);
    audio_.setLoops(trackPlayer_, trackRepeat_ ? 0 : 1);
    audio_.play(trackPlayer_);
}

void Engine::noteSeen(const std::string &path) {
    if (path.empty()) return;
    std::string key = normalizedPath(path);
    if (seen_[key]) return;
    seen_[key] = true;
    seenChanged_ = true;
    // A script read to its end goes into read.sav too.
    if (key.size() > 4 && key.compare(key.size() - 4, 4, ".txt") == 0) readChanged_ = true;
}

// A card's picture. Ore o Hoshigaru Tsuma to Gibo's ini names `obj\small\cg\...` for pictures whose full versions are in
// cg2.pak, and its exe, traced under Wine, reads them from `obj\small\cg2\...`.
ImageRef Engine::galleryThumbnail(const std::string &path) const {
    if (ImageRef thumb = images_->get(path)) return thumb;
    std::string name = normalizedPath(path);
    auto at = name.find("\\small\\cg\\");
    if (at == std::string::npos) return nullptr;
    return images_->get(name.replace(at, 10, "\\small\\cg2\\"));
}

// `drift PICTURE X FROMY TOY`, one for each sketch in turn, and `driftmotion SPEED FADE GAP ALPHA`: Test Games 92 and 93's
// music screens show line art that pans down at SPEED pixels a second from FROMY to TOY, fading in and out over FADE ms
// at its ends, drawn at ALPHA of 256, then after GAP ms the next sketch, round and round. Measured under Wine.
void Engine::composeDrift(Canvas &out) {
    const auto *motion = screen_->setting("driftmotion");
    if (!motion || motion->size() < 4) return;
    std::vector<const std::vector<std::string> *> sketches;
    for (auto [it, end] = screen_->settings.equal_range("drift"); it != end; ++it)
        if (it->second.size() >= 4) sketches.push_back(&it->second);
    if (sketches.empty()) return;
    double speed = std::max(1, field(*motion, 0)), fade = std::max(1, field(*motion, 1)), gap = field(*motion, 2), alpha = field(*motion, 3) / 256.0;
    double at = double(screenMs_ - driftFrom_);
    // Whole rounds first, so a long stay on the screen costs nothing.
    double round = 0;
    for (const auto *s : sketches) round += std::abs(field(*s, 3) - field(*s, 2)) / speed * 1000 + gap;
    if (round <= 0) return;
    at = std::fmod(at, round);
    for (size_t i = 0; i < sketches.size(); i++) {
        const auto &s = *sketches[(driftFirst_ + i) % sketches.size()];
        double from = field(s, 2), to = field(s, 3), shown = std::abs(to - from) / speed * 1000;
        if (at < shown) {
            ImageRef art = images_->get(s[0]);
            if (!art) return;
            double y = from + (to > from ? 1 : -1) * speed * at / 1000;
            double look = std::min({1.0, at / fade, (shown - at) / fade});
            drawPart(out, *art, field(s, 1), int(std::floor(y)), 0, 0, art->w, art->h, float(alpha * look));
            return;
        }
        at -= shown;
        if (at < gap) return;
        at -= gap;
    }
}

// A patch's own name for a file, `sys_5\cg\x.bmp`, is the same file.
bool Engine::isCgPath(const std::string &path) const { return withoutPatch(normalizedPath(path)).rfind("cg\\", 0) == 0; }

// A patch's folder, `sys_N\`, dropped from the front of a normalized path.
std::string Engine::withoutPatch(const std::string &key) {
    size_t slash = key.find('\\');
    if (key.rfind("sys_", 0) != 0 || slash == std::string::npos || slash == 4) return key;
    for (size_t i = 4; i < slash; i++)
        if (!std::isdigit(static_cast<unsigned char>(key[i]))) return key;
    return key.substr(slash + 1);
}

bool Engine::isCgSlot(int slot) const {
    for (const Sprites *sprites : {&back_, &front_})
        if (auto it = sprites->find(slot); it != sprites->end() && isCgPath(it->second.path)) return true;
    return false;
}

// `cellshift TAB DX`: how far across a gallery tab moves its cards, 80 pixels a tab either side of the middle in Test
// Games 92 and 93.
int Engine::cellShift() const {
    if (!screen_) return 0;
    for (auto [it, end] = screen_->settings.equal_range("cellshift"); it != end; ++it)
        if (it->second.size() >= 2 && field(it->second, 0) == galleryTab_) return field(it->second, 1);
    return 0;
}

const std::vector<Engine::GalleryEntry> &Engine::galleryEntries() const {
    if (screen_ && screen_->name == "scene") return sceneEntries_;
    if (screen_ && screen_->name == "anime") return animeEntries_;
    return cgEntries_;
}

// A picture unlocks once seen. An anime entry's movies unlock together with its scene's script.
bool Engine::galleryImageSeen(const GalleryEntry &entry, size_t image) const {
    if (screen_ && screen_->name == "anime") return sceneUnlocked(entry);
    return image < entry.images.size() && seen(entry.images[image]);
}

bool Engine::sceneUnlocked(const GalleryEntry &scene) const {
    return std::any_of(scene.keys.begin(), scene.keys.end(), [&](const std::string &key) { return seen(key); });
}

// A patched script is seen under its patch's name or the plain one, whichever the save that reached it used, as both
// open the same file.
bool Engine::seen(const std::string &path) const {
    std::string key = normalizedPath(path);
    for (const std::string &name : {key, withoutPatch(key), normalizedPath(files_.savedName(key))}) {
        auto it = seen_.find(name);
        if (it != seen_.end() && it->second) return true;
    }
    return false;
}

void Engine::saveSeen() {
    if (!seenChanged_ || saveDir_.empty() || !makeFolders(saveDir_)) return;
    std::string text;
    for (const auto &[path, on] : seen_)
        if (on) text += path + "\n";
    writeWhole(saveDir_ + "/vndegen_seen.txt", std::vector<uint8_t>(text.begin(), text.end()));
    saveGlobals();
    seenChanged_ = false;
}

int Engine::galleryCellAt(int x, int y) const {
    if (!screen_ || (!isPictureGallery() && screen_->name != "scene" && screen_->name != "music")) return -1;
    const auto *size = screen_->setting("cellsize");
    if (!size || size->size() < 2) return -1;
    int w = std::atoi((*size)[0].c_str()), h = std::atoi((*size)[1].c_str());
    int index = 0;
    for (auto it = screen_->settings.lower_bound("cell"); it != screen_->settings.upper_bound("cell"); ++it, index++) {
        int cx = std::atoi(it->second[0].c_str()) + cellShift(), cy = std::atoi(it->second[1].c_str());
        if (x >= cx && y >= cy && x < cx + w && y < cy + h) return index;
    }
    return -1;
}

// Cells run in the file's order, a page at a time. Cells not unlocked yet stay locked, as the PC's do.
int Engine::galleryEntry(int cell) const {
    const auto &entries = galleryEntries();
    int perPage = int(screen_->settings.count("cell"));
    int wanted = page_ * perPage + cell;
    if (!screen_->settings.count("gallerytabs")) return wanted < int(entries.size()) ? wanted : -1;
    for (int i = 0; i < int(entries.size()); i++)
        if (entries[size_t(i)].page == galleryTab_ && wanted-- == 0) return i;
    return -1;
}

// How many pages a character's gallery tab fills. The buttons for pages past those aren't there.
bool Engine::galleryPageExists(int page) const {
    if (!screen_ || (!isPictureGallery() && screen_->name != "scene") || !screen_->settings.count("gallerytabs")) return true;
    const auto &entries = galleryEntries();
    int count = int(std::count_if(entries.begin(), entries.end(), [&](const GalleryEntry &e) { return e.page == galleryTab_; }));
    return page == 0 || page * int(screen_->settings.count("cell")) < count;
}

// Test Game 84 lists its music as cards: a card and a title for a track that's been heard, a blank card for one that hasn't.
void Engine::composeTrackCards(Canvas &out) {
    if (screen_->name != "music" || !screen_->settings.count("cell")) return;
    const auto *card = screen_->setting("cellcard"), *blank = screen_->setting("cellempty"), *title = screen_->setting("celltitle"), *frame = screen_->setting("cellframe");
    ImageRef cardArt = card && !card->empty() ? images_->get((*card)[0]) : nullptr, blankArt = blank && blank->size() >= 3 ? images_->get((*blank)[0]) : nullptr;
    ImageRef frameArt = frame && frame->size() >= 3 ? images_->get((*frame)[0]) : nullptr;
    int perPage = int(screen_->settings.count("cell")), index = 0;
    int hovered = pointerSeen_ ? galleryCellAt(pointerX_, pointerY_) : -1;
    for (auto it = screen_->settings.lower_bound("cell"); it != screen_->settings.upper_bound("cell"); ++it, index++) {
        int cx = field(it->second, 0), cy = field(it->second, 1), track = page_ * perPage + index;
        bool heard = track < int(tracks_.size()) && seen(tracks_[size_t(track)]);
        if (!heard) {
            if (blankArt) drawPart(out, *blankArt, cx + field(*blank, 1), cy + field(*blank, 2), 0, 0, blankArt->w, blankArt->h, 1);
            continue;
        }
        if (cardArt) drawPart(out, *cardArt, cx, cy, 0, 0, cardArt->w, cardArt->h, 1);
        if (ImageRef name = title && title->size() >= 2 ? images_->get(trackTitles_[size_t(track)]) : nullptr) drawPart(out, *name, cx + field(*title, 0), cy + field(*title, 1), 0, 0, name->w, name->h, 1);
        bool playing = track == currentTrack_ && trackPlayer_;
        if (frameArt && frameArt->h >= 5 && (playing || index == hovered)) drawPart(out, *frameArt, cx + field(*frame, 1), cy + field(*frame, 2), 0, (playing ? 2 : 1) * (frameArt->h / 5), frameArt->w, frameArt->h / 5, 1);
    }
}

// `tracktime DIGITS X Y X2 Y2 STEP` where the time played and the length go, with the digits STEP apart since the PC's
// overlap their cells, and `trackseek FILL X Y` how far through it is.
void Engine::composeTrackTime(Canvas &out) {
    if (screen_->name != "music") return;
    const auto *times = screen_->setting("tracktime");
    int64_t at = trackPlayer_ ? audio_.positionMs(trackPlayer_) : 0, all = trackPlayer_ ? audio_.lengthMs(trackPlayer_) : 0;
    if (ImageRef digits = times && times->size() >= 6 ? images_->get((*times)[0]) : nullptr) {
        int w = digits->w / 11, step = field(*times, 5);
        auto clock = [&](int64_t ms, int x, int y) {
            int seconds = int(ms / 1000), shown[5] = {seconds / 600 % 10, seconds / 60 % 10, 10, seconds / 10 % 6, seconds % 10};
            for (int i = 0; i < 5; i++) drawPart(out, *digits, x + i * step, y, shown[i] * w, 0, w, digits->h, 1);
        };
        clock(at, field(*times, 1), field(*times, 2));
        clock(all, field(*times, 3), field(*times, 4));
    }
    const auto *seek = screen_->setting("trackseek");
    if (ImageRef fill = seek && seek->size() >= 3 && all > 0 ? images_->get((*seek)[0]) : nullptr) drawPart(out, *fill, field(*seek, 1), field(*seek, 2), 0, 0, int(fill->w * std::min(at, all) / all), fill->h, 1);
}

void Engine::composeGallery(Canvas &out) {
    if (viewingImage_) {
        out.clear();
        composeViewerPicture(out);
        composeViewer(out);
        return;
    }
    if (!isPictureGallery() && screen_->name != "scene") return;
    const auto *sheetName = screen_->setting("cellsheet");
    ImageRef sheet = sheetName && !sheetName->empty() ? images_->get((*sheetName)[0]) : nullptr;
    const auto *inset = screen_->setting("cellthumb");
    int dx = inset && inset->size() >= 2 ? std::atoi((*inset)[0].c_str()) : 0, dy = inset && inset->size() >= 2 ? std::atoi((*inset)[1].c_str()) : 0;
    const auto &entries = galleryEntries();
    int index = 0;
    int hovered = pointerSeen_ ? galleryCellAt(pointerX_, pointerY_) : -1;
    int perPage = int(screen_->settings.count("cell"));
    for (auto it = screen_->settings.lower_bound("cell"); it != screen_->settings.upper_bound("cell"); ++it, index++) {
        int cx = std::atoi(it->second[0].c_str()) + cellShift(), cy = std::atoi(it->second[1].c_str());
        int found = galleryEntry(index);
        // Test Game 85 leaves out the cards past a tab's last entry.
        if (found < 0 && screen_->settings.count("cellhide")) continue;
        size_t entry = found < 0 ? entries.size() : size_t(found);
        bool unlocked = false;
        if (entry < entries.size()) {
            const GalleryEntry &e = entries[entry];
            if (screen_->name == "scene") unlocked = sceneUnlocked(e);
            else
                for (size_t i = 0; i < e.images.size() && !unlocked; i++) unlocked = galleryImageSeen(e, i);
        }
        if (sheet) {
            int h = sheet->h / 5;
            int row = !unlocked ? int(StateDisabled) : index == hovered ? int(StateHover) : int(StateNormal);
            drawPart(out, *sheet, cx, cy, 0, row * h, sheet->w, h, 1);
        }
        // Or a plain card with a blank one for what's locked, and a frame for the hovered one, as in Test Game 84.
        const auto *card = screen_->setting("cellcard"), *blank = screen_->setting("cellempty"), *frame = screen_->setting("cellframe");
        if (entry < entries.size() && unlocked && card && !card->empty()) {
            if (ImageRef art = images_->get((*card)[0])) drawPart(out, *art, cx, cy, 0, 0, art->w, art->h, 1);
        } else if (entry < entries.size() && blank && blank->size() >= 3) {
            if (ImageRef art = images_->get((*blank)[0])) drawPart(out, *art, cx + field(*blank, 1), cy + field(*blank, 2), 0, 0, art->w, art->h, 1);
        }
        if (unlocked)
            if (ImageRef thumb = galleryThumbnail(entries[entry].thumbnail)) drawPart(out, *thumb, cx + dx, cy + dy, 0, 0, thumb->w, thumb->h, 1);
        if (unlocked && index == hovered && frame && frame->size() >= 3)
            if (ImageRef art = images_->get((*frame)[0]); art && art->h >= 5) drawPart(out, *art, cx + field(*frame, 1), cy + field(*frame, 2), 0, art->h / 5, art->w, art->h / 5, 1);
        // A card's number, faint on a locked card in some games, and how many of a CG's pictures have been seen.
        const auto *lockedAlpha = screen_->setting("celllocked");
        float faint = unlocked || !lockedAlpha ? 1 : float(field(*lockedAlpha, 0)) / 100;
        if (const auto *label = screen_->setting("celllabel"); label && label->size() >= 3)
            if (ImageRef no = images_->get((*label)[0])) drawPart(out, *no, cx + field(*label, 1), cy + field(*label, 2), 0, 0, no->w, no->h, faint);
        if (const auto *digits = screen_->setting("celldigits"); digits && digits->size() >= 5)
            if (ImageRef sheetOfTen = images_->get((*digits)[0])) {
                int w = sheetOfTen->w / 10, count = field(*digits, 3), step = field(*digits, 4), value = page_ * perPage + index + 1;
                for (int i = count - 1; i >= 0; i--, value /= 10)
                    drawPart(out, *sheetOfTen, cx + field(*digits, 1) + i * step, cy + field(*digits, 2), value % 10 * w, 0, w, sheetOfTen->h, faint);
            }
        const auto *counts = screen_->setting("cellcount");
        if (unlocked && counts && counts->size() >= 6 && screen_->name == "cg") {
            // `cellcount DIGITS SLASH X Y SLASHX TOTALX [SLASHY] [CELLS]`. A SLASH of `-` is the digit sheet's cell after 9, as
            // in Test Game 84, whose sheet of CELLS also holds a colon.
            bool slashInSheet = (*counts)[1] == "-";
            ImageRef sheetOfTen = images_->get((*counts)[0]), slash = slashInSheet ? nullptr : images_->get((*counts)[1]);
            const auto &pictures = entries[entry].images;
            int have = int(std::count_if(pictures.begin(), pictures.end(), [&](const std::string &p) { return seen(p); })), all = int(pictures.size());
            if (sheetOfTen && (slash || slashInSheet)) {
                int cells = slashInSheet && counts->size() >= 8 ? std::max(11, field(*counts, 7)) : slashInSheet ? 11 : 10;
                int w = sheetOfTen->w / cells, x = cx + field(*counts, 2), y = cy + field(*counts, 3);
                auto two = [&](int value, int at) {
                    drawPart(out, *sheetOfTen, at, y, value / 10 % 10 * w, 0, w, sheetOfTen->h, 1);
                    drawPart(out, *sheetOfTen, at + w, y, value % 10 * w, 0, w, sheetOfTen->h, 1);
                };
                two(have, x);
                int slashY = y + (counts->size() >= 7 ? field(*counts, 6) : slashInSheet ? 0 : -1);
                if (slashInSheet) drawPart(out, *sheetOfTen, cx + field(*counts, 4), slashY, 10 * w, 0, w, sheetOfTen->h, 1);
                else drawPart(out, *slash, cx + field(*counts, 4), slashY, 0, 0, slash->w, slash->h, 1);
                // `celltotal DIGITS DY`: the total in a sheet of its own, DY lower, as Test Game 89 draws it.
                if (const auto *total = screen_->setting("celltotal"); total && total->size() >= 2) {
                    if (ImageRef totalDigits = images_->get((*total)[0])) {
                        int tw = totalDigits->w / 10, tx = cx + field(*counts, 5), ty = y + field(*total, 1);
                        drawPart(out, *totalDigits, tx, ty, all / 10 % 10 * tw, 0, tw, totalDigits->h, 1);
                        drawPart(out, *totalDigits, tx + tw, ty, all % 10 * tw, 0, tw, totalDigits->h, 1);
                    }
                } else {
                    two(all, cx + field(*counts, 5));
                }
            }
        }
    }
}

void Engine::openGalleryCell(int index) {
    int found = galleryEntry(index);
    if (found < 0) return;
    size_t entry = size_t(found);
    if (isPictureGallery()) {
        if (entry >= galleryEntries().size()) return;
        if (viewerPanel()) {
            openViewer(entry);
            return;
        }
        viewing_.clear();
        for (const std::string &p : cgEntries_[entry].images)
            if (seen(p)) viewing_.push_back(p);
        if (viewing_.empty()) return;
        viewingIndex_ = 0;
        viewingImage_ = true;
        dirty_ = true;
    } else if (entry < sceneEntries_.size() && sceneUnlocked(sceneEntries_[entry])) {
        startReplay(sceneEntries_[entry]);
    }
}

// A replay runs its own flow, and ending it comes back to the gallery it started from.
bool Engine::startReplay(const GalleryEntry &scene) {
    auto flow = loadScript(scene.flow);
    if (!flow || !flow->first()) return false;
    stopTrack();
    if (titleMusic_) {
        audio_.fade(titleMusic_, 500, 0, true);
        audio_.retire(titleMusic_);
        titleMusic_ = 0;
    }
    screen_ = nullptr;
    viewingImage_ = false;
    viewAuto_ = false;
    // The anime viewer's movie and its sound go with it; coming back opens the picture afresh.
    stopViewerMovie();
    stack_.clear();
    scene_ = {};
    back_.clear();
    front_.clear();
    // A new run has no scene of its own yet, so a save before its first names none.
    lastScene_.clear();
    flowCallId_ = 0;
    stack_.push_back({flow, flow->first()});
    replaying_ = true;
    waiting_ = Waiting::None;
    dirty_ = true;
    return true;
}

// The gallery tab the viewer returns to.
void Engine::viewerTab(int page) { galleryTab_ = page; }

// MARK: Settings

int Engine::setting(const std::string &name) const {
    // Config's screen mode is the host window's own state, not a stored setting.
    if (name == "screen") return fullScreen_ ? 1 : 0;
    auto it = settings_.find(name);
    return it == settings_.end() ? 0 : it->second;
}

// What Config starts from, with any a game's table changes.
std::map<std::string, int> Engine::startingSettings() const {
    std::map<std::string, int> out = defaultSettings();
    for (const auto &[start, type] : voiceTypes_) out["voice" + std::to_string(type % 50)] = 1;
    // 0 for the base archives' art, 1 for the art set's.
    if (!artSet_.empty()) out["art"] = 0;
    // The protagonist drawn whole and pubic hair shown, where a game has the other versions.
    if (!artVersions_.empty()) out["hero"] = out["hair"] = 1;
    auto table = screens_.find("defaults");
    if (table != screens_.end())
        for (auto [it, end] = table->second.settings.equal_range("default"); it != end; ++it)
            if (it->second.size() >= 2) out[it->second[0]] = field(it->second, 1);
    return out;
}

// Voices already playing follow the Individual Voices boxes too: one whose speaker is unchecked goes quiet, and plays on
// if the box is checked again or Undo puts it back.
void Engine::applyVoiceChoices() {
    std::map<int, std::string> playing;
    auto follow = [&](int player) {
        auto file = voiceFiles_.find(player);
        if (!player || file == voiceFiles_.end()) return;
        audio_.setGroup(player, voiceAllowed(file->second) ? soundGroup(kKindVoice, file->second) : kMutedVoices);
        playing.insert(*file);
    };
    follow(voicePlayer_);
    for (const auto &[channel, player] : voice_) follow(player);
    voiceFiles_ = std::move(playing);
}

void Engine::applySettings() {
    int master = setting("mute_master") ? 0 : setting("master");
    auto loud = [&](const char *name, const std::string &mute) { return setting(mute) ? 0 : setting(name) * master / 100; };
    audio_.setGroupVolume(0, loud("bgm", "mute_bgm"));
    audio_.setGroupVolume(1, loud("se", "mute_se"));
    audio_.setGroupVolume(2, loud("voice", "mute_voice"));
    audio_.setGroupVolume(kGroupHse, loud("hse", "mute_hse"));
    audio_.setGroupVolume(kGroupBackVoice, loud("backvoice", "mute_backvoice"));
    audio_.setGroupVolume(3, setting("music") * master / 100);
    audio_.setGroupVolume(kMutedVoices, 0);
    applyVoiceChoices();
    static const int kCharMs[] = {40, 20, 8, 0};
    charMs_ = kCharMs[std::clamp(setting("text"), 0, 3)];
    static const int kAutoPercent[] = {200, 100, 50, 0};
    autoDelayMs_ = kAutoDelayMs * kAutoPercent[std::clamp(setting("auto"), 0, 3)] / 100;
    autoPerCharMs_ = -1;
    // `textcurve SETTING A B` and `autocurve SETTING A B` under `screen defaults`: a 0 to 100 slider's text speed as
    // A - B x value ms a character, and auto's pause after a line as that many ms for each of its characters, as Test
    // Game 82's are (set 2, timed under Wine: 167 - 3v and 100 - v).
    if (auto defaults = screens_.find("defaults"); defaults != screens_.end()) {
        if (const auto *curve = defaults->second.setting("textcurve"); curve && curve->size() >= 3)
            charMs_ = std::max(0, field(*curve, 1) - field(*curve, 2) * setting((*curve)[0]));
        // `textpoints SETTING V MS V MS...`: the same by straight lines between measured points, as Test Game 87's text
        // speed, which isn't a line (set 1, under Wine).
        if (const auto *points = defaults->second.setting("textpoints"); points && points->size() >= 5) {
            double v = setting((*points)[0]), ms = std::atof((*points)[2].c_str());
            for (size_t i = 1; i + 3 < points->size(); i += 2) {
                double v0 = std::atof((*points)[i].c_str()), m0 = std::atof((*points)[i + 1].c_str());
                double v1 = std::atof((*points)[i + 2].c_str()), m1 = std::atof((*points)[i + 3].c_str());
                if (v >= v0 && v <= v1 && v1 > v0) { ms = m0 + (m1 - m0) * (v - v0) / (v1 - v0); break; }
                if (v > v1) ms = m1;
            }
            charMs_ = std::max(0, int(std::lround(ms)));
        }
        if (const auto *curve = defaults->second.setting("autocurve"); curve && curve->size() >= 3)
            autoPerCharMs_ = std::max(0, field(*curve, 1) - field(*curve, 2) * setting((*curve)[0]));
    }
    if (!windowAlwaysClear_) windowOpacity_ = float(setting("opacity")) / 100;
    applyArtSwitch();
    applyArtSet();
}

// The PC swaps every CG and sprite on screen the moment the art set changes.
void Engine::applyArtSet() {
    std::string wanted = setting("art") ? artSet_ : "";
    if (wanted == artShown_) return;
    artShown_ = wanted;
    images_->setArtSet(wanted);
    for (Sprites *sprites : {&back_, &front_})
        for (auto &[slot, sprite] : *sprites)
            if (!sprite.path.empty() && !sprite.flipCount)
                if (ImageRef image = images_->get(shownPath(sprite.path))) sprite.image = image;
    // A transition and motion blur hold frames drawn with the old art, so they end here.
    transitioning_ = false;
    ruleImage_ = nullptr;
    previousValid_ = false;
    dirty_ = true;
}

void Engine::loadSettings() {
    settings_ = startingSettings();
    if (!saveDir_.empty()) {
        std::ifstream f(saveDir_ + "/vndegen_settings.txt");
        std::string name;
        int value;
        while (f >> name >> value)
            settings_[name] = value;
    }
    applySettings();
}

void Engine::saveSettings() {
    if (saveDir_.empty() || !makeFolders(saveDir_)) return;
    std::string text;
    for (const auto &[name, value] : settings_) text += name + " " + std::to_string(value) + "\n";
    writeWhole(saveDir_ + "/vndegen_settings.txt", std::vector<uint8_t>(text.begin(), text.end()));
}

int Engine::clampSetting(const std::string &name, int value) const {
    int low = 0, high = 100;
    if (screen_)
        for (auto [it, end] = screen_->settings.equal_range("range"); it != end; ++it)
            if (it->second.size() >= 3 && it->second[0] == name) {
                low = field(it->second, 1);
                high = field(it->second, 2);
            }
    return std::clamp(value, low, high);
}

void Engine::changeSetting(const std::string &name, int value) {
    settings_[name] = value;
    applySettings();
    dirty_ = true;
}

// Table lines: `slider NAME FILL X Y [FULL]` with the value that fills it, 100 unless given, `number NAME DIGITS X Y
// [STEP | X2 X3]`, `range NAME MIN MAX` for what a slider or step can set, 0 to 100 unless given, `bar FILL X Y PERCENT` for one that can't change,
// `radio NAME VALUE SHEET X Y COLS ROWS COL`, `check NAME SHEET X Y COLS ROWS COL [ON ONHOVER OFF OFFHOVER]` and
// `step NAME DELTA SHEET X Y COLS ROWS COL`.
// Sheets run normal, hover, selected, selected and hovered, as the PC's do.
// `readswap SCRIPT FROM TO`: once that script has been read to its end, a settings widget's sheet FROM is drawn from TO,
// as Moral Orc's Individual Voices box reads ???? until the story names Fiona.
std::string Engine::readSwapped(const std::string &sheet) const {
    if (!screen_) return sheet;
    for (auto [it, end] = screen_->settings.equal_range("readswap"); it != end; ++it)
        if (it->second.size() >= 3 && normalizedPath(it->second[1]) == normalizedPath(sheet) && seen(it->second[0])) return it->second[2];
    return sheet;
}

void Engine::composeSettingsWidgets(Canvas &out) {
    auto lines = [&](const char *kind) { return screen_->settings.equal_range(kind); };
    // `rows` gives the sheet's rows for on, on and hovered, off, and off and hovered, when they aren't in the usual order.
    auto cell = [&](const std::vector<std::string> &f, size_t at, bool on, const int *rows = nullptr) {
        ImageRef sheet = images_->get(readSwapped(f[at]));
        if (!sheet) return;
        int x = field(f, at + 1), y = field(f, at + 2), w = sheet->w / field(f, at + 3), h = sheet->h / field(f, at + 4);
        bool over = pointerSeen_ && !dialogOpen_ && inRect(pointerX_, pointerY_, x, y, w, h);
        int row = rows ? rows[(on ? 0 : 2) + (over ? 1 : 0)] : (on ? 2 : 0) + (over ? 1 : 0);
        drawPart(out, *sheet, x, y, field(f, at + 5) * w, row * h, w, h, 1);
    };
    // The PC rounds a fill to the nearest pixel: at 60, The Alchemist's 441-pixel opacity bar fills 265.
    for (auto [it, end] = lines("slider"); it != end; ++it) {
        const auto &f = it->second;
        int scale = f.size() >= 5 ? std::max(1, field(f, 4)) : 100;
        if (ImageRef fill = f.size() >= 4 ? images_->get(f[1]) : nullptr)
            drawPart(out, *fill, field(f, 2), field(f, 3), 0, 0, (fill->w * setting(f[0]) + scale / 2) / scale, fill->h, 1);
    }
    // `knob NAME SHEET X0 Y STEP`: a knob that sits STEP pixels further on for each unit, as Test Game 87's rose on its
    // track (set 1).
    for (auto [it, end] = lines("knob"); it != end; ++it) {
        const auto &f = it->second;
        if (ImageRef knob = f.size() >= 5 ? images_->get(f[1]) : nullptr)
            drawPart(out, *knob, field(f, 2) + field(f, 4) * setting(f[0]), field(f, 3), 0, 0, knob->w, knob->h, 1);
    }
    for (auto [it, end] = lines("bar"); it != end; ++it) {
        const auto &f = it->second;
        if (ImageRef fill = f.size() >= 4 ? images_->get(f[0]) : nullptr)
            drawPart(out, *fill, field(f, 1), field(f, 2), 0, 0, fill->w * field(f, 3) / 100, fill->h, 1);
    }
    for (auto [it, end] = lines("number"); it != end; ++it) {
        const auto &f = it->second;
        // `number NAME text RIGHT BASELINE SIZE STEP R G B OR OG OB`: the value as text in face 0 (MS Gothic's stand-in), its
        // digits STEP apart and the last one's cell ending at RIGHT, with no leading zeros and a one-pixel outline, as Test
        // Game 82's Config draws its numbers, from no picture.
        if (f.size() >= 12 && f[1] == "text") {
            std::string digits = std::to_string(setting(f[0]));
            int size = field(f, 4), step = field(f, 5), left = field(f, 2) - step * int(digits.size()), baseline = field(f, 3);
            for (int pass = 0; pass < 2; pass++)
                for (size_t i = 0; i < digits.size(); i++) {
                    GlyphImage g;
                    if (!glyph(0, uint32_t(digits[i]), size, false, g) || g.w <= 0 || g.h <= 0) continue;
                    // Each digit sits in the middle of its step, as the PC's wider spacing leaves it.
                    int x = left + int(i) * step + (step - g.advance) / 2 + g.left, y = baseline - g.top;
                    if (pass == 0) {
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++)
                                if (dx || dy) drawCoverage(out, g.coverage.data(), g.w, g.h, x + dx, y + dy, field(f, 9), field(f, 10), field(f, 11), 256);
                    } else {
                        drawCoverage(out, g.coverage.data(), g.w, g.h, x, y, field(f, 6), field(f, 7), field(f, 8), 256);
                    }
                }
            continue;
        }
        ImageRef digits = f.size() >= 4 ? images_->get(f[1]) : nullptr;
        if (!digits) continue;
        // A STEP spaces the digits apart, and X2 X3 place the second and third, as round the point a panel prints in a wait
        // time of ×1.00.
        int w = digits->w / 10, value = setting(f[0]), step = f.size() >= 5 ? field(f, 4) : w;
        int xs[3] = {field(f, 2), field(f, 2) + step, field(f, 2) + 2 * step};
        if (f.size() >= 6) {
            xs[1] = field(f, 4);
            xs[2] = field(f, 5);
        }
        for (int i = 2; i >= 0; i--, value /= 10) drawPart(out, *digits, xs[i], field(f, 3), value % 10 * w, 0, w, digits->h, 1);
    }
    for (auto [it, end] = lines("radio"); it != end; ++it) {
        const auto &f = it->second;
        if (f.size() < 8) continue;
        // Screen mode is disabled where the host has no full screen, as on an iPhone or iPad.
        if (f[0] == "screen" && !canFullScreen_) {
            if (ImageRef sheet = images_->get(f[2])) {
                int w = sheet->w / std::max(1, field(f, 5)), h = sheet->h / std::max(1, field(f, 6));
                drawPart(out, *sheet, field(f, 3), field(f, 4), field(f, 7) * w, int(StateDisabled) * h, w, h, 1);
            }
            continue;
        }
        cell(f, 2, setting(f[0]) == field(f, 1));
    }
    for (auto [it, end] = lines("check"); it != end; ++it) {
        const auto &f = it->second;
        if (f.size() < 7) continue;
        // `check NAME SHEET X Y COLS ROWS COL ON ONHOVER OFF OFFHOVER`, as Test Game 85's voice cards are.
        const int rows[4] = {field(f, 7), field(f, 8), field(f, 9), field(f, 10)};
        cell(f, 1, setting(f[0]) != 0, f.size() >= 11 ? rows : nullptr);
    }
    for (auto [it, end] = lines("step"); it != end; ++it)
        if (it->second.size() >= 8) cell(it->second, 2, false);
}

bool Engine::pressSettingsWidget(int x, int y, bool dragging) {
    auto lines = [&](const char *kind) { return screen_->settings.equal_range(kind); };
    auto within = [&](const std::vector<std::string> &f, size_t at) {
        ImageRef sheet = images_->get(f[at]);
        return sheet && inRect(x, y, field(f, at + 1), field(f, at + 2), sheet->w / field(f, at + 3), sheet->h / field(f, at + 4));
    };
    for (auto [it, end] = lines("slider"); it != end; ++it) {
        const auto &f = it->second;
        ImageRef fill = f.size() >= 4 ? images_->get(f[1]) : nullptr;
        // A held press keeps following the finger along the bar, past its ends.
        int left = field(f, 2), top = field(f, 3);
        if (fill && y >= top && y < top + fill->h && x >= left - (dragging ? 40 : 0) && x < left + fill->w + (dragging ? 40 : 0)) {
            int scale = f.size() >= 5 ? std::max(1, field(f, 4)) : 100;
            changeSetting(f[0], clampSetting(f[0], (x - left) * scale / std::max(1, fill->w - 1)));
            return true;
        }
    }
    // A press on a knob's track, or a drag along it, puts the knob's middle under the pointer.
    for (auto [it, end] = lines("knob"); it != end; ++it) {
        const auto &f = it->second;
        ImageRef knob = f.size() >= 5 ? images_->get(f[1]) : nullptr;
        int x0 = field(f, 2), top = field(f, 3), step = std::max(1, field(f, 4));
        if (!knob) continue;
        int right = x0 + step * clampSetting(f[0], 1 << 20) + knob->w;
        if (y >= top && y < top + knob->h && x >= x0 - (dragging ? 40 : 0) && x < right + (dragging ? 40 : 0)) {
            int value = (x - x0 - knob->w / 2 + step / 2) / step;
            changeSetting(f[0], clampSetting(f[0], value));
            return true;
        }
    }
    if (dragging) return false;
    for (auto [it, end] = lines("radio"); it != end; ++it)
        if (it->second.size() >= 8 && within(it->second, 2)) {
            // Screen mode asks the host to change the window, which reports back what it did.
            if (it->second[0] == "screen") {
                if (canFullScreen_) windowRequest_ = field(it->second, 1);
                return true;
            }
            // `radiooff NAME`: pressing the chosen one again sets 0, as Test Game 88's screen sizes go back to automatic.
            bool off = false;
            for (auto [o, oEnd] = lines("radiooff"); o != oEnd; ++o)
                if (!o->second.empty() && o->second[0] == it->second[0]) off = setting(it->second[0]) == field(it->second, 1);
            int value = off ? 0 : field(it->second, 1);
            changeSetting(it->second[0], value);
            // A radio's `group` sets the others with it, as Test Game 87's one Confirmations switch covers six (set 1).
            for (auto [g, gEnd] = lines("group"); g != gEnd; ++g)
                if (!g->second.empty() && g->second[0] == it->second[0])
                    for (size_t i = 1; i < g->second.size(); i++) changeSetting(g->second[i], value);
            return true;
        }
    for (auto [it, end] = lines("check"); it != end; ++it)
        if (it->second.size() >= 7 && within(it->second, 1)) {
            const std::string &name = it->second[0];
            int value = setting(name) ? 0 : 1;
            changeSetting(name, value);
            // `group NAME SETTING...`: a box such as All Women sets its own and each of those to its new state.
            for (auto [g, gEnd] = lines("group"); g != gEnd; ++g)
                if (!g->second.empty() && g->second[0] == name)
                    for (size_t i = 1; i < g->second.size(); i++) changeSetting(g->second[i], value);
            return true;
        }
    for (auto [it, end] = lines("step"); it != end; ++it)
        if (it->second.size() >= 8 && within(it->second, 2)) {
            changeSetting(it->second[0], clampSetting(it->second[0], setting(it->second[0]) + field(it->second, 1)));
            return true;
        }
    return false;
}

// MARK: Global variables

// A damaged cg_view.ini's number mustn't reach past the flags, which end where the tracks' begin.
static bool validCgId(int id) { return id >= 0 && size_t(id) < (kTrackFlagsAt - kCgFlagsAt) / 4; }

// cfg.sav holds the game's settings, then its 10,000 global variables at byte 4000: a save of Test Game 95's has 1 at its
// macro GLB_PROLOGUE_OVER's number, 1000, after the prologue. VNDegen keeps the rest of the file as it found it.
void Engine::loadGlobals() {
    if (saveDir_.empty()) return;
    auto packed = readWhole(saveDir_ + "/" + nameInFolder(saveDir_, "cfg.sav"));
    if (packed.empty()) return;
    configFile_ = unpackEntry(packed.data(), packed.size());
    if (configFile_.size() < 44000) {
        configFile_.clear();
        return;
    }
    // The sprite gallery's outfit and lock for each heroine, at 1212 and 1224 (Test Game 88 under Wine).
    for (size_t h = 0; h < spriteHeroines_.size() && h < 3; h++) {
        int32_t outfit = 0, locked = 0;
        std::memcpy(&outfit, &configFile_[kSpriteOutfitAt + h * 4], 4);
        std::memcpy(&locked, &configFile_[kSpriteLockAt + h * 4], 4);
        // A value the gallery couldn't have written, from a damaged file, falls back to her first outfit.
        int count = int(spriteHeroines_[h].outfits.size());
        spriteOutfit_[h] = outfit >= 0 && outfit < count ? outfit : 0;
        spriteLocked_[h] = locked ? 1 : 0;
    }
    for (int i = 0; i < 10000; i++) {
        int32_t v;
        std::memcpy(&v, &configFile_[4000 + size_t(i) * 4], 4);
        if (v) globals_[i] = v;
    }
    // `implies A B` in the table's cfgsav screen: global A set means B is too. VNDegen 0.26.1 counted The Alchemist's
    // flags in the file's order and set GLB_ENDING (1050) where the prologue's end means GLB_PROLOGUE_OVER (1000), and an
    // ending can't come before the prologue, so the file is put right as it's read.
    bool repaired = false;
    if (auto table = screens_.find("cfgsav"); table != screens_.end())
        for (auto [it, end] = table->second.settings.equal_range("implies"); it != end; ++it) {
            int set = field(it->second, 0), also = field(it->second, 1);
            if (globals_.count(set) && globals_[set] && !(globals_.count(also) && globals_[also]) && also >= 0 && also < 10000) {
                globals_[also] = 1;
                const int32_t on = 1;
                std::memcpy(&configFile_[4000 + size_t(also) * 4], &on, 4);
                repaired = true;
                log("global %d set, as %d is", also, set);
            }
        }
    // A PC's backup carries what's been unlocked there, which the galleries show here too.
    size_t moved = configStringLength();
    auto flag = [&](size_t at) {
        int32_t v = 0;
        if (at + moved + 4 <= configFile_.size()) std::memcpy(&v, &configFile_[at + moved], 4);
        return v != 0;
    };
    // A picture's flag goes by its number in cg_view.ini, which needn't run in step with the entries.
    for (const GalleryEntry &e : cgEntries_)
        for (size_t p = 0; p < e.images.size(); p++)
            if (validCgId(e.imageIds[p]) && flag(kCgFlagsAt + size_t(e.imageIds[p]) * 4)) seen_[normalizedPath(e.images[p])] = true;
    for (size_t i = 0; i < tracks_.size(); i++)
        if (flag(kTrackFlagsAt + i * 4)) seen_[normalizedPath(tracks_[i])] = true;
    // Only that flag changes in the file: what VNDegen has unlocked besides goes in at its next save, as before.
    if (repaired) {
        auto packed = packEntry(configFile_);
        if (!packed.empty()) writeWhole(saveDir_ + "/" + nameInFolder(saveDir_, "cfg.sav"), packed);
    }
}

// cfg.sav holds two strings of their own length, which move what follows them: a script's name at 44020 and, 56 bytes
// after it, a movie's, which Horny Elves and a Moral Orc's PC writes (`movie\bga_001_000_000_000_000.wmv`) and the other
// games leave empty. The file's size says whether both are counted.
size_t Engine::configStringLength() const {
    auto lengthAt = [&](size_t at) {
        int32_t length = 0;
        if (at + 4 <= configFile_.size()) std::memcpy(&length, &configFile_[at], 4);
        return length > 0 && at + 4 + size_t(length) <= configFile_.size() ? size_t(length) : 0;
    };
    size_t first = lengthAt(kConfigStringAt), second = lengthAt(kConfigStringAt + 60 + first);
    if (second && kConfigSize + first + second == configFile_.size()) return first + second;
    return first;
}

void Engine::saveGlobals() {
    if (saveDir_.empty() || !makeFolders(saveDir_)) return;
    // A game with no file of the PC's yet gets what the PC writes on its first run. Zeros there would set the PC game's
    // volumes to nothing when the saves move to a PC.
    if (configFile_.empty()) {
        configFile_.assign(kConfigSize, 0);
        auto table = screens_.find("cfgsav");
        if (table != screens_.end())
            for (auto [it, end] = table->second.settings.equal_range("run"); it != end; ++it) {
                int32_t value = field(it->second, 2);
                for (int i = 0; i < field(it->second, 1); i++) std::memcpy(&configFile_[size_t(field(it->second, 0)) + size_t(i) * 4], &value, 4);
            }
    }
    for (int i = 0; i < 10000; i++) {
        auto it = globals_.find(i);
        int32_t v = it == globals_.end() ? 0 : it->second;
        std::memcpy(&configFile_[4000 + size_t(i) * 4], &v, 4);
    }
    for (size_t h = 0; h < spriteHeroines_.size() && h < 3; h++) {
        int32_t outfit = spriteOutfit_[h], locked = spriteLocked_[h];
        std::memcpy(&configFile_[kSpriteOutfitAt + h * 4], &outfit, 4);
        std::memcpy(&configFile_[kSpriteLockAt + h * 4], &locked, 4);
    }
    size_t moved = configStringLength();
    auto setFlag = [&](size_t at) {
        const int32_t on = 1;
        if (at + moved + 4 <= configFile_.size()) std::memcpy(&configFile_[at + moved], &on, 4);
    };
    for (const GalleryEntry &e : cgEntries_)
        for (size_t p = 0; p < e.images.size(); p++)
            if (validCgId(e.imageIds[p]) && seen(e.images[p])) setFlag(kCgFlagsAt + size_t(e.imageIds[p]) * 4);
    for (size_t i = 0; i < tracks_.size(); i++)
        if (seen(tracks_[i])) setFlag(kTrackFlagsAt + i * 4);
    auto packed = packEntry(configFile_);
    if (!packed.empty()) writeWhole(saveDir_ + "/" + nameInFolder(saveDir_, "cfg.sav"), packed);
}

// MARK: Saves

std::string Engine::savePath(int slot, const char *extension) const {
    char name[32];
    std::snprintf(name, sizeof name, "save%03d.%s", slot, extension);
    return saveDir_ + "/" + nameInFolder(saveDir_, name);
}

void Engine::noteThumbnail() {
    if (screen_) return;
    if (dirty_) compose();
    thumbnail_ = encodeThumbnail(canvas_.px.data(), canvas_.w, canvas_.h, thumbW_, thumbH_);
    dirty_ = true;
}

Engine::SlotInfo &Engine::slotInfo(int slot) {
    auto found = slots_.find(slot);
    if (found != slots_.end()) return found->second;
    SlotInfo &info = slots_[slot];
    if (saveDir_.empty() || !layout_) return info;
    auto packed = readWhole(savePath(slot, "sav"));
    if (packed.empty()) return info;
    auto plain = unpackEntry(packed.data(), packed.size());
    SaveData data;
    if (plain.empty() || !parseSave(plain, *layout_, data)) return info;
    info.exists = true;
    std::copy(std::begin(data.date), std::end(data.date), info.date);
    info.flow = data.flow;
    if (auto it = data.variables.find(routeVar_); routeVar_ >= 0 && it != data.variables.end()) info.route = it->second;
    info.comment = sjis::toUtf8(data.comment);
    info.variables = data.variables;
    auto bmp = readWhole(savePath(slot, "bmp"));
    if (!bmp.empty()) info.thumbnail = decodeBmpBytes(bmp);
    return info;
}

SaveData Engine::captureSave() {
    SaveData d;
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_r(&now, &local);
    d.date[0] = local.tm_year + 1900;
    d.date[1] = local.tm_mon + 1;
    d.date[2] = local.tm_mday;
    d.date[3] = local.tm_hour;
    d.date[4] = local.tm_min;
    d.date[5] = local.tm_sec;
    d.flow = stack_.empty() ? "" : files_.savedName(stack_.back().script->name);
    // A story table's flows and chapters aren't files: the PC's saves of those games name no flow, and nor do ours.
    if (d.flow.rfind("story:", 0) == 0 || d.flow.rfind("chapter:", 0) == 0) d.flow.clear();
    d.flowNode = flowCallId_;
    // A story table's game saves as the PC does around a choice: 3 while one waits and its number as the flow node, and
    // the last choice's result, 2 while one waits, as Test Game 87's saves have it (set 1). `lastchoice made` under
    // `screen defaults` writes 2 there instead from the first choice on, as Test Game 83's do whichever option was taken
    // (set 3).
    if (!stack_.empty() && stack_.back().script->name.rfind("story:", 0) == 0) {
        bool asking = waiting_ == Waiting::Choice;
        d.flowActive = asking ? 3 : 1;
        if (asking)
            for (const Node &c : stack_.back().script->nodes)
                if (c.opcode == 3 && stack_.back().node && c.next == stack_.back().node->id && c.p[kStoryPcNumber]) d.flowNode = c.p[kStoryPcNumber];
        const Screen *defaults = screens_.count("defaults") ? &screens_.at("defaults") : nullptr;
        const auto *mode = defaults ? defaults->setting("lastchoice") : nullptr;
        bool made = mode && !mode->empty() && (*mode)[0] == "made";
        d.lastChoice = asking || (made && !choiceImage_.empty()) ? 2 : choiceImage_.empty() ? -1 : choiceResult_;
    }
    d.choiceImage = choiceImage_;
    switch (layout_ ? choiceFlag(*layout_) : ChoiceFlag::Picture) {
        case ChoiceFlag::Picture: d.dateFlag = choiceImage_.empty() ? 0 : 1; break;
        case ChoiceFlag::Made: d.dateFlag = choiceMade_ ? 1 : 0; break;
        case ChoiceFlag::Never: d.dateFlag = 0; break;
    }
    d.variables = scenario_;
    d.width = canvas_.w;
    d.height = canvas_.h;
    for (const auto &[slot, s] : front_) {
        if (!s.image) continue;
        SavedSprite out;
        out.slot = slot;
        out.path = s.path;
        // Some games' PC names the picture twice and loads it from the second (namesPictureTwice).
        out.nextPath = !s.noise && layout_ && namesPictureTwice(*layout_) ? s.path : "";
        out.noise = s.noise;
        if (s.flipCount > 0) {
            out.flipFrame = s.flipShown;
            out.flipRepeat = s.flipLoop ? 1 : 0;
            out.flipReverse = s.flipReverse ? 1 : 0;
            out.flipStep = s.flipStep;
            out.flipParam15 = s.flipParam15;
            // A one-shot flipbook past its last picture has finished, which the PC saves with its playing bytes at 0.
            out.flipPlaying = s.flipLoop || s.flipShown < s.flipCount;
        }
        out.x = s.x;
        out.y = s.y;
        out.scaleX = s.scaleX;
        out.scaleY = s.scaleY;
        // The anchor being drawn with, since a saved animation keeps no pivot of its own.
        int anchorX, anchorY;
        anchorOf(s, anchorX, anchorY);
        out.anchorX = anchorX < 0 ? s.image->w / 2 : anchorX;
        out.anchorY = anchorY < 0 ? s.image->h / 2 : anchorY;
        bool pivoted = s.looping && s.loop.hasPivot;
        for (const Animation &a : s.changes) pivoted = pivoted || a.hasPivot;
        if (!s.anchorSet && !pivoted) out.anchorX = out.anchorY = 0;
        out.alpha = int(s.alpha * 256 + 0.5f);
        out.blur = s.blurAmount;
        out.blurOn = s.blurOn;
        out.waveOn = s.waveOn;
        out.waveSpeed = s.waveSpeed;
        out.waveRows = s.waveRows;
        out.waveSize = s.waveSize;
        // Every change as a line of text from the oldest one's start, as the PC writes them. A named animation goes in
        // the same text from the start of its current cycle: a load plays what's left of it once, and a repeating one
        // stops there.
        int64_t loopBegun = s.loop.begunMs;
        if (s.looping && s.loop.repeat && s.loop.periodMs > 0) loopBegun = now_ - (now_ - s.loop.begunMs) % s.loop.periodMs;
        int64_t oldest = s.looping ? loopBegun : now_;
        for (const Animation &a : s.changes) oldest = std::min(oldest, a.begunMs);
        auto frames = [this](int64_t ms) { return int(ms * fps_ / 1000); };
        // A parsed length is its frames in whole ms, rounded down, so rounding up gives the same frames back.
        auto framesOf = [this](int64_t ms) { return int((ms * fps_ + 999) / 1000); };
        auto writeTracks = [&](const Animation &a, int64_t begun) {
            for (const Track &t : a.tracks) {
                const char *name = t.property == Track::X ? (t.relative ? "relative_pos_x" : "pos_x") : t.property == Track::Y ? (t.relative ? "relative_pos_y" : "pos_y") : t.property == Track::ScaleX ? "scale_x" : t.property == Track::ScaleY ? "scale_y" : t.property == Track::Alpha ? "alpha" : "rotate";
                int length = framesOf(t.lengthMs);
                char line[128];
                // A scale line names its pivot, the point of the picture it keeps still, as the PC writes one:
                // `scale_x,1.100000,640,0,2400,linear`.
                if (t.property == Track::ScaleX || t.property == Track::ScaleY)
                    std::snprintf(line, sizeof line, "%s,%f,%d,%d,%d,%s\n", name, double(t.to), t.property == Track::ScaleX ? out.anchorX : out.anchorY,
                                  framesOf(t.startMs + (begun - oldest)), t.easeOut ? -length : length, t.easeOut ? "quadratic" : "linear");
                else
                    std::snprintf(line, sizeof line, "%s,%g,%d,%d,%s\n", name, double(t.relative ? t.to - t.from : t.to), framesOf(t.startMs + (begun - oldest)), t.easeOut ? -length : length, t.easeOut ? "quadratic" : "linear");
                out.animation += line;
            }
        };
        for (const Animation &a : s.changes) writeTracks(a, a.begunMs);
        if (s.looping) writeTracks(s.loop, loopBegun);
        out.animated = !out.animation.empty();
        // A repeating named animation with nothing else running is saved as the PC saves one: its text as command 19 gave
        // it, the frames since it began, and the byte that says it repeats, so it goes on repeating after a load, here and
        // on the PC.
        out.repeat = s.looping && s.loop.repeat && s.changes.empty();
        out.animationFrames = out.animated ? frames(now_ - oldest) : 0;
        if (out.repeat && !s.loop.source.empty()) {
            out.animation = s.loop.source;
            out.animationFrames = frames(now_ - s.loop.begunMs);
        }
        // Where it ends up.
        Sprite last = s;
        for (Animation &a : last.changes) a.begunMs = -1000000000;
        last.looping = false;
        SpriteView end = view(last);
        out.endX = end.x;
        out.endY = end.y;
        out.endScaleX = end.scaleX;
        out.endScaleY = end.scaleY;
        out.endAlpha = int(end.alpha * 256 + 0.5f);
        d.sprites.push_back(out);
    }
    // Splash emitters are records of their own, in their slots among the pictures', as the PC writes them. A droplet
    // lives a second or so, so only the emitter is kept.
    for (const auto &[slot, splash] : splashes_) {
        if (splash.path.empty()) continue;
        SavedSprite out;
        out.slot = slot;
        out.splash = true;
        out.splashImage = splash.path;
        out.splashNumbers = splash.saved;
        auto at = std::find_if(d.sprites.begin(), d.sprites.end(), [&](const SavedSprite &s) { return s.slot > slot; });
        d.sprites.insert(at, out);
    }
    for (const auto &[name, part] : parts_) part->savePC(d);
    d.windowShown = windowShown_;
    d.current = currentText_;
    d.backlog = backlog_;
    auto soundsOf = [&](const std::map<int, Playing> &table, const std::map<int, int> &players, int base) {
        for (const auto &[channel, playing] : table) {
            SavedSound sound;
            sound.index = base + channel;
            sound.path = playing.path;
            sound.volume = playing.volume;
            sound.pan = playing.pan;
            auto player = players.find(channel);
            sound.playing = player != players.end();
            if (sound.playing) sound.seconds = double(audio_.positionMs(player->second)) / 1000;
            d.sounds.push_back(sound);
        }
    };
    soundsOf(bgmNow_, bgm_, 0);
    soundsOf(seNow_, se_, 60);
    // Between scenes the PC's save still names the last one, and it won't ask a choice from a save that doesn't.
    d.scene = files_.savedName(scene_.script ? scene_.script->name : lastScene_);
    d.gtConfig = gt_.config;
    if (gt_.active || gt_.ejaculating) {
        d.gtState = gt_.extending ? std::max(1, gt_.played) : 0;
        d.gtCount = gt_.count;
        d.gtStartNode = gt_.count > 0 && int(gt_.texts.size()) >= gt_.count ? gt_.texts[gt_.texts.size() - size_t(gt_.count)] : 0;
        d.gtMarker = gt_.marker;
        d.gtScene = gt_.main ? files_.savedName(gt_.main->name) : "";
    }
    d.node = scene_.node ? scene_.node->id : 0;
    return d;
}

bool Engine::saveToSlot(int slot) {
    if (!layout_ || saveDir_.empty() || !makeFolders(saveDir_)) {
        complain("Saves aren't available for this game.");
        return false;
    }
    SaveData data = captureSave();
    auto packed = packEntry(buildSave(data, *layout_));
    if (packed.empty() || !writeWhole(savePath(slot, "sav"), packed)) {
        complain("The save couldn't be written.");
        return false;
    }
    if (!thumbnail_.empty()) writeWhole(savePath(slot, "bmp"), thumbnail_);
    writeFlipModes(slot, data);
    // The card that shows NEW, kept in cfg.sav as the PC keeps it.
    globals_[0] = newCardNumber(layout_, slot);
    saveGlobals();
    saveSeen();
    saveReadText();
    slots_.erase(slot);
    log("saved slot %d at %s #%d", slot, data.scene.c_str(), data.node);
    return true;
}

int Engine::newestSlot(int first, int last) {
    int newest = 0;
    const int *newestDate = nullptr;
    for (int slot = first; slot <= last; slot++) {
        const SlotInfo &info = slotInfo(slot);
        if (info.exists && (!newestDate || std::lexicographical_compare(newestDate, newestDate + 6, info.date, info.date + 6))) {
            newest = slot;
            newestDate = info.date;
        }
    }
    return newest;
}

void Engine::deleteSlot(int slot) {
    std::remove(savePath(slot, "sav").c_str());
    std::remove(savePath(slot, "bmp").c_str());
    std::remove(savePath(slot, "vnd").c_str());
    slots_.erase(slot);
    log("deleted slot %d", slot);
    dirty_ = true;
}

bool Engine::loadFromSlot(int slot) {
    if (!layout_) return false;
    auto packed = readWhole(savePath(slot, "sav"));
    auto plain = packed.empty() ? packed : unpackEntry(packed.data(), packed.size());
    SaveData data;
    if (plain.empty() || !parseSave(plain, *layout_, data)) {
        complain("That save couldn't be read.");
        return false;
    }
    restoreSave(data, slot);
    log("loaded slot %d at %s #%d", slot, data.scene.c_str(), data.node);
    return true;
}

void Engine::restoreSave(const SaveData &d, int slot) {
    // A save is the story, so loading one during a replay ends the replay, with its own bar and its Exit, and the CG
    // viewer it may have come from.
    replaying_ = false;
    replayFromViewer_ = false;
    staffRollLayers_ = nullptr;
    stopTrack();
    stopKind(kKindBgm, 300);
    stopKind(kKindSe, 300);
    stopKind(kKindVoice, 0);
    if (titleMusic_) {
        audio_.fade(titleMusic_, 300, 0, true);
        audio_.retire(titleMusic_);
        titleMusic_ = 0;
    }
    screen_ = nullptr;
    stack_.clear();
    back_.clear();
    front_.clear();
    splashes_.clear();
    transitioning_ = false;
    shakeType_ = 0;
    flashMs_ = 0;
    blur_ = false;
    scenario_ = d.variables;
    choiceImage_ = d.choiceImage;
    // Only Test Game 87's saves say whether a choice has been made; elsewhere a choice's picture means one was.
    choiceMade_ = layout_ && choiceFlag(*layout_) == ChoiceFlag::Made ? d.dateFlag != 0 : !d.choiceImage.empty();
    flowCallId_ = d.flowNode;
    bool atChoice = false;
    // Game parts first, since a part's chapters, which the story's place is found in, can depend on its state.
    Sidecar side = readSidecar(d, slot);
    loadPartsState(d, side);
    if (auto flow = d.flow.empty() ? nullptr : loadScript(d.flow)) {
        const Node *call = flow->node(d.flowNode);
        // The PC's save made while a choice waits names the choice, and it's asked again. Ours names the scene before, which
        // is what the PC itself needs to ask it: there, as on the PC, the last line shows and a click brings the choice.
        bool namesChoice = call && (call->opcode == 3 || call->opcode == 20);
        atChoice = namesChoice;
        stack_.push_back({flow, namesChoice ? call : call ? flow->node(call->next) : flow->first()});
        // Back to the scene before, so that saving again here writes what the PC can ask the choice from.
        if (namesChoice)
            for (const Node &candidate : flow->nodes)
                if (candidate.opcode == 1 && candidate.next == call->id) flowCallId_ = candidate.id;
    } else if (d.flow.empty() && !firstStoryFlow().empty() && !restoreStoryPlace(side, atChoice) && !resumeStoryAtNumber(d.flowNode, d.flowActive, d.scene, atChoice) &&
               !resumeStoryAt(d.scene)) {
        complain("The story table has no place for the save's scene " + d.scene);
    }
    std::map<int, FlipModes> flipModes = flipModesAt(d, side);
    for (const SavedSprite &saved : d.sprites) {
        // A splash's emitter comes back ready for the scene's next burst, as command 200 made it; its speed, which the
        // save doesn't keep, is the 300 every one of Test Game 88's has.
        if (saved.splash) {
            const auto &v = saved.splashNumbers;
            Splash &splash = splashes_[saved.slot];
            splash.path = saved.splashImage;
            splash.image = images_->get(saved.splashImage);
            splash.saved = v;
            splash.cap = std::max(1, v[1]);
            splash.minLife = std::max(1, v[7]);
            splash.maxLife = std::max(splash.minLife, v[8]);
            splash.speed = 300;
            if (!splash.image) warnOnce("A save names a missing splash picture: " + saved.splashImage);
            continue;
        }
        Sprite s;
        // The film noise starts again where the save left it running.
        if (saved.noise) {
            s.image = images_->get("effect\\noise2_small.bmp");
            if (!s.image) s.image = images_->get("sys\\noise2_small.bmp");
            s.noise = true;
            s.noiseFrom = now_;
            if (s.image) front_[saved.slot] = s;
            continue;
        }
        s.path = saved.path;
        // A movie starts again from its beginning. Whether it looped isn't known from the save, and the library's
        // looping ones are backgrounds, so it loops.
        if (normalizedPath(saved.path).rfind("movie\\", 0) == 0 && (s.movie = openMovie(saved.path, true))) {
            s.movieFrom = now_;
            bool ended = false;
            s.image = s.movie->frame(0, ended);
        } else {
            s.image = images_->get(shownPath(saved.path));
        }
        if (!s.image && !s.movie) warnOnce("A save names a missing picture: " + saved.path);
        s.x = saved.x;
        s.y = saved.y;
        s.scaleX = saved.scaleX;
        s.scaleY = saved.scaleY;
        s.alpha = float(saved.alpha) / 256;
        s.blurAmount = saved.blur;
        s.blurOn = saved.blurOn;
        s.waveOn = saved.waveOn;
        s.waveSpeed = saved.waveSpeed;
        s.waveRows = saved.waveRows;
        s.waveSize = saved.waveSize;
        if (s.image || s.movie) {
            s.anchorX = saved.anchorX;
            s.anchorY = saved.anchorY;
            s.anchorSet = saved.anchorX != 0 || saved.anchorY != 0;
        }
        if (saved.flipFrame >= 0) {
            resumeFlipbook(s);
            s.flipStep = saved.flipStep;
            s.flipParam15 = saved.flipParam15;
            restoreFlipbookModes(s, saved.slot, saved.flipFrame, flipModes, saved.flipRepeat, saved.flipRepeat >= 0 ? saved.flipReverse : -1);
        }
        if (!saved.animation.empty()) {
            Animation a = parseAnimation(saved.animation, s);
            a.begunMs = now_ - int64_t(saved.animationFrames) * 1000 / std::max(1, fps_);
            // A repeating one runs on as the named animation, at the place in its cycle the save was made at.
            if (saved.repeat && a.periodMs > 0) {
                a.repeat = true;
                a.source = saved.animation;
                s.loop = std::move(a);
                s.looping = true;
            } else {
                s.changes.push_back(std::move(a));
            }
        }
        front_[saved.slot] = s;
    }
    back_ = front_;
    backlog_ = d.backlog;
    currentText_ = d.current;
    for (const SavedSound &sound : d.sounds) {
        if (!sound.playing) continue;
        int kind = sound.index < 60 ? kKindBgm : kKindSe;
        int channel = sound.index < 60 ? sound.index : sound.index - 60;
        Bytes data = files_.read(sound.path);
        int player = data ? audio_.open(data) : 0;
        if (!player) continue;
        audio_.setGroup(player, soundGroup(kind, sound.path));
        audio_.setLoops(player, 0);
        audio_.fade(player, 0, sound.volume, false);
        if (sound.pan) audio_.setPan(player, sound.pan, 0);
        audio_.seekMs(player, int64_t(sound.seconds * 1000));
        audio_.start(player);
        channelsOf(kind)[channel] = player;
        (kind == kKindBgm ? bgmNow_ : seNow_)[channel] = {sound.path, sound.volume, sound.pan};
    }
    auto script = d.scene.empty() ? nullptr : loadScript(d.scene);
    scene_ = script ? Frame{script, script->node(d.node)} : Frame{};
    lastScene_ = d.scene;
    // The PC's saves don't hold Extend: two saves differing only in it match. It stays as it was across a load.
    bool extendWas = gt_.extend;
    gt_ = {};
    gt_.extend = extendWas;
    if (!d.gtConfig.empty() && readGtConfig(d.gtConfig) && d.gtState >= 0 && !d.gtScene.empty()) {
        gt_.main = loadScript(d.gtScene);
        if (gt_.main) {
            gt_.marker = d.gtMarker;
            gt_.count = d.gtCount;
            gtMeasure(*gt_.main, gt_.marker);
            gt_.active = true;
            // Inside one of its scripts, the script says which part it's in.
            bool inMain = normalizedPath(d.scene) == normalizedPath(d.gtScene);
            if (!inMain && !gt_.ejaculate.empty() && normalizedPath(d.scene) == normalizedPath(gt_.ejaculate[0])) {
                gt_.active = false;
                gt_.ejaculating = true;
            } else if (!inMain) {
                gt_.extending = true;
                gt_.played = d.gtState;
                for (int group = 0; group < 3; group++)
                    for (size_t i = 0; i < gt_.groups[group].size(); i++)
                        if (normalizedPath(gt_.groups[group][i]) == normalizedPath(d.scene)) {
                            gt_.group = group;
                            gt_.index = int(i);
                        }
            }
            const Node *resumeAt = scene_.node;
            if (inMain && gt_.count > 0 && resumeAt) {
                // The text on screen is the one before the node the save holds.
                const Node *n = gt_.main->first();
                int textId = 0;
                for (size_t guard = 0; n && n != resumeAt && guard < gt_.main->nodes.size(); guard++) {
                    if (n->opcode == 5) textId = n->id;
                    n = gt_.main->node(n->next);
                }
                auto at = std::find(gt_.texts.begin(), gt_.texts.end(), textId);
                int left = at == gt_.texts.end() ? 0 : int(gt_.texts.end() - at);
                gt_.shownCount = left <= gt_.count ? left : 0;
            }
        }
    }
    windowShown_ = d.windowShown;
    windowShownAt_ = -100000;
    // The PC shows the choice in an empty window, with nothing to click through first.
    layoutText(atChoice ? "" : sjis::toUtf8(d.current.name), atChoice ? "" : sjis::toUtf8(d.current.text));
    textStart_ = now_;
    textDone_ = true;
    // A game part's screen saved open waits for its input again, as it was.
    waiting_ = partScreen_ ? Waiting::PartScreen : atChoice ? Waiting::None : Waiting::Click;
    dirty_ = true;
}

// The table's `slots` line: the sheet, then x, y, the step across and down, rows per column and columns. Cards number down
// each column unless an `across` of 1 follows, and each row can sit further right than the one above by a last number. The
// `slot*` lines give where each part sits within a card.
int Engine::cardsPerPage() const {
    const auto *grid = screen_ ? screen_->setting("slots") : nullptr;
    return grid && grid->size() >= 7 ? field(*grid, 5) * field(*grid, 6) : 0;
}

// Where the card at this place on a page sits.
// `pageoff N`: page N's button is disabled on this screen, as Test Game 89's Quick Save page is on its save screen.
bool Engine::pageStepOff(const std::string &action) const {
    if (action == "pageprev") return page_ <= 0;
    if (action == "pagenext") return !galleryPageExists(page_ + 1);
    return false;
}

bool Engine::pageOff(const std::string &action) const {
    if (!screen_ || action.rfind("page", 0) != 0) return false;
    for (auto [it, end] = screen_->settings.equal_range("pageoff"); it != end; ++it)
        if (!it->second.empty() && action == "page" + it->second[0]) return true;
    return false;
}

void Engine::cardPlace(int index, int &x, int &y) const {
    const auto &grid = *screen_->setting("slots");
    int perColumn = field(grid, 5), columns = field(grid, 6);
    bool across = grid.size() >= 8 && field(grid, 7);
    int column = across ? index % columns : index / perColumn, row = across ? index / columns : index % perColumn;
    x = field(grid, 1) + column * field(grid, 3) + row * (grid.size() >= 9 ? field(grid, 8) : 0);
    y = field(grid, 2) + row * field(grid, 4);
}

// Where a slot's card sits on the current page.
bool Engine::cardAt(int slot, int &x, int &y) const {
    int perPage = cardsPerPage();
    if (!perPage || slot < 1) return false;
    cardPlace((slot - 1) % perPage, x, y);
    return true;
}

int Engine::route() const {
    auto it = routeVar_ >= 0 ? scenario_.find(routeVar_) : scenario_.end();
    return it == scenario_.end() ? 0 : it->second;
}

// A card's lock is a global variable from 200 on, one for each card from the first: locking a card under Wine set 200
// for the first and 204 for the fifth in the cfg.sav the PC wrote on quitting, in The Alchemist, Yarinaoshi no
// Christmas and Test Games 92 and 94 alike.
constexpr int kLockGlobals = 200;

bool Engine::locked(int slot) const {
    auto it = globals_.find(kLockGlobals + slot - 1);
    return it != globals_.end() && it->second;
}

void Engine::toggleLock(int slot) {
    int global = kLockGlobals + slot - 1;
    if (slot < 1 || global >= 10000) return;
    globals_[global] = locked(slot) ? 0 : 1;
    saveGlobals();
    log("slot %d %s", slot, globals_[global] ? "locked" : "unlocked");
    dirty_ = true;
}

bool Engine::overComment(int slot, int x, int y) const {
    const auto *line = screen_ ? screen_->setting("slotcomment") : nullptr;
    int cx, cy;
    if (!line || line->size() < 4 || !cardAt(slot, cx, cy)) return false;
    int size = field(*line, 3);
    return inRect(x, y, cx + field(*line, 0) - 4, cy + field(*line, 1) - size - 4, field(*line, 2) + 8, size + 12);
}

bool Engine::takeHostRequest(std::string &kind, std::string &payload) {
    if (hostRequestKind_.empty()) return false;
    kind = std::exchange(hostRequestKind_, std::string());
    log("host takes a %s request", kind.c_str());
    payload = std::exchange(hostRequestPayload_, std::string());
    return true;
}

int Engine::takeCommentRequest(std::string &utf8) {
    int slot = std::exchange(commentRequest_, 0);
    if (slot) utf8 = slotInfo(slot).comment;
    return slot;
}

bool Engine::setComment(int slot, const std::string &utf8) {
    auto packed = readWhole(savePath(slot, "sav"));
    auto plain = packed.empty() ? packed : unpackEntry(packed.data(), packed.size());
    auto changed = plain.empty() ? plain : withComment(plain, sjis::fromUtf8(utf8));
    if (changed.empty() || !writeWhole(savePath(slot, "sav"), packEntry(changed))) {
        complain("The comment couldn't be written.");
        return false;
    }
    slots_.erase(slot);
    log("slot %d comment set", slot);
    dirty_ = true;
    return true;
}

bool Engine::overCardPart(const char *key, int slot, int x, int y) const {
    const auto *line = screen_ ? screen_->setting(key) : nullptr;
    int cx, cy;
    if (!line || line->size() < 3 || !cardAt(slot, cx, cy)) return false;
    ImageRef button = images_->get((*line)[0]);
    return button && inRect(x, y, cx + field(*line, 1), cy + field(*line, 2), button->w, button->h / 5);
}

int Engine::slotAt(int x, int y) const {
    int perPage = cardsPerPage();
    if (!perPage) return -1;
    ImageRef sheet = images_->get((*screen_->setting("slots"))[0]);
    if (!sheet) return -1;
    const auto *rows = screen_->setting("slotrows");
    int w = sheet->w, h = sheet->h / (rows ? field(*rows, 0) : 5);
    for (int i = 0; i < perPage; i++) {
        int cx, cy;
        cardPlace(i, cx, cy);
        if (x >= cx && y >= cy && x < cx + w && y < cy + h) return page_ * perPage + i + 1;
    }
    return -1;
}

void Engine::composeSlots(Canvas &out) {
    int perPage = cardsPerPage();
    if (!perPage) return;
    ImageRef sheet = images_->get((*screen_->setting("slots"))[0]);
    if (!sheet) return;
    const auto *cardRows = screen_->setting("slotrows");
    int cardH = sheet->h / (cardRows ? field(*cardRows, 0) : 5);
    auto offset = [&](const char *key, int index) {
        const auto *line = screen_->setting(key);
        return line && int(line->size()) > index ? std::atoi((*line)[size_t(index)].c_str()) : 0;
    };
    auto picture = [&](const char *key) { const auto *line = screen_->setting(key); return line && !line->empty() ? images_->get((*line)[0]) : nullptr; };
    ImageRef digits = picture("slotdigits"), dateDigits = picture("datedigits"), noLabel = picture("slotlabel");
    ImageRef slash = picture("dateslash"), colon = picture("datecolon"), dash = picture("datedash");
    ImageRef frame = picture("slotframe"), empty = picture("slotempty");
    // A date sheet of 12 holds its slash and colon after the digits. The number shows how many digits a card number has.
    int dateColumns = offset("datedigits", 3) ? offset("datedigits", 3) : 10, numberDigits = offset("slotdigits", 3) ? offset("slotdigits", 3) : 2;
    bool loading = screen_->name == "load";
    int hovered = pointerSeen_ ? slotAt(pointerX_, pointerY_) : -1;
    // NEW marks the card saved to last, which the PC keeps in global 0: Test Game 94's held 2 and The Alchemist's 1, the
    // cards their screens marked, and Test Game 87 counts from 0.
    int newest = newCardSlot(layout_, variable(0, 0));
    auto drawNumber = [&](const ImageRef &sheetOfTen, int value, int count, int x, int y, int cells = 10, int step = 0) {
        if (!sheetOfTen) return x;
        int w = sheetOfTen->w / cells;
        if (step <= 0) step = w;
        for (int i = count - 1; i >= 0; i--) {
            int digit = value % 10;
            value /= 10;
            drawPart(out, *sheetOfTen, x + i * step, y, digit * w, 0, w, sheetOfTen->h, 1);
        }
        return x + count * step;
    };
    for (int index = 0; index < perPage; index++) {
            int slot = page_ * perPage + index + 1;
            SlotInfo &info = slotInfo(slot);
            int cx, cy;
            cardPlace(index, cx, cy);
            // `lockdim`: Test Games 92 and 94 draw a locked card as an empty one on the load screen. The Alchemist and Yarinaoshi
            // no Christmas draw it as any other.
            bool isLocked = info.exists && locked(slot);
            int row = (loading && !info.exists) || (isLocked && screen_->setting("lockdim")) ? int(StateDisabled) : slot == hovered ? int(StateHover) : int(StateNormal);
            // `emptycards OPACITY`: Test Game 92's PC draws an empty card on the load screen faded, at 80 of 256.
            float cardOpacity = loading && !info.exists && screen_->setting("emptycards") ? float(offset("emptycards", 0)) / 256 : 1;
            drawPart(out, *sheet, cx, cy, 0, cardRows ? 0 : row * cardH, sheet->w, cardH, cardOpacity);
            if (info.thumbnail) drawPart(out, *info.thumbnail, cx + offset("slotthumb", 0), cy + offset("slotthumb", 1), 0, 0, info.thumbnail->w, info.thumbnail->h, 1);
            else if (empty) drawPart(out, *empty, cx + offset("slotempty", 1), cy + offset("slotempty", 2), 0, 0, empty->w, empty->h, 1);
            // `slotthumbframe SHEET X Y`: a frame around the thumbnail of every card with a save, as Test Game 82's cards have.
            if (ImageRef around = picture("slotthumbframe"); around && info.exists)
                drawPart(out, *around, cx + offset("slotthumbframe", 1), cy + offset("slotthumbframe", 2), 0, 0, around->w, around->h, 1);
            if (frame && frame->h >= 5 && slot == hovered) drawPart(out, *frame, cx + offset("slotframe", 1), cy + offset("slotframe", 2), 0, frame->h / 5, frame->w, frame->h / 5, 1);
            if (noLabel) drawPart(out, *noLabel, cx + offset("slotlabel", 1), cy + offset("slotlabel", 2), 0, 0, noLabel->w, noLabel->h, 1);
            // `slotpic SHEET X Y`: a picture on every card, full or empty, as Test Game 87's Save Date label (set 1).
            for (auto [it, end] = screen_->settings.equal_range("slotpic"); it != end; ++it)
                if (it->second.size() >= 3)
                    if (ImageRef pic = images_->get(it->second[0])) drawPart(out, *pic, cx + field(it->second, 1), cy + field(it->second, 2), 0, 0, pic->w, pic->h, 1);
            // `slotchaptervar SHEET X Y COLS VAR`: the chapter's name by a scenario variable of the save, cell VAR's value of
            // a strip of COLS across, its edges rounded, as Test Game 87's cards name theirs by S 4000 (set 1).
            if (const auto *chapter = screen_->setting("slotchaptervar"); chapter && chapter->size() >= 5 && info.exists)
                if (ImageRef names = images_->get((*chapter)[0])) {
                    int cols = std::max(1, field(*chapter, 3));
                    auto it = info.variables.find(field(*chapter, 4));
                    int k = it == info.variables.end() ? 0 : it->second;
                    if (k >= 0 && k < cols) {
                        int x0 = (k * names->w + cols / 2) / cols, x1 = ((k + 1) * names->w + cols / 2) / cols;
                        drawPart(out, *names, cx + field(*chapter, 1), cy + field(*chapter, 2), x0, 0, x1 - x0, names->h, 1);
                    }
                }
            // `slotchapter SHEET X Y ROWS FLOW...`: the name of the chapter a save is in, a row of the sheet for each flow
            // script named in order, as Test Game 89's cards show.
            if (const auto *chapter = screen_->setting("slotchapter"); chapter && chapter->size() >= 5 && info.exists)
                if (ImageRef names = images_->get((*chapter)[0])) {
                    std::string flow = normalizedPath(info.flow);
                    flow = flow.substr(flow.find_last_of('\\') + 1);
                    for (size_t i = 4; i < chapter->size(); i++)
                        if (flow.rfind(normalizedPath((*chapter)[i]), 0) == 0) {
                            int h = names->h / std::max(1, field(*chapter, 3));
                            drawPart(out, *names, cx + field(*chapter, 1), cy + field(*chapter, 2), 0, int(i - 4) * h, names->w, h, 1);
                            break;
                        }
                }
            // The Q-Save page numbers its cards from 1 again.
            // Test Game 84's cards there read Q01.
            ImageRef quickMark = slot >= firstQuickSlot_ ? picture("slotquick") : nullptr;
            int numberX = cx + offset("slotdigits", 1);
            if (quickMark) {
                drawPart(out, *quickMark, numberX, cy + offset("slotdigits", 2), 0, 0, quickMark->w, quickMark->h, 1);
                numberX += quickMark->w;
            }
            // `slotdigits SHEET X Y COUNT [STEP]`: STEP apart, the cell width when absent (15 for Test Game 82's 14-wide digits).
            drawNumber(digits, slot >= firstQuickSlot_ ? slot - firstQuickSlot_ + 1 : slot, quickMark ? numberDigits - 1 : numberDigits, numberX, cy + offset("slotdigits", 2), 10, offset("slotdigits", 4));
            if (ImageRef del = picture("slotdelete"); del && del->h >= 5) {
                int h = del->h / 5;
                bool over = pointerSeen_ && !dialogOpen_ && overCardDelete(slot, pointerX_, pointerY_);
                int shown = !info.exists || isLocked ? int(StateDisabled) : over ? int(StateHover) : int(StateNormal);
                drawPart(out, *del, cx + offset("slotdelete", 1), cy + offset("slotdelete", 2), 0, shown * h, del->w, h, 1);
            }
            // The lock's rows: open, open under the pointer, closed, closed under the pointer, and none to set.
            if (ImageRef lock = picture("slotlock"); lock && lock->h >= 5) {
                int h = lock->h / 5;
                bool over = pointerSeen_ && !dialogOpen_ && overCardPart("slotlock", slot, pointerX_, pointerY_);
                int shown = !info.exists ? int(StateDisabled) : isLocked ? (over ? int(StateSelectedHover) : int(StateSelected)) : over ? int(StateHover) : int(StateNormal);
                drawPart(out, *lock, cx + offset("slotlock", 1), cy + offset("slotlock", 2), 0, shown * h, lock->w, h, 1);
            }
            // `slotprogress SHEET X Y ROWS`: the save's route mode, a row each from normal play on, as 94's cards show.
            if (const auto *progress = screen_->setting("slotprogress"); progress && progress->size() >= 4 && info.exists)
                if (ImageRef names = images_->get((*progress)[0]); names && info.route >= 0 && info.route < field(*progress, 3)) {
                    int h = names->h / std::max(1, field(*progress, 3));
                    drawPart(out, *names, cx + field(*progress, 1), cy + field(*progress, 2), 0, info.route * h, names->w, h, 1);
                }
            // `slotvar SHEET X Y COLS S N`: a cell of a row of COLS picked by the save's scenario variable N, as Test Game 82's
            // cards name the chapter by S5000. A value outside the row draws nothing.
            if (const auto *chosen = screen_->setting("slotvar"); chosen && chosen->size() >= 6 && info.exists && (*chosen)[4] == "S")
                if (ImageRef names = images_->get((*chosen)[0])) {
                    auto it = info.variables.find(field(*chosen, 5));
                    int value = it == info.variables.end() ? 0 : it->second, columns = std::max(1, field(*chosen, 3)), w = names->w / columns;
                    if (value >= 0 && value < columns) drawPart(out, *names, cx + field(*chosen, 1), cy + field(*chosen, 2), value * w, 0, w, names->h, 1);
                }
            // `slotnew SHEET X Y`: the card saved to last.
            if (ImageRef mark = picture("slotnew"); mark && slot == newest && info.exists)
                drawPart(out, *mark, cx + offset("slotnew", 1), cy + offset("slotnew", 2), 0, 0, mark->w, mark->h, 1);
            // `slotcomment X BASELINE WIDTH SIZE PROMPT...`: the card's comment in white on its bar, cut off at its width, or
            // for a save with none the PC's prompt to click and type one.
            if (const auto *line = screen_->setting("slotcomment"); line && line->size() >= 4 && info.exists) {
                std::string text = info.comment;
                if (text.empty())
                    for (size_t i = 4; i < line->size(); i++) text += (i > 4 ? " " : "") + (*line)[i];
                drawComment(out, text, cx + field(*line, 0), cy + field(*line, 1), field(*line, 2), field(*line, 3));
            }
            // `datefield year|month|day|hour|minute X Y [STEP] [blank]`: each part of the date at a place of its own on the card,
            // in place of the running layout, as Test Game 82's cards have it. STEP is the distance between digits, the sheet's
            // cell width when absent; `blank` leaves a leading zero out, the number keeping its right edge.
            if (screen_->setting("datefield")) {
                if (!info.exists || !dateDigits) continue;
                static const char *const kParts[] = {"year", "month", "day", "hour", "minute"};
                for (auto [line, end] = screen_->settings.equal_range("datefield"); line != end; ++line) {
                    const auto &w = line->second;
                    if (w.size() < 3) continue;
                    int part = int(std::find(std::begin(kParts), std::end(kParts), w[0]) - std::begin(kParts));
                    if (part >= 5) continue;
                    int count = part == 0 ? 4 : 2, cell = dateDigits->w / dateColumns, step = w.size() >= 4 && w[3] != "blank" ? field(w, 3) : cell;
                    bool blank = std::find(w.begin() + 3, w.end(), "blank") != w.end();
                    int value = info.date[part];
                    for (int i = count - 1; i >= 0; i--, value /= 10) {
                        if (blank && value == 0 && i < count - 1) break;
                        drawPart(out, *dateDigits, cx + field(w, 1) + i * step, cy + field(w, 2), value % 10 * cell, 0, cell, dateDigits->h, 1);
                    }
                }
                continue;
            }
            int x = cx + offset("datedigits", 1), y = cy + offset("datedigits", 2);
            auto mark = [&](const ImageRef &m, int cellInSheet) {
                if (m) {
                    drawPart(out, *m, x, y, 0, 0, m->w, m->h, 1);
                    x += m->w;
                } else if (dateDigits && dateColumns > 10) {
                    int w = dateDigits->w / dateColumns;
                    drawPart(out, *dateDigits, x, y, cellInSheet * w, 0, w, dateDigits->h, 1);
                    // The slash and colon are thin, and the PC's digits close up around them.
                    x += w - 2;
                }
            };
            if (info.exists) {
                x = drawNumber(dateDigits, info.date[0], 4, x, y, dateColumns);
                mark(slash, 10);
                x = drawNumber(dateDigits, info.date[1], 2, x, y, dateColumns);
                // `dateslash2 DX`: the second slash that much further on, as Test Game 87's sits 2 pixels right (set 1).
                x += offset("dateslash2", 0);
                mark(slash, 10);
                x = drawNumber(dateDigits, info.date[2], 2, x, y, dateColumns);
                x += offset("dategap", 0);
                // Test Game 85 has the time on a line of its own.
                if (screen_->setting("timeat")) {
                    x = cx + offset("timeat", 0);
                    y = cy + offset("timeat", 1);
                }
                x = drawNumber(dateDigits, info.date[3], 2, x, y, dateColumns);
                mark(colon, 11);
                x = drawNumber(dateDigits, info.date[4], 2, x, y, dateColumns);
                // Test Game 84's dates stop at the minute, and with `slotnoseconds` Test Game 87's too (set 1).
                if (dateColumns == 10 && !screen_->setting("slotnoseconds")) {
                    mark(colon, 11);
                    x = drawNumber(dateDigits, info.date[5], 2, x, y);
                }
            } else if (dash && dateDigits) {
                int w = dateDigits->w / 10;
                auto dashes = [&](int count) {
                    for (int i = 0; i < count; i++) drawPart(out, *dash, x + i * w, y, 0, 0, dash->w, dash->h, 1);
                    x += count * w;
                };
                dashes(4);
                mark(slash, 10);
                dashes(2);
                x += offset("dateslash2", 0);
                mark(slash, 10);
                dashes(2);
                x += offset("dategap", 0);
                if (screen_->setting("timeat")) {
                    x = cx + offset("timeat", 0);
                    y = cy + offset("timeat", 1);
                }
                dashes(2);
                mark(colon, 11);
                dashes(2);
                mark(colon, 11);
                dashes(2);
            }
    }
}

// The questions in the order of the game's sheet of them, and the Config setting that turns each off.
static const char *const kConfirmSetting[] = {"confirm_saveload", "confirm_saveload", "confirm_overwrite", "confirm_delete", "confirm_quick", "confirm_quick", "confirm_title", "confirm_exit"};

void Engine::openDialog(int question, const std::function<void()> &yes) {
    if (!screens_.count("dialog") || !setting(kConfirmSetting[question])) {
        yes();
        return;
    }
    dialogOpen_ = true;
    dialogQuestion_ = question;
    dialogYes_ = yes;
    dialogNoAsk_ = false;
    dirty_ = true;
}

void Engine::answerDialog(bool yes) {
    dialogOpen_ = false;
    dirty_ = true;
    auto then = dialogYes_;
    dialogYes_ = nullptr;
    // The box turns the question off whichever answer closes it, as the PC does, in the Config setting for it.
    if (dialogNoAsk_) {
        changeSetting(kConfirmSetting[dialogQuestion_], 0);
        saveSettings();
        dialogNoAsk_ = false;
    }
    if (yes && then) then();
}

// The table's dialog screen: a band, one row of a sheet of questions, and yes and no.
void Engine::composeDialog(Canvas &out) {
    const Screen &dialog = screens_.at("dialog");
    // `dim N` of 256 darkens what's behind; Test Game 84's questions don't.
    const auto *dim = dialog.setting("dim");
    tintFrame(out, 0, 0, 0, dim && !dim->empty() ? field(*dim, 0) : 128);
    // Test Game 84 has a picture to a question, in `question` lines in the sheet's order, and fixed art besides.
    int question = 0;
    bool separate = dialog.settings.count("question") != 0;
    for (auto [it, end] = dialog.settings.equal_range("question"); it != end; ++it, question++)
        if (question == dialogQuestion_ && it->second.size() >= 3)
            if (ImageRef picture = images_->get(it->second[0])) drawPart(out, *picture, field(it->second, 1), field(it->second, 2), 0, 0, picture->w, picture->h, 1);
    for (const Picture &p : dialog.pictures) {
        Picture shown = p;
        if (p.rows > 1 && !separate) shown.row = dialogQuestion_;
        drawPicture(out, shown);
    }
    for (const Button &b : dialog.buttons) {
        bool over = pointerSeen_ && b.contains(pointerX_, pointerY_);
        bool ticked = b.action == "noask" && dialogNoAsk_;
        drawButton(out, b, ticked ? (over ? StateSelectedHover : StateSelected) : over ? StateHover : StateNormal);
    }
}

// A menu's button, or in play the message bar's.
bool Engine::pressAction(const std::string &action) {
    const std::vector<Button> *lists[] = {screen_ ? &screen_->buttons : &bar().inner, screen_ ? nullptr : &bar().under};
    for (const auto *list : lists)
        if (list)
            for (const Button &b : *list)
                if (b.action == action) {
                    std::string copy = action;
                    testPress_ = true;
                    runAction(copy);
                    testPress_ = false;
                    return true;
                }
    return false;
}

Button *Engine::buttonAt(int x, int y, std::vector<Button> &buttons) {
    // A dialog's buttons over a title stay live while the title's are dimmed.
    bool own = screen_ && &buttons == &screen_->buttons;
    for (Button &b : buttons)
        if (shown(b) && !(own && dimmedBySubmenu(b)) && b.contains(x, y)) return &b;
    return nullptr;
}

// In a title with menu stages, stage 1 is the art before the menu and stage 2 the menu itself; stage 0 shows in both.
bool Engine::shown(const Button &b) const {
    return (b.stage == 0 || b.stage == menuStage_) && (b.group.empty() || submenuOpen(b.group)) && (b.needs.empty() || anyGlobalSet(b.needs)) &&
           (b.ifRead.empty() || seen(b.ifRead)) && (b.unlessRead.empty() || !seen(b.unlessRead));
}

// Whether a music control is lit: the player's state, a mode that's on, or a ♪ played.
bool Engine::musicLit(const Button &b) const {
    return (b.action == "music_stop" && !trackPlayer_) || (b.action == "music_play" && trackPlayer_ && !trackPaused_) || (b.action == "music_pause" && trackPaused_) ||
           (b.action == "music_repeat" && trackRepeat_) || (b.action == "music_shuffle" && trackShuffle_) ||
           (b.action == "music_one" && trackRepeat_) || (b.action == "music_all" && !trackRepeat_ && !trackShuffle_) ||
           (b.action == "music_random" && trackShuffle_) || testsPlayed_.count(b.action);
}

// A picture's `ifhover` and `unlesshover`: the action of the menu button under the pointer, if any.
bool Engine::hoverAllows(const Picture &p) const {
    if (p.ifHover.empty() && p.unlessHover.empty()) return true;
    // Only a control that shows its help counts: not one disabled, nor one lit on a `nolithelp` screen.
    std::string over;
    if (screen_ && pointerSeen_ && !dialogOpen_)
        for (const Button &b : screen_->buttons)
            if (b.sheet && shown(b) && b.contains(pointerX_, pointerY_) && !(b.action == "music_pause" && !trackPlayer_) &&
                !(screen_->setting("nolithelp") && musicLit(b)))
                over = b.action;
    // Or a `hover` line's name, its tenth field, as Moral Orc's volume has its help on the part of the screen it covers.
    if (over.empty() && screen_ && pointerSeen_ && !dialogOpen_)
        for (auto [it, end] = screen_->settings.equal_range("hover"); it != end; ++it) {
            const auto &f = it->second;
            if (f.size() >= 10 && inRect(pointerX_, pointerY_, field(f, 0), field(f, 1), field(f, 2) - field(f, 0), field(f, 3) - field(f, 1))) over = f[9];
        }
    auto listed = [&](const std::vector<std::string> &list) { return std::find(list.begin(), list.end(), over) != list.end(); };
    if (!p.ifHover.empty() && !listed(p.ifHover)) return false;
    return !listed(p.unlessHover);
}

bool Engine::shown(const Picture &p) const {
    bool staged = screen_ && screen_->setting("menustages");
    return (!staged || p.stage == 0 || p.stage == menuStage_) && (p.group.empty() || submenuOpen(p.group)) && (p.needs.empty() || anyGlobalSet(p.needs)) &&
           (p.ifRead.empty() || seen(p.ifRead)) && (p.unlessRead.empty() || !seen(p.unlessRead)) && hoverAllows(p);
}

// Whether the submenu of this name is open, or one inside it.
// `submenudim` on a title: while a submenu is open, the main buttons other than its opener are drawn in their sheet's
// last row and take no pointer, as Test Games 82 and 87's titles do (sets 1 and 2, under Wine).
bool Engine::dimmedBySubmenu(const Button &b) const {
    if (!screen_ || subMenu_.empty() || !b.group.empty() || !screen_->setting("submenudim")) return false;
    bool opener = (b.action.rfind("submenu:", 0) == 0 && submenuOpen(b.action.substr(8))) || (b.action == "start" && submenuOpen("start"));
    return !opener;
}

std::string Engine::waitScale() const {
    auto defaults = screens_.find("defaults");
    const auto *line = defaults == screens_.end() ? nullptr : defaults->second.setting("waitscale");
    return line && !line->empty() ? (*line)[0] : "waittime";
}

bool Engine::submenuOpen(const std::string &name) const {
    return subMenu_ == name || subMenu_.rfind(name + "/", 0) == 0;
}

bool Engine::anyGlobalSet(const std::vector<int> &numbers) const {
    for (int number : numbers)
        if (auto it = globals_.find(number); it != globals_.end() && it->second) return true;
    return false;
}

// `opening TILE_MS FADE_FROM FADE_MS`: each quarter of the screen shows the stage 1 art from its outer edge, the top left
// growing rightward, the top right downward, the bottom right leftward and the bottom left upward, with a quadratic ease
// out, and then the stage 2 pictures and the buttons fade in. From Test Game 84 under Wine: the reveal fits 0.52 seconds
// within 4 pixels over six launches, and the fade runs from 0.92 to 1.93 seconds.
void Engine::pointerToward(float &x, float &y) const {
    x = pointerSeen_ ? std::clamp(mouseX_ * 2 - 1, -1.0f, 1.0f) : 0;
    y = pointerSeen_ ? std::clamp(mouseY_ * 2 - 1, -1.0f, 1.0f) : 0;
}

// A voice plays while its speaker's Individual Voices box is checked. A voice of several speakers, listed in the table as
// `voicejoint TYPE SPEAKER...`, plays only while all of theirs are, which the PC's cfg.sav shows for Test Game 84.
int Engine::soundGroup(int kind, const std::string &path) const {
    std::string name = normalizedPath(path);
    name = name.substr(name.find_last_of('\\') + 1);
    // `hseprefix PREFIX` under `screen defaults` names the sex effects another way, as Test Game 87's se_h files (set 1).
    std::string prefix = "hse";
    if (auto defaults = screens_.find("defaults"); defaults != screens_.end())
        if (const auto *line = defaults->second.setting("hseprefix"); line && !line->empty()) prefix = normalizedPath((*line)[0]);
    if (kind == kKindSe && name.rfind(prefix, 0) == 0) return kGroupHse;
    if (kind == kKindVoice)
        for (const auto &[start, type] : voiceTypes_)
            if (name.rfind(start, 0) == 0) return type >= 50 ? kGroupBackVoice : kKindVoice;
    return kind;
}

bool Engine::voiceAllowed(const std::string &voice) const {
    std::string name = normalizedPath(voice);
    name = name.substr(name.find_last_of('\\') + 1);
    for (const auto &[start, type] : voiceTypes_) {
        if (name.rfind(start, 0) != 0) continue;
        int who = type % 50;
        if (auto table = screens_.find("defaults"); table != screens_.end())
            for (auto [it, end] = table->second.settings.equal_range("voicejoint"); it != end; ++it)
                if (!it->second.empty() && field(it->second, 0) == who) {
                    for (size_t i = 1; i < it->second.size(); i++)
                        if (!setting("voice" + it->second[i])) return false;
                    return true;
                }
        return setting("voice" + std::to_string(who)) != 0;
    }
    return true;
}

bool Engine::composeTitleOpening(Canvas &out) {
    const auto *line = screen_->setting("opening");
    if (openingFrom_ < 0 || !line || line->size() < 3) return false;
    int64_t t = screenMs_ - openingFrom_;
    int tileMs = std::max(1, field(*line, 0)), fadeFrom = field(*line, 1), fadeMs = std::max(1, field(*line, 2));
    if (t >= fadeFrom + fadeMs) {
        openingFrom_ = -1;
        return false;
    }
    float towardX, towardY;
    pointerToward(towardX, towardY);
    auto layer = [&](Canvas &c, int upTo, bool buttons) {
        c.resize(out.w, out.h);
        c.clear();
        for (const Picture &p : screen_->pictures)
            if (p.stage <= upTo) drawPicture(c, p, towardX, towardY);
        if (buttons)
            for (Button &b : screen_->buttons) drawButton(c, b, StateNormal);
    };
    Canvas none, art, all;
    layer(none, 0, false);
    layer(art, 1, false);
    layer(all, 2, true);
    int fade = int(std::clamp(float(t - fadeFrom) / float(fadeMs), 0.0f, 1.0f) * 256);
    if (fade > 0) blendFrame(art, all, fade);
    out = none;
    // `opening TILE FADE_FROM FADE_MS [DELAY LEVEL1 LEVEL2 LEVEL3]`: each quarter is revealed four times over, each copy
    // DELAY ms behind the one before, and the art shows at LEVELn% of its brightness where n copies cover it, in full where
    // all four do. Test Game 84's 70 ms and 33, 65 and 87 were fitted from PC screenshots under Wine.
    int delay = line->size() >= 7 ? field(*line, 3) : 0;
    int levels[5] = {0, 256, 256, 256, 256};
    if (delay > 0)
        for (int n = 1; n <= 3; n++) levels[n] = field(*line, 3 + n) * 256 / 100;
    int halfW = out.w / 2, halfH = out.h / 2;
    auto reach = [&](int64_t at, int half) {
        float k = std::clamp(float(at) / float(tileMs), 0.0f, 1.0f);
        return int(float(half) * (1 - (1 - k) * (1 - k)));
    };
    // Copies a strip of the art, darkened to `level` of 256, into the frame.
    auto copy = [&](int x0, int y0, int x1, int y1, int level) {
        x0 = std::max(0, x0), y0 = std::max(0, y0), x1 = std::min(out.w, x1), y1 = std::min(out.h, y1);
        if (x1 <= x0 || level <= 0) return;
        for (int y = y0; y < y1; y++) {
            const uint8_t *from = &art.px[(size_t(y) * out.w + x0) * 4];
            uint8_t *to = &out.px[(size_t(y) * out.w + x0) * 4];
            if (level >= 256) {
                std::memcpy(to, from, size_t(x1 - x0) * 4);
                continue;
            }
            for (int x = x0; x < x1; x++, from += 4, to += 4) {
                to[0] = uint8_t(from[0] * level >> 8);
                to[1] = uint8_t(from[1] * level >> 8);
                to[2] = uint8_t(from[2] * level >> 8);
                to[3] = 255;
            }
        }
    };
    int copies = delay > 0 ? 4 : 1;
    // From the copy furthest behind, which covers least, to the leading one: each strip between two copies' edges shows at
    // the level of how many cover it.
    for (int j = copies - 1; j >= 0; j--) {
        int64_t at = t - int64_t(j) * delay, after = t - int64_t(j + 1) * delay;
        int level = levels[j + 1];  // the strip between this copy's edge and the next one in is under j + 1 copies
        int across = reach(at, halfW), down = reach(at, halfH);
        int acrossIn = j + 1 < copies ? reach(after, halfW) : 0, downIn = j + 1 < copies ? reach(after, halfH) : 0;
        copy(acrossIn, 0, across, halfH, level);                        // top left, growing rightward
        copy(halfW, downIn, out.w, down, level);                        // top right, growing downward
        copy(out.w - across, halfH, out.w - acrossIn, out.h, level);    // bottom right, growing leftward
        copy(0, out.h - down, halfW, out.h - downIn, level);            // bottom left, growing upward
    }
    return true;
}

void Engine::composeScreen(Canvas &out) {
    if (composeTitleOpening(out)) return;
    if (spriteScreen()) {
        composeSprites(out);
        if (dialogOpen_) composeDialog(out);
        return;
    }
    out.clear();
    // `over SCREEN DIM`: a panel such as Test Game 84's Individual Voices draws on that screen, darkened by DIM of 256,
    // whose own buttons don't light up under the pointer.
    if (const auto *over = screen_->setting("over"); over && over->size() >= 2) {
        if (auto under = screens_.find((*over)[0]); under != screens_.end() && &under->second != screen_) {
            Screen *panel = screen_;
            bool seen = pointerSeen_;
            screen_ = &under->second;
            pointerSeen_ = false;
            composeScreen(out);
            screen_ = panel;
            pointerSeen_ = seen;
            tintFrame(out, 0, 0, 0, field(*over, 1));
        }
    }
    float towardX, towardY;
    pointerToward(towardX, towardY);
    for (const Picture &p : screen_->pictures)
        if (p.drift) composeDrift(out);
        else if (p.dim && shown(p)) tintFrame(out, 0, 0, 0, int(p.alpha * 256));
        else if (p.blinkMs > 0 && shown(p)) {
            Picture faded = p;
            faded.alpha *= float(0.5 + 0.5 * std::cos(2 * kPi * double(screenMs_ % p.blinkMs) / p.blinkMs));
            drawPicture(out, faded, towardX, towardY);
        } else if ((p.tab < 0 || p.tab == galleryTab_) && shown(p)) {
            if (p.rise.empty()) {
                drawPicture(out, p, towardX, towardY);
                continue;
            }
            Picture risen = p;
            risen.y += riseOffset(p.rise, screenMs_ - galleryTabAt_);
            drawPicture(out, risen, towardX, towardY);
        }
    composeSlots(out);
    composeGallery(out);
    if (viewingImage_) return;
    composeTrackCards(out);
    composeTrackTime(out);
    composeSettingsWidgets(out);
    bool fromTitle = returnTo_ == "title";
    // `helplast`: hovered help draws after every button, as Test Games 92 and 93 draw their side menu's bubbles over the
    // entry above.
    bool helpLast = screen_->setting("helplast") != nullptr;
    const Button *helpOf = nullptr;
    auto drawButton = [&](Canvas &c, const Button &b, ButtonState state, bool withHelp = false) {
        if (helpLast && b.help && (withHelp || state == StateHover || state == StateSelectedHover)) {
            Button shown = b;
            shown.help = nullptr;
            neko::drawButton(c, shown, state);
            helpOf = &b;
            return;
        }
        neko::drawButton(c, b, state, withHelp);
    };
    for (Button &b : screen_->buttons) {
        if (!shown(b)) continue;
        if (b.action.rfind("track", 0) == 0) {
            size_t index = size_t(std::atoi(b.action.c_str() + 5));
            if (index >= tracks_.size() || !seen(tracks_[index])) continue;
        }
        if (b.action.rfind("page", 0) == 0 && !galleryPageExists(std::atoi(b.action.c_str() + 4))) continue;
        if (dimmedBySubmenu(b)) {
            Button shown = b;
            shown.help = nullptr;
            drawButton(out, shown, StateDisabled);
            continue;
        }
        bool over = pointerSeen_ && b.contains(pointerX_, pointerY_) && !dialogOpen_;
        ButtonState state = over ? StateHover : StateNormal;
        bool current = (!subMenu_.empty() && ((b.action.rfind("submenu:", 0) == 0 && submenuOpen(b.action.substr(8))) || (b.action == "start" && submenuOpen("start")))) || b.action == "nav_" + screen_->name || b.action == "page" + std::to_string(page_) || (b.action == "nav_config" && screen_->name.rfind("config", 0) == 0) || b.action == "gtab" + std::to_string(galleryTab_) || b.action == "express" + std::to_string(setting("express")) ||
                       (screen_->name.rfind("config", 0) == 0 && b.action == "tab" + screen_->name.substr(6));
        // `selectedlook`: the current one keeps its selected look under the pointer, as in Test Games 92 and 93.
        if (current) state = over && !screen_->setting("selectedlook") ? StateSelectedHover : StateSelected;
        // The PC games show no help for the tab, page or menu already open.
        if (current && b.help) {
            Button shown = b;
            shown.help = nullptr;
            drawButton(out, shown, state);
            continue;
        }
        if (b.action == "nav_save" && fromTitle) state = StateDisabled;
        if (pageOff(b.action) || pageStepOff(b.action)) state = StateDisabled;
        // The PC games show no help for the tab, page or menu already open.
        if (current && b.help) {
            Button shown = b;
            shown.help = nullptr;
            drawButton(out, shown, state);
            continue;
        }
        if (b.action == "quickload" && !newestQuickSlot()) state = StateDisabled;
        if (b.action == "track" + std::to_string(currentTrack_) && trackPlayer_) state = over ? StateSelectedHover : StateSelected;
        bool musicOn = musicLit(b);
        if (musicOn) state = over ? StateSelectedHover : StateSelected;
        // `nolithelp`: a lit control shows no help, as Moral Orc's music screen shows none over Stop while nothing plays.
        if (musicOn && over && screen_->setting("nolithelp")) {
            Button quiet = b;
            quiet.help = nullptr;
            drawButton(out, quiet, state);
            continue;
        }
        // `askerlit`: the button that asked a question stays lit, with its help, until it's answered, as in Test Game 84.
        if (dialogOpen_ && b.action == dialogOpener_ && screen_->setting("askerlit")) {
            drawButton(out, b, StateSelected, true);
            continue;
        }
        if (b.action == "music_pause" && !trackPlayer_) state = StateDisabled;
        // The current page's button sits higher in Test Game 84.
        if (const auto *lift = screen_->setting("pagelift"); lift && current && b.action.rfind("page", 0) == 0) {
            Button raised = b;
            raised.y -= field(*lift, 0);
            drawButton(out, raised, state);
            continue;
        }
        if (b.action == "music_toggle") {
            Button shown = b;
            shown.column = trackPlayer_ && !trackPaused_ ? 1 : 2;
            drawButton(out, shown, state);
            continue;
        }
        drawButton(out, b, state);
    }
    if (helpOf) drawButtonHelp(out, *helpOf);
    // `hover X0 Y0 X1 Y1 PICTURE X Y [ROWS ROW [NAME]]`: a line of help shown while the pointer is over that part of the screen,
    // such as a settings group in Config, as the PC games show them. Test Game 84 keeps its lines as rows of one sheet.
    bool hoverShown = false;
    if (pointerSeen_ && !dialogOpen_)
        for (auto [it, end] = screen_->settings.equal_range("hover"); it != end; ++it) {
            const auto &f = it->second;
            if (f.size() < 7 || !inRect(pointerX_, pointerY_, field(f, 0), field(f, 1), field(f, 2) - field(f, 0), field(f, 3) - field(f, 1))) continue;
            if (ImageRef help = images_->get(f[4])) {
                int rows = f.size() >= 9 ? std::max(1, field(f, 7)) : 1, h = help->h / rows;
                drawPart(out, *help, field(f, 5), field(f, 6), 0, (f.size() >= 9 ? field(f, 8) : 0) * h, help->w, h, 1);
            }
            hoverShown = true;
            break;
        }
    // `hovercols X0 Y0 X1 Y1 PICTURE X Y COLS COL`: the same with the lines side by side in the sheet, as Test Game 87's
    // help cards are (set 1).
    if (pointerSeen_ && !dialogOpen_ && !hoverShown)
        for (auto [it, end] = screen_->settings.equal_range("hovercols"); it != end; ++it) {
            const auto &f = it->second;
            if (f.size() < 9 || !inRect(pointerX_, pointerY_, field(f, 0), field(f, 1), field(f, 2) - field(f, 0), field(f, 3) - field(f, 1))) continue;
            if (ImageRef help = images_->get(f[4])) {
                int w = help->w / std::max(1, field(f, 7));
                drawPart(out, *help, field(f, 5), field(f, 6), field(f, 8) * w, 0, w, help->h, 1);
            }
            break;
        }
    // `band Y R G B`, a line for each of two or more rows: the rows from the first listed to the last are multiplied by the
    // colour between the listed rows around them, across the screen and over all it draws, its help too. Test Game 88's
    // Config darkens its bottom so on all three pages (under Wine, against its background picture: within half a level).
    std::vector<std::array<int, 4>> band;
    for (auto [it, end] = screen_->settings.equal_range("band"); it != end; ++it)
        if (it->second.size() >= 4) band.push_back({field(it->second, 0), field(it->second, 1), field(it->second, 2), field(it->second, 3)});
    std::sort(band.begin(), band.end());
    for (size_t i = 0; i + 1 < band.size(); i++)
        for (int y = std::max(0, band[i][0]); y < std::min(out.h, band[i + 1][0] + (i + 2 == band.size() ? 1 : 0)); y++) {
            float t = float(y - band[i][0]) / float(std::max(1, band[i + 1][0] - band[i][0]));
            float m[3];
            for (int c = 0; c < 3; c++) m[c] = (float(band[i][c + 1]) + (float(band[i + 1][c + 1]) - float(band[i][c + 1])) * t) / 255.0f;
            uint8_t *row = out.px.data() + size_t(y) * size_t(out.w) * 4;
            for (int x = 0; x < out.w; x++)
                for (int c = 0; c < 3; c++) row[x * 4 + c] = uint8_t(std::lround(float(row[x * 4 + c]) * m[c]));
        }
    if (dialogOpen_) composeDialog(out);
}

// `under` buttons come up with their plate when the pointer's at the left of the bar, and `inner` ones when it's anywhere else
// along it, as Test Game 97 does. A game with no plate shows its one row anywhere along the bar, as Test Game 95 does.
bool Engine::innerButtonsShown() const {
    // A plate as wide as the screen comes up with all the buttons when the pointer's on it, as in Test Game 85.
    if (fullWidthPlate()) return barPinned_ || setting("bar_always") || (pointerSeen_ && messagePointerY() >= plateY_);
    return barPinned_ || setting("bar_always") || (pointerSeen_ && messagePointerY() >= barTop_ && pointerX_ >= (buttonPlate_ ? plateX_ + buttonPlate_->w : 0));
}

// A plate as wide as the screen carries both rows of buttons, and comes up with the pointer on it, as in Test Game 85.
bool Engine::fullWidthPlate() const { return buttonPlate_ && buttonPlate_->w >= width_; }

bool Engine::underButtonsShown() const {
    if (fullWidthPlate()) return innerButtonsShown();
    return buttonPlate_ && pointerSeen_ && messagePointerY() >= plateY_ && pointerX_ < plateX_ + buttonPlate_->w;
}

void Engine::composeMessageButtons(Canvas &out) {
    // Config's explanations: the message window buttons' pop-ups, msg_button.ini's tutorial pictures, show only while
    // it's on.
    bool explain = setting("tutorial") != 0;
    auto drawButton = [&](Canvas &c, const Button &b, ButtonState st) {
        if (explain || !b.help) {
            neko::drawButton(c, b, st);
            return;
        }
        Button shown = b;
        shown.help = nullptr;
        neko::drawButton(c, shown, st);
    };
    auto state = [&](const Button &b) {
        bool over = pointerSeen_ && b.contains(pointerX_, messagePointerY());
        // The art button reads ORIGINAL.ver while the arranged art shows, and ARRANGE.ver, its selected cells, while the
        // original does.
        bool on = (b.action == "Skip" && skipToggle_) || (b.action == "Auto" && autoMode_) || (b.action == "System" && barPinned_) ||
                  (b.action == "ModeChange" && setting("art"));
        if (b.action == "QuickLoad" && !newestQuickSlot()) return StateDisabled;
        if (b.action == "Voice" && currentText_.voice.empty()) return StateDisabled;
        if (b.action == "Ejaculation" && !gt_.active) return StateDisabled;
        if (b.action == "Extension") {
            if (!gt_.active || gt_.extending) return StateDisabled;
            on = gt_.extend;
        }
        if (b.action == "GOODTiming") on = setting("gt_off") != 0;
        if (b.action == "EXpress") {
            // 切替不可 while nothing on screen has another version, as Test Games 92 and 94 show it.
            if (!artSwitchable()) return StateDisabled;
            on = setting("express") == 0;
        }
        return on ? (over ? StateSelectedHover : StateSelected) : over ? StateHover : StateNormal;
    };
    if (innerButtonsShown()) {
        if (fullWidthPlate()) drawPart(out, *buttonPlate_, plateX_, plateY_, 0, 0, buttonPlate_->w, buttonPlate_->h, 1);
        for (const Button &b : bar().inner) drawButton(out, b, state(b));
    }
    if (!underButtonsShown()) return;
    if (!fullWidthPlate()) drawPart(out, *buttonPlate_, plateX_, plateY_, 0, 0, buttonPlate_->w, buttonPlate_->h, 1);
    for (const Button &b : bar().under) drawButton(out, b, state(b));
}

// The backlog: the story's frame, dimmed, with a page of the last lines, oldest at the top.
void Engine::composeLog(Canvas &out) {
    const Screen *log = screens_.count("log") ? &screens_.at("log") : nullptr;
    const auto *dim = log ? log->setting("dim") : nullptr;
    tintFrame(out, 0, 0, 0, dim ? field(*dim, 0) : kLogDim);
    if (!log) return;
    for (const Picture &p : log->pictures) drawPicture(out, p);
    // `scroll TOP MIDDLE BOTTOM X Y HEIGHT`, a thumb in three parts, which sits at the bottom on the newest lines.
    if (const auto *scroll = log->setting("scroll"); scroll && scroll->size() >= 6 && !backlog_.empty()) {
        ImageRef top = images_->get((*scroll)[0]), middle = images_->get((*scroll)[1]), bottom = images_->get((*scroll)[2]);
        if (top && middle && bottom) {
            int x = field(*scroll, 3), trackY = field(*scroll, 4), trackH = field(*scroll, 5), total = int(backlog_.size());
            const auto *rows = log->setting("rows");
            int shown = std::min(rows ? field(*rows, 4) : 1, total);
            int thumbH = std::max(top->h + bottom->h, trackH * shown / total);
            int y = trackY + (trackH - thumbH) * (total - shown - std::min(logTop_, total - shown)) / std::max(1, total - shown);
            drawPart(out, *top, x, y, 0, 0, top->w, top->h, 1);
            for (int my = y + top->h; my < y + thumbH - bottom->h; my += middle->h) drawPart(out, *middle, x, my, 0, 0, middle->w, std::min(middle->h, y + thumbH - bottom->h - my), 1);
            drawPart(out, *bottom, x, y + thumbH - bottom->h, 0, 0, bottom->w, bottom->h, 1);
        }
    }
    // `thumb PICTURE X TOP HEIGHT`, a thumb of its own size that travels HEIGHT down from TOP, at the bottom on the newest
    // lines, as Test Game 89's does.
    if (const auto *thumb = log->setting("thumb"); thumb && thumb->size() >= 4 && !backlog_.empty())
        if (ImageRef picture = images_->get((*thumb)[0])) {
            const auto *rows = log->setting("rows");
            int total = int(backlog_.size()), shown = std::min(rows ? field(*rows, 4) : 1, total);
            int travel = field(*thumb, 3) - picture->h, from = std::min(logTop_, total - shown);
            int y = field(*thumb, 2) + (total <= shown ? travel : travel * (total - shown - from) / (total - shown));
            drawPart(out, *picture, field(*thumb, 1), y, 0, 0, picture->w, picture->h, 1);
        }
    for (const Button &b : log->buttons) drawButton(out, b, pointerSeen_ && b.contains(pointerX_, pointerY_) ? StateHover : StateNormal);
    const auto *rows = log->setting("rows");
    if (!rows || rows->size() < 7) return;
    ImageRef rule = images_->get((*rows)[0]);
    int ruleX = std::atoi((*rows)[1].c_str()), ruleY = std::atoi((*rows)[2].c_str()), step = std::atoi((*rows)[3].c_str());
    int count = std::atoi((*rows)[4].c_str()), textX = std::atoi((*rows)[5].c_str()), textY = std::atoi((*rows)[6].c_str());
    int lastRow = std::min(count, int(backlog_.size()) - logTop_);
    // `rows ... SIZE`, `loggap G` and `logline N`: the log's own text, where the PC sets it apart from the message window's.
    struct Saved { int size; float gap; int line; } saved{fontSize_, textGap_, textLine_};
    if (rows->size() >= 8) fontSize_ = field(*rows, 7);
    if (const auto *f = log->setting("loggap"); f && !f->empty()) textGap_ = float(std::atof((*f)[0].c_str()));
    if (const auto *f = log->setting("logline"); f && !f->empty()) textLine_ = field(*f, 0);
    int ascent = fontAscent(fontFace_, fontSize_);
    for (int row = 0; row < lastRow; row++) {
        // Row 0 is the oldest on the page: the backlog runs newest first.
        const SavedText &t = backlog_[size_t(logTop_ + lastRow - 1 - row)];
        int y = ruleY + row * step;
        if (rule) drawPart(out, *rule, ruleX, y, 0, 0, rule->w, rule->h, 1);
        std::string text = sjis::toUtf8(t.text);
        std::string name = sjis::toUtf8(t.name);
        std::vector<Glyph> glyphs;
        // A name beside the text hangs to its left, and one above sits as far above it as in the message window.
        bool beside = nameY_ == textY_;
        layoutRun(name, beside ? textX - (textX_ - nameX_) : textX, y + textY + ascent + (beside ? 0 : nameY_ - textY_), beside, glyphs);
        drawGlyphs(out, glyphs, logTextR_, logTextG_, logTextB_, logFrameR_, logFrameG_, logFrameB_);
        glyphs.clear();
        layoutRun(text, textX, y + textY + ascent, false, glyphs);
        drawGlyphs(out, glyphs, logTextR_, logTextG_, logTextB_, logFrameR_, logFrameG_, logFrameB_);
    }
    fontSize_ = saved.size;
    textGap_ = saved.gap;
    textLine_ = saved.line;
}

std::shared_ptr<Script> Engine::loadScript(const std::string &path) {
    std::string key = normalizedPath(path);
    auto it = scripts_.find(key);
    if (it != scripts_.end()) return it->second;
    // The story table's flows, and a game part's chapters, which come from the table and the part, not the game's files.
    if (path.rfind("story:", 0) == 0) {
        auto script = compileStory(path.substr(6));
        if (script) scripts_[key] = script;
        return script;
    }
    if (path.rfind("chapter:", 0) == 0) return compileChapter(std::atoi(path.c_str() + 8));
    Bytes data = files_.read(path);
    if (!data) {
        complain("Missing script " + path);
        return nullptr;
    }
    auto script = std::make_shared<Script>();
    if (!parseScript(data->data(), data->size(), *script)) {
        complain("Unreadable script " + path);
        return nullptr;
    }
    script->name = path;
    for (int id : script->duplicateIds) warnOnce(path + " has two nodes numbered " + std::to_string(id) + ", and the first is followed");
    scripts_[key] = script;
    return script;
}

// MARK: The interpreter

// Looks ahead along the scene for the next few pictures. A branch-free walk, which is what scene scripts are.
void Engine::prefetchAhead() {
    if (!scene_.node || scene_.node == prefetchedFrom_) return;
    prefetchedFrom_ = scene_.node;
    std::vector<std::pair<std::string, int>> wanted;
    std::map<int, std::string> inSlot;
    const Node *n = scene_.node;
    for (int steps = 0; n && steps < 120 && wanted.size() < 6; steps++) {
        if (n->opcode == 15 || n->opcode == 23) {
            wanted.emplace_back(n->s[1], 0);
            inSlot[n->p[3]] = n->s[1];
        } else if (n->opcode == 18 && n->p[21]) {
            // A blur is slower than the unpacking, so it comes ready too.
            auto it = inSlot.find(n->p[3]);
            std::string path = it != inSlot.end() ? it->second : "";
            if (path.empty()) {
                auto now = back_.find(n->p[3]);
                if (now != back_.end()) path = now->second.path;
            }
            if (!path.empty()) wanted.emplace_back(path, (n->p[20] + 4) / 8);
        }
        n = scene_.script->node(n->next);
    }
    if (!wanted.empty()) images_->prefetch(wanted);
}

void Engine::advance() {
    Frame &frame = scene_.node ? scene_ : stack_.back();
    frame.node = frame.script->node(frame.node->next);
}

void Engine::run() {
    // Bounded so a script that loops without waiting can't hang the app. No library script comes near it in one step.
    for (int guard = 0; guard < 200000; guard++) {
        if (waiting_ != Waiting::None || screen_) return;
        if (scene_.node) {
            const Node &n = *scene_.node;
            // The PC's read.sav marks every node it runs, not only text, and its read-only skip goes by them.
            if (!jumping_ && scene_.script && readNodes_[readKey(scene_.script->name)].insert(n.id).second) {
                readChanged_ = true;
                textFresh_ = n.opcode == 5;
            }
            runScene(n);
            if (scene_.node == &n) advance();
            continue;
        }
        // A scene's script read to its end unlocks it in the Scene gallery. On the PC, leaving partway doesn't.
        if (scene_.script) noteSeen(scene_.script->name);
        if (gtNextScript()) continue;
        scene_ = {};
        if (stack_.empty() || !stack_.back().node) {
            if (!stack_.empty()) stack_.pop_back();
            if (stack_.empty()) {
                waiting_ = Waiting::Ended;
                log("the flow ended");
                return;
            }
            continue;
        }
        // On to the next node first, so a jump, even to the node itself, replaces it. The script is held, since a jump to
        // another flow lets go of this one.
        auto script = stack_.back().script;
        const Node &n = *stack_.back().node;
        stack_.back().node = script->node(n.next);
        runFlow(n);
    }
    warnOnce("A script ran too long without waiting.");
}

void Engine::runFlow(const Node &n) {
    Frame &flow = stack_.back();
    switch (n.opcode) {
    case 0:   // the start marker
    case 43:  // a debug print
        break;
    case 1: {  // call a scene
        auto script = loadScript(n.s[1]);
        if (script && script->first()) {
            scene_ = {script, script->first()};
            lastScene_ = script->name;
            // A story table's line saves the PC's own number for its place, when the table gives one.
            flowCallId_ = flow.script->name.rfind("story:", 0) == 0 && n.p[kStoryPcNumber] ? n.p[kStoryPcNumber] : n.id;
            log("scene %s", script->name.c_str());
        }
        break;
    }
    case 2:  // the end of the story, which goes back to the title, or with parameter 0 set the staff roll, then on
        if (n.p[0] == 1) {
            flow.node = flow.script->node(n.next);
            startStaffRoll();
            break;
        }
        log("flow end: %s", sjis::toUtf8(n.s[0]).c_str());
        flow.node = nullptr;
        if (screens_.count("title")) endStory("title");
        break;
    case 22:  // the end of a replay, which goes back to the scene gallery
        flow.node = nullptr;
        if (replaying_) endStory("scene");
        break;
    case 3:
    case 20:  // the same in a replay
        beginChoice(n);
        break;
    case 4:  // the other target when the option in parameter 0 was chosen, `next` otherwise: a save the PC wrote after the first
    case 21:
        flow.node = flow.script->node(choiceResult_ == (n.opcode == 4 ? n.p[0] : 0) ? n.otherwise : n.next);
        break;
    case 5: {  // a condition on a variable: the other target when it holds, `next` when it doesn't, as a choice's branch
        // The comparisons, as Test Game 94's exe orders them: ==, !=, <=, >=, <, >.
        int left = variableValue(n.p[0], variableIndex(n)), right = operand(n);
        bool holds = n.p[3] == 0 ? left == right : n.p[3] == 1 ? left != right : n.p[3] == 2 ? left <= right : n.p[3] == 3 ? left >= right : n.p[3] == 4 ? left < right : left > right;
        flow.node = flow.script->node(holds ? n.otherwise : n.next);
        break;
    }
    case 6: {  // a change to a variable: set, add one, take one, add or take the operand, as the exe orders them
        auto &table = n.p[0] == 1 ? scenario_ : globals_;
        int number = variableNumber(n.p[0], variableIndex(n)), value = operand(n);
        switch (n.p[3]) {
        case 0: table[number] = value; break;
        case 1: table[number]++; break;
        case 2: table[number]--; break;
        case 3: table[number] += value; break;
        case 4: table[number] -= value; break;
        default: warnOnce("variable change " + std::to_string(n.p[3]) + " isn't built: " + sjis::toUtf8(n.s[0])); break;
        }
        log("variable %d:%d = %d", n.p[0], number, table[number]);
        if (n.p[0] != 1) saveGlobals();
        break;
    }
    case 31: {  // go on in another flow script
        auto next = loadScript(n.s[1]);
        flow.script = next;
        flow.node = next ? next->first() : nullptr;
        break;
    }
    case 1001:  // the story table's goto
        flow.node = flow.script->node(n.otherwise);
        break;
    case 1002: {  // the story table's `open`: a game part's screen, which the flow waits on
        std::vector<int> args(n.p.begin() + 4, n.p.begin() + 4 + std::clamp(n.p[3], 0, 24));
        partResultScope_ = n.p[0];
        partResultNumber_ = n.p[1];
        flow.node = flow.script->node(n.next);
        bool opened = false;
        for (auto &[name, part] : parts_) {
            // Set first, so a screen that finishes as it opens finds itself the one open.
            partScreen_ = part.get();
            if (part->openScreen(*partHost_, n.s[1], args)) {
                opened = true;
                break;
            }
        }
        if (!opened) {
            partScreen_ = nullptr;
            complain("No game part opens the screen " + n.s[1]);
        } else if (partScreen_) {
            waiting_ = Waiting::PartScreen;
        }
        dirty_ = true;
        break;
    }
    case 1003: {  // the story table's `chapter N`: the scenes a game part gives, then on
        flow.node = flow.script->node(n.next);
        if (auto chapter = loadScript("chapter:" + std::to_string(n.p[0]))) stack_.push_back({chapter, chapter->first()});
        break;
    }
    default:
        warnOnce("flow opcode " + std::to_string(n.opcode) + " isn't built: " + sjis::toUtf8(n.s[0]));
        break;
    }
}

// A condition or a change names its variable by scope, parameter 0, and its place in that scope's list: parameter 1 for
// a global, 2 for a scenario variable. The editor keeps both, so the other can hold an old choice, as in Test Game 94.
int Engine::variableIndex(const Node &n) {
    return n.p[0] == 1 ? n.p[2] : n.p[1];
}

// The other side of a condition or a change: parameter 5 itself when parameter 4 is 2, else the global (0) or scenario
// variable (1) parameter 5 numbers, by its number rather than its place in a list, as the exe reads it.
int Engine::operand(const Node &n) const {
    if (n.p[4] == 2) return n.p[5];
    const auto &table = n.p[4] == 1 ? scenario_ : globals_;
    auto it = table.find(n.p[5]);
    return it == table.end() ? 0 : it->second;
}

// Scripts name a variable by its place in macrolist.ini's list, and saves keep it by the number the list gives it.
int Engine::variableNumber(int scope, int index) const {
    const auto &list = scope == 1 ? scenarioMacros_ : globalMacros_;
    return index >= 0 && index < int(list.size()) ? list[size_t(index)] : index;
}

int Engine::variableValue(int scope, int index) const {
    const auto &table = scope == 1 ? scenario_ : globals_;
    auto it = table.find(variableNumber(scope, index));
    return it == table.end() ? 0 : it->second;
}

void Engine::beginChoice(const Node &n) {
    choiceSheet_ = images_->get(n.s[1]);
    choiceImage_ = n.s[1];
    // A route's answer is 0 for the upper option or 1 for the lower. A choice that doesn't matter to the route gives 4,
    // and the PC asks it as usual, as seen under Wine.
    if (int answer = route() >= 1 && route() <= 4 && n.opcode == 3 ? n.p[size_t(9 + route())] : -1; answer == 0 || answer == 1) {
        choiceSheet_ = nullptr;
        choiceResult_ = answer;
        choiceMade_ = true;
        log("choice %s made for route %d: %d", n.s[1].c_str(), route(), answer);
        return;
    }
    if (!choiceSheet_) {
        complain("Missing choice " + n.s[1]);
        choiceResult_ = 0;
        return;
    }
    // A replay's choice gives no count. The one seen, in Test Game 84, has two options. A choice's count is parameter 0
    // and one in every game's flows; parameter 1 is it too in most, but 1 in Test Game 94's two-option choices.
    choiceCount_ = n.opcode == 20 ? 2 : std::max(1, n.p[0] + 1);
    for (int r = 0; r < 4; r++) choiceRoutes_[size_t(r)] = n.opcode == 3 ? n.p[size_t(10 + r)] : -1;
    choiceCells_.clear();
    choiceClosingFrom_ = -1;
    if (n.opcode == 3)
        for (int i = 0; i < std::min(n.p[kChoiceCells], int(kChoiceCellsMax)); i++) choiceCells_.push_back(n.p[size_t(kChoiceCells + 1 + i)]);
    choiceBack_ = images_->get(config_.text("ChoiceBackFilename", "obj\\n000_000.bmp"));
    choiceHover_ = -1;
    waiting_ = Waiting::Choice;
    dirty_ = true;
    log("choice %s, %d options", n.s[1].c_str(), choiceCount_);
}

void Engine::choose(int index) {
    if (waiting_ != Waiting::Choice) return;
    choiceResult_ = index;
    choiceMade_ = true;
    log("chose %d", index);
    // `close MS SCALE`: the chosen option shows pressed, and the box shrinks about its middle to SCALE and fades out over
    // MS before the story goes on, as Test Game 82's does (set 2, from Wine shots).
    if (const auto *close = choiceTable("close"); close && !close->empty() && field(*close, 0) > 0) {
        choiceHover_ = index;
        choiceClosingFrom_ = now_;
        waiting_ = Waiting::Time;
        waitUntil_ = now_ + field(*close, 0);
        dirty_ = true;
        return;
    }
    choiceSheet_ = nullptr;
    choiceBack_ = nullptr;
    waiting_ = Waiting::None;
    dirty_ = true;
}

// The choice box closing: each of its pictures and options, the chosen one pressed (row 2), drawn smaller about the box's
// middle, the first picture's, and fainter as it goes.
void Engine::composeChoice(Canvas &out) {
    const auto *close = choiceTable("close");
    int ms = close && !close->empty() ? std::max(1, field(*close, 0)) : 1;
    float end = close && close->size() >= 2 ? float(std::atof((*close)[1].c_str())) : 1;
    float t = std::clamp(float(now_ - choiceClosingFrom_) / float(ms), 0.0f, 1.0f);
    float scale = 1 - (1 - end) * t, alpha = 1 - t;
    auto table = screens_.find("choice");
    float cx = float(width_) / 2, cy = float(height_) / 2;
    std::vector<std::pair<Image, std::pair<float, float>>> pieces;  // a piece's pixels and its middle
    auto crop = [](const Image &from, int x, int y, int w, int h) {
        Image part;
        part.w = std::max(0, std::min(w, from.w - x));
        part.h = std::max(0, std::min(h, from.h - y));
        part.opaque = from.opaque;
        part.rgba.resize(size_t(part.w) * size_t(part.h) * 4);
        for (int row = 0; row < part.h; row++)
            std::copy_n(&from.rgba[(size_t(y + row) * size_t(from.w) + size_t(x)) * 4], size_t(part.w) * 4, &part.rgba[size_t(row) * size_t(part.w) * 4]);
        return part;
    };
    if (table != screens_.end()) {
        bool first = true;
        for (const Picture &p : table->second.pictures) {
            if (!p.image) continue;
            int w = p.image->w / std::max(1, p.cols), h = p.image->h / std::max(1, p.rows);
            if (first) {
                cx = float(p.x) + float(w) / 2;
                cy = float(p.y) + float(h) / 2;
                first = false;
            }
            pieces.push_back({crop(*p.image, p.column * w, p.row * h, w, h), {float(p.x) + float(w) / 2, float(p.y) + float(h) / 2}});
        }
    }
    int cellW = choiceSheet_->w / choiceColumns(), cellH = choiceSheet_->h / 5, x, top, step, slant;
    choicePlace(x, top, step, slant);
    for (int i = 0; i < choiceCount_; i++) {
        int column = size_t(i) < choiceCells_.size() ? choiceCells_[size_t(i)] : i;
        pieces.push_back({crop(*choiceSheet_, column * cellW, (i == choiceResult_ ? 2 : 0) * cellH, cellW, cellH),
                          {float(x + i * slant) + float(cellW) / 2, float(top + i * step) + float(cellH) / 2}});
    }
    for (const auto &[image, middle] : pieces)
        drawImage(out, image, cx + (middle.first - cx) * scale, cy + (middle.second - cy) * scale, scale, scale, 0, alpha);
    dirty_ = true;
}

int Engine::choiceColumns() const {
    const auto *columns = choiceTable("choicecolumns");
    return columns && !columns->empty() && field(*columns, 0) > 0 ? field(*columns, 0) : choiceCount_;
}

void Engine::runScene(const Node &n) {
    const auto &p = n.p;
    switch (n.opcode) {
    case 0:  // the start marker
    case 1:  // the first node's copy of the scene name, seen only with no parameters
        break;
    case 2:  // wait, unless Config turns waits off
        if (!setting("wait")) break;
        waiting_ = Waiting::Time;
        waitUntil_ = now_ + int64_t(p[0]) * setting(waitScale()) / 100;
        break;
    case 4:  // hide the text window
        if (windowShown_) dirty_ = true;
        windowShown_ = false;
        nameGlyphs_.clear();
        textGlyphs_.clear();
        break;
    case 5:
        showText(n);
        break;
    case 9:
        screenUpdate(n);
        break;
    case 10:  // the older layout's background, CG and standing pictures
    case 11:
    case 12:
    case 13:
    case 14:
    case 21:
        runOldLayout(n);
        break;
    case 15:  // load an image into a slot
    case 23:  // load an image for a difference switch: the same, shown by the next update
        loadImage(n);
        break;
    case 100:  // load a movie into a slot
        loadMovie(n);
        break;
    case 101:  // delete the movie in a slot, which goes like a deleted image at the next update
        back_.erase(p[0]);
        break;
    case 17: {  // delete a slot
        back_.erase(p[0]);
        break;
    }
    case 18:
        transform(n);
        break;
    case 19:  // with Config's effects or camera work off, these moves and shakes don't run
        if (setting("effect") && (setting("camera") || !isCgSlot(n.p[0]))) startNamedAnimation(n);
        break;
    case 20:
        stopNamedAnimation(p[0]);
        break;
    case 22:  // delete every slot
        back_.clear();
        break;
    case 25:
        transformOverTime(n);
        break;
    case 26:
        abortChange(p[0]);
        break;
    case 30:
        playSound(n, kKindBgm);
        break;
    case 31:
        stopChannel(kKindBgm, p[0], p[1]);
        break;
    case 32:
        stopKind(kKindBgm, p[1]);
        break;
    case 33:
        changeVolume(kKindBgm, p[0], p[1], p[2]);
        break;
    case 35:
        playSound(n, kKindSe);
        break;
    case 36:
        stopChannel(kKindSe, p[0], p[1]);
        break;
    case 37:
        stopKind(kKindSe, p[1]);
        break;
    case 38:
        changeVolume(kKindSe, p[0], p[1], p[2]);
        break;
    case 39: {  // move a sound effect left or right
        auto it = se_.find(p[0]);
        if (it != se_.end()) audio_.setPan(it->second, p[1], p[2]);
        break;
    }
    case 40:
        stopKind(kKindBgm, p[1]);
        stopKind(kKindSe, p[1]);
        stopKind(kKindVoice, p[1]);
        break;
    case 41:
        playSound(n, kKindVoice);
        break;
    case 42:
        stopChannel(kKindVoice, p[0], p[1]);
        break;
    case 50:  // flash: a colour over the screen that fades out
        if (!setting("effect")) break;
        flashMs_ = std::max(1, p[0]);
        flashR_ = p[1];
        flashG_ = p[2];
        flashB_ = p[3];
        flashStart_ = now_;
        dirty_ = true;
        break;
    case 51: {  // film noise over the scene, a picture in slot 10000 whatever the node says, as the PC's exe has it
        ImageRef frame = images_->get("effect\\noise2_small.bmp");
        if (!frame) frame = images_->get("sys\\noise2_small.bmp");
        if (!frame) {
            warnOnce("no noise picture");
            break;
        }
        Sprite &s = back_[kNoiseSlot];
        s = {};
        s.image = frame;
        s.noise = true;
        s.noiseFrom = now_;
        break;
    }
    case 52:
        back_.erase(kNoiseSlot);
        break;
    case 53:  // shake the screen
        if (!setting("effect")) break;
        shakeType_ = p[0];
        shakeSustained_ = false;
        shakeStart_ = now_;
        shakeStopAt_ = -1;
        break;
    case 55:  // a hand-held camera's sway, until stopped: up and down, then sideways, each with a period in frames
        if (!setting("effect")) break;
        shakeType_ = p[0];
        shakeSustained_ = true;
        shakeAmpY_ = float(p[1]);
        shakePeriodY_ = float(std::max(1, p[2])) * 1000 / float(std::max(1, fps_));
        shakeAmpX_ = float(p[3]);
        shakePeriodX_ = float(std::max(1, p[4])) * 1000 / float(std::max(1, fps_));
        shakeStart_ = now_;
        shakeStopAt_ = -1;
        break;
    case 115:  // walking: a shake with a small amplitude and step on each axis, until stopped
        if (!setting("effect")) break;
        shakeType_ = p[0];
        shakeSustained_ = true;
        shakeAmpY_ = float(p[1]);
        shakePeriodY_ = float(std::max(1, p[2])) * 100;
        shakeAmpX_ = float(p[3]);
        shakePeriodX_ = float(std::max(1, p[2])) * 200;
        shakeStart_ = now_;
        shakeStopAt_ = -1;
        break;
    case 54:  // stop shaking, over p[0] ms
        shakeStopAt_ = now_;
        shakeStopMs_ = p[0];
        if (p[0] <= 0) shakeType_ = 0;
        break;
    case 74:  // motion blur on: each frame keeps this much of the last
        if (!setting("effect")) break;
        blur_ = true;
        blurEnding_ = false;
        blurAmount_ = std::clamp(p[0], 0, 240);
        previousValid_ = false;
        break;
    case 75:  // motion blur off, over p[0] ms
        blurEnding_ = true;
        blurEndMs_ = std::max(1, p[0]);
        blurEndStart_ = now_;
        break;
    case 98:  // a ripple on a slot: speed, length, size
    case 99:  // and off
        for (auto *set : {&back_, &front_})
            if (auto it = set->find(p[0]); it != set->end()) {
                it->second.waveOn = n.opcode == 98;
                it->second.waveSpeed = p[1];
                it->second.waveRows = p[2];
                it->second.waveSize = p[3];
            }
        break;
    case 116:  // drives an external accessory, which VNDegen doesn't have
        break;
    case 121:
        startFlipbook(n);
        break;
    case 34:  // a music channel moves left or right
        if (auto it = bgm_.find(p[0]); it != bgm_.end()) audio_.setPan(it->second, p[1], p[2]);
        break;
    case 43:  // every voice stops
        stopKind(kKindVoice, p[0]);
        break;
    case 44:
        changeVolume(kKindVoice, p[0], p[1], p[2]);
        break;
    case 60:  // a replay from the gallery goes the other way here, to the scene's end
        if (replaying_) scene_.node = scene_.script->node(n.otherwise);
        break;
    case 89: {  // a line for the backlog that isn't shown
        SavedText line;
        line.name = n.s[1];
        line.text = n.s[2];
        backlog_.insert(backlog_.begin(), line);
        if (backlog_.size() > 60) backlog_.pop_back();
        break;
    }
    case 200: {
        Splash &splash = splashes_[p[0]];
        splash.image = images_->get(n.s[1]);
        splash.path = n.s[1];
        // What a save keeps of it: parameters 0, 5, 1 to 4, 6, 10, 11 and 7 to 9, as Test Game 88's saves hold them.
        splash.saved = {p[0], p[5], p[1], p[2], p[3], p[4], p[6], p[10], p[11], p[7], p[8], p[9]};
        splash.cap = std::max(1, p[5]);
        splash.minLife = std::max(1, p[10]);
        splash.maxLife = std::max(splash.minLife, p[11]);
        splash.speed = p[12];
        splash.droplets.clear();
        break;
    }
    case 201:
        splashBurst(n);
        break;
    case 153:
        gtBegin(n);
        break;
    case 154:
        gtReachMarker(n);
        break;
    case 120: {  // a sprite moves to another slot
        for (auto *set : {&back_, &front_}) {
            auto it = set->find(p[0]);
            if (it == set->end()) continue;
            (*set)[p[1]] = std::move(it->second);
            set->erase(p[0]);
        }
        break;
    }
    case 140:  // slots from this one up don't shake
        shakeLock_ = std::atoi(n.s[1].c_str());
        break;
    case 141:
        shakeLock_ = 1 << 30;
        break;
    default:
        if (partCommand(n)) break;
        warnOnce("scene opcode " + std::to_string(n.opcode) + " isn't built: " + sjis::toUtf8(n.s[0]));
        break;
    }
}

bool Engine::step(int ms) {
    if (paused_) return waiting_ != Waiting::Ended;
    if (bool square = wantsSquare(); square != square_) {
        square_ = square;
        dirty_ = true;
        log("frame %s", square ? "4:3" : "16:9");
    }
    // The player's track ending comes first, since its screen is a menu.
    if (trackPlayer_ && !trackPaused_ && !audio_.playing(trackPlayer_)) {
        int next = nextTrack(1);
        if (next < 0) stopTrack();
        else playTrack(next);
        // A track that starts on its own brings its page of the list up, as Test Game 84's does after the last track.
        if (next >= 0 && screen_ && screen_->name == "music")
            if (int perPage = int(screen_->settings.count("cell"))) page_ = next / perPage;
        dirty_ = true;
    }
    // A menu only redraws when something changes, and the music screen's clock and bar change as a track plays.
    if (screen_ && trackPlayer_ && !trackPaused_ && screen_->settings.count("tracktime")) {
        int64_t quarter = audio_.positionMs(trackPlayer_) / 250;
        if (quarter != trackQuarterShown_) {
            trackQuarterShown_ = quarter;
            dirty_ = true;
        }
    }
    if (screen_) {
        screenMs_ += ms;
        stepSprites();
        if (openingFrom_ >= 0 || screen_->settings.count("drift") || screen_->setting("menustages")) dirty_ = true;
        // A gallery's character coming up after its tab opens.
        if (screenMs_ - galleryTabAt_ < 400) dirty_ = true;
        stepViewer(ms);
    }
    if (screen_ || logOpen_ || dialogOpen_) {
        // The story's clock stops, and a movie's sound with it.
        stopMovieSound();
        return waiting_ != Waiting::Ended;
    }
    now_ += ms;
    for (auto &[name, part] : parts_)
        if (part->step(*partHost_)) dirty_ = true;
    advanceFlipbooks();
    stepMovies();
    for (auto &[slot, splash] : splashes_) {
        auto gone = [this](const Droplet &d) { return now_ - d.bornMs >= d.lifeMs; };
        auto end = std::remove_if(splash.droplets.begin(), splash.droplets.end(), gone);
        if (end != splash.droplets.end()) dirty_ = true;
        splash.droplets.erase(end, splash.droplets.end());
    }

    if (transitioning_ && (skip_ || skipToggle_ || now_ >= transitionStart_ + transitionMs_)) {
        transitioning_ = false;
        ruleImage_ = nullptr;
        dirty_ = true;
    }
    if (waiting_ == Waiting::StaffRoll) stepStaffRoll();
    // Config's wait skipping lets a click end a wait.
    if (waiting_ == Waiting::Time && (skip_ || skipToggle_ || now_ >= waitUntil_ || (clicked_ && setting("waitskip")))) {
        waiting_ = Waiting::None;
        clicked_ = false;
    }
    // A choice box that has finished closing goes, and the story goes on.
    if (choiceClosingFrom_ >= 0 && waiting_ != Waiting::Time) {
        choiceClosingFrom_ = -1;
        choiceSheet_ = nullptr;
        choiceBack_ = nullptr;
        dirty_ = true;
    }

    if (!textDone_) {
        // The last character has finished fading in.
        dirty_ = true;
        if (skip_ || now_ - textStart_ >= int64_t(textGlyphs_.size()) * charMs_ + kCharFadeMs) textDone_ = true;
    }
    bool skipping = skip_ || skipToggle_;
    // Auto moves on a moment after the text and any voice have finished.
    bool autoNext = false;
    if (autoMode_ && waiting_ == Waiting::Click && textDone_) {
        bool voiceGoing = voicePlayer_ && audio_.playing(voicePlayer_);
        if (voiceGoing) autoReadyAt_ = -1;
        else if (autoReadyAt_ < 0) autoReadyAt_ = now_ + (autoPerCharMs_ >= 0 ? autoPerCharMs_ * int64_t(textGlyphs_.size()) : autoDelayMs_);
        else if (now_ >= autoReadyAt_) autoNext = true;
    } else {
        autoReadyAt_ = -1;
    }
    if (waiting_ == Waiting::Click && (clicked_ || skipping || autoNext)) {
        bool clickedNow = clicked_;
        clicked_ = false;
        if (!textDone_ && !skipping) {
            // The first click finishes the text; the next one moves on.
            textDone_ = true;
            dirty_ = true;
        } else {
            // Config's クリック時、ボイスを止める: off, a click lets the line's voice play on until the next line's.
            if (voicePlayer_ && (!clickedNow || setting("voice_stop"))) {
                audio_.close(voicePlayer_);
                voicePlayer_ = 0;
            }
            waiting_ = Waiting::None;
        }
    }
    clicked_ = false;
    if (mouseMoved_) {
        mouseMoved_ = false;
        updateChoice();
    }
    run();
    prefetchAhead();
    innerWereShown_ = innerButtonsShown();
    underWereShown_ = underButtonsShown();

    for (auto *set : {&back_, &front_})
        for (auto &[slot, s] : *set) foldFinishedChanges(s);
    if (anythingMoves()) dirty_ = true;
    return waiting_ != Waiting::Ended;
}

void Engine::setPaused(bool paused) {
    paused_ = paused;
    // The next step after a resume picks the movie's sound up again.
    if (paused) {
        stopMovieSound();
        stopViewerMovieSound();
    }
    for (auto *channels : {&se_, &voice_})
        for (auto &[channel, player] : *channels) audio_.pause(player, paused);
    holdMusic();
}

void Engine::mouseMove(float x, float y) {
    mouseX_ = x;
    mouseY_ = y;
    mouseMoved_ = true;
    int px = int(x * float(width_)), py = int(y * float(square_ ? squareY_ + squareShift_ : height_));
    bool moved = px != pointerX_ || py != pointerY_;
    if (moved || !pointerSeen_) dirty_ = true;
    pointerX_ = px;
    pointerY_ = py;
    pointerSeen_ = true;
    if (spriteScreen() && moved) spritePointerMoved();
    if (partScreen_ && moved && !screen_ && !dialogOpen_ && !logOpen_) partScreen_->pointer(*partHost_, px, py);
    if (leftHeld_ && screen_ && !dialogOpen_ && !logOpen_) {
        if (viewingImage_ && viewerPanel()) dragViewerSlider(px, py);
        else pressSettingsWidget(px, py, true);
    }
}

void Engine::mouseButton(int button, bool down) {
    if (button == 0) {
        // The CG viewer's volume is kept when the slider's let go.
        if (!down && leftHeld_ && viewerDragging_) {
            viewerDragging_ = false;
            saveSettings();
        }
        leftHeld_ = down;
    }
    // A click during the title's opening ends it and does nothing more.
    if (down && openingFrom_ >= 0 && screen_ && screen_->name == "title") {
        openingFrom_ = -1;
        dirty_ = true;
        return;
    }
    if (!down) return;
    // A game part's open screen takes the presses, unless a menu or dialog is over it.
    if (partScreen_ && waiting_ == Waiting::PartScreen && !screen_ && !dialogOpen_ && !logOpen_) {
        if (partScreen_->press(*partHost_, pointerX_, pointerY_, button)) dirty_ = true;
        return;
    }
    // `mousechoice`: the right button takes a choice's last option, as Test Game 94's mouse icons say.
    if (button == 1 && waiting_ == Waiting::Choice && choiceSheet_ && !screen_ && !dialogOpen_ && !logOpen_ && choiceTable("mousechoice")) {
        choose(choiceCount_ - 1);
        return;
    }
    if (button == 1) {
        // The right button steps back: out of the log or a menu, else hides or shows the window.
        if (dialogOpen_) {
            answerDialog(false);
        } else if (logOpen_) {
            closeLog();
        } else if (screen_ && viewingImage_ && viewerPanel()) {
            viewPanelHidden_ = !viewPanelHidden_;
            dirty_ = true;
        } else if (screen_) {
            // `cancel ACTION` names what the right button does on a screen that isn't left with Back.
            if (screen_->setting("menustages") && menuStage_ >= 2) {
                menuStage_ = 1;
                subMenu_.clear();
                dirty_ = true;
            } else if (screen_->setting("submenudim") && !subMenu_.empty()) {
                size_t slash = subMenu_.rfind('/');
                subMenu_ = slash == std::string::npos ? "" : subMenu_.substr(0, slash);
                dirty_ = true;
            } else if (const auto *cancel = screen_->setting("cancel"); cancel && !cancel->empty()) runAction((*cancel)[0]);
            else if (screen_->name != "title") runAction("back");
        } else if (windowHiddenByPlayer_ || (windowShown_ && setting("rightclick") == 0)) {
            windowHiddenByPlayer_ = !windowHiddenByPlayer_;
            dirty_ = true;
        } else if (windowShown_) {
            static const char *const kRightClick[] = {"HideWin", "Skip", "Config", "Save", "Load"};
            runAction(kRightClick[std::clamp(setting("rightclick"), 0, 4)]);
        }
        return;
    }
    if (button != 0) return;
    if (windowHiddenByPlayer_) {
        windowHiddenByPlayer_ = false;
        dirty_ = true;
        return;
    }
    if (logOpen_ && !dialogOpen_) {
        Screen &log = screens_.at("log");
        if (Button *b = buttonAt(pointerX_, pointerY_, log.buttons)) {
            std::string action = b->action;
            runAction(action);
        }
        return;
    }
    if (dialogOpen_) {
        Screen &dialog = screens_.at("dialog");
        if (Button *b = buttonAt(pointerX_, pointerY_, dialog.buttons)) {
            if (b->action == "noask") {
                dialogNoAsk_ = !dialogNoAsk_;
                dirty_ = true;
            } else {
                answerDialog(b->action == "yes");
            }
        } else if (dialog.setting("mousechoice")) {
            // Test Game 94's left click away from the answers says yes, as its right click says no.
            answerDialog(true);
        }
        return;
    }
    if (screen_) {
        if (viewingImage_ && viewerPanel()) {
            pressViewer(pointerX_, pointerY_);
            return;
        }
        if (spriteScreen()) {
            pressSprites(pointerX_, pointerY_);
            return;
        }
        // A title with menu stages brings up its menu at the first click anywhere.
        if (menuStage_ < 2) {
            menuStage_ = 2;
            dirty_ = true;
            return;
        }
        if (Button *b = buttonAt(pointerX_, pointerY_, screen_->buttons)) {
            std::string action = b->action;
            if (!(action == "nav_save" && returnTo_ == "title") && !pageOff(action) && !pageStepOff(action)) runAction(action);
            if (dialogOpen_) dialogOpener_ = action;
            return;
        }
        if (viewingImage_) {
            if (++viewingIndex_ >= viewing_.size()) viewingImage_ = false;
            dirty_ = true;
            return;
        }
        if (screen_->name.rfind("config", 0) == 0) {
            pressSettingsWidget(pointerX_, pointerY_, false);
            return;
        }
        int cell = galleryCellAt(pointerX_, pointerY_);
        if (cell >= 0) {
            if (screen_->name == "music") playTrack(page_ * int(screen_->settings.count("cell")) + cell);
            else openGalleryCell(cell);
            return;
        }
        int slot = slotAt(pointerX_, pointerY_);
        if (slot < 0) return;
        // A click on a card's comment asks for one, from the save and load screens alike, as the PC's box opens.
        if (overComment(slot, pointerX_, pointerY_) && slotInfo(slot).exists) {
            commentRequest_ = slot;
            return;
        }
        if (overCardPart("slotlock", slot, pointerX_, pointerY_)) {
            if (slotInfo(slot).exists) toggleLock(slot);
            return;
        }
        if (overCardDelete(slot, pointerX_, pointerY_)) {
            if (slotInfo(slot).exists && !locked(slot)) openDialog(3, [this, slot] { deleteSlot(slot); });
            return;
        }
        if (locked(slot) && screen_->name == "save") return;
        if (screen_->name == "save") {
            bool overwrite = slotInfo(slot).exists;
            openDialog(overwrite ? 2 : 0, [this, slot] { saveToSlot(slot); dirty_ = true; });
        } else if (screen_->name == "load" && slotInfo(slot).exists) {
            openDialog(1, [this, slot] { loadFromSlot(slot); });
        }
        return;
    }
    // The manual animation control's panel takes its own clicks.
    if (int button = cameraButtonAt(pointerX_, pointerY_); button >= 0) {
        const auto *zoom = screens_.at("defaults").setting("animzoom");
        int step = zoom && zoom->size() >= 3 ? field(*zoom, 2) : 5;
        if (button == 1) cameraZoom(-movieZoom_);
        else cameraZoom(button == 2 ? step : -step);
        return;
    }
    if (windowShown_) {
        // A touch has no hover, so a tap where hidden buttons sit shows them, and the next tap presses one.
        if (messagePointerY() >= barTop_ && !innerWereShown_ && !underWereShown_ && !setting("bar_always")) return;
        if (innerWereShown_)
            if (Button *b = buttonAt(pointerX_, messagePointerY(), bar().inner)) {
                std::string action = b->action;
                runAction(action);
                return;
            }
        if (underWereShown_)
            if (Button *b = buttonAt(pointerX_, messagePointerY(), bar().under)) {
                std::string action = b->action;
                runAction(action);
                return;
            }
    }
    if (waiting_ == Waiting::Choice) {
        updateChoice();
        if (choiceHover_ >= 0) choose(choiceHover_);
        // And a left click away from the options, the first.
        else if (choiceSheet_ && choiceTable("mousechoice")) choose(0);
        return;
    }
    clicked_ = true;
}

// The table's `choice` screen gives where options start and the step between them. Without one they stack in the middle.
void Engine::choicePlace(int &x, int &top, int &step, int &slant) const {
    slant = 0;
    int cellW = choiceSheet_->w / choiceColumns(), cellH = choiceSheet_->h / 5;
    x = (width_ - cellW) / 2;
    top = (height_ - cellH * choiceCount_) / 2;
    step = cellH;
    auto it = screens_.find("choice");
    if (it == screens_.end()) return;
    if (const auto *place = it->second.setting("place"); place && place->size() >= 2) {
        top = field(*place, 0);
        step = field(*place, 1);
    }
    // `placefor COUNT TOP STEP X SLANT` for a choice of that many options, each further right than the last, as Test Game
    // 84's tilted frames have it.
    for (auto [line, end] = it->second.settings.equal_range("placefor"); line != end; ++line)
        if (line->second.size() >= 3 && field(line->second, 0) == choiceCount_) {
            top = field(line->second, 1);
            step = field(line->second, 2);
            if (line->second.size() >= 5) {
                x = field(line->second, 3);
                slant = field(line->second, 4);
            }
        }
}

void Engine::updateChoice() {
    if (waiting_ != Waiting::Choice || !choiceSheet_) return;
    int cellW = choiceSheet_->w / choiceColumns(), cellH = choiceSheet_->h / 5, x, top, step, slant;
    choicePlace(x, top, step, slant);
    int px = int(mouseX_ * float(width_)), py = int(mouseY_ * float(height_));
    int hover = -1;
    for (int i = 0; i < choiceCount_; i++)
        if (px >= x + i * slant && px < x + i * slant + cellW && py >= top + i * step && py < top + i * step + cellH) hover = i;
    if (hover != choiceHover_) {
        choiceHover_ = hover;
        dirty_ = true;
    }
}

bool Engine::jump(const std::string &script, int node) {
    // A flow script starts the story at that flow, from that node when it's above 0, so its choices run as the flow
    // interpreter runs them.
    if (normalizedPath(script).rfind("flow\\", 0) == 0) {
        auto flow = loadScript(script);
        if (!flow || !flow->first()) return false;
        const Node *start = node > 0 ? flow->node(node) : flow->first();
        if (!start) return false;
        screen_ = nullptr;
        stack_.clear();
        scene_ = {};
        stack_.push_back({flow, start});
        waiting_ = Waiting::None;
        dirty_ = true;
        return true;
    }
    auto target = loadScript(script);
    // A negative node starts the script from its top.
    if (!target || (node >= 0 && !target->node(node))) return false;
    // In a story table's game the story goes on after the scene, from the first line that names it, as a load does.
    if (!firstStoryFlow().empty()) {
        stack_.clear();
        partScreen_ = nullptr;
        resumeStoryAt(target->name);
    }
    const Node *n = target->first();
    scene_ = {target, n};
    waiting_ = Waiting::None;
    screen_ = nullptr;
    if (node < 0) return true;
    bool wasSkip = skip_;
    skip_ = true;
    jumping_ = true;
    for (int guard = 0; guard < 2000000 && scene_.node && scene_.node->id != node; guard++) {
        const Node &current = *scene_.node;
        runScene(current);
        if (scene_.node == &current) advance();
        waiting_ = Waiting::None;
        transitioning_ = false;
    }
    skip_ = wasSkip;
    jumping_ = false;
    if (blur_) previousValid_ = false;
    front_ = back_;
    dirty_ = true;
    return scene_.node != nullptr;
}

const char *Engine::debugStatus() {
    char buffer[256];
    const char *waiting = waiting_ == Waiting::None ? "running" : waiting_ == Waiting::Time ? "time" : waiting_ == Waiting::Click ? "click" : waiting_ == Waiting::Choice ? "choice" : waiting_ == Waiting::StaffRoll ? "staff roll" : waiting_ == Waiting::PartScreen ? "part screen" : "ended";
    std::snprintf(buffer, sizeof buffer, "%s #%d %s t=%lld", scene_.script ? scene_.script->name.c_str() : "(flow)", scene_.node ? scene_.node->id : -1, waiting, (long long)now_);
    status_ = buffer;
    return status_.c_str();
}

// MARK: Commands

// Parameters: the slot, reversed, x, y, scale x and y in thousandths, then whether scaling keeps the middle in place, and
// parameter 14 repeats it. Position is the unscaled picture's top left, as a value of 480, 270 for a 320 by 180 picture
// in a 1280 by 720 game centres it.
void Engine::startFlipbook(const Node &n) {
    const auto &p = n.p;
    Sprite s;
    s.path = n.s[1];
    s.image = images_->get(n.s[1]);
    if (!s.image) {
        complain("Missing image " + n.s[1]);
        return;
    }
    s.x = float(p[2]);
    s.y = float(p[3]);
    s.scaleX = float(p[4]) / 1000;
    s.scaleY = float(p[5]) / 1000;
    s.anchorX = s.anchorY = p[8] ? -1 : 0;
    s.anchorSet = true;
    s.flipLoop = p[14] != 0;
    s.flipReverse = p[1] != 0;
    s.flipStep = p[16];
    s.flipParam15 = p[15];
    resumeFlipbook(s);
    if (!s.flipCount) warnOnce("A flipbook isn't in anime_cfg.ini: " + n.s[1]);
    else if (s.flipReverse) {
        s.flipShown = s.flipCount - 1;
        if (ImageRef last = images_->get(flipPicture(s.path, s.flipShown))) s.image = last;
    }
    back_[p[0]] = s;
}

// A save holds the picture a flipbook is on but not how it plays. VNDegen's own saves keep that beside the save, in
// save###.vnd, which the PC game doesn't read. For a PC save, walking the scene to the saved place finds the command
// that started each one, following moves between slots: the scene the flow called before, then the scene with GOOD
// Timing's marker up to it, then the script the save is in. A walk that never reaches its place, as when a patch took that
// node away, says nothing, and a flipbook found nowhere repeats, as nearly all of them do. A PC save made after Ejaculate
// was pressed early can't say where the press was, so its walk runs to the marker.
std::map<int, Engine::FlipModes> Engine::flipModesAt(const SaveData &d, const Sidecar &side) {
    std::map<int, FlipModes> slots;
    // The modes VNDegen kept beside its save, or else the ones the scripts set on the way to the save's node.
    if (side.valid) return side.flips;
    auto walk = [&](const std::string &name, int stopAt) {
        auto script = name.empty() ? nullptr : loadScript(name);
        if (!script) return;
        std::map<int, FlipModes> walked = slots;
        const Node *n = script->first();
        size_t guard = 0;
        for (; n && n->id != stopAt && guard < script->nodes.size(); guard++, n = script->node(n->next)) {
            if (n->opcode == 121) walked[n->p[0]] = {n->s[1], n->p[14] != 0, n->p[1] != 0};
            else if (n->opcode == 17) walked.erase(n->p[0]);
            else if (n->opcode == 22) walked.clear();
            else if (n->opcode == 120) {
                auto it = walked.find(n->p[0]);
                if (it == walked.end()) continue;
                FlipModes moved = it->second;
                walked.erase(it);
                walked[n->p[1]] = moved;
            }
        }
        if (n ? n->id == stopAt : stopAt == 0) slots = std::move(walked);
    };
    // The scene before: back along the flow from the call the save is in, to the call before it. Test Game 93 has one scene
    // that ends with a flipbook still running into the next.
    if (auto flow = d.flow.empty() ? nullptr : loadScript(d.flow)) {
        int at = d.flowNode;
        for (int guard = 0; guard < 4096; guard++) {
            const Node *before = nullptr;
            for (const Node &candidate : flow->nodes)
                if (candidate.next == at) before = &candidate;
            if (!before) break;
            if (before->opcode == 1) {
                walk(before->s[1], 0);
                break;
            }
            at = before->id;
        }
    }
    if (!d.gtScene.empty() && normalizedPath(d.gtScene) != normalizedPath(d.scene)) walk(d.gtScene, d.gtMarker);
    walk(d.scene, d.node);
    return slots;
}

void Engine::writeFlipModes(int slot, const SaveData &d) {
    // `at SCENE NODE`, with `-` for no scene, and the fingerprint of the save it was written with.
    std::string text = "at " + (d.scene.empty() ? std::string("-") : sjis::toUtf8(d.scene)) + " " + std::to_string(d.node) + "\n";
    text += "print " + std::to_string(saveFingerprint(d)) + "\n";
    for (const auto &[at, s] : front_)
        if (s.flipCount) text += "flip " + std::to_string(at) + " " + (s.flipLoop ? "1" : "0") + " " + (s.flipReverse ? "1" : "0") + " " + sjis::toUtf8(s.path) + "\n";
    // Game parts' state, `part NAME LINE`, kept beside the save as the flipbooks' modes are.
    for (const auto &[name, part] : parts_) {
        std::vector<std::string> lines;
        part->save(lines);
        for (const std::string &line : lines) text += "part " + name + " " + line + "\n";
        if (part.get() == partScreen_)
            text += "screen " + name + " " + std::to_string(partResultScope_) + " " + std::to_string(partResultNumber_) + "\n";
    }
    // A story table's game: its exact place, `story SCRIPT NODE` for each flow frame, which the PC's save can't hold, and
    // the last result. A choice waiting is saved as the choice, to be asked again.
    if (d.flow.empty() && !firstStoryFlow().empty()) {
        for (size_t i = 0; i < stack_.size(); i++) {
            const Frame &frame = stack_[i];
            int id = frame.node ? frame.node->id : 0;
            if (i + 1 == stack_.size() && waiting_ == Waiting::Choice)
                for (const Node &c : frame.script->nodes)
                    if (c.opcode == 3 && c.next == id) id = c.id;
            text += "story " + frame.script->name + " " + std::to_string(id) + "\n";
        }
        text += "result " + std::to_string(choiceResult_) + "\n";
        if (waiting_ == Waiting::Choice) text += "choice\n";
    }
    writeWhole(savePath(slot, "vnd"), std::vector<uint8_t>(text.begin(), text.end()));
}

// What a save holds of the story's state, for telling whether VNDegen's lines beside it were written with it: its time,
// scene, node and scenario variables. The card's comment, which can change later, isn't in it.
uint32_t Engine::saveFingerprint(const SaveData &d) {
    uint32_t h = 2166136261u;
    auto mix = [&](uint32_t v) {
        for (int i = 0; i < 4; i++) h = (h ^ ((v >> (8 * i)) & 0xff)) * 16777619u;
    };
    for (int v : d.date) mix(uint32_t(v));
    for (char c : normalizedPath(d.scene)) mix(uint8_t(c));
    mix(uint32_t(d.node));
    for (const auto &[number, value] : d.variables)
        if (value) {
            mix(uint32_t(number));
            mix(uint32_t(value));
        }
    return h;
}

Engine::Sidecar Engine::readSidecar(const SaveData &d, int slot) {
    Sidecar side;
    if (slot < 0) return side;
    auto bytes = readWhole(savePath(slot, "vnd"));
    std::istringstream in(std::string(bytes.begin(), bytes.end()));
    std::string line, scene;
    int node = -1;
    bool printed = false;
    uint32_t print = 0;
    while (std::getline(in, line)) {
        std::istringstream words(line);
        std::string kind;
        words >> kind;
        if (kind == "at") {
            words >> scene >> node;
            if (scene == "-") scene.clear();
        } else if (kind == "print") {
            printed = bool(words >> print);
        } else if (kind == "flip") {
            int at = 0, loop = 1, reverse = 0;
            std::string path;
            if (words >> at >> loop >> reverse && std::getline(words >> std::ws, path) && !path.empty())
                side.flips[at] = {sjis::fromUtf8(path), loop != 0, reverse != 0};
        } else if (kind == "choice") {
            side.atChoice = true;
        } else if (kind == "part") {
            std::string name, rest;
            if (words >> name && std::getline(words >> std::ws, rest)) side.parts[name].push_back(rest);
        } else if (kind == "story") {
            std::string script;
            int id = 0;
            if (words >> script >> id) side.story.push_back({script, id});
        } else if (kind == "result") {
            side.hasResult = bool(words >> side.result);
        } else if (kind == "screen") {
            words >> side.screen >> side.screenScope >> side.screenNumber;
        }
    }
    // Lines written with another save than this one, such as a PC save copied over ours, don't count: the fingerprint
    // tells, or in lines from before it was kept, the scene and node.
    bool same = printed ? print == saveFingerprint(d)
                        : node >= 0 && normalizedPath(sjis::fromUtf8(scene)) == normalizedPath(d.scene) && node == d.node;
    if (!same) return {};
    side.valid = true;
    return side;
}

// Game parts start afresh, then take what the PC's save holds for them, then their own lines beside the save.
void Engine::loadPartsState(const SaveData &d, const Sidecar &side) {
    resetParts();
    for (const auto &[name, part] : parts_) part->loadPC(d);
    for (const auto &[name, part] : parts_)
        if (auto it = side.parts.find(name); it != side.parts.end()) part->load(it->second);
}

void Engine::restoreFlipbookModes(Sprite &s, int slot, int frame, const std::map<int, FlipModes> &slots, int recordRepeat, int recordReverse) {
    if (!s.flipCount) return;
    s.flipLoop = true;
    auto it = slots.find(slot);
    if (it != slots.end() && normalizedPath(it->second.path) == normalizedPath(s.path)) {
        s.flipLoop = it->second.loop;
        s.flipReverse = it->second.reverse;
    }
    // A record that says whether it repeats, as a PC save of Test Game 88 made while it plays does, is believed first.
    if (recordRepeat >= 0) s.flipLoop = recordRepeat != 0;
    if (recordReverse >= 0) s.flipReverse = recordReverse != 0;
    if (!s.flipLoop && frame >= s.flipCount) {
        s.flipBegun = now_ - int64_t(s.flipCount) * s.flipMs;
        s.flipShown = s.flipCount;
        return;
    }
    int step = s.flipReverse ? s.flipCount - 1 - frame : frame;
    s.flipBegun = now_ - int64_t(std::max(step, 0)) * s.flipMs;
    s.flipShown = std::clamp(frame, 0, s.flipCount - 1);
    if (ImageRef image = images_->get(flipPicture(s.path, s.flipShown))) s.image = image;
}

void Engine::resumeFlipbook(Sprite &s) {
    auto it = flipbooks_.find(normalizedPath(s.path));
    if (it == flipbooks_.end()) return;
    s.flipCount = it->second.first;
    s.flipMs = it->second.second;
    s.flipBegun = now_;
    s.flipShown = 0;
    s.additive = paintedOnBlack(s.path);
}

// Effect flipbooks in Test Games 84 and 85 have no alpha and are nearly all black, and the PC game draws them over the
// scene without darkening it, so their light adds. The CG flipbooks of other games, also without alpha, are under 10%
// black against the effects' 89% and more.
bool Engine::paintedOnBlack(const std::string &path) {
    auto key = normalizedPath(path);
    if (auto it = paintedOnBlack_.find(key); it != paintedOnBlack_.end()) return it->second;
    bool dark = false;
    if (ImageRef first = images_->get(flipPicture(path, 0)); first && first->opaque && !first->rgba.empty()) {
        size_t black = 0, all = first->rgba.size() / 4;
        for (size_t i = 0; i < all; i++) {
            const uint8_t *p = &first->rgba[i * 4];
            if (p[0] < 16 && p[1] < 16 && p[2] < 16) black++;
        }
        dark = black * 10 > all * 8;
    }
    paintedOnBlack_[key] = dark;
    return dark;
}

void Engine::advanceFlipbooks() {
    for (const auto &[slot, s] : front_)
        if (s.waveOn) dirty_ = true;
    std::vector<std::pair<std::string, int>> ahead;
    for (auto *set : {&back_, &front_})
        for (auto &[slot, s] : *set) {
            if (s.flipCount < 2) continue;
            int64_t step = (now_ - s.flipBegun) / s.flipMs;
            // Past its last picture a flipbook that doesn't repeat shows nothing: the PC game's saves hold it at a picture
            // beyond its count, and its effects leave no trace once they've played.
            int index = s.flipLoop ? int(step % s.flipCount) : int(std::min<int64_t>(step, s.flipCount));
            if (s.flipReverse && index < s.flipCount) index = s.flipCount - 1 - index;
            if (!s.flipLoop && index >= s.flipCount) {
                if (s.flipShown != s.flipCount) {
                    s.flipShown = s.flipCount;
                    if (set == &front_) dirty_ = true;
                }
                continue;
            }
            if (index == s.flipShown) continue;
            s.flipShown = index;
            if (ImageRef next = images_->get(flipPicture(s.path, index))) s.image = next;
            if (set == &front_) dirty_ = true;
            for (int more = 1; more <= 3; more++) ahead.push_back({flipPicture(s.path, (index + more) % s.flipCount), 0});
        }
    if (!ahead.empty()) images_->prefetch(ahead);
}

bool Engine::readGtConfig(const std::string &path) {
    Bytes data = files_.read(path);
    if (!data) {
        complain("Missing GOOD Timing config " + path);
        return false;
    }
    gt_.extension.clear();
    gt_.ejaculate.clear();
    for (auto &group : gt_.groups) group.clear();
    gt_.config = path;
    std::vector<std::string> *into = nullptr;
    for (const std::string &raw : split(sjis::toUtf8(std::string(data->begin(), data->end())), '\n')) {
        std::string line = trim(raw.substr(0, raw.find("//")));
        if (line.empty() || line == "##") continue;
        if (line[0] == '#') {
            into = line == "#Extension" ? &gt_.extension : line == "#Ejaculate" ? &gt_.ejaculate : line == "#GroupA" ? &gt_.groups[0]
                : line == "#GroupB" ? &gt_.groups[1] : line == "#GroupC" ? &gt_.groups[2] : nullptr;
            continue;
        }
        std::istringstream in(line);
        std::string file;
        while (into && in >> file) into->push_back(file);
    }
    return true;
}

// The count at the marker runs over its last texts, so it needs them listed.
void Engine::gtMeasure(const Script &script, int marker) {
    gt_.texts.clear();
    const Node *n = script.first();
    for (size_t guard = 0; n && n->id != marker && guard < script.nodes.size(); guard++) {
        if (n->opcode == 5) gt_.texts.push_back(n->id);
        n = script.node(n->next);
    }
}

void Engine::gtBegin(const Node &n) {
    if (!readGtConfig(n.s[1]) || !scene_.script) return;
    const Node *marker = &n;
    for (size_t guard = 0; marker && marker->opcode != 154 && guard < scene_.script->nodes.size(); guard++) marker = scene_.script->node(marker->next);
    if (!marker || marker->opcode != 154) {
        warnOnce("A GOOD Timing scene has no marker.");
        return;
    }
    gt_.active = true;
    gt_.extend = false;
    gt_.extending = false;
    gt_.ejaculating = false;
    gt_.main = scene_.script;
    gt_.marker = marker->id;
    gt_.count = marker->p[0];
    gt_.group = -1;
    gt_.index = 0;
    gt_.played = 0;
    gt_.shownCount = 0;
    gtMeasure(*scene_.script, marker->id);
    dirty_ = true;
}

void Engine::gtReachMarker(const Node &) {
    gt_.shownCount = 0;
    dirty_ = true;
    if (!gt_.active) return;
    if (!gt_.extend) {
        gt_.active = false;
        return;
    }
    gt_.extending = true;
    gt_.group = -1;
    gt_.index = -1;
    if (!gt_.extension.empty()) {
        if (auto script = loadScript(gt_.extension[0])) scene_ = {script, script->first()};
    } else {
        gtNextScript();
    }
}

void Engine::gtEjaculate() {
    if (!gt_.active || !gt_.main) return;
    if (voicePlayer_) {
        audio_.close(voicePlayer_);
        voicePlayer_ = 0;
    }
    gt_.active = false;
    gt_.shownCount = 0;
    waiting_ = Waiting::None;
    textDone_ = true;
    dirty_ = true;
    const Node *marker = gt_.main->node(gt_.marker);
    // Past the marker the story's art is already at its end, and the PC goes straight on. Saves show it skips this script.
    if (!gt_.extending && !gt_.ejaculate.empty()) {
        if (auto script = loadScript(gt_.ejaculate[0])) {
            gt_.ejaculating = true;
            scene_ = {script, script->first()};
            return;
        }
    }
    gt_.extending = false;
    scene_ = {gt_.main, marker ? gt_.main->node(marker->next) : nullptr};
}

bool Engine::gtBackToStory() {
    gt_.active = false;
    gt_.extending = false;
    gt_.ejaculating = false;
    if (!gt_.main) return false;
    const Node *marker = gt_.main->node(gt_.marker);
    scene_ = {gt_.main, marker ? gt_.main->node(marker->next) : nullptr};
    return true;
}

// A script of GOOD Timing's has ended. Groups play whole, and which comes next looks random on the PC, without one twice
// running. Docs/engines.md lists that as a guess.
bool Engine::gtNextScript() {
    if (gt_.ejaculating) return gtBackToStory();
    if (!gt_.extending) return false;
    gt_.played++;
    if (gt_.group >= 0 && size_t(gt_.index + 1) < gt_.groups[gt_.group].size()) {
        gt_.index++;
    } else {
        std::vector<int> open;
        for (int group = 0; group < 3; group++)
            if (!gt_.groups[group].empty() && group != gt_.group) open.push_back(group);
        if (open.empty() && gt_.group >= 0) open.push_back(gt_.group);
        if (open.empty()) return gtBackToStory();
        gt_.group = gt_.group < 0 ? open[0] : open[size_t(std::rand()) % open.size()];
        gt_.index = 0;
    }
    auto script = loadScript(gt_.groups[gt_.group][size_t(gt_.index)]);
    if (!script) return gtBackToStory();
    scene_ = {script, script->first()};
    return true;
}

// The count before GOOD Timing's marker, as Test Game 88's PC draws it, from the table's `gt` screen:
//   gtcount HEART X Y DIGITS TENSX UNITSX Y LAST      the heart and the count in two digits from the DIGITS sheet, and
//                                                     at a count of 1 from the LAST sheet, a red 0 and 1, instead
//   gtpieces FILE,X,Y...                              the heart's segments, lit while the count is above their place:
//                                                     at N the last N are lit, the first going dark first
void Engine::composeGtCount(Canvas &out) {
    if (gt_.shownCount <= 0 || setting("gt_off")) return;
    auto table = screens_.find("gt");
    if (table == screens_.end()) return;
    const auto *f = table->second.setting("gtcount");
    if (!f || f->size() < 8) return;
    auto draw = [&](const std::string &path, int x, int y) {
        if (ImageRef image = images_->get(path)) drawPart(out, *image, x, y, 0, 0, image->w, image->h, 1);
    };
    draw((*f)[0], field(*f, 1), field(*f, 2));
    if (const auto *pieces = table->second.setting("gtpieces")) {
        int count = int(pieces->size());
        for (int i = std::max(0, count - gt_.shownCount); i < count; i++) {
            auto parts = split((*pieces)[size_t(i)], ',');
            if (parts.size() >= 3) draw(parts[0], std::atoi(parts[1].c_str()), std::atoi(parts[2].c_str()));
        }
    }
    bool last = gt_.shownCount == 1;
    if (ImageRef digits = images_->get((*f)[last ? 7 : 3])) {
        int w = digits->w / (last ? 2 : 10), value = std::min(gt_.shownCount, 99);
        drawPart(out, *digits, field(*f, 4), field(*f, 6), value / 10 * w, 0, w, digits->h, 1);
        drawPart(out, *digits, field(*f, 5), field(*f, 6), value % 10 * w, 0, w, digits->h, 1);
    }
}

// Whether a picture on screen has a version change_texture_list.ini names, which the art switch can show instead.
bool Engine::artSwitchable() const {
    for (const auto &[slot, s] : front_) {
        if (s.path.empty()) continue;
        std::string name = normalizedPath(s.path);
        if (plainArt_.count(name)) return true;
        auto slash = name.rfind('\\');
        std::string file = name.substr(slash == std::string::npos ? 0 : slash + 1);
        if (file.size() >= 15 && file.compare(10, 5, "_000_") == 0 && artVersions_.count(file.substr(0, 10))) return true;
    }
    return false;
}

// A heroine's story sprite in the outfit the sprite gallery locked for her, as the PC wears it, when that file exists.
std::string Engine::lockedSprite(const std::string &path) const {
    if (spriteHeroines_.empty()) return path;
    std::string name = normalizedPath(path);
    auto slash = name.rfind('\\');
    size_t at = slash == std::string::npos ? 0 : slash + 1;
    // sc\sc_HHH_OOO_PPP_SSS_FFF.bmp
    if (name.compare(0, 3, "sc\\") != 0 || name.size() < at + 23 || name.compare(at, 3, "sc_") != 0) return path;
    int number = std::atoi(name.substr(at + 3, 3).c_str());
    for (size_t h = 0; h < spriteHeroines_.size(); h++) {
        const SpriteHeroine &heroine = spriteHeroines_[h];
        if (heroine.number != number || heroine.outfits.empty() || !spriteLocked_[h]) continue;
        int pick = std::clamp(spriteOutfit_[h], 0, int(heroine.outfits.size()) - 1);
        char id[8];
        std::snprintf(id, sizeof id, "%03d", heroine.outfits[size_t(pick)].id);
        std::string worn = name;
        worn.replace(at + 7, 3, id);
        return files_.exists(worn) ? worn : path;
    }
    return path;
}

std::string Engine::shownPath(const std::string &rawPath) const {
    std::string path = lockedSprite(rawPath);
    if (!artVersions_.empty()) return plainVariant(versionPath(path));
    if (setting("express")) return path;
    auto it = plainArt_.find(normalizedPath(path));
    return it == plainArt_.end() ? path : it->second;
}

// `cg\cg_000_000_000_000_000.bmp` is group cg_000_000's picture as drawn. The third number picks its version: 100 with the
// protagonist see-through, 1 without pubic hair, 101 both, where change_texture_list.ini says the group has them.
std::string Engine::versionPath(const std::string &path) const {
    std::string name = normalizedPath(path);
    auto slash = name.rfind('\\');
    std::string file = name.substr(slash == std::string::npos ? 0 : slash + 1);
    if (file.size() < 15 || file.compare(10, 5, "_000_") != 0) return path;
    auto it = artVersions_.find(file.substr(0, 10));
    if (it == artVersions_.end()) return path;
    int version = (it->second.first && !setting("hero") ? 100 : 0) + (it->second.second && !setting("hair") ? 1 : 0);
    if (!version) return path;
    char digits[4];
    std::snprintf(digits, sizeof digits, "%03d", version);
    std::string other = name;
    other.replace((slash == std::string::npos ? 0 : slash + 1) + 11, 3, digits);
    return files_.exists(other) ? other : path;
}

std::string Engine::plainVariant(const std::string &path) const {
    if (setting("express")) return path;
    auto it = screens_.find("defaults");
    if (it == screens_.end() || !it->second.setting("plainvariants")) return path;
    std::string name = normalizedPath(path);
    auto slash = name.rfind('\\');
    size_t at = (slash == std::string::npos ? 0 : slash + 1) + 19;
    // `cg\cg_001_000_000_000_304.bmp`: the fifth number's hundreds.
    if (name.size() < at + 3 || name.compare(at - 19, 3, "cg_") != 0 || name[at] < '1' || name[at] > '9') return path;
    std::string base = name;
    base[at] = '0';
    return files_.exists(base) ? base : path;
}

void Engine::applyArtSwitch() {
    if (plainArt_.empty() && artVersions_.empty()) return;
    bool changed = false;
    for (auto *set : {&back_, &front_})
        for (auto &[slot, s] : *set)
            if (!s.path.empty() && !s.flipCount)
                if (ImageRef image = images_->get(shownPath(s.path)); image && image != s.image) {
                    s.image = image;
                    changed = true;
                }
    if (!changed) return;
    // A transition and motion blur hold frames drawn with the old pictures, so they end here.
    transitioning_ = false;
    ruleImage_ = nullptr;
    previousValid_ = false;
    dirty_ = true;
}

// Parameters 3 and 4 say where it bursts and 5 how many fly from there.
void Engine::splashBurst(const Node &n) {
    auto it = splashes_.find(n.p[0]);
    if (it == splashes_.end() || !it->second.image) return;
    Splash &splash = it->second;
    if (skip_ || skipToggle_) return;
    int burst = std::clamp(n.p[5] * 4, 1, splash.cap);
    for (int i = 0; i < burst && int(splash.droplets.size()) < splash.cap; i++) {
        float angle = float(std::rand() % 360) * kPi / 180;
        float speed = float(splash.speed) * float(40 + std::rand() % 100) / 100;
        int life = splash.minLife + std::rand() % (splash.maxLife - splash.minLife + 1);
        splash.droplets.push_back({float(n.p[3]), float(n.p[4]), std::cos(angle) * speed, std::sin(angle) * speed, float(50 + std::rand() % 100) / 100, int(now_), life});
    }
    dirty_ = true;
}

// The film noise, read from The Alchemist's exe with approval (nekosdk-cleanroom.md): a crop of its frame half as wide
// and half as high, scaled up twice over at 1920 wide so it fills the screen, from a new random place every fourth frame.
void Engine::drawNoise(Canvas &out, const Sprite &s) {
    const Image &frame = *s.image;
    int w = frame.w / 2, h = frame.h / 2;
    if (w <= 0 || h <= 0) return;
    int64_t step = (now_ - s.noiseFrom) * std::max(1, fps_) / (1000 * kNoiseFramesPerPlace);
    // Any spread-out choice will do for the place, so a hash of the step and the noise's start stands in for the PC's
    // random numbers, and the same step draws the same crop however often the frame is redrawn.
    uint64_t key = uint64_t(step) * 0x9E3779B97F4A7C15ull ^ uint64_t(s.noiseFrom) * 0xC2B2AE3D27D4EB4Full;
    if (!noiseCrop_ || int64_t(key) != noiseCropStep_) {
        uint64_t z = key + 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        z ^= z >> 31;
        int ox = int(z % uint64_t(w)), oy = int((z >> 32) % uint64_t(h));
        auto crop = std::make_shared<Image>();
        crop->w = w;
        crop->h = h;
        crop->opaque = false;
        crop->rgba.resize(size_t(w) * size_t(h) * 4);
        for (int y = 0; y < h; y++)
            std::memcpy(&crop->rgba[size_t(y) * size_t(w) * 4], &frame.rgba[(size_t(oy + y) * size_t(frame.w) + size_t(ox)) * 4], size_t(w) * 4);
        noiseCrop_ = crop;
        noiseCropStep_ = int64_t(key);
    }
    float scale = 2.0f * float(out.w) / 1920;
    drawImage(out, *noiseCrop_, float(w) * scale / 2, float(h) * scale / 2, scale, scale, 0, 1, nullptr, false);
}

void Engine::composeSplashes(Canvas &out, int fromSlot, int toSlot) {
    for (auto &[slot, splash] : splashes_) {
        if (slot < fromSlot || slot >= toSlot || !splash.image) continue;
        for (const Droplet &d : splash.droplets) {
            float age = float(now_ - d.bornMs) / 1000;
            float life = float(now_ - d.bornMs) / float(d.lifeMs);
            if (life >= 1) continue;
            drawImage(out, *splash.image, d.x + d.vx * age, d.y + d.vy * age + 500 * age * age, d.scale, d.scale, 0, 1 - life);
        }
    }
}

void Engine::loadImage(const Node &n) {
    int slot = n.p[3];
    noteSeen(n.s[1]);
    ImageRef image = images_->get(shownPath(n.s[1]));
    if (!image) {
        complain("Missing image " + n.s[1]);
        return;
    }
    Sprite &s = back_[slot];
    // Command 15 starts the slot afresh, and 23, a difference image, lands where the last one was. Parameter 0 of 15 places
    // the picture: centred, or with its middle 256 or 512 to the left or right of centre, and parameters 1 and 2 move it
    // from there. Replaying scenes to the sprite places in the PC game's saves shows this, at 1280 and 1920 wide.
    if (n.opcode == 15) {
        static const int kFromCentre[] = {0, -256, 256, -512, 512};
        bool placed = n.p[0] >= 1 && n.p[0] <= 5;
        s.x = float(n.p[1] + (placed ? (width_ - image->w) / 2 + kFromCentre[n.p[0] - 1] : 0));
        s.y = float(n.p[2] + (placed ? (height_ - image->h) / 2 : 0));
        s.scaleX = s.scaleY = 1;
        s.anchorX = s.anchorY = -1;
        s.anchorSet = false;
        s.rotation = 0;
        s.alpha = 1;
        s.changes.clear();
        s.looping = false;
    }
    s.image = image;
    s.path = n.s[1];
    s.flipCount = 0;
    s.movie.reset();
}

std::shared_ptr<Movie> Engine::openMovie(const std::string &path, bool loop) {
    if (!hasMovies_) {
        warnOnce("Movies need the decoder in the KiriKiri framework, which isn't there; left out: " + path);
        return nullptr;
    }
    // Scripts name `movie\x.wmv`, and the file is `movie\x` with no extension.
    std::string file = path;
    if (!files_.exists(file) && file.size() > 4 && normalizedPath(file.substr(file.size() - 4)) == ".wmv") file.resize(file.size() - 4);
    std::string why;
    auto movie = Movie::open(movies_, files_, file, loop, why);
    if (!movie) warnOnce("Can't play the movie " + path + ": " + why);
    return movie;
}

// Parameter 3 is the slot and 9 says the movie loops. It sits at the top left, as an image with nothing placing it does.
void Engine::loadMovie(const Node &n) {
    auto movie = openMovie(n.s[1], n.p[9] != 0);
    if (!movie) return;
    log("movie %s in slot %d%s", n.s[1].c_str(), n.p[3], n.p[9] ? ", looping" : "");
    Sprite &s = back_[n.p[3]];
    s = Sprite{};
    s.path = n.s[1];
    s.movie = movie;
    s.movieFrom = now_;
    bool ended = false;
    s.image = movie->frame(0, ended);
}

// The staff roll sits in a slot above any a script uses, with the message window put away. A game whose table names no
// staff roll goes straight on: Test Game 84's draws its own, which isn't built yet.
constexpr int kStaffRollSlot = 1 << 20;

void Engine::startStaffRoll() {
    auto it = screens_.find("staffroll");
    const auto *movie = it == screens_.end() ? nullptr : it->second.setting("movie");
    if (it != screens_.end() && !movie && !it->second.pictures.empty()) {
        const Screen &roll = it->second;
        log("staff roll, %d layers", int(roll.pictures.size()));
        staffRollLayers_ = &roll;
        staffRollFrom_ = now_;
        staffRollSkips_ = false;
        if (const auto *skip = roll.setting("skipif"); skip && !skip->empty()) {
            std::vector<int> numbers;
            for (const std::string &number : *skip) numbers.push_back(std::atoi(number.c_str()));
            staffRollSkips_ = anyGlobalSet(numbers);
        }
        stopKind(kKindBgm, 500);
        if (const auto *music = roll.setting("music"); music && !music->empty())
            if (Bytes data = files_.read((*music)[0]))
                if (int player = audio_.open(data)) {
                    audio_.setLoops(player, 1);
                    audio_.setGroup(player, kKindBgm);
                    audio_.start(player);
                    bgm_[-1] = player;
                    noteSeen((*music)[0]);
                }
        windowShown_ = false;
        clicked_ = false;
        waiting_ = Waiting::StaffRoll;
        dirty_ = true;
        return;
    }
    if (!movie || movie->size() < 2) {
        warnOnce("This game's staff roll isn't built, so the story goes on past it.");
        return;
    }
    auto value = scenario_.find(field(*movie, 0));
    size_t pick = size_t(std::max(0, value == scenario_.end() ? 0 : value->second));
    if (pick + 1 >= movie->size()) {
        warnOnce("No staff roll for " + std::to_string(pick));
        return;
    }
    auto opened = openMovie((*movie)[pick + 1], false);
    if (!opened) return;
    log("staff roll %s", (*movie)[pick + 1].c_str());
    const auto *skip = it->second.setting("skipif");
    staffRollSkips_ = false;
    if (skip && !skip->empty()) {
        std::vector<int> numbers;
        for (const std::string &number : *skip) numbers.push_back(std::atoi(number.c_str()));
        staffRollSkips_ = anyGlobalSet(numbers);
    }
    Sprite roll;
    roll.path = (*movie)[pick + 1];
    roll.movie = opened;
    roll.movieFrom = now_;
    bool ended = false;
    roll.image = opened->frame(0, ended);
    front_[kStaffRollSlot] = roll;
    windowShown_ = false;
    clicked_ = false;
    waiting_ = Waiting::StaffRoll;
    dirty_ = true;
}

// It ends with its movie or its length, or at a click once it may be skipped.
void Engine::stepStaffRoll() {
    bool ended;
    if (staffRollLayers_) {
        const auto *length = staffRollLayers_->setting("length");
        ended = now_ - staffRollFrom_ >= (length ? field(*length, 0) : 0);
        dirty_ = true;
    } else {
        auto it = front_.find(kStaffRollSlot);
        ended = it == front_.end() || !it->second.movie;
        if (!ended) it->second.movie->frame(double(now_ - it->second.movieFrom) / 1000, ended);
    }
    if (ended || (clicked_ && staffRollSkips_)) endStaffRoll();
}

void Engine::endStaffRoll() {
    front_.erase(kStaffRollSlot);
    back_.erase(kStaffRollSlot);
    if (staffRollLayers_) {
        stopKind(kKindBgm, 500);
        // Test Game 84's exe marks the staff roll seen, GLB_99_ENDING, which no flow sets.
        bool changed = false;
        for (auto [it, end] = staffRollLayers_->settings.equal_range("sets"); it != end; ++it) {
            globals_[field(it->second, 0)] = 1;
            changed = true;
        }
        if (changed) saveGlobals();
    }
    staffRollLayers_ = nullptr;
    waiting_ = Waiting::None;
    dirty_ = true;
}

// Each layer where its motion and fades put it at this moment of the staff roll. Layers in a row with the same fades fade
// as one picture, as the PC's scene does: its characters stay solid over its sky while it fades.
void Engine::composeStaffRoll(Canvas &out) {
    out.clear();
    double t = double(now_ - staffRollFrom_);
    const auto &layers = staffRollLayers_->pictures;
    auto fades = [](const Picture &a, const Picture &b) {
        return a.fadeInAt == b.fadeInAt && a.fadeInMs == b.fadeInMs && a.fadeOutAt == b.fadeOutAt && a.fadeOutMs == b.fadeOutMs;
    };
    for (size_t i = 0; i < layers.size();) {
        size_t end = i + 1;
        while (end < layers.size() && fades(layers[i], layers[end])) end++;
        const Picture &first = layers[i];
        float alpha = 1;
        if (first.fadeInAt >= 0) alpha *= float(std::clamp((t - first.fadeInAt) / std::max(1, first.fadeInMs), 0.0, 1.0));
        if (first.fadeOutAt >= 0) alpha *= float(1 - std::clamp((t - first.fadeOutAt) / std::max(1, first.fadeOutMs), 0.0, 1.0));
        bool together = end - i > 1 && alpha < 1;
        Canvas &target = together ? staffRollGroup_ : out;
        if (together) {
            if (staffRollGroup_.w != out.w || staffRollGroup_.h != out.h) staffRollGroup_.resize(out.w, out.h);
            staffRollGroup_.px = out.px;
        }
        if (alpha > 0)
            for (size_t k = i; k < end; k++) {
                const Picture &p = layers[k];
                Picture at = p;
                if (p.moveFrom >= 0 && t > p.moveFrom) at.y = int(std::lround(p.y - p.speed * (t - p.moveFrom) / 1000));
                at.y = std::clamp(at.y, p.clampTop, p.clampBottom);
                if (!together) at.alpha *= alpha;
                drawPicture(target, at);
            }
        if (together && alpha > 0) blendFrame(out, staffRollGroup_, int(alpha * 256));
        i = end;
    }
}

// Each sprite's movie shows the frame due at the story's clock, and the newest movie with sound plays it, until no
// sprite holds that movie any more.
void Engine::stepMovies() {
    std::shared_ptr<Movie> newest;
    int64_t newestFrom = -1;
    for (Sprites *sprites : {&front_, &back_})
        for (auto &[slot, s] : *sprites) {
            if (!s.movie) continue;
            bool ended = false;
            ImageRef image = s.movie->frame(double(now_ - s.movieFrom) / 1000, ended);
            if (image && image != s.image) {
                s.image = image;
                dirty_ = true;
            }
            if (s.movie->hasSound() && s.movieFrom > newestFrom) {
                newest = s.movie;
                newestFrom = s.movieFrom;
            }
        }
    if (newest == soundMovie_) return;
    stopMovieSound();
    soundMovie_ = newest;
    if (Movie *m = soundMovie_.get()) audio_.setStream([m](float *out, int frames) { return m->sound(out, frames); }, kKindSe, 100);
}

// The mix stops reading the movie's sound. Whatever the decoder has queued plays when it starts again.
void Engine::stopMovieSound() {
    if (!soundMovie_) return;
    audio_.clearStream();
    soundMovie_.reset();
}

// Parameters 6 and 7 are the point in the picture a scale keeps in place, and 8 and 9 say the middle instead.
// The point a scale keeps still: a running named animation's pivot, or a restored animation's while it runs, else the
// sprite's own anchor, which neither changes.
void Engine::anchorOf(const Sprite &s, int &x, int &y) const {
    x = s.anchorX;
    y = s.anchorY;
    for (const Animation &a : s.changes)
        if (a.hasPivot) {
            x = a.pivotX;
            y = a.pivotY;
        }
    if (s.looping && s.loop.hasPivot) {
        x = s.loop.pivotX;
        y = s.loop.pivotY;
    }
}

void Engine::takeScaleAnchor(Sprite &s, const Node &n) {
    s.anchorX = n.p[8] ? -1 : n.p[6];
    s.anchorY = n.p[9] ? -1 : n.p[7];
    s.anchorSet = true;
}

// Parameters 16 to 19 say which of position, scale, rotation and alpha the command changes.
void Engine::transform(const Node &n) {
    const auto &p = n.p;
    auto it = back_.find(p[3]);
    if (it == back_.end()) return;
    Sprite &s = it->second;
    // With Config's camera work off an event CG stays whole: it isn't moved or scaled.
    bool still = !setting("camera") && isCgSlot(p[3]);
    // Where the timed changes have it, without a named animation, which goes on adding its own part afterwards.
    Sprite base = s;
    base.looping = false;
    SpriteView was = view(base);
    s.changes.clear();
    if (p[16] && !still) {
        // Parameter 0 makes it a move from where the sprite is, as it does for a change over time.
        s.x = float(p[1]) + (p[0] == 1 ? was.x : 0);
        s.y = float(p[2]) + (p[0] == 1 ? was.y : 0);
    }
    if (p[17] && !still) {
        s.scaleX = float(p[4]) / 1000;
        s.scaleY = float(p[5]) / 1000;
        takeScaleAnchor(s, n);
    }
    if (p[19]) s.alpha = float(p[15]) / 256;
    // A background kept soft behind a character, when parameter 21 turns it on: the PC game draws sharp with 20 alone.
    s.blurAmount = p[20];
    s.blurOn = p[21] != 0;
}

void Engine::transformOverTime(const Node &n) {
    // It runs on the sprite on screen as well as the one waiting for the next update: in the PC game's opening, a
    // change that follows an update starts straight away.
    if (auto shown = front_.find(n.p[3]); shown != front_.end()) changeOverTime(shown->second, n);
    if (auto waiting = back_.find(n.p[3]); waiting != back_.end()) changeOverTime(waiting->second, n);
}

// Parameter 0 makes a move relative. Parameter 24 is the delay and 25 the length, both in frames at the game's FPS: the
// PC game's save shows a change of 2400 at 2140 about 36 seconds in. A negative length is what that save calls
// `quadratic`, which eases out: the footage's steps fall in even amounts.
void Engine::changeOverTime(Sprite &s, const Node &n) {
    const auto &p = n.p;
    foldFinishedChanges(s);
    SpriteView current = view(s);
    // With Config's camera work off an event CG isn't moved or scaled, and with effects off a change happens at once.
    bool still = !setting("camera") && isCgPath(s.path);
    int frameMs = 1000;
    int delay = p[24] * frameMs / std::max(1, fps_), length = std::abs(p[25]) * frameMs / std::max(1, fps_);
    Animation a;
    a.begunMs = now_;
    a.periodMs = delay + length;
    auto add = [&](Track::Property property, bool relative, float from, float to) {
        Track t;
        t.property = property;
        t.relative = relative;
        t.easeOut = p[25] < 0;
        t.from = from;
        t.to = to;
        t.startMs = delay;
        t.lengthMs = length;
        a.tracks.push_back(t);
    };
    if (p[16] && !still) {
        if (p[0] == 1) {
            if (p[1]) add(Track::X, true, 0, float(p[1]));
            if (p[2]) add(Track::Y, true, 0, float(p[2]));
        } else {
            add(Track::X, false, current.x, float(p[1]));
            add(Track::Y, false, current.y, float(p[2]));
        }
    }
    if (p[17] && !still) {
        takeScaleAnchor(s, n);
        add(Track::ScaleX, false, current.scaleX, float(p[4]) / 1000);
        add(Track::ScaleY, false, current.scaleY, float(p[5]) / 1000);
    }
    if (p[19]) add(Track::Alpha, false, current.alpha, float(p[15]) / 256);
    s.changes.push_back(std::move(a));
    if (skip_ || !setting("effect")) {
        s.changes.back().begunMs = now_ - s.changes.back().periodMs - 1;
        foldFinishedChanges(s);
    }
}

// Lines such as `relative_pos_x,-10,0,200,liner` and `scale,1.11,center,center,0,200,liner`: a property, a value, then
// the start and length in ms and the easing, with the scale's pivot words in between.
void Engine::startNamedAnimation(const Node &n) {
    if (auto shown = front_.find(n.p[0]); shown != front_.end()) namedAnimation(shown->second, n);
    if (auto waiting = back_.find(n.p[0]); waiting != back_.end()) namedAnimation(waiting->second, n);
}

Animation Engine::parseAnimation(const std::string &text, const Sprite &s) {
    Animation a;
    a.begunMs = now_;
    float lastScaleX = s.scaleX, lastScaleY = s.scaleY, lastX = s.x, lastY = s.y, lastAlpha = s.alpha, lastRotation = s.rotation;
    for (std::string line : split(text, '\n')) {
        line = trim(line);
        if (line.empty()) continue;
        auto parts = split(line, ',');
        if (parts.size() < 4) { warnOnce("animation line not read: " + line); continue; }
        const std::string &name = parts[0];
        float value = float(std::atof(parts[1].c_str()));
        // In frames, as a timed change's are. A negative length is `quadratic`, which eases out.
        int frames = std::atoi(parts[parts.size() - 2].c_str());
        int start = std::atoi(parts[parts.size() - 3].c_str()) * 1000 / std::max(1, fps_);
        int length = std::abs(frames) * 1000 / std::max(1, fps_);
        Track t;
        t.startMs = start;
        t.lengthMs = length;
        t.easeOut = frames < 0 || parts.back() == "quadratic";
        a.periodMs = std::max(a.periodMs, start + length);
        auto push = [&](Track::Property property, bool relative, float from, float to) {
            t.property = property;
            t.relative = relative;
            t.from = from;
            t.to = to;
            a.tracks.push_back(t);
        };
        if (name == "relative_pos_x") push(Track::X, true, 0, value);
        else if (name == "relative_pos_y") push(Track::Y, true, 0, value);
        else if (name == "pos_x") { push(Track::X, false, lastX, value); lastX = value; }
        else if (name == "pos_y") { push(Track::Y, false, lastY, value); lastY = value; }
        else if (name == "scale") {
            // `scale,value,x,y,start,length,easing`, where x and y are `center` or a point in the picture.
            if (parts.size() >= 7) {
                a.hasPivot = true;
                a.pivotX = parts[2] == "center" ? -1 : std::atoi(parts[2].c_str());
                a.pivotY = parts[3] == "center" ? -1 : std::atoi(parts[3].c_str());
            }
            push(Track::ScaleX, false, lastScaleX, value);
            push(Track::ScaleY, false, lastScaleY, value);
            lastScaleX = lastScaleY = value;
        }
        else if (name == "scale_x") { push(Track::ScaleX, false, lastScaleX, value); lastScaleX = value; }
        else if (name == "scale_y") { push(Track::ScaleY, false, lastScaleY, value); lastScaleY = value; }
        else if (name == "alpha") { push(Track::Alpha, false, lastAlpha, value); lastAlpha = value; }
        else if (name == "rotate") { push(Track::Rotation, false, lastRotation, value); lastRotation = value; }
        else warnOnce("animation property not read: " + name);
    }
    return a;
}

void Engine::namedAnimation(Sprite &s, const Node &n) {
    foldFinishedChanges(s);
    Animation a = parseAnimation(n.s[1], s);
    a.repeat = n.p[1] != 0;
    a.source = n.s[1];
    s.loop = std::move(a);
    s.looping = true;
}

void Engine::stopNamedAnimation(int slot) {
    for (auto *set : {&back_, &front_}) {
        auto it = set->find(slot);
        if (it != set->end()) it->second.looping = false;
    }
}

void Engine::abortChange(int slot) {
    for (auto *set : {&back_, &front_}) {
        auto it = set->find(slot);
        if (it == set->end()) continue;
        Sprite &s = it->second;
        bool looping = s.looping;
        s.looping = false;
        SpriteView v = view(s);
        s.looping = looping;
        s.x = v.x;
        s.y = v.y;
        s.scaleX = v.scaleX;
        s.scaleY = v.scaleY;
        s.rotation = v.rotation;
        s.alpha = v.alpha;
        for (const Animation &change : s.changes)
            if (change.hasPivot) {
                s.anchorX = change.pivotX;
                s.anchorY = change.pivotY;
                s.anchorSet = true;
            }
        s.changes.clear();
    }
}

void Engine::screenUpdate(const Node &n) {
    const auto &p = n.p;
    // With Config's effects off the PC cuts straight to the new picture, though a wait after it still waits.
    if (skip_ || p[1] <= 0 || !setting("effect")) {
        front_ = back_;
        dirty_ = true;
        return;
    }
    if (dirty_) compose();
    from_ = canvas_;
    front_ = back_;
    transitioning_ = true;
    transitionType_ = p[0];
    // Config's wait time stretches a fade as it does a wait.
    transitionMs_ = std::max(1, p[1] * setting("waittime") / 100);
    transitionStart_ = now_;
    ruleImage_ = n.s[1].empty() ? nullptr : images_->get(n.s[1]);
    dirty_ = true;
}

void Engine::showText(const Node &n) {
    // What the PC keeps of it: its saves hold the text on screen and the last 60 lines, newest first.
    if (gt_.active && !gt_.extending && gt_.count > 0) {
        auto at = std::find(gt_.texts.begin(), gt_.texts.end(), n.id);
        int left = at == gt_.texts.end() ? 0 : int(gt_.texts.end() - at);
        gt_.shownCount = left <= gt_.count ? left : 0;
    }
    noteTextRead(n);
    currentText_ = {};
    currentText_.tag = n.s[3].empty() ? -10 : 0;
    currentText_.name = n.s[1];
    currentText_.text = n.s[2];
    currentText_.voice = n.s[3];
    backlog_.insert(backlog_.begin(), currentText_);
    if (backlog_.size() > 60) backlog_.pop_back();
    if (!windowShown_) {
        windowShown_ = true;
        windowShownAt_ = now_;
    }
    layoutText(sjis::toUtf8(n.s[1]), sjis::toUtf8(n.s[2]));
    textStart_ = now_;
    textDone_ = skip_;
    textCount_++;
    waiting_ = Waiting::Click;
    dirty_ = true;
    if (!n.s[3].empty() && !skip_ && voiceAllowed(n.s[3])) {
        if (Bytes data = files_.read(voicePath(n.s[3]))) {
            if (voicePlayer_) audio_.close(voicePlayer_);
            voicePlayer_ = audio_.open(data);
            if (voicePlayer_) {
                voiceFiles_[voicePlayer_] = n.s[3];
                audio_.setGroup(voicePlayer_, soundGroup(kKindVoice, n.s[3]));
                audio_.setLoops(voicePlayer_, 1);
                audio_.start(voicePlayer_);
            }
        } else {
            complain("Missing voice " + n.s[3]);
        }
    }
}

// Parameters: the channel, volume, position, fade-in ms, then the loop count, where 0 repeats for ever.
void Engine::playSound(const Node &n, int kind) {
    const auto &p = n.p;
    std::map<int, int> &channels = kind == kKindBgm ? bgm_ : kind == kKindSe ? se_ : voice_;
    int channel = p[0];
    auto old = channels.find(channel);
    if (old != channels.end()) {
        audio_.close(old->second);
        channels.erase(old);
    }
    if (skip_ && kind != kKindBgm && p[4] != 0) return;
    if (kind == kKindVoice && !voiceAllowed(n.s[1])) return;
    Bytes data = files_.read(n.s[1]);
    if (!data) {
        complain("Missing sound " + n.s[1]);
        return;
    }
    int player = audio_.open(data);
    if (!player) {
        complain("Unreadable sound " + n.s[1]);
        return;
    }
    audio_.setLoops(player, kind == kKindBgm ? 0 : p[4]);
    if (p[2]) audio_.setPan(player, p[2], 0);
    int fade = p[3], volume = p[1];
    audio_.setGroup(player, soundGroup(kind, n.s[1]));
    audio_.fade(player, 0, fade > 0 ? 0 : volume, false);
    if (fade > 0) audio_.fade(player, fade, volume, false);
    audio_.start(player);
    channels[channel] = player;
    if (kind == kKindVoice) voiceFiles_[player] = n.s[1];
    if (kind == kKindBgm) noteSeen(n.s[1]);
    if (kind == kKindBgm) bgmNow_[channel] = {n.s[1], volume, p[2]};
    if (kind == kKindSe && p[4] == 0) seNow_[channel] = {n.s[1], volume, p[2]};
}

void Engine::changeVolume(int kind, int channel, int volume, int ms) {
    auto &channels = channelsOf(kind);
    auto it = channels.find(channel);
    if (it != channels.end()) audio_.fade(it->second, std::max(ms, 0), volume, false);
}

void Engine::stopChannel(int kind, int channel, int ms) {
    std::map<int, int> &channels = kind == kKindBgm ? bgm_ : kind == kKindSe ? se_ : voice_;
    if (kind == kKindBgm) bgmNow_.erase(channel);
    if (kind == kKindSe) seNow_.erase(channel);
    auto it = channels.find(channel);
    if (it == channels.end()) return;
    audio_.fade(it->second, std::max(ms, 0), 0, true);
    audio_.retire(it->second);
    channels.erase(it);
}

void Engine::stopKind(int kind, int ms) {
    std::map<int, int> &channels = kind == kKindBgm ? bgm_ : kind == kKindSe ? se_ : voice_;
    for (auto &[channel, player] : channels) {
        audio_.fade(player, std::max(ms, 0), 0, true);
        audio_.retire(player);
    }
    channels.clear();
    if (kind == kKindBgm) bgmNow_.clear();
    if (kind == kKindSe) seNow_.clear();
}

// MARK: Drawing

SpriteView Engine::view(const Sprite &s) const {
    SpriteView v{s.x, s.y, s.scaleX, s.scaleY, s.rotation, s.alpha};
    auto apply = [&](const Animation &a) {
        int ms = int(now_ - a.begunMs);
        if (a.repeat && a.periodMs > 0) ms %= a.periodMs;
        for (const Track &t : a.tracks) {
            if (ms < t.startMs && !t.relative) continue;
            float value = t.from + (t.to - t.from) * progressOf(t, ms);
            float *field = t.property == Track::X ? &v.x : t.property == Track::Y ? &v.y : t.property == Track::ScaleX ? &v.scaleX
                : t.property == Track::ScaleY ? &v.scaleY : t.property == Track::Rotation ? &v.rotation : &v.alpha;
            if (t.relative) *field += value;
            else *field = value;
        }
    };
    for (const Animation &a : s.changes) apply(a);
    if (s.looping) apply(s.loop);
    return v;
}

void Engine::foldFinishedChanges(Sprite &s) {
    while (!s.changes.empty() && now_ - s.changes.front().begunMs >= s.changes.front().periodMs) {
        Sprite only = s;
        only.changes.resize(1);
        only.looping = false;
        SpriteView v = view(only);
        s.x = v.x;
        s.y = v.y;
        s.scaleX = v.scaleX;
        s.scaleY = v.scaleY;
        s.rotation = v.rotation;
        s.alpha = v.alpha;
        // A finished change's pivot stays with the scale it leaves behind.
        if (s.changes.front().hasPivot) {
            s.anchorX = s.changes.front().pivotX;
            s.anchorY = s.changes.front().pivotY;
            s.anchorSet = true;
        }
        s.changes.erase(s.changes.begin());
    }
}

bool Engine::anythingMoves() const {
    if (transitioning_ || shakeType_ || flashMs_ || blur_) return true;
    if (auto noise = front_.find(kNoiseSlot); noise != front_.end() && noise->second.noise) return true;
    for (const auto &[slot, splash] : splashes_)
        if (!splash.droplets.empty()) return true;
    if (now_ - windowShownAt_ < kWindowSlideMs) return true;
    for (const auto &[slot, s] : front_) {
        if (s.looping) return true;
        if (!s.changes.empty()) return true;
        if (s.flipCount > 1 && (s.flipLoop || now_ - s.flipBegun < int64_t(s.flipCount) * s.flipMs)) return true;
    }
    return false;
}

void Engine::composeScene(Canvas &out, const Sprites &sprites) {
    out.clear();
    float shakeX = 0, shakeY = 0;
    if (shakeType_) {
        // A burst about 300 ms long that dies away, from the PC game's footage. Type 4 moves sideways; type 8 both ways.
        float t = float(now_ - shakeStart_);
        float decay = shakeSustained_ ? 1 : 1 - t / kShakeMs;
        if (shakeStopAt_ >= 0) decay = std::min(decay, shakeStopMs_ > 0 ? 1 - float(now_ - shakeStopAt_) / float(shakeStopMs_) : 0);
        int type = shakeType_;
        if (decay <= 0) {
            decay = 0;
            shakeType_ = 0;
        }
        if (shakeSustained_) {
            shakeX = std::sin(t / shakePeriodX_ * 2 * kPi) * shakeAmpX_ * decay;
            shakeY = std::sin(t / shakePeriodY_ * 2 * kPi) * shakeAmpY_ * decay;
        } else {
            float wave = std::cos(t / 1000 * 2 * kPi * 7.5f) * decay;
            shakeX = wave * kShakePixelsX * float(out.w) / 1280;
            if (type != 4) shakeY = -wave * kShakePixelsY * float(out.w) / 1280;
        }
    }
    int drawnTo = INT_MIN;
    for (const auto &[slot, s] : sprites) {
        if (!s.image) continue;
        composeSplashes(out, drawnTo, slot);
        drawParts(out, Layer::Slot, drawnTo, slot);
        drawnTo = slot;
        if (s.noise) {
            drawNoise(out, s);
            continue;
        }
        SpriteView v = view(s);
        float dx = slot < shakeLock_ ? shakeX : 0, dy = slot < shakeLock_ ? shakeY : 0;
        // About an eighth of the amount as a radius gives the sharpness of the PC game's footage.
        int radius = s.blurOn ? (s.blurAmount + 4) / 8 : 0;
        if (s.flipCount && s.flipShown >= s.flipCount) continue;
        std::string shown = s.flipCount ? flipPicture(s.path, s.flipShown) : shownPath(s.path);
        ImageRef image = radius > 0 ? images_->blurred(shown, radius) : s.image;
        // The position is the unscaled picture's top left, and it scales about its anchor, its middle unless told otherwise.
        int pivotX, pivotY;
        anchorOf(s, pivotX, pivotY);
        float w = float(s.image->w), h = float(s.image->h), anchorX = pivotX < 0 ? w * 0.5f : float(pivotX), anchorY = pivotY < 0 ? h * 0.5f : float(pivotY);
        float middleX = v.x + anchorX + (w * 0.5f - anchorX) * v.scaleX, middleY = v.y + anchorY + (h * 0.5f - anchorY) * v.scaleY;
        std::vector<int> shifts;
        if (s.waveOn) {
            // A sideways ripple travelling up the screen, in the picture's own pixels, so it grows with the sprite's scale.
            // Parameter 3 is how far each way, 520 over parameter 2 the rows to a wave, and 7.8 seconds over parameter 1
            // the time a wave takes to pass. Fitted to PC footage at 2, 2, 10 on a sprite at scale 1 and at 1, 3, 15 on
            // one at 1.5.
            shifts.resize(size_t(out.h));
            float rows = 520.0f / float(std::max(1, s.waveRows)) * std::abs(v.scaleY), ms = 7800.0f / float(std::max(1, s.waveSpeed));
            float size = float(s.waveSize) * std::abs(v.scaleX);
            for (int y = 0; y < out.h; y++) shifts[size_t(y)] = int(std::lround(size * std::sin(2 * kPi * (float(y) / rows + float(now_) / ms))));
        }
        // A movie zoomed by the manual animation control scales about the canvas, and the part shown follows the pointer:
        // with it at an edge, the movie's edge meets the screen's.
        float zoom = 1;
        if (s.movie && movieZoom_ > 100 && setting("anim_manual")) {
            zoom = float(movieZoom_) / 100;
            float px = pointerSeen_ ? float(pointerX_) : float(out.w) / 2, py = pointerSeen_ ? float(pointerY_) : float(out.h) / 2;
            middleX = middleX * zoom - (zoom - 1) * px;
            middleY = middleY * zoom - (zoom - 1) * py;
        }
        drawImage(out, image ? *image : *s.image, middleX + dx, middleY + dy, v.scaleX * zoom, v.scaleY * zoom, v.rotation, v.alpha, s.waveOn ? &shifts : nullptr, s.additive);
    }
    composeSplashes(out, drawnTo, INT_MAX);
    drawParts(out, Layer::Slot, drawnTo, INT_MAX);
}

void Engine::compose() {
    messageInStrip_ = false;
    if (screen_) {
        composeScreen(canvas_);
        return;
    }
    if (logOpen_) {
        canvas_ = logBase_;
        composeLog(canvas_);
        if (dialogOpen_) composeDialog(canvas_);
        return;
    }
    if (waiting_ == Waiting::StaffRoll && staffRollLayers_) {
        composeStaffRoll(canvas_);
        return;
    }
    // A zoom ends with the movie it was for.
    if (movieZoom_ != 100 && !movieShowing()) movieZoom_ = 100;
    composeScene(canvas_, front_);

    if (transitioning_) {
        float progress = std::clamp(float(now_ - transitionStart_) / float(std::max(1, transitionMs_)), 0.0f, 1.0f);
        if (ruleImage_ && ruleImage_->w > 0) {
            // A rule image: dark pixels change first. `from` stays where the rule hasn't reached yet.
            const Image &rule = *ruleImage_;
            const int soft = 32;
            int threshold = int(progress * float(255 + soft)) - soft;
            bool reverse = transitionType_ == 2;
            forRows(canvas_.h, size_t(canvas_.w) * canvas_.h, [&](int row0, int row1) {
            for (int y = row0; y < row1; y++) {
                const uint8_t *r = rule.rgba.data() + size_t(y * rule.h / canvas_.h) * rule.w * 4;
                uint8_t *d = canvas_.px.data() + size_t(y) * canvas_.w * 4;
                const uint8_t *f = from_.px.data() + size_t(y) * canvas_.w * 4;
                for (int x = 0; x < canvas_.w; x++, d += 4, f += 4) {
                    int value = r[size_t(x * rule.w / canvas_.w) * 4];
                    if (reverse) value = 255 - value;
                    int keep = std::clamp((value - threshold) * 256 / soft, 0, 256);
                    if (keep == 0) continue;
                    d[0] = uint8_t(d[0] + (((f[0] - d[0]) * keep) >> 8));
                    d[1] = uint8_t(d[1] + (((f[1] - d[1]) * keep) >> 8));
                    d[2] = uint8_t(d[2] + (((f[2] - d[2]) * keep) >> 8));
                }
            }
            });
        } else {
            blendFrame(canvas_, from_, 256 - int(progress * 256));
            // Types 40 and 53 to 55 blur both pictures most at the middle of the crossfade, as the PC game's footage shows.
            if (transitionType_ == 40 || (transitionType_ >= 53 && transitionType_ <= 55))
                softenFrame(canvas_, 8, int(std::sin(progress * kPi) * 256));
        }
    }

    if (blur_) {
        int amount = blurAmount_;
        if (blurEnding_) {
            float left = 1 - float(now_ - blurEndStart_) / float(blurEndMs_);
            if (left <= 0) { blur_ = false; blurEnding_ = false; amount = 0; }
            else amount = int(float(amount) * left);
        }
        if (previousValid_ && amount > 0) blendFrame(canvas_, previous_, amount);
        previous_ = canvas_;
        previousValid_ = true;
    }

    if (flashMs_) {
        int elapsed = int(now_ - flashStart_);
        if (elapsed >= flashMs_) flashMs_ = 0;
        else tintFrame(canvas_, flashR_, flashG_, flashB_, 256 - elapsed * 256 / flashMs_);
    }

    for (size_t i = 0; i < bars_.size(); i++)
        if (bars_[i]) drawPart(canvas_, *bars_[i], 0, barY_[i], 0, 0, bars_[i]->w, bars_[i]->h, 1);
    drawParts(canvas_, Layer::UnderWindow);
    messageInStrip_ = square_;
    if (!square_) composeMessage(canvas_);
    drawParts(canvas_, Layer::OverWindow);
    composeCameraPanel(canvas_);
    if (partScreen_) drawParts(canvas_, Layer::Screen);
    if (dialogOpen_) composeDialog(canvas_);

    if (choiceSheet_ && choiceClosingFrom_ >= 0) composeChoice(canvas_);
    if (waiting_ == Waiting::Choice && choiceSheet_) {
        auto table = screens_.find("choice");
        if (table != screens_.end()) {
            for (const Picture &p : table->second.pictures) drawPicture(canvas_, p);
            for (auto [line, end] = table->second.settings.equal_range("framefor"); line != end; ++line)
                if (line->second.size() >= 4 && field(line->second, 0) == choiceCount_)
                    if (ImageRef frame = images_->get(line->second[1])) drawPart(canvas_, *frame, field(line->second, 2), field(line->second, 3), 0, 0, frame->w, frame->h, 1);
        } else if (choiceBack_) {
            drawPart(canvas_, *choiceBack_, 0, (height_ - choiceBack_->h) / 2, 0, 0, choiceBack_->w, choiceBack_->h, 1);
        }
        int cellW = choiceSheet_->w / choiceColumns(), cellH = choiceSheet_->h / 5, x, top, step, slant;
        choicePlace(x, top, step, slant);
        for (int i = 0; i < choiceCount_; i++) {
            int column = size_t(i) < choiceCells_.size() ? choiceCells_[size_t(i)] : i;
            drawPart(canvas_, *choiceSheet_, x + i * slant, top + i * step, column * cellW, (i == choiceHover_ ? 1 : 0) * cellH, cellW, cellH, 1);
        }
        // `routes FILE X STEP G G G G`: once one of the routes' ending globals is set, each option names the routes that
        // take it, a sheet row per route and a column for cleared or not, stacked STEP apart beside the option's middle,
        // as Test Game 94 does.
        const auto *routes = choiceTable("routes");
        if (routes && routes->size() >= 7) {
            std::vector<int> ends;
            for (size_t r = 3; r < 7; r++) ends.push_back(field(*routes, r));
            ImageRef sheet = images_->get((*routes)[0]);
            if (sheet && anyGlobalSet(ends)) {
                int w = sheet->w / 2, h = sheet->h / 4, gap = field(*routes, 2);
                for (int i = 0; i < choiceCount_; i++) {
                    std::vector<int> takers;
                    for (int r = 0; r < 4; r++)
                        if (choiceRoutes_[size_t(r)] == i) takers.push_back(r);
                    if (takers.empty()) continue;
                    int y = top + i * step + cellH / 2 - ((int(takers.size()) - 1) * gap + h) / 2;
                    for (int r : takers) {
                        drawPart(canvas_, *sheet, field(*routes, 1), y, anyGlobalSet({ends[size_t(r)]}) ? w : 0, r * h, w, h, 1);
                        y += gap;
                    }
                }
            }
        }
    }
}

// A line of the table's choice screen, or null.
const std::vector<std::string> *Engine::choiceTable(const std::string &key) const {
    auto it = screens_.find("choice");
    return it == screens_.end() ? nullptr : it->second.setting(key);
}

void Engine::composeMessage(Canvas &out) {
    if (!windowShown_ || windowHiddenByPlayer_) return;
    if (windowImage_) {
        // In the 4:3 frame Test Game 88's PC puts its window two rows lower than the rest of the layer moves (under Wine).
        int top = windowY_ + (square_ ? 2 : 0), y = top;
        if (windowShowType_ == 2) {
            int elapsed = int(now_ - windowShownAt_);
            if (elapsed < kWindowSlideMs) y = out.h - (out.h - top) * elapsed / kWindowSlideMs;
        }
        drawPart(out, *windowImage_, windowX_, y, 0, 0, windowImage_->w, windowImage_->h, windowOpacity_);
    }
    drawParts(out, Layer::Window);
    drawGlyphRun(out);
    composeGtCount(out);
    composeMessageButtons(out);
}

// The rectangle the wait mark can touch at any angle.
void Engine::markRect(int &x, int &y, int &w, int &h) const {
    int cell = markPatterns_ > 1 ? waitMark_->w / markPatterns_ : waitMark_->w;
    if (markPatterns_ > 1) {
        x = markX_;
        y = markY_;
        w = cell;
        h = waitMark_->h;
        return;
    }
    int side = int(std::ceil(std::hypot(float(waitMark_->w), float(waitMark_->h)))) + 2;
    x = markX_ + waitMark_->w / 2 - side / 2;
    y = markY_ + waitMark_->h / 2 - side / 2;
    w = side;
    h = side;
}

bool Engine::markShowing() const {
    return !screen_ && !logOpen_ && !dialogOpen_ && !windowHiddenByPlayer_ && windowShown_ && waitMark_ && waiting_ == Waiting::Click && textDone_;
}

// The mark is the last thing drawn, over a copy of what's beneath it, so a tick where only the mark moved redraws just
// that: the composed scene stands. Text waits are most of a game, and a full 1080p compose took 12 ms on the mini's
// iPad simulator every tick for a mark about 30 pixels across.
void Engine::drawMark(Canvas &out) {
    if (!markShowing()) return;
    // One picture turns; several are the cells of a sheet, stepped through at the same speed.
    float turns = float(now_ - textStart_) / 1000 * markSpeed_;
    if (markPatterns_ == 1) {
        drawImage(out, *waitMark_, float(markX_) + waitMark_->w * 0.5f, float(markY_) + waitMark_->h * 0.5f, 1, 1, turns * 2 * kPi, 1);
    } else {
        int cell = waitMark_->w / markPatterns_;
        int index = int(turns * float(markPatterns_)) % markPatterns_;
        drawPart(out, *waitMark_, markX_, markY_, index * cell, 0, cell, waitMark_->h, 1);
    }
}

// Lays out a line or lines of UTF-8 from a pen position. `endsThere` lays a name out to end at x rather than start there.
void Engine::layoutRun(const std::string &text, int x, int y, bool endsThere, std::vector<Glyph> &out) {
    int lineHeight = textLine_ > 0 ? textLine_ : fontSize_ + lineInterval_ + 4;
    size_t first = out.size();
    float penX = float(x);
    int penY = y, used = 0;
    bool italic = false;
    for (size_t i = 0; i < text.size();) {
        // `|i` starts italics and `^i` ends them.
        if ((text[i] == '|' || text[i] == '^') && i + 1 < text.size() && text[i + 1] == 'i') {
            italic = text[i] == '|';
            i += 2;
            continue;
        }
        auto c = static_cast<unsigned char>(text[i]);
        int extra = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
        uint32_t cp = extra == 0 ? c : extra == 1 ? c & 0x1F : extra == 2 ? c & 0x0F : c & 0x07;
        for (int k = 1; k <= extra && i + k < text.size(); k++) cp = cp << 6 | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        i += size_t(extra) + 1;
        if (cp == '\r') continue;
        int units = cp < 0x100 ? 1 : 2;
        if (cp == '\n' || used + units > maxChars_) {
            penX = float(x);
            penY += lineHeight;
            used = 0;
            if (cp == '\n') continue;
        }
        GlyphImage g;
        if (!glyph(fontFace_, cp, fontSize_, false, g)) continue;
        out.push_back({int(std::lround(penX)), penY, cp, italic});
        penX += float(g.advance) + textGap_;
        used += units;
    }
    if (endsThere) {
        // A run that ends at its position, as a name beside the text does, ends at its last glyph, not the gap after it.
        int width = int(std::lround(penX - (out.size() > first ? textGap_ : 0))) - x;
        for (size_t i = first; i < out.size(); i++) out[i].x -= width;
    }
}

void Engine::layoutText(const std::string &name, const std::string &text) {
    nameGlyphs_.clear();
    textGlyphs_.clear();
    int ascent = fontAscent(fontFace_, fontSize_);
    // A name to the left of the text, on its line, ends at its position. One on a line of its own starts there.
    layoutRun(name, nameX_ + textDX_, nameY_ + textDY_ + ascent, nameY_ == textY_, nameGlyphs_);
    layoutRun(text, textX_ + textDX_, textY_ + textDY_ + ascent, false, textGlyphs_);
}

void Engine::messageExtent(const std::string &name, const std::string &text, int &right, int &bottom) {
    layoutText(name, text);
    right = bottom = 0;
    for (const auto *run : {&nameGlyphs_, &textGlyphs_})
        for (const Glyph &g : *run) {
            GlyphImage image;
            if (!glyph(fontFace_, g.codepoint, fontSize_, false, image) || image.w <= 0 || image.h <= 0) continue;
            // The frame reaches its width past the ink on every side, and the shadow 2 pixels right and down.
            int lean = g.italic ? image.h / 5 + 1 : 0, reach = std::max(frameWidth(), hasShadow() ? 2 : 0);
            right = std::max(right, g.x + image.left + image.w + lean + reach);
            bottom = std::max(bottom, g.y - image.top + image.h + reach);
        }
}

void Engine::drawGlyph(Canvas &out, const Glyph &g, int pass, int alpha, int r, int gr, int b, int fr, int fg, int fb) {
    if (alpha <= 0) return;
    int frame = frameWidth();
    bool shadow = hasShadow();
    GlyphImage image;
    if (!glyph(fontFace_, g.codepoint, fontSize_, false, image) || image.w <= 0 || image.h <= 0) return;
    std::vector<uint8_t> slanted;
    const uint8_t *coverage = image.coverage.data();
    int w = image.w;
    if (g.italic) {
        // Windows slants a face it has no italics for by about a fifth of the height.
        int lean = image.h / 5 + 1;
        w = image.w + lean;
        slanted.assign(size_t(w) * image.h, 0);
        for (int y = 0; y < image.h; y++) {
            int shift = (image.h - 1 - y) / 5;
            std::memcpy(&slanted[size_t(y) * w + shift], &image.coverage[size_t(y) * image.w], size_t(image.w));
        }
        coverage = slanted.data();
    }
    int x = g.x + image.left, y = g.y - image.top;
    if (pass == 0) {
        if (shadow) drawCoverage(out, coverage, w, image.h, x + 2, y + 2, fr, fg, fb, alpha);
        for (int dy = -frame; frame && dy <= frame; dy++)
            for (int dx = -frame; dx <= frame; dx++)
                if (dx * dx + dy * dy <= frame * frame + 1) drawCoverage(out, coverage, w, image.h, x + dx, y + dy, fr, fg, fb, alpha);
    } else {
        drawCoverage(out, coverage, w, image.h, x, y, r, gr, b, alpha);
    }
}

void Engine::drawComment(Canvas &out, const std::string &utf8, int x, int baseline, int width, int px) {
    int pen = x;
    for (size_t i = 0; i < utf8.size();) {
        unsigned char c = static_cast<unsigned char>(utf8[i]);
        int extra = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
        uint32_t cp = extra == 0 ? c : extra == 1 ? c & 0x1F : extra == 2 ? c & 0x0F : c & 0x07;
        for (int k = 1; k <= extra && i + k < utf8.size(); k++) cp = cp << 6 | (static_cast<unsigned char>(utf8[i + k]) & 0x3F);
        i += size_t(extra) + 1;
        GlyphImage g;
        if (!glyph(commentFace_, cp, px, false, g)) continue;
        // A glyph's ink can reach past its advance, as a box-drawing line's does, so both stay inside the width.
        if (pen + std::max(g.advance, g.left + g.w) > x + width) break;
        if (g.w > 0 && g.h > 0) drawCoverage(out, g.coverage.data(), g.w, g.h, pen + g.left, baseline - g.top, 255, 255, 255, 256);
        pen += g.advance;
    }
}

void Engine::drawGlyphs(Canvas &out, const std::vector<Glyph> &glyphs, int r, int g, int b, int fr, int fg, int fb) {
    for (int pass = 0; pass < 2; pass++)
        for (const Glyph &glyph : glyphs) drawGlyph(out, glyph, pass, 256, r, g, b, fr, fg, fb);
}

void Engine::drawGlyphRun(Canvas &out) {
    // Frames first, so a neighbouring glyph's frame never covers a letter.
    for (int pass = 0; pass < 2; pass++) {
        // The name is there at once; the text's characters each fade in, one after another.
        // The last lines before the marker have a red edge, which Config can turn off.
        bool counting = gt_.shownCount > 0 && setting("count_color") && !setting("gt_off");
        int shadowR_ = counting ? kCountEdgeR : this->shadowR_, shadowG_ = counting ? kCountEdgeG : this->shadowG_, shadowB_ = counting ? kCountEdgeB : this->shadowB_;
        for (const Glyph &g : nameGlyphs_) drawGlyph(out, g, pass, 256, textR_, textG_, textB_, shadowR_, shadowG_, shadowB_);
        int index = 0;
        for (const Glyph &g : textGlyphs_) {
            int64_t since = now_ - textStart_ - int64_t(index++) * charMs_;
            drawGlyph(out, g, pass, textDone_ ? 256 : int(std::clamp<int64_t>(since * 256 / kCharFadeMs, 0, 256)), textR_, textG_, textB_, shadowR_, shadowG_, shadowB_);
        }
    }
}

const uint8_t *Engine::frame(int &w, int &h, bool &changed) {
    int mx = 0, my = 0, mw = 0, mh = 0;
    if (dirty_) {
        compose();
        dirty_ = false;
        markSaved_ = false;
        frameChanged_ = true;
    }
    // The 4:3 frame is built again whenever the 16:9 one changes or the mark is showing, which moves with the strip.
    if (square_) {
        bool mark = messageInStrip_ && markShowing();
        if (frameChanged_ || mark || markSaved_ || frame_.h != squareY_ + squareShift_) {
            composeStrip();
            markSaved_ = mark;
            frameChanged_ = true;
        }
        w = frame_.w;
        h = frame_.h;
        changed = frameChanged_;
        frameChanged_ = false;
        return frame_.px.data();
    }
    if (markShowing()) {
        markRect(mx, my, mw, mh);
        if (markSaved_) canvas_.restoreRect(mx, my, mw, mh, underMark_);
        else canvas_.saveRect(mx, my, mw, mh, underMark_);
        markSaved_ = true;
        drawMark(canvas_);
        frameChanged_ = true;
    } else if (markSaved_) {
        // The story moved on with nothing else to redraw, so take the mark off.
        markRect(mx, my, mw, mh);
        canvas_.restoreRect(mx, my, mw, mh, underMark_);
        markSaved_ = false;
        frameChanged_ = true;
    }
    w = canvas_.w;
    h = canvas_.h;
    changed = frameChanged_;
    frameChanged_ = false;
    return canvas_.px.data();
}

// Whether the frame is 4:3: Config's `screensize` picks 16:9 (1 to 3) or 4:3 (4 to 6), and with it automatic (0) the
// shape nearer the host's view, with room either side of the middle so a window dragged across it doesn't flicker.
bool Engine::wantsSquare() const {
    if (!squareImage_) return false;
    int size = setting("screensize");
    if (size >= 1 && size <= 3) return false;
    if (size >= 4 && size <= 6) return true;
    if (viewAspect_ <= 0) return square_;
    return square_ ? viewAspect_ < 1.60f : viewAspect_ < 1.48f;
}

// The 4:3 frame: the 16:9 frame, the MsgWinSquare picture under it, and over that picture the message layer drawn as
// for 16:9 and moved down MsgWinSquareMovement rows, as Test Game 88's PC draws it in a 1280x960 window (under Wine).
void Engine::composeStrip() {
    int total = squareY_ + squareShift_;
    if (frame_.w != width_ || frame_.h != total) frame_.resize(width_, total);
    if (strip_.w != width_ || strip_.h != height_) strip_.resize(width_, height_);
    size_t row = size_t(width_) * 4;
    std::memcpy(frame_.px.data(), canvas_.px.data(), row * size_t(std::min(canvas_.h, total)));
    // The layer's rows from height_ - squareShift_ go to squareY_; under them is the picture.
    int from = height_ - squareShift_;
    std::fill(strip_.px.begin(), strip_.px.end(), uint8_t(0));
    drawPart(strip_, *squareImage_, 0, from, 0, 0, squareImage_->w, squareImage_->h, 1);
    if (messageInStrip_) {
        composeMessage(strip_);
        if (markShowing()) drawMark(strip_);
    }
    for (int y = 0; y < squareShift_ && from + y < strip_.h && squareY_ + y < total; y++)
        std::memcpy(frame_.px.data() + size_t(squareY_ + y) * row, strip_.px.data() + size_t(from + y) * row, row);
}

}  // namespace neko
