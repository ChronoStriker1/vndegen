// The runtime's own test game for the older layout as Test Games 82 and 87 have it (NekoSDKOldLayoutTests): a CG slot of
// its own, 82's, and 87's slots by a load's parameter 1 and five places, and 82's choices: columns of one sheet, and a box
// that closes.
const char kLayoutTest[] = R"(
screen defaults
layoutcgslot 102
layoutslotby p1
layoutslots 200 300 400 500 600
layoutx -512 -256 0 256 512

screen choice
choicecolumns 4
close 200 0.5
)";

// The older layout with a second record to each place, `layoutpairs`, as Test Game 96's PC saves hold a lit picture in
// the slot after each place's own (NekoSDKOldLayoutTests).
const char kLayoutPairsTest[] = R"(
screen defaults
layoutpairs
)";
