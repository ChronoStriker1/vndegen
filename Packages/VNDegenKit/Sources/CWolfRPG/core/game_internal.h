// Helpers shared by the parts of the Game implementation.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace wolf {

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
    SysFaces = 24,
};

constexpr int32_t kMillion = 1000000;

// A facing on the number pad, 1 to 9; anything else from a game's data or a save faces down.
inline int validDirection(int dir) { return dir >= 1 && dir <= 9 && dir != 5 ? dir : 2; }

inline bool compare(int32_t a, int32_t b, int op) {
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

inline std::string format(const char *fmt, auto... args) {
    char buf[512];
    std::snprintf(buf, sizeof buf, fmt, args...);
    return buf;
}


}  // namespace wolf
