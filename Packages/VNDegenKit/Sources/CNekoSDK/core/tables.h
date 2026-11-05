// VNDegen's table of each game's menus, which the PC game keeps in code. Picture names and numbers only, measured from the
// PC game under Wine, with no game text.
#pragma once
#include <string>

namespace neko {

// The table for a game, by the normalized UserDataDirectory its init.ini names, or null when VNDegen doesn't have one yet.
const char *menuTable(const std::string &userDataDirectory);

}  // namespace neko
