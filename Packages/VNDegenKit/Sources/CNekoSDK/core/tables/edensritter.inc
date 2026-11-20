const char kEdensRitterShared[] = R"(
screen load
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\e003_000.bmp -19 -3 1 2 0 1
slots obj\e005_000.bmp 30 101 536 122 4 2
slotcols 8
slotlabel obj\e006_001.bmp 5 9
slotdigits obj\e006_000.bmp 39 4 2
slotthumb 9 10
slotlock obj\e008_000.bmp 434 53
slotdelete obj\e009_000.bmp 464 57
slotnew obj\e010_000.bmp 469 6
datedigits obj\e007_000.bmp 220 57
dateslash obj\e007_002.bmp
datecolon obj\e007_003.bmp
datedash obj\e007_001.bmp
dategap 8
slotcomment 180 100 330 18 Saved Game
slotcommentcut 29
button page0 obj\e001_000.bmp 423 632 11 5 0
button page1 obj\e001_000.bmp 479 632 11 5 1
button page2 obj\e001_000.bmp 535 632 11 5 2
button page3 obj\e001_000.bmp 591 632 11 5 3
button page4 obj\e001_000.bmp 647 632 11 5 4
button page5 obj\e001_000.bmp 703 632 11 5 5
button page6 obj\e001_000.bmp 759 632 11 5 6
button page7 obj\e001_000.bmp 815 632 11 5 7
button page8 obj\e001_000.bmp 871 632 11 5 8
button page9 obj\e001_000.bmp 927 632 11 5 9
button page10 obj\e001_000.bmp 983 632 11 5 10
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8

screen save
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\e003_000.bmp -19 -3 1 2 0 0
slots obj\e005_000.bmp 30 101 536 122 4 2
slotcols 8
slotlabel obj\e006_001.bmp 5 9
slotdigits obj\e006_000.bmp 39 4 2
slotthumb 9 10
slotlock obj\e008_000.bmp 434 53
slotdelete obj\e009_000.bmp 464 57
slotnew obj\e010_000.bmp 469 6
datedigits obj\e007_000.bmp 220 57
dateslash obj\e007_002.bmp
datecolon obj\e007_003.bmp
datedash obj\e007_001.bmp
dategap 8
slotcomment 180 100 330 18 Saved Game
slotcommentcut 29
button page0 obj\e001_000.bmp 423 632 11 5 0
button page1 obj\e001_000.bmp 479 632 11 5 1
button page2 obj\e001_000.bmp 535 632 11 5 2
button page3 obj\e001_000.bmp 591 632 11 5 3
button page4 obj\e001_000.bmp 647 632 11 5 4
button page5 obj\e001_000.bmp 703 632 11 5 5
button page6 obj\e001_000.bmp 759 632 11 5 6
button page7 obj\e001_000.bmp 815 632 11 5 7
button page8 obj\e001_000.bmp 871 632 11 5 8
button page9 obj\e001_000.bmp 927 632 11 5 9
button page10 obj\e001_000.bmp 983 632 11 5 10
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8

screen dialog
dim 0
pic obj\b002_000.bmp 0 42
pic obj\b001_000.bmp 0 41
pic obj\b001_000.bmp 0 465
pic obj\k000_000.bmp 387 54 1 8 0 0
button yes obj\b004_000.bmp 432 201 2 5 0
button no obj\b004_000.bmp 704 201 2 5 1
button noask obj\k001_000.bmp 477 405 1 5 0

screen log
pic obj\p001_000.bmp 1185 110
scroll obj\p002_000t.bmp obj\p002_000m.bmp obj\p002_000b.bmp 1185 110 500
rows obj\p000_000.bmp 77 69 165 4 99 35
logvoice obj\o001_000.bmp 2 5 1 1112 36
button log_close obj\p003_000.bmp 1227 19 5 5 4
button log_top obj\p003_000.bmp 1180 34 5 5 0
button log_up obj\p003_000.bmp 1180 67 5 5 1
button log_down obj\p003_000.bmp 1180 622 5 5 2
button log_bottom obj\p003_000.bmp 1180 655 5 5 3

screen choice
pic obj\b002_000.bmp 0 42
pic obj\b001_000.bmp 0 41
pic obj\b001_000.bmp 0 465
pic obj\q000_000.bmp 387 54
place 160 152

screen music
selectedlook
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\i001_000.bmp 0 65
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\i000_000.bmp -19 -3
pic obj\i003_000.bmp 548 596
pic obj\i001_001.bmp 77 100 part 0 3 480 46 ifread bgm\bgm01.ogg
pic obj\i001_001.bmp 77 146 part 0 49 480 46 ifread bgm\bgm02.ogg
pic obj\i001_001.bmp 77 192 part 0 95 480 46 ifread bgm\bgm03.ogg
pic obj\i001_001.bmp 77 238 part 0 141 480 46 ifread bgm\bgm04.ogg
pic obj\i001_001.bmp 77 285 part 0 188 480 46 ifread bgm\bgm05.ogg
pic obj\i001_001.bmp 77 330 part 0 233 480 46 ifread bgm\bgm06.ogg
pic obj\i001_001.bmp 77 376 part 0 279 480 46 ifread bgm\bgm07.ogg
pic obj\i001_001.bmp 77 422 part 0 325 480 46 ifread bgm\bgm08.ogg
pic obj\i001_001.bmp 77 468 part 0 371 480 46 ifread bgm\bgm09.ogg
pic obj\i001_001.bmp 557 100 part 480 3 480 46 ifread bgm\bgm10.ogg
pic obj\i001_001.bmp 557 146 part 480 49 480 46 ifread bgm\bgm11.ogg
pic obj\i001_001.bmp 557 192 part 480 95 480 46 ifread bgm\bgm12.ogg
pic obj\i001_001.bmp 557 238 part 480 141 480 46 ifread bgm\bgm13.ogg
pic obj\i001_001.bmp 557 285 part 480 188 480 46 ifread bgm\bgm14.ogg
pic obj\i001_001.bmp 557 330 part 480 233 480 46 ifread bgm\bgm15.ogg
pic obj\i001_001.bmp 557 376 part 480 279 480 46 ifread bgm\bgm16.ogg
pic obj\i001_001.bmp 557 422 part 480 325 480 46 ifread bgm\bgm17.ogg
button track0 obj\i002_000.bmp 91 100 4 5 2
button track1 obj\i002_000.bmp 91 146 4 5 2
button track2 obj\i002_000.bmp 91 192 4 5 2
button track3 obj\i002_000.bmp 91 238 4 5 2
button track4 obj\i002_000.bmp 91 285 4 5 2
button track5 obj\i002_000.bmp 91 330 4 5 2
button track6 obj\i002_000.bmp 91 376 4 5 2
button track7 obj\i002_000.bmp 91 422 4 5 2
button track8 obj\i002_000.bmp 91 468 4 5 2
button track9 obj\i002_000.bmp 571 100 4 5 2
button track10 obj\i002_000.bmp 571 146 4 5 2
button track11 obj\i002_000.bmp 571 192 4 5 2
button track12 obj\i002_000.bmp 571 238 4 5 2
button track13 obj\i002_000.bmp 571 285 4 5 2
button track14 obj\i002_000.bmp 571 330 4 5 2
button track15 obj\i002_000.bmp 571 376 4 5 2
button track16 obj\i002_000.bmp 571 422 4 5 2
button music_prev obj\i003_001.bmp 562 631 7 5 0 help obj\i004_000.bmp 790 683 1 8 0 0
button music_play obj\i003_001.bmp 606 631 7 5 1 help obj\i004_000.bmp 790 683 1 8 0 1
button music_pause obj\i003_001.bmp 650 631 7 5 2 help obj\i004_000.bmp 790 683 1 8 0 2
button music_stop obj\i003_001.bmp 694 631 7 5 3 help obj\i004_000.bmp 790 683 1 8 0 3
button music_next obj\i003_001.bmp 738 631 7 5 4 help obj\i004_000.bmp 790 683 1 8 0 4
button music_repeat obj\i003_001.bmp 782 631 7 5 5 help obj\i004_000.bmp 790 683 1 8 0 5
button music_shuffle obj\i003_001.bmp 826 631 7 5 6 help obj\i004_000.bmp 790 683 1 8 0 6
slider music obj\i003_002.bmp 903 652
number music obj\i003_003.bmp 955 633 12
hover 900 625 1000 670 obj\i004_000.bmp 790 683 8 7
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8
)";
const char kEdensRitter1[] = R"(
screen title
pic obj\c000_000.bmp 0 -720
pic obj\d002_000.bmp 0 424
pic obj\d001_000.bmp 199 50
button start obj\d004_000.bmp 347 444 9 5 0 help obj\d005_000.bmp 0 666 1 10 0 0
button load obj\d004_000.bmp 549 444 9 5 1 help obj\d005_000.bmp 0 666 1 10 0 1
button quickload obj\d004_000.bmp 754 444 9 5 2 help obj\d005_000.bmp 0 666 1 10 0 2
button cg obj\d004_000.bmp 347 502 9 5 3 help obj\d005_000.bmp 0 666 1 10 0 3
button scene obj\d004_000.bmp 549 502 9 5 4 help obj\d005_000.bmp 0 666 1 10 0 4
button music obj\d004_000.bmp 754 502 9 5 5 help obj\d005_000.bmp 0 666 1 10 0 5
button manual obj\d004_000.bmp 347 560 9 5 6 help obj\d005_000.bmp 0 666 1 10 0 6
button config obj\d004_000.bmp 549 560 9 5 7 help obj\d005_000.bmp 0 666 1 10 0 7
button quit obj\d004_000.bmp 754 560 9 5 8 help obj\d005_000.bmp 0 666 1 10 0 8
button flowkey:BangaiFlow obj\d200_000.bmp 1164 440 1 5 0 help obj\d200_001.bmp 0 666 1 1 0 0
button flowkey:ExFlow obj\d300_000.bmp 1164 532 1 5 0 help obj\d300_001.bmp 0 666 1 1 0 0
button versioninfo obj\d008_000.bmp 1114 6 1 5 0 help obj\d008_001.bmp 0 666 1 1 0 0

screen config
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\j002_000.bmp 0 47
pic obj\j003_000.bmp 79 61 part 0 0 440 607
pic obj\j003_000.bmp 519 61 part 440 0 477 607
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\j000_000.bmp -19 -3
button cfg_defaults obj\j001_000.bmp 1010 60 3 5 0 help obj\j001_001.bmp 790 683 1 3 0 0
button cfg_undo obj\j001_000.bmp 1010 143 3 5 1 help obj\j001_001.bmp 790 683 1 3 0 1
button cfg_apply obj\j001_000.bmp 1010 226 3 5 2 help obj\j001_001.bmp 790 683 1 3 0 2
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8
radio screen 0 obj\j005_000.bmp 187 67 3 5 0
radio screen 1 obj\j005_000.bmp 289 67 3 5 1
radio screen 2 obj\j005_000.bmp 391 67 3 5 2
pic obj\j005_001.bmp 187 98 6 5 0 2
pic obj\j005_001.bmp 289 98 6 5 1 0
pic obj\j005_001.bmp 391 98 6 5 2 0
pic obj\j005_001.bmp 187 122 6 5 3 4
pic obj\j005_001.bmp 289 122 6 5 4 0
pic obj\j005_001.bmp 391 122 6 5 5 0
radio effect 1 obj\j005_002.bmp 84 172 2 5 0
radio effect 0 obj\j005_002.bmp 152 172 2 5 1
radio wait 1 obj\j005_002.bmp 230 172 2 5 0
radio wait 0 obj\j005_002.bmp 298 172 2 5 1
radio waitskip 1 obj\j005_002.bmp 376 172 2 5 0
radio waitskip 0 obj\j005_002.bmp 444 172 2 5 1
slider opacity obj\j005_003.bmp 163 226
number opacity obj\j005_005.bmp 384 203
step opacity -1 obj\j005_004.bmp 137 227 2 5 0
step opacity 1 obj\j005_004.bmp 442 227 2 5 1
check confirm_saveload obj\j005_006.bmp 87 275 8 5 0
check confirm_overwrite obj\j005_006.bmp 87 298 8 5 1
check confirm_delete obj\j005_006.bmp 87 321 8 5 2
check confirm_quick obj\j005_006.bmp 87 344 8 5 3
check confirm_title obj\j005_006.bmp 87 367 8 5 4
check confirm_exit obj\j005_006.bmp 87 390 8 5 5
check tutorial obj\j005_006.bmp 87 413 8 5 6
pic obj\j005_006.bmp 87 436 8 5 7 0
radio camera 1 obj\j005_002.bmp 108 488 2 5 0
radio camera 0 obj\j005_002.bmp 176 488 2 5 1
radio scat 1 obj\j005_002.bmp 353 488 2 5 0
radio scat 0 obj\j005_002.bmp 421 488 2 5 1
slider text obj\j005_003.bmp 163 544
number text obj\j005_005.bmp 384 523
step text -1 obj\j005_004.bmp 137 545 2 5 0
step text 1 obj\j005_004.bmp 442 545 2 5 1
slider auto obj\j005_003.bmp 163 594
number auto obj\j005_005.bmp 384 573
step auto -1 obj\j005_004.bmp 137 595 2 5 0
step auto 1 obj\j005_004.bmp 442 595 2 5 1
range waittime 70 130
slider waittime obj\j005_003.bmp 163 644 200
number waittime obj\j005_005.bmp 378 624 400 415
step waittime -1 obj\j005_004.bmp 137 645 2 5 0
step waittime 1 obj\j005_004.bmp 442 645 2 5 1
radio skipall 0 obj\j006_000.bmp 542 86 2 5 0
radio skipall 1 obj\j006_000.bmp 610 86 2 5 1
radio deckey 0 obj\j006_001.bmp 694 86 2 5 0
radio deckey 1 obj\j006_001.bmp 762 86 2 5 1
radio rightclick 0 obj\j006_002.bmp 685 116 5 5 0
radio rightclick 1 obj\j006_002.bmp 785 116 5 5 1
radio rightclick 2 obj\j006_002.bmp 885 116 5 5 2
radio rightclick 3 obj\j006_002.bmp 685 140 5 5 3
radio rightclick 4 obj\j006_002.bmp 785 140 5 5 4
radio pause_inactive 1 obj\j005_002.bmp 542 190 2 5 0
radio pause_inactive 0 obj\j005_002.bmp 610 190 2 5 1
slider master obj\j005_003.bmp 588 246
number master obj\j005_005.bmp 820 223
step master -1 obj\j005_004.bmp 562 247 2 5 0
step master 1 obj\j005_004.bmp 868 247 2 5 1
check mute_master obj\j010_000.bmp 958 241 1 5 0
pic obj\j009_000.bmp 911 243 1 5 0 4
slider bgm obj\j005_003.bmp 588 298
number bgm obj\j005_005.bmp 820 275
step bgm -1 obj\j005_004.bmp 562 299 2 5 0
step bgm 1 obj\j005_004.bmp 868 299 2 5 1
pic obj\j009_000.bmp 911 295 1 5 0 0
check mute_bgm obj\j010_000.bmp 958 293 1 5 0
slider se obj\j005_003.bmp 588 350
number se obj\j005_005.bmp 820 327
step se -1 obj\j005_004.bmp 562 351 2 5 0
step se 1 obj\j005_004.bmp 868 351 2 5 1
pic obj\j009_000.bmp 911 347 1 5 0 0
check mute_se obj\j010_000.bmp 958 345 1 5 0
slider hse obj\j005_003.bmp 588 403
number hse obj\j005_005.bmp 820 379
step hse -1 obj\j005_004.bmp 562 402 2 5 0
step hse 1 obj\j005_004.bmp 868 402 2 5 1
pic obj\j009_000.bmp 911 399 1 5 0 0
check mute_hse obj\j010_000.bmp 958 397 1 5 0
slider voice obj\j005_003.bmp 588 454
number voice obj\j005_005.bmp 820 431
step voice -1 obj\j005_004.bmp 562 455 2 5 0
step voice 1 obj\j005_004.bmp 868 455 2 5 1
pic obj\j009_000.bmp 911 451 1 5 0 0
check mute_voice obj\j010_000.bmp 958 449 1 5 0
slider backvoice obj\j005_003.bmp 588 506
number backvoice obj\j005_005.bmp 820 483
step backvoice -1 obj\j005_004.bmp 562 507 2 5 0
step backvoice 1 obj\j005_004.bmp 868 507 2 5 1
pic obj\j009_000.bmp 911 503 1 5 0 0
check mute_backvoice obj\j010_000.bmp 958 501 1 5 0
radio voice_stop 1 obj\j005_002.bmp 562 554 2 5 0
radio voice_stop 0 obj\j005_002.bmp 630 554 2 5 1
check voice0 obj\j008_000.bmp 562 607 8 5 0
check voice1 obj\j008_000.bmp 662 607 8 5 1
check voice2 obj\j008_000.bmp 762 607 8 5 2
check voice3 obj\j008_000.bmp 862 607 8 5 3
check voice4 obj\j008_000.bmp 562 631 8 5 4
check voice5 obj\j008_000.bmp 662 631 8 5 5
check voice6 obj\j008_000.bmp 762 631 8 5 6
check voice7 obj\j008_000.bmp 862 631 8 5 7
hover 79 61 519 92 obj\j004_000.bmp 301 683
hover 79 92 519 150 obj\j004_001.bmp 614 683 8 0
hover 79 155 225 200 obj\j004_001.bmp 614 683 8 1
hover 225 155 371 200 obj\j004_001.bmp 614 683 8 2
hover 371 155 519 200 obj\j004_001.bmp 614 683 8 3
hover 79 200 519 252 obj\j004_001.bmp 614 683 8 4
hover 79 252 519 465 obj\j004_001.bmp 614 683 8 5
hover 79 465 299 515 obj\j004_001.bmp 614 683 8 6
hover 299 465 519 515 obj\j004_001.bmp 614 683 8 7
hover 79 515 519 567 obj\j004_002.bmp 724 683 3 0
hover 79 567 519 617 obj\j004_002.bmp 724 683 3 1
hover 79 617 519 668 obj\j004_002.bmp 724 683 3 2
hover 519 61 680 110 obj\j004_003.bmp 534 683 5 0
hover 680 61 990 110 obj\j004_003.bmp 534 683 5 1
hover 519 110 990 165 obj\j004_003.bmp 534 683 5 2
hover 519 165 990 215 obj\j004_003.bmp 534 683 5 3
hover 519 215 990 268 obj\j004_004.bmp 664 683 8 0
hover 519 268 990 320 obj\j004_004.bmp 664 683 8 1
hover 519 320 990 372 obj\j004_004.bmp 664 683 8 2
hover 519 372 990 424 obj\j004_004.bmp 664 683 8 3
hover 519 424 990 476 obj\j004_004.bmp 664 683 8 4
hover 519 476 990 528 obj\j004_004.bmp 664 683 8 5
hover 519 528 990 580 obj\j004_004.bmp 664 683 8 6
hover 519 580 990 660 obj\j004_004.bmp 664 683 8 7

screen defaults
titlebgmheard
inneralways
textcomment
textsize 22
textgap 1
textshift 1 -1
textline 33
textfontfile fonts\EdensRitterEN.ttf
versionline Eden's Ritter: Paladins of Ecstasy
versionline File Version: 1, 0, 0, 0
versionline
versionline ○ Doppelganger Gaiden
versionline ○ Xenoarachne Gaiden
manual manual\index.html
default opacity 80
default text 50
default auto 50
default bgm 100
default voice 100
default backvoice 100

screen cg
selectedlook
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\f004_001.bmp 0 0 alpha 80 tab 0
pic obj\f004_000.bmp 166 0 tab 0
pic obj\f004_003.bmp 0 0 alpha 80 tab 1
pic obj\f004_002.bmp 166 0 tab 1
pic obj\f004_005.bmp 0 0 alpha 80 tab 2
pic obj\f004_004.bmp 166 0 tab 2
pic obj\f004_006.bmp 166 0 tab 3
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\f005_000.bmp 0 560 1 2 0 0
pic obj\f005_000.bmp 0 617 1 2 0 1
pic obj\f000_000.bmp -19 -3 1 4 0 0
cellsheet obj\f001_000.bmp 8
cellsize 202 154
cellthumb 26 40
cell 450 100
cell 666 100
cell 882 100
cell 450 268
cell 666 268
cell 882 268
cell 450 436
cell 666 436
cell 882 436
celllabel obj\e006_001.bmp 16 15
celldigits obj\e006_000.bmp 50 10 3 18
gallerytabs 0
button page0 obj\e001_000.bmp 534 626 11 5 0
button page1 obj\e001_000.bmp 590 626 11 5 1
button page2 obj\e001_000.bmp 646 626 11 5 2
button gtab0 obj\f005_001.bmp 124 564 4 5 0
button gtab1 obj\f005_001.bmp 230 564 4 5 1
button gtab2 obj\f005_001.bmp 83 621 4 5 2
button gtab3 obj\f005_001.bmp 189 621 4 5 3
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8

screen scene
selectedlook
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\f004_001.bmp 0 0 alpha 80 tab 0
pic obj\f004_000.bmp 166 0 tab 0
pic obj\f004_003.bmp 0 0 alpha 80 tab 1
pic obj\f004_002.bmp 166 0 tab 1
pic obj\f004_005.bmp 0 0 alpha 80 tab 2
pic obj\f004_004.bmp 166 0 tab 2
pic obj\f004_006.bmp 166 0 tab 3
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\f005_000.bmp 0 560 1 2 0 0
pic obj\f005_000.bmp 0 617 1 2 0 1
pic obj\f000_000.bmp -19 -3 1 4 0 3
cellsheet obj\f001_000.bmp 8
cellsize 202 154
cellthumb 26 40
cell 450 100
cell 666 100
cell 882 100
cell 450 268
cell 666 268
cell 882 268
cell 450 436
cell 666 436
cell 882 436
celllabel obj\e006_001.bmp 16 15
celldigits obj\e006_000.bmp 50 10 3 18
gallerytabs 0
button page0 obj\e001_000.bmp 534 626 11 5 0
button page1 obj\e001_000.bmp 590 626 11 5 1
button page2 obj\e001_000.bmp 646 626 11 5 2
button gtab0 obj\f005_001.bmp 124 564 4 5 0
button gtab1 obj\f005_001.bmp 230 564 4 5 1
button gtab2 obj\f005_001.bmp 83 621 4 5 2
button gtab3 obj\f005_001.bmp 189 621 4 5 3
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8
)";
const char kEdensRitter2[] = R"(
screen title
pic obj\c000_000.bmp 0 -720
pic obj\d002_000.bmp 0 424
pic obj\d001_000.bmp 189 40
button start obj\d004_000.bmp 347 444 9 5 0 help obj\d005_000.bmp 0 666 1 10 0 0
button load obj\d004_000.bmp 549 444 9 5 1 help obj\d005_000.bmp 0 666 1 10 0 1
button quickload obj\d004_000.bmp 754 444 9 5 2 help obj\d005_000.bmp 0 666 1 10 0 2
button cg obj\d004_000.bmp 347 502 9 5 3 help obj\d005_000.bmp 0 666 1 10 0 3
button scene obj\d004_000.bmp 549 502 9 5 4 help obj\d005_000.bmp 0 666 1 10 0 4
button music obj\d004_000.bmp 754 502 9 5 5 help obj\d005_000.bmp 0 666 1 10 0 5
button manual obj\d004_000.bmp 347 560 9 5 6 help obj\d005_000.bmp 0 666 1 10 0 6
button config obj\d004_000.bmp 549 560 9 5 7 help obj\d005_000.bmp 0 666 1 10 0 7
button quit obj\d004_000.bmp 754 560 9 5 8 help obj\d005_000.bmp 0 666 1 10 0 8
button versioninfo obj\d008_000.bmp 1114 6 1 5 0 help obj\d008_001.bmp 0 666 1 1 0 0

screen defaults
titlebgmheard
inneralways
textcomment
textsize 22
textgap 1
textshift 1 -1
textline 33
textfontfile fonts\EdensRitterEN.ttf
versionline Eden's Ritter: Priestess of Pleasure
versionline File Version: 1, 0, 0, 0
manual manual\index.html
default opacity 100
default bgm 80
default se 80
default hse 80
default voice 80
default backvoice 80

screen config
selectedlook
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\j004_000.bmp 15 115
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\j000_000.bmp 0 0
button tab obj\j002_000.bmp 455 25 3 5 0 help obj\j002_001.bmp 717 683 1 3 0 0
button tab2 obj\j002_000.bmp 538 25 3 5 1 help obj\j002_001.bmp 717 683 1 3 0 1
button tab3 obj\j002_000.bmp 621 25 3 5 2 help obj\j002_001.bmp 717 683 1 3 0 2
button cfg_defaults obj\j001_000.bmp 839 25 3 5 0 help obj\j001_001.bmp 790 683 1 3 0 0
button cfg_undo obj\j001_000.bmp 922 25 3 5 1 help obj\j001_001.bmp 790 683 1 3 0 1
button cfg_apply obj\j001_000.bmp 1005 25 3 5 2 help obj\j001_001.bmp 790 683 1 3 0 2
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8
hover 16 116 536 156 obj\j003_000.bmp 417 683 8 0
hover 16 158 536 228 obj\j003_000.bmp 417 683 8 1
hover 16 232 185 294 obj\j003_000.bmp 417 683 8 2
hover 192 232 360 294 obj\j003_000.bmp 417 683 8 3
hover 368 232 536 294 obj\j003_000.bmp 417 683 8 4
hover 16 297 536 636 obj\j003_000.bmp 417 683 8 5
hover 554 116 1074 377 obj\j003_000.bmp 417 683 8 6
hover 554 380 780 440 obj\j003_000.bmp 417 683 8 7
radio screen 0 obj\j005_000.bmp 144 121 2 5 0
radio screen 1 obj\j005_000.bmp 266 121 2 5 1
pic obj\j006_000.bmp 144 163 6 5 0 2
pic obj\j006_000.bmp 266 163 6 5 1 0
pic obj\j006_000.bmp 388 163 6 5 2 0
pic obj\j006_000.bmp 144 195 6 5 3 4
pic obj\j006_000.bmp 266 195 6 5 4 0
pic obj\j006_000.bmp 388 195 6 5 5 0
radio effect 1 obj\j007_000.bmp 22 259 2 5 0
radio effect 0 obj\j007_000.bmp 103 259 2 5 1
radio wait 1 obj\j007_000.bmp 198 259 2 5 0
radio wait 0 obj\j007_000.bmp 279 259 2 5 1
radio waitskip 1 obj\j007_000.bmp 374 259 2 5 0
radio waitskip 0 obj\j007_000.bmp 455 259 2 5 1
slider opacity obj\j008_000.bmp 128 338
number opacity obj\j010_000.bmp 388 309
step opacity -1 obj\j009_000.bmp 92 340 2 5 0
step opacity 1 obj\j009_000.bmp 442 340 2 5 1
pic obj\j011_000.bmp 59 545
pic obj\j011_001.bmp 59 545
check confirm_saveload obj\j012_000.bmp 575 147 8 5 0
check confirm_overwrite obj\j012_000.bmp 575 175 8 5 1
check confirm_delete obj\j012_000.bmp 575 203 8 5 2
check confirm_quick obj\j012_000.bmp 575 231 8 5 3
check confirm_title obj\j012_000.bmp 575 259 8 5 4
check confirm_exit obj\j012_000.bmp 575 287 8 5 5
check tutorial obj\j012_000.bmp 575 315 8 5 6
pic obj\j012_000.bmp 575 343 8 5 7 0
radio camera 1 obj\j007_000.bmp 587 407 2 5 0
radio camera 0 obj\j007_000.bmp 668 407 2 5 1

screen config2
selectedlook
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\j013_000.bmp 15 115
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\j000_000.bmp 0 0
button tab obj\j002_000.bmp 455 25 3 5 0 help obj\j002_001.bmp 717 683 1 3 0 0
button tab2 obj\j002_000.bmp 538 25 3 5 1 help obj\j002_001.bmp 717 683 1 3 0 1
button tab3 obj\j002_000.bmp 621 25 3 5 2 help obj\j002_001.bmp 717 683 1 3 0 2
button cfg_defaults obj\j001_000.bmp 839 25 3 5 0 help obj\j001_001.bmp 790 683 1 3 0 0
button cfg_undo obj\j001_000.bmp 922 25 3 5 1 help obj\j001_001.bmp 790 683 1 3 0 1
button cfg_apply obj\j001_000.bmp 1005 25 3 5 2 help obj\j001_001.bmp 790 683 1 3 0 2
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8
hover 16 116 536 195 obj\j003_001.bmp 724 683 3 0
hover 16 196 536 270 obj\j003_001.bmp 724 683 3 1
hover 16 560 536 636 obj\j003_001.bmp 724 683 3 2
hover 554 116 780 178 obj\j003_002.bmp 534 683 5 0
hover 782 116 1008 178 obj\j003_002.bmp 534 683 5 1
hover 554 180 1074 250 obj\j003_002.bmp 534 683 5 2
hover 554 254 780 316 obj\j003_002.bmp 534 683 5 3
radio text 0 obj\j014_000.bmp 37 153 4 5 0
radio text 1 obj\j014_000.bmp 159 153 4 5 1
radio text 2 obj\j014_000.bmp 281 153 4 5 2
radio text 3 obj\j014_000.bmp 403 153 4 5 3
radio auto 0 obj\j014_000.bmp 37 231 4 5 0
radio auto 1 obj\j014_000.bmp 159 231 4 5 1
radio auto 2 obj\j014_000.bmp 281 231 4 5 2
radio auto 3 obj\j014_000.bmp 403 231 4 5 3
radio skipall 0 obj\j015_000.bmp 590 143 2 5 0
radio skipall 1 obj\j015_000.bmp 671 143 2 5 1
radio deckey 0 obj\j016_000.bmp 818 143 2 5 0
radio deckey 1 obj\j016_000.bmp 899 143 2 5 1
radio rightclick 0 obj\j017_000.bmp 683 186 5 5 0
radio rightclick 1 obj\j017_000.bmp 805 186 5 5 1
radio rightclick 2 obj\j017_000.bmp 927 186 5 5 2
radio rightclick 3 obj\j017_000.bmp 683 218 5 5 3
radio rightclick 4 obj\j017_000.bmp 805 218 5 5 4
radio pause_inactive 1 obj\j007_000.bmp 590 281 2 5 0
radio pause_inactive 0 obj\j007_000.bmp 671 281 2 5 1
pic obj\j011_000.bmp 59 448
pic obj\j011_001.bmp 59 448
range waittime 70 130
slider waittime obj\j008_000.bmp 128 599 200
number waittime obj\j010_000.bmp 379 570 402 416
step waittime -1 obj\j009_000.bmp 92 601 2 5 0
step waittime 1 obj\j009_000.bmp 442 601 2 5 1

screen config3
selectedlook
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\j018_000.bmp 15 115
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\j000_000.bmp 0 0
button tab obj\j002_000.bmp 455 25 3 5 0 help obj\j002_001.bmp 717 683 1 3 0 0
button tab2 obj\j002_000.bmp 538 25 3 5 1 help obj\j002_001.bmp 717 683 1 3 0 1
button tab3 obj\j002_000.bmp 621 25 3 5 2 help obj\j002_001.bmp 717 683 1 3 0 2
button cfg_defaults obj\j001_000.bmp 839 25 3 5 0 help obj\j001_001.bmp 790 683 1 3 0 0
button cfg_undo obj\j001_000.bmp 922 25 3 5 1 help obj\j001_001.bmp 790 683 1 3 0 1
button cfg_apply obj\j001_000.bmp 1005 25 3 5 2 help obj\j001_001.bmp 790 683 1 3 0 2
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8
hover 16 116 536 190 obj\j003_003.bmp 417 683 8 0
hover 16 194 536 268 obj\j003_003.bmp 417 683 8 1
hover 16 272 536 346 obj\j003_003.bmp 417 683 8 2
hover 16 350 536 424 obj\j003_003.bmp 417 683 8 3
hover 16 428 536 502 obj\j003_003.bmp 417 683 8 4
hover 16 506 536 580 obj\j003_003.bmp 417 683 8 5
hover 554 116 780 178 obj\j003_003.bmp 417 683 8 6
hover 554 180 1074 658 obj\j003_003.bmp 417 683 8 7
slider master obj\j008_000.bmp 128 159
number master obj\j010_000.bmp 388 130
step master -1 obj\j009_000.bmp 92 161 2 5 0
step master 1 obj\j009_000.bmp 442 161 2 5 1
check mute_master obj\j021_000.bmp 452 126 1 5 0
pic obj\j020_000.bmp 506 126 1 5 0 4
slider bgm obj\j008_000.bmp 128 237
number bgm obj\j010_000.bmp 388 208
step bgm -1 obj\j009_000.bmp 92 239 2 5 0
step bgm 1 obj\j009_000.bmp 442 239 2 5 1
check mute_bgm obj\j021_000.bmp 452 204 1 5 0
pic obj\j020_000.bmp 506 204 1 5 0 0
slider se obj\j008_000.bmp 128 315
number se obj\j010_000.bmp 388 286
step se -1 obj\j009_000.bmp 92 317 2 5 0
step se 1 obj\j009_000.bmp 442 317 2 5 1
check mute_se obj\j021_000.bmp 452 282 1 5 0
pic obj\j020_000.bmp 506 282 1 5 0 0
slider hse obj\j008_000.bmp 128 393
number hse obj\j010_000.bmp 388 364
step hse -1 obj\j009_000.bmp 92 395 2 5 0
step hse 1 obj\j009_000.bmp 442 395 2 5 1
check mute_hse obj\j021_000.bmp 452 360 1 5 0
pic obj\j020_000.bmp 506 360 1 5 0 0
slider voice obj\j008_000.bmp 128 472
number voice obj\j010_000.bmp 388 443
step voice -1 obj\j009_000.bmp 92 474 2 5 0
step voice 1 obj\j009_000.bmp 442 474 2 5 1
check mute_voice obj\j021_000.bmp 452 439 1 5 0
pic obj\j020_000.bmp 506 439 1 5 0 0
slider backvoice obj\j008_000.bmp 128 550
number backvoice obj\j010_000.bmp 388 521
step backvoice -1 obj\j009_000.bmp 92 552 2 5 0
step backvoice 1 obj\j009_000.bmp 442 552 2 5 1
check mute_backvoice obj\j021_000.bmp 452 517 1 5 0
pic obj\j020_000.bmp 506 517 1 5 0 0
radio voice_stop 1 obj\j007_000.bmp 590 143 2 5 0
radio voice_stop 0 obj\j007_000.bmp 671 143 2 5 1
check voice0 obj\j019_000.bmp 676 519 7 5 0
check voice1 obj\j019_000.bmp 838 519 7 5 1
check voice2 obj\j019_000.bmp 951 568 7 5 2
check voice3 obj\j019_000.bmp 564 568 7 5 3
check voice4 obj\j019_000.bmp 703 619 7 5 4
check voice5 obj\j019_000.bmp 827 619 7 5 5
check voice6 obj\j019_000.bmp 951 619 7 5 6

screen cg
selectedlook
pic obj\f010_000.bmp 0 0
pic obj\f004_001.bmp 0 0 alpha 80 tab 0
pic obj\f004_000.bmp 75 0 tab 0
pic obj\f004_003.bmp 0 0 alpha 80 tab 1
pic obj\f004_002.bmp 75 0 tab 1
pic obj\f004_006.bmp 0 0 tab 2
pic obj\f004_007.bmp 0 0 tab 3
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\f005_000.bmp 0 560 1 2 0 0
pic obj\f005_000.bmp 0 617 1 2 0 1
pic obj\f000_000.bmp -19 -3 1 4 0 0
cellsheet obj\f001_000.bmp 8
cellsize 202 154
cellthumb 26 40
cell 450 100
cell 666 100
cell 882 100
cell 450 268
cell 666 268
cell 882 268
cell 450 436
cell 666 436
cell 882 436
celllabel obj\e006_001.bmp 16 15
celldigits obj\e006_000.bmp 50 10 3 18
gallerytabs 0
button page0 obj\e001_000.bmp 534 626 11 5 0
button page1 obj\e001_000.bmp 590 626 11 5 1
button page2 obj\e001_000.bmp 646 626 11 5 2
button gtab0 obj\f005_001.bmp 124 564 4 5 0
button gtab1 obj\f005_001.bmp 230 564 4 5 1
button gtab2 obj\f005_001.bmp 83 621 4 5 2
button gtab3 obj\f005_001.bmp 189 621 4 5 3
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8

screen scene
selectedlook
pic obj\f010_000.bmp 0 0
pic obj\f004_001.bmp 0 0 alpha 80 tab 0
pic obj\f004_000.bmp 75 0 tab 0
pic obj\f004_003.bmp 0 0 alpha 80 tab 1
pic obj\f004_002.bmp 75 0 tab 1
pic obj\f004_006.bmp 0 0 tab 2
pic obj\f004_007.bmp 0 0 tab 3
pic obj\e002_000.bmp 0 0
pic obj\e004_000.bmp 1117 0
pic obj\f005_000.bmp 0 560 1 2 0 0
pic obj\f005_000.bmp 0 617 1 2 0 1
pic obj\f000_000.bmp -19 -3 1 4 0 3
cellsheet obj\f001_000.bmp 8
cellsize 202 154
cellthumb 26 40
cell 450 100
cell 666 100
cell 882 100
cell 450 268
cell 666 268
cell 882 268
cell 450 436
cell 666 436
cell 882 436
celllabel obj\e006_001.bmp 16 15
celldigits obj\e006_000.bmp 50 10 3 18
gallerytabs 0
button page0 obj\e001_000.bmp 534 626 11 5 0
button page1 obj\e001_000.bmp 590 626 11 5 1
button page2 obj\e001_000.bmp 646 626 11 5 2
button gtab0 obj\f005_001.bmp 124 564 4 5 0
button gtab1 obj\f005_001.bmp 230 564 4 5 1
button gtab2 obj\f005_001.bmp 83 621 4 5 2
button gtab3 obj\f005_001.bmp 189 621 4 5 3
button nav_save obj\e004_001.bmp 1102 34 9 5 0 help obj\e004_002.bmp 900 683 1 9 0 0
button nav_load obj\e004_001.bmp 1102 88 9 5 1 help obj\e004_002.bmp 900 683 1 9 0 1
button nav_cg obj\e004_001.bmp 1102 142 9 5 2 help obj\e004_002.bmp 900 683 1 9 0 2
button nav_scene obj\e004_001.bmp 1102 196 9 5 3 help obj\e004_002.bmp 900 683 1 9 0 3
button nav_music obj\e004_001.bmp 1102 250 9 5 4 help obj\e004_002.bmp 900 683 1 9 0 4
button nav_config obj\e004_001.bmp 1102 304 9 5 5 help obj\e004_002.bmp 900 683 1 9 0 5
button nav_title obj\e004_001.bmp 1102 358 9 5 6 help obj\e004_002.bmp 900 683 1 9 0 6
button nav_quit obj\e004_001.bmp 1102 412 9 5 7 help obj\e004_002.bmp 900 683 1 9 0 7
button back obj\e004_001.bmp 1102 617 9 5 8 help obj\e004_002.bmp 900 683 1 9 0 8
)";
