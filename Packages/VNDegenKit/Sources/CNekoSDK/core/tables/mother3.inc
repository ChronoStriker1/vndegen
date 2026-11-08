// Haha Sannin to Ana Asobi's menus, a raw string that tables.cpp includes and menuTable() hands out.
// Test Game 88.
const char kMother3[] = R"(
screen title
pic obj\d000_000.bmp 0 -128
pic obj\d002_000.bmp 0 558
pic obj\d001_000.bmp 42 2
button versioninfo obj\d004_000.bmp 1122 7 1 5 0 help obj\d005_000.bmp 0 686 1 12 0 11
button sprites obj\d003_000.bmp 174 638 11 5 6 help obj\d005_000.bmp 0 686 1 12 0 6
# The manual and the web survey open in a browser on the PC; here the manual opens in the app and the survey only after
# asking.
button manual obj\d003_000.bmp 532 638 11 5 8 help obj\d005_000.bmp 0 686 1 12 0 8
button websurvey obj\d003_000.bmp 1069 638 11 5 10 help obj\d005_000.bmp 0 686 1 12 0 10
button start obj\d003_000.bmp 59 592 11 5 0 help obj\d005_000.bmp 0 686 1 12 0 0
button load obj\d003_000.bmp 238 592 11 5 1 help obj\d005_000.bmp 0 686 1 12 0 1
button quickload obj\d003_000.bmp 417 592 11 5 2 help obj\d005_000.bmp 0 686 1 12 0 2
button cg obj\d003_000.bmp 596 592 11 5 3 help obj\d005_000.bmp 0 686 1 12 0 3
button scene obj\d003_000.bmp 775 592 11 5 4 help obj\d005_000.bmp 0 686 1 12 0 4
button music obj\d003_000.bmp 954 592 11 5 5 help obj\d005_000.bmp 0 686 1 12 0 5
button config obj\d003_000.bmp 353 638 11 5 7 help obj\d005_000.bmp 0 686 1 12 0 7
button quit obj\d003_000.bmp 711 638 11 5 9 help obj\d005_000.bmp 0 686 1 12 0 9

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
button nav_sprites obj\e004_000.bmp 1152 303 10 5 5
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
button nav_sprites obj\e004_000.bmp 1152 303 10 5 5
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen cg
pic obj\f000_000.bmp 0 0 tab 0
pic obj\f000_002.bmp 0 0 tab 2
pic obj\f000_001.bmp 0 0 tab 1
pic obj\f000_003.bmp 0 0 tab 3
# The three heroines over each tab's room, the sprites the PC reads: Akiho, Ai and Yui, the tab's own heroine in her
# other pose, placed as the PC places them on that tab under Wine.
pic sc\sc_001_002_000_001_000.bmp 252 -58 tab 0
pic sc\sc_002_003_000_001_000.bmp -400 -84 tab 0
pic sc\sc_000_002_000_001_000.bmp 564 -60 tab 0
pic sc\sc_001_002_000_001_000.bmp 328 -56 tab 2
pic sc\sc_002_003_000_001_001.bmp -328 -84 tab 2
pic sc\sc_000_002_000_001_000.bmp 636 -60 tab 2
pic sc\sc_001_002_000_001_001.bmp 180 -56 tab 1
pic sc\sc_002_003_000_001_000.bmp -472 -84 tab 1
pic sc\sc_000_002_000_001_000.bmp 488 -60 tab 1
pic sc\sc_001_002_000_001_000.bmp 180 -56 tab 3
pic sc\sc_002_003_000_001_000.bmp -472 -84 tab 3
pic sc\sc_000_002_000_001_001.bmp 488 -60 tab 3
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
# As on the PC, no cards past a tab's last entry.
cellhide
# The cards move away from the tab's own heroine, as on the PC under Wine.
cellshift 2 80
cellshift 1 -80
cellshift 3 -80
# Pictures seen of the entry's, over how many it has, as the PC draws them.
cellcount obj\f004_000.bmp obj\f004_001.bmp 105 124 133 146
button page0 obj\e006_000.bmp 280 34 10 5 0
button page1 obj\e006_000.bmp 330 34 10 5 1
button gtab0 obj\f002_000.bmp 8 654 4 5 0
button gtab2 obj\f002_000.bmp 288 654 4 5 2
button gtab1 obj\f002_000.bmp 568 654 4 5 1
button gtab3 obj\f002_000.bmp 848 654 4 5 3
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
button nav_sprites obj\e004_000.bmp 1152 303 10 5 5
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen scene
pic obj\f000_000.bmp 0 0 tab 0
pic obj\f000_002.bmp 0 0 tab 2
pic obj\f000_001.bmp 0 0 tab 1
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
# As on the PC, no cards past a tab's last entry.
cellhide
# The cards move away from the tab's own heroine, as on the PC under Wine.
cellshift 2 80
cellshift 1 -80
cellshift 3 -80
button page0 obj\e006_000.bmp 280 34 10 5 0
button page1 obj\e006_000.bmp 330 34 10 5 1
button gtab0 obj\f002_000.bmp 8 654 4 5 0
button gtab2 obj\f002_000.bmp 288 654 4 5 2
button gtab1 obj\f002_000.bmp 568 654 4 5 1
button gtab3 obj\f002_000.bmp 848 654 4 5 3
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
button nav_sprites obj\e004_000.bmp 1152 303 10 5 5
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen music
hover 343 676 534 708 obj\g006_000.bmp 868 669 8 7
pic obj\g000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\g001_000.bmp 0 0
pic obj\g004_000.bmp 343 676
slider music obj\g004_001.bmp 370 681
number music obj\g005_000.bmp 461 659
button music_prev obj\g003_000.bmp 20 668 7 5 0 help obj\g006_000.bmp 868 669 1 8 0 0
button music_play obj\g003_000.bmp 64 668 7 5 1 help obj\g006_000.bmp 868 669 1 8 0 1
button music_pause obj\g003_000.bmp 108 668 7 5 2
button music_stop obj\g003_000.bmp 152 668 7 5 3
button music_next obj\g003_000.bmp 196 668 7 5 4 help obj\g006_000.bmp 868 669 1 8 0 4
button music_repeat obj\g003_000.bmp 240 668 7 5 5 help obj\g006_000.bmp 868 669 1 8 0 5
button music_shuffle obj\g003_000.bmp 284 668 7 5 6 help obj\g006_000.bmp 868 669 1 8 0 6
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
button nav_sprites obj\e004_000.bmp 1152 303 10 5 5
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9

screen config
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 399 203 559 261 obj\h004_000.bmp 350 669 9 4
hover 221 203 387 261 obj\h004_000.bmp 350 669 9 3
hover 47 176 211 261 obj\h004_000.bmp 350 669 9 2
hover 587 352 804 407 obj\h004_000.bmp 350 669 9 7
hover 47 87 559 124 obj\h004_000.bmp 350 669 9 0
hover 47 130 559 198 obj\h004_000.bmp 350 669 9 1
hover 587 416 1098 632 obj\h004_000.bmp 350 669 9 8
hover 587 88 1098 345 obj\h004_000.bmp 350 669 9 6
hover 47 268 559 603 obj\h004_000.bmp 350 669 9 5
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h005_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 413 25 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 544 25 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
button nav_sprites obj\e004_000.bmp 1152 303 10 5 5
button nav_config obj\e004_000.bmp 1152 345 10 5 6
button nav_title obj\e004_000.bmp 1152 387 10 5 7
button nav_quit obj\e004_000.bmp 1152 471 10 5 8
button back obj\e004_000.bmp 1152 513 10 5 9
radio screen 0 obj\h006_000.bmp 172 93 2 5 0
radio screen 1 obj\h006_000.bmp 294 93 2 5 1
pic obj\h007_000.bmp 172 133 6 5 0 4
pic obj\h007_000.bmp 294 133 6 5 1 4
pic obj\h007_000.bmp 416 133 6 5 2 4
pic obj\h007_000.bmp 172 165 6 5 3 4
pic obj\h007_000.bmp 294 165 6 5 4 4
pic obj\h007_000.bmp 416 165 6 5 5 4
radio effect 1 obj\h008_000.bmp 50 229 2 5 0
radio effect 0 obj\h008_000.bmp 131 229 2 5 1
radio wait 1 obj\h008_000.bmp 226 229 2 5 0
radio wait 0 obj\h008_000.bmp 307 229 2 5 1
radio waitskip 1 obj\h008_000.bmp 402 229 2 5 0
radio waitskip 0 obj\h008_000.bmp 483 229 2 5 1
radio camera 1 obj\h008_000.bmp 615 379 2 5 0
radio camera 0 obj\h008_000.bmp 696 379 2 5 1
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
check tutorial obj\h013_000.bmp 603 287 8 5 6
pic obj\h013_000.bmp 603 315 8 5 7 4
radio express 1 obj\h014_000.bmp 656 597 2 5 0
radio express 0 obj\h014_000.bmp 818 597 2 5 1

screen config2
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 584 291 804 346 obj\h004_002.bmp 362 669 6 5
hover 584 87 807 147 obj\h004_002.bmp 362 669 6 0
hover 811 87 1034 147 obj\h004_002.bmp 362 669 6 1
hover 584 226 804 283 obj\h004_002.bmp 362 669 6 3
hover 812 226 1033 283 obj\h004_002.bmp 362 669 6 4
hover 45 95 560 159 obj\h004_001.bmp 380 669 3 0
hover 584 153 1098 219 obj\h004_002.bmp 362 669 6 2
hover 45 169 560 237 obj\h004_001.bmp 380 669 3 1
hover 45 532 560 604 obj\h004_001.bmp 380 669 3 2
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h015_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 413 25 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 544 25 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
button nav_sprites obj\e004_000.bmp 1152 303 10 5 5
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
radio skipall 0 obj\h018_000.bmp 617 112 2 5 0
radio skipall 1 obj\h018_000.bmp 698 112 2 5 1
radio deckey 0 obj\h019_000.bmp 845 112 2 5 0
radio deckey 1 obj\h019_000.bmp 926 112 2 5 1
radio rightclick 0 obj\h020_000.bmp 710 155 5 5 0
radio rightclick 1 obj\h020_000.bmp 832 155 5 5 1
radio rightclick 2 obj\h020_000.bmp 954 155 5 5 2
radio rightclick 3 obj\h020_000.bmp 710 187 5 5 3
radio rightclick 4 obj\h020_000.bmp 832 187 5 5 4
radio pause_inactive 1 obj\h008_000.bmp 617 250 2 5 0
radio pause_inactive 0 obj\h008_000.bmp 698 250 2 5 1
# The PC's Electric Onahole drives a USB toy. VNDegen keeps the choice and does nothing with it.
radio onahole 1 obj\h008_000.bmp 845 250 2 5 0
radio onahole 0 obj\h008_000.bmp 926 250 2 5 1
radio count_color 1 obj\h008_000.bmp 617 314 2 5 0
radio count_color 0 obj\h008_000.bmp 698 314 2 5 1
# Wait time, ×0.70 to ×1.30 in steps of 0.01, stretches waits and fades; the bar is full at ×2.00.
range waittime 70 130
slider waittime obj\h009_000.bmp 155 568 200
number waittime obj\h011_000.bmp 409 540 429 449
step waittime -1 obj\h010_000.bmp 119 570 2 5 0
step waittime 1 obj\h010_000.bmp 469 570 2 5 1

screen config3
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 585 88 806 146 obj\h004_003.bmp 372 669 8 6
hover 46 88 560 159 obj\h004_003.bmp 372 669 8 0
hover 46 166 560 238 obj\h004_003.bmp 372 669 8 1
hover 46 245 560 314 obj\h004_003.bmp 372 669 8 2
hover 46 323 560 392 obj\h004_003.bmp 372 669 8 3
hover 46 400 560 471 obj\h004_003.bmp 372 669 8 4
hover 46 479 560 550 obj\h004_003.bmp 372 669 8 5
hover 585 153 1100 625 obj\h004_003.bmp 372 669 8 7
pic obj\h000_000.bmp 0 0
pic obj\e003_000.bmp 1138 0
pic obj\h001_000.bmp 0 0
pic obj\h021_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 413 25 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 544 25 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save obj\e004_000.bmp 1152 93 10 5 0
button nav_load obj\e004_000.bmp 1152 135 10 5 1
button nav_cg obj\e004_000.bmp 1152 177 10 5 2
button nav_scene obj\e004_000.bmp 1152 219 10 5 3
button nav_music obj\e004_000.bmp 1152 261 10 5 4
button nav_sprites obj\e004_000.bmp 1152 303 10 5 5
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
slider hse obj\h009_000.bmp 154 362
number hse obj\h011_000.bmp 415 334
step hse -1 obj\h010_000.bmp 119 364 2 5 0
step hse 1 obj\h010_000.bmp 469 364 2 5 1
check mute_hse obj\h024_000.bmp 479 329 1 5 0
slider voice obj\h009_000.bmp 154 440
number voice obj\h011_000.bmp 415 412
step voice -1 obj\h010_000.bmp 119 442 2 5 0
step voice 1 obj\h010_000.bmp 469 442 2 5 1
check mute_voice obj\h024_000.bmp 479 407 1 5 0
slider backvoice obj\h009_000.bmp 154 518
number backvoice obj\h011_000.bmp 415 490
step backvoice -1 obj\h010_000.bmp 119 520 2 5 0
step backvoice 1 obj\h010_000.bmp 469 520 2 5 1
check mute_backvoice obj\h024_000.bmp 479 485 1 5 0
# クリック時、ボイスを止める.
radio voice_stop 1 obj\h008_000.bmp 617 114 2 5 0
radio voice_stop 0 obj\h008_000.bmp 698 114 2 5 1
# Each ♪ plays the sample the PC reads for it under Wine; master and music have none, as on the PC.
pic obj\h023_000.bmp 533 95 1 5 0 4
pic obj\h023_000.bmp 533 173 1 5 0 4
button test:se:se\se_05.ogg obj\h023_000.bmp 533 251 1 5 0
button test:se:se\hse_002_005.ogg obj\h023_000.bmp 533 329 1 5 0
button test:voice:voice\ai0_0161.ogg obj\h023_000.bmp 533 407 1 5 0
button test:voice:voice\ai9_0003.ogg obj\h023_000.bmp 533 485 1 5 0
check voice0 obj\h022_000.bmp 612 528 5 5 0
check voice1 obj\h022_000.bmp 785 528 5 5 1
check voice2 obj\h022_000.bmp 958 528 5 5 2

screen log
pic obj\m001_000.bmp 1185 110
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1185 110 500
# Four entries a page, each under a rule, measured under Wine.
rows obj\m000_000.bmp 77 74 165 4 100 35
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
# The box that stops the question, whichever answer follows, as on the PC.
button noask obj\i001_000.bmp 498 444 1 5 0

screen gt
# The count before the marker, measured under Wine: a heart of twelve segments, which go dark one a count clockwise
# from the top of its right lobe, the count in navy digits, and at 1 a red 01.
gtcount obj\l005_000.bmp 1147 607 obj\l007_000.bmp 1156 1173 616 obj\l007_001.bmp
gtpieces obj\l006_000.bmp,1175,609 obj\l006_001.bmp,1180,609 obj\l006_002.bmp,1184,620 obj\l006_003.bmp,1186,627 obj\l006_004.bmp,1177,637 obj\l006_005.bmp,1175,641 obj\l006_006.bmp,1165,641 obj\l006_007.bmp,1159,638 obj\l006_008.bmp,1149,627 obj\l006_009.bmp,1149,619 obj\l006_010.bmp,1149,609 obj\l006_011.bmp,1159,609

screen sprites
# The sprite gallery (立ち絵鑑賞), measured under Wine: an explanation page, then the heroine (her story sprite in the
# chosen outfit, pose and face) over a background, sharp or blurred, moving up and down with the pointer, the panels and
# the picker bar, which slide away 3 seconds after they appear or a click changes something unless pinned, and the list
# each thumbnail opens.
spriteexplain obj\d999_000.bmp
spritemusic bgm\bgm10.ogg
spriteresting 3000
spritefigure 640
spritepanel obj\p001_000.bmp 0 26
spritename obj\p001_001.bmp 28 95
spriteradio lock obj\p002_000.bmp 115 206 273
spriteradio music obj\p002_000.bmp 115 206 321
spriteradio blur obj\p002_001.bmp 115 206 370
spritezoom obj\p003_000.bmp 140 180 220 421
spritezoomdigits obj\p003_001.bmp 46 70 85 427
spritehint obj\p004_000.bmp 901 407
spritepin obj\p006_000.bmp 1200 503
spritebar obj\p005_000.bmp 0 537
spritearrows obj\p007_000.bmp 76 255 630 241
spritethumbs 112 594 241
spriteheroines obj\sc\chara_001_002_000_000_000.bmp obj\sc\chara_002_003_000_000_000.bmp obj\sc\chara_000_002_000_000_000.bmp
spritelabel 0 obj\p012_000.bmp 122 689 12
spritelabel 1 obj\p011_000.bmp 373 689 1
spritenumber 1 obj\p010_001.bmp 689 431 447
spritelabel 2 obj\p010_000.bmp 578 689 1
spritenumber 2 obj\p010_001.bmp 689 690 706 722
spritelabel 3 obj\p009_000.bmp 855 689 3
spritelabel 4 obj\p008_000.bmp 1096 689 3
spritehelp lock obj\p001_002.bmp 50 177 3
spritehelp music obj\p001_003.bmp 50 246
spritehelp blur obj\p001_004.bmp 50 295
# The lists, each its own window: backgrounds, faces and outfits as tiles lit red when chosen and blue under the
# pointer; outfits with a box, a number and a name, two rows at a time with a scroll bar, and the chosen one large;
# poses and heroines as figures, the chosen lit red, the one under the pointer blue and the others dimmed.
spritelistclose obj\p015_000.bmp 1150 22
spritelistframe 0 obj\p025_000.bmp 83 15
spritelist 0 4 0 158 118 245 178
spritelisttile 0 thumb_
spritelistlit 0 obj\p027_000.bmp
spritelistlabel 0 obj\p026_000.bmp 0 128 12
spritelistframe 1 obj\p022_000.bmp 83 15
spritelist 1 5 0 176 64 185 211
spritelisttile 1 thumb_
spritelistlit 1 obj\p024_000.bmp
spritelistpicture 1 obj\p023_000.bmp 30 146
spritelistnumber 1 obj\p023_001.bmp 146 88 104
spritelistframe 2 obj\p017_000.bmp 83 15
spritelist 2 4 2 162 62 160 316
spritelisttile 2 thumb_
spritelistlit 2 obj\p021_007.bmp
spritelistpicture 2 obj\p021_000.bmp 0 0
spritelistnumber 2 obj\p021_002.bmp 215 104 116 128
spritelistcheck 2 obj\p021_001.bmp 0 0
spritelistlabel 2 obj\p021_005.bmp 19 249 11 0
spritelistlabel 2 obj\p021_004.bmp 19 249 11 1
spritelistlabel 2 obj\p021_003.bmp 19 249 9 2
spritelistpreview 2 preview_ 810 37
spritelistpreviewlabel 2 obj\p020_002.bmp 924 638 11 0
spritelistpreviewlabel 2 obj\p020_001.bmp 924 638 11 1
spritelistpreviewlabel 2 obj\p020_000.bmp 924 638 9 2
spritelistpreviewnumber 2 obj\p010_001.bmp 603 1054 1073 1092
spritelistscroll 2 obj\p018_000.bmp 114 108 20 518 150
spritelistarrows 2 obj\p019_000.bmp 114 64 86 628 650
spritelistframe 3 obj\p016_000.bmp 83 15
spritelistfigure 3 30 0.8 339 677 1037
spritelisttint 3 106,106,106 199,245,250 166,106,106
spritelistfade 3 obj\p015_001.bmp 105 492
spritelistplace 3 obj\p016_001.bmp 148,609 546,621 902,621
spritelistframe 4 obj\p014_000.bmp 83 15
spritelistfigure 4 30 0.8 339 677 1037
spritelisttint 4 106,106,106 199,245,250 166,106,106
spritelistfade 4 obj\p015_001.bmp 105 492
spritelistplace 4 obj\p015_002.bmp 148,609 546,621 902,621
button back obj\p000_000.bmp 1209 12 1 5 0

screen defaults
# Version info, the PC's message box, as it shows for the copy measured under Wine; which installed parts each bonus
# line needs isn't known.
versionline 【母三人とアナあそび】
versionline ファイルバージョン：1, 0, 0, 0
versionline
versionline ○オフィシャル通販特典
manual manual\index.html
websurvey webアンケート.url
default onahole 1
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

screen defaults
# How the PC sets the message window's text, measured under Wine against the same lines here, with VNDegen's stand-in
# fonts in place of the Windows ones. Each glyph two pixels further on than its advance, lines 27 apart, the first glyph 2
# pixels in and the baseline 1 higher.
textgap 2
textline 27
textshift 2 -1
)";
