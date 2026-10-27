#include "game.h"
#include "game_internal.h"

#include "reader.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace fs = std::filesystem;

namespace wolf {


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
    hero_.placeAt(dbNumber(sdb_, SysPositions, 0, 1), dbNumber(sdb_, SysPositions, 0, 2));
    hero_.chara.graphic = gameDat_.startHeroGraphic;
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
        int ev = c.map == map_ ? c.mapEvent : -1;
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
        map_ = std::make_shared<Map>(parseMap(*data));
    } catch (const std::exception &e) {
        log("map " + file + ": " + e.what());
        return false;
    }
    mapId_ = id;
    events_.clear();
    for (auto &ev : map_->events) {
        MapEventState s;
        s.data = &ev;
        s.actor.placeAt(ev.x, ev.y);
        s.actor.chara.visible = false;
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
            Actor &a = e.actor;
            Character &c = a.chara;
            c.visible = page >= 0;
            a.route = nullptr;
            if (page >= 0) {
                const MapPage &pg = e.data->pages[page];
                a.speed = pg.moveSpeed;
                a.frequency = pg.moveFrequency;
                a.animSpeed = pg.animSpeed;
                // Page option bits: 0x01 idle animation, 0x02 walk animation, 0x04 fixed direction, 0x08 through,
                // 0x10 drawn above.
                a.idleAnimation = pg.flags & 0x01;
                a.walkAnimation = pg.flags & 0x02;
                a.fixedDirection = pg.flags & 0x04;
                a.through = pg.flags & 0x08;
                c.above = pg.flags & 0x10;
                if (pg.moveType == 1 && !pg.route.steps.empty()) {
                    a.route = &pg.route;
                    a.routeStep = 0;
                    a.routeFromCommand = false;
                }
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
    t.stack.push_back({&e.data->pages[e.page].commands, 0, -1, eventIndex, 0, map_});
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

    // Parallel map event pages.
    if (mapParallel_.size() != events_.size()) mapParallel_.assign(events_.size(), Thread{});
    for (size_t i = 0; i < events_.size(); i++) {
        auto &e = events_[i];
        if (e.page < 0 || e.data->pages[e.page].trigger != 2) continue;
        Thread &t = mapParallel_[i];
        if (t.done()) {
            t = Thread{};
            startPage(int(i), t);
        }
        run(t);
        if (mapId_ != loadedMap_) break;  // the event moved the hero to another map
    }

    if (!threads_.empty()) {
        run(threads_.front());
        if (!threads_.empty() && threads_.front().done()) {
            threads_.erase(threads_.begin());
            refreshPages();
        }
    }
    loadedMap_ = mapId_;

    updateCharacters();

    updatePictures();
    if (shake_.frames > 0) {
        shake_.frames--;
        shake_.offset = shake_.frames ? std::sin(frame_ * 0.3 * shake_.speed) * shake_.power * 2 : 0;
    }
    if (transition_.frames > 0 && ++transition_.elapsed >= transition_.frames) transition_.frames = 0;
    autoKeys_ = 0;
    tintR_.step();
    tintG_.step();
    tintB_.step();
    updateCamera();
}

void Game::updatePictures() {
    for (auto &[n, p] : pictures_) {
        for (Tween *tw : {&p.x, &p.y, &p.opacity, &p.zoomX, &p.zoomY, &p.angle, &p.r, &p.g, &p.b}) tw->step();
        if (p.autoPatternMode && ++p.autoTimer >= p.autoInterval) {
            p.autoTimer = 0;
            int next = p.pattern + p.autoDir;
            int lo = std::min(p.autoFrom, p.autoTo), hi = std::max(p.autoFrom, p.autoTo);
            if (next < lo || next > hi) {
                if (p.autoPatternMode == 1) p.autoPatternMode = 0;
                else if (p.autoPatternMode == 2) next = p.autoFrom;
                else {
                    p.autoDir = -p.autoDir;
                    next = p.pattern + p.autoDir;
                }
            }
            if (p.autoPatternMode) p.pattern = next;
        }
        if (p.shakeFrames > 0) {
            p.shakeFrames--;
            p.shakeOffset = p.shakeFrames ? std::sin(frame_ * 0.3 * std::max(1, p.shakeSpeed)) * p.shakePower : 0;
        }
        if (p.flickerFrames > 0) p.flickerFrames--;
    }
    std::erase_if(pictures_, [](auto &kv) {
        // A picture faded out by an erase with a duration goes once it is invisible.
        return kv.second.erasing && kv.second.opacity.value() <= 0;
    });
}

// Keeps the hero in the middle of the screen, without showing past the map's edges.
void Game::updateCamera() {
    if (!map_) return;
    double tile = gameDat_.tileSize();
    double mapW = map_->width * tile, mapH = map_->height * tile;
    double x = (hero_.chara.x + 0.5) * tile - screenWidth() / 2.0;
    double y = (hero_.chara.y + 0.5) * tile - screenHeight() / 2.0;
    cameraX_ = mapW <= screenWidth() ? (mapW - screenWidth()) / 2 : std::clamp(x, 0.0, mapW - screenWidth());
    cameraY_ = mapH <= screenHeight() ? (mapH - screenHeight()) / 2 : std::clamp(y, 0.0, mapH - screenHeight());
}

bool Game::debugTeleport(int mapId, int x, int y) {
    hero_.placeAt(x, y);
    if (!loadMap(mapId)) return false;
    updateCamera();
    return true;
}

}  // namespace wolf
