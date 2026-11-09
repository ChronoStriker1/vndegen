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

// The log in the message window, `inwindow`, stepped by the window's BackText and NextText buttons (NekoSDKWindowLogTests).
const char kWindowLogTest[] = R"(
screen defaults
default bar_always 1

screen log
inwindow
)";

// Test Game 96's comments through the `yabai` part (NekoSDKCommentTests).
const char kCommentTest[] = R"(
screen defaults
parts yabai
)";

// Test Game 96's confession tree, the `yabaitree` part, opened from a story table with its hub's variables set
// (NekoSDKTreeTests): truth 1, Misuzu shown at stage 3 with questions 0 and 2 of stage 1 and 1 and 2 of stage 2 asked.
const char kTreeTest[] = R"(
screen defaults
parts yabaitree

screen story
flow main
set S 15100 = 1
set S 15101 = 3
set S 15102 = 3
set S 15103 = 4
set S 15108 = 1
set S 15133 = 5
set S 15134 = 6
open tree -> S 15120
set S 15121 = 1
end
)";

// Test Game 96's vote, the `yabaivote` part, opened from a story table with the hub's clock set (NekoSDKVoteTests).
const char kVoteTest[] = R"(
screen defaults
parts yabaivote

screen story
flow main
resume S 105 == 8 at pick   # a PC save at the pick, which the PC's mode alone tells
set S 15104 = 21
set S 15105 = 1
label pick
open vote 1 -> S 15140
set S 15141 = 1
end
)";

// A game that saves in Test Game 83's layout with the test part on, for a part's own slot and its save record, and with
// flipbooks timed by command 121's parameter 16 and playing their ini line's sound, as Test Game 90's are
// (NekoSDKPartImageTests).
const char kPartSlotTest[] = R"(
screen defaults
parts selftest
fliptime
flipsound
)";
