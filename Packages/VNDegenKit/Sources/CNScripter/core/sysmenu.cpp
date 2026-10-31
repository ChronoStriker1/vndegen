// NScripter's built-in screens: the right-click menu, the save and load lists, yes/no boxes, message boxes, the
// message history (lookback), and text input. Games that build their own screens never open these.
#include <algorithm>
#include <ctime>

#include "engine.h"

namespace vnns {

namespace {
std::string sjis(const char *utf8) { return utf8ToSjis(utf8); }
}  // namespace

void Engine::openUi(SystemUi ui) {
    ui.prevWait = wait_;
    ui.prevDone = std::move(onWaitDone_);
    onWaitDone_ = nullptr;
    ui.prevWaitUntil = waitUntil_;
    ui_ = std::make_unique<SystemUi>(std::move(ui));
    ui_->hover = -1;
    wait_ = WaitKind::Dialog;
    sceneDirty_ = true;
}

void Engine::closeUi(int choice) {
    std::unique_ptr<SystemUi> ui = std::move(ui_);
    wait_ = ui->prevWait;
    onWaitDone_ = std::move(ui->prevDone);
    waitUntil_ = ui->prevWaitUntil;
    sceneDirty_ = true;
    if (ui->kind == SystemUi::TextInput) lastInput_ = ui->input;
    if (ui->onChoose) ui->onChoose(choice);
}

bool Engine::textGosubActive() const { return !textGosub_.empty(); }

void Engine::openRightClickMenu() {
    if (lookbackOnRight_) {
        openLookback();
        return;
    }
    if (rmenu_.empty()) {
        // Without a menu, a right click hides the window until the next click.
        if (windowShown_) {
            SystemUi ui;
            ui.kind = SystemUi::HideWindow;
            windowShown_ = false;
            ui.onChoose = [this](int) { windowShown_ = true; };
            openUi(std::move(ui));
        }
        return;
    }
    SystemUi ui;
    ui.kind = SystemUi::Menu;
    for (auto &[text, fn] : rmenu_) ui.items.push_back(text);
    ui.onChoose = [this](int i) {
        if (i < 0 || size_t(i) >= rmenu_.size()) return;
        runSystemFunction(rmenu_[size_t(i)].second);
    };
    openUi(std::move(ui));
}

void Engine::runSystemFunction(const std::string &fn) {
    if (fn == "save") openSaveLoad(true);
    else if (fn == "load") openSaveLoad(false);
    else if (fn == "lookback") openLookback();
    else if (fn == "skip") skipMode_ = true;
    else if (fn == "automode") autoMode_ = true;
    else if (fn == "reset" || fn == "resetdlg") {
        yesNo(sjis("リセットしますか？"), [this](bool yes) {
            if (yes) resetGame(false);
        });
    } else if (fn == "end") {
        yesNo(sjis("終了しますか？"), [this](bool yes) {
            if (yes) {
                saveSystemData();
                ended_ = true;
            }
        });
    } else if (fn == "windowerase") {
        SystemUi ui;
        ui.kind = SystemUi::HideWindow;
        bool shown = windowShown_;
        windowShown_ = false;
        ui.onChoose = [this, shown](int) { windowShown_ = shown; };
        openUi(std::move(ui));
    } else if (fn == "rmenu") {
        openRightClickMenu();
    } else {
        logf("systemcall %s is not supported", fn.c_str());
    }
}

namespace {
// The menu bar functions VNDegen runs, and how each one shows. Volume (DWAVEVOLUME) opens a Windows dialog, and
// Fullscreen and Window (FULL, WINDOW) change the window, so the Game menu leaves them out.
enum class MenuMark { None, ReadSkipOff, ReadSkipOn, SlowText, NormalText, FastText };
struct MenuFunction {
    const char *name;
    MenuMark mark;
};
constexpr MenuFunction kMenuFunctions[] = {
    {"skip", MenuMark::None},           {"kidokuoff", MenuMark::ReadSkipOff}, {"kidokuon", MenuMark::ReadSkipOn},
    {"textslow", MenuMark::SlowText},   {"textmiddle", MenuMark::NormalText}, {"textfast", MenuMark::FastText},
};

const MenuFunction *menuFunction(const std::string &fn) {
    for (const MenuFunction &f : kMenuFunctions)
        if (fn == f.name) return &f;
    return nullptr;
}

void appendJsonString(std::string &out, const std::string &utf8) {
    out += '"';
    for (unsigned char c : utf8) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        } else {
            out += char(c);
        }
    }
    out += '"';
}
}  // namespace

std::string Engine::windowMenuJson() const {
    size_t next = 0;
    std::string items = windowMenuItemsJson(next, 0);
    return items == "[]" ? std::string() : items;
}

// The items from `next` on at `level`, until one shallower. Each item's id is its index in insertmenu's order, and a
// submenu holds the deeper items that follow it.
std::string Engine::windowMenuItemsJson(size_t &next, int level) const {
    std::string out = "[";
    while (next < windowMenu_.size() && windowMenu_[next].level >= level) {
        size_t id = next++;
        const WindowMenuItem &item = windowMenu_[id];
        if (item.level > level) continue;  // deeper than any submenu open here
        bool sub = item.fn == "sub";
        std::string children = sub ? windowMenuItemsJson(next, level + 1) : "[]";
        const MenuFunction *f = sub ? nullptr : menuFunction(item.fn);
        // A submenu of items VNDegen leaves out, such as Screen Mode's, goes too.
        if (sub ? children == "[]" : !f) continue;
        bool checked = false;
        if (f) {
            switch (f->mark) {
            case MenuMark::None: break;
            case MenuMark::ReadSkipOff: checked = !kidokuSkip_; break;
            case MenuMark::ReadSkipOn: checked = kidokuSkip_; break;
            case MenuMark::SlowText: checked = speedChoice_ == 0; break;
            case MenuMark::NormalText: checked = speedChoice_ == 1; break;
            case MenuMark::FastText: checked = speedChoice_ == 2; break;
            }
        }
        if (out.size() > 1) out += ',';
        out += "{\"id\":\"" + std::to_string(id) + "\",\"title\":";
        appendJsonString(out, sjisToUtf8(item.text));
        out += checked ? ",\"checked\":true" : ",\"checked\":false";
        out += f && f->mark != MenuMark::None ? ",\"radio\":true" : ",\"radio\":false";
        out += ",\"enabled\":true,\"items\":" + children + "}";
    }
    return out + "]";
}

bool Engine::chooseWindowMenuItem(size_t index) {
    if (index >= windowMenu_.size()) return false;
    const MenuFunction *f = menuFunction(windowMenu_[index].fn);
    if (!f) return false;
    logf("menu bar: %s", windowMenu_[index].fn.c_str());
    switch (f->mark) {
    case MenuMark::None: runSystemFunction("skip"); break;
    case MenuMark::ReadSkipOff: kidokuSkip_ = false; break;
    case MenuMark::ReadSkipOn:
        kidokuSkip_ = true;
        kidokuOn_ = true;
        break;
    case MenuMark::SlowText:
    case MenuMark::NormalText:
    case MenuMark::FastText:
        speedChoice_ = f->mark == MenuMark::SlowText ? 0 : f->mark == MenuMark::NormalText ? 1 : 2;
        textSpeed_ = defaultSpeeds_[speedChoice_];
        break;
    }
    return true;
}

void Engine::yesNo(const std::string &message, std::function<void(bool)> done, bool okCancel) {
    SystemUi ui;
    ui.kind = SystemUi::YesNo;
    ui.message = message;
    // The PC engine shows a Windows message box, labelled in the system's language. A message in plain ASCII comes
    // from a translated game, so its buttons are labelled in English.
    bool english = std::all_of(message.begin(), message.end(), [](char c) { return uint8_t(c) < 0x80; });
    if (english) ui.items = {okCancel ? "OK" : "Yes", okCancel ? "Cancel" : "No"};
    else ui.items = {okCancel ? "OK" : sjis("はい"), okCancel ? sjis("キャンセル") : sjis("いいえ")};
    ui.onChoose = [done](int i) { done(i == 0); };
    openUi(std::move(ui));
    // As in a Windows message box, the first button has the focus, so Return answers it.
    ui_->hover = 0;
    ui_->keyboard = true;
}

void Engine::openSaveLoad(bool save) {
    SystemUi ui;
    ui.kind = SystemUi::Menu;
    ui.title = save ? (saveNames_[0].empty() ? sjis("＜セーブ＞") : saveNames_[0])
                    : (saveNames_[1].empty() ? sjis("＜ロード＞") : saveNames_[1]);
    std::string name = saveNames_[2].empty() ? sjis("しおり") : saveNames_[2];
    for (int i = 1; i <= saveNumber_; i++) {
        std::string line = name + std::to_string(i) + "  ";
        SaveInfo info;
        if (readSaveInfo(i, info)) {
            char buf[64];
            snprintf(buf, sizeof buf, "%d/%d %02d:%02d", info.month, info.day, info.hour, info.minute);
            line += buf;
            ui.enabled.push_back(true);
        } else {
            line += "-------";
            ui.enabled.push_back(save);
        }
        ui.items.push_back(line);
    }
    ui.onChoose = [this, save](int i) {
        if (i < 0) return;
        int slot = i + 1;
        if (save) {
            saveGame(slot, std::string());
        } else {
            loadGame(slot);
        }
    };
    openUi(std::move(ui));
}

void Engine::openLookback() {
    if (log_.empty()) return;
    SystemUi ui;
    ui.kind = SystemUi::Lookback;
    ui.page = int(log_.size()) - 1;
    openUi(std::move(ui));
}

// Draws the open built-in screen over the frame.
// yesnobox, okcancelbox, mesbox and input, laid out as the PC engine's Windows dialogs are: light grey, black text,
// buttons side by side, over the game without dimming it. A message box is centred; input's window sits at the top
// left of the screen, sized by its arguments (docs/nscripter-cleanroom.md has the measurements).
void Engine::drawDialog(Image &out, SystemUi &ui) {
    const int fh = 14, fw = 14;  // the dialog font, a little larger than Windows' for small screens
    const uint32_t face = rgba(0xF5, 0xF5, 0xF5), ink = rgba(0, 0, 0);
    auto textWidth = [&](const std::string &str) {
        int w = 0;
        for (size_t i = 0; i < str.size(); i += sjisCharLen(str, i)) w += sjisCharLen(str, i) == 1 ? fw / 2 : fw;
        return w;
    };
    auto button = [&](int x, int y, int w, int h, const std::string &label, bool focused) {
        fillRect(out, x, y, w, h, focused ? rgba(0, 0, 0) : rgba(0x70, 0x70, 0x70));
        fillRect(out, x + 1, y + 1, w - 2, h - 2, rgba(0xF0, 0xF0, 0xF0));
        if (focused) fillRect(out, x + 1, y + h - 2, w - 2, 1, rgba(0xA0, 0xA0, 0xA0));
        drawGlyphs(out, label, x + (w - textWidth(label)) / 2, y + (h - fh) / 2, fw, fh, 0, ink, false, false, 0, 0);
    };
    ui.buttons.clear();
    if (ui.kind == SystemUi::TextInput) {
        // Window and field: the arguments in 4/3 pixels, less the frame Windows draws around the window.
        int w = ui.dlgW > 0 ? ui.dlgW * 4 / 3 - 6 : 397, h = ui.dlgH > 0 ? ui.dlgH * 4 / 3 - 32 : 92;
        int fieldW = ui.fieldW > 0 ? ui.fieldW * 4 / 3 : 266, fieldH = ui.fieldH > 0 ? ui.fieldH * 4 / 3 : 26;
        w = std::clamp(w, 120, out.w);
        h = std::clamp(h, 60, out.h);
        fillRect(out, 0, 0, w, h, face);
        // Long prompts and typed text lose their end, or their start for typed text, so the cursor stays in view.
        auto fit = [&](std::string str, int width, bool keepEnd) {
            while (!str.empty() && textWidth(str) > width) {
                if (keepEnd) {
                    str.erase(0, sjisCharLen(str, 0));
                } else {
                    size_t last = 0;
                    for (size_t i = 0; i < str.size(); i += sjisCharLen(str, i)) last = i;
                    str.erase(last);
                }
            }
            return str;
        };
        drawGlyphs(out, fit(ui.message, w - 24, false), 12, 12, fw, fh, 0, ink, false, false, 0, 0);
        int fy = 46, bw = 70, bx = w - 21 - bw;
        fieldW = std::min(fieldW, std::max(bx - 12 - 8, 40));
        fillRect(out, 12, fy, fieldW, fieldH, rgba(0x80, 0x80, 0x80));
        fillRect(out, 13, fy + 1, fieldW - 1, fieldH - 1, rgba(0xFF, 0xFF, 0xFF));
        drawGlyphs(out, fit(ui.items.empty() ? std::string() : ui.items[0], fieldW - 10, true), 17, fy + 4, fw, fh, 0,
                   ink, false, false, 0, 0);
        int bh = std::min(fieldH, h - fy - 4);
        button(bx, fy, bw, bh, "OK", false);
        ui.buttons.push_back({bx, fy, bw, bh});
        return;
    }
    // A message box: the message, then its buttons in a row, centred on the screen.
    const int bw = 64, bh = 26, gap = 16;
    int rowW = int(ui.items.size()) * bw + (int(ui.items.size()) - 1) * gap;
    int w = std::min(std::max(textWidth(ui.message), rowW) + 48, out.w);
    int h = 12 + fh + 14 + bh + 12;
    int x = (out.w - w) / 2, y = (out.h - h) / 2;
    fillRect(out, x, y, w, h, face);
    drawGlyphs(out, ui.message, x + 24, y + 12, fw, fh, 0, ink, false, false, 0, 0);
    int bx = x + (w - rowW) / 2, by = y + 12 + fh + 14;
    for (size_t i = 0; i < ui.items.size(); i++) {
        button(bx, by, bw, bh, ui.items[i], int(i) == ui.hover);
        ui.buttons.push_back({bx, by, bw, bh});
        bx += bw + gap;
    }
}

void Engine::drawSystemUi(Image &out) {
    if (!ui_) return;
    SystemUi &ui = *ui_;
    if (ui.kind == SystemUi::HideWindow) return;
    if (ui.kind == SystemUi::Lookback) {
        multiplyRect(out, 0, 0, out.w, out.h, rgba(0x60, 0x60, 0x60));
        const std::string &text = log_[size_t(std::clamp(ui.page, 0, int(log_.size()) - 1))];
        int x = win_.tx, y = win_.ty;
        size_t start = 0;
        while (start <= text.size()) {
            size_t nl = text.find('\n', start);
            std::string line = text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
            drawGlyphs(out, line, x, y, win_.fw, win_.fh, win_.px, rgba(0xFF, 0xFF, 0x88), win_.bold, true, 0, 0);
            y += win_.fh + win_.py;
            if (nl == std::string::npos) break;
            start = nl + 1;
        }
        return;
    }
    if (ui.kind == SystemUi::YesNo || ui.kind == SystemUi::TextInput) {
        drawDialog(out, ui);
        return;
    }
    multiplyRect(out, 0, 0, out.w, out.h, rgba(0x80, 0x80, 0x80));
    int fw = 24, fh = 24, rowH = fh + 8;
    int rows = int(ui.items.size()) + (ui.title.empty() ? 0 : 1) + (ui.message.empty() ? 0 : 2);
    int boxW = 0;
    auto textWidth = [&](const std::string &s) {
        int w = 0;
        for (size_t i = 0; i < s.size(); i += sjisCharLen(s, i)) w += sjisCharLen(s, i) == 1 ? fw / 2 : fw;
        return w;
    };
    for (auto &s : ui.items) boxW = std::max(boxW, textWidth(s));
    boxW = std::min(std::max({boxW, textWidth(ui.title), textWidth(ui.message)}) + 48, out.w - 32);
    // Lines too long for the screen lose their end, or their start for typed text, so its cursor stays in view.
    auto fit = [&](std::string s, bool keepEnd) {
        while (textWidth(s) > boxW - 48) {
            if (keepEnd) {
                s.erase(0, sjisCharLen(s, 0));
            } else {
                size_t last = 0;
                for (size_t i = 0; i < s.size(); i += sjisCharLen(s, i)) last = i;
                s.erase(last);
            }
        }
        return s;
    };
    int boxH = rows * rowH + 32;
    int bx = (out.w - boxW) / 2, by = (out.h - boxH) / 2;
    fillRect(out, bx, by, boxW, boxH, rgba(0x20, 0x20, 0x30));
    int y = by + 16;
    ui.rowTop = 0;
    if (!ui.title.empty()) {
        drawGlyphs(out, fit(ui.title, false), bx + 24, y, fw, fh, 0, rgba(0xFF, 0xFF, 0xFF), true, false, 0, 0);
        y += rowH;
    }
    if (!ui.message.empty()) {
        drawGlyphs(out, fit(ui.message, false), bx + 24, y, fw, fh, 0, rgba(0xFF, 0xFF, 0xFF), true, false, 0, 0);
        y += rowH * 2;
    }
    ui.rowTop = y;
    ui.rowH = rowH;
    ui.boxX = bx;
    ui.boxW = boxW;
    for (size_t i = 0; i < ui.items.size(); i++) {
        bool enabled = i >= ui.enabled.size() || ui.enabled[i];
        uint32_t color = !enabled ? rgba(0x70, 0x70, 0x70) : int(i) == ui.hover ? rgba(0xFF, 0xFF, 0x60) : rgba(0xE0, 0xE0, 0xE0);
        drawGlyphs(out, fit(ui.items[i], ui.kind == SystemUi::TextInput), bx + 24, y, fw, fh, 0, color, true, false, 0, 0);
        y += rowH;
    }
}

void Engine::pollSystemUi() {
    if (!ui_) {
        wait_ = WaitKind::None;
        return;
    }
    SystemUi &ui = *ui_;
    if (ui.kind == SystemUi::TextInput) {
        for (size_t i = 0; i < typed_.size();) {
            size_t n = sjisCharLen(typed_, i);
            unsigned char c = uint8_t(typed_[i]);
            if (n == 1 && c < 0x20) {
                if (c == '\b' && !ui.input.empty()) {
                    // Remove the last character, one or two bytes.
                    size_t last = 0;
                    for (size_t k = 0; k < ui.input.size(); k += sjisCharLen(ui.input, k)) last = k;
                    ui.input.erase(last);
                } else if (c == '\r' || c == '\n') {
                    typed_.clear();
                    closeUi(0);
                    return;
                }
            } else if (int(ui.input.size() + n) <= ui.maxLen) {
                ui.input += typed_.substr(i, n);
            }
            i += n;
        }
        typed_.clear();
        // A byte-counted mid can leave half a character at the end, which would take the cursor as its second byte.
        size_t last = 0;
        for (size_t i = 0; i < ui.input.size(); i += sjisCharLen(ui.input, i)) last = i;
        bool half = !ui.input.empty() && last == ui.input.size() - 1 && sjisLead(uint8_t(ui.input[last]));
        ui.items = {ui.input.substr(0, half ? last : ui.input.size()) + "_"};
        sceneDirty_ = true;
        if (in_.click && !ui.buttons.empty()) {
            const auto &b = ui.buttons[0];
            if (mouseX_ >= b[0] && mouseX_ < b[0] + b[2] && mouseY_ >= b[1] && mouseY_ < b[1] + b[3]) {
                closeUi(0);
                return;
            }
        }
        for (int k : in_.keys)
            if (k == VNNS_KEY_RETURN) {
                closeUi(0);
                return;
            } else if (k == VNNS_KEY_BACKSPACE && !ui.input.empty()) {
                size_t last = 0;
                for (size_t q = 0; q < ui.input.size(); q += sjisCharLen(ui.input, q)) last = q;
                ui.input.erase(last);
            }
        return;
    }
    if (ui.kind == SystemUi::HideWindow) {
        if (in_.click || in_.rclick || !in_.keys.empty()) closeUi(0);
        return;
    }
    if (ui.kind == SystemUi::Lookback) {
        bool up = in_.wheel > 0, down = in_.wheel < 0;
        for (int k : in_.keys) {
            if (k == VNNS_KEY_UP || k == VNNS_KEY_PAGEUP) up = true;
            if (k == VNNS_KEY_DOWN || k == VNNS_KEY_PAGEDOWN) down = true;
        }
        if (up && ui.page > 0) {
            ui.page--;
            sceneDirty_ = true;
        } else if (down) {
            if (ui.page + 1 >= int(log_.size())) {
                closeUi(-1);
                return;
            }
            ui.page++;
            sceneDirty_ = true;
        }
        if (in_.click || in_.rclick) closeUi(-1);
        return;
    }
    int hover = -1;
    if (!ui.buttons.empty()) {
        for (size_t i = 0; i < ui.buttons.size(); i++) {
            const auto &b = ui.buttons[i];
            if (mouseX_ >= b[0] && mouseX_ < b[0] + b[2] && mouseY_ >= b[1] && mouseY_ < b[1] + b[3]) hover = int(i);
        }
    } else if (ui.rowH > 0 && mouseX_ >= ui.boxX && mouseX_ < ui.boxX + ui.boxW && mouseY_ >= ui.rowTop) {
        int r = (mouseY_ - ui.rowTop) / ui.rowH;
        if (r < int(ui.items.size())) hover = r;
    }
    if (hover >= 0 || ui.keyboard) {
        if (hover >= 0) ui.keyboard = false;
    }
    for (int k : in_.keys) {
        int n = int(ui.items.size());
        // Message box buttons sit in a row, so left and right move between them too.
        bool next = k == VNNS_KEY_DOWN || k == VNNS_KEY_RIGHT, prev = k == VNNS_KEY_UP || k == VNNS_KEY_LEFT;
        if (next || prev) {
            ui.keyboard = true;
            ui.hover = ((ui.hover < 0 ? (next ? -1 : 0) : ui.hover) + (next ? 1 : -1) + n) % n;
            sceneDirty_ = true;
        }
        if ((k == VNNS_KEY_RETURN || k == VNNS_KEY_SPACE) && ui.hover >= 0) {
            bool enabled = size_t(ui.hover) >= ui.enabled.size() || ui.enabled[size_t(ui.hover)];
            if (enabled) {
                closeUi(ui.hover);
                return;
            }
        }
        if (k == VNNS_KEY_ESCAPE) {
            closeUi(ui.kind == SystemUi::YesNo ? 1 : -1);
            return;
        }
    }
    if (!ui.keyboard && hover != ui.hover) {
        ui.hover = hover;
        sceneDirty_ = true;
    }
    if (in_.rclick) {
        closeUi(ui.kind == SystemUi::YesNo ? 1 : -1);
        return;
    }
    // A click answers what is under the pointer, not the item the keys focused.
    if (in_.click && hover >= 0) {
        bool enabled = size_t(hover) >= ui.enabled.size() || ui.enabled[size_t(hover)];
        if (enabled) closeUi(hover);
    }
}

namespace {
using E = Engine;

void cmdSystemCall(E &e, const std::string &) { e.runSystemFunction(e.readWord()); }

void cmdYesNoBox(E &e, const std::string &name) {
    E::VarRef v = e.readVar();
    e.readComma();
    std::string message = e.readStr();
    if (e.readComma()) e.readStr();  // the Windows dialog's title
    e.yesNo(message, [&e, v](bool yes) { e.setVar(v, yes ? 1 : 0); }, name == "okcancelbox");
}

void cmdMesBox(E &e, const std::string &) {
    std::string message = e.readStr();
    if (e.readComma()) e.readStr();
    SystemUi ui;
    ui.kind = SystemUi::YesNo;
    ui.message = message;
    ui.items = {"OK"};
    e.openUi(std::move(ui));
}

// inputstr / input / textfield: a line of text typed by the player. The host's keyboard types into it.
void cmdInput(E &e, const std::string &name) {
    E::VarRef v;
    std::string prompt, initial;
    int maxLen = 32;
    int dlgW = 0, dlgH = 0, fieldW = 0, fieldH = 0;
    if (name == "input") {
        // input "prompt","default",max,zenkaku   or   input $var,"prompt","default",max,zenkaku[,window...]
        e.skipSpaces();
        if (e.peek() == '$') {
            v = e.readVar();
            e.readComma();
        } else {
            v.kind = E::VarRef::None;
        }
        prompt = e.readStr();
        e.readComma();
        initial = e.readStr();
        if (e.readComma()) maxLen = e.readInt();
        if (e.readComma()) e.readInt();  // zenkaku
        // Then the window's width and height and the field's, which the PC engine's dialog takes in 4/3 pixels.
        int *size[] = {&dlgW, &dlgH, &fieldW, &fieldH};
        for (int *v : size)
            if (e.readComma()) *v = e.readInt();
        e.skipStatement();
    } else if (name == "textfield") {
        v = e.readVar();
        initial = e.strVar(v.index);
        e.skipStatement();
    } else {
        v = e.readVar();
        e.readComma();
        prompt = e.readStr();
        if (e.readComma()) maxLen = e.readInt();
        e.skipStatement();
        initial = v.kind == E::VarRef::Str ? e.strVar(v.index) : std::string();
    }
    SystemUi ui;
    ui.kind = SystemUi::TextInput;
    ui.message = prompt;
    ui.input = initial;
    ui.maxLen = maxLen * 2;
    ui.items = {initial};
    ui.dlgW = dlgW;
    ui.dlgH = dlgH;
    ui.fieldW = fieldW;
    ui.fieldH = fieldH;
    e.typed_.clear();
    ui.onChoose = [&e, v](int) {
        if (v.kind != E::VarRef::None) e.setVar(v, e.lastInput_);
    };
    e.openUi(std::move(ui));
}

}  // namespace

void registerSystemCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["systemcall"] = cmdSystemCall;
    t["yesnobox"] = cmdYesNoBox;
    t["okcancelbox"] = cmdYesNoBox;
    t["mesbox"] = cmdMesBox;
    t["inputstr"] = cmdInput;
    t["input"] = cmdInput;
    t["textfield"] = cmdInput;
}

}  // namespace vnns
