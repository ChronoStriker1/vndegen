// What the runtime draws around the story: the message window's buttons, which each game's msg_button.ini lists, and the
// menus, which its exe holds in code and so VNDegen holds as a table for each game (tables.cpp). Everything draws from
// the game's own art.
#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "canvas.h"
#include "image.h"

namespace neko {

// Sheets have a column per button and a row per state.
enum ButtonState { StateNormal, StateHover, StateSelected, StateSelectedHover, StateDisabled };

struct Button {
    std::string action;
    ImageRef sheet;
    int x = 0, y = 0;      // where the cell draws
    int cols = 1, rows = 5, column = 0;
    int normalRow = 0, hoverRow = 1;
    bool toggle = false;   // stays down until pressed again
    bool always = false;   // reacts even when `disabled` asks otherwise
    ImageRef help;         // shown while hovered
    int helpX = 0, helpY = 0, helpCols = 1, helpRows = 1, helpColumn = 0, helpRow = 0;
    int cellW() const { return sheet ? sheet->w / cols : 0; }
    int cellH() const { return sheet ? sheet->h / rows : 0; }
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + cellW() && py < y + cellH(); }
};

struct Picture {
    ImageRef image;
    int x = 0, y = 0;
    int cols = 1, rows = 1, column = 0, row = 0;
    float alpha = 1;
};

void drawPicture(Canvas &out, const Picture &p);
void drawButton(Canvas &out, const Button &b, ButtonState state);

// A menu screen from a game's table: pictures and buttons, drawn in order, and where the screen's own parts go.
struct Screen {
    std::string name;
    std::vector<Picture> pictures;
    std::vector<Button> buttons;
    // Other lines of the table, by their first word, for the parts each kind of screen draws for itself.
    std::multimap<std::string, std::vector<std::string>> settings;
    const std::vector<std::string> *setting(const std::string &key) const {
        auto it = settings.find(key);
        return it == settings.end() ? nullptr : &it->second;
    }
};

// Reads a game's menu table. Lines: `screen NAME`, `pic PATH X Y [COLS ROWS COL ROW]`,
// `button ACTION PATH X Y COLS ROWS COL [toggle] [help PATH X Y COLS ROWS COL ROW]`, and any other line a setting of the
// screen it's in.
std::map<std::string, Screen> parseScreens(const char *table, Images &images, const std::function<void(const std::string &)> &warn);

// The message window's buttons, from msg_button.ini: `#under` ones show while the pointer's at the bottom of the screen, and
// `#inner` ones with the window.
struct MessageButtons {
    std::vector<Button> under, inner;
};
MessageButtons parseMessageButtons(const std::vector<uint8_t> &ini, Images &images);

}  // namespace neko
