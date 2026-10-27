#include "game.h"

#include "reader.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fs = std::filesystem;

namespace wolf {

namespace {

// System database types, in the order every Wolf RPG Editor 2 project has them.
enum SysType {
    SysMaps = 0,
    SysStringVarNames = 4,
    SysSystemStrings = 5,
    SysSystemVarNames = 6,
    SysPositions = 7,
    SysWindows = 10,
    SysTextColors = 12,
    SysNormalVarNames = 14,
    SysSpare1 = 15,
};

constexpr int32_t kMillion = 1000000;

bool compare(int32_t a, int32_t b, int op) {
    switch (op) {
        case 0: return a > b;
        case 1: return a >= b;
        case 2: return a == b;
        case 3: return a <= b;
        case 4: return a < b;
        case 5: return a != b;
        case 6: return (a & b) != 0;
    }
    return false;
}

std::string format(const char *fmt, auto... args) {
    char buf[512];
    std::snprintf(buf, sizeof buf, fmt, args...);
    return buf;
}

}  // namespace

Game::Game(fs::path root) : root_(root), files_(root) {}

bool Game::load(std::string *error) {
    try {
        auto need = [&](const std::string &p) {
            auto d = files_.read(p);
            if (!d) throw FormatError("missing " + p);
            return *d;
        };
        gameDat_ = parseGameDat(need("BasicData/Game.dat"));
        Encoding enc = gameDat_.encoding;
        commons_ = parseCommonEvents(need("BasicData/CommonEvent.dat"));
        udb_ = parseDatabase(need("BasicData/DataBase.project"), need("BasicData/DataBase.dat"), enc);
        cdb_ = parseDatabase(need("BasicData/CDataBase.project"), need("BasicData/CDataBase.dat"), enc);
        sdb_ = parseDatabase(need("BasicData/SysDataBase.project"), need("BasicData/SysDataBase.dat"), enc);
        tilesets_ = parseTilesets(need("BasicData/TileSetData.dat"));
    } catch (const std::exception &e) {
        if (error) *error = e.what();
        return false;
    }

    auto rows = [&](int type) { return type < int(sdb_.types.size()) ? sdb_.types[type].rows.size() : 0; };
    normal_.assign(std::max<size_t>(rows(SysNormalVarNames), 100), 0);
    for (int i = 0; i < 9; i++) spare_[i].assign(std::max<size_t>(rows(SysSpare1 + i), 100), 0);
    system_.assign(std::max<size_t>(rows(SysSystemVarNames), 200), 0);
    strings_.assign(std::max<size_t>(rows(SysStringVarNames), 100), "");
    systemStrings_.assign(std::max<size_t>(rows(SysSystemStrings), 100), "");
    int maxCommon = 0;
    for (auto &ce : commons_) maxCommon = std::max(maxCommon, ce.id + 1);
    cself_.assign(maxCommon, {});
    cselfStr_.assign(maxCommon, {});

    // The game starts at the first entry of the position list.
    int startMap = dbNumber(sdb_, SysPositions, 0, 0);
    hero_.x = dbNumber(sdb_, SysPositions, 0, 1);
    hero_.y = dbNumber(sdb_, SysPositions, 0, 2);
    hero_.graphic = gameDat_.startHeroGraphic;
    if (!loadMap(startMap)) {
        if (error) *error = format("cannot load start map %d", startMap);
        return false;
    }
    tintR_.set(100, 0);
    tintG_.set(100, 0);
    tintB_.set(100, 0);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Variables

int32_t Game::dbNumber(const Database &db, int type, int row, int field) const {
    if (type < 0 || type >= int(db.types.size())) return 0;
    auto &t = db.types[type];
    if (row < 0 || row >= int(t.rows.size()) || field < 0 || field >= int(t.fieldKinds.size()) || t.isString(field))
        return 0;
    auto &nums = t.rows[row].numbers;
    size_t slot = t.slot(field);
    return slot < nums.size() ? nums[slot] : 0;
}

std::string Game::dbString(const Database &db, int type, int row, int field) const {
    if (type < 0 || type >= int(db.types.size())) return {};
    auto &t = db.types[type];
    if (row < 0 || row >= int(t.rows.size()) || field < 0 || field >= int(t.fieldKinds.size()) || !t.isString(field))
        return {};
    auto &strs = t.rows[row].strings;
    size_t slot = t.slot(field);
    return slot < strs.size() ? strs[slot] : std::string();
}

std::string Game::systemString(int i) const { return dbString(sdb_, SysSystemStrings, i, 0); }

// Rows of the window images type: 0 the pause icon, 1 the choice window, 2 the choice cursor.
std::string Game::windowSkin() const { return dbString(sdb_, SysWindows, 1, 0); }

std::string Game::cursorSkin() const { return dbString(sdb_, SysWindows, 2, 0); }

std::array<uint8_t, 3> Game::textColor(int i) const {
    if (i <= 0 || i >= int(sdb_.types.size() > SysTextColors ? sdb_.types[SysTextColors].rows.size() : 0))
        return {255, 255, 255};
    return {uint8_t(dbNumber(sdb_, SysTextColors, i, 0)), uint8_t(dbNumber(sdb_, SysTextColors, i, 1)),
            uint8_t(dbNumber(sdb_, SysTextColors, i, 2))};
}

int32_t *Game::selfSlot(int32_t id, const Thread &t) {
    const Thread::Call &c = t.stack.back();
    if (id >= 1600000 && id < 1600100) {
        if (c.commonEvent < 0 || c.commonEvent >= int(cself_.size())) return nullptr;
        return &cself_[c.commonEvent][id - 1600000];
    }
    if (id >= 15000000 && id < 16000000) {
        int ce = (id - 15000000) / 100;
        if (ce >= int(cself_.size())) return nullptr;
        return &cself_[ce][(id - 15000000) % 100];
    }
    if (id >= 1100000 && id < 1100010) {
        int ev = c.mapEvent;
        if (ev < 0 || ev >= int(events_.size())) return nullptr;
        return &events_[ev].self[id - 1100000];
    }
    if (id >= 1000000 && id < 1100000) {
        int ev = (id - 1000000) / 10;
        for (auto &e : events_)
            if (e.data && e.data->id == ev) return &e.self[(id - 1000000) % 10];
        return nullptr;
    }
    return nullptr;
}

bool Game::isStringVar(int32_t id) const {
    if (id >= 3000000 && id < 4000000) return true;
    if (id >= 9900000 && id < 10000000) return true;
    if (id >= 1600005 && id < 1600010) return true;
    if (id >= 15000000 && id < 16000000) {
        int s = (id - 15000000) % 100;
        return s >= 5 && s < 10;
    }
    return false;
}

int32_t Game::value(int32_t v, const Thread &t) {
    if (v < kMillion && v > -kMillion) return v;
    return readVar(v, t);
}

int32_t Game::readVar(int32_t id, const Thread &t) {
    if (id >= 2000000 && id < 2100000) return id - 2000000 < int(normal_.size()) ? normal_[id - 2000000] : 0;
    if (id >= 2100000 && id < 3000000) {
        int bank = (id - 2100000) / 100000, i = (id - 2100000) % 100000;
        return i < int(spare_[bank].size()) ? spare_[bank][i] : 0;
    }
    if (id >= 8000000 && id < 9000000) {
        int range = id - 8000000;
        return std::uniform_int_distribution<int>(0, std::max(0, range))(rng_);
    }
    if (id >= 9000000 && id < 9100000) return id - 9000000 < int(system_.size()) ? system_[id - 9000000] : 0;
    if (id >= 1000000000) {
        const Database &db = id >= 1300000000 ? sdb_ : id >= 1100000000 ? cdb_ : udb_;
        // 1 X TT DDDD FF: X picks the database, then type, row and field.
        int type = (id / 1000000) % 100;
        int row = (id / 100) % 10000;
        int field = id % 100;
        return dbNumber(db, type, row, field);
    }
    if (int32_t *s = selfSlot(id, t)) return *s;
    if (id >= 9180000 && id < 9200000) return 0;  // hero and event info, filled in with movement
    return id;
}

void Game::writeVar(int32_t id, int32_t v, const Thread &t) {
    if (id >= 2000000 && id < 2100000) {
        size_t i = id - 2000000;
        if (i >= normal_.size()) normal_.resize(i + 1);
        normal_[i] = v;
        return;
    }
    if (id >= 2100000 && id < 3000000) {
        int bank = (id - 2100000) / 100000;
        size_t i = (id - 2100000) % 100000;
        if (i >= spare_[bank].size()) spare_[bank].resize(i + 1);
        spare_[bank][i] = v;
        return;
    }
    if (id >= 9000000 && id < 9100000) {
        size_t i = id - 9000000;
        if (i >= system_.size()) system_.resize(i + 1);
        system_[i] = v;
        return;
    }
    if (int32_t *s = selfSlot(id, t)) {
        *s = v;
        return;
    }
    log(format("write to unsupported variable %d", id));
}

std::string Game::readStr(int32_t id, const Thread &t) const {
    if (id >= 3000000 && id < 4000000) return id - 3000000 < int(strings_.size()) ? strings_[id - 3000000] : "";
    if (id >= 9900000 && id < 10000000)
        return id - 9900000 < int(systemStrings_.size()) ? systemStrings_[id - 9900000] : "";
    const Thread::Call &c = t.stack.back();
    if (id >= 1600000 && id < 1600100 && c.commonEvent >= 0) return cselfStr_[c.commonEvent][id - 1600000];
    if (id >= 15000000 && id < 16000000) {
        int ce = (id - 15000000) / 100;
        if (ce < int(cselfStr_.size())) return cselfStr_[ce][(id - 15000000) % 100];
    }
    if (id >= 1000000000) {
        const Database &db = id >= 1300000000 ? sdb_ : id >= 1100000000 ? cdb_ : udb_;
        return dbString(db, (id / 1000000) % 100, (id / 100) % 10000, id % 100);
    }
    return {};
}

void Game::writeStr(int32_t id, const std::string &s, const Thread &t) {
    if (id >= 3000000 && id < 4000000) {
        size_t i = id - 3000000;
        if (i >= strings_.size()) strings_.resize(i + 1);
        strings_[i] = s;
        return;
    }
    if (id >= 9900000 && id < 10000000) {
        size_t i = id - 9900000;
        if (i >= systemStrings_.size()) systemStrings_.resize(i + 1);
        systemStrings_[i] = s;
        return;
    }
    const Thread::Call &c = t.stack.back();
    if (id >= 1600000 && id < 1600100 && c.commonEvent >= 0) {
        cselfStr_[c.commonEvent][id - 1600000] = s;
        return;
    }
    if (id >= 15000000 && id < 16000000) {
        int ce = (id - 15000000) / 100;
        if (ce < int(cselfStr_.size())) cselfStr_[ce][(id - 15000000) % 100] = s;
        return;
    }
    log(format("write to unsupported string variable %d", id));
}

std::string Game::expandText(const std::string &text, const Thread *t) const {
    std::string out;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] != '\\') {
            out += text[i];
            continue;
        }
        auto code = [&](const char *name) {
            size_t n = std::strlen(name);
            return text.compare(i + 1, n, name) == 0 && i + 1 + n < text.size() && text[i + 1 + n] == '[';
        };
        auto number = [&](size_t open, size_t *close) {
            *close = text.find(']', open);
            if (*close == std::string::npos) return -1;
            return std::atoi(text.c_str() + open + 1);
        };
        size_t close = 0;
        if (code("self")) {
            int n = number(i + 5, &close);
            Thread copy = t ? *t : Thread{};
            if (t && !copy.stack.empty())
                out += std::to_string(const_cast<Game *>(this)->readVar(1100000 + n, copy));
            i = close;
        } else if (code("cself")) {
            int n = number(i + 6, &close);
            if (t && !t->stack.empty() && t->stack.back().commonEvent >= 0) {
                int ce = t->stack.back().commonEvent;
                out += (n >= 5 && n < 10) ? cselfStr_[ce][n] : std::to_string(cself_[ce][n]);
            }
            i = close;
        } else if (code("v")) {
            int n = number(i + 2, &close);
            if (n >= 0 && n < int(normal_.size())) out += std::to_string(normal_[n]);
            i = close;
        } else if (code("s")) {
            int n = number(i + 2, &close);
            if (n >= 0 && n < int(strings_.size())) out += strings_[n];
            i = close;
        } else if (code("sys")) {
            int n = number(i + 4, &close);
            if (n >= 0 && n < int(system_.size())) out += std::to_string(system_[n]);
            i = close;
        } else if (code("sysS")) {
            int n = number(i + 5, &close);
            if (n >= 0 && n < int(systemStrings_.size())) out += systemStrings_[n];
            i = close;
        } else {
            out += text[i];
        }
        if (close == std::string::npos) break;
    }
    return out;
}

// ---------------------------------------------------------------------------------------------------------------
// Maps

bool Game::loadMap(int id) {
    std::string file = dbString(sdb_, SysMaps, id, 0);
    auto data = files_.read(file);
    if (!data) {
        log("missing map file " + file);
        return false;
    }
    try {
        map_ = std::make_unique<Map>(parseMap(*data));
    } catch (const std::exception &e) {
        log("map " + file + ": " + e.what());
        return false;
    }
    mapId_ = id;
    events_.clear();
    for (auto &ev : map_->events) {
        MapEventState s;
        s.data = &ev;
        s.chara.x = ev.x;
        s.chara.y = ev.y;
        s.chara.visible = false;
        events_.push_back(s);
    }
    mapParallel_.clear();
    refreshPages();
    return true;
}

int Game::activePage(const MapEventState &e, const Thread &ctx) {
    if (e.erased) return -1;
    for (int p = int(e.data->pages.size()) - 1; p >= 0; p--) {
        auto &pg = e.data->pages[p];
        bool ok = true;
        for (int i = 0; i < 4 && ok; i++) {
            int op = pg.condOps[i] >> 4, enabled = pg.condOps[i] & 0x0F;
            if (!enabled) continue;
            ok = compare(readVar(pg.condVars[i], ctx), pg.condValues[i], op);
        }
        if (ok) return p;
    }
    return -1;
}

void Game::refreshPages() {
    for (size_t i = 0; i < events_.size(); i++) {
        Thread ctx;
        ctx.stack.push_back({nullptr, 0, -1, int(i)});
        int page = activePage(events_[i], ctx);
        if (page != events_[i].page) {
            auto &e = events_[i];
            e.page = page;
            e.ranAutorun = false;
            Character &c = e.chara;
            c.visible = page >= 0;
            if (page >= 0) {
                const MapPage &pg = e.data->pages[page];
                c.graphic = pg.graphic;
                c.chip = pg.graphic.empty() && pg.chip > 0 ? pg.chip : -1;
                c.direction = pg.direction ? pg.direction : 2;
                c.pattern = pg.frame;
                c.opacity = pg.opacity;
                c.blend = pg.blend;
                c.shadow = pg.shadow;
            }
        }
    }
}

void Game::startPage(int eventIndex, Thread &t) {
    auto &e = events_[eventIndex];
    t.stack.push_back({&e.data->pages[e.page].commands, 0, -1, eventIndex});
}

bool Game::commonConditionMet(const CommonEvent &ce) {
    Thread ctx;
    ctx.stack.push_back({nullptr, 0, ce.id, -1});
    return compare(readVar(ce.conditionVariable, ctx), ce.conditionValue, ce.conditionOperator);
}

// ---------------------------------------------------------------------------------------------------------------
// Frame

void Game::update(const Input &input) {
    input_ = input;
    frame_++;

    // Choices take the keys while they are open.
    if (choice_.active && !choice_.visible.empty()) {
        int n = int(choice_.visible.size());
        if (input.pressed & KeyDown) choice_.cursor = (choice_.cursor + 1) % n;
        if (input.pressed & KeyUp) choice_.cursor = (choice_.cursor + n - 1) % n;
        if (input.pressed & KeyConfirm) {
            choice_.result = choice_.visible[choice_.cursor] + 1;
            choice_.active = false;
        } else if ((input.pressed & KeyCancel) && choice_.cancelCase >= 0) {
            choice_.result = choice_.cancelCase;
            choice_.active = false;
        }
    }
    if (message_.active && (input.pressed & (KeyConfirm | KeyCancel))) message_.active = false;

    // Autorun map events start once per page activation, one at a time, ahead of anything else.
    if (threads_.empty()) {
        for (size_t i = 0; i < events_.size(); i++) {
            auto &e = events_[i];
            if (e.page < 0 || e.ranAutorun) continue;
            auto &pg = e.data->pages[e.page];
            if (pg.trigger == 1 && pg.commands.size() > 1) {
                e.ranAutorun = true;
                Thread t;
                startPage(int(i), t);
                threads_.push_back(std::move(t));
                break;
            }
        }
    }
    if (threads_.empty()) {
        for (auto &ce : commons_) {
            if (ce.runCondition == 1 && ce.commands.size() > 1 && commonConditionMet(ce)) {
                Thread t;
                t.stack.push_back({&ce.commands, 0, ce.id, -1});
                threads_.push_back(std::move(t));
                break;
            }
        }
    }

    // Parallel common events run every frame while their condition holds.
    if (parallel_.size() != commons_.size()) parallel_.resize(commons_.size());
    for (size_t i = 0; i < commons_.size(); i++) {
        auto &ce = commons_[i];
        if (ce.runCondition < 2 || ce.commands.size() <= 1) continue;
        if (ce.runCondition == 2 && !threads_.empty()) continue;
        Thread &t = parallel_[i];
        if (t.done()) {
            if (!commonConditionMet(ce)) continue;
            t = Thread{};
            t.stack.push_back({&ce.commands, 0, ce.id, -1});
        }
        run(t);
    }

    if (!threads_.empty()) {
        run(threads_.front());
        if (threads_.front().done()) {
            threads_.erase(threads_.begin());
            refreshPages();
        }
    }

    for (auto &[n, p] : pictures_) {
        for (Tween *tw : {&p.x, &p.y, &p.opacity, &p.zoomX, &p.zoomY, &p.angle, &p.r, &p.g, &p.b}) tw->step();
    }
    tintR_.step();
    tintG_.step();
    tintB_.step();
    updateCamera();
}

// Keeps the hero in the middle of the screen, without showing past the map's edges.
void Game::updateCamera() {
    if (!map_) return;
    double tile = gameDat_.tileSize();
    double mapW = map_->width * tile, mapH = map_->height * tile;
    double x = (hero_.x + 0.5) * tile - screenWidth() / 2.0;
    double y = (hero_.y + 0.5) * tile - screenHeight() / 2.0;
    cameraX_ = mapW <= screenWidth() ? (mapW - screenWidth()) / 2 : std::clamp(x, 0.0, mapW - screenWidth());
    cameraY_ = mapH <= screenHeight() ? (mapH - screenHeight()) / 2 : std::clamp(y, 0.0, mapH - screenHeight());
}

bool Game::debugTeleport(int mapId, int x, int y) {
    hero_.x = x;
    hero_.y = y;
    if (!loadMap(mapId)) return false;
    updateCamera();
    return true;
}

void Game::run(Thread &t) {
    if (t.wait > 0) {
        t.wait--;
        return;
    }
    // A thread runs until it waits. The cap stops a loop without waits from hanging the frame.
    for (int budget = 0; budget < 100000 && !t.done(); budget++) {
        if (!step(t)) return;
    }
}

void Game::jumpToBranchEnd(Thread::Call &c, int indent) {
    const Commands &cmds = *c.commands;
    while (c.pc < cmds.size() && !(cmds[c.pc].indent == indent && cmds[c.pc].code == 499)) c.pc++;
    if (c.pc < cmds.size()) c.pc++;
}

// After a branching command, moves to the body of the case it chose, or past the branch.
bool Game::jumpToCase(Thread::Call &c, int indent, int caseValue) {
    const Commands &cmds = *c.commands;
    size_t pc = c.pc;
    for (; pc < cmds.size(); pc++) {
        auto &cmd = cmds[pc];
        if (cmd.indent != indent) continue;
        if (cmd.code == 499) break;
        bool match = (cmd.code == 401 && cmd.arg(0) == caseValue) || (caseValue == 0 && cmd.code == 420) ||
                     (caseValue == -2 && cmd.code == 421);
        if (match) {
            c.pc = pc + 1;
            return true;
        }
    }
    c.pc = pc < cmds.size() ? pc + 1 : pc;
    return false;
}

void Game::unsupported(const Command &c) {
    if (unsupported_[c.code]++ == 0) log(format("command %d is not supported yet", c.code));
}

bool Game::step(Thread &t) {
    Thread::Call &c = t.stack.back();
    if (t.waitingMessage) {
        if (message_.active) return false;
        t.waitingMessage = false;
    }
    if (t.waitingChoice) {
        if (choice_.active) return false;
        t.waitingChoice = false;
        jumpToCase(c, t.choiceIndent, choice_.result);
        return true;
    }
    if (c.pc >= c.commands->size()) {
        t.stack.pop_back();
        return true;
    }
    const Command &cmd = (*c.commands)[c.pc];
    int indent = cmd.indent;
    c.pc++;

    switch (cmd.code) {
        case 0:    // blank line
        case 99:   // checkpoint
        case 103:  // comment
        case 106:  // debug text
        case 107:  // clear debug text
        case 212:  // label
            return true;

        case 101:  // message: the thread resumes once the player closes the window
            message_.active = true;
            message_.text = expandText(cmd.str(0), &t);
            t.waitingMessage = true;
            return false;

        case 102: {  // choices
            int flags = cmd.arg(0);
            int count = flags & 0x0F;
            int cancel = (flags & 0xF0) >> 4;
            choice_ = ChoiceState{};
            choice_.active = true;
            for (int i = 0; i < count; i++) {
                std::string text = expandText(cmd.str(i), &t);
                choice_.items.push_back(text);
                if (!text.empty()) choice_.visible.push_back(i);
            }
            // 0 sends cancel to its own case, 1 ignores it, 2 and up pick a choice.
            choice_.cancelCase = cancel == 1 ? -1 : cancel == 0 ? -2 : cancel - 1;
            t.waitingChoice = true;
            t.choiceIndent = indent;
            return false;
        }

        case 111: {  // numeric condition
            int flags = cmd.arg(0);
            int count = flags & 0x0F;
            bool elseCase = (flags & 0xF0) != 0;
            int chosen = 0;
            for (int i = 0; i < count; i++) {
                int32_t left = readVar(cmd.arg(1 + i * 3), t);
                int32_t rflags = cmd.arg(3 + i * 3);
                int32_t right = (rflags & 0xF0) ? cmd.arg(2 + i * 3) : value(cmd.arg(2 + i * 3), t);
                if (compare(left, right, rflags & 0x0F)) {
                    chosen = i + 1;
                    break;
                }
            }
            if (chosen == 0 && !elseCase) chosen = -1;
            jumpToCase(c, indent, chosen);
            return true;
        }

        case 112: {  // string condition
            int flags = cmd.arg(0);
            int count = flags & 0x0F;
            bool elseCase = (flags & 0xF0) != 0;
            int chosen = 0;
            for (int i = 0; i < count; i++) {
                int32_t leftWord = cmd.arg(1 + i);
                int32_t leftId = leftWord & 0x00FFFFFF;
                if (leftId < 1000000) leftId += 0;
                int op = (leftWord >> 24) & 0xF0;
                bool rightIsVar = ((leftWord >> 24) & 0x0F) == 1;
                std::string left = readStr(leftId >= 1000000 ? leftId : 3000000 + leftId, t);
                std::string right = rightIsVar ? readStr(cmd.arg(1 + count + i), t) : cmd.str(i);
                bool ok = false;
                switch (op) {
                    case 0x00: ok = left == right; break;
                    case 0x10: ok = left != right; break;
                    case 0x20: ok = left.find(right) != std::string::npos; break;
                    case 0x30: ok = left.starts_with(right); break;
                }
                if (ok) {
                    chosen = i + 1;
                    break;
                }
            }
            if (chosen == 0 && !elseCase) chosen = -1;
            jumpToCase(c, indent, chosen);
            return true;
        }

        case 401:
        case 420:
        case 421:
            // Reaching another case of the same branch ends the case that ran.
            jumpToBranchEnd(c, indent);
            return true;
        case 499:
            return true;

        case 121:
            setVariable(t, cmd);
            return true;
        case 122:
            setString(t, cmd);
            return true;

        case 150:
            pictureCommand(t, cmd);
            return true;

        case 151: {  // change screen colour
            int32_t packed = cmd.arg(0);
            int frames = value(cmd.arg(1), t);
            // The colour is three percentages packed into one value, red in the lowest byte.
            tintR_.set(packed & 0xFF, frames);
            tintG_.set((packed >> 8) & 0xFF, frames);
            tintB_.set((packed >> 16) & 0xFF, frames);
            return true;
        }

        case 170:  // loop
            return true;
        case 179: {  // loop a number of times
            size_t at = c.pc - 1;
            auto it = t.loopCounts.find(at);
            if (it == t.loopCounts.end()) it = t.loopCounts.emplace(at, value(cmd.arg(0), t)).first;
            if (it->second-- <= 0) {
                t.loopCounts.erase(it);
                const Commands &cmds = *c.commands;
                while (c.pc < cmds.size() && !(cmds[c.pc].indent == indent && cmds[c.pc].code == 498)) c.pc++;
                if (c.pc < cmds.size()) c.pc++;
            }
            return true;
        }
        case 498: {  // loop end: back to the loop's start
            const Commands &cmds = *c.commands;
            size_t pc = c.pc - 1;
            while (pc > 0) {
                pc--;
                if (cmds[pc].indent == indent && (cmds[pc].code == 170 || cmds[pc].code == 179)) break;
            }
            c.pc = pc;
            return true;
        }
        case 171:    // break out of the innermost loop
        case 176: {  // go back to the start of the innermost loop
            const Commands &cmds = *c.commands;
            size_t pc = c.pc - 1;
            int depth = indent;
            while (pc > 0) {
                pc--;
                if (cmds[pc].indent < depth && (cmds[pc].code == 170 || cmds[pc].code == 179)) break;
                if (cmds[pc].indent < depth) depth = cmds[pc].indent;
            }
            int loopIndent = cmds[pc].indent;
            if (cmd.code == 176) {
                c.pc = pc;
                return true;
            }
            t.loopCounts.erase(pc);
            c.pc = pc + 1;
            while (c.pc < cmds.size() && !(cmds[c.pc].indent == loopIndent && cmds[c.pc].code == 498)) c.pc++;
            if (c.pc < cmds.size()) c.pc++;
            return true;
        }
        case 172:  // end this event
            t.stack.clear();
            return true;
        case 173:  // erase this event
            if (c.mapEvent >= 0) events_[c.mapEvent].erased = true;
            return true;
        case 180: {  // wait
            t.wait = value(cmd.arg(0), t);
            return t.wait <= 0;
        }

        case 213: {  // jump to label
            const Commands &cmds = *c.commands;
            const std::string &name = cmd.str(0);
            for (size_t i = 0; i < cmds.size(); i++) {
                if (cmds[i].code == 212 && cmds[i].str(0) == name) {
                    c.pc = i + 1;
                    return true;
                }
            }
            return true;
        }

        case 210:
        case 211: {  // call common event, or reserve it
            int id = value(cmd.arg(0), t);
            if (id >= 500000 && id < 600000) {
                callCommon(t, id - 500000, cmd, 1);
            } else {
                unsupported(cmd);  // map event pages are called the same way, once map events run
            }
            return true;
        }
        case 300: {
            int id = commonByName(cmd.str(0));
            if (id < 0) {
                log("no common event named " + cmd.str(0));
                return true;
            }
            callCommon(t, id, cmd, 1);
            return true;
        }

        case 221: {  // load one variable from a save slot
            // No saves exist yet in this spike, so every value reads as 0.
            writeVar(cmd.arg(0), 0, t);
            return true;
        }

        case 130: {  // move to another map or position
            int mapId = value(cmd.arg(1), t);
            hero_.x = value(cmd.arg(2), t);
            hero_.y = value(cmd.arg(3), t);
            if (cmd.arg(0) == -1 && mapId >= 0 && mapId != mapId_) loadMap(mapId);
            return true;
        }

        default:
            unsupported(cmd);
            return true;
    }
}

std::vector<const Character *> Game::characters() const {
    std::vector<const Character *> out{&hero_};
    for (auto &p : party_)
        if (p.visible && !p.graphic.empty()) out.push_back(&p);
    for (auto &e : events_)
        if (e.chara.visible) out.push_back(&e.chara);
    return out;
}

int Game::commonByName(const std::string &name) const {
    for (auto &ce : commons_)
        if (ce.name == name) return ce.id;
    return -1;
}

void Game::callCommon(Thread &t, int id, const Command &cmd, int argBase) {
    if (id < 0 || id >= int(cself_.size())) return;
    const CommonEvent *ce = nullptr;
    for (auto &e : commons_)
        if (e.id == id) ce = &e;
    if (!ce) return;
    int flags = cmd.arg(argBase);
    int numbers = flags & 0x0F;
    int strings = (flags >> 4) & 0x0F;
    int refFlags = (flags >> 8) & 0xFF;
    bool wantsReturn = ((flags >> 24) & 0xFF) == 1;
    std::array<int32_t, 100> &self = cself_[id];
    for (int i = 0; i < numbers && i < 5; i++) self[i] = value(cmd.arg(argBase + 1 + i), t);
    int next = argBase + 1 + numbers;
    for (int i = 0; i < strings && i < 5; i++) {
        if (refFlags & (1 << i)) cselfStr_[id][5 + i] = readStr(cmd.arg(next++), t);
        else cselfStr_[id][5 + i] = cmd.str(1 + i);
    }
    (void)wantsReturn;
    t.stack.push_back({&ce->commands, 0, id, -1});
}

void Game::setVariable(Thread &t, const Command &c) {
    int32_t target = c.arg(0);
    int32_t r1 = c.arg(1), r2 = c.arg(2);
    uint32_t flags = uint32_t(c.arg(3));
    uint8_t b0 = flags & 0xFF, b1 = (flags >> 8) & 0xFF;
    int calc = b1 & 0xF0, assign = b1 & 0x0F;
    if (b0 & 0x10) target = readVar(target, t);
    int32_t a = (b0 & 0x04) ? r1 : value(r1, t);
    if (b0 & 0x20) a = readVar(readVar(r1, t), t);
    int32_t b = (b0 & 0x08) ? r2 : value(r2, t);
    if (b0 & 0x40) b = readVar(readVar(r2, t), t);
    int32_t rhs = a;
    switch (calc) {
        case 0x00: rhs = a + b; break;
        case 0x10: rhs = a - b; break;
        case 0x20: rhs = a * b; break;
        case 0x30: rhs = b ? a / b : 0; break;
        case 0x40: rhs = b ? a % b : 0; break;
        case 0x50: rhs = a & b; break;
        case 0x60: rhs = std::uniform_int_distribution<int>(std::min(a, b), std::max(a, b))(rng_); break;
        case 0xF0: rhs = int32_t(std::lround(std::atan2(double(a), double(b)) * 1800 / M_PI)); break;
    }
    // Right side 2 of exactly zero with addition is how the editor writes "assign the first value".
    int32_t cur = assign == 0 ? 0 : readVar(target, t);
    int32_t out = cur;
    switch (assign) {
        case 0x00: out = rhs; break;
        case 0x01: out = cur + rhs; break;
        case 0x02: out = cur - rhs; break;
        case 0x03: out = cur * rhs; break;
        case 0x04: out = rhs ? cur / rhs : cur; break;
        case 0x05: out = rhs ? cur % rhs : cur; break;
        case 0x06: out = std::max(cur, rhs); break;
        case 0x07: out = std::min(cur, rhs); break;
        case 0x08: out = std::abs(rhs); break;
        case 0x09: out = int32_t(std::lround(std::atan2(double(rhs), 1.0) * 1800 / M_PI)); break;
        case 0x0A: out = int32_t(std::lround(std::sin(rhs * M_PI / 1800) * 1000)); break;
        case 0x0B: out = int32_t(std::lround(std::cos(rhs * M_PI / 1800) * 1000)); break;
    }
    writeVar(target, out, t);
}

void Game::setString(Thread &t, const Command &c) {
    int32_t target = c.arg(0);
    uint32_t flags = uint32_t(c.arg(1));
    int mode = flags & 0x0F;
    int assign = (flags >> 8) & 0x0F;
    if (flags & 0xF0) target = readVar(target, t);
    if (target < 1000000) target += 3000000;
    std::string rhs;
    switch (mode) {
        case 0: rhs = expandText(c.str(0), &t); break;
        case 1: rhs = readStr(c.arg(2), t); break;
        case 2: rhs = std::to_string(readVar(c.arg(2), t)); break;
        default: unsupported(c); return;
    }
    std::string cur = readStr(target, t);
    switch (assign) {
        case 0x00: writeStr(target, rhs, t); break;
        case 0x01: writeStr(target, cur + rhs, t); break;
        default: unsupported(c); break;
    }
}

void Game::pictureCommand(Thread &t, const Command &c) {
    uint32_t flags = uint32_t(c.arg(0));
    int display = flags & 0xF0;
    int exec = flags & 0x0F;
    int anchor = (flags >> 8) & 0xF0;
    int blend = (flags >> 8) & 0x0F;
    int number = value(c.arg(1), t);
    int frames = value(c.arg(2), t);

    if (exec == 2) {  // erase
        auto it = pictures_.find(number);
        if (it == pictures_.end()) return;
        if (frames <= 0) pictures_.erase(it);
        else it->second.opacity.set(0, frames);
        return;
    }

    Picture &p = pictures_[number];
    bool show = exec == 0;
    if (show) {
        Picture fresh;
        switch (display) {
            case 0x00: fresh.kind = PictureKind::File; fresh.source = c.str(0); break;
            case 0x10: fresh.kind = PictureKind::File; fresh.source = readStr(c.arg(11), t); break;
            case 0x20: fresh.kind = PictureKind::Text; fresh.source = expandText(c.str(0), &t); break;
            case 0x30: fresh.kind = PictureKind::Window; fresh.source = c.str(0); break;
            default: fresh.kind = PictureKind::Window; fresh.source = readStr(c.arg(11), t); break;
        }
        fresh.generation = ++generation_;
        fresh.x.set(value(c.arg(7), t), 0);
        fresh.y.set(value(c.arg(8), t), 0);
        fresh.opacity.set(0, 0);
        fresh.zoomX.set(100, 0);
        fresh.zoomY.set(100, 0);
        for (Tween *tw : {&fresh.r, &fresh.g, &fresh.b}) tw->set(100, 0);
        p = fresh;
    }
    p.anchor = anchor >> 4;
    p.blend = blend;
    p.divX = std::max(1, value(c.arg(3), t));
    p.divY = std::max(1, value(c.arg(4), t));
    p.pattern = value(c.arg(5), t);
    int over = show ? 0 : frames;
    p.opacity.set(value(c.arg(6), t), show ? frames : over);
    if (!show) {
        p.x.set(value(c.arg(7), t), over);
        p.y.set(value(c.arg(8), t), over);
    }
    int zoom = value(c.arg(9), t);
    p.zoomX.set(zoom, over);
    p.zoomY.set(c.args.size() > 18 ? value(c.arg(18), t) : zoom, over);
    p.angle.set(value(c.arg(10), t), over);
    if (c.args.size() > 17) {
        p.r.set(value(c.arg(15), t), over);
        p.g.set(value(c.arg(16), t), over);
        p.b.set(value(c.arg(17), t), over);
    }
}

}  // namespace wolf
