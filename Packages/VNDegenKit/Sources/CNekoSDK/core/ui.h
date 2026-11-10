// What the runtime draws around the story: the message window's buttons, which each game's msg_button.ini lists, and the
// menus, which its exe holds in code and so VNDegen holds as a table for each game (tables/, one file a game). Everything
// draws from the game's own art.
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
    int stage = 0;         // in a title with `menustages`, 2 for the menu a click brings up
    std::string group;     // shown only while the screen's submenu of this name, or one inside it, is open
    std::vector<int> needs;  // `if G,G...`: shown only once one of these global variables is set
    // `ifread SCRIPT` and `unlessread SCRIPT`: shown only once that script has been read to its end, or only until then.
    std::string ifRead, unlessRead;
    ImageRef help;         // shown while hovered
    int helpX = 0, helpY = 0, helpCols = 1, helpRows = 1, helpColumn = 0, helpRow = 0;
    ImageRef disc;
    std::string discPath;
    int discX = 0, discY = 0, discCols = 1, discRows = 1, discColumn = 0, discRow = 0, discAxis = 3;
    float discSpeed = 0;
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
    std::string group;  // shown only while the screen's submenu of this name, or one inside it, is open
    std::vector<int> needs;  // `if G,G...`: shown only once one of these global variables is set
    // `ifread SCRIPT` and `unlessread SCRIPT`: shown only once that script has been read to its end, or only until then.
    std::string ifRead, unlessRead;
    // `ifhover A,A...` and `unlesshover A,A...`: shown only while the pointer is over a button of one of those actions, or
    // only while it isn't, as Moral Orc's music screen shades the portrait of whoever speaks a control's help.
    std::vector<std::string> ifHover, unlessHover;
    // `more FILE,FILE...`: other pictures that can take this one's place when the screen opens. `pickby G` shows the
    // one global variable G numbers, and `randomif G,G...` one at random once any of those is set, as Test Game 94's
    // title picks its art. `variants` holds them all, this one first.
    std::vector<ImageRef> variants;
    int pickBy = -1;
    std::vector<int> randomIf;
    // `random`: one at random each time the screen opens, as Test Game 83's menus pick their room (set 3).
    bool randomAlways = false;
    bool overDim = false;
    // How far, in pixels, the picture moves against the pointer when it's at the screen's edge, across and down.
    float followX = 0, followY = 0;
    // `part X Y W H`: only that rectangle of the picture, drawn at the picture's place.
    int partX = 0, partY = 0, partW = 0, partH = 0;
    // Where a screen's `drift` line art draws among its pictures. It has no image of its own.
    bool drift = false;
    // `rise MS:DY,MS:DY...`: on a gallery tab, how far below its place the picture is that long after the tab opens,
    // between the points in a straight line, as Horny Elves and a Moral Orc's characters come up.
    std::vector<std::pair<int, int>> rise;
    // `pic dim 0 0 alpha N`: the screen behind it darkened by N of 256, as Test Game 94's main menu dims its title.
    bool dim = false;
    // `blink MS`: fades in and out, fully shown every MS, as Test Game 94's title prompt does.
    int blinkMs = 0;
    // `turn DEGREES`: turned about its own middle, that many degrees a second clockwise by the screen's clock (the other
    // way when negative), as the three lace circles of Test Game 83's title turn (set 3). `angle` is where it has got
    // to, in radians, set by the screen as it draws.
    float turn = 0, angle = 0;
    // `scale PERCENT` draws the picture at that share of its size, with X and Y its new top left. Each pixel is the mean
    // of the area it covers, which is what Test Game 90's PC shows for its one half-size gallery picture (set 1: 0.19 of
    // 255 from a PC shot, against 0.27 for bilinear and 0.57 for nearest). The picture is shrunk once, as the table is read.
    // A staff roll's layer, by milliseconds from its start: `move MS SPEED` moves it up SPEED pixels a second from then
    // (down for a negative SPEED), `clamp TOP BOTTOM` keeps its top between those, and `fadein MS LENGTH` and
    // `fadeout MS LENGTH` fade it in and out.
    int moveFrom = -1;
    float speed = 0;
    int clampTop = -1000000, clampBottom = 1000000;
    int fadeInAt = -1, fadeInMs = 0, fadeOutAt = -1, fadeOutMs = 0;
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
    // Whether any of its pictures turns, so the screen is drawn again each frame.
    bool turning = false;
    // Other lines of the table, by their first word, for the parts each kind of screen draws for itself.
    std::multimap<std::string, std::vector<std::string>> settings;
    // The same lines in the order the table gives them, first word included, for a block read in order: the story.
    std::vector<std::vector<std::string>> lines;
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
