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
