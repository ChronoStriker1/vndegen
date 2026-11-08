// Funbag Fantasy 4's menus, a raw string that tables.cpp includes and menuTable() hands out.
const char kFunbag4[] = R"(
screen title
askerlit
pic obj\c000_000.bmp 0 0
pic obj\c000_001.bmp 0 0
pic obj\d000_000.bmp 435 0
pic obj\d001_000.bmp 0 0
pic obj\c000_002.bmp 0 660
pic obj\d004_000.bmp 568 687
button start obj\d003_000.bmp 58 355 6 5 0 help obj\d006_000.bmp 58 311 1 1 0 0
button load obj\d003_000.bmp 220 355 6 5 1 help obj\d006_001.bmp 220 311 1 1 0 0
button quickload obj\d003_000.bmp 382 355 6 5 2 help obj\d006_002.bmp 382 311 1 1 0 0
button cg obj\d003_000.bmp 58 443 6 5 3 help obj\d006_003.bmp 58 399 1 1 0 0
button scene obj\d003_000.bmp 220 443 6 5 4 help obj\d006_004.bmp 220 399 1 1 0 0
button music obj\d003_000.bmp 382 443 6 5 5 help obj\d006_005.bmp 382 399 1 1 0 0
button config obj\d003_001.bmp 173 536 3 5 0 help obj\d006_006.bmp 173 492 1 1 0 0
button quit obj\d003_001.bmp 301 536 3 5 2 help obj\d006_008.bmp 301 492 1 1 0 0

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
button nav_save obj\e003_000.bmp 1109 59 9 5 0 help obj\e004_000.bmp 973 15 1 1 0 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1 help obj\e004_001.bmp 977 73 1 1 0 0
button nav_cg obj\e003_000.bmp 1117 175 9 5 2 help obj\e004_002.bmp 981 131 1 1 0 0
button nav_scene obj\e003_000.bmp 1121 233 9 5 3 help obj\e004_003.bmp 985 189 1 1 0 0
button nav_music obj\e003_000.bmp 1125 291 9 5 4 help obj\e004_004.bmp 989 247 1 1 0 0
button nav_config obj\e003_000.bmp 1129 349 9 5 5 help obj\e004_005.bmp 993 305 1 1 0 0
button nav_title obj\e003_000.bmp 1133 407 9 5 6 help obj\e004_006.bmp 997 363 1 1 0 0
button nav_quit obj\e003_000.bmp 1137 465 9 5 7 help obj\e004_007.bmp 1001 421 1 1 0 0
button back obj\e003_000.bmp 1145 581 9 5 8 help obj\e004_008.bmp 1009 537 1 1 0 0

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
button nav_save obj\e003_000.bmp 1109 59 9 5 0 help obj\e004_000.bmp 973 15 1 1 0 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1 help obj\e004_001.bmp 977 73 1 1 0 0
button nav_cg obj\e003_000.bmp 1117 175 9 5 2 help obj\e004_002.bmp 981 131 1 1 0 0
button nav_scene obj\e003_000.bmp 1121 233 9 5 3 help obj\e004_003.bmp 985 189 1 1 0 0
button nav_music obj\e003_000.bmp 1125 291 9 5 4 help obj\e004_004.bmp 989 247 1 1 0 0
button nav_config obj\e003_000.bmp 1129 349 9 5 5 help obj\e004_005.bmp 993 305 1 1 0 0
button nav_title obj\e003_000.bmp 1133 407 9 5 6 help obj\e004_006.bmp 997 363 1 1 0 0
button nav_quit obj\e003_000.bmp 1137 465 9 5 7 help obj\e004_007.bmp 1001 421 1 1 0 0
button back obj\e003_000.bmp 1145 581 9 5 8 help obj\e004_008.bmp 1009 537 1 1 0 0

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
button nav_save obj\e003_000.bmp 1109 59 9 5 0 help obj\e004_000.bmp 973 15 1 1 0 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1 help obj\e004_001.bmp 977 73 1 1 0 0
button nav_cg obj\e003_000.bmp 1117 175 9 5 2 help obj\e004_002.bmp 981 131 1 1 0 0
button nav_scene obj\e003_000.bmp 1121 233 9 5 3 help obj\e004_003.bmp 985 189 1 1 0 0
button nav_music obj\e003_000.bmp 1125 291 9 5 4 help obj\e004_004.bmp 989 247 1 1 0 0
button nav_config obj\e003_000.bmp 1129 349 9 5 5 help obj\e004_005.bmp 993 305 1 1 0 0
button nav_title obj\e003_000.bmp 1133 407 9 5 6 help obj\e004_006.bmp 997 363 1 1 0 0
button nav_quit obj\e003_000.bmp 1137 465 9 5 7 help obj\e004_007.bmp 1001 421 1 1 0 0
button back obj\e003_000.bmp 1145 581 9 5 8 help obj\e004_008.bmp 1009 537 1 1 0 0
# The CG viewer's panel, measured under Wine.
viewer obj\j000_000.bmp 373 489
viewerhelp obj\j006_001.bmp 870 681 12
viewerbutton bgm obj\j006_000.bmp 434 640
viewerbgm 0 2 0 0
viewerslider obj\j005_000.bmp 416 610 11
viewernumber volume obj\g005_000.bmp 481 580 16 3
viewerbutton zoomout obj\j004_000.bmp 561 634 3 0 help 8
viewerbutton zoomreset obj\j004_000.bmp 604 634 3 1 help 9
viewerbutton zoomin obj\j004_000.bmp 647 634 3 2 help 10
viewernumber zoom obj\j003_000.bmp 600 597 15 1
viewernumber zoomfrac obj\j003_001.bmp 627 600 13 2
viewerbutton replay obj\j002_000.bmp 713 640 help 1
viewerbutton prevcg obj\j002_001.bmp 793 640 6 0 help 3
viewerbutton prevvar obj\j002_001.bmp 848 640 6 1 help 4
viewerbutton nextvar obj\j002_001.bmp 1023 640 6 2 help 5
viewerbutton nextcg obj\j002_001.bmp 1078 640 6 3 help 6
viewerbutton auto obj\j002_001.bmp 1138 640 6 4 help 7
viewerbutton return obj\j002_001.bmp 1199 640 6 5 help 2
viewerbutton hide obj\j001_000.bmp 1230 577 help 0
viewernumber index obj\j003_000.bmp 921 619 15 2
viewernumber total obj\j003_001.bmp 976 641 13 2
viewerauto 8000 0

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
button nav_save obj\e003_000.bmp 1109 59 9 5 0 help obj\e004_000.bmp 973 15 1 1 0 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1 help obj\e004_001.bmp 977 73 1 1 0 0
button nav_cg obj\e003_000.bmp 1117 175 9 5 2 help obj\e004_002.bmp 981 131 1 1 0 0
button nav_scene obj\e003_000.bmp 1121 233 9 5 3 help obj\e004_003.bmp 985 189 1 1 0 0
button nav_music obj\e003_000.bmp 1125 291 9 5 4 help obj\e004_004.bmp 989 247 1 1 0 0
button nav_config obj\e003_000.bmp 1129 349 9 5 5 help obj\e004_005.bmp 993 305 1 1 0 0
button nav_title obj\e003_000.bmp 1133 407 9 5 6 help obj\e004_006.bmp 997 363 1 1 0 0
button nav_quit obj\e003_000.bmp 1137 465 9 5 7 help obj\e004_007.bmp 1001 421 1 1 0 0
button back obj\e003_000.bmp 1145 581 9 5 8 help obj\e004_008.bmp 1009 537 1 1 0 0

screen music
pic obj\g000_000.bmp 0 0
pic obj\g001_000.bmp 0 0
pic obj\e002_000.bmp 1072 0
pic obj\e005_000.bmp 582 46
pic obj\g004_000.bmp 356 54
slider music obj\g004_001.bmp 383 60
number music obj\g005_000.bmp 490 31
button music_prev obj\g003_000.bmp 651 26 7 5 0 help obj\g006_000.bmp 651 86 1 1 0 0
button music_play obj\g003_000.bmp 711 26 7 5 1 help obj\g006_001.bmp 711 86 1 1 0 0
button music_pause obj\g003_000.bmp 771 26 7 5 2
button music_stop obj\g003_000.bmp 831 26 7 5 3
button music_next obj\g003_000.bmp 891 26 7 5 4 help obj\g006_004.bmp 891 86 1 1 0 0
button music_repeat obj\g003_000.bmp 951 26 7 5 5 help obj\g006_005.bmp 915 86 1 1 0 0
button music_shuffle obj\g003_000.bmp 1011 26 7 5 6 help obj\g006_006.bmp 722 86 1 1 0 0
hover 356 54 568 92 obj\g006_007.bmp 384 96
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
button nav_save obj\e003_000.bmp 1109 59 9 5 0 help obj\e004_000.bmp 973 15 1 1 0 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1 help obj\e004_001.bmp 977 73 1 1 0 0
button nav_cg obj\e003_000.bmp 1117 175 9 5 2 help obj\e004_002.bmp 981 131 1 1 0 0
button nav_scene obj\e003_000.bmp 1121 233 9 5 3 help obj\e004_003.bmp 985 189 1 1 0 0
button nav_music obj\e003_000.bmp 1125 291 9 5 4 help obj\e004_004.bmp 989 247 1 1 0 0
button nav_config obj\e003_000.bmp 1129 349 9 5 5 help obj\e004_005.bmp 993 305 1 1 0 0
button nav_title obj\e003_000.bmp 1133 407 9 5 6 help obj\e004_006.bmp 997 363 1 1 0 0
button nav_quit obj\e003_000.bmp 1137 465 9 5 7 help obj\e004_007.bmp 1001 421 1 1 0 0
button back obj\e003_000.bmp 1145 581 9 5 8 help obj\e004_008.bmp 1009 537 1 1 0 0

screen config
pic obj\h000_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\e002_000.bmp 1072 0
pic obj\h005_000.bmp 12 151
button tab obj\h003_000.bmp 331 34 3 5 0 help obj\h003_001.bmp 331 108 1 1 0 0
button tab2 obj\h003_000.bmp 451 34 3 5 1 help obj\h003_002.bmp 451 108 1 1 0 0
button tab3 obj\h003_000.bmp 571 34 3 5 2 help obj\h003_003.bmp 571 108 1 1 0 0
button cfg_defaults obj\h002_000.bmp 714 34 3 5 0 help obj\h002_001.bmp 714 108 1 1 0 0
button cfg_undo obj\h002_000.bmp 834 34 3 5 1 help obj\h002_002.bmp 834 108 1 1 0 0
button cfg_apply obj\h002_000.bmp 954 34 3 5 2 help obj\h002_003.bmp 728 108 1 1 0 0
button nav_save obj\e003_000.bmp 1109 59 9 5 0 help obj\e004_000.bmp 973 15 1 1 0 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1 help obj\e004_001.bmp 977 73 1 1 0 0
button nav_cg obj\e003_000.bmp 1117 175 9 5 2 help obj\e004_002.bmp 981 131 1 1 0 0
button nav_scene obj\e003_000.bmp 1121 233 9 5 3 help obj\e004_003.bmp 985 189 1 1 0 0
button nav_music obj\e003_000.bmp 1125 291 9 5 4 help obj\e004_004.bmp 989 247 1 1 0 0
button nav_config obj\e003_000.bmp 1129 349 9 5 5 help obj\e004_005.bmp 993 305 1 1 0 0
button nav_title obj\e003_000.bmp 1133 407 9 5 6 help obj\e004_006.bmp 997 363 1 1 0 0
button nav_quit obj\e003_000.bmp 1137 465 9 5 7 help obj\e004_007.bmp 1001 421 1 1 0 0
button back obj\e003_000.bmp 1145 581 9 5 8 help obj\e004_008.bmp 1009 537 1 1 0 0
radio screen 0 obj\h006_000.bmp 141 157 2 5 0
radio screen 1 obj\h006_000.bmp 263 157 2 5 1
pic obj\h007_000.bmp 141 199 6 5 0 4
pic obj\h007_000.bmp 263 199 6 5 1 4
pic obj\h007_000.bmp 385 199 6 5 2 4
pic obj\h007_000.bmp 141 231 6 5 3 4
pic obj\h007_000.bmp 263 231 6 5 4 4
pic obj\h007_000.bmp 385 231 6 5 5 4
radio effect 1 obj\h008_000.bmp 19 295 2 5 0
radio effect 0 obj\h008_000.bmp 100 295 2 5 1
radio wait 1 obj\h008_000.bmp 195 295 2 5 0
radio wait 0 obj\h008_000.bmp 276 295 2 5 1
radio waitskip 1 obj\h008_000.bmp 371 295 2 5 0
radio waitskip 0 obj\h008_000.bmp 452 295 2 5 1
radio camera 1 obj\h008_000.bmp 584 443 2 5 0
radio camera 0 obj\h008_000.bmp 665 443 2 5 1
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
check tutorial obj\h013_000.bmp 572 351 8 5 6
pic obj\h013_000.bmp 572 379 8 5 7 4
hover 15 154 529 189 obj\h004_000.bmp 12 108
hover 555 154 1067 409 obj\h004_006.bmp 550 81
hover 15 197 529 263 obj\h004_001.bmp 12 150
hover 15 271 178 327 obj\h004_002.bmp 12 224
hover 192 271 354 327 obj\h004_003.bmp 188 224
hover 368 271 529 327 obj\h004_004.bmp 364 224
hover 15 335 529 670 obj\h004_005.bmp 12 288
hover 555 418 774 474 obj\h004_007.bmp 550 371

screen config2
pic obj\h000_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\e002_000.bmp 1072 0
pic obj\h015_000.bmp 12 151
button tab obj\h003_000.bmp 331 34 3 5 0 help obj\h003_001.bmp 331 108 1 1 0 0
button tab2 obj\h003_000.bmp 451 34 3 5 1 help obj\h003_002.bmp 451 108 1 1 0 0
button tab3 obj\h003_000.bmp 571 34 3 5 2 help obj\h003_003.bmp 571 108 1 1 0 0
button cfg_defaults obj\h002_000.bmp 714 34 3 5 0 help obj\h002_001.bmp 714 108 1 1 0 0
button cfg_undo obj\h002_000.bmp 834 34 3 5 1 help obj\h002_002.bmp 834 108 1 1 0 0
button cfg_apply obj\h002_000.bmp 954 34 3 5 2 help obj\h002_003.bmp 728 108 1 1 0 0
button nav_save obj\e003_000.bmp 1109 59 9 5 0 help obj\e004_000.bmp 973 15 1 1 0 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1 help obj\e004_001.bmp 977 73 1 1 0 0
button nav_cg obj\e003_000.bmp 1117 175 9 5 2 help obj\e004_002.bmp 981 131 1 1 0 0
button nav_scene obj\e003_000.bmp 1121 233 9 5 3 help obj\e004_003.bmp 985 189 1 1 0 0
button nav_music obj\e003_000.bmp 1125 291 9 5 4 help obj\e004_004.bmp 989 247 1 1 0 0
button nav_config obj\e003_000.bmp 1129 349 9 5 5 help obj\e004_005.bmp 993 305 1 1 0 0
button nav_title obj\e003_000.bmp 1133 407 9 5 6 help obj\e004_006.bmp 997 363 1 1 0 0
button nav_quit obj\e003_000.bmp 1137 465 9 5 7 help obj\e004_007.bmp 1001 421 1 1 0 0
button back obj\e003_000.bmp 1145 581 9 5 8 help obj\e004_008.bmp 1009 537 1 1 0 0
radio text 0 obj\h016_000.bmp 34 189 4 5 0
radio auto 0 obj\h016_000.bmp 34 267 4 5 0
radio text 1 obj\h016_000.bmp 156 189 4 5 1
radio auto 1 obj\h016_000.bmp 156 267 4 5 1
radio text 2 obj\h016_000.bmp 278 189 4 5 2
radio auto 2 obj\h016_000.bmp 278 267 4 5 2
radio text 3 obj\h016_000.bmp 400 189 4 5 3
radio auto 3 obj\h016_000.bmp 400 267 4 5 3
radio skipall 0 obj\h018_000.bmp 587 179 2 5 0
radio skipall 1 obj\h018_000.bmp 668 179 2 5 1
radio deckey 0 obj\h019_000.bmp 815 179 2 5 0
radio deckey 1 obj\h019_000.bmp 896 179 2 5 1
radio rightclick 0 obj\h020_000.bmp 680 222 5 5 0
radio rightclick 1 obj\h020_000.bmp 802 222 5 5 1
radio rightclick 2 obj\h020_000.bmp 924 222 5 5 2
radio rightclick 3 obj\h020_000.bmp 680 254 5 5 3
radio rightclick 4 obj\h020_000.bmp 802 254 5 5 4
radio pause_inactive 1 obj\h008_000.bmp 587 317 2 5 0
radio pause_inactive 0 obj\h008_000.bmp 668 317 2 5 1
pic obj\h008_000.bmp 815 317 2 5 0 4
pic obj\h008_000.bmp 896 317 2 5 1 4
# Wait time, ×0.70 to ×1.30 in steps of 0.01, stretches waits and fades; the bar is full at ×2.00.
range waittime 70 130
slider waittime obj\h009_000.bmp 124 635 200
number waittime obj\h011_000.bmp 378 607 20
step waittime -1 obj\h010_000.bmp 89 637 2 5 0
step waittime 1 obj\h010_000.bmp 439 637 2 5 1
hover 15 156 530 226 obj\h004_008.bmp 12 108
hover 15 234 530 307 obj\h004_009.bmp 12 192
hover 553 154 774 210 obj\h004_011.bmp 550 81
hover 781 154 1001 210 obj\h004_012.bmp 677 81
hover 553 219 1067 285 obj\h004_013.bmp 550 171
hover 553 292 774 348 obj\h004_014.bmp 550 219
hover 781 292 1001 348 obj\h004_015.bmp 779 219
hover 15 600 530 667 obj\h004_010.bmp 12 526

screen config3
pic obj\h000_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\e002_000.bmp 1072 0
pic obj\h021_000.bmp 12 151
button tab obj\h003_000.bmp 331 34 3 5 0 help obj\h003_001.bmp 331 108 1 1 0 0
button tab2 obj\h003_000.bmp 451 34 3 5 1 help obj\h003_002.bmp 451 108 1 1 0 0
button tab3 obj\h003_000.bmp 571 34 3 5 2 help obj\h003_003.bmp 571 108 1 1 0 0
button cfg_defaults obj\h002_000.bmp 714 34 3 5 0 help obj\h002_001.bmp 714 108 1 1 0 0
button cfg_undo obj\h002_000.bmp 834 34 3 5 1 help obj\h002_002.bmp 834 108 1 1 0 0
button cfg_apply obj\h002_000.bmp 954 34 3 5 2 help obj\h002_003.bmp 728 108 1 1 0 0
button nav_save obj\e003_000.bmp 1109 59 9 5 0 help obj\e004_000.bmp 973 15 1 1 0 0
button nav_load obj\e003_000.bmp 1113 117 9 5 1 help obj\e004_001.bmp 977 73 1 1 0 0
button nav_cg obj\e003_000.bmp 1117 175 9 5 2 help obj\e004_002.bmp 981 131 1 1 0 0
button nav_scene obj\e003_000.bmp 1121 233 9 5 3 help obj\e004_003.bmp 985 189 1 1 0 0
button nav_music obj\e003_000.bmp 1125 291 9 5 4 help obj\e004_004.bmp 989 247 1 1 0 0
button nav_config obj\e003_000.bmp 1129 349 9 5 5 help obj\e004_005.bmp 993 305 1 1 0 0
button nav_title obj\e003_000.bmp 1133 407 9 5 6 help obj\e004_006.bmp 997 363 1 1 0 0
button nav_quit obj\e003_000.bmp 1137 465 9 5 7 help obj\e004_007.bmp 1001 421 1 1 0 0
button back obj\e003_000.bmp 1145 581 9 5 8 help obj\e004_008.bmp 1009 537 1 1 0 0
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
hover 16 154 530 224 obj\h004_016.bmp 12 108
hover 553 154 773 211 obj\h004_022.bmp 551 108
hover 553 219 1067 690 obj\h004_023.bmp 551 172
hover 16 234 530 303 obj\h004_017.bmp 12 188
hover 16 311 530 380 obj\h004_018.bmp 12 265
hover 16 388 530 458 obj\h004_019.bmp 12 342
hover 16 467 530 536 obj\h004_020.bmp 12 421
hover 16 546 530 614 obj\h004_021.bmp 12 498
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
# The box that stops the question, whichever answer follows, as on the PC.
button noask obj\i001_000.bmp 477 452 1 5 0

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

screen defaults
# The official English release sets its text in Lucida Sans Unicode (Wine's font log), here Open Sans, sized and spaced
# by rendering a line of the publisher's Windows screenshots here and laying it over theirs: 16 with a gap of 2.25
# matches its x-height and both its ends within 2 pixels. Lines as far apart as there, the first glyph one gap in.
textfont 7
textsize 16
textgap 2.25
textline 25
textshift 3 0
)";
