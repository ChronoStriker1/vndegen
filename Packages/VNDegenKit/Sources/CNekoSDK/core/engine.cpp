#include "engine.h"

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <cerrno>
#include <ctime>
#include <iterator>
#include <sys/stat.h>

#include "font.h"
#include "sjis.h"
#include "tables.h"

namespace neko {
namespace {

constexpr int kKindBgm = 0, kKindSe = 1, kKindVoice = 2;
// How long the message window takes to slide in, read from the PC game's footage.
constexpr int kWindowSlideMs = 200;
// How long each character of text takes to fade in, measured from the PC game's footage.
constexpr int kCharFadeMs = 160;
// How far the backlog dims the frame behind it, from the PC game's footage: about 0.29 of the brightness.
constexpr int kLogDim = 182;
// How long Auto waits after the text and voice are done.
constexpr int kAutoDelayMs = 1000;
constexpr float kPi = 3.14159265f;
constexpr float kShakeMs = 300, kShakePixelsX = 24, kShakePixelsY = 28;

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
    // Test Game 95 says 1020 but draws on 1080: its message window and buttons reach the bottom of that.
    height_ = std::max(config_.number("WindowHeight", 720), windowImage_ ? windowY_ + windowImage_->h : 0);
    canvas_.resize(width_, height_);
    canvas_.clear();

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
    logTextR_ = config_.number("MsgLogShadowColorR", 255);
    logTextG_ = config_.number("MsgLogShadowColorG", 128);
    logTextB_ = config_.number("MsgLogShadowColorB");
    logFrameR_ = config_.number("MsgLogColorR");
    logFrameG_ = config_.number("MsgLogColorG");
    logFrameB_ = config_.number("MsgLogColorB");

    // What a game's exe draws on its own, which no data file names.
    folder_ = normalizedPath(config_.text("UserDataDirectory"));
    layout_ = saveLayout(folder_);
    thumbW_ = config_.number("SaveSmallImageSizeW", 160);
    thumbH_ = config_.number("SaveSmallImageSizeH", 90);
    // Where Windows keeps them, under the data folder, so a backup from a PC restores to the same place.
    if (!dataDir.empty()) {
        std::string relative = config_.text("UserDataDirectory");
        std::replace(relative.begin(), relative.end(), '\\', '/');
        saveDir_ = dataDir + "/AppData/Roaming/" + relative + "/save";
    }
    if (folder_ == "waffle\\x2019_retry") {
        bars_ = {images_->get("obj\\l000_000.bmp"), images_->get("obj\\l000_001.bmp")};
        barY_ = {0, 618};
        // The PC game shows no dimming from its message window here, at its Config defaults and at 90.
        windowOpacity_ = 0;
    }

    if (config_.number("MsgWinButtonPlateDisable") == 0) buttonPlate_ = images_->get(config_.text("MsgWinButtonPlateFilename"));
    barTop_ = bars_.size() > 1 ? barY_[1] : height_ - 102;
    plateX_ = config_.number("MsgWinButtonPlatePosX");
    plateY_ = config_.number("MsgWinButtonPlatePosY");
    plateHiddenY_ = config_.number("MsgWinButtonPlateBottomPosY", height_);
    if (Bytes ini = files_.read(config_.text("MsgButtonDeclFilename", "cfg\\msg_button.ini"))) messageButtons_ = parseMessageButtons(*ini, *images_);
    titleBgm_ = config_.text("TitleBGM");

    if (const char *table = menuTable(folder_))
        screens_ = parseScreens(table, *images_, [this](const std::string &what) { warnOnce(what); });
    log("started: %dx%d, %d scripts in scr, %s", width_, height_, int(files_.list("scr").size()), screens_.empty() ? "no menus yet" : "menus");
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
    if (fromStory) returnTo_.clear();
    else if (screen_ && screen_->name == "title") returnTo_ = "title";
    else if (name == "title") returnTo_.clear();
    screen_ = &it->second;
    pressed_.clear();
    dirty_ = true;
    if (name == "title" && !titleBgm_.empty() && !titleMusic_) {
        if (Bytes data = files_.read(titleBgm_)) {
            titleMusic_ = audio_.open(data);
            if (titleMusic_) {
                audio_.setLoops(titleMusic_, 0);
                audio_.play(titleMusic_);
            }
        }
    }
}

void Engine::closeScreen() {
    screen_ = nullptr;
    dirty_ = true;
}

void Engine::startStory() {
    if (titleMusic_) {
        audio_.fade(titleMusic_, 500, 0, true);
        titleMusic_ = 0;
    }
    screen_ = nullptr;
    stack_.clear();
    scene_ = {};
    back_.clear();
    front_.clear();
    scenario_.clear();
    waiting_ = Waiting::None;
    std::string first = config_.text("FirstFlow");
    auto flow = first.empty() ? nullptr : loadScript(first);
    if (!flow || !flow->first()) {
        complain("This game has no flow script to start from.");
        waiting_ = Waiting::Ended;
        return;
    }
    stack_.push_back({flow, flow->first()});
    log("story from %s", first.c_str());
    dirty_ = true;
}

void Engine::runAction(const std::string &action) {
    log("action %s", action.c_str());
    dirty_ = true;
    if (action == "start") { startStory(); return; }
    if (action == "quit" || action == "Exit") { openDialog(7, [this] { waiting_ = Waiting::Ended; }); return; }
    if (action == "Title") { openDialog(6, [this] { screen_ = nullptr; openScreen("title"); }); return; }
    if (action == "Skip") { skipToggle_ = !skipToggle_; return; }
    if (action == "Auto") { autoMode_ = !autoMode_; return; }
    if (action == "HideWin") { windowHiddenByPlayer_ = true; return; }
    if (action == "LogMode") { openLog(); return; }
    if (action == "Voice") {
        if (!currentText_.voice.empty()) {
            if (voicePlayer_) audio_.stop(voicePlayer_);
            if (Bytes data = files_.read(currentText_.voice)) {
                voicePlayer_ = audio_.open(data);
                if (voicePlayer_) {
                    audio_.setLoops(voicePlayer_, 1);
                    audio_.play(voicePlayer_);
                }
            }
        }
        return;
    }
    if (action == "QuickSave") {
        noteThumbnail();
        openDialog(4, [this] { saveToSlot(0); });
        return;
    }
    if (action == "QuickLoad" || action == "quickload") {
        if (slotInfo(0).exists) openDialog(5, [this] { loadFromSlot(0); });
        return;
    }
    if (action == "log_close") { closeLog(); return; }
    if (action == "log_up") { scrollLog(1); return; }
    if (action == "log_down") { scrollLog(-1); return; }
    if (action == "log_top") { scrollLog(int(backlog_.size())); return; }
    if (action == "log_bottom") { scrollLog(-int(backlog_.size())); return; }
    if (action == "back") {
        if (returnTo_.empty()) closeScreen();
        else openScreen(returnTo_);
        return;
    }
    if (action.rfind("page", 0) == 0) { page_ = std::atoi(action.c_str() + 4); return; }
    if (action == "nav_save" || action == "Save") { noteThumbnail(); openScreen("save"); return; }
    if (action == "nav_load" || action == "Load" || action == "load") { noteThumbnail(); openScreen("load"); return; }
    if (action == "nav_config" || action == "Config" || action == "config") { noteThumbnail(); openScreen("config"); return; }
    if (action == "nav_cg" || action == "cg") { openScreen("cg"); return; }
    if (action == "nav_scene" || action == "scene") { openScreen("scene"); return; }
    if (action == "nav_music" || action == "music") { openScreen("music"); return; }
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

void Engine::mouseWheel(int notches) {
    if (screen_ || dialogOpen_) return;
    if (logOpen_) scrollLog(notches);
    else if (notches > 0) openLog();
}

// MARK: Saves

std::string Engine::savePath(int slot, const char *extension) const {
    char name[32];
    std::snprintf(name, sizeof name, "/save%03d.%s", slot, extension);
    return saveDir_ + name;
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

std::vector<uint8_t> readWhole(const std::string &path) {
    std::ifstream f(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(f), {}};
}

// Written beside the target and renamed over it, so a crash can't leave half a save.
bool writeWhole(const std::string &path, const std::vector<uint8_t> &data) {
    std::string temporary = path + ".part";
    {
        std::ofstream f(temporary, std::ios::binary | std::ios::trunc);
        if (!f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()))) return false;
    }
    return std::rename(temporary.c_str(), path.c_str()) == 0;
}

}  // namespace

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
    d.flow = stack_.empty() ? "" : stack_.back().script->name;
    d.flowNode = flowCallId_;
    d.choiceImage = choiceImage_;
    d.dateFlag = choiceImage_.empty() ? 0 : 1;
    d.variables = scenario_;
    d.width = canvas_.w;
    d.height = canvas_.h;
    for (const auto &[slot, s] : front_) {
        if (!s.image) continue;
        SavedSprite out;
        out.slot = slot;
        out.path = s.path;
        out.x = s.x;
        out.y = s.y;
        out.scaleX = s.scaleX;
        out.scaleY = s.scaleY;
        out.anchorX = s.image->w / 2;
        out.anchorY = s.image->h / 2;
        out.alpha = int(s.alpha * 256 + 0.5f);
        out.blur = s.blurAmount;
        out.blurOn = s.blurOn;
        // Every change as a line of text from the oldest one's start, as the PC writes them.
        int64_t oldest = s.changes.empty() ? now_ : s.changes.front().begunMs;
        for (const Animation &a : s.changes) oldest = std::min(oldest, a.begunMs);
        auto frames = [this](int64_t ms) { return int(ms * fps_ / 1000); };
        for (const Animation &a : s.changes) {
            for (const Track &t : a.tracks) {
                const char *name = t.property == Track::X ? (t.relative ? "relative_pos_x" : "pos_x") : t.property == Track::Y ? (t.relative ? "relative_pos_y" : "pos_y") : t.property == Track::ScaleX ? "scale_x" : t.property == Track::ScaleY ? "scale_y" : t.property == Track::Alpha ? "alpha" : "rotate";
                int length = frames(t.lengthMs);
                char line[128];
                std::snprintf(line, sizeof line, "%s,%g,%d,%d,%s\n", name, double(t.relative ? t.to - t.from : t.to), frames(t.startMs + (a.begunMs - oldest)), t.easeOut ? -length : length, t.easeOut ? "quadratic" : "linear");
                out.animation += line;
            }
        }
        out.animated = !out.animation.empty();
        out.animationFrames = out.animated ? frames(now_ - oldest) : 0;
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
    d.scene = scene_.script ? scene_.script->name : "";
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
    slots_.erase(slot);
    log("saved slot %d at %s #%d", slot, data.scene.c_str(), data.node);
    return true;
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
    restoreSave(data);
    log("loaded slot %d at %s #%d", slot, data.scene.c_str(), data.node);
    return true;
}

void Engine::restoreSave(const SaveData &d) {
    stopKind(kKindBgm, 300);
    stopKind(kKindSe, 300);
    stopKind(kKindVoice, 0);
    if (titleMusic_) {
        audio_.fade(titleMusic_, 300, 0, true);
        titleMusic_ = 0;
    }
    screen_ = nullptr;
    stack_.clear();
    back_.clear();
    front_.clear();
    transitioning_ = false;
    shakeType_ = 0;
    flashMs_ = 0;
    blur_ = false;
    scenario_ = d.variables;
    choiceImage_ = d.choiceImage;
    flowCallId_ = d.flowNode;
    if (auto flow = d.flow.empty() ? nullptr : loadScript(d.flow)) {
        const Node *call = flow->node(d.flowNode);
        stack_.push_back({flow, call ? flow->node(call->next) : flow->first()});
    }
    for (const SavedSprite &saved : d.sprites) {
        Sprite s;
        s.path = saved.path;
        s.image = images_->get(saved.path);
        if (!s.image) warnOnce("A save names a missing picture: " + saved.path);
        s.x = saved.x;
        s.y = saved.y;
        s.scaleX = saved.scaleX;
        s.scaleY = saved.scaleY;
        s.alpha = float(saved.alpha) / 256;
        s.blurAmount = saved.blur;
        s.blurOn = saved.blurOn;
        if (!saved.animation.empty()) {
            Animation a = parseAnimation(saved.animation, s);
            a.begunMs = now_ - int64_t(saved.animationFrames) * 1000 / std::max(1, fps_);
            s.changes.push_back(std::move(a));
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
        audio_.setLoops(player, 0);
        audio_.fade(player, 0, sound.volume, false);
        if (sound.pan) audio_.setPan(player, sound.pan, 0);
        audio_.seekMs(player, int64_t(sound.seconds * 1000));
        audio_.play(player);
        channelsOf(kind)[channel] = player;
        (kind == kKindBgm ? bgmNow_ : seNow_)[channel] = {sound.path, sound.volume, sound.pan};
    }
    auto script = d.scene.empty() ? nullptr : loadScript(d.scene);
    scene_ = script ? Frame{script, script->node(d.node)} : Frame{};
    windowShown_ = d.windowShown;
    windowShownAt_ = -100000;
    layoutText(sjis::toUtf8(d.current.name), sjis::toUtf8(d.current.text));
    textStart_ = now_;
    textDone_ = true;
    waiting_ = Waiting::Click;
    dirty_ = true;
}

// The table's `slots` line: the sheet, then x, y, the step across and down, and rows per column. The `slot*` lines give
// where each part sits within a card.
int Engine::slotAt(int x, int y) const {
    const auto *grid = screen_ ? screen_->setting("slots") : nullptr;
    if (!grid || grid->size() < 7) return -1;
    int x0 = std::atoi((*grid)[1].c_str()), y0 = std::atoi((*grid)[2].c_str()), dx = std::atoi((*grid)[3].c_str()), dy = std::atoi((*grid)[4].c_str());
    int perColumn = std::atoi((*grid)[5].c_str()), columns = std::atoi((*grid)[6].c_str());
    ImageRef sheet = images_->get((*grid)[0]);
    if (!sheet) return -1;
    int w = sheet->w, h = sheet->h / 5;
    for (int c = 0; c < columns; c++)
        for (int r = 0; r < perColumn; r++) {
            int cx = x0 + c * dx, cy = y0 + r * dy;
            if (x >= cx && y >= cy && x < cx + w && y < cy + h) return page_ * perColumn * columns + c * perColumn + r + 1;
        }
    return -1;
}

void Engine::composeSlots(Canvas &out) {
    const auto *grid = screen_->setting("slots");
    if (!grid || grid->size() < 7) return;
    int x0 = std::atoi((*grid)[1].c_str()), y0 = std::atoi((*grid)[2].c_str()), dx = std::atoi((*grid)[3].c_str()), dy = std::atoi((*grid)[4].c_str());
    int perColumn = std::atoi((*grid)[5].c_str()), columns = std::atoi((*grid)[6].c_str());
    ImageRef sheet = images_->get((*grid)[0]);
    if (!sheet) return;
    int cardH = sheet->h / 5;
    auto offset = [&](const char *key, int index) {
        const auto *line = screen_->setting(key);
        return line && int(line->size()) > index ? std::atoi((*line)[size_t(index)].c_str()) : 0;
    };
    auto picture = [&](const char *key) { const auto *line = screen_->setting(key); return line && !line->empty() ? images_->get((*line)[0]) : nullptr; };
    ImageRef digits = picture("slotdigits"), dateDigits = picture("datedigits"), noLabel = picture("slotlabel");
    ImageRef slash = picture("dateslash"), colon = picture("datecolon"), dash = picture("datedash");
    bool loading = screen_->name == "load";
    int hovered = pointerSeen_ ? slotAt(pointerX_, pointerY_) : -1;
    auto drawNumber = [&](const ImageRef &sheetOfTen, int value, int count, int x, int y) {
        if (!sheetOfTen) return x;
        int w = sheetOfTen->w / 10;
        for (int i = count - 1; i >= 0; i--) {
            int digit = value % 10;
            value /= 10;
            drawPart(out, *sheetOfTen, x + i * w, y, digit * w, 0, w, sheetOfTen->h, 1);
        }
        return x + count * w;
    };
    for (int c = 0; c < columns; c++)
        for (int r = 0; r < perColumn; r++) {
            int slot = page_ * perColumn * columns + c * perColumn + r + 1;
            SlotInfo &info = slotInfo(slot);
            int cx = x0 + c * dx, cy = y0 + r * dy;
            int row = loading && !info.exists ? int(StateDisabled) : slot == hovered ? int(StateHover) : int(StateNormal);
            drawPart(out, *sheet, cx, cy, 0, row * cardH, sheet->w, cardH, 1);
            if (noLabel) drawPart(out, *noLabel, cx + offset("slotlabel", 1), cy + offset("slotlabel", 2), 0, 0, noLabel->w, noLabel->h, 1);
            drawNumber(digits, slot, 2, cx + offset("slotdigits", 1), cy + offset("slotdigits", 2));
            if (info.thumbnail) drawPart(out, *info.thumbnail, cx + offset("slotthumb", 0), cy + offset("slotthumb", 1), 0, 0, info.thumbnail->w, info.thumbnail->h, 1);
            int x = cx + offset("datedigits", 1), y = cy + offset("datedigits", 2);
            auto mark = [&](const ImageRef &m) {
                if (!m) return;
                drawPart(out, *m, x, y, 0, 0, m->w, m->h, 1);
                x += m->w;
            };
            if (info.exists) {
                x = drawNumber(dateDigits, info.date[0], 4, x, y);
                mark(slash);
                x = drawNumber(dateDigits, info.date[1], 2, x, y);
                mark(slash);
                x = drawNumber(dateDigits, info.date[2], 2, x, y);
                x += offset("dategap", 0);
                x = drawNumber(dateDigits, info.date[3], 2, x, y);
                mark(colon);
                x = drawNumber(dateDigits, info.date[4], 2, x, y);
                mark(colon);
                x = drawNumber(dateDigits, info.date[5], 2, x, y);
            }
        }
}

void Engine::openDialog(int question, const std::function<void()> &yes) {
    if (!screens_.count("dialog")) {
        yes();
        return;
    }
    dialogOpen_ = true;
    dialogQuestion_ = question;
    dialogYes_ = yes;
    dirty_ = true;
}

// The table's dialog screen: a band, one row of a sheet of questions, and yes and no.
void Engine::composeDialog(Canvas &out) {
    const Screen &dialog = screens_.at("dialog");
    tintFrame(out, 0, 0, 0, 128);
    for (const Picture &p : dialog.pictures) {
        Picture shown = p;
        if (p.rows > 1) shown.row = dialogQuestion_;
        drawPicture(out, shown);
    }
    for (const Button &b : dialog.buttons) drawButton(out, b, pointerSeen_ && b.contains(pointerX_, pointerY_) ? StateHover : StateNormal);
}

bool Engine::pressAction(const std::string &action) {
    if (!screen_) return false;
    for (const Button &b : screen_->buttons)
        if (b.action == action) {
            std::string copy = action;
            runAction(copy);
            return true;
        }
    return false;
}

Button *Engine::buttonAt(int x, int y, std::vector<Button> &buttons) {
    for (Button &b : buttons)
        if (b.contains(x, y)) return &b;
    return nullptr;
}

void Engine::composeScreen(Canvas &out) {
    out.clear();
    for (const Picture &p : screen_->pictures) drawPicture(out, p);
    composeSlots(out);
    bool fromTitle = returnTo_ == "title";
    for (Button &b : screen_->buttons) {
        bool over = pointerSeen_ && b.contains(pointerX_, pointerY_) && !dialogOpen_;
        ButtonState state = over ? StateHover : StateNormal;
        if (b.action == "nav_" + screen_->name || b.action == "page" + std::to_string(page_)) state = over ? StateSelectedHover : StateSelected;
        if (b.action == "nav_save" && fromTitle) state = StateDisabled;
        drawButton(out, b, state);
    }
    if (dialogOpen_) composeDialog(out);
}

// `under` buttons come up with their plate when the pointer's at the left of the bar, and `inner` ones when it's anywhere else
// along it, as Test Game 97 does. A game with no plate keeps its buttons up all the time.
bool Engine::innerButtonsShown() const {
    if (!buttonPlate_) return true;
    return pointerSeen_ && pointerY_ >= barTop_ && pointerX_ >= plateX_ + buttonPlate_->w;
}

bool Engine::underButtonsShown() const {
    return buttonPlate_ && pointerSeen_ && pointerY_ >= plateY_ && pointerX_ < plateX_ + buttonPlate_->w;
}

void Engine::composeMessageButtons(Canvas &out) {
    auto state = [&](const Button &b) {
        bool over = pointerSeen_ && b.contains(pointerX_, pointerY_);
        bool on = (b.action == "Skip" && skipToggle_) || (b.action == "Auto" && autoMode_);
        return on ? (over ? StateSelectedHover : StateSelected) : over ? StateHover : StateNormal;
    };
    if (innerButtonsShown())
        for (const Button &b : messageButtons_.inner) drawButton(out, b, state(b));
    if (!underButtonsShown()) return;
    drawPart(out, *buttonPlate_, plateX_, plateY_, 0, 0, buttonPlate_->w, buttonPlate_->h, 1);
    for (const Button &b : messageButtons_.under) drawButton(out, b, state(b));
}

// The backlog: the story's frame, dimmed, with a page of the last lines, oldest at the top.
void Engine::composeLog(Canvas &out) {
    tintFrame(out, 0, 0, 0, kLogDim);
    const Screen *log = screens_.count("log") ? &screens_.at("log") : nullptr;
    if (!log) return;
    for (const Button &b : log->buttons) drawButton(out, b, pointerSeen_ && b.contains(pointerX_, pointerY_) ? StateHover : StateNormal);
    const auto *rows = log->setting("rows");
    if (!rows || rows->size() < 7) return;
    ImageRef rule = images_->get((*rows)[0]);
    int ruleX = std::atoi((*rows)[1].c_str()), ruleY = std::atoi((*rows)[2].c_str()), step = std::atoi((*rows)[3].c_str());
    int count = std::atoi((*rows)[4].c_str()), textX = std::atoi((*rows)[5].c_str()), textY = std::atoi((*rows)[6].c_str());
    int lastRow = std::min(count, int(backlog_.size()) - logTop_);
    int ascent = fontAscent(fontFace_, fontSize_);
    for (int row = 0; row < lastRow; row++) {
        // Row 0 is the oldest on the page: the backlog runs newest first.
        const SavedText &t = backlog_[size_t(logTop_ + lastRow - 1 - row)];
        int y = ruleY + row * step;
        if (rule) drawPart(out, *rule, ruleX, y, 0, 0, rule->w, rule->h, 1);
        std::string text = sjis::toUtf8(t.text);
        std::string name = sjis::toUtf8(t.name);
        std::vector<Glyph> glyphs;
        layoutRun(name, nameY_ == textY_ ? textX - (textX_ - nameX_) : textX, y + textY + ascent, nameY_ == textY_, glyphs);
        drawGlyphs(out, glyphs, logTextR_, logTextG_, logTextB_, logFrameR_, logFrameG_, logFrameB_);
        glyphs.clear();
        layoutRun(text, textX, y + textY + ascent, false, glyphs);
        drawGlyphs(out, glyphs, logTextR_, logTextG_, logTextB_, logFrameR_, logFrameG_, logFrameB_);
    }
}

std::shared_ptr<Script> Engine::loadScript(const std::string &path) {
    std::string key = normalizedPath(path);
    auto it = scripts_.find(key);
    if (it != scripts_.end()) return it->second;
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
    scripts_[key] = script;
    return script;
}

// MARK: The interpreter

void Engine::advance() {
    Frame &frame = scene_.node ? scene_ : stack_.back();
    frame.node = frame.script->node(frame.node->next);
}

void Engine::run() {
    // Bounded so a script that loops without waiting can't hang the app. No library script comes near it in one step.
    for (int guard = 0; guard < 200000; guard++) {
        if (waiting_ != Waiting::None) return;
        if (scene_.node) {
            const Node &n = *scene_.node;
            runScene(n);
            if (scene_.node == &n) advance();
            continue;
        }
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
        const Node &n = *stack_.back().node;
        Frame &flow = stack_.back();
        const Node *before = flow.node;
        runFlow(n);
        if (flow.node == before) flow.node = flow.script->node(n.next);
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
            flowCallId_ = n.id;
            log("scene %s", script->name.c_str());
        }
        break;
    }
    case 2:  // the end of the story, which goes back to the title
        log("flow end: %s", sjis::toUtf8(n.s[0]).c_str());
        flow.node = nullptr;
        break;
    case 3:
        beginChoice(n);
        break;
    case 4:  // the other target for the option shown first, `next` for the second: a save the PC wrote after the first
        flow.node = flow.script->node(choiceResult_ == 0 ? n.otherwise : n.next);
        break;
    case 5: {  // a condition on a variable: `next` when it holds
        int left = variableValue(n.p[0], n.p[1]), right = n.p[4] == 2 ? n.p[5] : variableValue(n.p[4], n.p[5]);
        // Only `<=`, 2, is read from a script's own words so far. The others follow the order such lists usually have.
        bool holds = n.p[3] == 0 ? left == right : n.p[3] == 1 ? left != right : n.p[3] == 2 ? left <= right : n.p[3] == 3 ? left >= right : n.p[3] == 4 ? left < right : left > right;
        flow.node = flow.script->node(holds ? n.next : n.otherwise);
        break;
    }
    case 6: {  // set or step a variable: scope, index, operation (0 set, 1 add one), value
        auto &table = n.p[0] == 1 ? scenario_ : globals_;
        if (n.p[3] == 1) table[n.p[1]]++;
        else table[n.p[1]] = n.p[5];
        log("variable %d:%d = %d", n.p[0], n.p[1], table[n.p[1]]);
        break;
    }
    case 31: {  // go on in another flow script
        auto next = loadScript(n.s[1]);
        flow.script = next;
        flow.node = next ? next->first() : nullptr;
        break;
    }
    default:
        warnOnce("flow opcode " + std::to_string(n.opcode) + " isn't built: " + sjis::toUtf8(n.s[0]));
        break;
    }
}

int Engine::variableValue(int scope, int index) const {
    const auto &table = scope == 1 ? scenario_ : globals_;
    auto it = table.find(index);
    return it == table.end() ? 0 : it->second;
}

void Engine::beginChoice(const Node &n) {
    choiceSheet_ = images_->get(n.s[1]);
    choiceImage_ = n.s[1];
    if (!choiceSheet_) {
        complain("Missing choice " + n.s[1]);
        choiceResult_ = 0;
        return;
    }
    choiceCount_ = std::max(1, n.p[1]);
    choiceBack_ = images_->get(config_.text("ChoiceBackFilename", "obj\\n000_000.bmp"));
    choiceHover_ = -1;
    waiting_ = Waiting::Choice;
    dirty_ = true;
    log("choice %s, %d options", n.s[1].c_str(), choiceCount_);
}

void Engine::choose(int index) {
    if (waiting_ != Waiting::Choice) return;
    choiceResult_ = index;
    log("chose %d", index);
    choiceSheet_ = nullptr;
    choiceBack_ = nullptr;
    waiting_ = Waiting::None;
    dirty_ = true;
}

void Engine::runScene(const Node &n) {
    const auto &p = n.p;
    switch (n.opcode) {
    case 0:  // the start marker
    case 1:  // the first node's copy of the scene name, seen only with no parameters
        break;
    case 2:  // wait
        waiting_ = Waiting::Time;
        waitUntil_ = now_ + p[0];
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
    case 15:  // load an image into a slot
    case 23:  // load an image for a difference switch: the same, shown by the next update
        loadImage(n);
        break;
    case 17: {  // delete a slot
        back_.erase(p[0]);
        break;
    }
    case 18:
        transform(n);
        break;
    case 19:
        startNamedAnimation(n);
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
        flashMs_ = std::max(1, p[0]);
        flashR_ = p[1];
        flashG_ = p[2];
        flashB_ = p[3];
        flashStart_ = now_;
        dirty_ = true;
        break;
    case 51:  // noise over the screen, and its end: not drawn yet
    case 52:
        warnOnce("noise isn't drawn yet");
        break;
    case 53:  // shake the screen
        shakeType_ = p[0];
        shakeSustained_ = false;
        shakeStart_ = now_;
        shakeStopAt_ = -1;
        break;
    case 55:  // a hand-held camera's sway, until stopped: up and down, then sideways, each with a period in frames
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
    case 116:  // drives an external accessory, which VNDegen doesn't have
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
        warnOnce("scene opcode " + std::to_string(n.opcode) + " isn't built: " + sjis::toUtf8(n.s[0]));
        break;
    }
}

bool Engine::step(int ms) {
    if (paused_) return waiting_ != Waiting::Ended;
    if (screen_ || logOpen_ || dialogOpen_) return waiting_ != Waiting::Ended;
    now_ += ms;

    if (transitioning_ && (skip_ || skipToggle_ || now_ >= transitionStart_ + transitionMs_)) {
        transitioning_ = false;
        ruleImage_ = nullptr;
        dirty_ = true;
    }
    if (waiting_ == Waiting::Time && (skip_ || skipToggle_ || now_ >= waitUntil_)) waiting_ = Waiting::None;

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
        else if (autoReadyAt_ < 0) autoReadyAt_ = now_ + kAutoDelayMs;
        else if (now_ >= autoReadyAt_) autoNext = true;
    } else {
        autoReadyAt_ = -1;
    }
    if (waiting_ == Waiting::Click && (clicked_ || skipping || autoNext)) {
        clicked_ = false;
        if (!textDone_ && !skipping) {
            // The first click finishes the text; the next one moves on.
            textDone_ = true;
            dirty_ = true;
        } else {
            if (voicePlayer_) { audio_.stop(voicePlayer_); voicePlayer_ = 0; }
            waiting_ = Waiting::None;
        }
    }
    clicked_ = false;
    if (mouseMoved_) {
        mouseMoved_ = false;
        updateChoice();
    }
    run();

    for (auto *set : {&back_, &front_})
        for (auto &[slot, s] : *set) foldFinishedChanges(s);
    if (anythingMoves()) dirty_ = true;
    return waiting_ != Waiting::Ended;
}

void Engine::setPaused(bool paused) {
    paused_ = paused;
    for (auto *channels : {&bgm_, &se_, &voice_})
        for (auto &[channel, player] : *channels) audio_.pause(player, paused);
}

void Engine::mouseMove(float x, float y) {
    mouseX_ = x;
    mouseY_ = y;
    mouseMoved_ = true;
    int px = int(x * float(width_)), py = int(y * float(height_));
    if (px != pointerX_ || py != pointerY_ || !pointerSeen_) dirty_ = true;
    pointerX_ = px;
    pointerY_ = py;
    pointerSeen_ = true;
}

void Engine::mouseButton(int button, bool down) {
    if (!down) return;
    if (button == 1) {
        // The right button steps back: out of the log or a menu, else hides or shows the window.
        if (dialogOpen_) {
            dialogOpen_ = false;
            dialogYes_ = nullptr;
            dirty_ = true;
        } else if (logOpen_) {
            closeLog();
        } else if (screen_) {
            if (screen_->name != "title") runAction("back");
        } else if (windowShown_) {
            windowHiddenByPlayer_ = !windowHiddenByPlayer_;
            dirty_ = true;
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
            dialogOpen_ = false;
            dirty_ = true;
            auto yes = dialogYes_;
            dialogYes_ = nullptr;
            if (b->action == "yes" && yes) yes();
        }
        return;
    }
    if (screen_) {
        if (Button *b = buttonAt(pointerX_, pointerY_, screen_->buttons)) {
            std::string action = b->action;
            if (!(action == "nav_save" && returnTo_ == "title")) runAction(action);
            return;
        }
        int slot = slotAt(pointerX_, pointerY_);
        if (slot < 0) return;
        if (screen_->name == "save") {
            bool overwrite = slotInfo(slot).exists;
            openDialog(overwrite ? 2 : 0, [this, slot] { saveToSlot(slot); dirty_ = true; });
        } else if (screen_->name == "load" && slotInfo(slot).exists) {
            openDialog(1, [this, slot] { loadFromSlot(slot); });
        }
        return;
    }
    if (windowShown_) {
        if (innerButtonsShown())
            if (Button *b = buttonAt(pointerX_, pointerY_, messageButtons_.inner)) {
                std::string action = b->action;
                runAction(action);
                return;
            }
        if (underButtonsShown())
            if (Button *b = buttonAt(pointerX_, pointerY_, messageButtons_.under)) {
                std::string action = b->action;
                runAction(action);
                return;
            }
    }
    if (waiting_ == Waiting::Choice) {
        updateChoice();
        if (choiceHover_ >= 0) choose(choiceHover_);
        return;
    }
    clicked_ = true;
}

// Options stack around the middle of the screen. Where the PC game puts them is still to be measured.
void Engine::updateChoice() {
    if (waiting_ != Waiting::Choice || !choiceSheet_) return;
    int cellW = choiceSheet_->w / choiceCount_, cellH = choiceSheet_->h / 5;
    int x = (width_ - cellW) / 2, top = (height_ - cellH * choiceCount_) / 2;
    int px = int(mouseX_ * float(width_)), py = int(mouseY_ * float(height_));
    int hover = px >= x && px < x + cellW && py >= top && py < top + cellH * choiceCount_ ? (py - top) / cellH : -1;
    if (hover != choiceHover_) {
        choiceHover_ = hover;
        dirty_ = true;
    }
}

bool Engine::jump(const std::string &script, int node) {
    auto target = loadScript(script);
    if (!target || !target->node(node)) return false;
    const Node *n = target->first();
    scene_ = {target, n};
    waiting_ = Waiting::None;
    bool wasSkip = skip_;
    skip_ = true;
    for (int guard = 0; guard < 2000000 && scene_.node && scene_.node->id != node; guard++) {
        const Node &current = *scene_.node;
        runScene(current);
        if (scene_.node == &current) advance();
        waiting_ = Waiting::None;
        transitioning_ = false;
    }
    skip_ = wasSkip;
    if (blur_) previousValid_ = false;
    front_ = back_;
    dirty_ = true;
    return scene_.node != nullptr;
}

const char *Engine::debugStatus() {
    char buffer[256];
    const char *waiting = waiting_ == Waiting::None ? "running" : waiting_ == Waiting::Time ? "time" : waiting_ == Waiting::Click ? "click" : waiting_ == Waiting::Choice ? "choice" : "ended";
    std::snprintf(buffer, sizeof buffer, "%s #%d %s t=%lld", scene_.script ? scene_.script->name.c_str() : "(flow)", scene_.node ? scene_.node->id : -1, waiting, (long long)now_);
    status_ = buffer;
    return status_.c_str();
}

// MARK: Commands

void Engine::loadImage(const Node &n) {
    int slot = n.p[3];
    ImageRef image = images_->get(n.s[1]);
    if (!image) {
        complain("Missing image " + n.s[1]);
        return;
    }
    Sprite &s = back_[slot];
    // A load keeps the slot's position and scale, so a difference image lands where the last one was.
    s.image = image;
    s.path = n.s[1];
}

// Parameters 16 to 19 say which of position, scale, rotation and alpha the command changes.
void Engine::transform(const Node &n) {
    const auto &p = n.p;
    auto it = back_.find(p[3]);
    if (it == back_.end()) return;
    Sprite &s = it->second;
    s.changes.clear();
    if (p[16]) {
        s.x = float(p[1]);
        s.y = float(p[2]);
    }
    if (p[17]) {
        s.scaleX = float(p[4]) / 1000;
        s.scaleY = float(p[5]) / 1000;
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
    if (p[16]) {
        if (p[0] == 1) {
            if (p[1]) add(Track::X, true, 0, float(p[1]));
            if (p[2]) add(Track::Y, true, 0, float(p[2]));
        } else {
            add(Track::X, false, current.x, float(p[1]));
            add(Track::Y, false, current.y, float(p[2]));
        }
    }
    if (p[17]) {
        add(Track::ScaleX, false, current.scaleX, float(p[4]) / 1000);
        add(Track::ScaleY, false, current.scaleY, float(p[5]) / 1000);
    }
    if (p[19]) add(Track::Alpha, false, current.alpha, float(p[15]) / 256);
    s.changes.push_back(std::move(a));
    if (skip_) {
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
    float lastScaleX = s.scaleX, lastScaleY = s.scaleY, lastX = s.x, lastY = s.y, lastAlpha = s.alpha;
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
            push(Track::ScaleX, false, lastScaleX, value);
            push(Track::ScaleY, false, lastScaleY, value);
            lastScaleX = lastScaleY = value;
        }
        else if (name == "scale_x") { push(Track::ScaleX, false, lastScaleX, value); lastScaleX = value; }
        else if (name == "scale_y") { push(Track::ScaleY, false, lastScaleY, value); lastScaleY = value; }
        else if (name == "alpha") { push(Track::Alpha, false, lastAlpha, value); lastAlpha = value; }
        else warnOnce("animation property not read: " + name);
    }
    return a;
}

void Engine::namedAnimation(Sprite &s, const Node &n) {
    foldFinishedChanges(s);
    Animation a = parseAnimation(n.s[1], s);
    a.repeat = n.p[1] != 0;
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
        s.changes.clear();
    }
}

void Engine::screenUpdate(const Node &n) {
    const auto &p = n.p;
    if (skip_ || p[1] <= 0) {
        front_ = back_;
        dirty_ = true;
        return;
    }
    if (dirty_) compose();
    from_ = canvas_;
    front_ = back_;
    transitioning_ = true;
    transitionType_ = p[0];
    transitionMs_ = p[1];
    transitionStart_ = now_;
    ruleImage_ = n.s[1].empty() ? nullptr : images_->get(n.s[1]);
    dirty_ = true;
}

void Engine::showText(const Node &n) {
    // What the PC keeps of it: its saves hold the text on screen and the last 60 lines, newest first.
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
    if (!n.s[3].empty() && !skip_) {
        if (Bytes data = files_.read(n.s[3])) {
            if (voicePlayer_) audio_.stop(voicePlayer_);
            voicePlayer_ = audio_.open(data);
            if (voicePlayer_) {
                audio_.setLoops(voicePlayer_, 1);
                audio_.fade(voicePlayer_, 0, 100, false);
                audio_.play(voicePlayer_);
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
    audio_.fade(player, 0, fade > 0 ? 0 : volume, false);
    if (fade > 0) audio_.fade(player, fade, volume, false);
    audio_.play(player);
    channels[channel] = player;
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
    channels.erase(it);
}

void Engine::stopKind(int kind, int ms) {
    std::map<int, int> &channels = kind == kKindBgm ? bgm_ : kind == kKindSe ? se_ : voice_;
    for (auto &[channel, player] : channels) audio_.fade(player, std::max(ms, 0), 0, true);
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
        s.changes.erase(s.changes.begin());
    }
}

bool Engine::anythingMoves() const {
    if (transitioning_ || shakeType_ || flashMs_ || blur_) return true;
    if (now_ - windowShownAt_ < kWindowSlideMs) return true;
    for (const auto &[slot, s] : front_) {
        if (s.looping) return true;
        if (!s.changes.empty()) return true;
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
    for (const auto &[slot, s] : sprites) {
        if (!s.image) continue;
        SpriteView v = view(s);
        float dx = slot < shakeLock_ ? shakeX : 0, dy = slot < shakeLock_ ? shakeY : 0;
        // About an eighth of the amount as a radius gives the sharpness of the PC game's footage.
        int radius = s.blurOn ? (s.blurAmount + 4) / 8 : 0;
        ImageRef image = radius > 0 ? images_->blurred(s.path, radius) : s.image;
        drawImage(out, image ? *image : *s.image, out.w * 0.5f + v.x + dx, out.h * 0.5f + v.y + dy, v.scaleX, v.scaleY, v.rotation, v.alpha);
    }
}

void Engine::compose() {
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
    composeMessage(canvas_);
    if (dialogOpen_) composeDialog(canvas_);

    if (waiting_ == Waiting::Choice && choiceSheet_) {
        if (choiceBack_) drawPart(canvas_, *choiceBack_, 0, (height_ - choiceBack_->h) / 2, 0, 0, choiceBack_->w, choiceBack_->h, 1);
        int cellW = choiceSheet_->w / choiceCount_, cellH = choiceSheet_->h / 5;
        int x = (width_ - cellW) / 2, top = (height_ - cellH * choiceCount_) / 2;
        for (int i = 0; i < choiceCount_; i++)
            drawPart(canvas_, *choiceSheet_, x, top + i * cellH, i * cellW, (i == choiceHover_ ? 1 : 0) * cellH, cellW, cellH, 1);
    }
}

void Engine::composeMessage(Canvas &out) {
    if (!windowShown_ || windowHiddenByPlayer_) return;
    if (windowImage_) {
        int y = windowY_;
        if (windowShowType_ == 2) {
            int elapsed = int(now_ - windowShownAt_);
            if (elapsed < kWindowSlideMs) y = out.h - (out.h - windowY_) * elapsed / kWindowSlideMs;
        }
        drawPart(out, *windowImage_, windowX_, y, 0, 0, windowImage_->w, windowImage_->h, windowOpacity_);
    }
    drawGlyphRun(out);
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
    int lineHeight = fontSize_ + lineInterval_ + 4;
    size_t first = out.size();
    int penX = x, penY = y, used = 0;
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
            penX = x;
            penY += lineHeight;
            used = 0;
            if (cp == '\n') continue;
        }
        GlyphImage g;
        if (!glyph(fontFace_, cp, fontSize_, false, g)) continue;
        out.push_back({penX, penY, cp, italic});
        penX += g.advance;
        used += units;
    }
    if (endsThere) {
        int width = penX - x;
        for (size_t i = first; i < out.size(); i++) out[i].x -= width;
    }
}

void Engine::layoutText(const std::string &name, const std::string &text) {
    nameGlyphs_.clear();
    textGlyphs_.clear();
    int ascent = fontAscent(fontFace_, fontSize_);
    // A name to the left of the text, on its line, ends at its position. One on a line of its own starts there.
    layoutRun(name, nameX_, nameY_ + ascent, nameY_ == textY_, nameGlyphs_);
    layoutRun(text, textX_, textY_ + ascent, false, textGlyphs_);
}

void Engine::drawGlyph(Canvas &out, const Glyph &g, int pass, int alpha, int r, int gr, int b, int fr, int fg, int fb) {
    if (alpha <= 0) return;
    // init.ini's font types: 1 a shadow, 2 a thin frame, 9 both, 13 a wide frame, 17 a wider one.
    int frame = fontType_ == 17 ? 3 : fontType_ == 13 ? 2 : (fontType_ == 2 || fontType_ == 9) ? 1 : 0;
    bool shadow = fontType_ == 1 || fontType_ == 9;
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

void Engine::drawGlyphs(Canvas &out, const std::vector<Glyph> &glyphs, int r, int g, int b, int fr, int fg, int fb) {
    for (int pass = 0; pass < 2; pass++)
        for (const Glyph &glyph : glyphs) drawGlyph(out, glyph, pass, 256, r, g, b, fr, fg, fb);
}

void Engine::drawGlyphRun(Canvas &out) {
    // Frames first, so a neighbouring glyph's frame never covers a letter.
    for (int pass = 0; pass < 2; pass++) {
        // The name is there at once; the text's characters each fade in, one after another.
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

}  // namespace neko
