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
    int tab = -1;  // a gallery's art for one of its tabs, or -1 for all of them
    int stage = 0;  // in a title with an opening, 1 for the art the tiles reveal and 2 for what fades in after
    // How far, in pixels, the picture moves against the pointer when it's at the screen's edge, across and down.
    float followX = 0, followY = 0;
    // `part X Y W H`: only that rectangle of the picture, drawn at the picture's place.
    int partX = 0, partY = 0, partW = 0, partH = 0;
    // Where a screen's `drift` line art draws among its pictures. It has no image of its own.
    bool drift = false;
};

// `towardX` and `towardY` are where the pointer is, from -1 at the screen's left or top edge to 1 at the other.
void drawPicture(Canvas &out, const Picture &p, float towardX = 0, float towardY = 0);
// `withHelp` shows the button's help whatever its state, as for the button whose question is open.
void drawButton(Canvas &out, const Button &b, ButtonState state, bool withHelp = false);
// A button's help alone, for a screen that draws it after all of its buttons so none covers it.
void drawButtonHelp(Canvas &out, const Button &b);

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

// Reads a game's menu table. Lines: `screen NAME`, `pic PATH X Y [COLS ROWS COL ROW] [alpha N] [tab N] [stage N] [follow X Y]`,
// `button ACTION PATH X Y COLS ROWS COL [toggle] [help PATH X Y COLS ROWS COL ROW]`, and any other line a setting of the
// screen it's in. The first `drift` line also marks where the drifting line art draws among the pictures.
std::map<std::string, Screen> parseScreens(const char *table, Images &images, const std::function<void(const std::string &)> &warn);

// The message window's buttons, from msg_button.ini: `#under` ones show while the pointer's at the bottom of the screen, and
// `#inner` ones with the window.
struct MessageButtons {
    std::vector<Button> under, inner;
};
MessageButtons parseMessageButtons(const std::vector<uint8_t> &ini, Images &images);

}  // namespace neko
