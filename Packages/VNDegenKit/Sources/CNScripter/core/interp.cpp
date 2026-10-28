// Script parsing and control flow: the cursor, expressions, variables, conditions, and the run loop.
#include <cctype>
#include <cstdarg>
#include <cstring>
#include <stdexcept>

#include "engine.h"

namespace vnns {

static bool isIdentStart(char c) { return isalpha(uint8_t(c)) || c == '_'; }
static bool isIdentChar(char c) { return isalnum(uint8_t(c)) || c == '_'; }

void Engine::errorf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    logf("script line %d: %s", cur_.line + 1, buf);
}

char Engine::peek() const {
    const std::string &t = script_.text();
    return cur_.pos < t.size() ? t[cur_.pos] : '\n';
}

void Engine::skipSpaces() {
    const std::string &t = script_.text();
    while (cur_.pos < t.size() && (t[cur_.pos] == ' ' || t[cur_.pos] == '\t')) cur_.pos++;
    // A full-width space also separates arguments in some scripts.
    while (cur_.pos + 1 < t.size() && uint8_t(t[cur_.pos]) == 0x81 && uint8_t(t[cur_.pos + 1]) == 0x40) {
        cur_.pos += 2;
        while (cur_.pos < t.size() && (t[cur_.pos] == ' ' || t[cur_.pos] == '\t')) cur_.pos++;
    }
}

bool Engine::atStatementEnd() const {
    char c = peek();
    return c == '\n' || c == ':' || c == ';';
}

bool Engine::readComma() {
    skipSpaces();
    if (peek() == ',') {
        cur_.pos++;
        skipSpaces();
        return true;
    }
    return false;
}

std::string Engine::readWord() {
    skipSpaces();
    const std::string &t = script_.text();
    size_t start = cur_.pos;
    while (cur_.pos < t.size() && isIdentChar(t[cur_.pos])) cur_.pos++;
    return sjisLower(std::string_view(t).substr(start, cur_.pos - start));
}

void Engine::jumpToLine(int line) {
    if (line < 0 || line >= script_.lineCount()) {
        errorf("jump to line %d is outside the script", line + 1);
        ended_ = true;
        return;
    }
    cur_.pos = script_.lineStart(line);
    jumps_++;
    cur_.line = line;
    cur_.stmt = 0;
    inTextLine_ = false;
}

bool Engine::jumpToLabel(std::string_view label) {
    int line = script_.labelLine(label);
    if (line < 0) {
        errorf("label *%s not found", std::string(label).c_str());
        return false;
    }
    if (labelLogOn_) noteLabelVisited(script_.labelIndex(label));
    // Execution continues on the line after the label; the label line itself isn't run.
    jumpToLine(line);
    return true;
}

// ---- variables ----

int Engine::getNumVar(int index) const {
    if (index < 0 || size_t(index) >= num_.size()) return 0;
    return num_[size_t(index)];
}

void Engine::setNumVar(int index, int value) {
    if (index < 0 || size_t(index) >= num_.size()) {
        errorf("variable %%%d is out of range", index);
        return;
    }
    if (auto it = intLimits_.find(index); it != intLimits_.end()) {
        if (value < it->second.first) value = it->second.first;
        if (value > it->second.second) value = it->second.second;
    }
    num_[size_t(index)] = value;
}

std::string &Engine::strVar(int index) {
    static std::string dummy;
    if (index < 0 || size_t(index) >= str_.size()) {
        errorf("variable $%d is out of range", index);
        dummy.clear();
        return dummy;
    }
    return str_[size_t(index)];
}

int *Engine::arrayElement(int index, const std::vector<int> &subs) {
    auto it = arrays_.find(index);
    if (it == arrays_.end()) return nullptr;
    ArrayVar &a = it->second;
    if (subs.size() != a.dims.size()) return nullptr;
    size_t off = 0;
    for (size_t i = 0; i < subs.size(); i++) {
        if (subs[i] < 0 || subs[i] > a.dims[i]) return nullptr;
        off = off * size_t(a.dims[i] + 1) + size_t(subs[i]);
    }
    return off < a.data.size() ? &a.data[off] : nullptr;
}

// ---- expressions ----

namespace {
// Expressions nest through parentheses, signs and variable indexes. Real scripts nest a few levels; a script nested
// thousands deep would exhaust the stack, so it ends the game instead.
struct NestingGuard {
    static inline thread_local int depth = 0;
    NestingGuard() {
        if (++depth > 256) {
            depth = 0;
            throw std::runtime_error("an expression is nested too deeply");
        }
    }
    ~NestingGuard() { depth = std::max(0, depth - 1); }
};

struct ExprParser {
    Engine &e;
    const std::string &t;
    explicit ExprParser(Engine &eng) : e(eng), t(eng_text(eng)) {}
    static const std::string &eng_text(Engine &eng);
    char peek() const { return e.cur_.pos < t.size() ? t[e.cur_.pos] : '\n'; }
    char peekAt(size_t k) const { return e.cur_.pos + k < t.size() ? t[e.cur_.pos + k] : '\n'; }

    // The index after %, $ or ?: a number, an alias, or another variable.
    int index() {
        NestingGuard guard;
        e.skipSpaces();
        char c = peek();
        if (isdigit(uint8_t(c))) return number();
        if (c == '%') {
            e.cur_.pos++;
            return e.getNumVar(index());
        }
        if (c == '?') return arrayValue();
        if (isIdentStart(c)) {
            std::string w = e.readWord();
            auto it = e.numAlias_.find(w);
            if (it != e.numAlias_.end()) return it->second;
            e.errorf("unknown alias %s", w.c_str());
            return 0;
        }
        e.errorf("bad variable index");
        return 0;
    }
    int number() {
        long v = 0;
        while (isdigit(uint8_t(peek()))) v = v * 10 + (t[e.cur_.pos++] - '0');
        return int(v);
    }
    std::vector<int> subscripts() {
        std::vector<int> subs;
        for (;;) {
            e.skipSpaces();
            if (peek() != '[') break;
            e.cur_.pos++;
            subs.push_back(expr());
            e.skipSpaces();
            if (peek() == ']') e.cur_.pos++;
        }
        return subs;
    }
    int arrayValue() {
        e.cur_.pos++;  // '?'
        int idx = index();
        std::vector<int> subs = subscripts();
        int *p = e.arrayElement(idx, subs);
        return p ? *p : 0;
    }
    int factor() {
        NestingGuard guard;
        e.skipSpaces();
        char c = peek();
        if (c == '-') {
            e.cur_.pos++;
            return -factor();
        }
        if (c == '+') {
            e.cur_.pos++;
            return factor();
        }
        if (c == '(') {
            e.cur_.pos++;
            int v = expr();
            e.skipSpaces();
            if (peek() == ')') e.cur_.pos++;
            return v;
        }
        if (isdigit(uint8_t(c))) return number();
        if (c == '%') {
            e.cur_.pos++;
            return e.getNumVar(index());
        }
        if (c == '?') return arrayValue();
        if (isIdentStart(c)) {
            size_t save = e.cur_.pos;
            std::string w = e.readWord();
            auto it = e.numAlias_.find(w);
            if (it != e.numAlias_.end()) return it->second;
            e.cur_.pos = save;
            e.errorf("expected a number, found %s", w.c_str());
            return 0;
        }
        e.errorf("expected a number");
        return 0;
    }
    int term() {
        int v = factor();
        for (;;) {
            e.skipSpaces();
            char c = peek();
            if (c == '*') {
                e.cur_.pos++;
                v *= factor();
            } else if (c == '/') {
                e.cur_.pos++;
                int d = factor();
                v = d ? v / d : 0;
            } else if ((c == 'm' || c == 'M') && (peekAt(1) | 0x20) == 'o' && (peekAt(2) | 0x20) == 'd' &&
                       !isIdentChar(peekAt(3))) {
                e.cur_.pos += 3;
                int d = factor();
                v = d ? v % d : 0;
            } else {
                return v;
            }
        }
    }
    int expr() {
        int v = term();
        for (;;) {
            e.skipSpaces();
            char c = peek();
            if (c == '+') {
                e.cur_.pos++;
                v += term();
            } else if (c == '-') {
                e.cur_.pos++;
                v -= term();
            } else {
                return v;
            }
        }
    }

    std::string strTerm() {
        NestingGuard guard;
        e.skipSpaces();
        char c = peek();
        if (c == '"') {
            e.cur_.pos++;
            size_t start = e.cur_.pos;
            while (e.cur_.pos < t.size() && t[e.cur_.pos] != '"' && t[e.cur_.pos] != '\n')
                e.cur_.pos += sjisCharLen(t, e.cur_.pos);
            std::string s = t.substr(start, e.cur_.pos - start);
            if (peek() == '"') e.cur_.pos++;
            return s;
        }
        if (c == '$') {
            e.cur_.pos++;
            return e.strVar(index());
        }
        if (c == '*') {
            size_t start = e.cur_.pos++;
            while (isIdentChar(peek())) e.cur_.pos++;
            return t.substr(start, e.cur_.pos - start);
        }
        if (c == '#') {
            size_t start = e.cur_.pos++;
            while (isxdigit(uint8_t(peek()))) e.cur_.pos++;
            return t.substr(start, e.cur_.pos - start);
        }
        if (c == '(') {
            // ("file")"if loaded""otherwise": checks the file log.
            e.cur_.pos++;
            std::string file = str();
            e.skipSpaces();
            if (peek() == ')') e.cur_.pos++;
            std::string yes = strTerm(), no = strTerm();
            return e.fileLog_.count(normalizePath(file)) ? yes : no;
        }
        if (c == '%' || isdigit(uint8_t(c)) || c == '-') {
            // A number where a string is expected becomes its decimal text.
            return std::to_string(expr());
        }
        if (isIdentStart(c)) {
            std::string w = e.readWord();
            auto it = e.strAlias_.find(w);
            if (it != e.strAlias_.end()) return it->second;
            return w;  // bare words such as `bg black`
        }
        return {};
    }
    std::string str() {
        std::string s = strTerm();
        for (;;) {
            e.skipSpaces();
            if (peek() != '+') return s;
            e.cur_.pos++;
            s += strTerm();
        }
    }
};
const std::string &ExprParser::eng_text(Engine &eng) { return eng.script().text(); }
}  // namespace

int Engine::readInt() {
    ExprParser p(*this);
    return p.expr();
}

std::string Engine::readStr() {
    ExprParser p(*this);
    return p.str();
}

std::string Engine::readColorOrStr() { return readStr(); }

bool Engine::peekIsStringArg() {
    skipSpaces();
    char c = peek();
    if (c == '"' || c == '$' || c == '#' || c == '*' || c == '(') return true;
    if (isIdentStart(c)) {
        size_t save = cur_.pos;
        std::string w = readWord();
        cur_.pos = save;
        return !numAlias_.count(w);
    }
    return false;
}

std::string Engine::readLabel() {
    skipSpaces();
    std::string s = readStr();
    if (!s.empty() && s[0] == '*') s.erase(0, 1);
    return s;
}

Engine::VarRef Engine::readVar() {
    skipSpaces();
    VarRef v;
    ExprParser p(*this);
    char c = peek();
    if (c == '%') {
        cur_.pos++;
        v.kind = VarRef::Num;
        v.index = p.index();
    } else if (c == '$') {
        cur_.pos++;
        v.kind = VarRef::Str;
        v.index = p.index();
    } else if (c == '?') {
        cur_.pos++;
        v.kind = VarRef::Array;
        v.index = p.index();
        v.subs = p.subscripts();
    } else {
        errorf("expected a variable");
    }
    return v;
}

void Engine::setVar(const VarRef &v, int value) {
    switch (v.kind) {
    case VarRef::Num: setNumVar(v.index, value); break;
    case VarRef::Str: strVar(v.index) = std::to_string(value); break;
    case VarRef::Array:
        if (int *p = arrayElement(v.index, v.subs)) *p = value;
        break;
    default: break;
    }
}

void Engine::setVar(const VarRef &v, const std::string &value) {
    switch (v.kind) {
    case VarRef::Num: setNumVar(v.index, atoi(value.c_str())); break;
    case VarRef::Str: strVar(v.index) = value; break;
    case VarRef::Array:
        if (int *p = arrayElement(v.index, v.subs)) *p = atoi(value.c_str());
        break;
    default: break;
    }
}

// One comparison: numbers or strings with = == != <> < > <= >=, or fchk / lchk.
static bool readComparison(Engine &e) {
    e.skipSpaces();
    size_t save = e.cur_.pos;
    std::string w = e.readWord();
    if (w == "fchk") {
        std::string f = e.readStr();
        return e.fileLog_.count(normalizePath(f)) != 0;
    }
    if (w == "lchk") {
        std::string l = e.readLabel();
        int idx = e.script().labelIndex(l);
        return idx >= 0 && size_t(idx) < e.labelLog_.size() && e.labelLog_[size_t(idx)];
    }
    e.cur_.pos = save;
    e.skipSpaces();
    char c = e.peek();
    bool isString = c == '"' || c == '$';
    std::string ls, rs;
    int li = 0, ri = 0;
    if (isString) ls = e.readStr();
    else li = e.readInt();
    e.skipSpaces();
    const std::string &t = e.script().text();
    auto at = [&](size_t k) { return e.cur_.pos + k < t.size() ? t[e.cur_.pos + k] : '\n'; };
    std::string op;
    if ((at(0) == '>' || at(0) == '<' || at(0) == '=' || at(0) == '!') && (at(1) == '=' || at(1) == '>'))
        op = std::string{at(0), at(1)};
    else if (at(0) == '<' || at(0) == '>' || at(0) == '=')
        op = std::string{at(0)};
    if (op.empty()) {
        e.errorf("expected a comparison");
        return false;
    }
    e.cur_.pos += op.size();
    int cmp;
    if (isString) {
        rs = e.readStr();
        cmp = ls.compare(rs);
    } else {
        ri = e.readInt();
        cmp = li < ri ? -1 : li > ri ? 1 : 0;
    }
    if (op == "=" || op == "==") return cmp == 0;
    if (op == "!=" || op == "<>") return cmp != 0;
    if (op == "<") return cmp < 0;
    if (op == ">") return cmp > 0;
    if (op == "<=" || op == "=<") return cmp <= 0;
    if (op == ">=" || op == "=>") return cmp >= 0;
    return false;
}

bool Engine::readCondition() {
    bool result = readComparison(*this);
    for (;;) {
        skipSpaces();
        char c = peek();
        if (c == '&') {
            cur_.pos++;
            if (peek() == '&') cur_.pos++;
            bool r = readComparison(*this);
            result = result && r;
        } else if (c == '|') {
            cur_.pos++;
            if (peek() == '|') cur_.pos++;
            bool r = readComparison(*this);
            result = result || r;
        } else {
            return result;
        }
    }
}

void Engine::skipToLineEnd() {
    const std::string &t = script_.text();
    while (cur_.pos < t.size() && t[cur_.pos] != '\n') cur_.pos += sjisCharLen(t, cur_.pos);
}

// Skips the rest of the current statement, respecting quoted strings.
void Engine::skipStatement() {
    const std::string &t = script_.text();
    bool quoted = false;
    while (cur_.pos < t.size()) {
        char c = t[cur_.pos];
        if (c == '\n') return;
        if (c == '"') quoted = !quoted;
        if (!quoted && (c == ':' || c == ';')) return;
        cur_.pos += sjisCharLen(t, cur_.pos);
    }
}

void Engine::gosub(size_t returnPos, int line, int stmt, std::string_view label) {
    Frame f;
    f.kind = Frame::Gosub;
    f.pos = returnPos;
    f.line = line;
    f.stmt = stmt;
    stack_.push_back(f);
    if (!jumpToLabel(label)) stack_.pop_back();
}

// ---- the run loop ----

void Engine::executeStatement() {
    const std::string &t = script_.text();
    if (cur_.pos >= t.size()) {
        ended_ = true;
        return;
    }
    if (inTextLine_) {
        textStep();
        return;
    }
    skipSpaces();
    char c = peek();
    if (c == '\n') {
        // Read-text marks cover the newline of each line passed in sequence.
        if (!defineMode_) markRead(cur_.pos);
        cur_.pos++;
        cur_.line++;
        cur_.stmt = 0;
        return;
    }
    if (c == ';') {
        skipToLineEnd();
        return;
    }
    if (c == ':') {
        cur_.pos++;
        cur_.stmt++;
        return;
    }
    size_t lineStart = script_.lineStart(cur_.line);
    bool atLineStart = true;
    for (size_t p = lineStart; p < cur_.pos; p++)
        if (t[p] != ' ' && t[p] != '\t') atLineStart = false;
    if (atLineStart && (c == '*' || c == '~')) {
        if (!defineMode_) markRead(cur_.pos);
        if (c == '*' && labelLogOn_) {
            // A label passed in sequence counts as visited.
            size_t p = cur_.pos + 1, start = p;
            while (p < t.size() && isIdentChar(t[p])) p++;
            noteLabelVisited(script_.labelIndex(std::string_view(t).substr(start, p - start)));
        }
        skipToLineEnd();
        return;
    }
    if (isIdentStart(c)) {
        size_t save = cur_.pos;
        std::string word = readWord();
        // v123 / dv123 / mv123 voice shortcuts carry their number in the name.
        bool known = commandTable().count(word) || defsubs_.count(word) || (word[0] == '_' && commandTable().count(word.substr(1)));
        if (!known) {
            size_t d = 0;
            while (d < word.size() && isalpha(uint8_t(word[d]))) d++;
            std::string head = word.substr(0, d);
            if ((head == "v" || head == "dv" || head == "mv") && d < word.size()) known = true;
        }
        if (known) {
            size_t start = cur_.pos;
            cur_.pos = save;
            if (!defineMode_) markRead(save);
            noteStatementStart(false);
            cur_.pos = start;
            executeCommand(word);
            return;
        }
        // English mode marks text lines with '>', so an unknown word is a mistake there too.
        cur_.pos = save;
        errorf("unknown command %s", word.c_str());
        skipStatement();
        return;
    }
    if (defineMode_) {
        errorf("text in the define block");
        skipToLineEnd();
        return;
    }
    if (skipMode_ && kidokuSkip_ && !isRead(cur_.pos)) skipMode_ = false;  // skip stops at unread text
    markRead(cur_.pos);
    beginTextLine();
}

void Engine::executeCommand(const std::string &name) {
    std::string n = name;
    bool builtin = false;
    if (n.size() > 1 && n[0] == '_' && commandTable().count(n.substr(1))) {
        n.erase(0, 1);
        builtin = true;
    }
    if (!builtin && defsubs_.count(n)) {
        // A user command: a gosub to the label of that name, whose getparam reads the arguments written here.
        size_t params = cur_.pos;
        skipStatement();
        Frame f;
        f.pos = cur_.pos;
        f.line = cur_.line;
        f.stmt = cur_.stmt;
        f.paramPos = params;
        f.hasParams = true;
        stack_.push_back(f);
        if (!jumpToLabel(n)) stack_.pop_back();
        return;
    }
    auto it = commandTable().find(n);
    CommandFn fn = nullptr;
    if (it != commandTable().end()) {
        fn = it->second;
    } else {
        // v123, dv123, mv123
        size_t d = 0;
        while (d < n.size() && isalpha(uint8_t(n[d]))) d++;
        auto voice = commandTable().find("__voice");
        if (voice != commandTable().end() && d < n.size()) fn = voice->second;
    }
    if (!fn) {
        errorf("unknown command %s", n.c_str());
        skipStatement();
        return;
    }
    int jumps = jumps_;
    continueLine_ = false;
    fn(*this, n);
    if (jumps_ != jumps || continueLine_ || ended_) return;
    skipSpaces();
    if (!atStatementEnd()) {
        errorf("%s: unexpected text after the arguments", n.c_str());
        skipStatement();
    }
}

void Engine::runUntilBlocked() {
    // A script loop without waits would hang the host; cap the work per frame.
    for (int budget = 200000; budget > 0; budget--) {
        if (ended_ || wait_ != WaitKind::None) return;
        executeStatement();
    }
}

}  // namespace vnns
