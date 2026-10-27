// Text lines, the text window, glyph drawing, string sprites, and the text commands.
#include <algorithm>
#include <cctype>
#include <map>
#include <tuple>

#include "engine.h"

namespace vnns {

namespace {
uint32_t parseHexColor(std::string_view s) {
    if (!s.empty() && s[0] == '#') s.remove_prefix(1);
    unsigned v = 0;
    for (size_t i = 0; i < 6 && i < s.size(); i++) {
        char c = s[i];
        v = v << 4 | unsigned(isdigit(uint8_t(c)) ? c - '0' : (tolower(c) - 'a' + 10) & 15);
    }
    return rgba(int(v >> 16 & 255), int(v >> 8 & 255), int(v & 255));
}
bool isHex6(const std::string &t, size_t p) {
    for (size_t i = 0; i < 6; i++)
        if (p + i >= t.size() || !isxdigit(uint8_t(t[p + i]))) return false;
    return true;
}
}  // namespace

int Engine::fullAdvance() const { return win_.fw + win_.px; }
int Engine::halfAdvance() const { return (win_.fw + win_.px) / 2; }

// ---- glyphs ----

bool Engine::glyphFor(uint32_t cp, int w, int h, bool bold, const Glyph *&out) {
    static std::map<std::tuple<uint32_t, int, int, bool>, Glyph> cache;
    auto key = std::make_tuple(cp, w, h, bold);
    if (auto it = cache.find(key); it != cache.end()) {
        out = &it->second;
        return true;
    }
    if (!fonts_) fonts_ = makeRasterizer(fontName_);
    if (!fonts_) return false;
    Glyph g;
    if (!fonts_->render(cp, w, h, bold, g)) return false;
    if (cache.size() > 4096) cache.clear();
    out = &cache.emplace(key, std::move(g)).first->second;
    return true;
}

static void drawCoverage(Image &out, const Glyph &g, int x, int y, uint32_t color, int alpha = 255) {
    for (int gy = 0; gy < g.h; gy++) {
        int yy = y + gy;
        if (yy < 0 || yy >= out.h) continue;
        uint32_t *d = out.row(yy);
        const uint8_t *c = &g.coverage[size_t(gy) * size_t(g.w)];
        for (int gx = 0; gx < g.w; gx++) {
            int xx = x + gx;
            if (xx < 0 || xx >= out.w || !c[gx]) continue;
            uint32_t a = mul255(c[gx], uint32_t(alpha));
            d[xx] = over(d[xx], scalePx(color | 0xFF000000u, a));
        }
    }
}

// Draws Shift-JIS text on one line from (x, y), each character in its cell. Returns nothing; layout is fixed-pitch.
void Engine::drawGlyphs(Image &out, const std::string &sjis, int x, int y, int fw, int fh, int pitchX, uint32_t color,
                        bool bold, bool shadow, int, int, bool) {
    int px = x;
    for (size_t i = 0; i < sjis.size();) {
        size_t n;
        uint32_t cp = sjisDecode(sjis, i, &n);
        i += n;
        bool half = n == 1;
        int cw = half ? fw / 2 : fw;
        const Glyph *g = nullptr;
        if (cp != ' ' && cp != 0x3000 && glyphFor(cp, cw, fh, bold, g)) {
            if (shadow) drawCoverage(out, *g, px + shadeX_, y + shadeY_, rgba(0, 0, 0));
            drawCoverage(out, *g, px, y, color);
        }
        px += half ? (fw + pitchX) / 2 : fw + pitchX;
    }
}

// ":s" string sprites: [/fw,fh,pitch[,mode]][;]#color[#color...]text. Each color is one cell.
ImageRef Engine::renderStringImage(const std::string &spec, int &cells) {
    int fw = spFont_.set ? spFont_.fw : win_.fw, fh = spFont_.set ? spFont_.fh : win_.fh;
    int pitch = spFont_.set ? spFont_.px : win_.px;
    bool bold = spFont_.set ? spFont_.bold : win_.bold, shadow = spFont_.set ? spFont_.shadow : win_.shadow;
    size_t p = 0;
    if (p < spec.size() && spec[p] == '/') {
        p++;
        // fw,fh,pitch[,mode...]: later settings exist in newer engines; the first four are what this draws.
        int vals[4] = {fw, fh, pitch, 0};
        int k = 0;
        while (p < spec.size() && spec[p] != ';' && spec[p] != '#') {
            int v = atoi(spec.c_str() + p);
            if (k < 4) vals[k] = v;
            k++;
            while (p < spec.size() && spec[p] != ',' && spec[p] != ';' && spec[p] != '#') p++;
            if (p < spec.size() && spec[p] == ',') p++;
        }
        fw = vals[0];
        fh = vals[1];
        pitch = vals[2];
        if (vals[3] == 2) shadow = true;  // outlined text, drawn here as shadowed
    }
    if (p < spec.size() && spec[p] == ';') p++;
    std::vector<uint32_t> colors;
    while (p + 6 < spec.size() + 1 && spec[p] == '#' && isHex6(spec, p + 1)) {
        colors.push_back(parseHexColor(spec.substr(p, 7)));
        p += 7;
    }
    if (colors.empty()) colors.push_back(rgba(255, 255, 255));
    std::string text = spec.substr(p);
    int width = 0;
    for (size_t i = 0; i < text.size();) {
        size_t n = sjisCharLen(text, i);
        width += n == 1 ? (fw + pitch) / 2 : fw + pitch;
        i += n;
    }
    width = std::max(1, width);
    int h = fh + (shadow ? shadeY_ : 0);
    cells = int(colors.size());
    Image img(width * cells, std::max(1, h), 0);
    for (int c = 0; c < cells; c++)
        drawGlyphs(img, text, c * width, 0, fw, fh, pitch, colors[size_t(c)], bold, shadow, 0, 0);
    return std::make_shared<const Image>(std::move(img));
}

// ---- the window ----

void Engine::showWindow() {
    // Text output brings the screen up to date, as on the original engine.
    if (sceneDirty_ || !windowShown_) commitScene();
    windowHiddenByEffect_ = false;
    textOff_ = false;  // textoff hides the window until the next text, which brings it back
    if (!windowShown_ && !textOff_) {
        windowShown_ = true;
        sceneDirty_ = true;
        if (windowEffect_.type > 1 && windowEffect_.duration > 0 && !skipMode_) {
            windowShown_ = false;
            composeFrame();
            Image from = frame_;
            windowShown_ = true;
            effect_.from = std::move(from);
            effect_.e = windowEffect_;
            effect_.mask = windowEffect_.mask.empty() ? nullptr : loadFileImage(windowEffect_.mask);
            effect_.start = now_;
            effect_.active = true;
            wait_ = WaitKind::Effect;
            waitSkippable_ = effectSkip_;
            waitUntil_ = now_ + windowEffect_.duration;
        }
    }
}

void Engine::drawWindow(Image &out) {
    if (!windowShown_ || textOff_) return;
    if (win_.imageWindow) {
        if (win_.image.image) blit(out, *win_.image.image, win_.x1, win_.y1, 0, 0, win_.image.cellWidth, win_.image.image->h);
    } else {
        multiplyRect(out, win_.x1, win_.y1, win_.x2 - win_.x1 + 1, win_.y2 - win_.y1 + 1, win_.color);
    }
    if (textHidden_) return;
    drawClickCursor(out);
    for (const TextChar &c : page_) {
        if (!c.gaiji.empty()) {
            if (ImageRef img = loadFileImage(c.gaiji)) blit(out, *img, c.x, c.y, 0, 0, img->w, img->h);
            continue;
        }
        drawGlyphs(out, c.ch, c.x, c.y, c.ruby ? rubyW_ * 2 : win_.fw, c.h, 0, c.color, win_.bold, win_.shadow, 0, 0);
    }
}

// The cursor beside the last character while a built-in click wait runs. Without setcursor the PC engine uses images
// built into its exe, which VNDegen can't ship, so a small triangle stands in.
void Engine::drawClickCursor(Image &out) {
    if (wait_ != WaitKind::Click || !windowShown_ || !inTextLine_) return;
    ClickCursor &c = cursors_[clickCursorPage_ ? 1 : 0];
    int x = c.absolute ? c.dx : win_.tx + penX_ + c.dx;
    int y = c.absolute ? c.dy : win_.ty + penY_ + c.dy;
    if (c.set) {
        if (!c.sprite.img.image) return;
        Sprite s = c.sprite;
        s.x = x;
        s.y = y;
        s.visible = s.loaded = true;
        int cw = s.img.cellWidth;
        // Cursor animation runs off the engine clock.
        int cells = std::max(1, s.img.cells);
        int t = s.img.cellTimes.empty() ? 0 : s.img.cellTimes[0];
        int cell = t > 0 ? int((now_ / t) % cells) : 0;
        blit(out, *s.img.image, x, y, cell * cw, 0, cw, s.img.image->h);
        return;
    }
    int size = std::max(6, win_.fh / 2);
    int top = y + (win_.fh - size) / 2 + int((now_ / 120) % 4);
    for (int row = 0; row < size; row++) {
        int half = (size - row) / 2;
        fillRect(out, x + size / 2 - half, top + row, half * 2, 1, rgba(255, 255, 255));
    }
}

void Engine::pushLog() {
    if (pageText_.empty()) return;
    std::string text = pageText_;
    while (!text.empty() && text.back() == '\n') text.pop_back();
    log_.push_back(text);
    logTags_.push_back(pageTag_);
    while (!log_.empty() && int(log_.size()) > maxLogPages_) {
        log_.erase(log_.begin());
        logTags_.erase(logTags_.begin());
    }
}

void Engine::clearPage() {
    pushLog();
    page_.clear();
    pageText_.clear();
    textButtons_.clear();
    penX_ = 0;
    penY_ = 0;
    sceneDirty_ = true;
}

void Engine::newLine() {
    penX_ = indent_ * fullAdvance();
    penY_ += win_.fh + win_.py;
    pageText_ += '\n';
}

void Engine::putChar(const std::string &ch, bool isRuby) {
    showWindow();
    bool half = ch.size() == 1;
    int adv = half ? halfAdvance() : fullAdvance();
    int lineWidth = win_.cols * fullAdvance();
    if (penX_ + adv > lineWidth && penX_ > indent_ * fullAdvance()) {
        // Line-start kinsoku characters hang past the edge instead of starting a line.
        if (kinsokuStart_.find(ch) == std::string::npos || ch.size() == 1) newLine();
    }
    TextChar c;
    c.ch = ch;
    c.x = win_.tx + penX_;
    c.y = win_.ty + penY_ + (rubyOn_ ? rubyH_ : 0);
    c.color = textColor_;
    c.w = half ? win_.fw / 2 : win_.fw;
    c.h = win_.fh;
    c.ruby = isRuby;
    for (const GaijiRule &g : gaiji_)
        if (g.ch == ch) c.gaiji = g.image;
    page_.push_back(c);
    pageText_ += ch;
    lastCharX_ = c.x;
    lastCharY_ = c.y;
    penX_ += adv;
    sceneDirty_ = true;
}

// One step of a text line: a character, an inline control, or the line's end.
bool Engine::beginTextLine() {
    inTextLine_ = true;
    pretextDone_ = false;
    lineHadText_ = false;
    noNewlineAtEnd_ = false;
    // A page wait's clear happens as the next page starts, before that page's head becomes the savepoint.
    if (pendingPageClear_) {
        clearPage();
        pendingPageClear_ = false;
    }
    noteTextLineStart();
    return true;
}

void Engine::textClickWait(bool page) {
    showWindow();
    composeFrame();
    if (!textGosub_.empty()) {
        isPageWait_ = page;
        Frame f;
        f.kind = Frame::TextGosub;
        f.pos = cur_.pos;
        f.line = cur_.line;
        f.stmt = cur_.stmt;
        f.resumeText = true;
        stack_.push_back(f);
        inTextLine_ = false;
        if (!jumpToLabel(textGosub_)) {
            stack_.pop_back();
            inTextLine_ = true;
        }
        return;
    }
    waitClick([this, page] {
        if (page) {
            clearPage();
            pendingPageClear_ = false;
        }
    });
    clickCursorPage_ = page;
}

void Engine::textStep() {
    const std::string &t = script_.text();
    if (!pendingChars_.empty()) {
        std::string ch = pendingChars_.front();
        pendingChars_.erase(pendingChars_.begin());
        putChar(ch);
        if (!textFast_ && !skipMode_ && !ctrlSkip() && textSpeed_ > 0) {
            wait_ = WaitKind::Text;
            waitUntil_ = now_ + textSpeed_;
            waitSkippable_ = true;
        }
        return;
    }
    size_t &p = cur_.pos;
    // The line's head: a [tag] for pretextgosub, the english '>' marker.
    if (!lineHadText_ && !pretextDone_) {
        pretextDone_ = true;
        if (english_ && t[p] == '>') p++;
        pretextTag_.clear();
        if (t[p] == '[') {
            size_t end = t.find(']', p);
            if (end != std::string::npos && t.find('\n', p) > end) {
                pretextTag_ = t.substr(p + 1, end - p - 1);
                p = end + 1;
            }
        }
        if (page_.empty() && pageText_.empty()) pageTag_ = pretextTag_;
        if (pendingPageClear_) {
            clearPage();
            pendingPageClear_ = false;
        }
        if (!preTextGosub_.empty()) {
            Frame f;
            f.kind = Frame::TextGosub;
            f.pos = p;
            f.line = cur_.line;
            f.stmt = cur_.stmt;
            f.resumeText = true;
            stack_.push_back(f);
            inTextLine_ = false;
            if (!jumpToLabel(preTextGosub_)) {
                stack_.pop_back();
                inTextLine_ = true;
            }
            return;
        }
    }
    char c = p < t.size() ? t[p] : '\n';
    if (c == '\n') {
        inTextLine_ = false;
        if (linePage_ && !noNewlineAtEnd_) {
            // linepage: each text line ends in a page wait.
            noNewlineAtEnd_ = true;
            textClickWait(true);
            if (!textGosub_.empty()) return;
            return;
        }
        if (!noNewlineAtEnd_ && lineHadText_) newLine();
        p++;
        cur_.line++;
        cur_.stmt = 0;
        textFast_ = false;
        return;
    }
    lineHadText_ = true;
    if (c == '@' || c == '\\') {
        p++;
        bool page = c == '\\';
        textFast_ = false;
        if (page) {
            // The newline right after a page wait doesn't start a new line.
            if (p < t.size() && t[p] == '\n') noNewlineAtEnd_ = true;
            pendingPageClear_ = true;
        }
        textClickWait(page);
        return;
    }
    if (c == '/' && p + 1 < t.size() && t[p + 1] == '\n') {
        p++;
        noNewlineAtEnd_ = true;
        return;
    }
    if (c == '!' && p + 1 < t.size()) {
        char k = char(tolower(uint8_t(t[p + 1])));
        if (k == 's' && p + 2 < t.size() && (t[p + 2] == 'd' || t[p + 2] == 'D')) {
            p += 3;
            textSpeed_ = defaultSpeeds_[speedChoice_];
            return;
        }
        if ((k == 's' || k == 'd' || k == 'w') && p + 2 < t.size() && isdigit(uint8_t(t[p + 2]))) {
            p += 2;
            int v = 0;
            while (p < t.size() && isdigit(uint8_t(t[p]))) v = v * 10 + (t[p++] - '0');
            if (k == 's') textSpeed_ = v;
            else if (!skipMode_ && !ctrlSkip()) waitTimer(v, k == 'd');
            return;
        }
    }
    if (c == '#' && isHex6(t, p + 1)) {
        textColor_ = parseHexColor(t.substr(p, 7));
        p += 7;
        return;
    }
    if (c == '_') {
        p++;
        ignoreClickStr_ = true;
        return;
    }
    if (c == '$' || c == '%') {
        VarRef v = readVar();
        std::string s = v.kind == VarRef::Str ? strVar(v.index) : std::to_string(getNumVar(v.index));
        for (size_t i = 0; i < s.size();) {
            size_t n = sjisCharLen(s, i);
            pendingChars_.push_back(s.substr(i, n));
            i += n;
        }
        return;
    }
    if (c == '{') {
        // {var,value,var,value...} assigns as the text is shown.
        p++;
        while (peek() != '}' && peek() != '\n') {
            VarRef v = readVar();
            readComma();
            if (v.kind == VarRef::Str) {
                // The value runs to the next comma or brace, unquoted.
                size_t start = p;
                while (p < t.size() && t[p] != ',' && t[p] != '}' && t[p] != '\n') p += sjisCharLen(t, p);
                setVar(v, t.substr(start, p - start));
            } else {
                setVar(v, readInt());
            }
            readComma();
        }
        if (peek() == '}') p++;
        return;
    }
    if (c == '<' && !textBtnOff_) {
        // <N text> or <text>: a text button.
        size_t end = t.find('>', p);
        if (end != std::string::npos && t.find('\n', p) > end) {
            p++;
            int number = -1;
            if (isdigit(uint8_t(t[p]))) {
                number = 0;
                while (isdigit(uint8_t(t[p]))) number = number * 10 + (t[p++] - '0');
            } else {
                number = textBtnStart_ + int(textButtons_.size());
            }
            std::string text = t.substr(p, end - p);
            p = end + 1;
            textButtons_.emplace_back(number, text);
            uint32_t saved = textColor_;
            textColor_ = linkColor_[0];
            size_t first = page_.size();
            for (size_t i = 0; i < text.size();) {
                size_t n = sjisCharLen(text, i);
                putChar(text.substr(i, n));
                i += n;
            }
            for (size_t i = first; i < page_.size(); i++) page_[i].link = number;
            textColor_ = saved;
            return;
        }
    }
    if (c == '(' && rubyOn_) {
        // (base/ruby)
        size_t slash = t.find('/', p), close = t.find(')', p), nl = t.find('\n', p);
        if (slash < close && close < nl) {
            std::string base = t.substr(p + 1, slash - p - 1), ruby = t.substr(slash + 1, close - slash - 1);
            p = close + 1;
            int startX = penX_;
            for (size_t i = 0; i < base.size();) {
                size_t n = sjisCharLen(base, i);
                putChar(base.substr(i, n));
                i += n;
            }
            // Ruby is centred over the base, in its own smaller cells above the line.
            int baseW = penX_ - startX;
            int rubyCount = 0;
            for (size_t i = 0; i < ruby.size(); i += sjisCharLen(ruby, i)) rubyCount++;
            int rx = win_.tx + startX + (baseW - rubyCount * rubyW_) / 2;
            int ry = win_.ty + penY_;
            for (size_t i = 0; i < ruby.size();) {
                size_t n = sjisCharLen(ruby, i);
                TextChar rc;
                rc.ch = ruby.substr(i, n);
                rc.x = rx;
                rc.y = ry;
                rc.color = textColor_;
                rc.w = rubyW_;
                rc.h = rubyH_;
                rc.ruby = true;
                page_.push_back(rc);
                rx += rubyW_;
                i += n;
            }
            return;
        }
    }
    // An ordinary character.
    size_t n = sjisCharLen(t, p);
    std::string ch = t.substr(p, n);
    p += n;
    if (english_ && cfg_.englishWordWrap && ch == " ") {
        // Word wrap (the option Test Game 98's patched engine needs): break before a word that won't fit.
        size_t q = p;
        int w = 0;
        while (q < t.size() && t[q] != ' ' && t[q] != '\n' && t[q] != '@' && t[q] != '\\') {
            size_t m = sjisCharLen(t, q);
            w += m == 1 ? halfAdvance() : fullAdvance();
            q += m;
        }
        int lineWidth = win_.cols * fullAdvance();
        if (penX_ + halfAdvance() + w > lineWidth && penX_ > 0) {
            newLine();
            return;
        }
    }
    putChar(ch);
    bool clickStrHit = !ignoreClickStr_ && !clickStr_.empty() && n == 2 && clickStr_.find(ch) != std::string::npos;
    ignoreClickStr_ = false;
    if (clickStrHit) {
        char next = p < t.size() ? t[p] : '\n';
        if (next != '@' && next != '\\') {
            int row = penY_ / std::max(1, win_.fh + win_.py);
            bool page = row >= win_.rows - clickStrLines_;
            if (page) pendingPageClear_ = true;
            textFast_ = false;
            textClickWait(page);
            return;
        }
    }
    if (!textFast_ && !skipMode_ && !ctrlSkip() && textSpeed_ > 0) {
        wait_ = WaitKind::Text;
        waitUntil_ = now_ + textSpeed_;
        waitSkippable_ = true;
    }
}

// ---- commands ----

namespace {
using E = Engine;

void cmdSetWindow(E &e, const std::string &name) {
    if (name == "setwindow2") {
        std::string s = e.readStr();
        if (!s.empty() && s[0] == '#') {
            e.win_.imageWindow = false;
            e.win_.color = parseHexColor(s);
        } else {
            e.win_.imageWindow = true;
            e.win_.imageTag = s;
            e.loadImage(s, e.win_.image);
        }
        e.sceneDirty_ = true;
        return;
    }
    E::Window &w = e.win_;
    int v[11];
    for (int i = 0; i < 11; i++) {
        if (i) e.readComma();
        v[i] = e.readInt();
    }
    w.tx = v[0];
    w.ty = v[1];
    w.cols = v[2];
    w.rows = v[3];
    w.fw = v[4];
    w.fh = v[5];
    w.px = v[6];
    w.py = v[7];
    w.speed = v[8];
    w.bold = v[9] != 0;
    w.shadow = v[10] != 0;
    e.readComma();
    std::string s = e.readStr();
    if (!s.empty() && s[0] == '#') {
        w.imageWindow = false;
        w.color = parseHexColor(s);
        e.readComma();
        w.x1 = e.readInt();
        e.readComma();
        w.y1 = e.readInt();
        e.readComma();
        w.x2 = e.readInt();
        e.readComma();
        w.y2 = e.readInt();
    } else {
        w.imageWindow = true;
        w.imageTag = s;
        e.loadImage(s, w.image);
        e.readComma();
        w.x1 = e.readInt();
        e.readComma();
        w.y1 = e.readInt();
        if (w.image.image) {
            w.x2 = w.x1 + w.image.cellWidth - 1;
            w.y2 = w.y1 + w.image.image->h - 1;
        }
    }
    e.textSpeed_ = w.speed;
    if (name != "setwindow3") {
        e.log_.clear();
        e.logTags_.clear();
    }
    e.page_.clear();
    e.pageText_.clear();
    e.penX_ = e.penY_ = 0;
    e.windowShown_ = false;
    e.sceneDirty_ = true;
}

void cmdTextClear(E &e, const std::string &) {
    e.page_.clear();
    e.pageText_.clear();
    e.penX_ = e.penY_ = 0;
    e.sceneDirty_ = true;
}

void cmdTextOnOff(E &e, const std::string &name) {
    if (name == "texton") {
        e.textOff_ = false;
        e.showWindow();
    } else {
        e.textOff_ = true;
        e.windowShown_ = false;
        e.commitScene();
    }
    e.sceneDirty_ = true;
}

void cmdTextSpeed(E &e, const std::string &name) {
    if (name == "textspeeddefault") {
        e.textSpeed_ = e.win_.speed;
        return;
    }
    e.textSpeed_ = e.readInt();
}

void cmdDefaultSpeed(E &e, const std::string &) {
    for (int i = 0; i < 3; i++) {
        if (i) e.readComma();
        e.defaultSpeeds_[i] = e.readInt();
    }
}

void cmdBr(E &e, const std::string &) {
    e.showWindow();
    e.newLine();
    e.sceneDirty_ = true;
}

void cmdLocate(E &e, const std::string &) {
    int x = e.readInt();
    e.readComma();
    int y = e.readInt();
    e.penX_ = x * e.halfAdvance();
    e.penY_ = y * (e.win_.fh + e.win_.py);
}

void cmdPutText(E &e, const std::string &) {
    std::string s = e.readStr();
    bool noBreak = !s.empty() && s.back() == '/';
    if (noBreak) s.pop_back();
    for (size_t i = 0; i < s.size();) {
        size_t n = sjisCharLen(s, i);
        e.putChar(s.substr(i, n));
        i += n;
    }
    if (!noBreak) e.newLine();
    e.sceneDirty_ = true;
}

void cmdClickStr(E &e, const std::string &) {
    e.clickStr_ = e.readStr();
    if (e.readComma()) e.clickStrLines_ = e.readInt();
}

void cmdShadeDistance(E &e, const std::string &) {
    e.shadeX_ = e.readInt();
    e.readComma();
    e.shadeY_ = e.readInt();
}

void cmdSpFont(E &e, const std::string &) {
    e.skipSpaces();
    size_t save = e.cur_.pos;
    if (e.readWord() == "clear") {
        e.spFont_ = E::SpFont();
        return;
    }
    e.cur_.pos = save;
    E::SpFont f;
    f.set = true;
    f.name = e.readStr();
    f.fw = e.win_.fw;
    f.fh = e.win_.fh;
    f.px = e.win_.px;
    f.py = e.win_.py;
    f.bold = e.win_.bold;
    f.shadow = e.win_.shadow;
    int *vals[4] = {&f.fw, &f.fh, &f.px, &f.py};
    for (int i = 0; i < 4 && e.readComma(); i++) *vals[i] = e.readInt();
    if (e.readComma()) f.bold = e.readInt() != 0;
    if (e.readComma()) f.shadow = e.readInt() != 0;
    e.spFont_ = f;
}

void cmdFont(E &e, const std::string &) {
    e.fontName_ = e.readStr();
    e.fonts_.reset();
}

void cmdTextColor(E &e, const std::string &) {
    e.textColor_ = parseHexColor(e.readStr());
    e.defaultTextColor_ = e.textColor_;
}

void cmdRubyOn(E &e, const std::string &name) {
    e.rubyOn_ = true;
    e.rubyW_ = e.win_.fw / 2;
    e.rubyH_ = e.win_.fh / 2;
    e.skipSpaces();
    if (!e.atStatementEnd()) {
        e.rubyW_ = e.readInt();
        e.readComma();
        e.rubyH_ = e.readInt();
            if (e.readComma()) e.rubyFont_ = e.readStr();
    }
    if (name == "rubyon2") e.rubyOn_ = true;
}

void cmdRubyOff(E &e, const std::string &) { e.rubyOn_ = false; }
void cmdIndent(E &e, const std::string &) { e.indent_ = e.readInt(); }
void cmdGetText(E &e, const std::string &) { e.setVar(e.readVar(), e.pageText_); }

void cmdGetCursorPos(E &e, const std::string &name) {
    E::VarRef x = e.readVar();
    e.readComma();
    E::VarRef y = e.readVar();
    if (name == "getcursorpos2") {
        e.setVar(x, e.lastCharX_);
        e.setVar(y, e.lastCharY_);
    } else {
        e.setVar(x, e.win_.tx + e.penX_);
        e.setVar(y, e.win_.ty + e.penY_);
    }
}

void cmdGetNextLine(E &e, const std::string &) {
    E::VarRef x = e.readVar();
    e.readComma();
    E::VarRef y = e.readVar();
    e.setVar(x, e.win_.tx + e.indent_ * e.fullAdvance());
    e.setVar(y, e.win_.ty + e.penY_ + e.win_.fh + e.win_.py);
}

void cmdTexec(E &e, const std::string &name) {
    // Completes the wait textgosub was called for: a page wait clears the window (texec2 leaves it to the next line).
    if (e.isPageWait_) {
        if (name == "texec") {
            e.clearPage();
            e.pendingPageClear_ = false;
        } else {
            e.pendingPageClear_ = true;
        }
    }
    e.textFast_ = false;
}

void cmdIsPage(E &e, const std::string &) { e.setVar(e.readVar(), e.isPageWait_ ? 1 : 0); }

void cmdGetTag(E &e, const std::string &) {
    std::vector<std::string> parts;
    size_t start = 0;
    const std::string &tag = e.pretextTag_;
    for (size_t i = 0; i <= tag.size();) {
        if (i == tag.size() || tag[i] == '/') {
            parts.push_back(tag.substr(start, i - start));
            start = ++i;
            continue;
        }
        i += sjisCharLen(tag, i);
    }
    size_t k = 0;
    do {
        E::VarRef v = e.readVar();
        if (v.kind == E::VarRef::Str) e.setVar(v, k < parts.size() ? parts[k] : std::string());
        else e.setVar(v, k < parts.size() ? atoi(parts[k].c_str()) : 0);
        k++;
    } while (e.readComma());
}

void cmdTextHide(E &e, const std::string &name) {
    e.textHidden_ = name == "texthide";
    e.sceneDirty_ = true;
}

void cmdLookbackFlush(E &e, const std::string &) {
    e.log_.clear();
    e.logTags_.clear();
}

void cmdGetLog(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    int back = e.readInt();
    std::string s;
    if (back == 0) s = e.pageText_;
    else if (back > 0 && size_t(back) <= e.log_.size()) s = e.log_[e.log_.size() - size_t(back)];
    e.setVar(v, s);
}

void cmdCheckPage(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    int back = e.readInt();
    e.setVar(v, back >= 0 && size_t(back) <= e.log_.size() ? 1 : 0);
}

// Multi-line string sprites: strsp n,text,x,y,cols,rows,fw,fh,px,py,bold,shadow[,color...]; '\' breaks a line.
void cmdStrSp(E &e, const std::string &name) {
    int n = e.readInt();
    e.readComma();
    std::string text = e.readStr();
    int v[10];
    for (int i = 0; i < 10; i++) {
        e.readComma();
        v[i] = e.readInt();
    }
    std::vector<uint32_t> colors;
    while (e.readComma()) colors.push_back(parseHexColor(e.readStr()));
    if (colors.empty()) colors.push_back(rgba(255, 255, 255));
    int x = v[0], y = v[1], cols = v[2], rows = v[3], fw = v[4], fh = v[5], px = v[6], py = v[7];
    bool bold = v[8] != 0, shadow = v[9] != 0;
    int cellW = cols * (fw + px), cellH = rows * (fh + py);
    std::vector<std::string> lines(1);
    int col = 0;
    for (size_t i = 0; i < text.size();) {
        size_t k = sjisCharLen(text, i);
        std::string ch = text.substr(i, k);
        i += k;
        if (ch == "\\") {
            lines.emplace_back();
            col = 0;
            continue;
        }
        int adv = k == 1 ? 1 : 2;
        if (col + adv > cols * 2) {
            lines.emplace_back();
            col = 0;
        }
        lines.back() += ch;
        col += adv;
    }
    Image img(cellW * int(colors.size()), std::max(1, cellH), 0);
    for (size_t c = 0; c < colors.size(); c++)
        for (size_t l = 0; l < lines.size() && int(l) < rows; l++)
            e.drawGlyphs(img, lines[l], int(c) * cellW, int(l) * (fh + py), fw, fh, px, colors[c], bold, shadow, 0, 0);
    Sprite *s = e.spriteRef(n);
    if (!s) return;
    *s = Sprite();
    // strsp sprites save with no tag and alpha 255, as on the PC, so they don't come back on load.
    s->alpha = 255;
    s->img.image = std::make_shared<const Image>(std::move(img));
    s->img.cells = int(colors.size());
    s->img.cellWidth = cellW;
    s->img.loop = 3;
    s->loaded = true;
    s->visible = name == "strsp";
    s->x = x;
    s->y = y;
}

void cmdLinkColor(E &e, const std::string &) {
    e.linkColor_[0] = parseHexColor(e.readStr());
    e.readComma();
    e.linkColor_[1] = parseHexColor(e.readStr());
}

void cmdTextBtnStart(E &e, const std::string &) { e.textBtnStart_ = e.readInt(); }

void cmdGetTextBtnStr(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    int n = e.readInt();
    std::string s;
    for (auto &[num, text] : e.textButtons_)
        if (num == n) s = text;
    e.setVar(v, s);
}

void cmdEraseTextBtn(E &e, const std::string &) {
    for (TextChar &c : e.page_)
        if (c.link >= 0) c.color = e.linkColor_[0];
    e.sceneDirty_ = true;
}

void cmdTextBtnOff(E &e, const std::string &) { e.textBtnOff_ = true; }

void cmdSetCursor(E &e, const std::string &name) {
    int n = e.readInt();
    e.readComma();
    std::string tag = e.readStr();
    e.readComma();
    int dx = e.readInt();
    e.readComma();
    int dy = e.readInt();
    if (n < 0 || n > 1) return;
    E::ClickCursor &c = e.cursors_[n];
    c = E::ClickCursor();
    c.set = true;
    c.dx = dx;
    c.dy = dy;
    c.absolute = name == "abssetcursor";
    if (e.loadImage(tag, c.sprite.img)) c.sprite.loaded = true;
}

}  // namespace

void registerTextCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["setwindow"] = cmdSetWindow;
    t["setwindow2"] = cmdSetWindow;
    t["setwindow3"] = cmdSetWindow;
    t["textclear"] = cmdTextClear;
    t["texton"] = cmdTextOnOff;
    t["textoff"] = cmdTextOnOff;
    t["textspeed"] = cmdTextSpeed;
    t["textspeeddefault"] = cmdTextSpeed;
    t["defaultspeed"] = cmdDefaultSpeed;
    t["br"] = cmdBr;
    t["locate"] = cmdLocate;
    t["puttext"] = cmdPutText;
    t["clickstr"] = cmdClickStr;
    t["shadedistance"] = cmdShadeDistance;
    t["spfont"] = cmdSpFont;
    t["font"] = cmdFont;
    t["defaultfont"] = cmdFont;
    t["textcolor"] = cmdTextColor;
    t["rubyon"] = cmdRubyOn;
    t["rubyon2"] = cmdRubyOn;
    t["rubyoff"] = cmdRubyOff;
    t["indent"] = cmdIndent;
    t["gettext"] = cmdGetText;
    t["getcursorpos"] = cmdGetCursorPos;
    t["getcursorpos2"] = cmdGetCursorPos;
    t["getnextline"] = cmdGetNextLine;
    t["texec"] = cmdTexec;
    t["texec2"] = cmdTexec;
    t["ispage"] = cmdIsPage;
    t["gettag"] = cmdGetTag;
    t["texthide"] = cmdTextHide;
    t["textshow"] = cmdTextHide;
    t["lookbackflush"] = cmdLookbackFlush;
    t["getlog"] = cmdGetLog;
    t["checkpage"] = cmdCheckPage;
    t["strsp"] = cmdStrSp;
    t["strsph"] = cmdStrSp;
    t["linkcolor"] = cmdLinkColor;
    t["textbtnstart"] = cmdTextBtnStart;
    t["gettextbtnstr"] = cmdGetTextBtnStr;
    t["erasetextbtn"] = cmdEraseTextBtn;
    t["textbtnoff"] = cmdTextBtnOff;
    t["setcursor"] = cmdSetCursor;
    t["abssetcursor"] = cmdSetCursor;
}

}  // namespace vnns
