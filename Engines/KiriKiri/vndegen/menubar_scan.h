// Finds where a game's script asks whether the window's menu bar is hidden. A game that does usually puts its menu
// commands on screen when it is, so the engine tells it so. Without such a test the engine reports the menu bar as
// shown, so the game builds its commands there and VNDegen lists them in its Game menu.
#ifndef VNKK_MENUBAR_SCAN_H
#define VNKK_MENUBAR_SCAN_H

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

struct VNKKMenuBarTest {
    // 1-based line in the script.
    size_t line;
    // How the script tests it, such as "negation" or "compares with false".
    const char *rule;
    // The line, trimmed and cut to a readable length, for the log.
    std::string excerpt;
};

// The first test of `menu.visible` for false in a script's text, such as `kag.menu.visible == false`,
// `!kag.menu.visible`, or `menu.visible != true`. Other objects' menus, such as `storeMenu.visible`, don't count, and
// neither do KAG comment lines, which start with a semicolon, or lines that start with `//`.
std::optional<VNKKMenuBarTest> VNKKFindHiddenMenuBarTest(std::string_view script);

#endif
