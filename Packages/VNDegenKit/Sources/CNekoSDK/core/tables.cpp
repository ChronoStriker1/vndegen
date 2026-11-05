#include "tables.h"

namespace neko {
namespace {

// Test Game 97.
const char kChristmas[] = R"(
screen title
pic obj\d000_000.bmp 0 0
pic obj\d002_000.bmp 596 457
pic obj\d001_000.bmp 32 690 1 1 0 0
button start obj\d004_000.bmp 67 44 9 5 0
button load obj\d004_000.bmp 67 84 9 5 1
button quickload obj\d004_000.bmp 67 124 9 5 2
button cg obj\d004_000.bmp 67 164 9 5 3
button scene obj\d004_000.bmp 67 204 9 5 4
button music obj\d004_000.bmp 67 244 9 5 5
button config obj\d004_000.bmp 67 284 9 5 6
button quit obj\d004_000.bmp 67 364 9 5 8

screen load
pic obj\e000_000.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\e002_000.bmp 0 3 1 2 0 1
slots obj\e007_000.bmp 63 83 546 140 4 2
slotlabel obj\e008_000.bmp 9 16
slotdigits obj\e008_001.bmp 37 9
slotthumb 12 32
datedigits obj\e008_002.bmp 213 62
dateslash obj\e008_004.bmp
datecolon obj\e008_005.bmp
dategap 13
button page0 obj\e006_000.bmp 873 20 10 5 0
button page1 obj\e006_000.bmp 905 20 10 5 1
button page2 obj\e006_000.bmp 937 20 10 5 2
button page3 obj\e006_000.bmp 969 20 10 5 3
button page4 obj\e006_000.bmp 1001 20 10 5 4
button page5 obj\e006_000.bmp 1033 20 10 5 5
button page6 obj\e006_000.bmp 1065 20 10 5 6
button page7 obj\e006_000.bmp 1097 20 10 5 7
button page8 obj\e006_000.bmp 1129 20 10 5 8
button page9 obj\e006_000.bmp 1161 20 10 5 9
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8

screen save
pic obj\e000_000.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\e002_000.bmp 0 3 1 2 0 0
slots obj\e007_000.bmp 63 83 546 140 4 2
slotlabel obj\e008_000.bmp 9 16
slotdigits obj\e008_001.bmp 37 9
slotthumb 12 32
datedigits obj\e008_002.bmp 213 62
dateslash obj\e008_004.bmp
datecolon obj\e008_005.bmp
dategap 13
button page0 obj\e006_000.bmp 873 20 10 5 0
button page1 obj\e006_000.bmp 905 20 10 5 1
button page2 obj\e006_000.bmp 937 20 10 5 2
button page3 obj\e006_000.bmp 969 20 10 5 3
button page4 obj\e006_000.bmp 1001 20 10 5 4
button page5 obj\e006_000.bmp 1033 20 10 5 5
button page6 obj\e006_000.bmp 1065 20 10 5 6
button page7 obj\e006_000.bmp 1097 20 10 5 7
button page8 obj\e006_000.bmp 1129 20 10 5 8
button page9 obj\e006_000.bmp 1161 20 10 5 9
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8

screen dialog
pic obj\i000_000.bmp 0 197 1 8 0 0
button yes obj\b003_000.bmp 435 330 2 5 0
button no obj\b003_000.bmp 650 330 2 5 1
)";

}  // namespace

const char *menuTable(const std::string &folder) {
    if (folder == "waffle\\x2019_retry") return kChristmas;
    return nullptr;
}

}  // namespace neko
