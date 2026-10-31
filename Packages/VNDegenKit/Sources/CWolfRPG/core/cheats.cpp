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
        !fieldNamed(cdb_, HeroStatus, MaxSP, "最大SP"))
        return false;
    return int(cdb_.types.size()) > OwnedItems + 2 && int(udb_.types.size()) > ItemDatabase + 2;
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
            if (hero < 0 || hero >= int(cdb_.types[HeroStatus].rows.size())) continue;
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
    return true;
}

bool Game::cheatAddItem(int kind, int id, int count) {
    if (mapId_ < 0 || !hasStandardParty() || kind < 0 || kind > 2) return false;
    int32_t *owned = dbNumberCell(cdbId(OwnedItems + kind, id, 0));
    if (!owned) return false;
    *owned = int32_t(std::clamp<int64_t>(int64_t(*owned) + count, 0, std::max(kMaxOwned, *owned)));
    return true;
}

bool Game::cheatRestoreParty() {
    if (mapId_ < 0 || !hasStandardParty()) return false;
    for (int i = 0; i < MemberCount; i++) {
        int hero = dbNumber(cdb_, PartyInfo, 0, FirstMember + i);
        if (hero < 0) continue;
        for (auto [value, limit] : {std::pair{HP, MaxHP}, std::pair{SP, MaxSP}}) {
            int32_t *cell = dbNumberCell(cdbId(HeroStatus, hero, value));
            if (cell) *cell = std::max(*cell, dbNumber(cdb_, HeroStatus, hero, limit));
        }
    }
    return true;
}

bool Game::cheatSetVariable(int id, int value) {
    if (mapId_ < 0 || id < 0 || id >= int(normal_.size())) return false;
    normal_[id] = value;
    return true;
}

}  // namespace wolf
