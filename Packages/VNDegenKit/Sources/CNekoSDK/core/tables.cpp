#include "tables.h"

// Each game's table is its own file under tables/, so the games can be worked on apart. A table is picture names and
// numbers measured from the PC game under Wine, in the lines the runtime's screens read.
namespace neko {
namespace {

#include "tables/christmas.inc"
#include "tables/alchemist.inc"
#include "tables/mother3.inc"
#include "tables/tsumagibo.inc"
#include "tables/funbagfantasy3if.inc"
#include "tables/funbag4.inc"
#include "tables/moralorc.inc"
#include "tables/animother.inc"
#include "tables/rinjin.inc"
#include "tables/funbag2.inc"
#include "tables/sideboob2.inc"
#include "tables/funbagfantasy1.inc"
#include "tables/yabai.inc"
#include "tables/storytest.inc"
#include "tables/layouttest.inc"
#include "tables/cardtest.inc"
#include "tables/bandtest.inc"
#include "tables/widgettest.inc"

}  // namespace

std::string &menuTablePrefix() {
    static std::string prefix;
    return prefix;
}

const char *menuTable(const std::string &folder) {
    if (folder == "waffle\\x2019_retry") return kChristmas;
    if (folder == "waffle\\alchemy") return kAlchemist;
    if (folder == "waffle\\mother3") return kMother3;
    if (folder == "waffle\\tmgb") return kTsumaGibo;
    if (folder == "waffle\\funbagfantasy3") return kFunbagFantasy;
    if (folder == "waffle\\funbag4") return kFunbag4;
    if (folder == "waffle\\moralorc") return kMoralOrc;
    if (folder == "waffle\\animother") return kAniMother;
    if (folder == "waffle\\隣人に壊されていく俺の妻") return kRinjin;
    if (folder == "waffle\\funbagfantasy2") return kFunbag2;
    if (folder == "waffle\\funbag_fantasy_sideboob_story_2") return kSideboob2;
    if (folder == "waffle\\funbagfantasy") return kFunbagFantasy1;
    if (folder == "waffle\\ヤバい！－復讐・闇サイト－") return kYabai1;
    if (folder == "vndegen\\storytest" || folder == "vndegen\\storytestpc") return kStoryTest;
    // The same story with a load screen and a question box, for a tap across a load (NekoSDKStoryTableTests).
    if (folder == "vndegen\\storytestload") {
        static const std::string table = std::string(kStoryTest) + kStoryLoadScreens;
        return table.c_str();
    }
    if (folder == "vndegen\\layouttest") return kLayoutTest;
    if (folder == "vndegen\\layoutpairs") return kLayoutPairsTest;
    if (folder == "vndegen\\squaretest0") return kSquareWindowTest;
    if (folder == "vndegen\\cardtest") return kCardTest;
    if (folder == "vndegen\\bandtest") return kBandTest;
    if (folder == "vndegen\\widgettest") return kWidgetTest;
    if (folder == "vndegen\\widgettesthide") return kWidgetTestHide;
    if (folder == "vndegen\\widgettestpart") return kWidgetTestPart;
    if (folder == "vndegen\\widgettesttabs") return kWidgetTestTabs;
    if (folder == "vndegen\\windowlogtest") return kWindowLogTest;
    if (folder == "vndegen\\commenttest") return kCommentTest;
    if (folder == "vndegen\\treetest") return kTreeTest;
    if (folder == "vndegen\\votetest") return kVoteTest;
    if (folder == "vndegen\\partslot" || folder == "vndegen\\windowlist") return kPartSlotTest;
    if (folder == "vndegen\\scrollsave") return kScrollSaveTest;
    return nullptr;
}

}  // namespace neko
