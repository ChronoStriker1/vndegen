// Cheats for VNDegen's Cheats page, on the game's own variables and databases, so a save made afterwards is an ordinary
// save.
//
// Wolf RPG Editor has no gold, items or party of its own: games keep them in their databases. The standard systems that
// come with the editor, which most of the library's games build on, keep them in the changeable database: the hero
// statuses in type 0, the party and its gold in type 6, and how many of each item, weapon and armor the party has in
// types 7 to 9, with the items' names in the user database's types 2 to 4. A game that keeps its own layout gets the
// variables only.
#include "game.h"
#include "game_internal.h"

#include <algorithm>
#include <cstdio>

namespace wolf {

namespace {

enum : int {
    HeroStatus = 0,
    PartyInfo = 6,
    OwnedItems = 7,  // then weapons and armor
    ItemDatabase = 2,  // in the user database, then weapons and armor
    Gold = 0,          // PartyInfo's fields: gold, then the members
    FirstMember = 1,
    MemberCount = 6,
    HP = 6,  // HeroStatus's fields: HP, max HP, SP, max SP
    MaxHP = 7,
    SP = 8,
    MaxSP = 9,
};

constexpr int32_t kMaxGold = 999999999;
constexpr int32_t kMaxOwned = 99;

bool fieldNamed(const Database &db, int type, size_t field, const char *name) {
    return type < int(db.types.size()) && field < db.types[type].fieldNames.size() &&
           db.types[type].fieldNames[field] == name;
}

bool numberField(const Database &db, int type, size_t field) {
    return type < int(db.types.size()) && field < db.types[type].fieldKinds.size() && !db.types[type].isString(field);
}

size_t rowCount(const Database &db, int type) { return type < int(db.types.size()) ? db.types[type].rows.size() : 0; }

// dbNumberCell's ids give a row four digits.
constexpr int kMaxRows = 10000;

// A cell of the changeable database, as the variable number Wolf's commands use for it.
int32_t cdbId(int type, int row, int field) { return 1100000000 + type * kMillion + row * 100 + field; }

std::string json(const std::string &text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c < 0x20) {
            char escaped[8];
            std::snprintf(escaped, sizeof escaped, "\\u%04x", c);
            out += escaped;
        } else {
            out += char(c);
        }
    }
    return out + "\"";
}

}  // namespace

bool Game::hasStandardParty() const {
    if (!fieldNamed(cdb_, PartyInfo, Gold, "所持金") || !fieldNamed(cdb_, HeroStatus, HP, "HP") ||
        !fieldNamed(cdb_, HeroStatus, MaxHP, "最大HP") || !fieldNamed(cdb_, HeroStatus, SP, "SP") ||
        !fieldNamed(cdb_, HeroStatus, MaxSP, "最大SP") || rowCount(cdb_, PartyInfo) < 1)
        return false;
    // The rest of the layout the cheats use: numbers where they read and write numbers.
    for (int field = Gold; field < FirstMember + MemberCount; field++)
        if (!numberField(cdb_, PartyInfo, field)) return false;
    for (int field : {HP, MaxHP, SP, MaxSP})
        if (!numberField(cdb_, HeroStatus, field)) return false;
    for (int kind = 0; kind < 3; kind++)
        if (!numberField(cdb_, OwnedItems + kind, 0) || int(udb_.types.size()) <= ItemDatabase + kind) return false;
    return true;
}

std::string Game::cheatState() const {
    std::string out = "{\"inGame\":";
    out += mapId_ >= 0 ? "true" : "false";
    if (mapId_ < 0) return out + "}";
    if (hasStandardParty()) {
        out += ",\"gold\":" + std::to_string(dbNumber(cdb_, PartyInfo, 0, Gold));
        out += ",\"party\":[";
        bool first = true;
        for (int i = 0; i < MemberCount; i++) {
            int hero = dbNumber(cdb_, PartyInfo, 0, FirstMember + i);
            if (hero < 0 || hero >= int(rowCount(cdb_, HeroStatus))) continue;
            if (!first) out += ',';
            first = false;
            out += '[' + json(dbString(cdb_, HeroStatus, hero, 0));
            for (int field : {HP, MaxHP, SP, MaxSP}) out += ',' + std::to_string(dbNumber(cdb_, HeroStatus, hero, field));
            out += ']';
        }
        out += "],\"items\":[";
        first = true;
        static const char *kinds[] = {"item", "weapon", "armor"};
        for (int kind = 0; kind < 3; kind++) {
            auto &names = udb_.types[ItemDatabase + kind];
            int count = int(std::min(names.rows.size(), cdb_.types[OwnedItems + kind].rows.size()));
            for (int id = 0; id < count; id++) {
                std::string name = dbString(udb_, ItemDatabase + kind, id, 0);
                if (name.empty() && id < int(names.dataNames.size())) name = names.dataNames[id];
                if (name.empty()) continue;
                if (!first) out += ',';
                first = false;
                out += '[' + json(kinds[kind]) + ',' + std::to_string(id) + ',' + json(name) + ',' +
                       std::to_string(dbNumber(cdb_, OwnedItems + kind, id, 0)) + ']';
            }
        }
        out += ']';
    }
    // Normal variables, with the names the system database gives them.
    out += ",\"variables\":[";
    const std::vector<std::string> *names =
        SysNormalVarNames < int(sdb_.types.size()) ? &sdb_.types[SysNormalVarNames].dataNames : nullptr;
    for (size_t i = 0; i < normal_.size(); i++) {
        if (i) out += ',';
        out += '[' + std::to_string(i) + ',' + json(names && i < names->size() ? (*names)[i] : "") + ',' +
               std::to_string(normal_[i]) + ']';
    }
    return out + "]}";
}

bool Game::cheatAddGold(int amount) {
    if (mapId_ < 0 || !hasStandardParty()) return false;
    int32_t *gold = dbNumberCell(cdbId(PartyInfo, 0, Gold));
    if (!gold) return false;
    *gold = int32_t(std::clamp<int64_t>(int64_t(*gold) + amount, 0, kMaxGold));
    refreshPages();
    return true;
}

bool Game::cheatAddItem(int kind, int id, int count) {
    if (mapId_ < 0 || !hasStandardParty() || kind < 0 || kind > 2) return false;
    if (id < 0 || id >= int(std::min<size_t>(rowCount(cdb_, OwnedItems + kind), kMaxRows))) return false;
    int32_t *owned = dbNumberCell(cdbId(OwnedItems + kind, id, 0));
    if (!owned) return false;
    *owned = int32_t(std::clamp<int64_t>(int64_t(*owned) + count, 0, std::max(kMaxOwned, *owned)));
    refreshPages();
    return true;
}

bool Game::cheatRestoreParty() {
    if (mapId_ < 0 || !hasStandardParty()) return false;
    for (int i = 0; i < MemberCount; i++) {
        int hero = dbNumber(cdb_, PartyInfo, 0, FirstMember + i);
        if (hero < 0 || hero >= int(std::min<size_t>(rowCount(cdb_, HeroStatus), kMaxRows))) continue;
        for (auto [value, limit] : {std::pair{HP, MaxHP}, std::pair{SP, MaxSP}}) {
            int32_t *cell = dbNumberCell(cdbId(HeroStatus, hero, value));
            if (cell) *cell = std::max(*cell, dbNumber(cdb_, HeroStatus, hero, limit));
        }
    }
    refreshPages();
    return true;
}

bool Game::cheatSetVariable(int id, int value) {
    if (mapId_ < 0 || id < 0 || id >= int(normal_.size())) return false;
    normal_[id] = value;
    // Map event pages show by conditions on variables, which the game checks again when they change.
    refreshPages();
    return true;
}

void Game::setSpeed(int speed) {
    if (audio_) audio_->setFrameRate(gameDat_.fps() * std::max(1, speed));
}

}  // namespace wolf
