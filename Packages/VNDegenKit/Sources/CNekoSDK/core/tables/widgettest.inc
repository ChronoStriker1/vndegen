// The runtime's own test game for shared table settings (NekoSDKWidgetTests): a title whose submenu dims the other
// buttons, Config with a knob slider, and waits scaled by a setting of their own.
const char kWidgetTest[] = R"(
screen defaults
waitscale autowait
default autowait 50

screen title
submenudim
button submenu:menu obj\b.bmp 0 0 1 5 0
button config obj\b.bmp 0 100 1 5 0
button start obj\b.bmp 0 400 1 5 0
button sub obj\b.bmp 200 0 1 5 0 group menu

screen config
range rose 0 50
knob rose obj\k.bmp 100 300 6
)";

// The same title with `submenudim hide`: the other buttons go while a submenu is open (Test Game 83's); a colour key, a
// knob with a step of 4.08 and a number without leading zeros, all Test Game 83's.
const char kWidgetTestHide[] = R"(
screen defaults
colorkey 0 255 0

screen title
submenudim hide
button submenu:menu obj\b.bmp 0 0 1 5 0
button config obj\b.bmp 0 100 1 5 0
button sub obj\b.bmp 200 0 1 5 0 group menu
pic obj\g.bmp 300 300

screen config
knob rose obj\k.bmp 100 300 4.08
number rose obj\d.bmp 600 400 15 blank
)";

// A sprite gallery naming a game part, `part selftest`, as Test Game 87's does: the part draws over the screen's pictures
// and under its buttons, takes the presses it wants, and draws over everything (NekoSDKWidgetTests).
const char kWidgetTestPart[] = R"(
screen defaults
parts selftest

screen title
button sprites obj\b.bmp 0 0 1 5 0

screen sprites
part selftest
pic obj\d.bmp 380 401
button nav_title obj\b.bmp 0 0 1 5 0
button nav_load obj\b.bmp 405 405 1 5 0
)";
