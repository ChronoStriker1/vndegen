// The runtime's own test game for save cards laid out as Test Game 82's are (NekoSDKCardFieldTests): the chapter by a
// scenario variable, the date's parts at places of their own with a mark of the date's sheet between, a frame around the
// thumbnail, spaced card numbers and a comment button; and
// a Config number drawn as text (NekoSDKNumberTextTests); and a dialog whose question is a cell of a strip, over its
// frame, and a gallery card from Test Game 82's `ID,PICTURE` lines with a frame over its thumbnail, and title pictures
// that turn and that draw at half size, and a scene card from Test Game 90's five-field lines (NekoSDKCardFieldTests).
const char kCardTest[] = R"(
screen title
pic obj\spin.bmp 1100 600 turn 90
pic obj\half.bmp 40 40 scale 50
button load obj\none.bmp 0 0 1 1 0
button config obj\none.bmp 0 0 1 1 0
button quit obj\none.bmp 0 0 1 1 0
button cg obj\none.bmp 0 0 1 1 0
button scene obj\none.bmp 0 0 1 1 0

screen load
slots obj\c.bmp 60 99 510 119 5 2 1
slotrows 5
slotthumb 29 10
slotthumbframe obj\f.bmp 27 8
slotdigits obj\d.bmp 229 6 3 15
datedigits obj\d.bmp 0 0
datefield year 251 28 15
datefield month 347 28 15 blank
datefield day 406 28 15 blank
datemark 9 330 28
slotcommentbutton obj\f.bmp 200 60
slotchaptervar obj\n.bmp 350 6 3 5000

screen dialog
dim 0
pic obj\f.bmp 300 300
question obj\n.bmp 0 0 3 0
question obj\n.bmp 0 0 3 0
question obj\n.bmp 0 0 3 0
question obj\n.bmp 0 0 3 0
question obj\n.bmp 0 0 3 0
question obj\n.bmp 0 0 3 0
question obj\n.bmp 0 0 3 1
question obj\n.bmp 310 310 3 2

screen cg
cell 100 100
cellthumb 10 10
cellthumbframe obj\f.bmp 8 8
cellsize 60 50
button nav_scene obj\f.bmp 600 600 1 1 0

screen scene
cell 300 300
cellthumb 0 0

screen config
number opacity text 200 100 20 15 255 255 255 76 10 73
)";

const char kEmptyCardTest[] = R"(
screen title
button load obj\none.bmp 0 0 1 1 0
button config obj\none.bmp 0 0 1 1 0
button quit obj\none.bmp 0 0 1 1 0
button cg obj\none.bmp 0 0 1 1 0
button scene obj\none.bmp 0 0 1 1 0

screen load
slots obj\c.bmp 60 99 510 119 5 2 1
slotrows 5
slotdigits obj\d.bmp 229 6 3 15
)";

const char kViewerRestsTest[] = R"(
screen title
button cg obj\none.bmp 0 0 1 1 0

screen cg
cell 100 100
cellsize 60 50
cellthumb 10 10
viewer obj\p.bmp 0 620
viewerclicks
viewerrests 500 360
viewerbutton return obj\t.bmp 900 650 1 0
)";
