// Buttons and choices: btn, spbtn, exbtn, the btnwait family, text buttons, csel and select.
#include <algorithm>

#include "engine.h"

namespace vnns {

int Engine::hitButton(int x, int y) {
    for (size_t i = 0; i < buttons_.size(); i++) {
        const Button &b = buttons_[i];
        if (b.kind == Button::Sprite) {
            if (b.sprite < 0 || b.sprite >= kSpriteCount) continue;
            const Sprite &s = shown_.sp[size_t(b.sprite)];
            // bsp buttons often sit on hidden sprites that show themselves on hover, so only spbtn needs visibility.
            if (!s.loaded || (!s.visible && !b.bsp) || !s.img.image) continue;
            int w = s.img.cellWidth, h = s.img.image->h;
            if (x < s.x || y < s.y || x >= s.x + w || y >= s.y + h) continue;
            if (transBtn_) {
                uint32_t p = s.img.image->row(y - s.y)[s.cell * w + (x - s.x)];
                if (chA(p) == 0) continue;
            }
            return int(i);
        }
        if (x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h) return int(i);
    }
    // Text buttons in the window.
    for (const TextChar &c : page_)
        if (c.link >= 0 && x >= c.x && y >= c.y && x < c.x + (c.ch.size() == 1 ? halfAdvance() : fullAdvance()) &&
            y < c.y + c.h)
            for (size_t i = 0; i < buttons_.size(); i++)
                if (buttons_[i].kind == Button::TextLink && buttons_[i].number == c.link) return int(i);
    return -1;
}

void Engine::updateHover() {
    int hit = keyboardButton_ >= 0 ? keyboardButton_ : hitButton(mouseX_, mouseY_);
    if (hit == hoverButton_) return;
    hoverButton_ = hit;
    if (bexec_) {
        applyBspHover(hit);
        return;
    }
    // Sprite buttons show cell 1 while hovered and cell 0 otherwise; exbtn runs its control string.
    for (size_t i = 0; i < buttons_.size(); i++) {
        const Button &b = buttons_[i];
        if (b.kind != Button::Sprite || b.sprite < 0 || b.sprite >= kSpriteCount) continue;
        int cell = int(i) == hit ? 1 : 0;
        for (Scene *s : {&scene_, &shown_}) {
            Sprite &sp = s->sp[size_t(b.sprite)];
            if (sp.img.cells > 1) sp.cell = std::min(cell, sp.img.cells - 1);
        }
    }
    if (hit >= 0 && !buttons_[size_t(hit)].hoverCtrl.empty()) applySpStr(buttons_[size_t(hit)].hoverCtrl);
    else if (hit < 0 && !btnDefault_.empty()) applySpStr(btnDefault_);
    if (!btnDefault_.empty() || (hit >= 0 && !buttons_[size_t(hit)].hoverCtrl.empty())) commitScene();
    for (TextChar &c : page_)
        if (c.link >= 0) c.color = hit >= 0 && buttons_[size_t(hit)].number == c.link ? linkColor_[1] : linkColor_[0];
    sceneDirty_ = true;
}

// Region buttons from btndef's image show their hover picture over the screen.
void Engine::drawOverlays(Image &out) {
    if (wait_ == WaitKind::Button && hoverButton_ >= 0 && hoverButton_ < int(buttons_.size())) {
        const Button &b = buttons_[size_t(hoverButton_)];
        if (b.kind == Button::Region && btnImage_.image) blit(out, *btnImage_.image, b.x, b.y, b.ox, b.oy, b.w, b.h);
    }
    drawSystemUi(out);
}

void Engine::startButtonWait(const VarRef &result, bool clearAfter, bool textButtons) {
    if (textButtons && !textBtnOff_)
        for (auto &[num, text] : textButtons_) {
            Button b;
            b.number = num;
            b.kind = Button::TextLink;
            buttons_.push_back(b);
        }
    textBtnOff_ = false;
    if (!textButtons && !btnNoWindowErase_ && windowShown_) {
        windowShown_ = false;
        windowHiddenByEffect_ = true;
    }
    commitScene();
    btnResult_ = result;
    btnClearAfter_ = clearAfter;
    btnStart_ = now_;
    hoverButton_ = -2;
    updateHover();
    wait_ = WaitKind::Button;
    composeFrame();
}

// Called each frame during a button wait. Returns true once the wait has a result.
bool Engine::pollButtonWait() {
    if (bexec_) return pollBexec();
    updateHover();
    int result = 0;
    bool done = false;
    auto has = [&](const char *k) { return btnKeys_.count(k) != 0; };
    if (in_.click) {
        result = hoverButton_ >= 0 ? buttons_[size_t(hoverButton_)].number : 0;
        lastClickX_ = mouseX_;
        lastClickY_ = mouseY_;
        done = true;
    } else if (in_.rclick) {
        result = -1;
        done = true;
    } else if (in_.mclick && has("mclick")) {
        result = -70;
        done = true;
    } else if (in_.wheel > 0 && useWheel_) {
        result = -2;
        done = true;
    } else if (in_.wheel < 0 && useWheel_) {
        result = -3;
        done = true;
    }
    for (int k : in_.keys) {
        if (done) break;
        int r = 0;
        if (k == VNNS_KEY_ESCAPE && useEscSpc_) r = -10;
        else if (k == VNNS_KEY_SPACE && useEscSpc_) r = -11;
        else if (k == VNNS_KEY_SPACE && has("spclclk")) r = hoverButton_ >= 0 ? buttons_[size_t(hoverButton_)].number : 0;
        else if (k == VNNS_KEY_PAGEUP && (has("page") || has("pageup"))) r = -12;
        else if (k == VNNS_KEY_PAGEDOWN && has("page")) r = -13;
        else if (k == VNNS_KEY_RETURN && has("enter")) r = -19;
        else if (k == VNNS_KEY_TAB && has("tab")) r = -20;
        else if (k >= VNNS_KEY_F1 && k <= VNNS_KEY_F1 + 11 && has("function")) r = -21 - (k - VNNS_KEY_F1);
        else if (has("cursor") && k == VNNS_KEY_UP) r = -40;
        else if (has("cursor") && k == VNNS_KEY_RIGHT) r = -41;
        else if (has("cursor") && k == VNNS_KEY_DOWN) r = -42;
        else if (has("cursor") && k == VNNS_KEY_LEFT) r = -43;
        else if (has("insert") && k == VNNS_KEY_INSERT) r = -50;
        else if (has("zxc") && k == 'Z') r = -51;
        else if (has("zxc") && k == 'X') r = -52;
        else if (has("zxc") && k == 'C') r = -53;
        else if (k == VNNS_KEY_RETURN || k == VNNS_KEY_SPACE) r = hoverButton_ >= 0 ? buttons_[size_t(hoverButton_)].number : 0;
        if (r != 0 || k == VNNS_KEY_RETURN || k == VNNS_KEY_SPACE) {
            result = r;
            done = true;
        }
    }
    if (!done && has("skipoff") && (skipMode_ || ctrlSkip())) {
        skipMode_ = false;
        result = -60;
        done = true;
    }
    if (!done && btnTimeout_ > 0 && now_ - btnStart_ >= btnTimeout_) {
        if (!btnTimeoutWaitsVoice_ || !audio_.playing(0)) {
            result = useWheel_ ? -5 : -2;
            done = true;
        }
    }
    if (!done) return false;
    lastBtnTime_ = int(now_ - btnStart_);
    btnTimeout_ = 0;
    btnKeys_.clear();
    setVar(btnResult_, result);
    if (btnClearAfter_ && result > 0) {
        buttons_.clear();
        btnDefault_.clear();
    }
    wait_ = WaitKind::None;
    hoverButton_ = -1;
    sceneDirty_ = true;
    return true;
}

namespace {
using E = Engine;

void cmdBtnDef(E &e, const std::string &) {
    e.skipSpaces();
    size_t save = e.cur_.pos;
    std::string w = e.readWord();
    e.buttons_.clear();
    e.btnDefault_.clear();
    e.btnKeys_.clear();
    e.transBtn_ = false;
    e.btnTimeout_ = 0;
    if (w == "clear") return;
    e.cur_.pos = save;
    std::string tag = e.readStr();
    e.btnImage_ = LoadedImage();
    if (!tag.empty()) e.loadImage(tag, e.btnImage_);
}

void cmdBtn(E &e, const std::string &) {
    Button b;
    b.kind = Button::Region;
    int v[7];
    for (int i = 0; i < 7; i++) {
        if (i) e.readComma();
        v[i] = e.readInt();
    }
    b.number = v[0];
    b.x = v[1];
    b.y = v[2];
    b.w = v[3];
    b.h = v[4];
    b.ox = v[5];
    b.oy = v[6];
    e.buttons_.push_back(b);
}

void cmdSpBtn(E &e, const std::string &name) {
    int sp = e.readInt();
    e.readComma();
    int num = e.readInt();
    std::string ctrl;
    if (name.find("exbtn") != std::string::npos) {
        e.readComma();
        ctrl = e.readStr();
    }
    if (name.rfind("cellcheck", 0) == 0) {
        Sprite *s = e.spriteRef(sp);
        if (!s || !s->loaded || s->img.cells < 2) return;
    }
    Button b;
    b.kind = Button::Sprite;
    b.sprite = sp;
    b.number = num;
    b.hoverCtrl = ctrl;
    e.buttons_.push_back(b);
}

void cmdExBtnD(E &e, const std::string &) { e.btnDefault_ = e.readStr(); }

void cmdTextExBtn(E &e, const std::string &) {
    int num = e.readInt();
    e.readComma();
    std::string ctrl = e.readStr();
    e.textBtnCtrl_[num] = ctrl;
}

void cmdBtnWait(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    bool text = name == "textbtnwait";
    if (text)
        for (auto &[num, ctrl] : e.textBtnCtrl_)
            for (Button &b : e.buttons_)
                if (b.number == num) b.hoverCtrl = ctrl;
    e.startButtonWait(v, name == "btnwait", text);
    if (text)
        for (Button &b : e.buttons_)
            if (b.kind == Button::TextLink)
                if (auto it = e.textBtnCtrl_.find(b.number); it != e.textBtnCtrl_.end()) b.hoverCtrl = it->second;
}

void cmdBtnTime(E &e, const std::string &name) {
    e.btnTimeout_ = e.readInt();
    e.btnTimeoutWaitsVoice_ = name == "btntime2";
}

void cmdGetBtnTimer(E &e, const std::string &) { e.setVar(e.readVar(), e.lastBtnTime_); }

void cmdBtnKey(E &e, const std::string &name) {
    std::string key = name.substr(3);  // getcursor -> cursor, getpageup -> pageup
    if (name == "spclclk") key = "spclclk";
    if (name == "transbtn") {
        e.transBtn_ = true;
        return;
    }
    if (name == "btndown") {
        // btndown [1|0]: button waits return as the button goes down rather than up.
        e.skipSpaces();
        e.btnDown_ = e.atStatementEnd() || e.readInt() != 0;
        return;
    }
    e.btnKeys_.insert(key);
}

void cmdBtnNoWindowErase(E &e, const std::string &) { e.btnNoWindowErase_ = true; }

void cmdBtnArea(E &e, const std::string &) { e.btnArea_ = e.readInt(); }

void cmdGetMousePos(E &e, const std::string &) {
    e.setVar(e.readVar(), e.mouseX_);
    e.readComma();
    e.setVar(e.readVar(), e.mouseY_);
}

void cmdClickPos(E &e, const std::string &) {
    e.setVar(e.readVar(), e.lastClickX_);
    e.readComma();
    e.setVar(e.readVar(), e.lastClickY_);
}

int keyFromName(const std::string &k) {
    std::string u;
    for (char c : k) u += char(toupper(uint8_t(c)));
    if (u.size() == 1) return uint8_t(u[0]);
    if (u == "SPACE") return VNNS_KEY_SPACE;
    if (u == "RETURN" || u == "ENTER") return VNNS_KEY_RETURN;
    if (u == "CTRL") return VNNS_KEY_CTRL;
    if (u == "SHIFT") return VNNS_KEY_SHIFT;
    if (u == "UP") return VNNS_KEY_UP;
    if (u == "DOWN") return VNNS_KEY_DOWN;
    if (u == "LEFT") return VNNS_KEY_LEFT;
    if (u == "RIGHT") return VNNS_KEY_RIGHT;
    if (u == "PAGEUP") return VNNS_KEY_PAGEUP;
    if (u == "PAGEDOWN") return VNNS_KEY_PAGEDOWN;
    if (u == "ESC" || u == "ESCAPE") return VNNS_KEY_ESCAPE;
    if (u.size() >= 2 && u[0] == 'F') return VNNS_KEY_F1 + atoi(u.c_str() + 1) - 1;
    return -1;
}

void cmdCheckKey(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    int k = keyFromName(e.readStr());
    e.setVar(v, k >= 0 && e.keysDown_.count(k) ? 1 : 0);
}

void cmdIsDown(E &e, const std::string &) { e.setVar(e.readVar(), e.mouseDown_ ? 1 : 0); }

// ---- bclear / bsp / bdef / btime / bexec ----

void cmdBclear(E &e, const std::string &) {
    e.buttons_.clear();
    e.btnDefault_.clear();
    e.bCursor_ = e.bDown_ = e.bTrans_ = false;
}

void cmdBsp(E &e, const std::string &) {
    Button b;
    b.kind = Button::Sprite;
    b.bsp = true;
    b.sprite = e.readInt();
    b.number = b.sprite;
    if (e.readComma()) b.offCtrl = e.readStr();
    if (e.readComma()) b.onCtrl = e.readStr();
    if (e.readComma()) b.pressCtrl = e.readStr();
    e.buttons_.push_back(b);
}

void cmdBdef(E &e, const std::string &) { e.btnDefault_ = e.readStr(); }

void cmdBtime(E &e, const std::string &) {
    e.btnTimeout_ = e.readInt();
    e.btnTimeoutWaitsVoice_ = e.readComma() && e.readInt() != 0;
    e.bTimeStart_ = -1;
}

void cmdBflag(E &e, const std::string &name) {
    if (name == "bcursor") e.bCursor_ = true;
    else if (name == "bdown") e.bDown_ = true;
    else e.bTrans_ = true;
}

void cmdBexec(E &e, const std::string &) {
    E::VarRef str, num;
    E::VarRef first = e.readVar();
    if (first.kind == E::VarRef::Str) {
        str = first;
        if (e.readComma()) num = e.readVar();
    } else {
        num = first;
    }
    e.btnResult_ = str;
    e.bexecNum_ = num;
    e.bexec_ = true;
    e.keyboardButton_ = -1;
    if (e.bTimeStart_ < 0) e.bTimeStart_ = e.now_;
    e.transBtn_ = e.bTrans_;
    e.commitScene();
    e.hoverButton_ = -2;
    e.updateHover();
    e.wait_ = WaitKind::Button;
    e.composeFrame();
}

// ---- choices ----

void readChoices(E &e, bool withLabels, std::vector<std::pair<std::string, std::string>> &items) {
    items.clear();
    // select's arguments may continue on following lines after a trailing comma.
    for (;;) {
        e.skipSpaces();
        while (e.peek() == '\n') {
            e.cur_.pos++;
            e.cur_.line++;
            e.skipSpaces();
        }
        std::string text = e.readStr();
        std::string label;
        if (withLabels) {
            e.readComma();
            label = e.readLabel();
        }
        items.emplace_back(text, label);
        if (!e.readComma()) break;
    }
}

void cmdCsel(E &e, const std::string &) {
    readChoices(e, true, e.cselItems_);
    if (e.script().labelLine("customsel") >= 0) {
        e.gosub(e.cur_.pos, e.cur_.line, e.cur_.stmt, "customsel");
        // customsel ends with cselgoto, never a return, so the frame is dropped there.
        return;
    }
    e.startSelect(E::SelectMode::Goto);
}

void cmdSelect(E &e, const std::string &name) {
    E::SelectMode mode = name == "selgosub" ? E::SelectMode::Gosub : E::SelectMode::Goto;
    if (name == "selnum") {
        E::VarRef v = e.readVar();
        e.readComma();
        e.selectVar_ = v;
        readChoices(e, false, e.cselItems_);
        mode = E::SelectMode::Number;
    } else {
        readChoices(e, true, e.cselItems_);
    }
    e.startSelect(mode);
}

void cmdGetCselNum(E &e, const std::string &) { e.setVar(e.readVar(), int(e.cselItems_.size())); }

void cmdGetCselStr(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    int n = e.readInt();
    e.setVar(v, n >= 0 && size_t(n) < e.cselItems_.size() ? e.cselItems_[size_t(n)].first : std::string());
}

void cmdCselGoto(E &e, const std::string &) {
    int n = e.readInt();
    // Leave the customsel subroutine and remove its choice images.
    if (!e.stack_.empty()) e.stack_.pop_back();
    for (int slot : e.cselSprites_) e.scene_.sp[size_t(slot)] = Sprite();
    e.cselSprites_.clear();
    if (n >= 0 && size_t(n) < e.cselItems_.size()) e.jumpToLabel(e.cselItems_[size_t(n)].second);
}

// cselbtn choice,button,x,y: the choice's text as a button at (x, y), in the select colors.
void cmdCselBtn(E &e, const std::string &) {
    int choice = e.readInt();
    e.readComma();
    int num = e.readInt();
    e.readComma();
    int x = e.readInt();
    e.readComma();
    int y = e.readInt();
    if (choice < 0 || size_t(choice) >= e.cselItems_.size()) return;
    std::string spec = "#" + e.selectColorHex_[0] + "#" + e.selectColorHex_[1] + e.cselItems_[size_t(choice)].first;
    // A free sprite slot from the top of the range holds the choice's image.
    int slot = kSpriteCount - 1 - choice;
    Sprite *s = e.spriteRef(slot);
    if (!s) return;
    *s = Sprite();
    int cells = 1;
    s->img.image = e.renderStringImage("/" + std::to_string(e.win_.fw) + "," + std::to_string(e.win_.fh) + "," +
                                           std::to_string(e.win_.px) + ";" + spec,
                                       cells);
    s->img.cells = cells;
    s->img.cellWidth = s->img.image ? s->img.image->w / std::max(1, cells) : 0;
    s->img.loop = 3;
    s->loaded = s->visible = s->img.image != nullptr;
    s->x = x;
    s->y = y;
    s->cell = 1;  // not hovered: the second color
    Button b;
    b.kind = Button::Sprite;
    b.sprite = slot;
    b.number = num;
    e.buttons_.push_back(b);
    e.cselSprites_.push_back(slot);
}

void cmdSelectColor(E &e, const std::string &) {
    for (int i = 0; i < 2; i++) {
        if (i) e.readComma();
        std::string c = e.readStr();
        e.selectColorHex_[i] = c.size() > 1 ? c.substr(1) : "FFFFFF";
    }
}

}  // namespace

// bsp buttons: hovered shows its onCtrl (or cell 1), the others their offCtrl (or cell 0); none hovered runs bdef.
void Engine::applyBspHover(int hit) {
    for (size_t i = 0; i < buttons_.size(); i++) {
        const Button &b = buttons_[i];
        if (!b.bsp || b.sprite < 0 || b.sprite >= kSpriteCount) continue;
        bool on = int(i) == hit;
        const std::string &ctrl = on ? b.onCtrl : b.offCtrl;
        if (!ctrl.empty()) {
            applySpStr(ctrl);
        } else {
            Sprite &sp = scene_.sp[size_t(b.sprite)];
            if (sp.img.cells > 1) sp.cell = on ? 1 : 0;
        }
    }
    if (hit < 0 && !btnDefault_.empty()) applySpStr(btnDefault_);
    commitScene();
}

bool Engine::finishBexec(const std::string &result, int sprite) {
    if (btnResult_.kind != VarRef::None) setVar(btnResult_, result);
    if (bexecNum_.kind != VarRef::None) setVar(bexecNum_, sprite);
    bexec_ = false;
    wait_ = WaitKind::None;
    hoverButton_ = -1;
    keyboardButton_ = -1;
    sceneDirty_ = true;
    return true;
}

static const char *bexecKeyName(int k) {
    static char letter[2];
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9')) {
        letter[0] = char(k);
        letter[1] = 0;
        return letter;
    }
    switch (k) {
    case VNNS_KEY_SPACE: return "SPACE";
    case VNNS_KEY_RETURN: return "RETURN";
    case VNNS_KEY_CTRL: return "CTRL";
    case VNNS_KEY_SHIFT: return "SHIFT";
    case VNNS_KEY_UP: return "UP";
    case VNNS_KEY_DOWN: return "DOWN";
    case VNNS_KEY_LEFT: return "LEFT";
    case VNNS_KEY_RIGHT: return "RIGHT";
    case VNNS_KEY_PAGEUP: return "PAGEUP";
    case VNNS_KEY_PAGEDOWN: return "PAGEDOWN";
    case VNNS_KEY_ESCAPE: return "ESC";
    default: break;
    }
    static char fn[4];
    if (k >= VNNS_KEY_F1 && k < VNNS_KEY_F1 + 12) {
        snprintf(fn, sizeof fn, "F%d", k - VNNS_KEY_F1 + 1);
        return fn;
    }
    return nullptr;
}

bool Engine::pollBexec() {
    if (in_.click || in_.rclick || in_.wheel) keyboardButton_ = -1;
    updateHover();
    auto pressButton = [&](int i) {
        const Button &b = buttons_[size_t(i)];
        if (!b.pressCtrl.empty()) {
            applySpStr(b.pressCtrl);
        } else if (b.sprite >= 0 && b.sprite < kSpriteCount && scene_.sp[size_t(b.sprite)].img.cells > 2) {
            scene_.sp[size_t(b.sprite)].cell = 2;
        }
        commitScene();
        return finishBexec("S" + std::to_string(b.sprite), b.sprite);
    };
    bool leftEvent = bDown_ ? in_.click : in_.clickUp;
    if (leftEvent) {
        lastClickX_ = mouseX_;
        lastClickY_ = mouseY_;
        if (hoverButton_ >= 0) return pressButton(hoverButton_);
        return finishBexec("LCLICK", -1);
    }
    if (in_.rclick) return finishBexec("RCLICK", -1);
    if (in_.mclick) return finishBexec("MCLICK", -1);
    if (in_.wheel > 0) return finishBexec("WHEELUP", -1);
    if (in_.wheel < 0) return finishBexec("WHEELDOWN", -1);
    for (int k : in_.keys) {
        bool arrow = k == VNNS_KEY_UP || k == VNNS_KEY_DOWN || k == VNNS_KEY_LEFT || k == VNNS_KEY_RIGHT;
        if (!bCursor_ && arrow && !buttons_.empty()) {
            // Without bcursor the arrows move between the buttons.
            int n = int(buttons_.size());
            int cur = hoverButton_ >= 0 ? hoverButton_ : -1;
            int dir = (k == VNNS_KEY_DOWN || k == VNNS_KEY_RIGHT) ? 1 : -1;
            keyboardButton_ = cur < 0 ? (dir > 0 ? 0 : n - 1) : (cur + dir + n) % n;
            updateHover();
            continue;
        }
        if (!bCursor_ && (k == VNNS_KEY_SPACE || k == VNNS_KEY_RETURN) && hoverButton_ >= 0) return pressButton(hoverButton_);
        if (const char *name = bexecKeyName(k)) return finishBexec(name, -1);
    }
    if (btnTimeout_ > 0 && now_ - bTimeStart_ >= btnTimeout_ && (!btnTimeoutWaitsVoice_ || !audio_.playing(0))) {
        bTimeStart_ = now_;
        return finishBexec("TIMEOUT", -1);
    }
    return false;
}

// The built-in choice display: the choices in the text window, one per line, picked with the mouse or keys.
void Engine::startSelect(SelectMode mode) {
    selectMode_ = mode;
    selectReturn_ = {cur_.pos, cur_.line, cur_.stmt};
    showWindow();
    clearPage();
    selectFirst_ = page_.size();
    selectRows_.clear();
    for (auto &[text, label] : cselItems_) {
        if (text.empty()) {
            selectRows_.push_back(-1);
            continue;
        }
        selectRows_.push_back(penY_);
        uint32_t saved = textColor_;
        textColor_ = selectColor(false);
        for (size_t i = 0; i < text.size();) {
            size_t n = sjisCharLen(text, i);
            putChar(text.substr(i, n));
            i += n;
        }
        textColor_ = saved;
        newLine();
    }
    selectHover_ = -1;
    wait_ = WaitKind::Select;
    composeFrame();
}

uint32_t Engine::selectColor(bool hover) const {
    const std::string &h = selectColorHex_[hover ? 0 : 1];
    unsigned v = unsigned(strtoul(h.c_str(), nullptr, 16));
    return rgba(int(v >> 16 & 255), int(v >> 8 & 255), int(v & 255));
}

bool Engine::pollSelect() {
    int rowH = win_.fh + win_.py;
    int hover = -1;
    for (size_t i = 0; i < selectRows_.size(); i++) {
        int y = selectRows_[i];
        if (y < 0) continue;
        int top = win_.ty + y;
        if (mouseY_ >= top && mouseY_ < top + rowH && mouseX_ >= win_.tx && mouseX_ < win_.tx + win_.cols * fullAdvance())
            hover = int(i);
    }
    for (int k : in_.keys) {
        int dir = k == VNNS_KEY_DOWN ? 1 : k == VNNS_KEY_UP ? -1 : 0;
        if (!dir) continue;
        int n = int(selectRows_.size());
        int h = selectHover_;
        for (int step = 0; step < n; step++) {
            h = (h + dir + n) % n;
            if (selectRows_[size_t(h)] >= 0) break;
        }
        selectHover_ = h;
        hover = h;
        keyboardSelect_ = true;
    }
    if (hover < 0 && keyboardSelect_) hover = selectHover_;
    {
        selectHover_ = hover;
        for (size_t i = selectFirst_; i < page_.size(); i++) {
            int y = page_[i].y - win_.ty - (rubyOn_ ? rubyH_ : 0);
            bool on = hover >= 0 && selectRows_[size_t(hover)] == y;
            page_[i].color = selectColor(on);
        }
        sceneDirty_ = true;
    }
    bool pick = in_.click && hover >= 0;
    for (int k : in_.keys)
        if ((k == VNNS_KEY_RETURN || k == VNNS_KEY_SPACE) && selectHover_ >= 0) pick = true;
    if (!pick) return false;
    keyboardSelect_ = false;
    int choice = selectHover_;
    wait_ = WaitKind::None;
    clearPage();
    windowShown_ = false;
    const std::string &label = cselItems_[size_t(choice)].second;
    switch (selectMode_) {
    case SelectMode::Goto: jumpToLabel(label); break;
    case SelectMode::Gosub: gosub(selectReturn_.pos, selectReturn_.line, selectReturn_.stmt, label); break;
    case SelectMode::Number: setVar(selectVar_, choice); break;
    }
    return true;
}

void registerButtonCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["btndef"] = cmdBtnDef;
    t["bclear"] = cmdBclear;
    t["bsp"] = cmdBsp;
    t["bdef"] = cmdBdef;
    t["btime"] = cmdBtime;
    for (const char *n : {"bcursor", "bdown", "btrans"}) t[n] = cmdBflag;
    t["bexec"] = cmdBexec;
    t["btn"] = cmdBtn;
    for (const char *n : {"spbtn", "cellcheckspbtn", "exbtn", "cellcheckexbtn"}) t[n] = cmdSpBtn;
    t["exbtn_d"] = cmdExBtnD;
    t["textexbtn"] = cmdTextExBtn;
    for (const char *n : {"btnwait", "btnwait2", "textbtnwait", "selectbtnwait"}) t[n] = cmdBtnWait;
    t["btntime"] = cmdBtnTime;
    t["btntime2"] = cmdBtnTime;
    t["getbtntimer"] = cmdGetBtnTimer;
    for (const char *n : {"getcursor", "getenter", "gettab", "getfunction", "getpage", "getpageup", "getinsert",
                          "getzxc", "getskipoff", "getmclick", "spclclk", "transbtn", "btndown"})
        t[n] = cmdBtnKey;
    t["btnarea"] = cmdBtnArea;
    t["btnnowindowerase"] = cmdBtnNoWindowErase;
    t["getmousepos"] = cmdGetMousePos;
    t["clickpos"] = cmdClickPos;
    t["checkkey"] = cmdCheckKey;
    t["isdown"] = cmdIsDown;
    t["csel"] = cmdCsel;
    t["select"] = cmdSelect;
    t["selgosub"] = cmdSelect;
    t["selnum"] = cmdSelect;
    t["getcselnum"] = cmdGetCselNum;
    t["getcselstr"] = cmdGetCselStr;
    t["cselgoto"] = cmdCselGoto;
    t["cselbtn"] = cmdCselBtn;
    t["selectcolor"] = cmdSelectColor;
}

}  // namespace vnns
