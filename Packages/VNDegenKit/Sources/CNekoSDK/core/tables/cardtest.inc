// The runtime's own test game for save cards laid out as Test Game 82's are (NekoSDKCardFieldTests): the chapter by a
// scenario variable, the date's parts at places of their own, a frame around the thumbnail and spaced card numbers; and
// a Config number drawn as text (NekoSDKNumberTextTests); and a dialog whose question is a cell of a strip, over its
// frame (NekoSDKCardFieldTests).
const char kCardTest[] = R"(
screen title
button load obj\none.bmp 0 0 1 1 0
button config obj\none.bmp 0 0 1 1 0
button quit obj\none.bmp 0 0 1 1 0

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

screen config
number opacity text 200 100 20 15 255 255 255 76 10 73
)";
