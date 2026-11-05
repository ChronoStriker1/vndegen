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

// Test Game 88.
const char kMother3[] = R"(
screen title
pic obj\d000_000.bmp 0 -128
pic obj\d002_000.bmp 0 558
pic obj\d001_000.bmp 42 2
pic obj\d004_000.bmp 1122 7 1 5 0 0
pic obj\d003_000.bmp 174 638 11 5 6 0
pic obj\d003_000.bmp 532 638 11 5 8 0
pic obj\d003_000.bmp 1074 638 11 5 10 0
button start obj\d003_000.bmp 59 592 11 5 0
button load obj\d003_000.bmp 238 592 11 5 1
button quickload obj\d003_000.bmp 417 592 11 5 2
button cg obj\d003_000.bmp 597 592 11 5 3
button scene obj\d003_000.bmp 775 592 11 5 4
button music obj\d003_000.bmp 954 592 11 5 5
button config obj\d003_000.bmp 353 638 11 5 7
button quit obj\d003_000.bmp 711 638 11 5 9

screen load
pic obj\e000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\e002_000.bmp 0 0 1 2 0 1
slots obj\e007_000.bmp 26 104 546 136 4 2
slotlabel obj\e008_000.bmp 9 16
slotdigits obj\e008_001.bmp 37 9
slotthumb 8 11
datedigits obj\e009_000.bmp 193 58
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
slotlock obj\e010_000.bmp 446 52
slotdelete obj\e011_000.bmp 473 52
dategap 13
button page0 obj\e006_000.bmp 280 34 10 5 0
button page1 obj\e006_000.bmp 330 34 10 5 1
button page2 obj\e006_000.bmp 380 34 10 5 2
button page3 obj\e006_000.bmp 430 34 10 5 3
button page4 obj\e006_000.bmp 480 34 10 5 4
button page5 obj\e006_000.bmp 530 34 10 5 5
button page6 obj\e006_000.bmp 580 34 10 5 6
button page7 obj\e006_000.bmp 630 34 10 5 7
button page8 obj\e006_000.bmp 680 34 10 5 8
button page9 obj\e006_000.bmp 730 34 10 5 9
button page10 obj\e006_001.bmp 782 34 1 5 0
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
pic obj\e004_000.bmp 1152 303 10 5 5 0
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen save
pic obj\e000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\e002_000.bmp 0 0 1 2 0 0
slots obj\e007_000.bmp 26 104 546 136 4 2
slotlabel obj\e008_000.bmp 9 16
slotdigits obj\e008_001.bmp 37 9
slotthumb 8 11
datedigits obj\e009_000.bmp 193 58
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
slotlock obj\e010_000.bmp 446 52
slotdelete obj\e011_000.bmp 473 52
dategap 13
button page0 obj\e006_000.bmp 280 34 10 5 0
button page1 obj\e006_000.bmp 330 34 10 5 1
button page2 obj\e006_000.bmp 380 34 10 5 2
button page3 obj\e006_000.bmp 430 34 10 5 3
button page4 obj\e006_000.bmp 480 34 10 5 4
button page5 obj\e006_000.bmp 530 34 10 5 5
button page6 obj\e006_000.bmp 580 34 10 5 6
button page7 obj\e006_000.bmp 630 34 10 5 7
button page8 obj\e006_000.bmp 680 34 10 5 8
button page9 obj\e006_000.bmp 730 34 10 5 9
button page10 obj\e006_001.bmp 782 34 1 5 0
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
pic obj\e004_000.bmp 1152 303 10 5 5 0
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen cg
pic obj\f000_000.bmp 0 0 tab 0
pic obj\f000_002.bmp 0 0 tab 1
pic obj\f000_001.bmp 0 0 tab 2
pic obj\f000_003.bmp 0 0 tab 3
pic obj\e003_000.bmp 1138 0
pic obj\f001_000.bmp 0 0 1 2 0 0
cellsheet obj\f003_000.bmp
cellsize 186 156
cellthumb 8 26
cell 303 82
cell 303 245
cell 303 408
cell 502 82
cell 502 245
cell 502 408
celllabel obj\e008_000.bmp 10 15
celldigits obj\e008_001.bmp 38 10 2 14
gallerytabs
button page0 obj\e006_000.bmp 280 34 10 5 0
button page1 obj\e006_000.bmp 330 34 10 5 1
button gtab0 obj\f002_000.bmp 8 654 4 5 0
button gtab1 obj\f002_000.bmp 288 654 4 5 2
button gtab2 obj\f002_000.bmp 568 654 4 5 1
button gtab3 obj\f002_000.bmp 848 654 4 5 3
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
pic obj\e004_000.bmp 1152 303 10 5 5 0
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen scene
pic obj\f000_000.bmp 0 0 tab 0
pic obj\f000_002.bmp 0 0 tab 1
pic obj\f000_001.bmp 0 0 tab 2
pic obj\f000_003.bmp 0 0 tab 3
pic obj\e003_000.bmp 1138 0
pic obj\f001_000.bmp 0 0 1 2 0 1
cellsheet obj\f003_000.bmp
cellsize 186 156
cellthumb 8 26
cell 303 82
cell 303 245
cell 303 408
cell 502 82
cell 502 245
cell 502 408
celllabel obj\e008_000.bmp 10 15
celldigits obj\e008_001.bmp 38 10 2 14
gallerytabs
button page0 obj\e006_000.bmp 280 34 10 5 0
button page1 obj\e006_000.bmp 330 34 10 5 1
button gtab0 obj\f002_000.bmp 8 654 4 5 0
button gtab1 obj\f002_000.bmp 288 654 4 5 2
button gtab2 obj\f002_000.bmp 568 654 4 5 1
button gtab3 obj\f002_000.bmp 848 654 4 5 3
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
pic obj\e004_000.bmp 1152 303 10 5 5 0
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen music
pic obj\g000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\g001_000.bmp 0 0
pic obj\g004_000.bmp 343 676
slider music obj\g004_001.bmp 370 681
number music obj\g005_000.bmp 461 659
button music_prev obj\g003_000.bmp 20 668 7 5 0
button music_play obj\g003_000.bmp 64 668 7 5 1
button music_pause obj\g003_000.bmp 108 668 7 5 2
button music_stop obj\g003_000.bmp 152 668 7 5 3
button music_next obj\g003_000.bmp 196 668 7 5 4
button music_repeat obj\g003_000.bmp 240 668 7 5 5
button music_shuffle obj\g003_000.bmp 284 668 7 5 6
button track0 obj\g002_000.bmp 84 187 13 5 0
button track1 obj\g002_000.bmp 84 244 13 5 1
button track2 obj\g002_000.bmp 84 301 13 5 2
button track3 obj\g002_000.bmp 84 358 13 5 3
button track4 obj\g002_000.bmp 84 415 13 5 4
button track5 obj\g002_000.bmp 84 472 13 5 5
button track6 obj\g002_000.bmp 84 529 13 5 6
button track7 obj\g002_000.bmp 560 187 13 5 7
button track8 obj\g002_000.bmp 560 244 13 5 8
button track9 obj\g002_000.bmp 560 301 13 5 9
button track10 obj\g002_000.bmp 560 358 13 5 10
button track11 obj\g002_000.bmp 560 415 13 5 11
button track12 obj\g002_000.bmp 560 472 13 5 12
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
pic obj\e004_000.bmp 1152 303 10 5 5 0
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen config
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h005_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0
button tab2 obj\h003_000.bmp 413 25 3 5 1
button tab3 obj\h003_000.bmp 544 25 3 5 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
pic obj\e004_000.bmp 1152 303 10 5 5 0
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9
pic obj\h006_000.bmp 172 93 2 5 0 4
pic obj\h006_000.bmp 294 93 2 5 1 4
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
pic obj\h008_000.bmp 615 379 2 5 0 4
pic obj\h008_000.bmp 696 379 2 5 1 4
slider opacity obj\h009_000.bmp 155 307
number opacity obj\h011_000.bmp 415 279
step opacity -1 obj\h010_000.bmp 121 311 2 5 0
step opacity 1 obj\h010_000.bmp 471 311 2 5 1
check confirm_saveload obj\h013_000.bmp 603 119 8 5 0
check confirm_overwrite obj\h013_000.bmp 603 147 8 5 1
check confirm_delete obj\h013_000.bmp 603 175 8 5 2
check confirm_quick obj\h013_000.bmp 603 203 8 5 3
check confirm_title obj\h013_000.bmp 603 231 8 5 4
check confirm_exit obj\h013_000.bmp 603 259 8 5 5
pic obj\h013_000.bmp 603 287 8 5 6 4
pic obj\h013_000.bmp 603 315 8 5 7 4
radio express 1 obj\h014_000.bmp 656 597 2 5 0
radio express 0 obj\h014_000.bmp 818 597 2 5 1

screen config2
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h015_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0
button tab2 obj\h003_000.bmp 413 25 3 5 1
button tab3 obj\h003_000.bmp 544 25 3 5 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
pic obj\e004_000.bmp 1152 303 10 5 5 0
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9
radio text 0 obj\h016_000.bmp 64 122 4 5 0
radio auto 0 obj\h016_000.bmp 64 200 4 5 0
radio text 1 obj\h016_000.bmp 186 122 4 5 1
radio auto 1 obj\h016_000.bmp 186 200 4 5 1
radio text 2 obj\h016_000.bmp 308 122 4 5 2
radio auto 2 obj\h016_000.bmp 308 200 4 5 2
radio text 3 obj\h016_000.bmp 430 122 4 5 3
radio auto 3 obj\h016_000.bmp 430 200 4 5 3
pic obj\h018_000.bmp 617 112 2 5 0 4
pic obj\h018_000.bmp 698 112 2 5 1 2
pic obj\h019_000.bmp 845 114 2 5 0 4
pic obj\h019_000.bmp 926 114 2 5 1 4
radio rightclick 0 obj\h020_000.bmp 710 155 5 5 0
radio rightclick 1 obj\h020_000.bmp 832 155 5 5 1
radio rightclick 2 obj\h020_000.bmp 954 155 5 5 2
radio rightclick 3 obj\h020_000.bmp 710 187 5 5 3
radio rightclick 4 obj\h020_000.bmp 832 187 5 5 4
pic obj\h008_000.bmp 617 250 2 5 0 4
pic obj\h008_000.bmp 698 250 2 5 1 4
pic obj\h008_000.bmp 845 250 2 5 0 4
pic obj\h008_000.bmp 926 250 2 5 1 4
check count_color obj\h008_000.bmp 617 344 2 5 0
pic obj\h008_000.bmp 698 344 2 5 1 4
bar obj\h009_000.bmp 155 568 50
pic obj\h010_000.bmp 119 570 2 5 0 4
pic obj\h010_000.bmp 469 570 2 5 1 4

screen config3
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h021_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0
button tab2 obj\h003_000.bmp 413 25 3 5 1
button tab3 obj\h003_000.bmp 544 25 3 5 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
pic obj\e004_000.bmp 1152 303 10 5 5 0
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9
slider master obj\h009_000.bmp 154 128
number master obj\h011_000.bmp 415 100
step master -1 obj\h010_000.bmp 119 130 2 5 0
step master 1 obj\h010_000.bmp 469 130 2 5 1
check mute_master obj\h024_000.bmp 479 95 1 5 0
slider bgm obj\h009_000.bmp 154 206
number bgm obj\h011_000.bmp 415 178
step bgm -1 obj\h010_000.bmp 119 208 2 5 0
step bgm 1 obj\h010_000.bmp 469 208 2 5 1
check mute_bgm obj\h024_000.bmp 479 173 1 5 0
slider se obj\h009_000.bmp 154 284
number se obj\h011_000.bmp 415 256
step se -1 obj\h010_000.bmp 119 286 2 5 0
step se 1 obj\h010_000.bmp 469 286 2 5 1
check mute_se obj\h024_000.bmp 479 251 1 5 0
bar obj\h009_000.bmp 154 362 80
pic obj\h010_000.bmp 119 364 2 5 0 4
pic obj\h010_000.bmp 469 364 2 5 1 4
pic obj\h024_000.bmp 479 329 1 5 0 4
slider voice obj\h009_000.bmp 154 440
number voice obj\h011_000.bmp 415 412
step voice -1 obj\h010_000.bmp 119 442 2 5 0
step voice 1 obj\h010_000.bmp 469 442 2 5 1
check mute_voice obj\h024_000.bmp 479 407 1 5 0
bar obj\h009_000.bmp 154 518 80
pic obj\h010_000.bmp 119 520 2 5 0 4
pic obj\h010_000.bmp 469 520 2 5 1 4
pic obj\h024_000.bmp 479 485 1 5 0 4
pic obj\h008_000.bmp 617 114 2 5 0 4
pic obj\h008_000.bmp 698 114 2 5 1 4

screen log
pic obj\m001_000.bmp 1185 110
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1185 110 500
rows obj\m000_000.bmp 77 124 165 4 160 12
button log_close obj\l001_000.bmp 1240 22 2 5 0
button log_top obj\m003_000.bmp 1180 34 4 5 0
button log_up obj\m003_000.bmp 1180 68 4 5 1
button log_down obj\m003_000.bmp 1180 622 4 5 2
button log_bottom obj\m003_000.bmp 1180 655 4 5 3

screen dialog
pic obj\b001_000.bmp 0 130
pic obj\i000_000.bmp 414 173 1 8 0 0
button yes obj\b003_000.bmp 449 268 2 5 0
button no obj\b003_000.bmp 449 350 2 5 1
pic obj\b002_000.bmp 414 440

screen gt
gtcount obj\l005_000.bmp 1147 607 obj\l007_000.bmp 1173 616 obj\l007_001.bmp 1156 616

screen defaults
default bar_always 1
default opacity 100
default express 1

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

// Test Game 93.
const char kTsumaGibo[] = R"(
screen title
pic obj\d000_000.bmp 0 -128
pic obj\d002_000.bmp 0 558
pic obj\d001_000.bmp 79 2
pic obj\d004_000.bmp 1122 7 1 5 0 0
pic obj\d003_000.bmp 815 640 10 5 7 0
pic obj\d003_000.bmp 1005 640 10 5 9 0
button start obj\d003_000.bmp 135 596 10 5 0
button load obj\d003_000.bmp 321 596 10 5 1
button quickload obj\d003_000.bmp 509 596 10 5 2
button config obj\d003_000.bmp 701 596 10 5 6
button quit obj\d003_000.bmp 885 596 10 5 8
button cg obj\d003_000.bmp 245 640 10 5 3
button scene obj\d003_000.bmp 437 640 10 5 4
button music obj\d003_000.bmp 625 640 10 5 5

screen load
pic obj\e000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\e002_000.bmp 0 0 1 2 0 1
slots obj\e007_000.bmp 26 104 546 136 4 2
slotlabel obj\e008_000.bmp 9 16
slotdigits obj\e008_001.bmp 37 9
slotthumb 8 11
datedigits obj\e009_000.bmp 193 58
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
slotlock obj\e010_000.bmp 446 52
slotdelete obj\e011_000.bmp 473 52
dategap 13
button page0 obj\e006_000.bmp 280 34 10 5 0
button page1 obj\e006_000.bmp 330 34 10 5 1
button page2 obj\e006_000.bmp 380 34 10 5 2
button page3 obj\e006_000.bmp 430 34 10 5 3
button page4 obj\e006_000.bmp 480 34 10 5 4
button page5 obj\e006_000.bmp 530 34 10 5 5
button page6 obj\e006_000.bmp 580 34 10 5 6
button page7 obj\e006_000.bmp 630 34 10 5 7
button page8 obj\e006_000.bmp 680 34 10 5 8
button page9 obj\e006_000.bmp 730 34 10 5 9
button page10 obj\e006_001.bmp 782 34 1 5 0
button nav_save obj\e004_000.bmp 1152 93 9 5 0
button nav_load obj\e004_000.bmp 1152 135 9 5 1
button nav_cg obj\e004_000.bmp 1152 177 9 5 2
button nav_scene obj\e004_000.bmp 1152 219 9 5 3
button nav_music obj\e004_000.bmp 1152 261 9 5 4
button nav_config obj\e004_000.bmp 1152 303 9 5 5
button nav_title obj\e004_000.bmp 1152 345 9 5 6
button nav_quit obj\e004_000.bmp 1152 387 9 5 7
button back obj\e004_000.bmp 1152 469 9 5 8

screen save
pic obj\e000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\e002_000.bmp 0 0 1 2 0 0
slots obj\e007_000.bmp 26 104 546 136 4 2
slotlabel obj\e008_000.bmp 9 16
slotdigits obj\e008_001.bmp 37 9
slotthumb 8 11
datedigits obj\e009_000.bmp 193 58
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
slotlock obj\e010_000.bmp 446 52
slotdelete obj\e011_000.bmp 473 52
dategap 13
button page0 obj\e006_000.bmp 280 34 10 5 0
button page1 obj\e006_000.bmp 330 34 10 5 1
button page2 obj\e006_000.bmp 380 34 10 5 2
button page3 obj\e006_000.bmp 430 34 10 5 3
button page4 obj\e006_000.bmp 480 34 10 5 4
button page5 obj\e006_000.bmp 530 34 10 5 5
button page6 obj\e006_000.bmp 580 34 10 5 6
button page7 obj\e006_000.bmp 630 34 10 5 7
button page8 obj\e006_000.bmp 680 34 10 5 8
button page9 obj\e006_000.bmp 730 34 10 5 9
button page10 obj\e006_001.bmp 782 34 1 5 0
button nav_save obj\e004_000.bmp 1152 93 9 5 0
button nav_load obj\e004_000.bmp 1152 135 9 5 1
button nav_cg obj\e004_000.bmp 1152 177 9 5 2
button nav_scene obj\e004_000.bmp 1152 219 9 5 3
button nav_music obj\e004_000.bmp 1152 261 9 5 4
button nav_config obj\e004_000.bmp 1152 303 9 5 5
button nav_title obj\e004_000.bmp 1152 345 9 5 6
button nav_quit obj\e004_000.bmp 1152 387 9 5 7
button back obj\e004_000.bmp 1152 469 9 5 8

screen cg
pic obj\f000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\f001_000.bmp 0 0 1 2 0 0
cellsheet obj\f003_000.bmp
cellsize 186 156
cellthumb 8 26
cell 279 82
cell 478 82
cell 677 82
cell 279 245
cell 478 245
cell 677 245
cell 279 408
cell 478 408
cell 677 408
celllabel obj\e008_000.bmp 10 15
celldigits obj\e008_001.bmp 38 10 3 14
cellcount obj\f004_000.bmp obj\f004_001.bmp 100 134 134 149
gallerytabs
button page0 obj\e006_000.bmp 280 34 10 5 0
button gtab0 obj\f002_000.bmp 8 654 3 5 0
button gtab1 obj\f002_000.bmp 384 654 3 5 1
button gtab2 obj\f002_000.bmp 760 654 3 5 2
button nav_save obj\e004_000.bmp 1152 93 9 5 0
button nav_load obj\e004_000.bmp 1152 135 9 5 1
button nav_cg obj\e004_000.bmp 1152 177 9 5 2
button nav_scene obj\e004_000.bmp 1152 219 9 5 3
button nav_music obj\e004_000.bmp 1152 261 9 5 4
button nav_config obj\e004_000.bmp 1152 303 9 5 5
button nav_title obj\e004_000.bmp 1152 345 9 5 6
button nav_quit obj\e004_000.bmp 1152 387 9 5 7
button back obj\e004_000.bmp 1152 469 9 5 8

screen scene
pic obj\f000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\f001_000.bmp 0 0 1 2 0 1
cellsheet obj\f003_000.bmp
cellsize 186 156
cellthumb 8 26
cell 279 82
cell 478 82
cell 677 82
cell 279 245
cell 478 245
cell 677 245
cell 279 408
cell 478 408
cell 677 408
celllabel obj\e008_000.bmp 10 15
celldigits obj\e008_001.bmp 38 10 3 14
cellcount obj\f004_000.bmp obj\f004_001.bmp 100 134 134 149
gallerytabs
button page0 obj\e006_000.bmp 280 34 10 5 0
button gtab0 obj\f002_000.bmp 8 654 3 5 0
button gtab1 obj\f002_000.bmp 384 654 3 5 1
button gtab2 obj\f002_000.bmp 760 654 3 5 2
button nav_save obj\e004_000.bmp 1152 93 9 5 0
button nav_load obj\e004_000.bmp 1152 135 9 5 1
button nav_cg obj\e004_000.bmp 1152 177 9 5 2
button nav_scene obj\e004_000.bmp 1152 219 9 5 3
button nav_music obj\e004_000.bmp 1152 261 9 5 4
button nav_config obj\e004_000.bmp 1152 303 9 5 5
button nav_title obj\e004_000.bmp 1152 345 9 5 6
button nav_quit obj\e004_000.bmp 1152 387 9 5 7
button back obj\e004_000.bmp 1152 469 9 5 8

screen music
pic obj\g000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\g001_000.bmp 0 0
pic obj\g004_000.bmp 346 676
slider music obj\g004_001.bmp 373 681
number music obj\g005_000.bmp 461 659
button music_prev obj\g003_000.bmp 20 668 7 5 0
button music_play obj\g003_000.bmp 64 668 7 5 1
button music_pause obj\g003_000.bmp 108 668 7 5 2
button music_stop obj\g003_000.bmp 152 668 7 5 3
button music_next obj\g003_000.bmp 196 668 7 5 4
button music_repeat obj\g003_000.bmp 240 668 7 5 5
button music_shuffle obj\g003_000.bmp 284 668 7 5 6
button track0 obj\g002_000.bmp 84 168 8 5 0
button track1 obj\g002_000.bmp 84 272 8 5 1
button track2 obj\g002_000.bmp 84 376 8 5 2
button track3 obj\g002_000.bmp 84 480 8 5 3
button track4 obj\g002_000.bmp 604 168 8 5 4
button track5 obj\g002_000.bmp 604 272 8 5 5
button track6 obj\g002_000.bmp 604 376 8 5 6
button track7 obj\g002_000.bmp 604 480 8 5 7
button nav_save obj\e004_000.bmp 1152 93 9 5 0
button nav_load obj\e004_000.bmp 1152 135 9 5 1
button nav_cg obj\e004_000.bmp 1152 177 9 5 2
button nav_scene obj\e004_000.bmp 1152 219 9 5 3
button nav_music obj\e004_000.bmp 1152 261 9 5 4
button nav_config obj\e004_000.bmp 1152 303 9 5 5
button nav_title obj\e004_000.bmp 1152 345 9 5 6
button nav_quit obj\e004_000.bmp 1152 387 9 5 7
button back obj\e004_000.bmp 1152 469 9 5 8

screen config
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h005_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0
button tab2 obj\h003_000.bmp 413 25 3 5 1
button tab3 obj\h003_000.bmp 544 25 3 5 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2
button nav_save obj\e004_000.bmp 1152 93 9 5 0
button nav_load obj\e004_000.bmp 1152 135 9 5 1
button nav_cg obj\e004_000.bmp 1152 177 9 5 2
button nav_scene obj\e004_000.bmp 1152 219 9 5 3
button nav_music obj\e004_000.bmp 1152 261 9 5 4
button nav_config obj\e004_000.bmp 1152 303 9 5 5
button nav_title obj\e004_000.bmp 1152 345 9 5 6
button nav_quit obj\e004_000.bmp 1152 387 9 5 7
button back obj\e004_000.bmp 1152 469 9 5 8
pic obj\h006_000.bmp 172 93 2 5 0 4
pic obj\h006_000.bmp 294 93 2 5 1 4
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
pic obj\h008_000.bmp 615 379 2 5 0 4
pic obj\h008_000.bmp 696 379 2 5 1 4
slider opacity obj\h009_000.bmp 155 307
number opacity obj\h011_000.bmp 415 279
step opacity -1 obj\h010_000.bmp 121 311 2 5 0
step opacity 1 obj\h010_000.bmp 471 311 2 5 1
check confirm_saveload obj\h013_000.bmp 603 119 8 5 0
check confirm_overwrite obj\h013_000.bmp 603 147 8 5 1
check confirm_delete obj\h013_000.bmp 603 175 8 5 2
check confirm_quick obj\h013_000.bmp 603 203 8 5 3
check confirm_title obj\h013_000.bmp 603 231 8 5 4
check confirm_exit obj\h013_000.bmp 603 259 8 5 5
pic obj\h013_000.bmp 603 287 8 5 6 4
pic obj\h013_000.bmp 603 315 8 5 7 4

screen config2
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h015_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0
button tab2 obj\h003_000.bmp 413 25 3 5 1
button tab3 obj\h003_000.bmp 544 25 3 5 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2
button nav_save obj\e004_000.bmp 1152 93 9 5 0
button nav_load obj\e004_000.bmp 1152 135 9 5 1
button nav_cg obj\e004_000.bmp 1152 177 9 5 2
button nav_scene obj\e004_000.bmp 1152 219 9 5 3
button nav_music obj\e004_000.bmp 1152 261 9 5 4
button nav_config obj\e004_000.bmp 1152 303 9 5 5
button nav_title obj\e004_000.bmp 1152 345 9 5 6
button nav_quit obj\e004_000.bmp 1152 387 9 5 7
button back obj\e004_000.bmp 1152 469 9 5 8
radio text 0 obj\h016_000.bmp 64 122 4 5 0
radio auto 0 obj\h016_000.bmp 64 200 4 5 0
radio text 1 obj\h016_000.bmp 186 122 4 5 1
radio auto 1 obj\h016_000.bmp 186 200 4 5 1
radio text 2 obj\h016_000.bmp 308 122 4 5 2
radio auto 2 obj\h016_000.bmp 308 200 4 5 2
radio text 3 obj\h016_000.bmp 430 122 4 5 3
radio auto 3 obj\h016_000.bmp 430 200 4 5 3
pic obj\h018_000.bmp 617 112 2 5 0 4
pic obj\h018_000.bmp 698 112 2 5 1 2
pic obj\h019_000.bmp 845 114 2 5 0 4
pic obj\h019_000.bmp 926 114 2 5 1 4
radio rightclick 0 obj\h020_000.bmp 710 155 5 5 0
radio rightclick 1 obj\h020_000.bmp 832 155 5 5 1
radio rightclick 2 obj\h020_000.bmp 954 155 5 5 2
radio rightclick 3 obj\h020_000.bmp 710 187 5 5 3
radio rightclick 4 obj\h020_000.bmp 832 187 5 5 4
pic obj\h008_000.bmp 617 250 2 5 0 4
pic obj\h008_000.bmp 698 250 2 5 1 4
pic obj\h008_000.bmp 845 250 2 5 0 4
pic obj\h008_000.bmp 926 250 2 5 1 4
check count_color obj\h008_000.bmp 617 344 2 5 0
pic obj\h008_000.bmp 698 344 2 5 1 4
bar obj\h009_000.bmp 155 568 50
pic obj\h010_000.bmp 119 570 2 5 0 4
pic obj\h010_000.bmp 469 570 2 5 1 4

screen config3
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h021_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0
button tab2 obj\h003_000.bmp 413 25 3 5 1
button tab3 obj\h003_000.bmp 544 25 3 5 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2
button nav_save obj\e004_000.bmp 1152 93 9 5 0
button nav_load obj\e004_000.bmp 1152 135 9 5 1
button nav_cg obj\e004_000.bmp 1152 177 9 5 2
button nav_scene obj\e004_000.bmp 1152 219 9 5 3
button nav_music obj\e004_000.bmp 1152 261 9 5 4
button nav_config obj\e004_000.bmp 1152 303 9 5 5
button nav_title obj\e004_000.bmp 1152 345 9 5 6
button nav_quit obj\e004_000.bmp 1152 387 9 5 7
button back obj\e004_000.bmp 1152 469 9 5 8
slider master obj\h009_000.bmp 154 128
number master obj\h011_000.bmp 415 100
step master -1 obj\h010_000.bmp 119 130 2 5 0
step master 1 obj\h010_000.bmp 469 130 2 5 1
check mute_master obj\h024_000.bmp 479 95 1 5 0
slider bgm obj\h009_000.bmp 154 206
number bgm obj\h011_000.bmp 415 178
step bgm -1 obj\h010_000.bmp 119 208 2 5 0
step bgm 1 obj\h010_000.bmp 469 208 2 5 1
check mute_bgm obj\h024_000.bmp 479 173 1 5 0
slider se obj\h009_000.bmp 154 284
number se obj\h011_000.bmp 415 256
step se -1 obj\h010_000.bmp 119 286 2 5 0
step se 1 obj\h010_000.bmp 469 286 2 5 1
check mute_se obj\h024_000.bmp 479 251 1 5 0
bar obj\h009_000.bmp 154 362 80
pic obj\h010_000.bmp 119 364 2 5 0 4
pic obj\h010_000.bmp 469 364 2 5 1 4
pic obj\h024_000.bmp 479 329 1 5 0 4
slider voice obj\h009_000.bmp 154 440
number voice obj\h011_000.bmp 415 412
step voice -1 obj\h010_000.bmp 119 442 2 5 0
step voice 1 obj\h010_000.bmp 469 442 2 5 1
check mute_voice obj\h024_000.bmp 479 407 1 5 0
bar obj\h009_000.bmp 154 518 80
pic obj\h010_000.bmp 119 520 2 5 0 4
pic obj\h010_000.bmp 469 520 2 5 1 4
pic obj\h024_000.bmp 479 485 1 5 0 4
pic obj\h008_000.bmp 617 114 2 5 0 4
pic obj\h008_000.bmp 698 114 2 5 1 4

screen log
pic obj\m001_000.bmp 1185 110
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1185 110 500
rows obj\m000_000.bmp 77 124 165 4 160 12
button log_close obj\l001_000.bmp 1240 22 2 5 0
button log_top obj\m003_000.bmp 1180 34 4 5 0
button log_up obj\m003_000.bmp 1180 68 4 5 1
button log_down obj\m003_000.bmp 1180 622 4 5 2
button log_bottom obj\m003_000.bmp 1180 655 4 5 3

screen dialog
pic obj\b001_000.bmp 0 130
pic obj\i000_000.bmp 414 173 1 8 0 0
button yes obj\b003_000.bmp 449 268 2 5 0
button no obj\b003_000.bmp 449 350 2 5 1
pic obj\b002_000.bmp 414 440

screen choice
pic obj\b001_000.bmp 0 130
pic obj\n000_000.bmp 414 173
place 263 90

screen start
pic obj\d005_000.bmp 390 150
button start_prologue obj\d007_000.bmp 449 268 2 5 0
button start_main obj\d007_000.bmp 449 350 2 5 1

screen defaults
default bar_always 1
default opacity 100

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

}  // namespace

const char kFunbagFantasy[] = R"(
screen title
opening 520 920 1010
askerlit
pic obj\b201_000.bmp 0 0 stage 1
pic obj\b201_002.bmp -60 -40 stage 1 follow 1 1.5
pic obj\b202_002.bmp 362 -40 stage 1 follow 0.625 1
pic obj\b000_001.bmp 0 0 stage 1
pic obj\b000_002.bmp 0 603
pic obj\b003_000.bmp 617 408 stage 2
button start obj\b004_000.bmp 23 119 10 5 0 help obj\b005_000.bmp 23 86 1 1 0 0
button load obj\b004_000.bmp 23 159 10 5 1 help obj\b005_001.bmp 23 126 1 1 0 0
button quickload obj\b004_000.bmp 23 199 10 5 2 help obj\b005_002.bmp 23 166 1 1 0 0
button cg obj\b004_000.bmp 23 239 10 5 3 help obj\b005_003.bmp 23 206 1 1 0 0
button scene obj\b004_000.bmp 23 279 10 5 4 help obj\b005_004.bmp 23 246 1 1 0 0
button music obj\b004_000.bmp 23 319 10 5 5 help obj\b005_005.bmp 23 286 1 1 0 0
button config obj\b004_000.bmp 23 359 10 5 6 help obj\b005_006.bmp 23 326 1 1 0 0
button quit obj\b004_000.bmp 23 559 10 5 9 help obj\b005_009.bmp 23 526 1 1 0 0

screen load
pic obj\e001_000.bmp 0 0
slots obj\e002_000.bmp 44 93 162 140 4 5 1 20
slotrows 1
slotthumb 9 20
slotempty obj\d004_000.bmp 50 52
slotframe obj\d021_000.bmp 7 18
slotdigits obj\d005_000.bmp 15 6 3
datedigits obj\d008_000.bmp 68 107 12
slotlock obj\d006_000.bmp 10 101
slotdelete obj\d007_000.bmp 28 106
dategap 2
slotquick obj\d010_000.bmp
pagelift 8
button page0 obj\d009_000.bmp 470 661 11 5 0
button page1 obj\d009_000.bmp 509 661 11 5 1
button page2 obj\d009_000.bmp 548 661 11 5 2
button page3 obj\d009_000.bmp 587 661 11 5 3
button page4 obj\d009_000.bmp 626 661 11 5 4
button page5 obj\d009_000.bmp 665 661 11 5 5
button page6 obj\d009_000.bmp 704 661 11 5 6
button page7 obj\d009_000.bmp 743 661 11 5 7
button page8 obj\d009_000.bmp 782 661 11 5 8
button page9 obj\d009_000.bmp 821 661 11 5 9
button page10 obj\d009_000.bmp 860 661 11 5 10
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9

screen save
pic obj\d001_000.bmp 0 0
slots obj\d002_000.bmp 44 93 162 140 4 5 1 20
slotrows 1
slotthumb 9 20
slotempty obj\d004_000.bmp 50 52
slotframe obj\d021_000.bmp 7 18
slotdigits obj\d005_000.bmp 15 6 3
datedigits obj\d008_000.bmp 68 107 12
slotlock obj\d006_000.bmp 10 101
slotdelete obj\d007_000.bmp 28 106
dategap 2
slotquick obj\d010_000.bmp
pagelift 8
button page0 obj\d009_000.bmp 470 661 11 5 0
button page1 obj\d009_000.bmp 509 661 11 5 1
button page2 obj\d009_000.bmp 548 661 11 5 2
button page3 obj\d009_000.bmp 587 661 11 5 3
button page4 obj\d009_000.bmp 626 661 11 5 4
button page5 obj\d009_000.bmp 665 661 11 5 5
button page6 obj\d009_000.bmp 704 661 11 5 6
button page7 obj\d009_000.bmp 743 661 11 5 7
button page8 obj\d009_000.bmp 782 661 11 5 8
button page9 obj\d009_000.bmp 821 661 11 5 9
button page10 obj\d009_000.bmp 860 661 11 5 10
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9

screen cg
pic obj\f001_000.bmp 0 0
cellcard obj\f002_000.bmp
cellempty obj\d003_000.bmp -2 -2
cellframe obj\d021_000.bmp 7 18
cellsize 159 135
cellthumb 9 20
cell 44 93
cell 206 93
cell 368 93
cell 530 93
cell 692 93
cell 64 233
cell 226 233
cell 388 233
cell 550 233
celldigits obj\d005_000.bmp 15 6 3 7
gallerytabs
pagelift 8
button page0 obj\d009_000.bmp 369 47 11 5 0
button page1 obj\d009_000.bmp 408 47 11 5 1
button page2 obj\d009_000.bmp 447 47 11 5 2
button gtab0 obj\f004_000.bmp 20 662 9 5 0
button gtab1 obj\f004_000.bmp 118 662 9 5 1
button gtab2 obj\f004_000.bmp 216 662 9 5 2
button gtab3 obj\f004_000.bmp 314 662 9 5 3
button gtab4 obj\f004_000.bmp 412 662 9 5 4
button gtab5 obj\f004_000.bmp 510 662 9 5 5
button gtab6 obj\f004_000.bmp 608 662 9 5 6
button gtab7 obj\f004_000.bmp 706 662 9 5 7
button gtab8 obj\f004_000.bmp 804 662 9 5 8
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9

screen scene
pic obj\g001_000.bmp 0 0
cellcard obj\g002_000.bmp
cellempty obj\d003_000.bmp -2 -2
cellframe obj\d021_000.bmp 7 18
cellsize 159 135
cellthumb 9 20
cell 44 93
cell 206 93
cell 368 93
cell 530 93
cell 692 93
cell 64 233
cell 226 233
cell 388 233
cell 550 233
celldigits obj\d005_000.bmp 15 6 3 7
gallerytabs
pagelift 8
button page0 obj\d009_000.bmp 369 47 11 5 0
button page1 obj\d009_000.bmp 408 47 11 5 1
button page2 obj\d009_000.bmp 447 47 11 5 2
button gtab0 obj\f004_000.bmp 20 662 9 5 0
button gtab1 obj\f004_000.bmp 118 662 9 5 1
button gtab2 obj\f004_000.bmp 216 662 9 5 2
button gtab3 obj\f004_000.bmp 314 662 9 5 3
button gtab4 obj\f004_000.bmp 412 662 9 5 4
button gtab5 obj\f004_000.bmp 510 662 9 5 5
button gtab6 obj\f004_000.bmp 608 662 9 5 6
button gtab7 obj\f004_000.bmp 706 662 9 5 7
button gtab8 obj\f004_000.bmp 804 662 9 5 8
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9

screen music
pic obj\h001_000.bmp 0 0
cellcard obj\h002_000.bmp
cellempty obj\d003_000.bmp -2 -2
celltitle 9 38
cellframe obj\d021_000.bmp 7 18
cellsize 159 135
cell 44 93
cell 206 93
cell 368 93
cell 530 93
cell 692 93
cell 64 233
cell 226 233
cell 388 233
cell 550 233
cell 712 233
cell 84 373
cell 246 373
cell 408 373
cell 570 373
cell 732 373
button page0 obj\d009_000.bmp 810 520 11 5 0
button page1 obj\d009_000.bmp 849 520 11 5 1
slider music obj\h009_001.bmp 622 598
tracktime obj\h007_000.bmp 130 635 780 635 10
trackseek obj\h008_000.bmp 205 650
button music_prev obj\h005_000.bmp 122 590 5 5 0
button music_toggle obj\h005_000.bmp 211 590 5 5 2
button music_stop obj\h005_000.bmp 300 590 5 5 3
button music_next obj\h005_000.bmp 388 590 5 5 4
pic obj\h006_000.bmp 482 590 3 5 0 0
button music_repeat obj\h006_000.bmp 513 590 3 5 1
button music_shuffle obj\h006_000.bmp 544 590 3 5 2
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9

screen config
pic obj\c000_000.bmp 0 0
pic obj\c003_000.bmp 24 397
pic obj\c003_001.bmp 24 568 alpha 60
pic obj\c003_002.bmp 24 568
button tab obj\c002_000.bmp 326 70 3 5 0
button tab2 obj\c002_000.bmp 444 70 3 5 1
button tab3 obj\c002_000.bmp 562 70 3 5 2
button cfg_defaults obj\c001_000.bmp 680 11 3 5 0
button cfg_undo obj\c001_000.bmp 785 38 3 5 1
button cfg_apply obj\c001_000.bmp 890 65 3 5 2
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9
pic obj\c005_000.bmp 132 127 2 5 0 4
pic obj\c005_000.bmp 239 127 2 5 1 4
pic obj\c005_001.bmp 132 164 6 5 0 4
pic obj\c005_001.bmp 239 164 6 5 1 4
pic obj\c005_001.bmp 346 164 6 5 2 4
pic obj\c005_001.bmp 132 192 6 5 3 4
pic obj\c005_001.bmp 239 192 6 5 4 4
pic obj\c005_001.bmp 346 192 6 5 5 4
pic obj\c005_002.bmp 27 247 2 5 0 4
pic obj\c005_002.bmp 97 247 2 5 1 4
pic obj\c005_002.bmp 180 247 2 5 0 4
pic obj\c005_002.bmp 250 247 2 5 1 4
pic obj\c005_002.bmp 333 247 2 5 0 4
pic obj\c005_002.bmp 403 247 2 5 1 4
pic obj\c005_002.bmp 487 391 2 5 0 4
pic obj\c005_002.bmp 557 391 2 5 1 4
slider opacity obj\c005_003.bmp 119 326
number opacity obj\c005_005.bmp 343 294
step opacity -1 obj\c005_004.bmp 88 328 2 5 0
step opacity 1 obj\c005_004.bmp 392 328 2 5 1
check confirm_saveload obj\c005_006.bmp 499 149 8 5 0
check confirm_overwrite obj\c005_006.bmp 499 175 8 5 1
check confirm_delete obj\c005_006.bmp 499 201 8 5 2
check confirm_quick obj\c005_006.bmp 499 227 8 5 3
check confirm_title obj\c005_006.bmp 499 253 8 5 4
check confirm_exit obj\c005_006.bmp 499 279 8 5 5
pic obj\c005_006.bmp 499 305 8 5 6 4
pic obj\c005_006.bmp 499 331 8 5 7 4

screen config2
pic obj\c000_001.bmp 0 0
pic obj\c003_000.bmp 24 397
pic obj\c003_001.bmp 24 568 alpha 60
pic obj\c003_002.bmp 24 568
button tab obj\c002_000.bmp 326 70 3 5 0
button tab2 obj\c002_000.bmp 444 70 3 5 1
button tab3 obj\c002_000.bmp 562 70 3 5 2
button cfg_defaults obj\c001_000.bmp 680 11 3 5 0
button cfg_undo obj\c001_000.bmp 785 38 3 5 1
button cfg_apply obj\c001_000.bmp 890 65 3 5 2
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9
radio text 0 obj\c006_000.bmp 39 155 4 5 0
radio auto 0 obj\c006_000.bmp 39 222 4 5 0
radio text 1 obj\c006_000.bmp 115 155 4 5 1
radio auto 1 obj\c006_000.bmp 115 222 4 5 1
radio text 2 obj\c006_000.bmp 191 155 4 5 2
radio auto 2 obj\c006_000.bmp 191 222 4 5 2
radio text 3 obj\c006_000.bmp 267 155 4 5 3
radio auto 3 obj\c006_000.bmp 267 222 4 5 3
pic obj\c006_001.bmp 520 145 2 5 0 4
pic obj\c006_001.bmp 591 145 2 5 1 2
pic obj\c006_002.bmp 718 145 2 5 0 4
pic obj\c006_002.bmp 790 145 2 5 1 4
radio rightclick 0 obj\c006_003.bmp 601 182 5 5 0
radio rightclick 1 obj\c006_003.bmp 707 182 5 5 1
radio rightclick 2 obj\c006_003.bmp 813 182 5 5 2
radio rightclick 3 obj\c006_003.bmp 601 211 5 5 3
radio rightclick 4 obj\c006_003.bmp 707 211 5 5 4
pic obj\c005_002.bmp 520 265 2 5 0 4
pic obj\c005_002.bmp 591 265 2 5 1 4
pic obj\c005_002.bmp 718 265 2 5 0 4
pic obj\c005_002.bmp 790 265 2 5 1 4
bar obj\c005_003.bmp 119 313 50
number delay obj\c005_005.bmp 343 281
pic obj\c005_004.bmp 88 315 2 5 0 4
pic obj\c005_004.bmp 392 315 2 5 1 4

screen config3
pic obj\c000_002.bmp 0 0
pic obj\c003_000.bmp 24 397
pic obj\c003_001.bmp 24 568 alpha 60
pic obj\c003_002.bmp 24 568
button tab obj\c002_000.bmp 326 70 3 5 0
button tab2 obj\c002_000.bmp 444 70 3 5 1
button tab3 obj\c002_000.bmp 562 70 3 5 2
button cfg_defaults obj\c001_000.bmp 680 11 3 5 0
button cfg_undo obj\c001_000.bmp 785 38 3 5 1
button cfg_apply obj\c001_000.bmp 890 65 3 5 2
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9
slider master obj\c005_003.bmp 119 168
number master obj\c005_005.bmp 338 135
step master -1 obj\c005_004.bmp 97 170 2 5 0
step master 1 obj\c005_004.bmp 401 170 2 5 1
check mute_master obj\c007_002.bmp 387 135 1 5 0
pic obj\c007_001.bmp 431 135 1 5 0 4
bar obj\c005_003.bmp 581 168 80
pic obj\c005_004.bmp 559 170 2 5 0 4
pic obj\c005_004.bmp 863 170 2 5 1 4
pic obj\c007_002.bmp 849 135 1 5 0 4
pic obj\c007_001.bmp 893 135 1 5 0 4
slider bgm obj\c005_003.bmp 119 256
number bgm obj\c005_005.bmp 338 223
step bgm -1 obj\c005_004.bmp 97 258 2 5 0
step bgm 1 obj\c005_004.bmp 401 258 2 5 1
check mute_bgm obj\c007_002.bmp 387 223 1 5 0
pic obj\c007_001.bmp 431 223 1 5 0 4
slider voice obj\c005_003.bmp 581 256
number voice obj\c005_005.bmp 800 223
step voice -1 obj\c005_004.bmp 559 258 2 5 0
step voice 1 obj\c005_004.bmp 863 258 2 5 1
check mute_voice obj\c007_002.bmp 849 223 1 5 0
pic obj\c007_001.bmp 893 223 1 5 0 4
slider se obj\c005_003.bmp 119 344
number se obj\c005_005.bmp 338 311
step se -1 obj\c005_004.bmp 97 346 2 5 0
step se 1 obj\c005_004.bmp 401 346 2 5 1
check mute_se obj\c007_002.bmp 387 311 1 5 0
pic obj\c007_001.bmp 431 311 1 5 0 4
bar obj\c005_003.bmp 581 344 80
pic obj\c005_004.bmp 559 346 2 5 0 4
pic obj\c005_004.bmp 863 346 2 5 1 4
pic obj\c007_002.bmp 849 311 1 5 0 4
pic obj\c007_001.bmp 893 311 1 5 0 4
pic obj\c005_002.bmp 506 414 2 5 0 4
pic obj\c005_002.bmp 576 414 2 5 1 4
button tabvoices obj\c007_000.bmp 700 414 1 5 0

screen configvoices
over config3 154
cancel tab3
pic obj\c030_000.bmp 243 118
check voice0 obj\c030_001.bmp 426 172 1 5 0
check voice1 obj\c030_002.bmp 427 198 1 5 0
check voice2 obj\c030_003.bmp 428 224 1 5 0
check voice3 obj\c030_004.bmp 429 252 1 5 0
check voice4 obj\c030_005.bmp 431 280 1 5 0
check voice5 obj\c030_006.bmp 431 306 1 5 0
check voice6 obj\c030_007.bmp 432 332 1 5 0
check voice7 obj\c030_008.bmp 433 360 1 5 0
check voice8 obj\c030_009.bmp 434 391 1 5 0
check voice9 obj\c030_010.bmp 435 417 1 5 0
check voice10 obj\c030_011.bmp 436 443 1 5 0
check voice11 obj\c030_012.bmp 438 499 1 5 0
check voice13 obj\c030_014.bmp 646 164 1 5 0
check voice14 obj\c030_015.bmp 647 190 1 5 0
check voice15 obj\c030_016.bmp 648 216 1 5 0
check voice16 obj\c030_017.bmp 649 242 1 5 0
check voice17 obj\c030_018.bmp 650 268 1 5 0
check voice18 obj\c030_019.bmp 651 294 1 5 0
check voice19 obj\c030_020.bmp 652 322 1 5 0
check voice20 obj\c030_021.bmp 653 348 1 5 0
check voice21 obj\c030_022.bmp 654 376 1 5 0
check voice22 obj\c030_023.bmp 655 407 1 5 0
check voice23 obj\c030_024.bmp 656 438 1 5 0
check voice24 obj\c030_025.bmp 658 500 1 5 0
check voice_women obj\c030_013.bmp 439 527 1 5 0
group voice_women voice0 voice1 voice2 voice3 voice4 voice5 voice6 voice7 voice8 voice9 voice10 voice11
check voice_men obj\c030_026.bmp 659 528 1 5 0
group voice_men voice13 voice14 voice15 voice16 voice17 voice18 voice19 voice20 voice21 voice22 voice23 voice24
button tab3 obj\r002_000.bmp 945 147 1 5 0

screen log
dim 200
pic obj\r000_001.bmp 1157 11
scroll obj\r000_002.bmp obj\r000_003.bmp obj\r000_004.bmp 1175 100 520
rows obj\r000_000.bmp 77 69 165 4 140 47
button log_close obj\r002_000.bmp 1231 14 1 5 0
button log_top obj\r001_000.bmp 1172 28 4 5 0
button log_up obj\r001_000.bmp 1172 65 4 5 1
button log_down obj\r001_000.bmp 1172 624 4 5 2
button log_bottom obj\r001_000.bmp 1172 661 4 5 3

screen dialog
dim 0
question obj\m000_000.bmp 243 118
question obj\m000_001.bmp 243 118
question obj\m000_002.bmp 243 118
question obj\m000_003.bmp 243 118
question obj\m000_004.bmp 243 118
question obj\m000_005.bmp 243 118
question obj\m000_006.bmp 243 118
question obj\m000_007.bmp 243 118
pic obj\m002_000.bmp 539 481 1 5 0 0
button yes obj\a003_001.bmp 492 333 2 5 0
button no obj\a003_001.bmp 715 324 2 5 1

screen start
after 0
flow MainFlow1
pic obj\b201_000.bmp 0 0
pic obj\b201_002.bmp -60 -40 follow 1 1.5
pic obj\b202_002.bmp 362 -40 follow 0.625 1
pic obj\b000_001.bmp 0 0
pic obj\b000_002.bmp 0 603
pic obj\b003_000.bmp 617 408
pic obj\b004_000.bmp 23 119 10 5 0 2
pic obj\b004_000.bmp 23 159 10 5 1 0
pic obj\b004_000.bmp 23 199 10 5 2 0
pic obj\b004_000.bmp 23 239 10 5 3 0
pic obj\b004_000.bmp 23 279 10 5 4 0
pic obj\b004_000.bmp 23 319 10 5 5 0
pic obj\b004_000.bmp 23 359 10 5 6 0
pic obj\b004_000.bmp 23 559 10 5 9 0
pic obj\b005_000.bmp 23 86
pic obj\b010_000.bmp 243 118
button start_prologue obj\b010_001.bmp 434 283 2 5 0
button start_main obj\b010_001.bmp 437 362 2 5 1

screen choice
framefor 2 obj\k001_000.bmp 243 118
framefor 5 obj\k002_000.bmp 243 25
placefor 2 283 79 340 3
placefor 5 129 78 328 6

screen defaults
default voice_women 1
default voice_men 1
voicejoint 30 5 6
voicejoint 31 11 24
voicejoint 32 4 5 6 8 7
voicejoint 33 9 17 3
voicejoint 34 5 6 8 7
voicejoint 35 0 10 1
voicejoint 36 9 7 8
voicejoint 37 4 5 6
voicejoint 38 7 8
voicejoint 39 9 10
voicejoint 40 1 0
voicejoint 41 4 8
quickslots 201 220
default bar_always 1
default delay 100

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
run 2000 1 100
run 2004 8 80
run 2120 9 1
run 2160 1 1
)";

const char kFunbag4[] = R"(
screen title
askerlit
pic obj\c000_000.bmp 0 0
pic obj\c000_001.bmp 0 0
pic obj\d000_000.bmp 435 0
pic obj\d001_000.bmp 0 0
pic obj\c000_002.bmp 0 660
pic obj\d004_000.bmp 568 687
button start obj\d003_000.bmp 58 355 6 5 0
button load obj\d003_000.bmp 220 355 6 5 1
button quickload obj\d003_000.bmp 382 355 6 5 2
button cg obj\d003_000.bmp 58 443 6 5 3
button scene obj\d003_000.bmp 220 443 6 5 4
button music obj\d003_000.bmp 382 443 6 5 5
button config obj\d003_001.bmp 173 536 3 5 0
button quit obj\d003_001.bmp 301 536 3 5 2

screen load
pic obj\e000_001.bmp 0 0
pic obj\e005_000.bmp 339 46
pic obj\e001_000.bmp 0 0 1 2 0 1
pic obj\e002_000.bmp 1072 0
slots obj\e007_000.bmp 30 140 521 138 4 2
slotlabel obj\e008_000.bmp 9 10
slotdigits obj\e008_001.bmp 37 6
slotthumb 13 20
datedigits obj\e009_000.bmp 190 35
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
timeat 218 60
slotlock obj\e011_000.bmp 9 92
slotdelete obj\e011_001.bmp 448 8
button page0 obj\e005_001.bmp 411 26 11 5 0
button page1 obj\e005_001.bmp 471 26 11 5 1
button page2 obj\e005_001.bmp 531 26 11 5 2
button page3 obj\e005_001.bmp 591 26 11 5 3
button page4 obj\e005_001.bmp 651 26 11 5 4
button page5 obj\e005_001.bmp 711 26 11 5 5
button page6 obj\e005_001.bmp 771 26 11 5 6
button page7 obj\e005_001.bmp 831 26 11 5 7
button page8 obj\e005_001.bmp 891 26 11 5 8
button page9 obj\e005_001.bmp 951 26 11 5 9
button page10 obj\e005_001.bmp 1011 26 11 5 10
button nav_save obj\e003_000.bmp 1109 59 9 5 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1
button nav_cg obj\e003_000.bmp 1117 175 9 5 2
button nav_scene obj\e003_000.bmp 1121 233 9 5 3
button nav_music obj\e003_000.bmp 1125 291 9 5 4
button nav_config obj\e003_000.bmp 1129 349 9 5 5
button nav_title obj\e003_000.bmp 1133 407 9 5 6
button nav_quit obj\e003_000.bmp 1137 465 9 5 7
button back obj\e003_000.bmp 1145 581 9 5 8

screen save
pic obj\e000_000.bmp 0 0
pic obj\e005_000.bmp 339 46
pic obj\e001_000.bmp 0 0 1 2 0 0
pic obj\e002_000.bmp 1072 0
slots obj\e007_000.bmp 30 140 521 138 4 2
slotlabel obj\e008_000.bmp 9 10
slotdigits obj\e008_001.bmp 37 6
slotthumb 13 20
datedigits obj\e009_000.bmp 190 35
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
timeat 218 60
slotlock obj\e011_000.bmp 9 92
slotdelete obj\e011_001.bmp 448 8
button page0 obj\e005_001.bmp 411 26 11 5 0
button page1 obj\e005_001.bmp 471 26 11 5 1
button page2 obj\e005_001.bmp 531 26 11 5 2
button page3 obj\e005_001.bmp 591 26 11 5 3
button page4 obj\e005_001.bmp 651 26 11 5 4
button page5 obj\e005_001.bmp 711 26 11 5 5
button page6 obj\e005_001.bmp 771 26 11 5 6
button page7 obj\e005_001.bmp 831 26 11 5 7
button page8 obj\e005_001.bmp 891 26 11 5 8
button page9 obj\e005_001.bmp 951 26 11 5 9
button page10 obj\e005_001.bmp 1011 26 11 5 10
button nav_save obj\e003_000.bmp 1109 59 9 5 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1
button nav_cg obj\e003_000.bmp 1117 175 9 5 2
button nav_scene obj\e003_000.bmp 1121 233 9 5 3
button nav_music obj\e003_000.bmp 1125 291 9 5 4
button nav_config obj\e003_000.bmp 1129 349 9 5 5
button nav_title obj\e003_000.bmp 1133 407 9 5 6
button nav_quit obj\e003_000.bmp 1137 465 9 5 7
button back obj\e003_000.bmp 1145 581 9 5 8

screen cg
pic obj\f000_000.bmp 0 0 tab 0
pic obj\f000_001.bmp 0 0 tab 1
pic obj\f000_002.bmp 0 0 tab 2
pic obj\f000_004.bmp 0 0 tab 3
pic obj\f000_003.bmp 0 0 tab 4
pic obj\f000_005.bmp 0 0 tab 5
pic obj\f001_000.bmp 0 0 1 2 0 0
pic obj\f006_000.bmp 0 0 tab 0
pic obj\f006_001.bmp 0 0 tab 1
pic obj\f006_002.bmp 0 0 tab 2
pic obj\f006_004.bmp 0 0 tab 3
pic obj\f006_003.bmp 0 0 tab 4
pic obj\f006_005.bmp 0 0 tab 5
pic obj\e002_000.bmp 1072 0
pic obj\f002_000.bmp 0 657
cellsheet obj\f004_000.bmp
cellsize 186 156
cellthumb 12 30
cell 433 62
cell 633 62
cell 833 62
cell 454 232
cell 654 232
cell 854 232
cell 465 402
cell 665 402
cell 865 402
celllabel obj\e008_000.bmp 8 12
celldigits obj\e008_001.bmp 36 8 3 14
cellcount obj\f005_000.bmp obj\f005_001.bmp 102 126 134 147 2
gallerytabs
cellhide
button page0 obj\e005_001.bmp 28 638 11 5 0
button page1 obj\e005_001.bmp 88 638 11 5 1
button page2 obj\e005_001.bmp 148 638 11 5 2
button gtab0 obj\f003_000.bmp 314 580 6 5 0
button gtab1 obj\f003_000.bmp 438 580 6 5 1
button gtab2 obj\f003_000.bmp 562 580 6 5 2
button gtab3 obj\f003_000.bmp 686 580 6 5 4
button gtab4 obj\f003_000.bmp 810 580 6 5 3
button gtab5 obj\f003_000.bmp 934 580 6 5 5
button nav_save obj\e003_000.bmp 1109 59 9 5 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1
button nav_cg obj\e003_000.bmp 1117 175 9 5 2
button nav_scene obj\e003_000.bmp 1121 233 9 5 3
button nav_music obj\e003_000.bmp 1125 291 9 5 4
button nav_config obj\e003_000.bmp 1129 349 9 5 5
button nav_title obj\e003_000.bmp 1133 407 9 5 6
button nav_quit obj\e003_000.bmp 1137 465 9 5 7
button back obj\e003_000.bmp 1145 581 9 5 8

screen scene
pic obj\f000_000.bmp 0 0 tab 0
pic obj\f000_001.bmp 0 0 tab 1
pic obj\f000_002.bmp 0 0 tab 2
pic obj\f000_004.bmp 0 0 tab 3
pic obj\f000_003.bmp 0 0 tab 4
pic obj\f000_005.bmp 0 0 tab 5
pic obj\f001_000.bmp 0 0 1 2 0 1
pic obj\f006_000.bmp 0 0 tab 0
pic obj\f006_001.bmp 0 0 tab 1
pic obj\f006_002.bmp 0 0 tab 2
pic obj\f006_004.bmp 0 0 tab 3
pic obj\f006_003.bmp 0 0 tab 4
pic obj\f006_005.bmp 0 0 tab 5
pic obj\e002_000.bmp 1072 0
pic obj\f002_000.bmp 0 657
cellsheet obj\f004_000.bmp
cellsize 186 156
cellthumb 12 30
cell 433 62
cell 633 62
cell 833 62
cell 454 232
cell 654 232
cell 854 232
cell 465 402
cell 665 402
cell 865 402
celllabel obj\e008_000.bmp 8 12
celldigits obj\e008_001.bmp 36 8 3 14
cellcount obj\f005_000.bmp obj\f005_001.bmp 102 126 134 147 2
gallerytabs
cellhide
button page0 obj\e005_001.bmp 28 638 11 5 0
button page1 obj\e005_001.bmp 88 638 11 5 1
button page2 obj\e005_001.bmp 148 638 11 5 2
button gtab0 obj\f003_000.bmp 314 580 6 5 0
button gtab1 obj\f003_000.bmp 438 580 6 5 1
button gtab2 obj\f003_000.bmp 562 580 6 5 2
button gtab3 obj\f003_000.bmp 686 580 6 5 4
button gtab4 obj\f003_000.bmp 810 580 6 5 3
button gtab5 obj\f003_000.bmp 934 580 6 5 5
button nav_save obj\e003_000.bmp 1109 59 9 5 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1
button nav_cg obj\e003_000.bmp 1117 175 9 5 2
button nav_scene obj\e003_000.bmp 1121 233 9 5 3
button nav_music obj\e003_000.bmp 1125 291 9 5 4
button nav_config obj\e003_000.bmp 1129 349 9 5 5
button nav_title obj\e003_000.bmp 1133 407 9 5 6
button nav_quit obj\e003_000.bmp 1137 465 9 5 7
button back obj\e003_000.bmp 1145 581 9 5 8

screen music
pic obj\g000_000.bmp 0 0
pic obj\g001_000.bmp 0 0
pic obj\e002_000.bmp 1072 0
pic obj\e005_000.bmp 582 46
pic obj\g004_000.bmp 356 54
slider music obj\g004_001.bmp 383 60
number music obj\g005_000.bmp 490 31
button music_prev obj\g003_000.bmp 651 26 7 5 0
button music_play obj\g003_000.bmp 711 26 7 5 1
button music_pause obj\g003_000.bmp 771 26 7 5 2
button music_stop obj\g003_000.bmp 831 26 7 5 3
button music_next obj\g003_000.bmp 891 26 7 5 4
button music_repeat obj\g003_000.bmp 951 26 7 5 5
button music_shuffle obj\g003_000.bmp 1011 26 7 5 6
button track0 obj\g002_000.bmp 29 143 9 5 0
button track1 obj\g002_000.bmp 29 204 9 5 1
button track2 obj\g002_000.bmp 29 265 9 5 2
button track3 obj\g002_000.bmp 29 326 9 5 3
button track4 obj\g002_000.bmp 29 387 9 5 4
button track5 obj\g002_000.bmp 29 448 9 5 5
button track6 obj\g002_000.bmp 29 509 9 5 6
button track7 obj\g002_000.bmp 29 570 9 5 7
button track8 obj\g002_000.bmp 29 631 9 5 8
button track9 obj\g002_001.bmp 372 143 9 5 0
button track10 obj\g002_001.bmp 372 204 9 5 1
button track11 obj\g002_001.bmp 372 265 9 5 2
button track12 obj\g002_001.bmp 372 326 9 5 3
button track13 obj\g002_001.bmp 372 387 9 5 4
button track14 obj\g002_001.bmp 372 448 9 5 5
button track15 obj\g002_001.bmp 372 509 9 5 6
button track16 obj\g002_001.bmp 372 570 9 5 7
button track17 obj\g002_001.bmp 372 631 9 5 8
button track18 obj\g002_002.bmp 715 143 9 5 0
button track19 obj\g002_002.bmp 715 204 9 5 1
button track20 obj\g002_002.bmp 715 265 9 5 2
button track21 obj\g002_002.bmp 715 326 9 5 3
button track22 obj\g002_002.bmp 715 387 9 5 4
button track23 obj\g002_002.bmp 715 448 9 5 5
button track24 obj\g002_002.bmp 715 509 9 5 6
button track25 obj\g002_002.bmp 715 570 9 5 7
button track26 obj\g002_002.bmp 715 631 9 5 8
button nav_save obj\e003_000.bmp 1109 59 9 5 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1
button nav_cg obj\e003_000.bmp 1117 175 9 5 2
button nav_scene obj\e003_000.bmp 1121 233 9 5 3
button nav_music obj\e003_000.bmp 1125 291 9 5 4
button nav_config obj\e003_000.bmp 1129 349 9 5 5
button nav_title obj\e003_000.bmp 1133 407 9 5 6
button nav_quit obj\e003_000.bmp 1137 465 9 5 7
button back obj\e003_000.bmp 1145 581 9 5 8

screen config
pic obj\h000_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\e002_000.bmp 1072 0
pic obj\h005_000.bmp 12 151
button tab obj\h003_000.bmp 331 34 3 5 0
button tab2 obj\h003_000.bmp 451 34 3 5 1
button tab3 obj\h003_000.bmp 571 34 3 5 2
button cfg_defaults obj\h002_000.bmp 714 34 3 5 0
button cfg_undo obj\h002_000.bmp 834 34 3 5 1
button cfg_apply obj\h002_000.bmp 954 34 3 5 2
button nav_save obj\e003_000.bmp 1109 59 9 5 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1
button nav_cg obj\e003_000.bmp 1117 175 9 5 2
button nav_scene obj\e003_000.bmp 1121 233 9 5 3
button nav_music obj\e003_000.bmp 1125 291 9 5 4
button nav_config obj\e003_000.bmp 1129 349 9 5 5
button nav_title obj\e003_000.bmp 1133 407 9 5 6
button nav_quit obj\e003_000.bmp 1137 465 9 5 7
button back obj\e003_000.bmp 1145 581 9 5 8
pic obj\h006_000.bmp 141 157 2 5 0 4
pic obj\h006_000.bmp 263 157 2 5 1 4
pic obj\h007_000.bmp 141 199 6 5 0 4
pic obj\h007_000.bmp 263 199 6 5 1 4
pic obj\h007_000.bmp 385 199 6 5 2 4
pic obj\h007_000.bmp 141 231 6 5 3 4
pic obj\h007_000.bmp 263 231 6 5 4 4
pic obj\h007_000.bmp 385 231 6 5 5 4
pic obj\h008_000.bmp 19 295 2 5 0 4
pic obj\h008_000.bmp 100 295 2 5 1 4
pic obj\h008_000.bmp 195 295 2 5 0 4
pic obj\h008_000.bmp 276 295 2 5 1 4
pic obj\h008_000.bmp 371 295 2 5 0 4
pic obj\h008_000.bmp 452 295 2 5 1 4
pic obj\h008_000.bmp 584 443 2 5 0 4
pic obj\h008_000.bmp 665 443 2 5 1 4
slider opacity obj\h009_000.bmp 124 373
number opacity obj\h011_000.bmp 385 345
step opacity -1 obj\h010_000.bmp 89 376 2 5 0
step opacity 1 obj\h010_000.bmp 439 376 2 5 1
check confirm_saveload obj\h013_000.bmp 572 183 8 5 0
check confirm_overwrite obj\h013_000.bmp 572 211 8 5 1
check confirm_delete obj\h013_000.bmp 572 239 8 5 2
check confirm_quick obj\h013_000.bmp 572 267 8 5 3
check confirm_title obj\h013_000.bmp 572 295 8 5 4
check confirm_exit obj\h013_000.bmp 572 323 8 5 5
pic obj\h013_000.bmp 572 351 8 5 6 4
pic obj\h013_000.bmp 572 379 8 5 7 4

screen config2
pic obj\h000_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\e002_000.bmp 1072 0
pic obj\h015_000.bmp 12 151
button tab obj\h003_000.bmp 331 34 3 5 0
button tab2 obj\h003_000.bmp 451 34 3 5 1
button tab3 obj\h003_000.bmp 571 34 3 5 2
button cfg_defaults obj\h002_000.bmp 714 34 3 5 0
button cfg_undo obj\h002_000.bmp 834 34 3 5 1
button cfg_apply obj\h002_000.bmp 954 34 3 5 2
button nav_save obj\e003_000.bmp 1109 59 9 5 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1
button nav_cg obj\e003_000.bmp 1117 175 9 5 2
button nav_scene obj\e003_000.bmp 1121 233 9 5 3
button nav_music obj\e003_000.bmp 1125 291 9 5 4
button nav_config obj\e003_000.bmp 1129 349 9 5 5
button nav_title obj\e003_000.bmp 1133 407 9 5 6
button nav_quit obj\e003_000.bmp 1137 465 9 5 7
button back obj\e003_000.bmp 1145 581 9 5 8
radio text 0 obj\h016_000.bmp 34 189 4 5 0
radio auto 0 obj\h016_000.bmp 34 267 4 5 0
radio text 1 obj\h016_000.bmp 156 189 4 5 1
radio auto 1 obj\h016_000.bmp 156 267 4 5 1
radio text 2 obj\h016_000.bmp 278 189 4 5 2
radio auto 2 obj\h016_000.bmp 278 267 4 5 2
radio text 3 obj\h016_000.bmp 400 189 4 5 3
radio auto 3 obj\h016_000.bmp 400 267 4 5 3
pic obj\h018_000.bmp 587 179 2 5 0 4
pic obj\h018_000.bmp 668 179 2 5 1 2
pic obj\h019_000.bmp 815 179 2 5 0 4
pic obj\h019_000.bmp 896 179 2 5 1 4
radio rightclick 0 obj\h020_000.bmp 680 222 5 5 0
radio rightclick 1 obj\h020_000.bmp 802 222 5 5 1
radio rightclick 2 obj\h020_000.bmp 924 222 5 5 2
radio rightclick 3 obj\h020_000.bmp 680 254 5 5 3
radio rightclick 4 obj\h020_000.bmp 802 254 5 5 4
pic obj\h008_000.bmp 587 317 2 5 0 4
pic obj\h008_000.bmp 668 317 2 5 1 4
pic obj\h008_000.bmp 815 317 2 5 0 4
pic obj\h008_000.bmp 896 317 2 5 1 4
bar obj\h009_000.bmp 124 635 50
pic obj\h010_000.bmp 89 637 2 5 0 4
pic obj\h010_000.bmp 439 637 2 5 1 4

screen config3
pic obj\h000_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\e002_000.bmp 1072 0
pic obj\h021_000.bmp 12 151
button tab obj\h003_000.bmp 331 34 3 5 0
button tab2 obj\h003_000.bmp 451 34 3 5 1
button tab3 obj\h003_000.bmp 571 34 3 5 2
button cfg_defaults obj\h002_000.bmp 714 34 3 5 0
button cfg_undo obj\h002_000.bmp 834 34 3 5 1
button cfg_apply obj\h002_000.bmp 954 34 3 5 2
button nav_save obj\e003_000.bmp 1109 59 9 5 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1
button nav_cg obj\e003_000.bmp 1117 175 9 5 2
button nav_scene obj\e003_000.bmp 1121 233 9 5 3
button nav_music obj\e003_000.bmp 1125 291 9 5 4
button nav_config obj\e003_000.bmp 1129 349 9 5 5
button nav_title obj\e003_000.bmp 1133 407 9 5 6
button nav_quit obj\e003_000.bmp 1137 465 9 5 7
button back obj\e003_000.bmp 1145 581 9 5 8
slider master obj\h009_000.bmp 124 194
number master obj\h011_000.bmp 385 166
step master -1 obj\h010_000.bmp 89 197 2 5 0
step master 1 obj\h010_000.bmp 439 197 2 5 1
check mute_master obj\h024_000.bmp 449 162 1 5 0
slider bgm obj\h009_000.bmp 124 272
number bgm obj\h011_000.bmp 385 244
step bgm -1 obj\h010_000.bmp 89 275 2 5 0
step bgm 1 obj\h010_000.bmp 439 275 2 5 1
check mute_bgm obj\h024_000.bmp 449 240 1 5 0
slider se obj\h009_000.bmp 124 350
number se obj\h011_000.bmp 385 322
step se -1 obj\h010_000.bmp 89 353 2 5 0
step se 1 obj\h010_000.bmp 439 353 2 5 1
check mute_se obj\h024_000.bmp 449 318 1 5 0
bar obj\h009_000.bmp 124 428 80
pic obj\h010_000.bmp 89 431 2 5 0 4
pic obj\h010_000.bmp 439 431 2 5 1 4
pic obj\h024_000.bmp 449 396 1 5 0 4
slider voice obj\h009_000.bmp 124 507
number voice obj\h011_000.bmp 385 479
step voice -1 obj\h010_000.bmp 89 510 2 5 0
step voice 1 obj\h010_000.bmp 439 510 2 5 1
check mute_voice obj\h024_000.bmp 449 475 1 5 0
bar obj\h009_000.bmp 124 585 80
pic obj\h010_000.bmp 89 588 2 5 0 4
pic obj\h010_000.bmp 439 588 2 5 1 4
pic obj\h024_000.bmp 449 553 1 5 0 4
pic obj\h008_000.bmp 587 179 2 5 0 4
pic obj\h008_000.bmp 668 179 2 5 1 4
check voice0 obj\h022_000.bmp 569 250 8 5 0 0 2 4 1
check voice1 obj\h022_000.bmp 569 304 8 5 1 0 2 4 1
check voice2 obj\h022_000.bmp 569 358 8 5 2 0 2 4 1
check voice3 obj\h022_000.bmp 569 412 8 5 3 0 2 4 1
check voice4 obj\h022_000.bmp 569 466 8 5 4 0 2 4 1
check voice5 obj\h022_000.bmp 569 520 8 5 5 0 2 4 1
check voice6 obj\h022_000.bmp 569 574 8 5 6 0 2 4 1
check voice7 obj\h022_000.bmp 569 628 8 5 7 0 2 4 1
check voice8 obj\h022_001.bmp 735 250 8 5 0 0 2 4 1
check voice9 obj\h022_001.bmp 735 304 8 5 1 0 2 4 1
check voice10 obj\h022_001.bmp 735 358 8 5 2 0 2 4 1
check voice11 obj\h022_001.bmp 735 412 8 5 3 0 2 4 1
check voice12 obj\h022_001.bmp 735 466 8 5 4 0 2 4 1
check voice13 obj\h022_001.bmp 735 520 8 5 5 0 2 4 1
check voice14 obj\h022_001.bmp 735 574 8 5 6 0 2 4 1
check voice15 obj\h022_001.bmp 735 628 8 5 7 0 2 4 1
check voice16 obj\h022_002.bmp 901 250 8 5 0 0 2 4 1
check voice17 obj\h022_002.bmp 901 304 8 5 1 0 2 4 1
check voice18 obj\h022_002.bmp 901 358 8 5 2 0 2 4 1
check voice19 obj\h022_002.bmp 901 412 8 5 3 0 2 4 1
check voice20 obj\h022_002.bmp 901 466 8 5 4 0 2 4 1
check voice21 obj\h022_002.bmp 901 520 8 5 5 0 2 4 1
check voice22 obj\h022_002.bmp 901 574 8 5 6 0 2 4 1
check voice23 obj\h022_002.bmp 901 628 8 5 7 0 2 4 1

screen log
pic obj\m001_000.bmp 1172 28
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1184 110 500
dim 0
rows obj\m000_000.bmp 77 69 165 4 100 36
button log_close obj\l002_003.bmp 1228 7 2 5 0
button log_top obj\m003_000.bmp 1180 36 4 5 0
button log_up obj\m003_000.bmp 1180 69 4 5 1
button log_down obj\m003_000.bmp 1180 626 4 5 2
button log_bottom obj\m003_000.bmp 1180 659 4 5 3

screen dialog
dim 0
pic obj\b001_000.bmp 0 174
pic obj\i000_000.bmp 346 220 1 8 0 0
button yes obj\b003_000.bmp 424 320 2 5 0
button no obj\b003_000.bmp 700 320 2 5 1
pic obj\i001_000.bmp 477 452 1 5 0 0

screen choice
pic obj\b001_000.bmp 0 174
pic obj\n000_000.bmp 346 208
placefor 2 290 89 414 0

screen defaults
default opacity 60
voicejoint 25 22 23
voicejoint 26 16 17 18
voicejoint 27 1 2
voicejoint 28 3 4
voicejoint 29 0 3 4

screen cfgsav
run 4 2 1
run 16 1 60
run 20 1 50
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

const char *menuTable(const std::string &folder) {
    if (folder == "waffle\\x2019_retry") return kChristmas;
    if (folder == "waffle\\alchemy") return kAlchemist;
    if (folder == "waffle\\mother3") return kMother3;
    if (folder == "waffle\\tmgb") return kTsumaGibo;
    if (folder == "waffle\\funbagfantasy3") return kFunbagFantasy;
    if (folder == "waffle\\funbag4") return kFunbag4;
    return nullptr;
}

}  // namespace neko
