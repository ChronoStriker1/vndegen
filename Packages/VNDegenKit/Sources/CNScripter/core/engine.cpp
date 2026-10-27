// The engine's lifecycle, its frame step, waits and input.
#include "engine.h"

#include <algorithm>
#include <cmath>

namespace vnns {

void registerCoreCommands(std::unordered_map<std::string, CommandFn> &t);
void registerSceneCommands(std::unordered_map<std::string, CommandFn> &t);
void registerTextCommands(std::unordered_map<std::string, CommandFn> &t);
void registerButtonCommands(std::unordered_map<std::string, CommandFn> &t);
void registerSoundCommands(std::unordered_map<std::string, CommandFn> &t);
void registerSaveCommands(std::unordered_map<std::string, CommandFn> &t);
void registerSystemCommands(std::unordered_map<std::string, CommandFn> &t);
void registerPluginCommands(std::unordered_map<std::string, CommandFn> &t);
void registerMovieCommands(std::unordered_map<std::string, CommandFn> &t);

const std::unordered_map<std::string, CommandFn> &commandTable() {
    static const std::unordered_map<std::string, CommandFn> table = [] {
        std::unordered_map<std::string, CommandFn> t;
        registerCoreCommands(t);
        registerSceneCommands(t);
        registerTextCommands(t);
        registerButtonCommands(t);
        registerSoundCommands(t);
        registerSaveCommands(t);
        registerSystemCommands(t);
        registerPluginCommands(t);
        registerMovieCommands(t);
        return t;
    }();
    return table;
}


bool Engine::start(std::string *error) {
    if (!fs_.open(cfg_.gameRoot)) {
        if (error) *error = "The game folder can't be read.";
        return false;
    }
    if (!script_.load(fs_, error)) return false;
    const ScriptSettings &st = script_.settings();
    width_ = st.width;
    height_ = st.height;
    underline_ = height_ - 1;
    win_.x2 = width_ - 1;
    win_.y2 = height_ - 1;
    globalBorder_ = st.globalBorder;
    num_.assign(size_t(std::max(st.variables, globalBorder_ + 1)), 0);
    str_.assign(num_.size(), std::string());
    frame_ = Image(width_, height_, rgba(0, 0, 0));
    rngState_ = 12345;
    resetGame(true);
    return true;
}

// reset starts the game again from *start; definereset re-runs the define block. Global variables survive both.
void Engine::resetGame(bool fromDefine) {
    for (int i = 0; i < globalBorder_ && size_t(i) < num_.size(); i++) {
        num_[size_t(i)] = 0;
        str_[size_t(i)].clear();
    }
    stack_.clear();
    scene_ = Scene();
    commitScene();
    clearPage();
    log_.clear();
    windowShown_ = false;
    wait_ = WaitKind::None;
    onWaitDone_ = nullptr;
    effect_.active = false;
    buttons_.clear();
    inTextLine_ = false;
    pendingChars_.clear();
    textColor_ = defaultTextColor_;
    skipMode_ = autoMode_ = false;
    trapMode_ = 0;
    for (int c = 0; c < AudioMixer::kChannels; c++) audio_.stop(c);
    bgmName_.clear();
    if (fromDefine) {
        defineMode_ = true;
        if (!jumpToLabel("define")) jumpToLine(0);
    } else {
        defineMode_ = false;
        if (!jumpToLabel("start")) ended_ = true;
    }
}

int Engine::nextRandom(int n) {
    if (n <= 0) return 0;
    rngState_ = (rngState_ * 1103515245 + 12345) & 0x7FFFFFFF;
    return int((rngState_ >> 8) % n);
}

bool Engine::ctrlSkip() const { return keysDown_.count(VNNS_KEY_CTRL) != 0; }

void Engine::waitTimer(int ms, bool skippable) {
    if (ms <= 0) return;
    wait_ = WaitKind::Timer;
    waitUntil_ = now_ + ms;
    waitSkippable_ = skippable;
}

void Engine::waitClick(std::function<void()> done) {
    wait_ = WaitKind::Click;
    waitStart_ = now_;
    onWaitDone_ = std::move(done);
}

void Engine::finishWait() {
    wait_ = WaitKind::None;
    waitRightToo_ = false;
    auto fn = std::move(onWaitDone_);
    onWaitDone_ = nullptr;
    if (fn) fn();
}

// A left click during a wait jumps to the trap label, if one is set.
bool Engine::checkTrap() {
    if (!trapMode_ || trapStopped_) return false;
    bool hit = (trapMode_ != 3 && in_.click) || ((trapMode_ == 2 || trapMode_ == 3) && in_.rclick);
    if (!hit) return false;
    trapMode_ = 0;
    wait_ = WaitKind::None;
    onWaitDone_ = nullptr;
    if (effect_.active) finishEffect();
    inTextLine_ = false;
    pendingChars_.clear();
    jumpToLabel(trapLabel_);
    return true;
}

void Engine::processWait() {
    bool skipping = skipMode_ || ctrlSkip();
    switch (wait_) {
    case WaitKind::None: break;
    case WaitKind::Timer:
        if (checkTrap()) return;
        if (now_ >= waitUntil_ || (waitSkippable_ && (in_.click || skipping))) finishWait();
        break;
    case WaitKind::Text:
        if (checkTrap()) return;
        if (in_.click) textFast_ = true;
        if (now_ >= waitUntil_ || in_.click || skipping) finishWait();
        break;
    case WaitKind::Click: {
        if (checkTrap()) return;
        bool go = in_.click || skipping;
        for (int k : in_.keys)
            if (k == VNNS_KEY_RETURN || k == VNNS_KEY_SPACE) go = true;
        if (autoClick_ > 0 && now_ - waitStart_ >= autoClick_) go = true;
        if (autoMode_ && now_ - waitStart_ >= autoModeTime_ && !audio_.playing(0)) go = true;
        if (in_.rclick && waitRightToo_) {
            lastRet_ = 0;
            finishWait();
            break;
        }
        if (in_.rclick && rmode_) {
            openRightClickMenu();
            break;
        }
        if (in_.wheel > 0 && !textGosubActive()) {
            openLookback();
            break;
        }
        if (go) {
            lastRet_ = 1;
            lastClickX_ = mouseX_;
            lastClickY_ = mouseY_;
            finishWait();
        }
        break;
    }
    case WaitKind::Effect:
        if (checkTrap()) return;
        if (now_ >= waitUntil_ || (waitSkippable_ && (in_.click || skipping))) {
            finishEffect();
            wait_ = WaitKind::None;
            if (effectBlank_ > 0 && !skipping) waitTimer(effectBlank_, false);
        }
        break;
    case WaitKind::Quake: {
        double left = double(quakeEnd_ - now_);
        if (left <= 0) {
            quakeX_ = quakeY_ = 0;
            wait_ = WaitKind::None;
            sceneDirty_ = true;
            break;
        }
        // Shakes decay over the duration; quakex and quakey shake along one axis.
        double t = 1.0 - left / std::max<int64_t>(1, quakeDuration_);
        int amp = std::max(1, int(quakeAmp_ * (1.0 - t)));
        if (quakeKind_ == 0) {
            quakeX_ = nextRandom(amp * 2 + 1) - amp;
            quakeY_ = nextRandom(amp * 2 + 1) - amp;
        } else {
            double phase = std::sin(t * quakeAmp_ * 2 * M_PI);
            int off = int(phase * 8 * (1.0 - t));
            quakeX_ = quakeKind_ == 1 ? off : 0;
            quakeY_ = quakeKind_ == 2 ? off : 0;
        }
        sceneDirty_ = true;
        break;
    }
    case WaitKind::Button: pollButtonWait(); break;
    case WaitKind::Select: pollSelect(); break;
    case WaitKind::Movie: break;  // stepMovie ends it
    case WaitKind::Dialog: pollSystemUi(); break;
    }
}

bool Engine::step(int ms) {
    if (ended_) return false;
    if (paused_) return true;
    if (quitRequested_) {
        saveSystemData();
        ended_ = true;
        return false;
    }
    now_ += ms;
    frameChanged_ = false;
    if (movie_) stepMovie(ms);
    if (loopBgmPending_ && !audio_.playing(AudioMixer::kBgm)) {
        // loopbgm's head has finished: its loop part takes over.
        loopBgmPending_ = false;
        Bytes data;
        if (readSound(loopBgmTail_, data)) audio_.play(AudioMixer::kBgm, std::move(data), true, volBgm_);
    }
    processWait();
    animateSprites();
    if (!ended_) runUntilBlocked();
    // A wait that started this frame may already be satisfied by this frame's click; leave that to the next frame.
    in_ = Input();
    if (sceneDirty_ || effect_.active) composeFrame();
    return !ended_;
}

// ---- input ----

void Engine::mouseMove(int x, int y) {
    mouseX_ = x;
    mouseY_ = y;
}

void Engine::mouseButton(int button, bool down) {
    if (button == 1) mouseDown_ = down;
    if (!down) return;
    if (button == 1) in_.click = true;
    if (button == 3) in_.rclick = true;
    if (button == 2) in_.mclick = true;
}

void Engine::wheel(int notches) { in_.wheel += notches; }

void Engine::key(int code, bool down) {
    if (down) {
        keysDown_.insert(code);
        in_.keys.push_back(code);
        // Escape works as a right click, except where a button wait reads it itself.
        if (code == VNNS_KEY_ESCAPE && !(wait_ == WaitKind::Button && useEscSpc_)) in_.rclick = true;
    } else {
        keysDown_.erase(code);
    }
}

void Engine::typeText(const std::string &utf8) { typed_ += utf8ToSjis(utf8); }

}  // namespace vnns
