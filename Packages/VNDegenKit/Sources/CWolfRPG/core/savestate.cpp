// Saving and loading: the save commands and the game state in Save/SaveDataNN.sav, as the PC engine writes them, so
// saves move between the PC and VNDegen both ways. docs/wolfrpg-save-format.md says how each field's meaning was
// established. Fields VNDegen doesn't model keep the values of the last save the session read or wrote, or the values
// the PC engine writes into a fresh save.
#include "game.h"
#include "game_internal.h"
#include "savefile.h"

#include "wolf_casepath.h"
#include "reader.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <strings.h>

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#else
#include <iconv.h>
#endif

namespace fs = std::filesystem;

namespace wolf {

namespace {

// Save versions the PC engine writes: Wolf RPG Editor 2.2x writes 125, 3.x writes 142, and 3.71 writes 159 (0x9F)
// in the 3.5 container. Newer engines read older versions, so a game whose own saves are unknown gets the older one
// unless its data is in the 3.x or 3.5 format.
constexpr uint16_t kVersion2 = 125, kVersion3 = 142, kVersion35 = 0x9F;

// UTF-8 to the game's encoding. Characters Shift-JIS lacks become '?'.
std::string encode(const std::string &utf8, Encoding enc) {
    if (enc == Encoding::UTF8 || std::all_of(utf8.begin(), utf8.end(), [](char c) { return (c & 0x80) == 0; }))
        return utf8;
#ifdef __APPLE__
    CFStringRef s = CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8 *>(utf8.data()), CFIndex(utf8.size()),
                                            kCFStringEncodingUTF8, false);
    if (!s) return utf8;
    CFRange all = CFRangeMake(0, CFStringGetLength(s));
    CFIndex used = 0;
    CFStringGetBytes(s, all, kCFStringEncodingDOSJapanese, '?', false, nullptr, 0, &used);
    std::string out(size_t(used), '\0');
    CFStringGetBytes(s, all, kCFStringEncodingDOSJapanese, '?', false, reinterpret_cast<UInt8 *>(out.data()), used,
                     &used);
    CFRelease(s);
    return out;
#else
    thread_local iconv_t cd = iconv_open("CP932", "UTF-8");
    iconv(cd, nullptr, nullptr, nullptr, nullptr);
    std::string out;
    char *in = const_cast<char *>(utf8.data());
    size_t inLeft = utf8.size();
    while (inLeft > 0) {
        char buf[256];
        char *o = buf;
        size_t oLeft = sizeof buf;
        size_t r = iconv(cd, &in, &inLeft, &o, &oLeft);
        out.append(buf, size_t(o - buf));
        if (r == size_t(-1) && errno != E2BIG) {
            // Skip the character that has no Shift-JIS form.
            out += '?';
            do {
                in++;
                inLeft--;
            } while (inLeft > 0 && (static_cast<unsigned char>(*in) & 0xC0) == 0x80);
        }
    }
    return out;
#endif
}

SaveString saveString(const std::string &utf8, Encoding enc) { return {encode(utf8, enc), true}; }

std::string text(const SaveString &s, Encoding enc) { return toUtf8(s.bytes, enc); }

// A character as the PC engine writes one with nothing special about it. The fields from version 0x9F hold what
// Wolf RPG Editor 3.71 writes for the hero and party; map events differ in v9fA and v9fD (snapshot sets those).
SaveCharacter defaultCharacter(int32_t id) {
    SaveCharacter c;
    c.v9fPairLists.resize(4);
    c.v9fD = 1;
    c.v9fFlag2 = 1;
    c.v9fE = {0, 1, 0, 0, 0, 0, 1, 0};
    c.id = id;
    c.unknown5 = -1;
    c.appearance = {255, 0, 2, 1};
    c.route.flags = {3, 3, 3, 0, 3, 2};
    c.route2.flags = {3, 3, 3, 0, 3, 2};
    c.unknown9 = {0, 3, 6, 2, 0, 0};
    c.v70 = {1, 0};
    c.v78 = 2;
    c.v8c = 131073;
    return c;
}

// The fields of a fresh save, as the PC engine writes one before any map is loaded (a version 142 save made by
// command 222 for a file that didn't exist).
SaveFile defaultSave(uint16_t version, bool utf8) {
    SaveFile f;
    f.version = version;
    f.map.mapId = -1;
    f.map.autotiles.resize(version >= 0x8A ? 31 : version >= 0x73 ? 15 : 7);
    f.map.v72 = {0, 0, 0, 0, 100, 100, 0, 0, 0, 0, 100};
    auto &s = f.system;
    s.b = {0, 0, -1, -1, 1065353216, 0, 0, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0};
    s.c = {0, 65535, 65535};
    s.e = {0, 65535, 65535};
    s.f = {12, -1, 0, 0, -1, 0, 0, 0, 255};
    s.i = {16, 5};
    s.j = {160, 68, 75, 200};
    s.k = {120, 1000000, 1};
    s.l = 1;
    s.v100a = {-1, 20};
    s.v100Values.assign(5, -1);
    s.v101 = {144, 16, 145, 32, 146, 64};
    s.v102 = 2;
    s.volumes = {100, 100, 100};
    s.v108a = {15, 0, 0};
    s.v109 = 1;
    s.v110 = 1;
    s.v122 = 1;
    s.v126 = 5;
    s.v129 = {0, 5};
    s.v132 = {100, 100, 0, 0};
    s.v134 = 1;
    s.v136 = 1;
    s.v8a = {0, 7, 7, -1};
    s.v8aStrings.resize(5);
    s.v8d.fill(1);
    s.v8dBytes.fill(1);
    s.v8d2 = {1, 1};
    s.v8e = 1;
    s.v9fValues = {0, 1, 0, 1};
    s.v9fCount = 1;
    f.party.hero = defaultCharacter(-2);
    for (int i = 0; i < 5; i++) f.party.members.push_back(defaultCharacter(-3 - i));
    f.variableDb.unknown = 111;
    // The header is random apart from the encoding mark at 6 and a zero at 8; serializeSave fills in the checksum.
    std::random_device rd;
    for (auto &b : f.header) b = uint8_t(rd());
    f.header[6] = utf8 ? 'U' : 0;
    f.header[8] = version >= kVersion35 ? 3 : 0;  // the 3.5 container (savefile.cpp)
    return f;
}

std::vector<int32_t> trimmed(std::vector<int32_t> v) {
    while (!v.empty() && v.back() == 0) v.pop_back();
    return v;
}

// A variable list as the engine sizes it: as long as the system database has names for that kind of variable, or
// longer to keep a value the game wrote past the end.
std::vector<int32_t> savedList(const std::vector<int32_t> &vars, const Database &sdb, int nameType) {
    size_t n = nameType < int(sdb.types.size()) ? sdb.types[nameType].rows.size() : 0;
    std::vector<int32_t> out = trimmed(vars);
    if (out.size() < n) out.resize(n);
    return out;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Files

// A save file a game names, such as "Save/System.sav", always inside the game folder: a name that is absolute or climbs
// out with ".." gives an empty path, which reads and writes nothing.
fs::path Game::saveFilePath(const std::string &name) const {
    std::string rel = name;
    std::replace(rel.begin(), rel.end(), '\\', '/');
    if (rel.empty() || rel[0] == '/' || rel.find(':') != std::string::npos) return {};
    for (size_t at = 0; at <= rel.size();) {
        size_t end = std::min(rel.find('/', at), rel.size());
        if (rel.compare(at, end - at, "..") == 0) return {};
        at = end + 1;
    }
    std::string full = (root_ / rel).string();
    std::vector<char> out(full.size() + 1024);
    fs::path path = casepath_resolve(full.c_str(), out.data(), out.size()) < 0 ? fs::path(full) : fs::path(out.data());
    // A folder on the way that is a link elsewhere mustn't take the file out of the game folder either.
    std::error_code e1, e2;
    fs::path real = fs::weakly_canonical(path, e1), base = fs::weakly_canonical(root_, e2);
    if (e1 || e2) return {};
    auto [r, b] = std::mismatch(real.begin(), real.end(), base.begin(), base.end());
    if (b != base.end()) return {};
    return path;
}

std::shared_ptr<SaveFile> Game::readSave(const fs::path &path, std::string *error) const {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error) *error = "no save at " + path.string();
        return nullptr;
    }
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
    try {
        return std::make_shared<SaveFile>(parseSave(bytes));
    } catch (const std::exception &e) {
        if (error) *error = path.string() + ": " + e.what();
        return nullptr;
    }
}

bool Game::writeSave(const fs::path &path, const SaveFile &save, std::string *error) {
    if (path.empty()) {
        if (error) *error = "save file name outside the game folder";
        return false;
    }
    try {
        std::vector<uint8_t> bytes = serializeSave(save);
        fs::create_directories(path.parent_path());
        // Written beside the slot and renamed over it, so a failed write leaves the old save as it was.
        fs::path tmp = path;
        tmp += ".tmp";
        // Whatever is at the temporary name goes first, so a link left there can't send the write elsewhere.
        fs::remove(tmp);
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
            if (!out) throw std::runtime_error("cannot write " + tmp.string());
        }
        fs::rename(tmp, path);
        casepath_changed(path.string().c_str());
        return true;
    } catch (const std::exception &e) {
        if (error) *error = e.what();
        log(std::string("save failed: ") + e.what());
        return false;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// Writing the state

int Game::playFrames() const { return loadedPlayFrames_ + int(frame_ - frameAtLoad_); }

int Game::playSeconds() const { return loadedPlaySeconds_ + int((frame_ - frameAtLoad_) / uint64_t(gameDat_.fps())); }

SaveFile Game::blankSave() {
    Encoding enc = gameDat_.encoding;
    uint16_t version = saveBase_ ? saveBase_->version
                       : wolf35_                                                    ? kVersion35
                       : (enc == Encoding::UTF8 || gameDat_.settings.size() > 21) ? kVersion3
                                                                                   : kVersion2;
    SaveFile f = defaultSave(version, enc == Encoding::UTF8);
    f.title = saveString(gameDat_.title, enc);
    // With no map loaded the engine still writes a 20 by 15 map of tile 100000.
    f.map.width = 20;
    f.map.height = 15;
    f.map.hasTiles = true;
    f.map.layers = 3;
    f.map.tiles.assign(3 * 20 * 15, 100000);
    f.variables.numbers.push_back(savedList({}, sdb_, SysNormalVarNames));
    for (int i = 0; i < 9; i++) f.variables.numbers.push_back(savedList({}, sdb_, SysSpare1 + i));
    f.variables.commonSelf.assign(cself_.size(), std::vector<int32_t>(100));
    f.variables.commonSelfStrings.assign(std::max<size_t>(1, cselfStr_.size()), std::vector<SaveString>(5));
    for (auto &m : f.party.members) m.hidden = 1;
    f.variables.tickCount = int32_t(frame_) + 1;
    return f;
}

SaveFile Game::snapshot() {
    Encoding enc = gameDat_.encoding;
    // Fields VNDegen doesn't model come from the last save the session saw; without one, from a save the game already
    // has, which the same engine wrote; else from the engine's defaults.
    if (!saveBase_) {
        std::error_code ec;
        fs::path dir = saveFilePath("Save");
        fs::file_time_type newest{};
        for (auto &e : fs::directory_iterator(dir, ec)) {
            std::string n = e.path().filename().string();
            if (n.size() < 8 || strncasecmp(n.c_str(), "SaveData", 8) != 0) continue;
            auto t = e.last_write_time(ec);
            if (saveBase_ && t <= newest) continue;
            if (auto s = readSave(e.path(), nullptr)) {
                saveBase_ = s;
                newest = t;
            }
        }
    }
    SaveFile f = saveBase_ ? *saveBase_ : blankSave();
    f.title = saveString(gameDat_.title, enc);
    f.header[6] = enc == Encoding::UTF8 ? 'U' : 0;
    // The XY arrays, which saves hold from version 0x9F.
    f.numberedArrays.clear();
    f.namedArrays.clear();
    for (auto &[number, rows] : xyNumbered_) f.numberedArrays.push_back({number, {}, rows});
    for (auto &[name, rows] : xyNamed_) f.namedArrays.push_back({0, saveString(name, enc), rows});
    std::random_device rd;
    // Fresh seeds, as each PC save has: at 0, 3 and 9 for the XOR, and in the 3.5 container the whole header but the
    // encoding mark and the container byte.
    if (f.header[8] == 3) {
        for (int i = 0; i < 20; i++)
            if (i != 6 && i != 8) f.header[i] = uint8_t(rd());
    } else {
        for (int i : {0, 3, 9}) f.header[i] = uint8_t(rd());
    }

    // Section 1: the map, its tiles and tileset, and its events.
    SaveMap &m = f.map;
    SaveMap old = std::move(m);
    m = SaveMap{};
    m.unknown1 = old.mapId == mapId_ ? old.unknown1 : std::array<int32_t, 2>{};
    m.v72 = old.v72;
    m.values2 = old.values2;
    m.mapId = map_ ? mapId_ : -1;
    m.autotiles.resize(f.version >= 0x8A ? 31 : f.version >= 0x73 ? 15 : 7);
    if (map_) {
        m.width = map_->width;
        m.height = map_->height;
        // System variable 127 asks the engine not to save the map's tiles.
        // Saves before version 0x9F hold three layers.
        m.layers = map_->layers;
        m.hasTiles = !(system_.size() > 127 && system_[127] == 1) && (f.version >= kVersion35 || m.layers == 3) &&
                     map_->tiles.size() == size_t(m.layers) * size_t(m.width) * size_t(m.height);
        if (!m.hasTiles && f.version < kVersion35) m.layers = 3;
        if (m.hasTiles) m.tiles = map_->tiles;
        if (map_->tileset >= 0 && map_->tileset < int(tilesets_.size())) {
            const Tileset &ts = tilesets_[map_->tileset];
            m.tilesetBase = saveString(ts.baseFile, enc);
            for (size_t i = 0; i < m.autotiles.size() && i < ts.autotileFiles.size(); i++)
                m.autotiles[i] = saveString(ts.autotileFiles[i], enc);
            m.tileFlags.assign(ts.passability.begin(), ts.passability.end());
        }
    }
    auto fill = [&](SaveCharacter &c, const Actor &a) {
        c.x = int32_t(std::lround(a.chara.x * 2));
        c.y = int32_t(std::lround(a.chara.y * 2));
        c.graphic = saveString(a.chara.graphic, enc);
        c.appearance = {uint16_t(a.chara.opacity), uint16_t(a.chara.blend), uint16_t(a.chara.direction),
                        uint16_t(a.chara.pattern)};
    };
    for (auto &e : events_) {
        SaveCharacter c = defaultCharacter(e.data->id);
        c.v9fA = {-1, -1};
        c.v9fD = 0;
        // Wolf RPG Editor 3.71 follows each map event with its name.
        c.v9fNoteFlag = 1;
        c.v9fNote = saveString(e.data->name, enc);
        for (auto &o : old.events)
            if (old.mapId == mapId_ && o.id == e.data->id) c = o;
        fill(c, e.actor);
        c.hidden = e.page < 0 || !e.actor.chara.visible;
        if (e.page >= 0) {
            const MapPage &pg = e.data->pages[e.page];
            c.route.flags = {pg.moveType, pg.moveSpeed, pg.moveFrequency, pg.animSpeed, pg.flags, pg.route.flags};
            c.unknown9 = {0, uint16_t(pg.moveSpeed), uint16_t(pg.moveFrequency), uint16_t(pg.animSpeed ? pg.animSpeed : 3), 0, 0};
        }
        c.route.steps.clear();
        c.route2.steps.clear();
        m.events.push_back(std::move(c));
    }

    // Section 2: what the system section is known to hold.
    SaveSystem &s = f.system;
    s.b[0] = playSeconds();
    for (int i = 0; i < 8 && i < int(system_.size()); i++) s.b[6 + i] = system_[i];
    s.frameCount = playFrames();
    for (int i = 0; i < 3 && 100 + i < int(system_.size()); i++) s.volumes[i] = system_[100 + i] ? system_[100 + i] : 100;

    // Section 3: the variables.
    SaveVariables &v = f.variables;
    v.tickCount = int32_t(frame_);
    v.eventSelf.clear();
    for (auto &[key, self] : eventSelf_) {
        auto [mapId, eventId] = key;
        if (mapId < 0 || eventId < 0 || mapId > 9999 || eventId > 9999) continue;
        if (v.eventSelf.size() <= size_t(mapId)) v.eventSelf.resize(mapId + 1);
        auto &evs = v.eventSelf[mapId];
        if (evs.size() <= size_t(eventId)) evs.resize(eventId + 1);
        evs[eventId] = trimmed({self.begin(), self.end()});
    }
    v.numbers.clear();
    v.numbers.push_back(savedList(normal_, sdb_, SysNormalVarNames));
    for (size_t i = 0; i < spare_.size(); i++) v.numbers.push_back(savedList(spare_[i], sdb_, SysSpare1 + int(i)));
    v.strings.clear();
    size_t usedStrings = strings_.size();
    while (usedStrings > 0 && strings_[usedStrings - 1].empty()) usedStrings--;
    for (size_t i = 0; i < usedStrings; i++) v.strings.push_back(saveString(strings_[i], enc));
    v.commonSelf.clear();
    v.commonSelfStrings.clear();
    for (size_t i = 0; i < cself_.size(); i++) {
        v.commonSelf.emplace_back(cself_[i].begin(), cself_[i].end());
        std::vector<SaveString> strs;
        for (int k = 5; k < 10; k++) strs.push_back(saveString(cselfStr_[i][k], enc));
        v.commonSelfStrings.push_back(std::move(strs));
    }
    if (v.commonSelfStrings.empty()) v.commonSelfStrings.emplace_back(5);

    // Section 4: the hero and the party.
    fill(f.party.hero, hero_);
    f.party.hero.hidden = 0;
    f.party.members.resize(party_.size(), defaultCharacter(0));
    for (size_t i = 0; i < party_.size(); i++) {
        SaveCharacter &c = f.party.members[i];
        c.id = -3 - int32_t(i);
        fill(c, party_[i]);
        c.hidden = party_[i].chara.graphic.empty() || !party_[i].chara.visible;
    }
    if (f.party.trail.size() < 100) f.party.trail.resize(100);

    // Section 5: pictures. The PC engine shows none of a save's pictures after loading it, and it crashed on pictures
    // written from VNDegen's state, so saves carry none, written as the engine writes an empty list.
    f.pictures = {};

    // Section 6: the CDB.
    f.variableDb.types.clear();
    for (auto &t : cdb_.types) {
        SaveDbType st;
        st.head = -2;
        st.idMethod = int32_t(t.idMethod);
        st.fieldKinds = t.fieldKinds;
        for (auto &row : t.rows) {
            SaveDbType::Row r;
            r.numbers = row.numbers;
            for (auto &str : row.strings) r.strings.push_back(saveString(str, enc));
            st.rows.push_back(std::move(r));
        }
        f.variableDb.types.push_back(std::move(st));
    }
    return f;
}

bool Game::saveSlot(int slot, std::string *error) {
    if (slot < 0) {
        if (error) *error = "bad slot";
        return false;
    }
    SaveFile f = snapshot();
    if (!writeSave(saveFilePath(format("Save/SaveData%02d.sav", slot)), f, error)) return false;
    saveBase_ = std::make_shared<SaveFile>(std::move(f));
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// Reading it back

bool Game::loadSlot(int slot, std::string *error) {
    auto s = readSave(saveFilePath(format("Save/SaveData%02d.sav", slot)), error);
    if (!s) return false;
    if (s->map.mapId < 0) {
        if (error) *error = "the save holds no map";
        return false;
    }
    pendingLoad_ = s;
    if (system_.size() > 25) system_[25] = slot;
    return true;
}

void Game::applyPendingLoad() {
    auto s = std::move(pendingLoad_);
    pendingLoad_ = nullptr;
    if (s) applySave(*s);
}

void Game::applySave(const SaveFile &s) {
    Encoding enc = s.utf8() ? Encoding::UTF8 : Encoding::ShiftJIS;
    // Every running event ends, as on the PC.
    threads_.clear();
    parallel_.clear();
    mapParallel_.clear();
    xyNumbered_.clear();
    xyNamed_.clear();
    for (auto &a : s.numberedArrays) xyNumbered_[a.key] = a.rows;
    for (auto &a : s.namedArrays) xyNamed_[toUtf8(a.name.bytes, enc)] = a.rows;
    textEntry_ = {};
    message_ = {};
    choice_ = {};
    shake_ = {};
    // The generation keeps counting, so the renderer never takes a frozen frame from before the load.
    transition_ = {.generation = transition_.generation + 1};
    scrollX_ = scrollY_ = 0;
    pictures_.clear();  // the PC engine shows no pictures after a load, not even those in the save
    autoKeys_ = 0;

    // Variables first: the map's event pages depend on them.
    const SaveVariables &v = s.variables;
    auto copyInto = [](std::vector<int32_t> &to, const std::vector<int32_t> &from) {
        if (to.size() < from.size()) to.resize(from.size());
        std::fill(to.begin(), to.end(), 0);
        std::copy(from.begin(), from.end(), to.begin());
    };
    if (!v.numbers.empty()) copyInto(normal_, v.numbers[0]);
    for (size_t i = 0; i < spare_.size(); i++) copyInto(spare_[i], i + 1 < v.numbers.size() ? v.numbers[i + 1] : std::vector<int32_t>{});
    std::fill(strings_.begin(), strings_.end(), std::string());
    if (strings_.size() < v.strings.size()) strings_.resize(v.strings.size());
    for (size_t i = 0; i < v.strings.size(); i++) strings_[i] = text(v.strings[i], enc);
    for (size_t i = 0; i < cself_.size(); i++) {
        cself_[i].fill(0);
        cselfStr_[i].fill({});
        if (i < v.commonSelf.size())
            for (size_t k = 0; k < 100 && k < v.commonSelf[i].size(); k++) cself_[i][k] = v.commonSelf[i][k];
        if (i < v.commonSelfStrings.size())
            for (size_t k = 0; k < 5 && k < v.commonSelfStrings[i].size(); k++)
                cselfStr_[i][5 + k] = text(v.commonSelfStrings[i][k], enc);
    }
    eventSelf_.clear();
    for (size_t mapId = 0; mapId < v.eventSelf.size(); mapId++)
        for (size_t ev = 0; ev < v.eventSelf[mapId].size(); ev++) {
            auto &vals = v.eventSelf[mapId][ev];
            if (vals.empty()) continue;
            auto &self = eventSelf_[{int(mapId), int(ev)}];
            for (size_t k = 0; k < self.size() && k < vals.size(); k++) self[k] = vals[k];
        }

    // The CDB, type by type where the save's fields match the game's.
    for (size_t i = 0; i < cdb_.types.size() && i < s.variableDb.types.size(); i++) {
        auto &t = cdb_.types[i];
        auto &st = s.variableDb.types[i];
        if (st.fieldKinds != t.fieldKinds) {
            log(format("save: CDB type %zu has other fields than the game's; kept the game's", i));
            continue;
        }
        t.rows.clear();
        for (auto &r : st.rows) {
            DatabaseType::Row row;
            row.numbers = r.numbers;
            for (auto &str : r.strings) row.strings.push_back(text(str, enc));
            t.rows.push_back(std::move(row));
        }
    }

    // System variables the save holds.
    auto setSys = [&](size_t i, int32_t value) {
        if (system_.size() <= i) system_.resize(i + 1);
        system_[i] = value;
    };
    for (int i = 0; i < 8; i++) setSys(i, s.system.b[6 + i]);
    for (int i = 0; i < 3; i++) setSys(100 + i, s.system.volumes[i]);
    setSys(28, s.system.frameCount);
    setSys(29, s.system.b[0]);
    setSys(24, 1);
    loadedPlaySeconds_ = s.system.b[0];
    loadedPlayFrames_ = s.system.frameCount;
    frameAtLoad_ = frame_;

    // The party's graphics are system strings 10 to 15.
    if (systemStrings_.size() < 16) systemStrings_.resize(16);
    systemStrings_[10] = text(s.party.hero.graphic, enc);
    for (size_t i = 0; i < 5; i++)
        systemStrings_[11 + i] = i < s.party.members.size() ? text(s.party.members[i].graphic, enc) : std::string();
    syncPartyGraphics();

    auto place = [](Actor &a, const SaveCharacter &c) {
        a.placeAt(c.x / 2, c.y / 2);
        a.chara.x = c.x / 2.0;
        a.chara.y = c.y / 2.0;
        a.chara.opacity = c.appearance[0];
        a.chara.blend = c.appearance[1];
        if (c.appearance[2]) a.chara.direction = validDirection(c.appearance[2]);
        a.chara.pattern = c.appearance[3];
        // A route a command gave doesn't survive a load; one the event's page gives, which loadMap installed, does.
        if (a.routeFromCommand) {
            a.route = nullptr;
            a.routeFromCommand = false;
        }
    };
    place(hero_, s.party.hero);
    for (size_t i = 0; i < party_.size() && i < s.party.members.size(); i++) place(party_[i], s.party.members[i]);

    // The map: its events take their pages from the variables restored above, then their saved positions.
    if (!loadMap(s.map.mapId)) log(format("save: cannot load map %d", s.map.mapId));
    loadedMap_ = mapId_;
    if (map_) {
        if (s.map.hasTiles && s.map.width == map_->width && s.map.height == map_->height &&
            s.map.tiles.size() == map_->tiles.size())
            map_->tiles = s.map.tiles;
        // A tileset changed by command 241 is saved as its contents; find it by its base image and passability.
        std::string base = text(s.map.tilesetBase, enc);
        for (size_t i = 0; i < tilesets_.size(); i++) {
            auto &ts = tilesets_[i];
            if (ts.baseFile == base && ts.passability.size() == s.map.tileFlags.size() &&
                std::equal(ts.passability.begin(), ts.passability.end(), s.map.tileFlags.begin(),
                           [](uint32_t a, int32_t b) { return a == uint32_t(b); })) {
                if (int(i) != map_->tileset && !(map_->tileset >= 0 && map_->tileset < int(tilesets_.size()) &&
                                                 tilesets_[map_->tileset].baseFile == base))
                    map_->tileset = int(i);
                break;
            }
        }
        for (auto &e : events_) {
            for (auto &c : s.map.events) {
                if (c.id != e.data->id) continue;
                place(e.actor, c);
                // A saved event shows nothing while its page would: it was erased.
                if (c.hidden && e.page >= 0) {
                    e.erased = true;
                    e.page = -1;
                    e.actor.chara.visible = false;
                }
                break;
            }
        }
    }

    saveBase_ = std::make_shared<SaveFile>(s);
    updateCamera();
    log(format("loaded a save: map %d, hero at %d,%d", mapId_, hero_.tx, hero_.ty));
}

// ---------------------------------------------------------------------------------------------------------------
// Variables in saves (commands 221 and 222)

// The slot argument is a slot number, or a string variable that names the file, such as "Save/System.sav".
fs::path Game::saveTarget(int32_t slotArg, const Thread &t) {
    if (isStringVar(slotArg)) return saveFilePath(readStr(slotArg, t));
    return saveFilePath(format("Save/SaveData%02d.sav", value(slotArg, t)));
}

void Game::readSavedVariable(const SaveFile *s, int32_t address, int32_t target, Thread &t) {
    Encoding enc = s && s->utf8() ? Encoding::UTF8 : Encoding::ShiftJIS;
    if (isStringVar(target)) {
        std::string str;
        if (s) {
            auto &v = s->variables;
            if (address >= 3000000 && address < 4000000 && size_t(address - 3000000) < v.strings.size())
                str = text(v.strings[address - 3000000], enc);
            else if (address >= 1000000000) {
                int type = (address / 1000000) % 100, row = (address / 100) % 10000, field = address % 100;
                if (address >= 1100000000 && address < 1200000000 && type < int(s->variableDb.types.size())) {
                    auto &st = s->variableDb.types[type];
                    if (row < int(st.rows.size()) && field < int(st.fieldKinds.size()) && st.fieldKinds[field] / 1000 != 1) {
                        size_t slot = st.fieldKinds[field] % 1000;
                        if (slot < st.rows[row].strings.size()) str = text(st.rows[row].strings[slot], enc);
                    }
                } else {
                    str = readStr(address, t);
                }
            }
        }
        writeStr(target, str, t);
        return;
    }
    int32_t value = 0;
    if (address == 9000024) value = s != nullptr;  // whether the save could be read
    else if (s) {
        auto &v = s->variables;
        auto at = [](const std::vector<int32_t> &l, size_t i) { return i < l.size() ? l[i] : 0; };
        if (address >= 2000000 && address < 2100000) value = v.numbers.empty() ? 0 : at(v.numbers[0], address - 2000000);
        else if (address >= 2100000 && address < 3000000) {
            size_t bank = 1 + (address - 2100000) / 100000;
            value = bank < v.numbers.size() ? at(v.numbers[bank], (address - 2100000) % 100000) : 0;
        } else if (address == 9000028) value = s->system.frameCount;
        else if (address == 9000029) value = s->system.b[0];
        else if (address >= 9000000 && address < 9000008) value = s->system.b[6 + address - 9000000];
        else if (address >= 9000100 && address < 9000103) value = s->system.volumes[address - 9000100];
        else if (address >= 15000000 && address < 16000000) {
            size_t ce = (address - 15000000) / 100;
            value = ce < v.commonSelf.size() ? at(v.commonSelf[ce], (address - 15000000) % 100) : 0;
        } else if (address >= 1100000000 && address < 1200000000) {
            int type = (address / 1000000) % 100, row = (address / 100) % 10000, field = address % 100;
            if (type < int(s->variableDb.types.size())) {
                auto &st = s->variableDb.types[type];
                if (row < int(st.rows.size()) && field < int(st.fieldKinds.size()) && st.fieldKinds[field] / 1000 == 1)
                    value = at(st.rows[row].numbers, st.fieldKinds[field] % 1000);
            }
        } else if (address >= 1000000000) {
            value = readVar(address, t);  // the user and system databases aren't saved
        } else {
            log(format("save: reading variable %d from a save is not supported", address));
        }
    }
    writeVar(target, value, t);
}

void Game::writeSavedVariable(SaveFile &s, int32_t address, int32_t source, Thread &t) {
    Encoding enc = s.utf8() ? Encoding::UTF8 : Encoding::ShiftJIS;
    auto &v = s.variables;
    auto put = [](std::vector<int32_t> &l, size_t i, int32_t x) {
        if (l.size() <= i) l.resize(i + 1);
        l[i] = x;
    };
    if (address >= 3000000 && address < 4000000) {
        size_t i = address - 3000000;
        if (v.strings.size() <= i) v.strings.resize(i + 1);
        v.strings[i] = saveString(isStringVar(source) ? readStr(source, t) : std::to_string(value(source, t)), enc);
        return;
    }
    int32_t x = value(source, t);
    if (address >= 2000000 && address < 2100000) {
        if (v.numbers.empty()) v.numbers.emplace_back();
        put(v.numbers[0], address - 2000000, x);
    } else if (address >= 2100000 && address < 3000000) {
        size_t bank = 1 + (address - 2100000) / 100000;
        if (v.numbers.size() <= bank) v.numbers.resize(bank + 1);
        put(v.numbers[bank], (address - 2100000) % 100000, x);
    } else if (address >= 15000000 && address < 16000000) {
        size_t ce = (address - 15000000) / 100;
        if (v.commonSelf.size() <= ce) v.commonSelf.resize(ce + 1, std::vector<int32_t>(100));
        put(v.commonSelf[ce], (address - 15000000) % 100, x);
    } else if (address >= 1100000000 && address < 1200000000) {
        int type = (address / 1000000) % 100, row = (address / 100) % 10000, field = address % 100;
        if (type < int(s.variableDb.types.size())) {
            auto &st = s.variableDb.types[type];
            if (row < int(st.rows.size()) && field < int(st.fieldKinds.size())) {
                size_t slot = st.fieldKinds[field] % 1000;
                if (st.fieldKinds[field] / 1000 == 1) put(st.rows[row].numbers, slot, x);
                else if (slot < st.rows[row].strings.size())
                    st.rows[row].strings[slot] =
                        saveString(isStringVar(source) ? readStr(source, t) : std::to_string(x), enc);
            }
        }
    } else if (address >= 9000100 && address <= 9000102) {
        s.system.volumes[size_t(address - 9000100)] = x;  // Two Beasts keeps its volume settings in its system save
    } else {
        log(format("save: writing variable %d into a save is not supported", address));
    }
}

// ---------------------------------------------------------------------------------------------------------------
// The commands

void Game::saveCommand(Thread &t, const Command &c) {
    switch (c.code) {
        case 220: {  // save or load: exec code in the low byte of the first argument, slot in the second
            int exec = c.arg(0) & 0xFF;
            int slot = value(c.arg(1), t);
            std::string error;
            if (exec == 0) {
                if (!saveSlot(slot, &error)) log("save failed: " + error);
            } else if (exec == 1) {
                if (!loadSlot(slot, &error)) {
                    log("load failed: " + error);
                    if (system_.size() > 24) system_[24] = 0;
                }
            } else {
                unsupported(c);
            }
            return;
        }
        case 221: {  // read a variable from a save: target, slot, address, address flag
            int32_t address = c.arg(3) & 1 ? value(c.arg(2), t) : c.arg(2);
            auto s = readSave(saveTarget(c.arg(1), t), nullptr);
            readSavedVariable(s.get(), address, c.arg(0), t);
            return;
        }
        case 222: {  // write a variable into a save: source, slot, address, address flag
            int32_t address = c.arg(3) & 1 ? value(c.arg(2), t) : c.arg(2);
            fs::path path = saveTarget(c.arg(1), t);
            auto s = readSave(path, nullptr);
            // A save that doesn't exist yet starts as an empty game, as the PC engine writes one.
            SaveFile f = s ? *s : blankSave();
            writeSavedVariable(f, address, c.arg(0), t);
            std::string error;
            if (!writeSave(path, f, &error)) log("save failed: " + error);
            return;
        }
    }
    unsupported(c);
}

}  // namespace wolf
