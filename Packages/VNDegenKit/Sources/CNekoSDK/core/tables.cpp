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
#include "tables/storytest.inc"
#include "tables/layouttest.inc"
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
    if (folder == "vndegen\\storytest") return kStoryTest;
    if (folder == "vndegen\\layouttest") return kLayoutTest;
    if (folder == "vndegen\\bandtest") return kBandTest;
    if (folder == "vndegen\\widgettest") return kWidgetTest;
    if (folder == "vndegen\\widgettesthide") return kWidgetTestHide;
    return nullptr;
}

}  // namespace neko
