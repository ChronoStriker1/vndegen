// The runtime's own test game for a screen's `band` (NekoSDKBandTests): a white title, its rows 10 to 20 multiplied by a
// colour that goes from none to 128, 64, 32.
const char kBandTest[] = R"(
screen title
pic obj\w.bmp 0 0
band 10 255 255 255
band 20 128 64 32
)";
