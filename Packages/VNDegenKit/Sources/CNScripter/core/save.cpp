// Save files, in the PC engine's formats, so saves move between VNDegen and Windows both ways. NScripter 2.95 to 3.04
// all write the same layout ("format 2"). docs/nscripter-cleanroom.md says how it was worked out; field names below
// follow what each field was seen to hold. Integers are little-endian int32; strings are Shift-JIS, NUL-terminated.
#include <sys/stat.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <ctime>

#include "engine.h"

namespace vnns {

namespace {

struct Writer {
    Bytes &out;
    void i(int v) {
        uint32_t u = uint32_t(v);
        for (int k = 0; k < 4; k++) out.push_back(uint8_t(u >> (8 * k)));
    }
    void s(const std::string &v) {
        out.insert(out.end(), v.begin(), v.end());
        out.push_back(0);
    }
};

struct Reader {
    const Bytes &d;
    size_t p = 0;
    bool bad = false;
    int i() {
        if (p + 4 > d.size()) {
            bad = true;
            return 0;
        }
        int v = int32_t(le32(&d[p]));
        p += 4;
        return v;
    }
    std::string s() {
        size_t e = p;
        while (e < d.size() && d[e]) e++;
        if (e >= d.size()) {
            bad = true;
            p = d.size();  // every later read fails at once
            return {};
        }
        std::string v(reinterpret_cast<const char *>(&d[p]), e - p);
        p = e + 1;
        return v;
    }
    // A count of strings that follow, each at least one byte, so it can't be more than the bytes left.
    size_t count() {
        int n = i();
        if (n < 0 || size_t(n) > d.size() - std::min(p, d.size())) {
            bad = true;
            return 0;
        }
        return size_t(n);
    }
};

uint32_t rgbOf(uint32_t px) { return uint32_t(chR(px)) << 16 | uint32_t(chG(px)) << 8 | uint32_t(chB(px)); }
uint32_t pxOf(uint32_t rgb) { return rgba(int(rgb >> 16 & 255), int(rgb >> 8 & 255), int(rgb & 255)); }

std::string joinPath(const std::string &a, const std::string &b) { return a.empty() ? b : a + "\\" + b; }

}  // namespace

// ---- the savepoint ----

bool Engine::insideTextGosub() const {
    return std::any_of(stack_.begin(), stack_.end(), [](const Frame &f) { return f.kind == Frame::TextGosub; });
}

// Freezes the savepoint at the statement starting now: the state without the message history, which saves take as
// it is when they're written.
void Engine::freezeSavepoint(bool atTextHead) {
    (void)atTextHead;
    savepoint_.clear();
    serializeState(savepoint_);
    savepointLine_ = stmtStart_.line;
    savepointStmt_ = stmtStart_.stmt;
    savepointLive_ = false;
}

// Called as each statement starts. Until the first text, saves record the statement that saves. A text line that
// opens a page freezes the savepoint at its head, and it stays there through any commands until the next page, saveon
// or saveoff.
void Engine::noteStatementStart(bool text) {
    stmtStart_ = cur_;
    if (defineMode_ || !text) return;
    bool newPage = page_.empty() && pageText_.empty();
    if (newPage || savepointLive_) freezeSavepoint(true);
}

// ---- serializing ----

// Everything a save holds up to the message history.
void Engine::serializeState(Bytes &out) {
    Writer w{out};
    const SaveExtras &x = extras_;
    w.i(2);
    w.i(win_.bold ? 1 : 0);
    w.i(win_.shadow ? 1 : 0);
    w.i(x.h12);
    w.i(rmode_ ? 1 : 0);
    // The text color is stored red, blue, green.
    w.i(chR(textColor_));
    w.i(chB(textColor_));
    w.i(chG(textColor_));
    w.s(x.h32);
    w.s(x.h33);
    w.i(windowEffect_.type);
    w.i(windowEffect_.duration);
    w.s(windowEffect_.mask);
    w.i(win_.tx);
    w.i(win_.ty);
    w.i(win_.cols);
    w.i(win_.rows);
    w.i(win_.fw);
    w.i(win_.fh);
    w.i(win_.fw + win_.px);
    w.i(win_.fh + win_.py);
    w.i(win_.imageWindow ? -1 : int(rgbOf(win_.color)));
    w.i(textSpeed_);
    w.i(win_.x1);
    w.i(win_.y1);
    w.i(win_.x2);
    w.i(win_.y2);
    w.s(win_.imageWindow ? win_.imageTag : std::string());
    for (int v : x.h100) w.i(v);
    w.s(scene_.bgTag);
    for (const Sprite &s : scene_.ld) w.s(s.loaded ? s.img.tag : std::string());
    for (const Sprite &s : scene_.ld) w.i(s.x);
    for (const Sprite &s : scene_.ld) w.i(s.y);
    for (int q : x.ldQ) w.i(q);
    for (const Sprite &s : scene_.ld) w.i(s.alpha);
    for (const Sprite &s : scene_.sp) {
        w.s(s.img.tag);
        w.i(s.x);
        w.i(s.y);
        w.i(s.visible ? 1 : 0);
        w.i(s.cell);
        w.i(s.alpha);
    }
    for (int k = 0; k < globalBorder_ && size_t(k) < num_.size(); k++) {
        w.i(num_[size_t(k)]);
        w.s(str_[size_t(k)]);
    }
    // The call stack, flattened: a gosub is its return offset; a for loop is variable, limit, step and the negated
    // offset of its body. textgosub frames aren't saved: the savepoint is the head of their text page.
    std::vector<int> stack;
    for (const Frame &f : stack_) {
        if (f.kind == Frame::TextGosub) continue;
        if (f.kind == Frame::For) {
            stack.insert(stack.end(), {f.var, f.to, f.step, -int(f.pos)});
        } else {
            stack.push_back(int(f.pos));
        }
    }
    w.i(int(stack.size()));
    for (int v : stack) w.i(v);
    w.i(scene_.monocro ? 1 : 0);
    uint32_t mono = scene_.monocroColor;
    w.i(chB(mono));
    w.i(chG(mono));
    w.i(chR(mono));
    w.i(scene_.nega);
    w.s(x.pStr1);
    w.s(waveName_);
    w.i(x.waveA);
    w.i(x.waveB);
    w.i(waveLoop_ ? 1 : 0);
    w.i(x.waveC);
    bool bgmLooping = !bgmName_.empty() && bgmLoop_;
    w.i(bgmLooping ? 1 : 0);
    w.i(bgmLooping || !loopBgmTail_.empty() ? 1 : 0);
    w.s(bgmLooping ? bgmName_ : std::string());
    w.i(eraseOnEffect_ ? 1 : 0);
    w.i(x.pInt4);
    for (const Scene::Bar &b : scene_.bars) {
        w.i(b.value);
        w.i(b.on ? b.x : -1);
        w.i(b.y);
        w.i(b.w);
        w.i(b.h);
        w.i(b.max);
        w.i(int(rgbOf(b.color)));
    }
    for (const Scene::Prnum &p : scene_.prnums) {
        w.i(p.value);
        w.i(p.on ? p.x : -1);
        w.i(p.y);
        w.i(p.w);
        w.i(p.h);
        w.i(int(rgbOf(p.color)));
    }
    w.i(x.qInt1);
    w.i(scene_.allSpHidden ? 1 : 0);
    w.i(x.qInt2);
    w.s(btnImage_.tag);
    for (int idx : arrayOrder_)
        for (int v : arrays_[idx].data) w.i(v);
    w.i(x.qInt3);
    w.i(x.qInt4);
    w.s(loopBgmHead_);
    w.s(loopBgmTail_);
    w.i(rubyOn_ ? 1 : 0);
    w.i(rubyOn_ ? rubyW_ : 0);
    w.i(rubyOn_ ? rubyH_ : 0);
    w.s(rubyFont_);
    w.i(x.qInt5);
    for (const Sprite2 &q : scene_.sp2) {
        w.s(q.img.tag);
        w.i(q.cx);
        w.i(q.cy);
        w.i(q.scaleX);
        w.i(q.scaleY);
        w.i(q.rot);
        w.i(q.visible ? 1 : 0);
        w.i(q.alpha);
        w.i(q.blend == Blend::Add ? 1 : q.blend == Blend::Sub ? 2 : 0);
    }
    for (int k = 0; k < 8; k++) {
        int v = x.tail[k];
        if (k == 2) v = windowShown_ ? 1 : 0;  // whether the text window is up
        if (k == 4) v = scene_.allSpHidden ? 1 : 0;
        if (k == 5) v = scene_.allSp2Hidden ? 1 : 0;
        w.i(v);
    }
    w.s(x.tStr);
    w.i(width_ / 4);
    w.i(width_ / 2);
    w.i(width_ * 3 / 4);
    w.i(underline_);
}

// The message history as it is now, then the savepoint's script position.
void Engine::serializeTail(Bytes &out, int line, int stmt) {
    Writer w{out};
    w.i(int(log_.size()));
    for (const std::string &p : log_) w.s(p);
    w.i(int(logTags_.size()));
    for (const std::string &t : logTags_) w.s(t);
    w.i(line);
    w.i(stmt);
}

// Reads a save into the engine and positions the script at its savepoint.
bool Engine::restoreState(const Bytes &data, std::string *error) {
    Reader r{data};
    if (r.i() != 2) {
        if (error) *error = "not an NScripter format-2 save";
        return false;
    }
    SaveExtras x;
    Scene sc;
    win_.bold = r.i() != 0;
    win_.shadow = r.i() != 0;
    x.h12 = r.i();
    rmode_ = r.i() != 0;
    int tr = r.i(), tb = r.i(), tg = r.i();
    textColor_ = rgba(tr, tg, tb);
    x.h32 = r.s();
    x.h33 = r.s();
    windowEffect_.type = r.i();
    windowEffect_.duration = r.i();
    windowEffect_.mask = r.s();
    win_.tx = r.i();
    win_.ty = r.i();
    win_.cols = r.i();
    win_.rows = r.i();
    win_.fw = r.i();
    win_.fh = r.i();
    win_.px = r.i() - win_.fw;
    win_.py = r.i() - win_.fh;
    int color = r.i();
    textSpeed_ = r.i();
    win_.x1 = r.i();
    win_.y1 = r.i();
    win_.x2 = r.i();
    win_.y2 = r.i();
    std::string winImage = r.s();
    win_.imageWindow = color == -1;
    if (win_.imageWindow) {
        win_.imageTag = winImage;
        loadImage(winImage, win_.image);
    } else {
        win_.color = pxOf(uint32_t(color));
    }
    for (int &v : x.h100) v = r.i();
    sc.bgTag = r.s();
    std::string ldTags[3];
    for (auto &t : ldTags) t = r.s();
    int ldx[3], ldy[3], lda[3];
    for (int &v : ldx) v = r.i();
    for (int &v : ldy) v = r.i();
    for (int &v : x.ldQ) v = r.i();
    for (int &v : lda) v = r.i();
    for (int k = 0; k < 3; k++) {
        Sprite &s = sc.ld[size_t(k)];
        if (!ldTags[k].empty() && loadImage(ldTags[k], s.img)) s.loaded = s.visible = true;
        s.img.tag = ldTags[k];
        s.x = ldx[k];
        s.y = ldy[k];
        s.alpha = lda[k];
    }
    for (Sprite &s : sc.sp) {
        std::string tag = r.s();
        s.x = r.i();
        s.y = r.i();
        s.visible = r.i() != 0;
        s.cell = r.i();
        s.alpha = r.i();
        if (!tag.empty() && loadImage(tag, s.img)) s.loaded = true;
        s.img.tag = tag;
        if (!s.loaded) s.visible = false;
    }
    for (int k = 0; k < globalBorder_ && size_t(k) < num_.size(); k++) {
        num_[size_t(k)] = r.i();
        str_[size_t(k)] = r.s();
    }
    int n = r.i();
    std::vector<int> flat;
    for (int k = 0; k < n && !r.bad; k++) flat.push_back(r.i());
    // Read the stack from the top down: a negative entry ends a for loop's four.
    std::vector<Frame> frames;
    for (int k = int(flat.size()); k > 0;) {
        Frame f;
        int v = flat[size_t(k - 1)];
        if (v < 0 && k >= 4) {
            f.kind = Frame::For;
            f.var = flat[size_t(k - 4)];
            f.to = flat[size_t(k - 3)];
            f.step = flat[size_t(k - 2)];
            f.pos = size_t(-v);
            k -= 4;
        } else {
            f.kind = Frame::Gosub;
            f.pos = size_t(std::max(v, 0));
            k -= 1;
        }
        f.pos = std::min(f.pos, script_.text().size());
        f.line = script_.lineAt(f.pos);
        // The statement index of a return position: the ':' separators before it on its line.
        f.stmt = 0;
        for (size_t p = script_.lineStart(f.line); p < f.pos; p += sjisCharLen(script_.text(), p))
            if (script_.text()[p] == ':') f.stmt++;
        frames.push_back(f);
    }
    std::reverse(frames.begin(), frames.end());
    sc.monocro = r.i() != 0;
    int mb = r.i(), mg = r.i(), mr = r.i();
    sc.monocroColor = rgba(mr, mg, mb);
    sc.nega = r.i();
    x.pStr1 = r.s();
    std::string wave = r.s();
    x.waveA = r.i();
    x.waveB = r.i();
    bool waveLoop = r.i() != 0;
    x.waveC = r.i();
    bool bgmOn = r.i() != 0;
    r.i();  // also set for loopbgm
    std::string bgm = r.s();
    eraseOnEffect_ = r.i() != 0;
    x.pInt4 = r.i();
    for (Scene::Bar &b : sc.bars) {
        b.value = r.i();
        b.x = r.i();
        b.on = b.x != -1;
        b.y = r.i();
        b.w = r.i();
        b.h = r.i();
        b.max = r.i();
        b.color = pxOf(uint32_t(r.i()));
    }
    for (Scene::Prnum &p : sc.prnums) {
        p.value = r.i();
        p.x = r.i();
        p.on = p.x != -1;
        p.y = r.i();
        p.w = r.i();
        p.h = r.i();
        p.color = pxOf(uint32_t(r.i()));
    }
    x.qInt1 = r.i();
    sc.allSpHidden = r.i() != 0;
    x.qInt2 = r.i();
    std::string btndef = r.s();
    for (int idx : arrayOrder_)
        for (int &v : arrays_[idx].data) v = r.i();
    x.qInt3 = r.i();
    x.qInt4 = r.i();
    std::string loopHead = r.s(), loopTail = r.s();
    rubyOn_ = r.i() != 0;
    int rw = r.i(), rh = r.i();
    if (rubyOn_) {
        rubyW_ = rw;
        rubyH_ = rh;
    }
    rubyFont_ = r.s();
    x.qInt5 = r.i();
    for (Sprite2 &q : sc.sp2) {
        std::string tag = r.s();
        q.cx = r.i();
        q.cy = r.i();
        q.scaleX = r.i();
        q.scaleY = r.i();
        q.rot = r.i();
        q.visible = r.i() != 0;
        q.alpha = r.i();
        int blend = r.i();
        q.blend = blend == 1 ? Blend::Add : blend == 2 ? Blend::Sub : Blend::Normal;
        if (!tag.empty() && loadImage(tag, q.img)) q.loaded = true;
        q.img.tag = tag;
    }
    for (int &v : x.tail) v = r.i();
    sc.allSp2Hidden = x.tail[5] != 0;
    x.tStr = r.s();
    r.i();
    r.i();
    r.i();  // standing image centres follow from the screen width
    underline_ = r.i();
    size_t historyAt = r.p;
    std::vector<std::string> log(r.count());
    for (auto &p : log) p = r.s();
    std::vector<std::string> tags(r.count());
    for (auto &t : tags) t = r.s();
    int line = r.i(), stmt = r.i();
    bool windowUp = x.tail[2] != 0;
    if (r.bad) {
        if (error) *error = "the save is shorter than its contents need";
        return false;
    }

    // Commit: the scene, sound, text and the script position. A movie playing now belongs to the game left behind.
    stopMovie();
    if (sc.bgTag == "black" || sc.bgTag == "white" || (!sc.bgTag.empty() && sc.bgTag[0] == '#')) {
        sc.bgColor = sc.bgTag == "white" ? rgba(255, 255, 255) : sc.bgTag == "black" ? rgba(0, 0, 0) : pxOf(uint32_t(strtoul(sc.bgTag.c_str() + 1, nullptr, 16)));
    } else if (!sc.bgTag.empty()) {
        loadImage(sc.bgTag, sc.bg);
    }
    extras_ = x;
    scene_ = std::move(sc);
    commitScene();
    btnImage_ = LoadedImage();
    if (!btndef.empty()) loadImage(btndef, btnImage_);
    log_ = std::move(log);
    logTags_ = std::move(tags);
    page_.clear();
    pageText_.clear();
    penX_ = penY_ = 0;
    windowShown_ = windowUp;
    stack_ = std::move(frames);
    buttons_.clear();
    pendingChars_.clear();
    inTextLine_ = false;
    wait_ = WaitKind::None;
    onWaitDone_ = nullptr;
    effect_.active = false;
    for (int c = 0; c < 50; c++) audio_.stop(c);
    audio_.stop(AudioMixer::kWave);
    waveName_.clear();
    waveLoop_ = false;
    if (waveLoop && !wave.empty()) {
        Bytes d;
        if (readSound(wave, d)) audio_.play(AudioMixer::kWave, std::move(d), true, volSe_);
        waveName_ = wave;
        waveLoop_ = true;
    }
    loopBgmPending_ = false;
    loopBgmHead_ = loopHead;
    loopBgmTail_ = loopTail;
    if (bgmOn && !bgm.empty()) {
        if (bgm != bgmName_ || !audio_.playing(AudioMixer::kBgm)) playBgm(bgm, true);
    } else if (!loopTail.empty()) {
        Bytes d;
        if (readSound(loopTail, d)) audio_.play(AudioMixer::kBgm, std::move(d), true, volBgm_);
        bgmName_.clear();
    } else {
        audio_.stop(AudioMixer::kBgm);
        bgmName_.clear();
    }
    jumpToLine(std::clamp(line, 0, script_.lineCount() - 1));
    // A statement number past the line's last statement stops at the line's end.
    for (int k = 0; k < stmt; k++) {
        skipStatement();
        if (peek() != ':') break;
        cur_.pos++;
        cur_.stmt++;
    }
    // The loaded save is the savepoint until the game moves on.
    savepoint_.assign(data.begin(), data.begin() + long(historyAt));
    savepointLine_ = line;
    savepointStmt_ = stmt;
    savepointLive_ = false;
    return true;
}

// ---- save slots ----

std::string Engine::savePath(int slot) const {
    return fs_.hostPath(joinPath(saveDir_, "save" + std::to_string(slot) + ".dat"));
}

bool Engine::saveGame(int slot, const std::string &note) {
    Bytes data;
    if (savepointLive_) {
        serializeState(data);
        serializeTail(data, stmtStart_.line, stmtStart_.stmt);
    } else {
        data = savepoint_;
        serializeTail(data, savepointLine_, savepointStmt_);
    }
    if (!note.empty() || savegame2_) {
        // savegame2's note follows the position: the note in double quotes, then '*'.
        data.push_back('"');
        data.insert(data.end(), note.begin(), note.end());
        data.push_back('"');
        data.push_back('*');
    }
    std::string path = savePath(slot);
    if (!fs_.write(path, data.data(), data.size())) {
        logf("could not write %s", path.c_str());
        return false;
    }
    saveSystemData();
    return true;
}

bool Engine::loadGame(int slot) {
    Bytes data;
    if (!readFile(savePath(slot), data)) return false;
    // A note from savegame2 isn't part of the state.
    // restoreState finds a broken save only after reading part of it into these; the rest waits until the save has
    // read through. The PC engine goes on as if nothing happened, so they're put back if the load fails.
    auto win = win_;
    auto windowEffect = windowEffect_;
    auto num = num_;
    auto str = str_;
    auto arrays = arrays_;
    auto rubyFont = rubyFont_;
    uint32_t textColor = textColor_;
    int textSpeed = textSpeed_, rubyW = rubyW_, rubyH = rubyH_, underline = underline_;
    bool rmode = rmode_, eraseOnEffect = eraseOnEffect_, rubyOn = rubyOn_;
    std::string error;
    if (!restoreState(data, &error)) {
        logf("save %d: %s", slot, error.c_str());
        win_ = std::move(win);
        windowEffect_ = std::move(windowEffect);
        num_ = std::move(num);
        str_ = std::move(str);
        arrays_ = std::move(arrays);
        rubyFont_ = std::move(rubyFont);
        textColor_ = textColor;
        textSpeed_ = textSpeed;
        rubyW_ = rubyW;
        rubyH_ = rubyH;
        underline_ = underline;
        rmode_ = rmode;
        eraseOnEffect_ = eraseOnEffect;
        rubyOn_ = rubyOn;
        return false;
    }
    if (!loadGosub_.empty()) gosub(cur_.pos, cur_.line, cur_.stmt, loadGosub_);
    return true;
}

bool Engine::loadSlot(int slot) {
    // The define block sets what saves don't hold (aliases, effects, arrays); run it to `game` first.
    for (int guard = 0; defineMode_ && !ended_ && guard < 1000000; guard++) executeStatement();
    return loadGame(slot);
}

bool Engine::readSaveInfo(int slot, SaveInfo &info) {
    struct stat st;
    std::string path = savePath(slot);
    if (stat(path.c_str(), &st) != 0) return false;
    struct tm lt;
    time_t t = st.st_mtime;
    localtime_r(&t, &lt);
    info.month = lt.tm_mon + 1;
    info.day = lt.tm_mday;
    info.hour = lt.tm_hour;
    info.minute = lt.tm_min;
    // savegame2's note: the bytes between the last two quotes, before the trailing '*'.
    Bytes data;
    if (readFile(path, data) && data.size() >= 3 && data.back() == '*' && data[data.size() - 2] == '"') {
        size_t end = data.size() - 2;
        size_t start = end;
        while (start > 0 && data[start - 1] != '"') start--;
        if (start > 0) info.note.assign(reinterpret_cast<const char *>(&data[start]), end - start);
    }
    return true;
}

// ---- system data: global variables, the file and label logs, read text ----

void Engine::loadGlobals() {
    Bytes data;
    if (globalOn_ && readFile(fs_.hostPath(joinPath(saveDir_, "gloval.sav")), data)) {
        size_t p = 0;
        for (size_t i = size_t(globalBorder_); i < num_.size() && p + 4 <= data.size(); i++) {
            num_[i] = int32_t(le32(&data[p]));
            p += 4;
            size_t end = p;
            while (end < data.size() && data[end]) end++;
            str_[i].assign(reinterpret_cast<const char *>(&data[p]), end - p);
            p = end + 1;
        }
    }
    // Logs: a decimal count and a newline, then each name in quotes, uppercase, every byte XOR 0x84.
    auto readLog = [&](const char *file, auto add) {
        Bytes d;
        if (!readFile(fs_.hostPath(joinPath(saveDir_, file)), d)) return;
        size_t p = 0;
        while (p < d.size() && d[p] != '\n') p++;
        while (p < d.size()) {
            while (p < d.size() && d[p] != '"') p++;
            size_t start = ++p;
            while (p < d.size() && d[p] != '"') p++;
            if (p > d.size()) break;
            std::string name;
            for (size_t k = start; k < p && k < d.size(); k++) name += char(d[k] ^ 0x84);
            if (!name.empty()) add(name);
            p++;
        }
    };
    if (fileLogOn_) {
        // The PC engine logs its default cursors and message history buttons as it starts.
        for (const char *f : {"uoncur.bmp", "uoffcur.bmp", "doncur.bmp", "doffcur.bmp", "cursor0.bmp", "cursor1.bmp"})
            noteFileUsed(f);
        readLog("NScrflog.dat", [&](const std::string &n) { noteFileUsed(n); });
    }
    if (labelLogOn_) {
        readLog("NScrllog.dat", [&](const std::string &n) {
            int idx = script_.labelIndex(sjisLower(n));
            if (idx >= 0) noteLabelVisited(idx);
        });
        noteLabelVisited(script_.labelIndex("define"));
    }
    if (kidokuOn_) {
        // An existing file keeps its size; a new one is sized as the PC engine sizes it.
        if (!readFile(fs_.hostPath(joinPath(saveDir_, "kidoku.dat")), kidoku_) || kidoku_.empty())
            kidoku_.assign(script_.kidokuSize(), 0);
    }
}

void Engine::noteFileUsed(const std::string &file) {
    // Logged as the PC engine logs it: the file part of the tag, uppercase.
    std::string key = normalizePath(file);
    if (fileLog_.insert(key).second) fileLogOrder_.push_back(key);
}

void Engine::noteLabelVisited(int idx) {
    if (idx < 0) return;
    if (size_t(idx) >= labelLog_.size()) labelLog_.resize(script_.labels().size());
    if (!labelLog_[size_t(idx)]) {
        labelLog_[size_t(idx)] = true;
        labelLogOrder_.push_back(idx);
    }
}

void Engine::markRead(size_t pos) {
    if (!kidokuOn_ || pos / 8 >= kidoku_.size()) return;
    kidoku_[pos / 8] |= uint8_t(1u << (pos % 8));
}

bool Engine::isRead(size_t pos) const {
    return pos / 8 < kidoku_.size() && (kidoku_[pos / 8] >> (pos % 8) & 1);
}

static std::string upperAscii(std::string s) {
    for (size_t i = 0; i < s.size(); i += sjisCharLen(s, i))
        if (sjisCharLen(s, i) == 1) s[i] = char(toupper(uint8_t(s[i])));
    return s;
}

void Engine::saveSystemData() {
    if (globalOn_) {
        Bytes out;
        Writer w{out};
        for (size_t i = size_t(globalBorder_); i < num_.size(); i++) {
            w.i(num_[i]);
            w.s(str_[i]);
        }
        std::string path = fs_.hostPath(joinPath(saveDir_, "gloval.sav"));
        if (!fs_.write(path, out.data(), out.size())) logf("could not write %s", path.c_str());
    }
    auto writeLog = [&](const char *file, const std::vector<std::string> &names) {
        std::string out = std::to_string(names.size()) + "\n";
        for (const std::string &n : names) {
            out += '"';
            for (char c : upperAscii(n)) out += char(uint8_t(c) ^ 0x84);
            out += '"';
        }
        fs_.write(fs_.hostPath(joinPath(saveDir_, file)), out.data(), out.size());
    };
    if (fileLogOn_) {
        std::vector<std::string> names;
        for (const std::string &k : fileLogOrder_) {
            // A tag such as ":c;file" logs only its file part.
            size_t semi = k.find(';');
            names.push_back(k[0] == ':' && semi != std::string::npos ? k.substr(semi + 1) : k);
        }
        writeLog("NScrflog.dat", names);
    }
    if (labelLogOn_) {
        std::vector<std::string> names;
        for (int idx : labelLogOrder_) names.push_back(script_.labels()[size_t(idx)]);
        writeLog("NScrllog.dat", names);
    }
    if (kidokuOn_ && !kidoku_.empty())
        fs_.write(fs_.hostPath(joinPath(saveDir_, "kidoku.dat")), kidoku_.data(), kidoku_.size());
}

void Engine::noteTextLineStart() { noteStatementStart(true); }

// ---- commands ----

namespace {
using E = Engine;

void cmdSaveGame(E &e, const std::string &name) {
    int slot = e.readInt();
    std::string note;
    e.savegame2_ = name == "savegame2";
    if (e.savegame2_ && e.readComma()) note = e.readStr();
    e.saveGame(slot, note);
    e.savegame2_ = false;
}

void cmdLoadGame(E &e, const std::string &) { e.loadGame(e.readInt()); }

// saveon: saves follow the current statement again. saveoff: the savepoint stays at the saveoff.
void cmdSaveOn(E &e, const std::string &) {
    if (e.autoSaveOff_) return;
    e.saveOn_ = true;
    e.savepointLive_ = true;
}

void cmdSaveOff(E &e, const std::string &) {
    if (e.autoSaveOff_) return;
    if (e.savepointLive_) e.freezeSavepoint();
    e.saveOn_ = false;
}

void cmdSavePoint(E &e, const std::string &) { e.freezeSavepoint(); }

void cmdAutoSaveOff(E &e, const std::string &) {
    e.autoSaveOff_ = true;
    e.saveOn_ = false;
}

void cmdSaveFileExist(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    E::SaveInfo info;
    e.setVar(v, e.readSaveInfo(e.readInt(), info) ? 1 : 0);
}

void cmdSaveTime(E &e, const std::string &) {
    int slot = e.readInt();
    E::SaveInfo info;
    bool ok = e.readSaveInfo(slot, info);
    int vals[4] = {ok ? info.month : 0, info.day, info.hour, info.minute};
    for (int i = 0; i < 4 && e.readComma(); i++) e.setVar(e.readVar(), vals[i]);
}

void cmdGetSaveStr(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    E::SaveInfo info;
    e.readSaveInfo(e.readInt(), info);
    e.setVar(v, info.note);
}

// getscreenshot w,h keeps the screen scaled to w x h; savescreenshot writes it as a 24-bit BMP and forgets it,
// savescreenshot2 keeps it for more files, deletescreenshot forgets it.
void cmdGetScreenshot(E &e, const std::string &) {
    int w = e.readInt();
    e.readComma();
    int h = e.readInt();
    if (w <= 0 || h <= 0) return;
    e.composeFrame();
    const Image &f = e.frame_;
    Image shot(w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            // Box-average the source area each target pixel covers.
            int x0 = x * f.w / w, x1 = std::max(x0 + 1, (x + 1) * f.w / w);
            int y0 = y * f.h / h, y1 = std::max(y0 + 1, (y + 1) * f.h / h);
            int r = 0, g = 0, b = 0, n = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    uint32_t p = f.row(yy)[xx];
                    r += chR(p);
                    g += chG(p);
                    b += chB(p);
                    n++;
                }
            shot.row(y)[x] = rgba(r / n, g / n, b / n);
        }
    e.screenshot_ = std::make_shared<Image>(std::move(shot));
}

void cmdSaveScreenshot(E &e, const std::string &name) {
    std::string file = e.readStr();
    if (!e.screenshot_) return;
    const Image &img = *e.screenshot_;
    uint32_t stride = uint32_t(img.w * 3 + 3) & ~3u;
    Bytes out(54 + size_t(stride) * size_t(img.h), 0);
    auto put32 = [&](size_t at, uint32_t v) {
        for (int k = 0; k < 4; k++) out[at + size_t(k)] = uint8_t(v >> (8 * k));
    };
    out[0] = 'B';
    out[1] = 'M';
    put32(2, uint32_t(out.size()));
    put32(10, 54);
    put32(14, 40);
    put32(18, uint32_t(img.w));
    put32(22, uint32_t(img.h));
    out[26] = 1;
    out[28] = 24;
    for (int y = 0; y < img.h; y++) {
        uint8_t *row = &out[54 + size_t(img.h - 1 - y) * stride];
        for (int x = 0; x < img.w; x++) {
            uint32_t p = img.row(y)[x];
            row[x * 3] = uint8_t(chB(p));
            row[x * 3 + 1] = uint8_t(chG(p));
            row[x * 3 + 2] = uint8_t(chR(p));
        }
    }
    std::string path = e.fs_.hostPath(file);
    if (!e.fs_.write(path, out.data(), out.size())) logf("could not write %s", path.c_str());
    // The image cache may hold an older picture of this name, such as a save thumbnail.
    e.forgetImage(file);
    if (name == "savescreenshot") e.screenshot_.reset();
}

void cmdDeleteScreenshot(E &e, const std::string &) { e.screenshot_.reset(); }

void cmdKidokuSkip(E &e, const std::string &) {
    e.kidokuSkip_ = true;
    e.kidokuOn_ = true;
}

}  // namespace

void registerSaveCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["savegame"] = cmdSaveGame;
    t["savegame2"] = cmdSaveGame;
    t["loadgame"] = cmdLoadGame;
    t["saveon"] = cmdSaveOn;
    t["saveoff"] = cmdSaveOff;
    t["savepoint"] = cmdSavePoint;
    t["autosaveoff"] = cmdAutoSaveOff;
    t["savefileexist"] = cmdSaveFileExist;
    t["savetime"] = cmdSaveTime;
    t["getsavestr"] = cmdGetSaveStr;
    t["kidokuskip"] = cmdKidokuSkip;
    t["getscreenshot"] = cmdGetScreenshot;
    t["savescreenshot"] = cmdSaveScreenshot;
    t["savescreenshot2"] = cmdSaveScreenshot;
    t["deletescreenshot"] = cmdDeleteScreenshot;
}

}  // namespace vnns
