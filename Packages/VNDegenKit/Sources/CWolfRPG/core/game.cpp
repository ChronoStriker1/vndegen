#include "game.h"
#include "game_internal.h"

#include "reader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <type_traits>

namespace fs = std::filesystem;

namespace wolf {

namespace {
// Wolf compares file names as Windows does: without regard to case, with either slash.
bool sameFileName(const std::string &a, const std::string &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
        char x = a[i] == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(a[i])));
        char y = b[i] == '\\' ? '/' : char(std::tolower(static_cast<unsigned char>(b[i])));
        if (x != y) return false;
    }
    return true;
}
}  // namespace


Game::Game(fs::path root) : root_(root), files_(root) {}

std::vector<uint8_t> Game::readData(const std::string &path) const {
    auto d = files_.read(path);
    if (!d) throw FormatError("missing " + path);
    return *d;
}

// The three databases, which games change as they run, as the files have them.
void Game::readDatabases() {
    Encoding enc = gameDat_.encoding;
    udb_ = parseDatabase(readData("BasicData/DataBase.project"), readData("BasicData/DataBase.dat"), enc);
    cdb_ = parseDatabase(readData("BasicData/CDataBase.project"), readData("BasicData/CDataBase.dat"), enc);
    sdb_ = parseDatabase(readData("BasicData/SysDataBase.project"), readData("BasicData/SysDataBase.dat"), enc);
}

bool Game::load(std::string *error) {
    try {
        // Encrypted files are decrypted first (data.h).
        auto basic = [&](const std::string &p, BasicFile kind) {
            auto d = readData(p);
            decryptBasicData(d, kind);
            return d;
        };
        gameDat_ = parseGameDat(basic("BasicData/Game.dat", BasicFile::GameDat));
        commons_ = parseCommonEvents(basic("BasicData/CommonEvent.dat", BasicFile::CommonEvent));
        readDatabases();
        tilesets_ = parseTilesets(basic("BasicData/TileSetData.dat", BasicFile::Other));
    } catch (const std::exception &e) {
        if (error) *error = e.what();
        return false;
    }
    audio_ = std::make_unique<Audio>(files_, 44100, true, log);
    audio_->setFrameRate(gameDat_.fps());
    if (!soundFont_.empty()) audio_->setSoundFontPath(soundFont_, soundFontFallback_);
    return startNewGame(error);
}

void Game::setSoundFont(std::string path, std::string fallback) {
    soundFont_ = std::move(path);
    soundFontFallback_ = std::move(fallback);
    if (audio_) audio_->setSoundFontPath(soundFont_, soundFontFallback_);
}

// Variables at their starts, the hero at the first start position, and the start map with its music.
bool Game::startNewGame(std::string *error) {
    auto rows = [&](int type) { return type < int(sdb_.types.size()) ? sdb_.types[type].rows.size() : 0; };
    normal_.assign(std::max<size_t>(rows(SysNormalVarNames), 100), 0);
    for (int i = 0; i < 9; i++) spare_[i].assign(std::max<size_t>(rows(SysSpare1 + i), 100), 0);
    system_.assign(std::max<size_t>(rows(SysSystemVarNames), 200), 0);
    // System variables the standard systems read to lay out their windows: 84 the old screen size code, 115 the
    // engine version times 100 (2.24 for Shift-JIS games and 3.28 for UTF-8 ones, as no Game.dat field is known to
    // hold it), 116 and 117 the screen size, 118 the tile size, and 119 the scale of a 320 by 240 game.
    {
        int w = gameDat_.screenWidth, h = gameDat_.screenHeight;
        system_[84] = w == 320 && h == 240 ? 0 : w == 640 && h == 480 ? 1 : w == 800 && h == 600 ? 2 : -1;
        system_[115] = gameDat_.encoding == Encoding::UTF8 ? 328 : 224;
        system_[116] = w;
        system_[117] = h;
        system_[118] = gameDat_.tileSize();
        system_[119] = w == 320 && h == 240 ? 2 : 1;
    }
    strings_.assign(std::max<size_t>(rows(SysStringVarNames), 100), "");
    systemStrings_.assign(std::max<size_t>(rows(SysSystemStrings), 100), "");
    int maxCommon = 0;
    for (auto &ce : commons_) maxCommon = std::max(maxCommon, ce.id + 1);
    cself_.assign(maxCommon, {});
    cselfStr_.assign(maxCommon, {});

    // The game starts at the first entry of the position list.
    int startMap = dbNumber(sdb_, SysPositions, 0, 0);
    hero_.placeAt(dbNumber(sdb_, SysPositions, 0, 1), dbNumber(sdb_, SysPositions, 0, 2));
    // System strings 10 to 15 name the hero's and the party members' character images.
    if (systemStrings_.size() > 10 && systemStrings_[10].empty()) systemStrings_[10] = gameDat_.startHeroGraphic;
    syncPartyGraphics();
    if (!loadMap(startMap)) {
        if (error) *error = format("cannot load start map %d", startMap);
        return false;
    }
    playMapMusic();
    tintR_.set(100, 0);
    tintG_.set(100, 0);
    tintB_.set(100, 0);
    return true;
}

// Returning to the title starts the game afresh, as the PC engine does: the databases as the files have them, every
// variable and picture cleared, and the start map again. The sound engine stays, since the host's audio thread holds
// it; its sounds stop.
void Game::returnToTitle() {
    pendingTitle_ = false;
    try {
        readDatabases();
    } catch (const std::exception &e) {
        log(std::string("cannot reread the databases: ") + e.what());
    }
    eventSelf_.clear();
    threads_.clear();
    parallel_.clear();
    mapParallel_.clear();
    pictures_.clear();
    events_.clear();
    map_.reset();
    mapId_ = loadedMap_ = -1;
    choice_ = {};
    message_ = {};
    hero_ = {};
    party_ = {};
    cameraX_ = cameraY_ = scrollX_ = scrollY_ = 0;
    moveDuringEvents_ = haltNonPicture_ = scrollLocked_ = false;
    autoKeys_ = 0;
    keyControl_ = 0;
    shake_ = {};
    mapZoom_ = {100, 100};
    mapZoomX_ = mapZoomY_ = {};
    // The generation keeps counting, so the renderer never takes an old frozen frame for a new transition.
    transition_ = {.generation = transition_.generation + 1};
    transitionFrames_ = 0;
    pendingLoad_.reset();
    loadedPlaySeconds_ = loadedPlayFrames_ = 0;
    frameAtLoad_ = frame_;
    if (audio_) {
        for (SoundKind k : {SoundKind::BGM, SoundKind::BGS}) audio_->stopMusic(k);
        audio_->stopAllSE();
    }
    std::string error;
    if (!startNewGame(&error)) log(error);
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

std::string Game::pauseIcon() const { return dbString(sdb_, SysWindows, 0, 0); }

int Game::pauseIconFrames() const { return std::max(1, dbNumber(sdb_, SysWindows, 0, 1)); }

std::string Game::faceGraphic(int n) const { return dbString(sdb_, SysFaces, n, 0); }

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
        if (!c.map || c.mapEvent < 0 || c.mapEvent >= int(c.map->events.size())) return nullptr;
        return &eventSelf_[{c.mapId, c.map->events[c.mapEvent].id}][id - 1100000];
    }
    if (id >= 1000000 && id < 1100000) return &eventSelf_[{mapId_, (id - 1000000) / 10}][(id - 1000000) % 10];
    return nullptr;
}

// 1 X TT DDDD FF: X picks the database (0 user, 1 changeable, 3 system), then type, row and field.
namespace {
template <class Cell>
Cell *dbCell(std::array<Database *, 3> dbs, int32_t id, bool wantString) {
    Database &db = *(id >= 1300000000 ? dbs[2] : id >= 1100000000 ? dbs[1] : dbs[0]);
    int type = (id / 1000000) % 100, row = (id / 100) % 10000, field = id % 100;
    if (type >= int(db.types.size())) return nullptr;
    DatabaseType &dt = db.types[type];
    if (row >= int(dt.rows.size()) || field >= int(dt.fieldKinds.size()) || dt.isString(field) != wantString)
        return nullptr;
    auto &cells = [&]() -> auto & {
        if constexpr (std::is_same_v<Cell, std::string>) return dt.rows[row].strings;
        else return dt.rows[row].numbers;
    }();
    size_t slot = dt.slot(field);
    return slot < cells.size() ? &cells[slot] : nullptr;
}
}  // namespace

int32_t *Game::dbNumberCell(int32_t id) { return dbCell<int32_t>({&udb_, &cdb_, &sdb_}, id, false); }

std::string *Game::dbStringCell(int32_t id) { return dbCell<std::string>({&udb_, &cdb_, &sdb_}, id, true); }

bool Game::isStringVar(int32_t id) const {
    if (id >= 3000000 && id < 4000000) return true;
    if (id >= 9900000 && id < 10000000) return true;
    if (id >= 1600005 && id < 1600010) return true;
    if (id >= 15000000 && id < 16000000) {
        int s = (id - 15000000) % 100;
        return s >= 5 && s < 10;
    }
    if (id >= 1000000000) {
        const Database &db = id >= 1300000000 ? sdb_ : id >= 1100000000 ? cdb_ : udb_;
        int type = (id / 1000000) % 100;
        return type < int(db.types.size()) && db.types[type].isString(id % 100);
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
    if (id >= 1000000000) {
        if (int32_t *cell = dbNumberCell(id)) *cell = v;
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
        if (i >= 10 && i <= 15) syncPartyGraphics();
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
    if (id >= 1000000000) {
        if (std::string *cell = dbStringCell(id)) *cell = s;
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
        // The text between the bracket at `open` and its partner, with codes inside expanded first, as in
        // \udb[0:\cself[0]:0]. Sets close to the partner bracket, or npos when there is none.
        size_t close = 0;
        auto argument = [&](size_t open) {
            int depth = 0;
            for (close = open; close < text.size(); close++) {
                if (text[close] == '[') depth++;
                else if (text[close] == ']' && --depth == 0) return expandText(text.substr(open + 1, close - open - 1), t);
            }
            close = std::string::npos;
            return std::string();
        };
        auto number = [&](size_t open) { return std::atoi(argument(open).c_str()); };
        if (code("cdb") || code("udb") || code("sdb")) {
            // \cdb[type:row:field], and the same for the user and system databases: the cell's text or number.
            std::string arg = argument(i + 4);
            if (close == std::string::npos) break;
            int type = 0, row = 0, field = 0;
            std::sscanf(arg.c_str(), "%d:%d:%d", &type, &row, &field);
            const Database &db = text[i + 1] == 'c' ? cdb_ : text[i + 1] == 'u' ? udb_ : sdb_;
            bool isString = type >= 0 && type < int(db.types.size()) && db.types[type].isString(field);
            out += isString ? dbString(db, type, row, field) : std::to_string(dbNumber(db, type, row, field));
            i = close;
        } else if (code("self")) {
            int n = number(i + 5);
            Thread copy = t ? *t : Thread{};
            if (t && !copy.stack.empty())
                out += std::to_string(const_cast<Game *>(this)->readVar(1100000 + n, copy));
            i = close;
        } else if (code("cself")) {
            int n = number(i + 6);
            if (t && !t->stack.empty() && t->stack.back().commonEvent >= 0 && n >= 0 && n < 100) {
                int ce = t->stack.back().commonEvent;
                out += (n >= 5 && n < 10) ? cselfStr_[ce][n] : std::to_string(cself_[ce][n]);
            }
            i = close;
        } else if (code("v")) {
            int n = number(i + 2);
            if (n >= 0 && n < int(normal_.size())) out += std::to_string(normal_[n]);
            i = close;
        } else if (code("s")) {
            int n = number(i + 2);
            if (n >= 0 && n < int(strings_.size())) out += strings_[n];
            i = close;
        } else if (code("sys")) {
            int n = number(i + 4);
            if (n >= 0 && n < int(system_.size())) out += std::to_string(system_[n]);
            i = close;
        } else if (code("sysS")) {
            int n = number(i + 5);
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
    // Waits on the old map's events end with them.
    auto dropMapWait = [&](Thread &t) {
        if (t.waitingRoute && t.waitingRoute != &hero_ && (t.waitingRoute < &party_.front() || t.waitingRoute > &party_.back()))
            t.waitingRoute = nullptr;
    };
    for (auto &t : threads_) dropMapWait(t);
    for (auto *list : {&parallel_, &mapParallel_})
        for (auto &t : *list) dropMapWait(t);
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
    playMapMusic();
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
            // A page condition on 1000000 to 1000009 is the event's own self variable, as 1100000 to 1100009 is:
            // Test Game 52's opening sets its self variable 0 and its next page waits on 1000000.
            int32_t var = pg.condVars[i];
            if (var >= 1000000 && var < 1000010) var += 100000;
            ok = compare(readVar(var, ctx), pg.condValues[i], op);
        }
        if (ok) return p;
    }
    return -1;
}

void Game::refreshPages() {
    for (size_t i = 0; i < events_.size(); i++) {
        Thread ctx;
        ctx.stack.push_back({nullptr, 0, -1, int(i), 0, map_, mapId_});
        int page = activePage(events_[i], ctx);
        if (page != events_[i].page) {
            auto &e = events_[i];
            e.page = page;
            e.ranAutorun = false;
            e.pageChanged = true;
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
                    a.route = std::shared_ptr<const MoveRoute>(map_, &pg.route);
                    a.routeStep = 0;
                    a.routeFromCommand = false;
                }
                // A page drawn as a map tile names no image, or (in Wolf RPG Editor 3.5) the map's own tileset image;
                // tile 0 draws nothing.
                bool tileImage = !pg.graphic.empty() && map_->tileset >= 0 && map_->tileset < int(tilesets_.size()) &&
                                 sameFileName(pg.graphic, tilesets_[map_->tileset].baseFile);
                c.graphic = tileImage ? std::string() : pg.graphic;
                c.chip = (pg.graphic.empty() || tileImage) && pg.chip > 0 ? pg.chip : -1;
                c.direction = validDirection(pg.direction);
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
    t.stack.push_back({&e.data->pages[e.page].commands, 0, -1, eventIndex, 0, map_, mapId_});
}

bool Game::commonConditionMet(const CommonEvent &ce) {
    Thread ctx;
    ctx.stack.push_back({nullptr, 0, ce.id, -1});
    return compare(readVar(ce.conditionVariable, ctx), ce.conditionValue, ce.conditionOperator);
}

// ---------------------------------------------------------------------------------------------------------------
// Frame

void Game::update(const Input &input) {
    lastPressed_ = input_.pressed;
    input_ = input;
    for (size_t k = 0; k < heldFrames_.size(); k++)
        heldFrames_[k] = ((input.held | autoKeys_) >> k) & 1 ? heldFrames_[k] + 1 : 0;
    frame_++;
    // A load requested last frame takes effect before anything runs (savestate.cpp).
    if (pendingLoad_) applyPendingLoad();
    if (pendingTitle_) returnToTitle();
    // System variables 28 and 29 count frames and seconds of play; saves keep them and clocks in games read them.
    if (system_.size() > 29) {
        system_[28]++;
        if (system_[28] % gameDat_.fps() == 0) system_[29]++;
    }

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
        input_.pressed &= ~(KeyConfirm | KeyCancel);
    }
    // Messages type one character a frame. The first confirm shows the rest at once, the next closes the window.
    if (message_.active) {
        if (message_.shown < message_.total) {
            if (input.pressed & (KeyConfirm | KeyCancel)) message_.shown = message_.total;
            else message_.shown++;
        } else if (input.pressed & (KeyConfirm | KeyCancel)) {
            message_.active = false;
        }
        // The press that finishes or closes a message is used up, so it doesn't also talk to the event in front.
        input_.pressed &= ~(KeyConfirm | KeyCancel);
    }

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
    // An event that starts on touching the hero also starts while the hero stands on it. The manual's specification
    // page has touch events checked every frame with the autorun ones, so a hero placed onto such an event starts it.
    if (threads_.empty() && !hero_.moving()) eventAt(hero_.tx, hero_.ty, 4, true);
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

    // Parallel common events run every frame while their condition holds, beside any other event, and stop where they
    // are once it doesn't (the manual's common event settings). Run condition 3, parallel always, ignores its condition.
    if (parallel_.size() != commons_.size()) parallel_.resize(commons_.size());
    for (size_t i = 0; i < commons_.size(); i++) {
        auto &ce = commons_[i];
        if (ce.runCondition < 2 || ce.commands.size() <= 1) continue;
        Thread &t = parallel_[i];
        if (ce.runCondition != 3 && !commonConditionMet(ce)) {
            t = Thread{};
            continue;
        }
        if (t.done()) {
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
        // An event whose page changed, even away and back, runs its page's script from the start: here between frames
        // rather than in refreshPages, which can run while this script does.
        if (t.done() || e.pageChanged) {
            t = Thread{};
            startPage(int(i), t);
            e.pageChanged = false;
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
    for (Tween *z : {&mapZoom_, &mapZoomX_, &mapZoomY_}) z->step();
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

void Game::syncPartyGraphics() {
    auto name = [&](size_t i) { return i < systemStrings_.size() ? systemStrings_[i] : std::string(); };
    hero_.chara.graphic = name(10);
    for (size_t i = 0; i < party_.size(); i++) {
        party_[i].chara.graphic = name(11 + i);
        party_[i].chara.visible = !party_[i].chara.graphic.empty();
    }
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
