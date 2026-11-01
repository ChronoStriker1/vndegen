// Cheats for VNDegen's Cheats page, on the game's own globals, so a save made afterwards is an ordinary save.
//
// The VM runs on a thread of its own, so the cheats read and write the globals only while it waits in yieldFrame for
// the host's next step, holding the lock that step takes.
#include "game.h"
#include "sjis.h"
#include "vm.h"
#include "vns4.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace s4 {

namespace {

// The global that holds the party's money, in the library games whose globals say which one it is, by code file.
struct GoldGlobal {
    const char *codeFile;
    const char *global;  // UTF-8
};
constexpr GoldGlobal kGold[] = {
    {"rance02", "GOLD"},           // Rance 02
    {"rance7", "◆金"},             // Sengoku Rance
    {"evenicle", "g_nPartyMoney"},  // Evenicle
};

constexpr int32_t kMaxGold = 9999999;

// The globals the page edits as whole numbers: int, bool, and the 32-bit type 55 the v6 games use for counts.
bool wholeNumber(int32_t type) { return type == T_INT || type == T_BOOL || type == 55; }

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

std::string lower(std::string s) {
    for (auto &c : s) c = char(std::tolower((unsigned char)c));
    return s;
}

}  // namespace

// The VM thread's turn is checked first: until it hands back control, its state, globalPage included, is its own.
bool Game::cheatReady() const { return !vmTurn_ && !ended && vm && vm->globalPage >= 0; }

int Game::goldGlobal() const {
    for (const auto &g : kGold) {
        if (lower(codeStem_) != g.codeFile) continue;
        for (size_t i = 0; i < vm->ain.globals.size(); i++)
            if (wholeNumber(vm->ain.globals[i].t.type) && sjis::toUtf8(vm->ain.globals[i].name) == g.global) return int(i);
    }
    return -1;
}

std::string Game::cheatState() {
    std::lock_guard<std::mutex> lock(mu_);
    if (!cheatReady()) return {};
    const Page &globals = *vm->page(vm->globalPage);
    std::string out = "{\"inGame\":true";
    if (int gold = goldGlobal(); gold >= 0) out += ",\"gold\":" + std::to_string(globals.v[size_t(gold)]);
    out += ",\"variables\":[";
    bool first = true;
    for (size_t i = 0; i < vm->ain.globals.size() && i < globals.v.size(); i++) {
        const Global &g = vm->ain.globals[i];
        std::string value;
        if (wholeNumber(g.t.type)) {
            value = std::to_string(globals.v[i]);
        } else if (g.t.type == T_FLOAT) {
            float f;
            std::memcpy(&f, &globals.v[i], sizeof f);
            value = "{\"o\":" + json(std::to_string(f)) + "}";
        } else if (g.t.type == T_STRING) {
            value = "{\"o\":" + json("\"" + sjis::toUtf8(vm->str(globals.v[i])) + "\"") + "}";
        } else {
            continue;  // structs, arrays and references
        }
        if (!first) out += ',';
        first = false;
        out += '[' + std::to_string(i) + ',' + json(sjis::toUtf8(g.name)) + ',' + value + ']';
    }
    return out + "]}";
}

int Game::cheatSetGlobal(int index, int32_t value) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!cheatReady()) return VNS4_CHEAT_BUSY;
    if (index < 0 || size_t(index) >= vm->ain.globals.size() || !wholeNumber(vm->ain.globals[size_t(index)].t.type))
        return VNS4_CHEAT_NOT_NUMBER;
    // A bool takes 0 or 1 as it is, so the page shows what the game holds.
    if (vm->ain.globals[size_t(index)].t.type == T_BOOL && value != 0 && value != 1) return VNS4_CHEAT_BOOL;
    vm->page(vm->globalPage)->v[size_t(index)] = value;
    return VNS4_CHEAT_OK;
}

bool Game::cheatAddGold(int amount) {
    std::lock_guard<std::mutex> lock(mu_);
    if (!cheatReady()) return false;
    int gold = goldGlobal();
    if (gold < 0) return false;
    int32_t &cell = vm->page(vm->globalPage)->v[size_t(gold)];
    cell = int32_t(std::clamp<int64_t>(int64_t(cell) + amount, 0, std::max(kMaxGold, cell)));
    return true;
}

}  // namespace s4
