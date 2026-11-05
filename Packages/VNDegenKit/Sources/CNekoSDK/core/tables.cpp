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
pic obj\e005_000.bmp 839 37
slots obj\e007_000.bmp 63 83 546 140 4 2
slotlabel obj\e008_000.bmp 9 16
slotdigits obj\e008_001.bmp 37 9
slotthumb 12 32
datedigits obj\e008_002.bmp 213 62
dateslash obj\e008_004.bmp
datecolon obj\e008_005.bmp
datedash obj\e008_003.bmp
slotlock obj\e009_000.bmp 453 9
slotdelete obj\e009_001.bmp 483 15
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
button page10 obj\e006_001.bmp 1194 20 1 5 0
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
pic obj\e005_000.bmp 839 37
slots obj\e007_000.bmp 63 83 546 140 4 2
slotlabel obj\e008_000.bmp 9 16
slotdigits obj\e008_001.bmp 37 9
slotthumb 12 32
datedigits obj\e008_002.bmp 213 62
dateslash obj\e008_004.bmp
datecolon obj\e008_005.bmp
datedash obj\e008_003.bmp
slotlock obj\e009_000.bmp 453 9
slotdelete obj\e009_001.bmp 483 15
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
button page10 obj\e006_001.bmp 1194 20 1 5 0
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
pic obj\f004_000.bmp 380 0
pic obj\e003_001.bmp 1110 0
pic obj\e005_000.bmp 839 37
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\f000_000.bmp 0 3 1 2 0 0
cellsheet obj\f002_000.bmp
cellsize 186 156
cellthumb 11 28
cell 27 110
cell 227 110
cell 427 110
celllocked 35
celllabel obj\e008_000.bmp 10 15
celldigits obj\e008_001.bmp 38 10 2 14
cellcount obj\f003_000.bmp obj\f003_001.bmp 100 128 134 149
button page0 obj\e006_000.bmp 874 20 10 5 0
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
pic obj\f004_000.bmp 380 0
pic obj\e003_001.bmp 1110 0
pic obj\e005_000.bmp 839 37
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\f000_000.bmp 0 3 1 2 0 1
cellsheet obj\f002_000.bmp
cellsize 186 156
cellthumb 11 28
cell 27 110
cell 227 110
celllocked 35
celllabel obj\e008_000.bmp 10 15
celldigits obj\e008_001.bmp 38 10 2 14
cellcount obj\f003_000.bmp obj\f003_001.bmp 100 128 134 149
button page0 obj\e006_000.bmp 874 20 10 5 0
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
pic obj\g001_000.bmp 0 0
pic obj\g003_000.bmp 610 44
pic obj\g005_000.bmp 644 16
slider music obj\g005_001.bmp 671 21
number music obj\g006_000.bmp 844 22
button track0 obj\g002_000.bmp 29 193 7 5 0
button track1 obj\g002_000.bmp 29 263 7 5 1
button track2 obj\g002_000.bmp 29 333 7 5 2
button music_prev obj\g004_000.bmp 914 16 7 5 0
button music_play obj\g004_000.bmp 955 16 7 5 1
button music_pause obj\g004_000.bmp 996 16 7 5 2
button music_stop obj\g004_000.bmp 1037 16 7 5 3
button music_next obj\g004_000.bmp 1078 16 7 5 4
button music_repeat obj\g004_000.bmp 1119 16 7 5 5
button music_shuffle obj\g004_000.bmp 1160 16 7 5 6
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

screen config
pic obj\h000_000.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\h001_000.bmp 0 0
pic obj\h005_000.bmp 43 85
button tab obj\h003_000.bmp 668 19 3 5 0
button tab2 obj\h003_000.bmp 754 19 3 5 1
button tab3 obj\h003_000.bmp 840 19 3 5 2
button cfg_defaults obj\h002_000.bmp 1007 19 3 5 0
button cfg_undo obj\h002_000.bmp 1093 19 3 5 1
button cfg_apply obj\h002_000.bmp 1179 19 3 5 2
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8
pic obj\h006_000.bmp 172 91 2 5 0 4
pic obj\h006_000.bmp 294 91 2 5 1 4
pic obj\h007_000.bmp 172 133 6 5 0 4
pic obj\h007_000.bmp 294 133 6 5 1 4
pic obj\h007_000.bmp 416 133 6 5 2 4
pic obj\h007_000.bmp 172 165 6 5 3 4
pic obj\h007_000.bmp 294 165 6 5 4 4
pic obj\h007_000.bmp 416 165 6 5 5 4
pic obj\h008_000.bmp 50 229 2 5 0 4
pic obj\h008_000.bmp 131 229 2 5 1 4
pic obj\h008_000.bmp 226 229 2 5 0 4
pic obj\h008_000.bmp 307 229 2 5 1 4
pic obj\h008_000.bmp 402 229 2 5 0 4
pic obj\h008_000.bmp 483 229 2 5 1 4
pic obj\h008_000.bmp 615 377 2 5 0 4
pic obj\h008_000.bmp 696 377 2 5 1 4
bar obj\h009_000.bmp 155 307 100
pic obj\h011_000.bmp 415 279 10 1 1 0
pic obj\h011_000.bmp 429 279 10 1 0 0
pic obj\h011_000.bmp 443 279 10 1 0 0
pic obj\h010_000.bmp 121 311 2 5 0 4
pic obj\h010_000.bmp 471 311 2 5 1 4
check confirm_saveload obj\h013_000.bmp 603 117 8 5 0
check confirm_overwrite obj\h013_000.bmp 603 145 8 5 1
check confirm_delete obj\h013_000.bmp 603 173 8 5 2
check confirm_quick obj\h013_000.bmp 603 201 8 5 3
check confirm_title obj\h013_000.bmp 603 229 8 5 4
check confirm_exit obj\h013_000.bmp 603 257 8 5 5
pic obj\h013_000.bmp 603 285 8 5 6 4
pic obj\h013_000.bmp 603 313 8 5 7 4

screen config2
pic obj\h000_000.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\h001_000.bmp 0 0
pic obj\h015_000.bmp 43 85
button tab obj\h003_000.bmp 668 19 3 5 0
button tab2 obj\h003_000.bmp 754 19 3 5 1
button tab3 obj\h003_000.bmp 840 19 3 5 2
button cfg_defaults obj\h002_000.bmp 1007 19 3 5 0
button cfg_undo obj\h002_000.bmp 1093 19 3 5 1
button cfg_apply obj\h002_000.bmp 1179 19 3 5 2
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8
radio text 0 obj\h016_000.bmp 64 122 4 5 0
radio text 1 obj\h016_000.bmp 186 122 4 5 1
radio text 2 obj\h016_000.bmp 308 122 4 5 2
radio text 3 obj\h016_000.bmp 430 122 4 5 3
radio auto 0 obj\h016_000.bmp 64 200 4 5 0
radio auto 1 obj\h016_000.bmp 186 200 4 5 1
radio auto 2 obj\h016_000.bmp 308 200 4 5 2
radio auto 3 obj\h016_000.bmp 430 200 4 5 3
pic obj\h018_000.bmp 617 112 2 5 0 4
pic obj\h018_000.bmp 698 112 2 5 1 2
pic obj\h019_000.bmp 845 112 2 5 0 4
pic obj\h019_000.bmp 926 112 2 5 1 4
radio rightclick 0 obj\h020_000.bmp 710 155 5 5 0
radio rightclick 1 obj\h020_000.bmp 832 155 5 5 1
radio rightclick 2 obj\h020_000.bmp 954 155 5 5 2
radio rightclick 3 obj\h020_000.bmp 710 187 5 5 3
radio rightclick 4 obj\h020_000.bmp 832 187 5 5 4
pic obj\h008_000.bmp 617 250 2 5 0 4
pic obj\h008_000.bmp 698 250 2 5 1 4
pic obj\h008_000.bmp 845 250 2 5 0 4
pic obj\h008_000.bmp 926 250 2 5 1 4
bar obj\h009_000.bmp 155 568 50
pic obj\h011_000.bmp 406 539 10 1 1 0
pic obj\h011_000.bmp 428 539 10 1 0 0
pic obj\h011_000.bmp 442 539 10 1 0 0
pic obj\h010_000.bmp 119 570 2 5 0 4
pic obj\h010_000.bmp 469 570 2 5 1 4

screen config3
pic obj\h000_000.bmp 0 0
pic obj\e003_001.bmp 1110 0
pic obj\e001_000.bmp 0 0
pic obj\e001_000.bmp 0 655
pic obj\h001_000.bmp 0 0
pic obj\h021_000.bmp 43 85
button tab obj\h003_000.bmp 668 19 3 5 0
button tab2 obj\h003_000.bmp 754 19 3 5 1
button tab3 obj\h003_000.bmp 840 19 3 5 2
button cfg_defaults obj\h002_000.bmp 1007 19 3 5 0
button cfg_undo obj\h002_000.bmp 1093 19 3 5 1
button cfg_apply obj\h002_000.bmp 1179 19 3 5 2
button nav_save obj\e003_000.bmp 1190 95 9 5 0
button nav_load obj\e003_000.bmp 1190 136 9 5 1
button nav_cg obj\e003_000.bmp 1190 177 9 5 2
button nav_scene obj\e003_000.bmp 1190 218 9 5 3
button nav_music obj\e003_000.bmp 1190 259 9 5 4
button nav_config obj\e003_000.bmp 1190 300 9 5 5
button nav_title obj\e003_000.bmp 1190 341 9 5 6
button nav_quit obj\e003_000.bmp 1190 382 9 5 7
button back obj\e003_000.bmp 1190 464 9 5 8
slider master obj\h009_000.bmp 155 128
number master obj\h011_000.bmp 415 100
step master -1 obj\h010_000.bmp 119 130 2 5 0
step master 1 obj\h010_000.bmp 469 130 2 5 1
check mute_master obj\h024_000.bmp 479 95 1 5 0
pic obj\h023_000.bmp 532 94 1 5 0 4
slider bgm obj\h009_000.bmp 155 206
number bgm obj\h011_000.bmp 415 178
step bgm -1 obj\h010_000.bmp 119 208 2 5 0
step bgm 1 obj\h010_000.bmp 469 208 2 5 1
check mute_bgm obj\h024_000.bmp 479 173 1 5 0
pic obj\h023_000.bmp 532 172 1 5 0 4
slider se obj\h009_000.bmp 155 284
number se obj\h011_000.bmp 415 256
step se -1 obj\h010_000.bmp 119 286 2 5 0
step se 1 obj\h010_000.bmp 469 286 2 5 1
check mute_se obj\h024_000.bmp 479 251 1 5 0
pic obj\h023_000.bmp 532 250 1 5 0 4
bar obj\h009_000.bmp 155 362 80
pic obj\h011_000.bmp 415 334 10 1 0 0
pic obj\h011_000.bmp 429 334 10 1 8 0
pic obj\h011_000.bmp 443 334 10 1 0 0
pic obj\h010_000.bmp 119 364 2 5 0 4
pic obj\h010_000.bmp 469 364 2 5 1 4
pic obj\h024_000.bmp 479 329 1 5 0 4
pic obj\h023_000.bmp 532 328 1 5 0 4
slider voice obj\h009_000.bmp 155 441
number voice obj\h011_000.bmp 415 412
step voice -1 obj\h010_000.bmp 119 442 2 5 0
step voice 1 obj\h010_000.bmp 469 442 2 5 1
check mute_voice obj\h024_000.bmp 479 407 1 5 0
pic obj\h023_000.bmp 532 406 1 5 0 4
bar obj\h009_000.bmp 155 519 80
pic obj\h011_000.bmp 415 490 10 1 0 0
pic obj\h011_000.bmp 429 490 10 1 8 0
pic obj\h011_000.bmp 443 490 10 1 0 0
pic obj\h010_000.bmp 119 520 2 5 0 4
pic obj\h010_000.bmp 469 520 2 5 1 4
pic obj\h024_000.bmp 479 485 1 5 0 4
pic obj\h023_000.bmp 532 484 1 5 0 4
pic obj\h008_000.bmp 617 112 2 5 0 4
pic obj\h008_000.bmp 698 112 2 5 1 4
pic obj\h022_000.bmp 600 198 1 5 0 4

screen log
pic obj\m001_000.bmp 1185 110
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1185 110 500
rows obj\m000_000.bmp 77 124 165 4 160 12
button log_close obj\m003_000.bmp 1232 22 5 5 0
button log_top obj\m003_000.bmp 1180 34 5 5 1
button log_up obj\m003_000.bmp 1180 68 5 5 2
button log_down obj\m003_000.bmp 1180 622 5 5 3
button log_bottom obj\m003_000.bmp 1180 656 5 5 4

screen cfgsav
run 4 2 1
run 16 2 50
run 24 1 100
run 36 1 100
run 40 1 1
run 48 1 2
run 52 1 1
run 76 1 1
run 108 1 1
run 116 1 60
run 120 1 1
run 168 1 1
run 244 1 32
run 280 1 9
run 324 2 1
run 340 1 1
run 348 1 1
run 360 6 1
run 400 100 1
run 848 1 1
run 852 1 480
run 860 1 1
run 1996 1 1
run 2000 1 100
run 2004 8 80
run 2120 9 1
run 2160 1 1
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
slotdigits obj\e008_001.bmp 58 4
slotthumb 19 34
datedigits obj\e009_000.bmp 287 65
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
slotlock obj\e011_000.bmp 15 139
slotdelete obj\e011_001.bmp 672 19
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
button page10 obj\e005_000.bmp 1458 64 11 5 10
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
slotdigits obj\e008_001.bmp 58 4
slotthumb 19 34
datedigits obj\e009_000.bmp 287 65
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
slotlock obj\e011_000.bmp 15 139
slotdelete obj\e011_001.bmp 672 19
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
button page10 obj\e005_000.bmp 1458 64 11 5 10
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
cellthumb 7 48
celllabel obj\e008_000.bmp 5 22
celldigits obj\e008_001.bmp 45 17 3 18
cellcount obj\f003_000.bmp obj\f003_001.bmp 161 194 193 206
cell 147 156
cell 432 156
cell 717 156
cell 204 411
cell 489 411
cell 774 411
cell 147 665
cell 432 665
cell 717 665
button page0 obj\e005_000.bmp 473 949 11 5 0
button page1 obj\e005_000.bmp 573 949 11 5 1
button page2 obj\e005_000.bmp 673 949 11 5 2
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
cellthumb 7 48
celllabel obj\e008_000.bmp 5 22
celldigits obj\e008_001.bmp 45 17 3 18
cellcount obj\f003_000.bmp obj\f003_001.bmp 161 194 193 206
cell 147 156
cell 432 156
cell 717 156
cell 204 411
cell 489 411
cell 774 411
cell 147 665
cell 432 665
cell 717 665
button page0 obj\e005_000.bmp 473 949 11 5 0
button page1 obj\e005_000.bmp 573 949 11 5 1
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
pic obj\g000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\g001_000.bmp 0 0
pic obj\g004_000.bmp 410 97
slider music obj\g004_001.bmp 473 100
number music obj\g005_000.bmp 651 44
button music_prev obj\g003_000.bmp 858 64 7 5 0
button music_play obj\g003_000.bmp 958 64 7 5 1
button music_pause obj\g003_000.bmp 1058 64 7 5 2
button music_stop obj\g003_000.bmp 1158 64 7 5 3
button music_next obj\g003_000.bmp 1258 64 7 5 4
button music_repeat obj\g003_000.bmp 1358 64 7 5 5
button music_shuffle obj\g003_000.bmp 1458 64 7 5 6
button track0 obj\g002_000.bmp 42 219 10 5 0
button track1 obj\g002_000.bmp 42 333 10 5 1
button track2 obj\g002_000.bmp 42 447 10 5 2
button track3 obj\g002_000.bmp 42 561 10 5 3
button track4 obj\g002_000.bmp 42 675 10 5 4
button track5 obj\g002_000.bmp 42 789 10 5 5
button track6 obj\g002_000.bmp 42 903 10 5 6
button track7 obj\g002_000.bmp 558 219 10 5 7
button track8 obj\g002_000.bmp 558 333 10 5 8
button track9 obj\g002_000.bmp 558 447 10 5 9
button track10 obj\g002_001.bmp 558 561 8 5 0
button track11 obj\g002_001.bmp 558 675 8 5 1
button track12 obj\g002_001.bmp 558 789 8 5 2
button track13 obj\g002_001.bmp 558 903 8 5 3
button track14 obj\g002_001.bmp 1074 219 8 5 4
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

screen config
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\h000_000.bmp 0 0
pic obj\h005_000.bmp 53 181
button tab obj\h002_000.bmp 432 70 3 5 0
button tab2 obj\h002_000.bmp 608 70 3 5 1
button tab3 obj\h002_000.bmp 784 70 3 5 2
button cfg_defaults obj\h001_000.bmp 1010 70 3 5 0
button cfg_undo obj\h001_000.bmp 1186 70 3 5 1
button cfg_apply obj\h001_000.bmp 1362 70 3 5 2
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap
pic obj\h006_000.bmp 236 208 2 5 0 4
pic obj\h006_000.bmp 418 208 2 5 1 4
pic obj\h007_000.bmp 236 297 6 5 0 4
pic obj\h007_000.bmp 418 297 6 5 1 4
pic obj\h007_000.bmp 599 297 6 5 2 4
pic obj\h007_000.bmp 236 355 6 5 3 4
pic obj\h007_000.bmp 418 355 6 5 4 4
pic obj\h007_000.bmp 599 355 6 5 5 4
pic obj\h008_000.bmp 63 482 2 5 0 4
pic obj\h008_000.bmp 181 482 2 5 1 4
pic obj\h008_000.bmp 312 482 2 5 0 4
pic obj\h008_000.bmp 430 482 2 5 1 4
pic obj\h008_000.bmp 561 482 2 5 0 4
pic obj\h008_000.bmp 679 482 2 5 1 4
pic obj\h008_000.bmp 827 687 2 5 0 4
pic obj\h008_000.bmp 945 687 2 5 1 4
pic obj\h999_000.bmp 1077 687 2 5 0 4
pic obj\h999_000.bmp 1318 687 2 5 1 4
slider opacity obj\h009_000.bmp 208 612
number opacity obj\h011_000.bmp 573 568
step opacity -1 obj\h010_000.bmp 140 613 2 5 0
step opacity 1 obj\h010_000.bmp 680 613 2 5 1
check confirm_saveload obj\h013_000.bmp 842 245 8 5 0
check confirm_overwrite obj\h013_000.bmp 842 291 8 5 1
check confirm_delete obj\h013_000.bmp 842 337 8 5 2
check confirm_quick obj\h013_000.bmp 842 383 8 5 3
check confirm_title obj\h013_000.bmp 842 429 8 5 4
check confirm_exit obj\h013_000.bmp 842 475 8 5 5
pic obj\h013_000.bmp 842 521 8 5 6 4
pic obj\h013_000.bmp 842 567 8 5 7 4

screen config2
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\h000_000.bmp 0 0
pic obj\h015_000.bmp 53 181
button tab obj\h002_000.bmp 432 70 3 5 0
button tab2 obj\h002_000.bmp 608 70 3 5 1
button tab3 obj\h002_000.bmp 784 70 3 5 2
button cfg_defaults obj\h001_000.bmp 1010 70 3 5 0
button cfg_undo obj\h001_000.bmp 1186 70 3 5 1
button cfg_apply obj\h001_000.bmp 1362 70 3 5 2
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap
radio text 0 obj\h016_000.bmp 251 208 4 5 0
radio text 1 obj\h016_000.bmp 373 208 4 5 1
radio text 2 obj\h016_000.bmp 495 208 4 5 2
radio text 3 obj\h016_000.bmp 617 208 4 5 3
radio auto 0 obj\h016_000.bmp 251 294 4 5 0
radio auto 1 obj\h016_000.bmp 373 294 4 5 1
radio auto 2 obj\h016_000.bmp 495 294 4 5 2
radio auto 3 obj\h016_000.bmp 617 294 4 5 3
pic obj\h018_000.bmp 889 238 2 5 0 4
pic obj\h018_000.bmp 1007 238 2 5 1 2
pic obj\h019_000.bmp 1263 238 2 5 0 4
pic obj\h019_000.bmp 1381 238 2 5 1 4
radio rightclick 0 obj\h020_000.bmp 1005 334 5 5 0
radio rightclick 1 obj\h020_000.bmp 1187 334 5 5 1
radio rightclick 2 obj\h020_000.bmp 1368 334 5 5 2
radio rightclick 3 obj\h020_000.bmp 1005 392 5 5 3
radio rightclick 4 obj\h020_000.bmp 1187 392 5 5 4
pic obj\h008_000.bmp 889 521 2 5 0 4
pic obj\h008_000.bmp 1007 521 2 5 1 4
pic obj\h008_000.bmp 1263 521 2 5 0 4
pic obj\h008_000.bmp 1381 521 2 5 1 4
bar obj\h009_000.bmp 208 814 50
pic obj\h011_000.bmp 563 771 10 1 1 0
pic obj\h011_000.bmp 597 771 10 1 0 0
pic obj\h011_000.bmp 623 771 10 1 0 0
pic obj\h010_000.bmp 140 816 2 5 0 4
pic obj\h010_000.bmp 680 816 2 5 1 4

screen config3
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\h000_000.bmp 0 0
pic obj\h021_000.bmp 53 181
button tab obj\h002_000.bmp 432 70 3 5 0
button tab2 obj\h002_000.bmp 608 70 3 5 1
button tab3 obj\h002_000.bmp 784 70 3 5 2
button cfg_defaults obj\h001_000.bmp 1010 70 3 5 0
button cfg_undo obj\h001_000.bmp 1186 70 3 5 1
button cfg_apply obj\h001_000.bmp 1362 70 3 5 2
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap
button back obj\e003_000.bmp 1738 831 10 5 9 swap
slider master obj\h009_000.bmp 208 255
number master obj\h011_000.bmp 552 212
step master -1 obj\h010_000.bmp 140 257 2 5 0
step master 1 obj\h010_000.bmp 680 257 2 5 1
check mute_master obj\h024_000.bmp 656 201 1 5 0
pic obj\h023_000.bmp 746 199 1 5 0 0
slider bgm obj\h009_000.bmp 208 392
number bgm obj\h011_000.bmp 552 349
step bgm -1 obj\h010_000.bmp 140 394 2 5 0
step bgm 1 obj\h010_000.bmp 680 394 2 5 1
check mute_bgm obj\h024_000.bmp 656 338 1 5 0
pic obj\h023_000.bmp 746 336 1 5 0 0
slider se obj\h009_000.bmp 208 529
number se obj\h011_000.bmp 552 486
step se -1 obj\h010_000.bmp 140 531 2 5 0
step se 1 obj\h010_000.bmp 680 531 2 5 1
check mute_se obj\h024_000.bmp 656 475 1 5 0
pic obj\h023_000.bmp 746 473 1 5 0 0
bar obj\h009_000.bmp 208 666 80
pic obj\h011_000.bmp 552 623 10 1 0 0
pic obj\h011_000.bmp 578 623 10 1 8 0
pic obj\h011_000.bmp 604 623 10 1 0 0
pic obj\h010_000.bmp 140 668 2 5 0 4
pic obj\h010_000.bmp 680 668 2 5 1 4
pic obj\h024_000.bmp 656 612 1 5 0 4
pic obj\h023_000.bmp 746 610 1 5 0 0
slider voice obj\h009_000.bmp 208 803
number voice obj\h011_000.bmp 552 760
step voice -1 obj\h010_000.bmp 140 805 2 5 0
step voice 1 obj\h010_000.bmp 680 805 2 5 1
check mute_voice obj\h024_000.bmp 656 749 1 5 0
pic obj\h023_000.bmp 746 747 1 5 0 0
bar obj\h009_000.bmp 208 940 80
pic obj\h011_000.bmp 552 897 10 1 0 0
pic obj\h011_000.bmp 578 897 10 1 8 0
pic obj\h011_000.bmp 604 897 10 1 0 0
pic obj\h010_000.bmp 140 942 2 5 0 4
pic obj\h010_000.bmp 680 942 2 5 1 4
pic obj\h024_000.bmp 656 886 1 5 0 4
pic obj\h023_000.bmp 746 884 1 5 0 0
pic obj\h008_000.bmp 889 238 2 5 0 4
pic obj\h008_000.bmp 1007 238 2 5 1 4
pic obj\h022_000.bmp 919 868 3 5 0 4
pic obj\h022_000.bmp 1206 959 3 5 1 4
pic obj\h022_000.bmp 1383 959 3 5 2 4

screen log
dim 216
pic obj\m001_000.bmp 1771 163
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1774 163 756
rows obj\m000_000.bmp 117 58 250 4 122 60
button log_close obj\l001_003.bmp 1851 21 2 5 0
button log_top obj\m003_000.bmp 1770 51 4 5 0
button log_up obj\m003_000.bmp 1770 102 4 5 1
button log_down obj\m003_000.bmp 1770 930 4 5 2
button log_bottom obj\m003_000.bmp 1770 980 4 5 3

screen cfgsav
run 4 2 1
run 16 2 50
run 24 1 60
run 36 1 100
run 40 1 1
run 48 1 2
run 52 1 1
run 76 1 1
run 108 1 1
run 116 1 60
run 120 1 1
run 168 1 1
run 200 1 1
run 212 1 1
run 244 1 32
run 280 1 9
run 340 1 1
run 348 1 1
run 360 6 1
run 400 100 1
run 848 1 1
run 852 1 480
run 860 1 1
run 1996 1 1
run 2000 1 100
run 2004 8 80
run 2120 9 1
run 2160 1 1
)";

}  // namespace

const char *menuTable(const std::string &folder) {
    if (folder == "waffle\\x2019_retry") return kChristmas;
    if (folder == "waffle\\alchemy") return kAlchemist;
    return nullptr;
}

}  // namespace neko
