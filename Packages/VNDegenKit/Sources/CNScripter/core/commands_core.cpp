// Define-block settings, variables, arithmetic, strings, control flow and timers.
#include <cctype>
#include <cmath>
#include <ctime>

#include "engine.h"

namespace vnns {

namespace {

using E = Engine;

// Commands that only matter on Windows, or that set features VNDegen doesn't draw, are read and ignored.
void ignoreArgs(E &e, const std::string &) { e.skipStatement(); }

// ---- define block ----

void cmdGame(E &e, const std::string &) {
    e.defineMode_ = false;
    e.loadGlobals();
    if (!e.jumpToLabel("start")) e.ended_ = true;
}

void cmdNsa(E &e, const std::string &) {
    e.fs_.enableNsa();
    e.skipStatement();
}

void cmdCaption(E &e, const std::string &) { e.caption_ = e.readStr(); }
void cmdGlobalOn(E &e, const std::string &) { e.globalOn_ = true; }
void cmdFileLog(E &e, const std::string &) { e.fileLogOn_ = true; }
void cmdLabelLog(E &e, const std::string &) {
    e.labelLogOn_ = true;
    e.labelLog_.resize(e.script().labels().size());
}

void cmdNumAlias(E &e, const std::string &) {
    std::string name = e.readWord();
    e.readComma();
    e.numAlias_[name] = e.readInt();
}

void cmdStrAlias(E &e, const std::string &) {
    std::string name = e.readWord();
    e.readComma();
    e.strAlias_[name] = e.readStr();
}

void cmdDefsub(E &e, const std::string &) {
    e.skipSpaces();
    if (e.peek() == '*') e.cur_.pos++;
    e.defsubs_.insert(e.readWord());
}

void cmdSaveDir(E &e, const std::string &) {
    std::string d = e.readStr();
    e.saveDir_ = d;
}

void cmdSaveNumber(E &e, const std::string &) { e.saveNumber_ = e.readInt(); }

void cmdSaveName(E &e, const std::string &) {
    for (int i = 0; i < 3; i++) {
        if (i) e.readComma();
        e.saveNames_[i] = e.readStr();
    }
}

void cmdIntLimit(E &e, const std::string &) {
    int v = e.readInt();
    e.readComma();
    int lo = e.readInt();
    e.readComma();
    int hi = e.readInt();
    e.intLimits_[v] = {lo, hi};
}

void cmdDim(E &e, const std::string &) {
    e.skipSpaces();
    if (e.peek() != '?') {
        e.errorf("dim expects an array");
        e.skipStatement();
        return;
    }
    E::VarRef v = e.readVar();
    E::ArrayVar a;
    a.dims = v.subs;
    size_t n = 1;
    for (int d : a.dims) n *= size_t(std::max(0, d) + 1);
    a.data.assign(n, 0);
    e.arrays_[v.index] = std::move(a);
}

void cmdMovl(E &e, const std::string &) {
    // movl ?n[...],a,b,c: fills the next dimension from the given prefix.
    E::VarRef v = e.readVar();
    std::vector<int> subs = v.subs;
    auto it = e.arrays_.find(v.index);
    if (it == e.arrays_.end()) {
        e.skipStatement();
        return;
    }
    subs.push_back(0);
    while (e.readComma()) {
        if (int *p = e.arrayElement(v.index, subs)) *p = e.readInt();
        else e.readInt();
        subs.back()++;
    }
}

void cmdTextGosub(E &e, const std::string &) { e.textGosub_ = e.readLabel(); }
void cmdPreTextGosub(E &e, const std::string &) { e.preTextGosub_ = e.readLabel(); }
void cmdLoadGosub(E &e, const std::string &) { e.loadGosub_ = e.readLabel(); }
void cmdMaxKaisouPage(E &e, const std::string &) { e.maxLogPages_ = std::max(1, e.readInt()); }
void cmdUseWheel(E &e, const std::string &) { e.useWheel_ = true; }
void cmdUseEscSpc(E &e, const std::string &) { e.useEscSpc_ = true; }
void cmdLinePage(E &e, const std::string &) { e.linePage_ = true; }
void cmdEnglish(E &e, const std::string &) {
    e.english_ = true;
    e.skipStatement();
}
void cmdKidokuSkip(E &e, const std::string &) { e.kidokuSkip_ = true; }
void cmdHumanz(E &e, const std::string &) { e.humanz_ = e.readInt(); }
void cmdWindowBack(E &e, const std::string &) { e.windowBack_ = true; }
void cmdUnderline(E &e, const std::string &) { e.underline_ = e.readInt(); }
void cmdEffectCut(E &e, const std::string &) { e.effectCut_ = true; }
void cmdEffectSkip(E &e, const std::string &) { e.effectSkip_ = e.readInt() != 0; }
void cmdEffectBlank(E &e, const std::string &) { e.effectBlank_ = e.readInt(); }
void cmdAutomodeTime(E &e, const std::string &) { e.autoModeTime_ = e.readInt(); }
void cmdRmode(E &e, const std::string &) { e.rmode_ = e.readInt() != 0; }
void cmdRoff(E &e, const std::string &) { e.rmode_ = false; }
void cmdRlookback(E &e, const std::string &) { e.lookbackOnRight_ = true; }

void cmdRmenu(E &e, const std::string &) {
    e.rmenu_.clear();
    do {
        std::string text = e.readStr();
        e.readComma();
        std::string fn = e.readWord();
        e.rmenu_.emplace_back(text, fn);
    } while (e.readComma());
}

void cmdTransMode(E &e, const std::string &) {
    std::string m = e.readWord();
    e.transMode_ = m == "copy" ? "c" : m == "alpha" ? "a" : "l";
}

void cmdLabelExist(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    e.setVar(v, e.script().labelLine(e.readLabel()) >= 0 ? 1 : 0);
}

void cmdSetKinsoku(E &e, const std::string &name) {
    std::string start = e.readStr();
    e.readComma();
    std::string end = e.readStr();
    if (name == "setkinsoku") {
        e.kinsokuStart_ = start;
        e.kinsokuEnd_ = end;
    } else {
        e.kinsokuStart_ += start;
        e.kinsokuEnd_ += end;
    }
}

// ---- variables ----

void cmdMov(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    int count = 1;
    if (name.size() > 3) count = atoi(name.c_str() + 3);
    for (int i = 0; i < count; i++) {
        if (!e.readComma()) break;
        if (v.kind == E::VarRef::Str) {
            e.setVar(v, e.readStr());
        } else {
            e.setVar(v, e.readInt());
        }
        if (v.kind == E::VarRef::Array && !v.subs.empty()) v.subs.back()++;
        else v.index++;
    }
}

void cmdArith(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    e.readComma();
    if (v.kind == E::VarRef::Str) {
        std::string s = e.readStr();
        if (name == "add") e.strVar(v.index) += s;
        return;
    }
    int rhs = e.readInt();
    int cur = 0;
    if (v.kind == E::VarRef::Num) cur = e.getNumVar(v.index);
    else if (int *p = e.arrayElement(v.index, v.subs)) cur = *p;
    if (name == "add") cur += rhs;
    else if (name == "sub") cur -= rhs;
    else if (name == "mul") cur *= rhs;
    else if (name == "div") cur = rhs ? cur / rhs : 0;
    else if (name == "mod") cur = rhs ? cur % rhs : 0;
    e.setVar(v, cur);
}

void cmdIncDec(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    int cur = 0;
    if (v.kind == E::VarRef::Num) cur = e.getNumVar(v.index);
    else if (int *p = e.arrayElement(v.index, v.subs)) cur = *p;
    e.setVar(v, cur + (name == "inc" ? 1 : -1));
}

void cmdRnd(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    e.readComma();
    int a = e.readInt();
    if (name == "rnd2") {
        e.readComma();
        int b = e.readInt();
        if (b < a) std::swap(a, b);
        e.setVar(v, a + e.nextRandom(b - a + 1));
    } else {
        e.setVar(v, e.nextRandom(a));
    }
}

void cmdTrig(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    e.readComma();
    double rad = e.readInt() * M_PI / 180.0;
    double r = name == "sin" ? std::sin(rad) : name == "cos" ? std::cos(rad) : std::tan(rad);
    e.setVar(v, int(std::lround(r * 1000)));
}

std::string fullWidthDigits(const std::string &s) {
    std::string out;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            out += char(0x82);
            out += char(0x4F + (c - '0'));
        } else if (c == '-') {
            out += char(0x81);
            out += char(0x7C);
        } else {
            out += c;
        }
    }
    return out;
}

void cmdItoa(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    e.readComma();
    std::string s = std::to_string(e.readInt());
    e.setVar(v, name == "itoa2" ? fullWidthDigits(s) : s);
}

void cmdAtoi(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    e.setVar(v, atoi(e.readStr().c_str()));
}

void cmdLen(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    e.setVar(v, int(e.readStr().size()));
}

void cmdMid(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    std::string s = e.readStr();
    e.readComma();
    int start = e.readInt();
    int len = int(s.size());
    if (e.readComma()) len = e.readInt();
    if (start < 0) start = 0;
    if (size_t(start) >= s.size() || len <= 0) {
        e.setVar(v, std::string());
        return;
    }
    e.setVar(v, s.substr(size_t(start), size_t(len)));
}

void cmdSplit(E &e, const std::string &) {
    std::string s = e.readStr();
    e.readComma();
    std::string sep = e.readStr();
    char d = sep.empty() ? ',' : sep[0];
    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= s.size();) {
        if (i == s.size() || s[i] == d) {
            parts.push_back(s.substr(start, i - start));
            start = ++i;
            continue;
        }
        i += sjisCharLen(s, i);
    }
    size_t k = 0;
    while (e.readComma()) {
        E::VarRef v = e.readVar();
        if (v.kind == E::VarRef::Str) e.setVar(v, k < parts.size() ? parts[k] : std::string());
        else e.setVar(v, k < parts.size() ? atoi(parts[k].c_str()) : 0);
        k++;
    }
}

void cmdCmp(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    std::string a = e.readStr();
    e.readComma();
    std::string b = e.readStr();
    int c = a.compare(b);
    e.setVar(v, c < 0 ? -1 : c > 0 ? 1 : 0);
}

void cmdDateTime(E &e, const std::string &name) {
    time_t now = time(nullptr);
    struct tm lt;
    localtime_r(&now, &lt);
    int vals[3];
    if (name == "date") {
        vals[0] = lt.tm_year + 1900;
        vals[1] = lt.tm_mon + 1;
        vals[2] = lt.tm_mday;
    } else {
        vals[0] = lt.tm_hour;
        vals[1] = lt.tm_min;
        vals[2] = lt.tm_sec;
    }
    for (int i = 0; i < 3; i++) {
        if (i) e.readComma();
        e.setVar(e.readVar(), vals[i]);
    }
}

void cmdGetVersion(E &e, const std::string &) { e.setVar(e.readVar(), 304); }

// getparam reads the arguments a user command was called with, in the caller's context.
void cmdGetParam(E &e, const std::string &) {
    const E::Frame *frame = nullptr;
    for (auto it = e.stack_.rbegin(); it != e.stack_.rend(); ++it)
        if (it->hasParams) {
            frame = &*it;
            break;
        }
    if (!frame) {
        e.errorf("getparam outside a user command");
        e.skipStatement();
        return;
    }
    // Read our own targets first, then evaluate the caller's arguments in order.
    struct Target {
        char mode;  // 0 value, 'i' numeric variable number, 's' string variable number
        E::VarRef v;
    };
    std::vector<Target> targets;
    do {
        e.skipSpaces();
        Target t{0, {}};
        char c = e.peek();
        if ((c == 'i' || c == 's') && e.cur_.pos + 1 < e.script().text().size() && e.script().text()[e.cur_.pos + 1] == '%') {
            t.mode = c;
            e.cur_.pos++;
        }
        t.v = e.readVar();
        targets.push_back(t);
    } while (e.readComma());
    E::Cursor mine = e.cur_;
    e.cur_.pos = frame->paramPos;
    for (size_t i = 0; i < targets.size(); i++) {
        if (i) e.readComma();
        if (e.atStatementEnd()) break;
        Target &t = targets[i];
        if (t.mode) {
            E::VarRef src = e.readVar();
            int number = src.index;  // the reference's own number, not its value
            E::Cursor caller = e.cur_;
            e.cur_ = mine;
            e.setVar(t.v, number);
            mine = e.cur_;
            e.cur_ = caller;
        } else if (t.v.kind == E::VarRef::Str) {
            std::string s = e.readStr();
            E::Cursor caller = e.cur_;
            e.cur_ = mine;
            e.setVar(t.v, s);
            mine = e.cur_;
            e.cur_ = caller;
        } else {
            int n = e.readInt();
            E::Cursor caller = e.cur_;
            e.cur_ = mine;
            e.setVar(t.v, n);
            mine = e.cur_;
            e.cur_ = caller;
        }
    }
    e.cur_ = mine;
}

void cmdGetIni(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    std::string file = e.readStr();
    e.readComma();
    std::string section = sjisLower(e.readStr());
    e.readComma();
    std::string key = sjisLower(e.readStr());
    Bytes data;
    std::string result;
    if (e.fs_.read(file, data)) {
        std::string text(data.begin(), data.end()), cur;
        size_t p = 0;
        while (p < text.size()) {
            size_t nl = text.find('\n', p);
            std::string line = text.substr(p, nl == std::string::npos ? std::string::npos : nl - p);
            p = nl == std::string::npos ? text.size() : nl + 1;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            if (line.size() > 1 && line[0] == '[') {
                cur = sjisLower(line.substr(1, line.find(']') - 1));
            } else if (cur == section) {
                size_t eq = line.find('=');
                if (eq != std::string::npos && sjisLower(line.substr(0, eq)) == key) {
                    result = line.substr(eq + 1);
                    break;
                }
            }
        }
    }
    e.setVar(v, result);
}

void cmdReadFile(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    Bytes data;
    e.fs_.read(e.readStr(), data);
    e.setVar(v, std::string(data.begin(), data.end()));
}

void cmdFileExist(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    e.setVar(v, e.fs_.exists(e.readStr()) ? 1 : 0);
}

void cmdGetReg(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.skipStatement();
    e.setVar(v, std::string());
}

void cmdZero(E &e, const std::string &) {
    // Queries with no meaning here (isfull, getwindowsize's sizes come from the screen) return 0 or the screen.
    E::VarRef v = e.readVar();
    e.setVar(v, 0);
    while (e.readComma()) e.setVar(e.readVar(), 0);
}

void cmdGetWindowSize(E &e, const std::string &) {
    e.setVar(e.readVar(), e.width_);
    e.readComma();
    e.setVar(e.readVar(), e.height_);
}

void cmdGetRet(E &e, const std::string &) { e.setVar(e.readVar(), e.lastRet_); }

// ---- control flow ----

void cmdGoto(E &e, const std::string &) { e.jumpToLabel(e.readLabel()); }

void cmdGosub(E &e, const std::string &) {
    std::string label = e.readLabel();
    e.gosub(e.cur_.pos, e.cur_.line, e.cur_.stmt, label);
}

void cmdReturn(E &e, const std::string &) {
    e.skipSpaces();
    std::string label;
    if (!e.atStatementEnd()) label = e.readLabel();
    // Leave any for loops opened inside the subroutine.
    while (!e.stack_.empty() && e.stack_.back().kind == E::Frame::For) e.stack_.pop_back();
    if (e.stack_.empty()) {
        e.errorf("return without gosub");
        return;
    }
    E::Frame f = e.stack_.back();
    e.stack_.pop_back();
    if (!label.empty()) {
        e.jumpToLabel(label);
        return;
    }
    e.cur_.pos = f.pos;
    e.cur_.line = f.line;
    e.cur_.stmt = f.stmt;
    e.jumps_++;
    if (f.resumeText) e.inTextLine_ = true;
}

void cmdIf(E &e, const std::string &name) {
    bool c = e.readCondition();
    if (name == "notif") c = !c;
    if (c) {
        e.continueLine_ = true;
    } else {
        e.skipToLineEnd();
        e.jumps_++;
    }
}

// Finds the `next` matching the for loop whose body starts at the cursor, and leaves the cursor after it.
static void skipPastNext(E &e) {
    const std::string &t = e.script().text();
    int depth = 0;
    while (e.cur_.pos < t.size()) {
        e.skipSpaces();
        char c = e.peek();
        if (c == '\n') {
            e.cur_.pos++;
            e.cur_.line++;
            e.cur_.stmt = 0;
            continue;
        }
        if (c == ':') {
            e.cur_.pos++;
            e.cur_.stmt++;
            continue;
        }
        if (isalpha(uint8_t(c))) {
            std::string w = e.readWord();
            if (w == "for") depth++;
            else if (w == "next" && depth-- == 0) return;
        }
        e.skipStatement();
        if (e.peek() == ';') e.skipToLineEnd();
    }
}

void cmdFor(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.skipSpaces();
    if (e.peek() == '=') e.cur_.pos++;
    int from = e.readInt();
    if (e.readWord() != "to") e.errorf("for without to");
    int to = e.readInt();
    int step = 1;
    e.skipSpaces();
    size_t save = e.cur_.pos;
    if (e.readWord() == "step") step = e.readInt();
    else e.cur_.pos = save;
    e.setVar(v, from);
    E::Frame f;
    f.kind = E::Frame::For;
    f.var = v.index;
    f.to = to;
    f.step = step;
    f.pos = e.cur_.pos;
    f.line = e.cur_.line;
    f.stmt = e.cur_.stmt;
    if ((step > 0 && from > to) || (step < 0 && from < to)) {
        skipPastNext(e);
        e.jumps_++;
        return;
    }
    e.stack_.push_back(f);
}

void cmdNext(E &e, const std::string &) {
    if (e.stack_.empty() || e.stack_.back().kind != E::Frame::For) {
        e.errorf("next without for");
        return;
    }
    E::Frame &f = e.stack_.back();
    int v = e.getNumVar(f.var) + f.step;
    e.setNumVar(f.var, v);
    if ((f.step > 0 && v <= f.to) || (f.step < 0 && v >= f.to) || f.step == 0) {
        e.cur_.pos = f.pos;
        e.cur_.line = f.line;
        e.cur_.stmt = f.stmt;
        e.jumps_++;
    } else {
        e.stack_.pop_back();
    }
}

void cmdBreak(E &e, const std::string &) {
    e.skipSpaces();
    std::string label;
    if (!e.atStatementEnd()) label = e.readLabel();
    if (!e.stack_.empty() && e.stack_.back().kind == E::Frame::For) {
        E::Frame f = e.stack_.back();
        e.stack_.pop_back();
        if (label.empty()) {
            e.cur_.pos = f.pos;
            e.cur_.line = f.line;
            e.cur_.stmt = f.stmt;
            skipPastNext(e);
            e.jumps_++;
            return;
        }
    }
    if (!label.empty()) e.jumpToLabel(label);
}

void cmdSkip(E &e, const std::string &) {
    int n = e.readInt();
    if (n == 0) return;
    e.jumpToLine(e.cur_.line + n);
}

void cmdJump(E &e, const std::string &name) {
    // jumpf: to the next '~' line; jumpb: back to the previous one.
    const std::string &t = e.script().text();
    int line = e.cur_.line;
    int step = name == "jumpf" ? 1 : -1;
    for (line += step; line >= 0 && line < e.script().lineCount(); line += step) {
        size_t p = e.script().lineStart(line);
        while (t[p] == ' ' || t[p] == '\t') p++;
        if (t[p] == '~') {
            e.jumpToLine(line);
            e.skipToLineEnd();
            return;
        }
    }
}

void cmdTableGoto(E &e, const std::string &) {
    int v = e.readInt();
    std::vector<std::string> labels;
    while (e.readComma()) labels.push_back(e.readLabel());
    if (v >= 0 && size_t(v) < labels.size()) e.jumpToLabel(labels[size_t(v)]);
}

void cmdEnd(E &e, const std::string &) {
    e.saveSystemData();
    e.ended_ = true;
}

void cmdReset(E &e, const std::string &) { e.resetGame(); }

void cmdDefineReset(E &e, const std::string &) { e.resetGame(true); }

void cmdTrap(E &e, const std::string &name) {
    e.skipSpaces();
    std::string w;
    size_t save = e.cur_.pos;
    if (isalpha(uint8_t(e.peek()))) w = e.readWord();
    if (w == "off") {
        e.trapMode_ = 0;
        e.trapPending_ = false;
        return;
    }
    if (w == "stop") {
        e.trapStopped_ = true;
        return;
    }
    if (w == "resume") {
        e.trapStopped_ = false;
        return;
    }
    e.cur_.pos = save;
    e.trapLabel_ = e.readLabel();
    e.trapMode_ = (name == "lr_trap" || name == "lr_trap2") ? 2 : name == "r_trap" ? 3 : 1;
    e.trapStopped_ = false;
    e.trapPending_ = false;
}

// ---- timers ----

void cmdWait(E &e, const std::string &name) {
    int ms = e.readInt();
    e.waitTimer(ms, name == "delay");
}

void cmdResetTimer(E &e, const std::string &) { e.timerBase_ = e.now_; }
void cmdGetTimer(E &e, const std::string &) { e.setVar(e.readVar(), int(e.now_ - e.timerBase_)); }

void cmdWaitTimer(E &e, const std::string &) {
    int ms = e.readInt();
    int64_t remain = e.timerBase_ + ms - e.now_;
    if (remain > 0) e.waitTimer(int(remain), false);
}

void cmdClick(E &e, const std::string &) {
    e.waitClick(nullptr);
}

void cmdLrClick(E &e, const std::string &) {
    e.waitClick(nullptr);
    e.waitRightToo_ = true;
}

void cmdSkipOff(E &e, const std::string &) {
    e.skipMode_ = false;
    e.autoMode_ = false;
}

void cmdIsSkip(E &e, const std::string &) { e.setVar(e.readVar(), e.skipMode_ ? 1 : e.autoMode_ ? 2 : 0); }

void cmdAutoClick(E &e, const std::string &) { e.autoClick_ = e.readInt(); }

}  // namespace

void registerCoreCommands(std::unordered_map<std::string, CommandFn> &t) {
    for (const char *n : {"nsa", "ns2", "ns3", "nsadir"}) t[n] = cmdNsa;
    t["game"] = cmdGame;
    t["caption"] = cmdCaption;
    t["globalon"] = cmdGlobalOn;
    t["filelog"] = cmdFileLog;
    t["labellog"] = cmdLabelLog;
    t["numalias"] = cmdNumAlias;
    t["stralias"] = cmdStrAlias;
    t["defsub"] = cmdDefsub;
    t["savedir"] = cmdSaveDir;
    t["savenumber"] = cmdSaveNumber;
    t["savename"] = cmdSaveName;
    t["intlimit"] = cmdIntLimit;
    t["dim"] = cmdDim;
    t["movl"] = cmdMovl;
    t["textgosub"] = cmdTextGosub;
    t["pretextgosub"] = cmdPreTextGosub;
    t["loadgosub"] = cmdLoadGosub;
    t["maxkaisoupage"] = cmdMaxKaisouPage;
    t["usewheel"] = cmdUseWheel;
    t["useescspc"] = cmdUseEscSpc;
    t["linepage"] = cmdLinePage;
    t["english"] = cmdEnglish;
    t["kidokuskip"] = cmdKidokuSkip;
    t["humanz"] = cmdHumanz;
    t["windowback"] = cmdWindowBack;
    t["underline"] = cmdUnderline;
    t["effectcut"] = cmdEffectCut;
    t["effectskip"] = cmdEffectSkip;
    t["effectblank"] = cmdEffectBlank;
    t["automode_time"] = cmdAutomodeTime;
    t["rmode"] = cmdRmode;
    t["roff"] = cmdRoff;
    t["rlookback"] = cmdRlookback;
    t["rmenu"] = cmdRmenu;
    t["transmode"] = cmdTransMode;
    t["labelexist"] = cmdLabelExist;
    t["setkinsoku"] = cmdSetKinsoku;
    t["addkinsoku"] = cmdSetKinsoku;
    for (const char *n : {"mov", "mov3", "mov4", "mov5", "mov6", "mov7", "mov8", "mov9", "mov10"}) t[n] = cmdMov;
    for (const char *n : {"add", "sub", "mul", "div", "mod"}) t[n] = cmdArith;
    t["inc"] = cmdIncDec;
    t["dec"] = cmdIncDec;
    t["rnd"] = cmdRnd;
    t["rnd2"] = cmdRnd;
    for (const char *n : {"sin", "cos", "tan"}) t[n] = cmdTrig;
    t["itoa"] = cmdItoa;
    t["itoa2"] = cmdItoa;
    t["atoi"] = cmdAtoi;
    t["len"] = cmdLen;
    t["mid"] = cmdMid;
    t["split"] = cmdSplit;
    t["cmp"] = cmdCmp;
    t["date"] = cmdDateTime;
    t["time"] = cmdDateTime;
    t["getversion"] = cmdGetVersion;
    t["getparam"] = cmdGetParam;
    t["getini"] = cmdGetIni;
    t["readfile"] = cmdReadFile;
    t["fileexist"] = cmdFileExist;
    t["getreg"] = cmdGetReg;
    t["isfull"] = cmdZero;
    t["getwindowsize"] = cmdGetWindowSize;
    t["getret"] = cmdGetRet;
    t["goto"] = cmdGoto;
    t["gosub"] = cmdGosub;
    t["return"] = cmdReturn;
    t["if"] = cmdIf;
    t["notif"] = cmdIf;
    t["for"] = cmdFor;
    t["next"] = cmdNext;
    t["break"] = cmdBreak;
    t["skip"] = cmdSkip;
    t["jumpf"] = cmdJump;
    t["jumpb"] = cmdJump;
    t["tablegoto"] = cmdTableGoto;
    t["end"] = cmdEnd;
    t["reset"] = cmdReset;
    t["definereset"] = cmdDefineReset;
    for (const char *n : {"trap", "lr_trap", "r_trap", "trap2", "lr_trap2"}) t[n] = cmdTrap;
    t["wait"] = cmdWait;
    t["delay"] = cmdWait;
    t["resettimer"] = cmdResetTimer;
    t["gettimer"] = cmdGetTimer;
    t["waittimer"] = cmdWaitTimer;
    t["click"] = cmdClick;
    t["lrclick"] = cmdLrClick;
    t["skipoff"] = cmdSkipOff;
    t["isskip"] = cmdIsSkip;
    t["autoclick"] = cmdAutoClick;
    // Windows-only or cosmetic settings.
    for (const char *n : {"deletemenu", "resetmenu", "insertmenu", "killmenu", "versionstr", "mode_saya", "mode_ext",
                          "automode", "menusetwindow", "menuselectcolor", "menuselectvoice", "selectvoice",
                          "clickvoice", "mousecursor", "mousemode", "debuglog", "autosaveoff", "errorsave", "spi",
                          "arc", "soundpressplgin", "cdfadeout", "noteraseman",
                          "clickskippage", "englishdialog", "minimizewindow", "movemousecursor", "menu_full",
                          "menu_window", "menu_click_def", "menu_click_page", "repaint", "filelogoff", "windowchip",
                          "lookbacksp", "lookbackbutton", "lookbackcolor", "lookbackvoice", "lookbackoff",
                          "lookbackflush_dummy", "kidokumode", "useescspc2", "shell", "winexec", "flushout",
                          "rubyoff_dummy"})
        t[n] = ignoreArgs;
}

}  // namespace vnns
