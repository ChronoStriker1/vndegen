// Funbag Fantasy 3 if's menus, a raw string that tables.cpp includes and menuTable() hands out.
const char kFunbagFantasy[] = R"(
screen title
opening 520 920 1010 70 33 65 87
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

# The staff roll its exe draws, measured under Wine: black, then the logo and the bars over 2 seconds from 2.96, the sky,
# the characters and the credits fading in from 7.03 as the logo fades out, each moving on its own, everything fading to
# black from 97 seconds, and Waffle's card from 100 until 107.7, with M19 playing. It marks GLB_99_ENDING (1050) seen,
# and once that's set a click goes straight on.
screen staffroll
length 107700
music bgm\M19.ogg
sets 1050
skipif 1050
pic obj\s000_000.bmp 0 0
pic obj\s000_004.bmp 0 115 move 2150 23.41 clamp -2464 115 fadein 7030 2000 fadeout 97000 2000
pic obj\s000_005.bmp 0 -1997 move 1600 -22.38 fadein 7030 2000 fadeout 97000 2000
pic obj\s000_006.bmp 574 720 move 7000 39.8 fadein 7030 2000 fadeout 97000 2000
pic obj\s000_003.bmp 288 148 fadein 2960 2000 fadeout 7030 2000
pic obj\s000_001.bmp 0 0 fadein 2960 2000
pic obj\s000_002.bmp 0 603 fadein 2960 2000
pic obj\s000_007.bmp 534 347 fadein 100000 2000
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
cellthumb 9 30
cellhide
cellcount obj\d008_000.bmp - 120 119 132 138 0 12
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
cell 104 513
cell 266 513
cell 428 513
cell 590 513
cell 752 513
celldigits obj\d005_000.bmp 8 11 3 7
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
viewer obj\n000_000.bmp 627 594
viewerbutton replay obj\n001_003.bmp 645 658 hide
viewerbutton prevcg obj\n001_004.bmp 717 658
viewerbutton prevvar obj\n001_005.bmp 781 658
viewerbutton nextvar obj\n001_006.bmp 947 658
viewerbutton nextcg obj\n001_007.bmp 1013 658
viewerbutton auto obj\n001_008.bmp 1081 658
viewerbutton return obj\n001_009.bmp 1151 658
viewerbutton hide obj\n001_010.bmp 1225 604
viewernumber index obj\n003_000.bmp 854 643 17 2 11
viewercell obj\n003_000.bmp 884 653 11 10
viewernumber total obj\n003_000.bmp 896 663 17 2 11
viewerauto 8000 7200

screen scene
pic obj\g001_000.bmp 0 0
cellcard obj\g002_000.bmp
cellempty obj\d003_000.bmp -2 -2
cellframe obj\d021_000.bmp 7 18
cellsize 159 135
cellthumb 9 30
cellhide
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
cell 104 513
cell 266 513
cell 428 513
cell 590 513
cell 752 513
celldigits obj\d005_000.bmp 8 11 3 7
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
# Three modes, one always on, as the PC's run: the track over again, the list round from the top (the default, after
# whose last track the PC turns back to the first page), or at random. A click on the lit one changes nothing.
button music_one obj\h006_000.bmp 482 590 3 5 0
button music_all obj\h006_000.bmp 513 590 3 5 1
button music_random obj\h006_000.bmp 544 590 3 5 2
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
button tab obj\c002_000.bmp 326 70 3 5 0 help obj\c002_001.bmp 0 663 1 3 0 0
button tab2 obj\c002_000.bmp 444 70 3 5 1 help obj\c002_001.bmp 0 663 1 3 0 1
button tab3 obj\c002_000.bmp 562 70 3 5 2 help obj\c002_001.bmp 0 663 1 3 0 2
button cfg_defaults obj\c001_000.bmp 680 11 3 5 0 help obj\c001_001.bmp 0 663 1 3 0 0
button cfg_undo obj\c001_000.bmp 785 38 3 5 1 help obj\c001_001.bmp 0 663 1 3 0 1
button cfg_apply obj\c001_000.bmp 890 65 3 5 2 help obj\c001_001.bmp 0 663 1 3 0 2
button nav_save obj\c010_020.bmp 1047 85 10 5 0
button nav_load obj\c010_020.bmp 1042 121 10 5 1
button nav_cg obj\c010_020.bmp 1037 157 10 5 2
button nav_scene obj\c010_020.bmp 1032 193 10 5 3
button nav_music obj\c010_020.bmp 1027 229 10 5 4
button nav_config obj\c010_020.bmp 1022 265 10 5 5
button nav_title obj\c010_020.bmp 1017 301 10 5 7
button nav_quit obj\c010_020.bmp 1012 337 10 5 8
button back obj\c010_020.bmp 978 629 10 5 9
radio screen 0 obj\c005_000.bmp 132 127 2 5 0
radio screen 1 obj\c005_000.bmp 239 127 2 5 1
pic obj\c005_001.bmp 132 164 6 5 0 4
pic obj\c005_001.bmp 239 164 6 5 1 4
pic obj\c005_001.bmp 346 164 6 5 2 4
pic obj\c005_001.bmp 132 192 6 5 3 4
pic obj\c005_001.bmp 239 192 6 5 4 4
pic obj\c005_001.bmp 346 192 6 5 5 4
radio effect 1 obj\c005_002.bmp 27 247 2 5 0
radio effect 0 obj\c005_002.bmp 97 247 2 5 1
radio wait 1 obj\c005_002.bmp 180 247 2 5 0
radio wait 0 obj\c005_002.bmp 250 247 2 5 1
radio waitskip 1 obj\c005_002.bmp 333 247 2 5 0
radio waitskip 0 obj\c005_002.bmp 403 247 2 5 1
radio camera 1 obj\c005_002.bmp 487 391 2 5 0
radio camera 0 obj\c005_002.bmp 557 391 2 5 1
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
check tutorial obj\c005_006.bmp 499 305 8 5 6
pic obj\c005_006.bmp 499 331 8 5 7 4
hover 21 124 471 153 obj\c004_000.bmp 0 663 8 0
hover 484 124 930 361 obj\c004_000.bmp 0 663 8 6
hover 21 160 471 219 obj\c004_000.bmp 0 663 8 1
hover 21 225 162 275 obj\c004_000.bmp 0 663 8 2
hover 177 225 319 275 obj\c004_000.bmp 0 663 8 3
hover 330 225 471 275 obj\c004_000.bmp 0 663 8 4
hover 21 281 471 361 obj\c004_000.bmp 0 663 8 5
hover 484 369 625 418 obj\c004_000.bmp 0 663 8 7

screen config2
pic obj\c000_001.bmp 0 0
pic obj\c003_000.bmp 24 397
pic obj\c003_001.bmp 24 568 alpha 60
pic obj\c003_002.bmp 24 568
button tab obj\c002_000.bmp 326 70 3 5 0 help obj\c002_001.bmp 0 663 1 3 0 0
button tab2 obj\c002_000.bmp 444 70 3 5 1 help obj\c002_001.bmp 0 663 1 3 0 1
button tab3 obj\c002_000.bmp 562 70 3 5 2 help obj\c002_001.bmp 0 663 1 3 0 2
button cfg_defaults obj\c001_000.bmp 680 11 3 5 0 help obj\c001_001.bmp 0 663 1 3 0 0
button cfg_undo obj\c001_000.bmp 785 38 3 5 1 help obj\c001_001.bmp 0 663 1 3 0 1
button cfg_apply obj\c001_000.bmp 890 65 3 5 2 help obj\c001_001.bmp 0 663 1 3 0 2
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
radio skipall 0 obj\c006_001.bmp 520 145 2 5 0
radio skipall 1 obj\c006_001.bmp 591 145 2 5 1
radio deckey 0 obj\c006_002.bmp 718 145 2 5 0
radio deckey 1 obj\c006_002.bmp 790 145 2 5 1
radio rightclick 0 obj\c006_003.bmp 601 182 5 5 0
radio rightclick 1 obj\c006_003.bmp 707 182 5 5 1
radio rightclick 2 obj\c006_003.bmp 813 182 5 5 2
radio rightclick 3 obj\c006_003.bmp 601 211 5 5 3
radio rightclick 4 obj\c006_003.bmp 707 211 5 5 4
radio pause_inactive 1 obj\c005_002.bmp 520 265 2 5 0
radio pause_inactive 0 obj\c005_002.bmp 591 265 2 5 1
pic obj\c005_002.bmp 718 265 2 5 0 4
pic obj\c005_002.bmp 790 265 2 5 1 4
# Wait time, ×0.70 to ×1.30 in steps of 0.01, stretches waits and fades; the bar is full at ×2.00.
range waittime 70 130
slider waittime obj\c005_003.bmp 119 313 200
number waittime obj\c005_005.bmp 343 281
step waittime -1 obj\c005_004.bmp 88 315 2 5 0
step waittime 1 obj\c005_004.bmp 392 315 2 5 1
hover 24 124 470 185 obj\c004_001.bmp 0 663 3 0
hover 24 190 470 260 obj\c004_001.bmp 0 663 3 1
hover 24 270 470 350 obj\c004_001.bmp 0 663 3 2
hover 491 123 681 172 obj\c004_002.bmp 0 666 5 0
hover 691 123 879 172 obj\c004_002.bmp 0 666 5 1
hover 491 181 934 238 obj\c004_002.bmp 0 666 5 2
hover 491 244 681 294 obj\c004_002.bmp 0 666 5 3
hover 691 244 879 294 obj\c004_002.bmp 0 666 5 4

screen config3
pic obj\c000_002.bmp 0 0
pic obj\c003_000.bmp 24 397
pic obj\c003_001.bmp 24 568 alpha 60
pic obj\c003_002.bmp 24 568
button tab obj\c002_000.bmp 326 70 3 5 0 help obj\c002_001.bmp 0 663 1 3 0 0
button tab2 obj\c002_000.bmp 444 70 3 5 1 help obj\c002_001.bmp 0 663 1 3 0 1
button tab3 obj\c002_000.bmp 562 70 3 5 2 help obj\c002_001.bmp 0 663 1 3 0 2
button cfg_defaults obj\c001_000.bmp 680 11 3 5 0 help obj\c001_001.bmp 0 663 1 3 0 0
button cfg_undo obj\c001_000.bmp 785 38 3 5 1 help obj\c001_001.bmp 0 663 1 3 0 1
button cfg_apply obj\c001_000.bmp 890 65 3 5 2 help obj\c001_001.bmp 0 663 1 3 0 2
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
hover 24 124 469 203 obj\c004_003.bmp 0 663 8 0
hover 482 124 928 203 obj\c004_003.bmp 0 663 8 3
hover 24 212 469 292 obj\c004_003.bmp 0 663 8 1
hover 482 212 928 292 obj\c004_003.bmp 0 663 8 4
hover 24 301 469 379 obj\c004_003.bmp 0 663 8 2
hover 482 301 928 379 obj\c004_003.bmp 0 663 8 5
hover 482 390 674 438 obj\c004_003.bmp 0 663 8 6
hover 692 390 882 438 obj\c004_003.bmp 0 663 8 7

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
# Measured under Wine, where the text falls back to another face; with Open Sans in place of MS Gothic the text's
# offset grows by the 3 pixels between their ascents, so its baseline stays where the PC drew it there.
rows obj\r000_000.bmp 77 69 165 4 140 50
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
# The box that stops the question, whichever answer follows, as on the PC.
button noask obj\m002_000.bmp 539 481 1 5 0
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
# The official English release asks for Lucida Sans Unicode (the one face its program names), here Open Sans, sized
# and spaced against the publisher's Windows screenshots: 16, nearest their x-height, with the gap that ends three
# lines within a few pixels of theirs; lines as far apart as there, the first glyph 3 in and the baseline 1 higher.
textfont 7
textsize 16
textgap 2.75
textline 23
textshift 3 -1
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
