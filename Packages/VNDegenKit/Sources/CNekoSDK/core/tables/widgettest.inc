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
slider rose obj\k.bmp 100 500 50 10
range low 10 50
knob low obj\k.bmp 100 400 6
step low -1 obj\b.bmp 105 395 1 5 0
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
// and under its buttons, takes the presses it wants, and draws over everything (NekoSDKWidgetTests). Its Scene screen
// replays the chapter a play_view2.ini line names, `replaychapters`, as Test Game 96's does, and the part draws under its
// card, Layer::MenuUnder, as Test Game 90's Save and Load strip runs behind their cards. `replaykeeps` brings a replay
// back to the page its card was on. Its Music button is off by `buttonsoff`, and its second tab for having no scene.
const char kWidgetTestPart[] = R"(
screen defaults
parts selftest
buttonsoff nav_music
emptytabsoff

screen title
button sprites obj\b.bmp 0 0 1 5 0
button scene obj\b.bmp 600 0 1 5 0

screen sprites
part selftest
pic obj\d.bmp 380 401
button nav_title obj\b.bmp 0 0 1 5 0
button nav_load obj\b.bmp 405 405 1 5 0

screen music

screen scene
part selftest
pic obj\d.bmp 290 312
gallerytabs 0
replaychapters
replaykeeps
button page1 obj\b.bmp 700 0 1 5 0
button gtab0 obj\b.bmp 800 0 1 5 0
button gtab1 obj\b.bmp 900 0 1 5 0
button nav_music obj\b.bmp 1000 0 1 5 0
cell 300 300
cellsize 40 30
cellthumb 0 0
)";

// The log in the message window, `inwindow`, stepped by the window's BackText and NextText buttons (NekoSDKWindowLogTests).
const char kWindowLogTest[] = R"(
screen defaults
default bar_always 1

screen log
inwindow
)";

// Test Game 96's comments through the `yabai` part, with the window's own buttons always showing and its wait mark
// still, as 96's are (NekoSDKCommentTests).
const char kCommentTest[] = R"(
screen defaults
parts yabai
inneralways
markstill
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

// A game that saves in Test Game 83's layout with the `scroll` part on, for a scrolling picture's record through a load
// and a save (NekoSDKPartImageTests).
const char kScrollSaveTest[] = R"(
screen defaults
parts scroll
)";

const char kWidgetTestTabs[] = R"(
screen title
button scene obj\b.bmp 600 0 1 5 0

screen scene
gallerytabs 0
cell 300 300
cellsize 40 30
cellthumb 0 0
button gtab0 obj\b.bmp 800 0 1 5 0
button gtab1 obj\b.bmp 900 0 1 5 0
button back obj\b.bmp 700 0 1 5 0
)";

const char kOutfitTest[] = R"(
screen defaults
outfitswitch outfit 000:003:004 001:002:003

screen config
radio outfit 0 obj\b.bmp 100 100 1 5 0
radio outfit 1 obj\b.bmp 200 100 1 5 0
radio outfit 2 obj\b.bmp 300 100 1 5 0
button back obj\b.bmp 400 100 1 5 0
)";

const char kTwoByteTest[] = R"(
screen defaults
twobyteadvance
windowpic obj\e.bmp 1000 500
)";

const char kPartPlateTest[] = R"(
screen defaults
parts selftest
partplate

screen story
flow main
open selftestwait 9 -> S 1
end
)";

const char kPartBandTest[] = R"(
screen defaults
parts selftest

screen story
flow main
scene scr\s.txt
open selftestband 2 -> S 1
open selftestwait 9 -> S 2
end
)";

const char kPartPlateBandTest[] = R"(
screen defaults
parts selftest
partplate

screen story
flow main
scene scr\s.txt
open selftestband 9 -> S 1
end
)";

const char kDiscTest[] = R"(
screen title
button config obj\b.bmp 0 100 1 5 0 help obj\h.bmp 300 300 1 1 0 0 disc obj\bar.bmp 90
)";

const char kPlateHoldsTest[] = R"(
screen defaults
plateholds
)";

const char kMusicVolumeTest[] = R"(
screen title
button music obj\b.bmp 0 0 1 5 0

screen music
slider music obj\k.bmp 100 600
button back obj\b.bmp 700 0 1 5 0
)";
