// Horny Elves and a Moral Orc's menus, a raw string that tables.cpp includes and menuTable() hands out.
// Horny Elves and a Moral Orc (Test Game 89), 1280x720, measured under Wine.
const char kMoralOrc[] = R"(
screen title
pic obj\c000_000.bmp 0 0
pic obj\d000_000.bmp 384 0
pic obj\d000_002.bmp 197 0
pic obj\d000_001.bmp 542 0
pic obj\c000_001.bmp 0 0
pic obj\d002_000.bmp 335 451
pic obj\d003_000.bmp 300 700
button start obj\d004_000.bmp 19 12 10 5 0
button load obj\d004_000.bmp 59 153 10 5 1
button quickload obj\d004_000.bmp 19 294 10 5 2
button cg obj\d004_000.bmp 1053 12 10 5 3
button scene obj\d004_000.bmp 1103 153 10 5 4
button music obj\d004_000.bmp 1053 294 10 5 5
button config obj\d004_000.bmp 1103 435 10 5 6
button quit obj\d004_000.bmp 1053 576 10 5 8

screen load
pic obj\f000_000.bmp 0 0
pic obj\f001_000.bmp 0 0
pic obj\f001_001.bmp 12 15 1 2 0 1
slots obj\f010_000.bmp 34 158 555 182 3 2
slotlabel obj\f011_000.bmp 20 24
slotdigits obj\f011_001.bmp 84 18
slotthumb 14 51
datedigits obj\f012_000.bmp 191 91
dateslash obj\f012_002.bmp
datecolon obj\f012_003.bmp
datedash obj\f012_001.bmp
dategap 16
slotlock obj\f014_000.bmp 489 85
slotdelete obj\f014_001.bmp 473 15
slotchapter obj\f013_000.bmp 186 16 20 flow01 flow02 flow03 flow04 flow05 flow06 flow07 flow08 flow09 flow10 flow11 flow12 flow13 flow14 flow15 flow16 flow18 flow19 flow20 flow21
button page0 obj\f009_000.bmp 1151 161 10 5 0
button page1 obj\f009_000.bmp 1213 195 10 5 1
button page2 obj\f009_000.bmp 1151 245 10 5 2
button page3 obj\f009_000.bmp 1213 279 10 5 3
button page4 obj\f009_000.bmp 1151 329 10 5 4
button page5 obj\f009_000.bmp 1213 363 10 5 5
button page6 obj\f009_000.bmp 1151 413 10 5 6
button page7 obj\f009_000.bmp 1213 447 10 5 7
button page8 obj\f009_000.bmp 1151 497 10 5 8
button page9 obj\f009_000.bmp 1213 531 10 5 9
button page10 obj\f009_001.bmp 1155 605 1 5 0
button nav_save obj\f002_000.bmp 666 6 8 5 0
button nav_load obj\f002_000.bmp 791 6 8 5 1
button nav_cg obj\f002_000.bmp 916 6 8 5 2
button nav_scene obj\f002_000.bmp 1041 6 8 5 3
button nav_music obj\f002_000.bmp 704 51 8 5 4
button nav_config obj\f002_000.bmp 829 51 8 5 5
button nav_title obj\f002_000.bmp 954 51 8 5 6
button nav_quit obj\f002_000.bmp 1079 51 8 5 7
button back obj\f002_001.bmp 1195 11 1 5 0

screen save
pic obj\f000_000.bmp 0 0
pic obj\f001_000.bmp 0 0
pic obj\f001_001.bmp 12 15 1 2 0 0
slots obj\f010_000.bmp 34 158 555 182 3 2
slotlabel obj\f011_000.bmp 20 24
slotdigits obj\f011_001.bmp 84 18
slotthumb 14 51
datedigits obj\f012_000.bmp 191 91
dateslash obj\f012_002.bmp
datecolon obj\f012_003.bmp
datedash obj\f012_001.bmp
dategap 16
slotlock obj\f014_000.bmp 489 85
slotdelete obj\f014_001.bmp 473 15
slotchapter obj\f013_000.bmp 186 16 20 flow01 flow02 flow03 flow04 flow05 flow06 flow07 flow08 flow09 flow10 flow11 flow12 flow13 flow14 flow15 flow16 flow18 flow19 flow20 flow21
button page0 obj\f009_000.bmp 1151 161 10 5 0
button page1 obj\f009_000.bmp 1213 195 10 5 1
button page2 obj\f009_000.bmp 1151 245 10 5 2
button page3 obj\f009_000.bmp 1213 279 10 5 3
button page4 obj\f009_000.bmp 1151 329 10 5 4
button page5 obj\f009_000.bmp 1213 363 10 5 5
button page6 obj\f009_000.bmp 1151 413 10 5 6
button page7 obj\f009_000.bmp 1213 447 10 5 7
button page8 obj\f009_000.bmp 1151 497 10 5 8
button page9 obj\f009_000.bmp 1213 531 10 5 9
pageoff 10
button page10 obj\f009_001.bmp 1155 605 1 5 0
button nav_save obj\f002_000.bmp 666 6 8 5 0
button nav_load obj\f002_000.bmp 791 6 8 5 1
button nav_cg obj\f002_000.bmp 916 6 8 5 2
button nav_scene obj\f002_000.bmp 1041 6 8 5 3
button nav_music obj\f002_000.bmp 704 51 8 5 4
button nav_config obj\f002_000.bmp 829 51 8 5 5
button nav_title obj\f002_000.bmp 954 51 8 5 6
button nav_quit obj\f002_000.bmp 1079 51 8 5 7
button back obj\f002_001.bmp 1195 11 1 5 0


screen cg
# The CG viewer's panel, measured under Wine. A hovered button's help is a speech bubble.
viewer obj\k000_000.bmp 0 534
viewerhelp obj\k004_001.bmp 257 351 12
viewerbutton bgm obj\k006_000.bmp 378 685 2 0
viewerbgm 0 0 1 2
viewerpic obj\h004_000.bmp 277 654
viewerslider obj\h004_001.bmp 302 657 11
viewernumber volume obj\f012_000.bmp 391 628 16 3
viewerbutton zoomout obj\k005_000.bmp 487 668 3 0 help 8
viewerbutton zoomreset obj\k005_000.bmp 536 668 3 1 help 9
viewerbutton zoomin obj\k005_000.bmp 585 668 3 2 help 10
viewernumber zoom obj\g007_000.bmp 531 626 20 1
viewernumber zoomfrac obj\g008_000.bmp 565 632 18 2
viewerbutton replay obj\k002_000.bmp 651 668 2 0 help 1
viewerbutton prevcg obj\k003_000.bmp 755 668 5 0 help 3
viewerbutton prevvar obj\k003_000.bmp 810 668 5 1 help 4
viewerbutton nextvar obj\k003_000.bmp 1008 668 5 2 help 5
viewerbutton nextcg obj\k003_000.bmp 1063 668 5 3 help 6
viewerbutton auto obj\k003_000.bmp 1123 668 5 4 help 7
viewerbutton return obj\k002_000.bmp 1182 668 2 1 help 2
viewerbutton hide obj\k001_000.bmp 1232 593 help 0
viewernumber index obj\g007_000.bmp 880 651 22 2
viewernumber total obj\g008_000.bmp 945 676 18 2
viewerauto 8000 0
pic obj\g000_000.bmp 0 0 tab 0
pic obj\g000_000.bmp 0 0 tab 1
pic obj\g000_001.bmp 0 0 tab 2
pic obj\g000_002.bmp 0 0 tab 3
pic obj\g000_003.bmp 0 0 tab 4
# Each tab's character, the sprite the PC reads for it, comes up from below when the tab opens and settles with a
# little overshoot, from a 30 fps recording under Wine (the first two points are below the screen, so not seen).
pic sc\sc_000_000_000_001_000.bmp 340 -40 tab 0 rise 0:720,33:620,67:366,100:117,133:-21,167:-21,200:0
pic sc\sc_001_000_000_001_000.bmp 340 -40 tab 1 rise 0:720,33:620,67:366,100:117,133:-21,167:-21,200:0
pic sc\sc_002_000_000_001_000.bmp 340 -40 tab 2 rise 0:720,33:620,67:366,100:117,133:-21,167:-21,200:0
pic sc\sc_003_000_000_001_000.bmp 340 -40 tab 3 rise 0:720,33:620,67:366,100:117,133:-21,167:-21,200:0
pic obj\g002_000.bmp 200 -40 tab 4 rise 0:720,33:620,67:366,100:117,133:-21,167:-21,200:0
pic obj\g001_000.bmp 0 0 tab 0
pic obj\g001_001.bmp 0 628 tab 0
pic obj\g001_000.bmp 0 0 tab 1
pic obj\g001_001.bmp 0 628 tab 1
pic obj\g001_002.bmp 0 0 tab 2
pic obj\g001_003.bmp 0 628 tab 2
pic obj\g001_004.bmp 0 0 tab 3
pic obj\g001_005.bmp 0 628 tab 3
pic obj\g001_006.bmp 0 0 tab 4
pic obj\g001_007.bmp 0 628 tab 4
pic obj\g003_000.bmp 12 15 1 2 0 0
cellsheet obj\g006_000.bmp
cellsize 110 76
cellthumb 29 33
cell 154 178
cell 154 360
cell 404 178
cell 404 360
cell 654 178
cell 654 360
cell 904 178
cell 904 360
celllabel obj\f011_000.bmp 13 18
celldigits obj\f011_001.bmp 55 12 2 22
cellcount obj\g007_000.bmp obj\g009_000.bmp 119 113 159 172 0
celltotal obj\g008_000.bmp 4
gallerytabs
cellhide
button pageprev obj\g005_000.bmp 20 264 2 5 0
button pagenext obj\g005_000.bmp 1186 264 2 5 1
button page0 obj\f009_000.bmp 910 622 10 5 0
button page1 obj\f009_000.bmp 984 622 10 5 1
button page2 obj\f009_000.bmp 1058 622 10 5 2
button gtab0 obj\g004_000.bmp 85 570 5 5 0
button gtab1 obj\g004_000.bmp 237 570 5 5 1
button gtab2 obj\g004_000.bmp 389 570 5 5 2
# Fiona's tab shows once the script that names her, ep_011_001_000, has been read to its end, as on the PC.
button gtab3 obj\g004_000.bmp 541 570 5 5 3 ifread scr\ep_011_001_000.txt
button gtab4 obj\g004_000.bmp 693 570 5 5 4
button nav_save obj\f002_000.bmp 666 6 8 5 0
button nav_load obj\f002_000.bmp 791 6 8 5 1
button nav_cg obj\f002_000.bmp 916 6 8 5 2
button nav_scene obj\f002_000.bmp 1041 6 8 5 3
button nav_music obj\f002_000.bmp 704 51 8 5 4
button nav_config obj\f002_000.bmp 829 51 8 5 5
button nav_title obj\f002_000.bmp 954 51 8 5 6
button nav_quit obj\f002_000.bmp 1079 51 8 5 7
button back obj\f002_001.bmp 1195 11 1 5 0

screen scene
# The PC draws a character beside the cards, from sprite parts the runtime doesn't build yet.
pic obj\g000_000.bmp 0 0 tab 0
pic obj\g000_000.bmp 0 0 tab 1
pic obj\g000_001.bmp 0 0 tab 2
pic obj\g000_002.bmp 0 0 tab 3
pic obj\g000_003.bmp 0 0 tab 4
pic obj\g001_000.bmp 0 0 tab 0
pic obj\g001_001.bmp 0 628 tab 0
pic obj\g001_000.bmp 0 0 tab 1
pic obj\g001_001.bmp 0 628 tab 1
pic obj\g001_002.bmp 0 0 tab 2
pic obj\g001_003.bmp 0 628 tab 2
pic obj\g001_004.bmp 0 0 tab 3
pic obj\g001_005.bmp 0 628 tab 3
pic obj\g001_006.bmp 0 0 tab 4
pic obj\g001_007.bmp 0 628 tab 4
pic obj\g003_000.bmp 12 15 1 2 0 1
cellsheet obj\g006_000.bmp
cellsize 110 76
cellthumb 29 33
cell 154 178
cell 154 360
cell 404 178
cell 404 360
cell 654 178
cell 654 360
cell 904 178
cell 904 360
celllabel obj\f011_000.bmp 13 18
celldigits obj\f011_001.bmp 55 12 2 22
gallerytabs
cellhide
button pageprev obj\g005_000.bmp 20 264 2 5 0
button pagenext obj\g005_000.bmp 1186 264 2 5 1
button page0 obj\f009_000.bmp 910 622 10 5 0
button page1 obj\f009_000.bmp 984 622 10 5 1
button page2 obj\f009_000.bmp 1058 622 10 5 2
button gtab0 obj\g004_000.bmp 85 570 5 5 0
button gtab1 obj\g004_000.bmp 237 570 5 5 1
button gtab2 obj\g004_000.bmp 389 570 5 5 2
# Fiona's tab shows once the script that names her, ep_011_001_000, has been read to its end, as on the PC.
button gtab3 obj\g004_000.bmp 541 570 5 5 3 ifread scr\ep_011_001_000.txt
button gtab4 obj\g004_000.bmp 693 570 5 5 4
button nav_save obj\f002_000.bmp 666 6 8 5 0
button nav_load obj\f002_000.bmp 791 6 8 5 1
button nav_cg obj\f002_000.bmp 916 6 8 5 2
button nav_scene obj\f002_000.bmp 1041 6 8 5 3
button nav_music obj\f002_000.bmp 704 51 8 5 4
button nav_config obj\f002_000.bmp 829 51 8 5 5
button nav_title obj\f002_000.bmp 954 51 8 5 6
button nav_quit obj\f002_000.bmp 1079 51 8 5 7
button back obj\f002_001.bmp 1195 11 1 5 0

screen music
nolithelp
pic obj\h000_000.bmp 0 0
pic obj\f001_000.bmp 0 0
# The top band is the galleries' leafy one, over the menus' own.
pic obj\g001_000.bmp 0 0
pic obj\h001_000.bmp 12 15
# The bottom band, then the cast along it over their outline, Fiona as a silhouette until the script that names her has
# been read, placed and layered against the PC's frame under Wine, which they match to the pixel.
pic obj\g001_001.bmp 0 628
pic obj\h005_000.bmp 0 390
# Each control's help is a speech bubble from one of the cast, whose portrait darkens while it shows.
pic obj\h005_001.bmp 944 392 unlesshover music_prev,music_stop
pic obj\h005_002.bmp 944 392 ifhover music_prev,music_stop
pic obj\h105_009.bmp 50 453 unlessread scr\ep_011_001_000.txt
pic obj\h005_009.bmp 50 453 ifread scr\ep_011_001_000.txt unlesshover volume
pic obj\h005_010.bmp 50 453 ifread scr\ep_011_001_000.txt ifhover volume
pic obj\h005_003.bmp 895 501 unlesshover music_play,music_next
pic obj\h005_004.bmp 895 501 ifhover music_play,music_next
pic obj\h005_005.bmp 1116 510 unlesshover music_pause,music_repeat
pic obj\h005_006.bmp 1116 510 ifhover music_pause,music_repeat
pic obj\h005_007.bmp -53 478 unlesshover music_shuffle
pic obj\h005_008.bmp -53 478 ifhover music_shuffle
pic obj\h004_000.bmp 290 683
slider music obj\h004_001.bmp 315 686
hover 290 683 477 711 obj\h006_000.bmp 237 388 8 7 volume
number music obj\f012_000.bmp 403 656
# Tracks 1 to 6 run down the left and 7 to 16 down the right.
button track0 obj\h002_000.bmp 21 120 16 5 0
button track1 obj\h002_000.bmp 21 177 16 5 1
button track2 obj\h002_000.bmp 21 234 16 5 2
button track3 obj\h002_000.bmp 21 291 16 5 3
button track4 obj\h002_000.bmp 21 348 16 5 4
button track5 obj\h002_000.bmp 21 405 16 5 5
button track6 obj\h002_000.bmp 279 120 16 5 6
button track7 obj\h002_000.bmp 279 177 16 5 7
button track8 obj\h002_000.bmp 279 234 16 5 8
button track9 obj\h002_000.bmp 279 291 16 5 9
button track10 obj\h002_000.bmp 279 348 16 5 10
button track11 obj\h002_000.bmp 279 405 16 5 11
button track12 obj\h002_000.bmp 279 462 16 5 12
button track13 obj\h002_000.bmp 279 519 16 5 13
button track14 obj\h002_000.bmp 279 576 16 5 14
button track15 obj\h002_000.bmp 279 633 16 5 15
button music_prev obj\h003_000.bmp 505 659 7 5 0 help obj\h006_000.bmp 522 268 1 8 0 0
button music_play obj\h003_000.bmp 565 659 7 5 1 help obj\h006_000.bmp 387 388 1 8 0 1
button music_pause obj\h003_000.bmp 625 659 7 5 2 help obj\h006_000.bmp 606 388 1 8 0 2
button music_stop obj\h003_000.bmp 685 659 7 5 3 help obj\h006_000.bmp 522 268 1 8 0 3
button music_next obj\h003_000.bmp 745 659 7 5 4 help obj\h006_000.bmp 387 388 1 8 0 4
button music_repeat obj\h003_000.bmp 805 659 7 5 5 help obj\h006_000.bmp 606 388 1 8 0 5
button music_shuffle obj\h003_000.bmp 865 659 7 5 6 help obj\h006_000.bmp 123 388 1 8 0 6
button nav_save obj\f002_000.bmp 666 6 8 5 0
button nav_load obj\f002_000.bmp 791 6 8 5 1
button nav_cg obj\f002_000.bmp 916 6 8 5 2
button nav_scene obj\f002_000.bmp 1041 6 8 5 3
button nav_music obj\f002_000.bmp 704 51 8 5 4
button nav_config obj\f002_000.bmp 829 51 8 5 5
button nav_title obj\f002_000.bmp 954 51 8 5 6
button nav_quit obj\f002_000.bmp 1079 51 8 5 7
button back obj\f002_001.bmp 1195 11 1 5 0

screen config
pic obj\i000_000.bmp 0 0
pic obj\i006_000.bmp 18 122
pic obj\f001_000.bmp 0 0
pic obj\i002_000.bmp 19 0
button tab obj\i004_000.bmp 1148 132 3 5 0
button tab2 obj\i004_000.bmp 1148 192 3 5 1
button tab3 obj\i004_000.bmp 1148 252 3 5 2
button cfg_defaults obj\i003_000.bmp 1148 347 3 5 0
button cfg_undo obj\i003_000.bmp 1148 407 3 5 1
button cfg_apply obj\i003_000.bmp 1148 467 3 5 2
button nav_save obj\f002_000.bmp 666 6 8 5 0
button nav_load obj\f002_000.bmp 791 6 8 5 1
button nav_cg obj\f002_000.bmp 916 6 8 5 2
button nav_scene obj\f002_000.bmp 1041 6 8 5 3
button nav_music obj\f002_000.bmp 704 51 8 5 4
button nav_config obj\f002_000.bmp 829 51 8 5 5
button nav_title obj\f002_000.bmp 954 51 8 5 6
button nav_quit obj\f002_000.bmp 1079 51 8 5 7
button back obj\f002_001.bmp 1195 11 1 5 0
# Resolution and choosing the window size at start have no meaning here.
radio screen 0 obj\i007_000.bmp 172 151 2 5 0
radio screen 1 obj\i007_000.bmp 294 151 2 5 1
pic obj\i008_000.bmp 172 193 6 5 0 4
pic obj\i008_000.bmp 294 193 6 5 1 4
pic obj\i008_000.bmp 416 193 6 5 2 4
pic obj\i008_000.bmp 172 225 6 5 3 4
pic obj\i008_000.bmp 294 225 6 5 4 4
pic obj\i008_000.bmp 416 225 6 5 5 4
radio effect 1 obj\i009_000.bmp 50 289 2 5 0
radio effect 0 obj\i009_000.bmp 131 289 2 5 1
radio wait 1 obj\i009_000.bmp 226 289 2 5 0
radio wait 0 obj\i009_000.bmp 307 289 2 5 1
radio waitskip 1 obj\i009_000.bmp 402 289 2 5 0
radio waitskip 0 obj\i009_000.bmp 483 289 2 5 1
radio camera 1 obj\i009_000.bmp 615 437 2 5 0
radio camera 0 obj\i009_000.bmp 696 437 2 5 1
slider opacity obj\i010_000.bmp 155 367
number opacity obj\f012_000.bmp 415 338
step opacity -1 obj\i010_001.bmp 119 369 2 5 0
step opacity 1 obj\i010_001.bmp 469 369 2 5 1
check confirm_saveload obj\i012_000.bmp 603 177 8 5 0
check confirm_overwrite obj\i012_000.bmp 603 205 8 5 1
check confirm_delete obj\i012_000.bmp 603 233 8 5 2
check confirm_quick obj\i012_000.bmp 603 261 8 5 3
check confirm_title obj\i012_000.bmp 603 289 8 5 4
check confirm_exit obj\i012_000.bmp 603 317 8 5 5
check tutorial obj\i012_000.bmp 603 345 8 5 6
pic obj\i012_000.bmp 603 373 8 5 7 4

screen config2
pic obj\i000_000.bmp 0 0
pic obj\i013_000.bmp 18 122
pic obj\f001_000.bmp 0 0
pic obj\i002_000.bmp 19 0
button tab obj\i004_000.bmp 1148 132 3 5 0
button tab2 obj\i004_000.bmp 1148 192 3 5 1
button tab3 obj\i004_000.bmp 1148 252 3 5 2
button cfg_defaults obj\i003_000.bmp 1148 347 3 5 0
button cfg_undo obj\i003_000.bmp 1148 407 3 5 1
button cfg_apply obj\i003_000.bmp 1148 467 3 5 2
button nav_save obj\f002_000.bmp 666 6 8 5 0
button nav_load obj\f002_000.bmp 791 6 8 5 1
button nav_cg obj\f002_000.bmp 916 6 8 5 2
button nav_scene obj\f002_000.bmp 1041 6 8 5 3
button nav_music obj\f002_000.bmp 704 51 8 5 4
button nav_config obj\f002_000.bmp 829 51 8 5 5
button nav_title obj\f002_000.bmp 954 51 8 5 6
button nav_quit obj\f002_000.bmp 1079 51 8 5 7
button back obj\f002_001.bmp 1195 11 1 5 0
radio text 0 obj\i014_000.bmp 64 182 4 5 0
radio auto 0 obj\i014_000.bmp 64 260 4 5 0
radio text 1 obj\i014_000.bmp 186 182 4 5 1
radio auto 1 obj\i014_000.bmp 186 260 4 5 1
radio text 2 obj\i014_000.bmp 308 182 4 5 2
radio auto 2 obj\i014_000.bmp 308 260 4 5 2
radio text 3 obj\i014_000.bmp 430 182 4 5 3
radio auto 3 obj\i014_000.bmp 430 260 4 5 3
# Wait time, ×0.70 to ×1.30 in steps of 0.01, stretches waits and fades; the bar is full at ×2.00.
range waittime 70 130
slider waittime obj\i010_000.bmp 155 638 200
number waittime obj\f012_000.bmp 413 609
step waittime -1 obj\i010_001.bmp 119 640 2 5 0
step waittime 1 obj\i010_001.bmp 469 640 2 5 1
radio skipall 0 obj\i015_000.bmp 617 172 2 5 0
radio skipall 1 obj\i015_000.bmp 698 172 2 5 1
radio deckey 0 obj\i016_000.bmp 845 172 2 5 0
radio deckey 1 obj\i016_000.bmp 926 172 2 5 1
radio rightclick 0 obj\i017_000.bmp 710 215 5 5 0
radio rightclick 1 obj\i017_000.bmp 832 215 5 5 1
radio rightclick 2 obj\i017_000.bmp 954 215 5 5 2
radio rightclick 3 obj\i017_000.bmp 710 247 5 5 3
radio rightclick 4 obj\i017_000.bmp 832 247 5 5 4
radio pause_inactive 1 obj\i009_000.bmp 617 310 2 5 0
radio pause_inactive 0 obj\i009_000.bmp 698 310 2 5 1
# Electric Onahole drives a toy over USB on the PC. VNDegen keeps the choice and does nothing with it.
radio onahole 1 obj\i009_000.bmp 845 310 2 5 0
radio onahole 0 obj\i009_000.bmp 926 310 2 5 1

screen config3
pic obj\i000_000.bmp 0 0
# Fiona as a silhouette until the script that names her, ep_011_001_000, has been read to its end, then herself, as on
# the PC under Wine.
pic obj\i118_000.bmp 18 122 unlessread scr\ep_011_001_000.txt
pic obj\i018_000.bmp 18 122 ifread scr\ep_011_001_000.txt
pic obj\f001_000.bmp 0 0
pic obj\i002_000.bmp 19 0
button tab obj\i004_000.bmp 1148 132 3 5 0
button tab2 obj\i004_000.bmp 1148 192 3 5 1
button tab3 obj\i004_000.bmp 1148 252 3 5 2
button cfg_defaults obj\i003_000.bmp 1148 347 3 5 0
button cfg_undo obj\i003_000.bmp 1148 407 3 5 1
button cfg_apply obj\i003_000.bmp 1148 467 3 5 2
button nav_save obj\f002_000.bmp 666 6 8 5 0
button nav_load obj\f002_000.bmp 791 6 8 5 1
button nav_cg obj\f002_000.bmp 916 6 8 5 2
button nav_scene obj\f002_000.bmp 1041 6 8 5 3
button nav_music obj\f002_000.bmp 704 51 8 5 4
button nav_config obj\f002_000.bmp 829 51 8 5 5
button nav_title obj\f002_000.bmp 954 51 8 5 6
button nav_quit obj\f002_000.bmp 1079 51 8 5 7
button back obj\f002_001.bmp 1195 11 1 5 0
slider master obj\i010_000.bmp 155 188
number master obj\f012_000.bmp 415 159
step master -1 obj\i010_001.bmp 119 190 2 5 0
step master 1 obj\i010_001.bmp 469 190 2 5 1
check mute_master obj\i021_000.bmp 479 155 1 5 0
pic obj\i020_000.bmp 533 155 1 5 0 4
slider bgm obj\i010_000.bmp 155 266
number bgm obj\f012_000.bmp 415 237
step bgm -1 obj\i010_001.bmp 119 268 2 5 0
step bgm 1 obj\i010_001.bmp 469 268 2 5 1
check mute_bgm obj\i021_000.bmp 479 233 1 5 0
pic obj\i020_000.bmp 533 233 1 5 0 4
slider se obj\i010_000.bmp 155 344
number se obj\f012_000.bmp 415 315
step se -1 obj\i010_001.bmp 119 346 2 5 0
step se 1 obj\i010_001.bmp 469 346 2 5 1
check mute_se obj\i021_000.bmp 479 311 1 5 0
slider hse obj\i010_000.bmp 155 422
number hse obj\f012_000.bmp 415 393
step hse -1 obj\i010_001.bmp 119 424 2 5 0
step hse 1 obj\i010_001.bmp 469 424 2 5 1
check mute_hse obj\i021_000.bmp 479 389 1 5 0
slider voice obj\i010_000.bmp 155 501
number voice obj\f012_000.bmp 415 472
step voice -1 obj\i010_001.bmp 119 503 2 5 0
step voice 1 obj\i010_001.bmp 469 503 2 5 1
check mute_voice obj\i021_000.bmp 479 468 1 5 0
slider backvoice obj\i010_000.bmp 155 579
number backvoice obj\f012_000.bmp 415 550
step backvoice -1 obj\i010_001.bmp 119 581 2 5 0
step backvoice 1 obj\i010_001.bmp 469 581 2 5 1
check mute_backvoice obj\i021_000.bmp 479 546 1 5 0
# Stop voice after clicking.
radio voice_stop 1 obj\i009_000.bmp 617 172 2 5 0
radio voice_stop 0 obj\i009_000.bmp 698 172 2 5 1
# Each ♪ plays the sample the PC reads for it under Wine; master and music have none, as on the PC.
button test:se:se\se_05.ogg obj\i020_000.bmp 533 311 1 5 0
button test:se:se\hse_002_005.ogg obj\i020_000.bmp 533 389 1 5 0
button test:voice:voice\mis0_0013.ogg obj\i020_000.bmp 533 468 1 5 0
button test:voice:voice\eim9_0003.ogg obj\i020_000.bmp 533 546 1 5 0
# Fiona's box reads ???? until the story names her, then her name.
readswap scr\ep_011_001_000.txt obj\i119_000.bmp obj\i019_000.bmp
check voice0 obj\i119_000.bmp 785 588 7 5 0
check voice1 obj\i119_000.bmp 885 549 7 5 1
check voice2 obj\i119_000.bmp 983 588 7 5 2
check voice3 obj\i119_000.bmp 587 588 7 5 3
check voice4 obj\i119_000.bmp 684 549 7 5 4
check voice5 obj\i119_000.bmp 854 648 7 5 5
check voice6 obj\i119_000.bmp 978 648 7 5 6

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
# Quick saves follow the 60 cards, as the PC writes them.
quickslots 61 66
default opacity 100
default onahole 1
default delay 100

screen choice
# Its one choice, at the end, has its own picture and no band behind it: four options filling the screen's height. Their
# places are a guess; no PC save reaches that choice yet.
place 0 179
screen start
# New Game starts the first chapter; init.ini names no FirstFlow.
flow Flow_01

screen dialog
pic obj\b001_000.bmp 0 241
pic obj\j000_000.bmp 320 150 1 8 0 0
button yes obj\b003_000.bmp 410 270 2 5 0
button no obj\b003_000.bmp 690 270 2 5 1

screen log
pic obj\n001_000.bmp 1191 110
thumb obj\e007_001.bmp 1180 95 525
rows obj\n000_000.bmp 77 61 165 4 146 36
button log_close obj\m001_000.bmp 1227 19 5 5 0
button log_top obj\e007_002.bmp 1180 34 4 5 0
button log_up obj\e007_002.bmp 1180 67 4 5 1
button log_down obj\e007_002.bmp 1180 622 4 5 2
button log_bottom obj\e007_002.bmp 1180 655 4 5 3

screen defaults
# The official English release sets its text in Lucida Sans Unicode (Wine's font log), here Open Sans, at the size and
# letter gap that best fit the word widths of the publisher's Windows screenshots, lines as far apart as there, and the
# first glyph one gap in.
textfont 7
textsize 16
textgap 3.93
textline 27
textshift 3 0
)";
