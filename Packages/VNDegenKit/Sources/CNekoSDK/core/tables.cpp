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
screen cg
pic obj\f000_001.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\f000_000.bmp 0 3 1 2 0 0
cellsheet obj\f002_000.bmp
cellsize 186 156
cellthumb 13 32
cell 30 114
cell 234 114
cell 436 114
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8

screen scene
pic obj\f000_001.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\f000_000.bmp 0 3 1 2 0 1
cellsheet obj\f002_000.bmp
cellsize 186 156
cellthumb 13 32
cell 30 114
cell 234 114
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8

screen music
pic obj\g000_000.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\g001_000.bmp 0 3
button track0 obj\g002_000.bmp 30 192 7 5 0
button track1 obj\g002_000.bmp 30 264 7 5 1
button track2 obj\g002_000.bmp 30 336 7 5 2
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8

screen config3
pic obj\h000_000.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\h001_000.bmp 0 0
pic obj\h021_000.bmp 43 85
slider master obj\h009_000.bmp 155 128
slider bgm obj\h009_000.bmp 155 206
slider se obj\h009_000.bmp 155 284
slider voice obj\h009_000.bmp 155 441
button tab obj\h003_000.bmp 757 18 3 5 1
button tab3 obj\h003_000.bmp 842 18 3 5 2
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8

screen config
pic obj\h000_000.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\h001_000.bmp 0 0
pic obj\h015_000.bmp 43 85
radio text 0 obj\h016_000.bmp 64 200 4 5 0
radio text 1 obj\h016_000.bmp 186 200 4 5 1
radio text 2 obj\h016_000.bmp 308 200 4 5 2
radio text 3 obj\h016_000.bmp 430 200 4 5 3
button tab obj\h003_000.bmp 757 18 3 5 1
button tab3 obj\h003_000.bmp 842 18 3 5 2
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8

screen log
rows obj\m000_000.bmp 77 124 165 4 160 12
button log_close obj\m003_000.bmp 1232 22 5 5 0
button log_top obj\m003_000.bmp 1180 34 5 5 1
button log_up obj\m003_000.bmp 1180 68 5 5 2
button log_down obj\m003_000.bmp 1180 622 5 5 3
button log_bottom obj\m003_000.bmp 1180 656 5 5 4

screen dialog
pic obj\i000_000.bmp 0 197 1 8 0 0
button yes obj\b003_000.bmp 435 330 2 5 0
button no obj\b003_000.bmp 650 330 2 5 1
)";

// Test Game 95.
const char kAlchemist[] = R"(
screen title
pic obj\c000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\d000_001.bmp 0 1038
pic obj\d001_000.bmp 0 0
pic obj\d002_000.bmp 968 97
pic obj\d004_000.bmp 924 1044
button start obj\d003_000.bmp 1072 523 7 5 0
button load obj\d003_000.bmp 1292 523 7 5 1
button quickload obj\d003_000.bmp 1512 523 7 5 2
button cg obj\d003_000.bmp 1072 657 7 5 3
button scene obj\d003_000.bmp 1292 657 7 5 5
button music obj\d003_000.bmp 1512 657 7 5 6
button config obj\d003_001.bmp 1207 800 3 5 0
button quit obj\d003_001.bmp 1427 800 3 5 2

screen load
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 128
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\e001_000.bmp 0 0 1 2 0 1
slots obj\e007_000.bmp 41 178 786 220 4 2
slotlabel obj\e008_000.bmp 18 9
slotdigits obj\e008_001.bmp 38 9
slotthumb 19 34
datedigits obj\e009_000.bmp 287 65
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
dategap 21
button page0 obj\e005_000.bmp 458 64 11 5 0
button page1 obj\e005_000.bmp 558 64 11 5 1
button page2 obj\e005_000.bmp 658 64 11 5 2
button page3 obj\e005_000.bmp 758 64 11 5 3
button page4 obj\e005_000.bmp 858 64 11 5 4
button page5 obj\e005_000.bmp 958 64 11 5 5
button page6 obj\e005_000.bmp 1058 64 11 5 6
button page7 obj\e005_000.bmp 1158 64 11 5 7
button page8 obj\e005_000.bmp 1258 64 11 5 8
button page9 obj\e005_000.bmp 1358 64 11 5 9
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap

screen save
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 128
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\e001_000.bmp 0 0 1 2 0 0
slots obj\e007_000.bmp 41 178 786 220 4 2
slotlabel obj\e008_000.bmp 18 9
slotdigits obj\e008_001.bmp 38 9
slotthumb 19 34
datedigits obj\e009_000.bmp 287 65
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
dategap 21
button page0 obj\e005_000.bmp 458 64 11 5 0
button page1 obj\e005_000.bmp 558 64 11 5 1
button page2 obj\e005_000.bmp 658 64 11 5 2
button page3 obj\e005_000.bmp 758 64 11 5 3
button page4 obj\e005_000.bmp 858 64 11 5 4
button page5 obj\e005_000.bmp 958 64 11 5 5
button page6 obj\e005_000.bmp 1058 64 11 5 6
button page7 obj\e005_000.bmp 1158 64 11 5 7
button page8 obj\e005_000.bmp 1258 64 11 5 8
button page9 obj\e005_000.bmp 1358 64 11 5 9
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap
screen cg
pic bg\bg_003_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\f004_000.bmp 1000 0
pic obj\f000_000.bmp 0 0 1 2 0 0
cellsheet obj\f002_000.bmp
cellsize 272 234
cellthumb 16 40
cell 147 156
cell 432 156
cell 717 156
cell 204 411
cell 489 411
cell 774 411
cell 147 672
cell 432 672
cell 717 672
button page0 obj\e005_000.bmp 474 960 11 5 0
button page1 obj\e005_000.bmp 574 960 11 5 1
button page2 obj\e005_000.bmp 674 960 11 5 2
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap

screen scene
pic bg\bg_003_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\f004_000.bmp 1000 0
pic obj\f000_000.bmp 0 0 1 2 0 1
cellsheet obj\f002_000.bmp
cellsize 272 234
cellthumb 16 40
cell 147 156
cell 432 156
cell 717 156
cell 204 411
cell 489 411
cell 774 411
cell 147 672
cell 432 672
cell 717 672
button page0 obj\e005_000.bmp 474 960 11 5 0
button page1 obj\e005_000.bmp 574 960 11 5 1
button page2 obj\e005_000.bmp 674 960 11 5 2
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap

screen music
pic obj\c000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\g001_000.bmp 0 0
button track0 obj\g002_000.bmp 45 216 10 5 0
button track1 obj\g002_000.bmp 45 330 10 5 1
button track2 obj\g002_000.bmp 45 444 10 5 2
button track3 obj\g002_000.bmp 45 558 10 5 3
button track4 obj\g002_000.bmp 45 672 10 5 4
button track5 obj\g002_000.bmp 45 786 10 5 5
button track6 obj\g002_000.bmp 45 900 10 5 6
button track7 obj\g002_000.bmp 558 216 10 5 7
button track8 obj\g002_000.bmp 558 330 10 5 8
button track9 obj\g002_000.bmp 558 444 10 5 9
button track10 obj\g002_001.bmp 558 558 8 5 0
button track11 obj\g002_001.bmp 558 672 8 5 1
button track12 obj\g002_001.bmp 558 786 8 5 2
button track13 obj\g002_001.bmp 558 900 8 5 3
button track14 obj\g002_001.bmp 1080 216 8 5 4
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap

screen config3
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\h000_000.bmp 0 0
pic obj\h021_000.bmp 57 175
slider master obj\h009_000.bmp 210 253
slider bgm obj\h009_000.bmp 210 391
slider se obj\h009_000.bmp 210 529
slider voice obj\h009_000.bmp 210 802
button tab obj\h002_000.bmp 610 73 3 5 1
button tab3 obj\h002_000.bmp 780 73 3 5 2
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap

screen config
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\h000_000.bmp 0 0
pic obj\h015_000.bmp 57 215
radio text 0 obj\h016_000.bmp 270 214 4 5 0
radio text 1 obj\h016_000.bmp 450 214 4 5 1
radio text 2 obj\h016_000.bmp 630 214 4 5 2
radio text 3 obj\h016_000.bmp 810 214 4 5 3
button tab obj\h002_000.bmp 610 73 3 5 1
button tab3 obj\h002_000.bmp 780 73 3 5 2
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap

screen dialog
pic obj\b001_000.bmp 0 270
pic obj\i000_000.bmp 660 380 1 8 0 0
button yes obj\b003_000.bmp 720 476 2 5 0
button no obj\b003_000.bmp 1005 476 2 5 1

screen choice
pic obj\n000_000.bmp 0 259
pic obj\n001_000.bmp 785 335
place 413 132

screen start
pic obj\d007_000.bmp 0 0
pic obj\d007_001.bmp 0 342
button start_prologue obj\d007_002.bmp 600 340 2 5 0
button start_main obj\d007_002.bmp 840 340 2 5 1
)";

}  // namespace

const char *menuTable(const std::string &folder) {
    if (folder == "waffle\\x2019_retry") return kChristmas;
    if (folder == "waffle\\alchemy") return kAlchemist;
    return nullptr;
}

}  // namespace neko
