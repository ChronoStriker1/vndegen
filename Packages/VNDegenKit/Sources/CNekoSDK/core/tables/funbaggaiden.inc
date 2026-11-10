const char kFunbagGaiden[] = R"(
screen title
menustages
startmenu 2
pic obj\d000_002.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\d000_004.bmp 461 672
pic obj\d000_005.bmp 159 0 stage 1
pic obj\d001_000.bmp 397 506 stage 1
pic obj\d000_003.bmp 240 17 stage 2
button start obj\d002_000.bmp 486 408 6 5 0 stage 2
button load obj\d002_000.bmp 486 448 6 5 1 stage 2
button submenu:extra obj\d002_000.bmp 486 488 6 5 2 stage 2
button config obj\d002_000.bmp 486 528 6 5 3 stage 2
button websurvey obj\d002_000.bmp 486 568 6 5 4 stage 2
button quit obj\d002_000.bmp 486 608 6 5 5 stage 2
button start_prologue obj\d002_001.bmp 810 408 2 5 0 stage 2 group start
button start_main obj\d002_001.bmp 810 448 2 5 1 stage 2 group start

screen start
flow main

screen load
pic obj\a103_000.bmp 0 0
pic obj\e003_000.bmp 1102 0
pic obj\l000_003.bmp 1110 106
pic obj\a002_001.bmp 0 0
pic obj\e000_001.bmp 26 25
emptycards 80
slots obj\e002_000.bmp 60 98 520 120 5 2 1
slotthumb 34 11
slotdigits obj\e002_001.bmp 297 6 3 16
slotnew obj\e002_003.bmp 376 -9
slotchaptervar obj\e002_004.bmp 260 62 4 5000
datedigits obj\e002_001.bmp 0 0
datefield year 268 26 16
datefield month 352 26 16 blank
datefield day 400 26 16 blank
datefield hour 268 46 16 blank
datefield minute 316 46 16 blank
button page0 obj\e001_000.bmp 399 37 10 5 0
button page1 obj\e001_000.bmp 459 37 10 5 1
button page2 obj\e001_000.bmp 519 37 10 5 2
button page3 obj\e001_000.bmp 579 37 10 5 3
button page4 obj\e001_000.bmp 639 37 10 5 4
button page5 obj\e001_000.bmp 699 37 10 5 5
button page6 obj\e001_000.bmp 759 37 10 5 6
button page7 obj\e001_000.bmp 819 37 10 5 7
button page8 obj\e001_000.bmp 879 37 10 5 8
button page9 obj\e001_000.bmp 939 37 10 5 9
button back obj\e003_001.bmp 1116 44 1 5 0
button nav_save obj\e003_002.bmp 1116 280 8 5 0
button nav_load obj\e003_002.bmp 1116 330 8 5 1
button nav_cg obj\e003_002.bmp 1116 380 8 5 2
button nav_scene obj\e003_002.bmp 1116 430 8 5 3
button nav_music obj\e003_002.bmp 1116 480 8 5 4
button nav_config obj\e003_002.bmp 1116 530 8 5 5
button nav_title obj\e003_002.bmp 1116 580 8 5 6
button nav_quit obj\e003_002.bmp 1116 630 8 5 7

screen save
pic obj\a002_000.bmp 0 0
pic obj\e003_000.bmp 1102 0
pic obj\l000_003.bmp 1110 106
pic obj\a002_001.bmp 0 0
pic obj\e000_000.bmp 26 25
slots obj\e002_000.bmp 60 98 520 120 5 2 1
slotthumb 34 11
slotdigits obj\e002_001.bmp 297 6 3 16
slotnew obj\e002_003.bmp 376 -9
slotchaptervar obj\e002_004.bmp 260 62 4 5000
datedigits obj\e002_001.bmp 0 0
datefield year 268 26 16
datefield month 352 26 16 blank
datefield day 400 26 16 blank
datefield hour 268 46 16 blank
datefield minute 316 46 16 blank
button page0 obj\e001_000.bmp 399 37 10 5 0
button page1 obj\e001_000.bmp 459 37 10 5 1
button page2 obj\e001_000.bmp 519 37 10 5 2
button page3 obj\e001_000.bmp 579 37 10 5 3
button page4 obj\e001_000.bmp 639 37 10 5 4
button page5 obj\e001_000.bmp 699 37 10 5 5
button page6 obj\e001_000.bmp 759 37 10 5 6
button page7 obj\e001_000.bmp 819 37 10 5 7
button page8 obj\e001_000.bmp 879 37 10 5 8
button page9 obj\e001_000.bmp 939 37 10 5 9
button back obj\e003_001.bmp 1116 44 1 5 0
button nav_save obj\e003_002.bmp 1116 280 8 5 0
button nav_load obj\e003_002.bmp 1116 330 8 5 1
button nav_cg obj\e003_002.bmp 1116 380 8 5 2
button nav_scene obj\e003_002.bmp 1116 430 8 5 3
button nav_music obj\e003_002.bmp 1116 480 8 5 4
button nav_config obj\e003_002.bmp 1116 530 8 5 5
button nav_title obj\e003_002.bmp 1116 580 8 5 6
button nav_quit obj\e003_002.bmp 1116 630 8 5 7

screen dialog
dim 155
pic obj\a003_000.bmp 311 75
question obj\h000_009.bmp 402 78 6 0
question obj\h000_009.bmp 402 78 6 1
question obj\h000_009.bmp 402 78 6 2
question obj\h000_009.bmp 402 78 6 2
question obj\h000_009.bmp 402 78 6 0
question obj\h000_009.bmp 402 78 6 1
question obj\h000_009.bmp 402 78 6 4
question obj\h000_009.bmp 402 78 6 3
question obj\h000_009.bmp 402 78 6 5
button yes obj\a003_002.bmp 361 188 2 5 0
button no obj\a003_002.bmp 361 299 2 5 1

screen choice
pic obj\a003_000.bmp 311 75
pic obj\h000_008.bmp 411 92
choicecolumns 2
placefor 2 187 122 361 0

screen config
pic obj\a002_000.bmp 0 0
pic obj\e003_000.bmp 1102 0
pic obj\l000_003.bmp 1110 106
pic obj\a002_001.bmp 0 0
pic obj\h000_000.bmp 26 25
pic obj\h001_000.bmp 22 94
pic obj\h002_000.bmp 22 459
pic obj\h003_000.bmp 558 94
pic obj\h004_000.bmp 558 266
button cfg_defaults obj\h005_000.bmp 490 44 3 5 0
button cfg_undo obj\h005_000.bmp 640 44 3 5 1
button cfg_apply obj\h005_000.bmp 790 44 3 5 2
button back obj\e003_001.bmp 1116 44 1 5 0
button nav_save obj\e003_002.bmp 1116 280 8 5 0
button nav_load obj\e003_002.bmp 1116 330 8 5 1
button nav_cg obj\e003_002.bmp 1116 380 8 5 2
button nav_scene obj\e003_002.bmp 1116 430 8 5 3
button nav_music obj\e003_002.bmp 1116 480 8 5 4
button nav_config obj\e003_002.bmp 1116 530 8 5 5
button nav_title obj\e003_002.bmp 1116 580 8 5 6
button nav_quit obj\e003_002.bmp 1116 630 8 5 7
radio screen 0 obj\h001_001.bmp 257 127 12 5 0
radio screen 1 obj\h001_001.bmp 376 127 12 5 1
pic obj\h001_001.bmp 257 155 12 5 2 4
pic obj\h001_001.bmp 376 155 12 5 3 4
pic obj\h001_001.bmp 211 183 12 5 4 4
pic obj\h001_001.bmp 317 183 12 5 5 4
pic obj\h001_001.bmp 423 183 12 5 6 4
pic obj\h001_001.bmp 211 211 12 5 7 4
pic obj\h001_001.bmp 317 211 12 5 8 4
pic obj\h001_001.bmp 423 211 12 5 9 4
radio effect 1 obj\h001_001.bmp 257 239 12 5 10
radio effect 0 obj\h001_001.bmp 376 239 12 5 11
radio wait 1 obj\h001_001.bmp 257 267 12 5 10
radio wait 0 obj\h001_001.bmp 376 267 12 5 11
slider opacity obj\h001_004.bmp 89 329
pic obj\h001_003.bmp 85 325
knob opacity obj\h001_002.bmp 77 321 4
step opacity -1 obj\h001_005.bmp 60 320 2 5 0
step opacity 1 obj\h001_005.bmp 500 320 2 5 1
number opacity obj\h001_006.bmp 417 299 18 blank
radio confirm_saveload 1 obj\h001_001.bmp 257 351 12 5 10
radio confirm_saveload 0 obj\h001_001.bmp 376 351 12 5 11
group confirm_saveload confirm_overwrite confirm_delete confirm_quick confirm_title confirm_exit
radio tutorial 1 obj\h001_001.bmp 257 379 12 5 10
radio tutorial 0 obj\h001_001.bmp 376 379 12 5 11
pic obj\a001_003.bmp 112 407 1 5 0 4
slider text obj\h001_004.bmp 89 526
pic obj\h001_003.bmp 85 522
knob text obj\h001_002.bmp 77 518 4
step text -1 obj\h001_005.bmp 60 517 2 5 0
step text 1 obj\h001_005.bmp 500 517 2 5 1
number text obj\h001_006.bmp 417 496 18 blank
slider auto obj\h001_004.bmp 89 582
pic obj\h001_003.bmp 85 578
knob auto obj\h001_002.bmp 77 574 4
step auto -1 obj\h001_005.bmp 60 573 2 5 0
step auto 1 obj\h001_005.bmp 500 573 2 5 1
number auto obj\h001_006.bmp 417 552 18 blank
range waittime 50 150
slider waittime obj\h001_004.bmp 89 638
pic obj\h001_003.bmp 85 634
knob waittime obj\h001_002.bmp -123 630 4
step waittime -1 obj\h001_005.bmp 60 629 2 5 0
step waittime 1 obj\h001_005.bmp 500 629 2 5 1
number waittime obj\h001_006.bmp 417 608 435 453
radio skipall 0 obj\h003_001.bmp 793 127 9 5 0
radio skipall 1 obj\h003_001.bmp 912 127 9 5 1
radio rightclick 0 obj\h003_001.bmp 743 155 9 5 2
radio rightclick 1 obj\h003_001.bmp 853 155 9 5 3
radio rightclick 2 obj\h003_001.bmp 959 155 9 5 4
radio rightclick 3 obj\h003_001.bmp 793 183 9 5 5
radio rightclick 4 obj\h003_001.bmp 912 183 9 5 6
radio deckey 0 obj\h003_001.bmp 793 211 9 5 7
radio deckey 1 obj\h003_001.bmp 912 211 9 5 8
slider bgm obj\h001_004.bmp 625 329
pic obj\h001_003.bmp 621 325
knob bgm obj\h001_002.bmp 613 321 4
step bgm -1 obj\h001_005.bmp 596 320 2 5 0
step bgm 1 obj\h001_005.bmp 1036 320 2 5 1
number bgm obj\h001_006.bmp 953 299 18 blank
slider se obj\h001_004.bmp 625 385
pic obj\h001_003.bmp 621 381
knob se obj\h001_002.bmp 613 377 4
step se -1 obj\h001_005.bmp 596 376 2 5 0
step se 1 obj\h001_005.bmp 1036 376 2 5 1
number se obj\h001_006.bmp 953 355 18 blank
slider voice obj\h001_004.bmp 625 441
pic obj\h001_003.bmp 621 437
knob voice obj\h001_002.bmp 613 433 4
step voice -1 obj\h001_005.bmp 596 432 2 5 0
step voice 1 obj\h001_005.bmp 1036 432 2 5 1
number voice obj\h001_006.bmp 953 411 18 blank
slider backvoice obj\h001_004.bmp 625 497
pic obj\h001_003.bmp 621 493
knob backvoice obj\h001_002.bmp 613 489 4
step backvoice -1 obj\h001_005.bmp 596 488 2 5 0
step backvoice 1 obj\h001_005.bmp 1036 488 2 5 1
number backvoice obj\h001_006.bmp 953 467 18 blank
radio mute_backvoice 0 obj\h001_001.bmp 793 519 12 5 10
radio mute_backvoice 1 obj\h001_001.bmp 912 519 12 5 11
radio voice_stop 1 obj\h001_001.bmp 793 547 12 5 10
radio voice_stop 0 obj\h001_001.bmp 912 547 12 5 11
pic obj\h004_001.bmp 893 298 1 5 0 4
pic obj\h004_001.bmp 893 354 1 5 0 0
pic obj\h004_001.bmp 893 410 1 5 0 0
pic obj\h004_001.bmp 893 466 1 5 0 0
pic obj\h004_002.bmp 747 575 6 5 2 0
pic obj\h004_002.bmp 853 575 6 5 3 0
pic obj\h004_002.bmp 959 575 6 5 4 0
pic obj\h004_002.bmp 747 603 6 5 5 0

screen defaults
parts scroll particles
colorkey 0 255 0
textfont 9
textsize 22
textline 32
textshift 0 -1
twobyteadvance
inneralways
default opacity 100
default text 50
default auto 50
default bgm 80
default se 60
default voice 80
default backvoice 80
default skipall 1
windowpic obj\l000_003.bmp 1063 489

screen cfgsav
size 96000
tracks 92000
nostring

screen staffroll_a
length 133900
music bgm\BGM29.ogg
pic bg\bg_b.bmp 0 0
pic obj\n004_000.bmp 627 0 fadein 16300 3000 fadeout 30500 3000
pic obj\n004_001.bmp 627 0 fadein 30500 3000 fadeout 43950 3000
pic obj\n004_002.bmp 627 0 fadein 43950 3000 fadeout 57400 3000
pic obj\n004_003.bmp 627 0 fadein 57400 3000 fadeout 70850 3000
pic obj\n004_004.bmp 627 0 fadein 70850 3000 fadeout 84300 3000
pic obj\n004_005.bmp 627 0 fadein 84300 3000 fadeout 97750 3000
pic obj\n004_006.bmp 627 0 fadein 97750 3000 fadeout 111200 3000
pic obj\n004_007.bmp 627 0 fadein 111200 3000
pic obj\n000_000.bmp 0 0 fadein 16300 3000
pic obj\d000_003.bmp 254 110 alpha 72 fadein 4200 800 fadeout 8000 2600
pic obj\d000_003.bmp 254 110 fadein 8000 2600 fadeout 15200 2000
pic obj\n001_041.bmp 133 254 1 1 0 0 fadein 20500 700 fadeout 24400 600
pic obj\n001_042.bmp 133 164 1 8 0 0 fadein 27800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 209 1 8 0 1 fadein 28600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 254 1 8 0 2 fadein 30100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 299 1 8 0 3 fadein 30800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 344 1 8 0 4 fadein 31600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 389 1 8 0 5 fadein 33100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 434 1 8 0 6 fadein 33800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 479 1 8 0 7 fadein 35300 700 fadeout 38600 600
pic obj\n001_043.bmp 133 164 1 5 0 0 fadein 42300 700 fadeout 50500 600
pic obj\n001_043.bmp 133 209 1 5 0 1 fadein 43900 700 fadeout 50500 600
pic obj\n001_043.bmp 133 254 1 5 0 2 fadein 44700 700 fadeout 50500 600
pic obj\n001_043.bmp 133 299 1 5 0 3 fadein 45500 700 fadeout 50500 600
pic obj\n001_043.bmp 133 344 1 5 0 4 fadein 47100 700 fadeout 50500 600
pic obj\n001_044.bmp 133 164 1 4 0 0 fadein 54300 700 fadeout 60900 600
pic obj\n001_044.bmp 133 209 1 4 0 1 fadein 55100 700 fadeout 60900 600
pic obj\n001_044.bmp 133 254 1 4 0 2 fadein 56700 700 fadeout 60900 600
pic obj\n001_044.bmp 133 299 1 4 0 3 fadein 57500 700 fadeout 60900 600
pic obj\n001_045.bmp 133 164 1 5 0 0 fadein 64600 700 fadeout 72100 600
pic obj\n001_045.bmp 133 254 1 5 0 1 fadein 65400 700 fadeout 72100 600
pic obj\n001_045.bmp 133 299 1 5 0 2 fadein 66200 700 fadeout 72100 600
pic obj\n001_045.bmp 133 344 1 5 0 3 fadein 67800 700 fadeout 72100 600
pic obj\n001_045.bmp 133 434 1 5 0 4 fadein 68600 700 fadeout 72100 600
pic obj\n001_046.bmp 133 164 1 6 0 0 fadein 76000 700 fadeout 83000 600
pic obj\n001_046.bmp 133 254 1 6 0 1 fadein 76900 700 fadeout 83000 600
pic obj\n001_046.bmp 133 344 1 6 0 2 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 389 1 6 0 3 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 434 1 6 0 4 fadein 79300 700 fadeout 83000 600
pic obj\n001_046.bmp 133 479 1 6 0 5 fadein 79300 700 fadeout 83000 600
pic obj\n001_047.bmp 133 164 1 7 0 0 fadein 86000 700 fadeout 92800 600
pic obj\n001_047.bmp 133 209 1 7 0 1 fadein 86800 700 fadeout 92800 600
pic obj\n001_047.bmp 133 254 1 7 0 2 fadein 87600 700 fadeout 92800 600
pic obj\n001_047.bmp 133 299 1 7 0 3 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 344 1 7 0 4 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 389 1 7 0 5 fadein 89300 700 fadeout 92800 600
pic obj\n001_047.bmp 133 434 1 7 0 6 fadein 89300 700 fadeout 92800 600
pic obj\n001_048.bmp 133 164 1 5 0 0 fadein 96600 700 fadeout 104000 600
pic obj\n001_048.bmp 133 254 1 5 0 1 fadein 98200 700 fadeout 104000 600
pic obj\n001_048.bmp 133 344 1 5 0 2 fadein 99000 700 fadeout 104000 600
pic obj\n001_048.bmp 133 389 1 5 0 3 fadein 99800 700 fadeout 104000 600
pic obj\n001_048.bmp 133 434 1 5 0 4 fadein 100600 700 fadeout 104000 600
pic obj\n001_049.bmp 133 164 1 4 0 0 fadein 108800 700 fadeout 113900 600
pic obj\n001_049.bmp 133 254 1 4 0 1 fadein 109600 700 fadeout 113900 600
pic obj\n001_049.bmp 133 344 1 4 0 2 fadein 110400 700 fadeout 113900 600
pic obj\n001_049.bmp 133 434 1 4 0 3 fadein 111300 700 fadeout 113900 600
pic obj\n001_050.bmp 133 164 1 3 0 0 fadein 117900 700 fadeout 123900 600
pic obj\n001_050.bmp 133 254 1 3 0 1 fadein 118700 700 fadeout 123900 600
pic obj\n001_050.bmp 133 344 1 3 0 2 fadein 120300 700 fadeout 123900 600
pic obj\n001_051.bmp 133 164 1 2 0 0 fadein 126200 700 fadeout 129800 600
pic obj\n001_051.bmp 133 254 1 2 0 1 fadein 126200 700 fadeout 129800 600
pic bg\bg_w.bmp 0 0 fadein 132300 1600

screen staffroll_b
length 133900
music bgm\BGM29.ogg
pic bg\bg_b.bmp 0 0
pic obj\n003_000.bmp 627 0 fadein 16300 3000 fadeout 30500 3000
pic obj\n003_001.bmp 627 0 fadein 30500 3000 fadeout 43950 3000
pic obj\n003_002.bmp 627 0 fadein 43950 3000 fadeout 57400 3000
pic obj\n003_003.bmp 627 0 fadein 57400 3000 fadeout 70850 3000
pic obj\n003_004.bmp 627 0 fadein 70850 3000 fadeout 84300 3000
pic obj\n003_005.bmp 627 0 fadein 84300 3000 fadeout 97750 3000
pic obj\n003_006.bmp 627 0 fadein 97750 3000 fadeout 111200 3000
pic obj\n003_007.bmp 627 0 fadein 111200 3000
pic obj\n000_000.bmp 0 0 fadein 16300 3000
pic obj\d000_003.bmp 254 110 alpha 72 fadein 4200 800 fadeout 8000 2600
pic obj\d000_003.bmp 254 110 fadein 8000 2600 fadeout 15200 2000
pic obj\n001_041.bmp 133 254 1 1 0 0 fadein 20500 700 fadeout 24400 600
pic obj\n001_042.bmp 133 164 1 8 0 0 fadein 27800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 209 1 8 0 1 fadein 28600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 254 1 8 0 2 fadein 30100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 299 1 8 0 3 fadein 30800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 344 1 8 0 4 fadein 31600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 389 1 8 0 5 fadein 33100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 434 1 8 0 6 fadein 33800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 479 1 8 0 7 fadein 35300 700 fadeout 38600 600
pic obj\n001_043.bmp 133 164 1 5 0 0 fadein 42300 700 fadeout 50500 600
pic obj\n001_043.bmp 133 209 1 5 0 1 fadein 43900 700 fadeout 50500 600
pic obj\n001_043.bmp 133 254 1 5 0 2 fadein 44700 700 fadeout 50500 600
pic obj\n001_043.bmp 133 299 1 5 0 3 fadein 45500 700 fadeout 50500 600
pic obj\n001_043.bmp 133 344 1 5 0 4 fadein 47100 700 fadeout 50500 600
pic obj\n001_044.bmp 133 164 1 4 0 0 fadein 54300 700 fadeout 60900 600
pic obj\n001_044.bmp 133 209 1 4 0 1 fadein 55100 700 fadeout 60900 600
pic obj\n001_044.bmp 133 254 1 4 0 2 fadein 56700 700 fadeout 60900 600
pic obj\n001_044.bmp 133 299 1 4 0 3 fadein 57500 700 fadeout 60900 600
pic obj\n001_045.bmp 133 164 1 5 0 0 fadein 64600 700 fadeout 72100 600
pic obj\n001_045.bmp 133 254 1 5 0 1 fadein 65400 700 fadeout 72100 600
pic obj\n001_045.bmp 133 299 1 5 0 2 fadein 66200 700 fadeout 72100 600
pic obj\n001_045.bmp 133 344 1 5 0 3 fadein 67800 700 fadeout 72100 600
pic obj\n001_045.bmp 133 434 1 5 0 4 fadein 68600 700 fadeout 72100 600
pic obj\n001_046.bmp 133 164 1 6 0 0 fadein 76000 700 fadeout 83000 600
pic obj\n001_046.bmp 133 254 1 6 0 1 fadein 76900 700 fadeout 83000 600
pic obj\n001_046.bmp 133 344 1 6 0 2 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 389 1 6 0 3 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 434 1 6 0 4 fadein 79300 700 fadeout 83000 600
pic obj\n001_046.bmp 133 479 1 6 0 5 fadein 79300 700 fadeout 83000 600
pic obj\n001_047.bmp 133 164 1 7 0 0 fadein 86000 700 fadeout 92800 600
pic obj\n001_047.bmp 133 209 1 7 0 1 fadein 86800 700 fadeout 92800 600
pic obj\n001_047.bmp 133 254 1 7 0 2 fadein 87600 700 fadeout 92800 600
pic obj\n001_047.bmp 133 299 1 7 0 3 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 344 1 7 0 4 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 389 1 7 0 5 fadein 89300 700 fadeout 92800 600
pic obj\n001_047.bmp 133 434 1 7 0 6 fadein 89300 700 fadeout 92800 600
pic obj\n001_048.bmp 133 164 1 5 0 0 fadein 96600 700 fadeout 104000 600
pic obj\n001_048.bmp 133 254 1 5 0 1 fadein 98200 700 fadeout 104000 600
pic obj\n001_048.bmp 133 344 1 5 0 2 fadein 99000 700 fadeout 104000 600
pic obj\n001_048.bmp 133 389 1 5 0 3 fadein 99800 700 fadeout 104000 600
pic obj\n001_048.bmp 133 434 1 5 0 4 fadein 100600 700 fadeout 104000 600
pic obj\n001_049.bmp 133 164 1 4 0 0 fadein 108800 700 fadeout 113900 600
pic obj\n001_049.bmp 133 254 1 4 0 1 fadein 109600 700 fadeout 113900 600
pic obj\n001_049.bmp 133 344 1 4 0 2 fadein 110400 700 fadeout 113900 600
pic obj\n001_049.bmp 133 434 1 4 0 3 fadein 111300 700 fadeout 113900 600
pic obj\n001_050.bmp 133 164 1 3 0 0 fadein 117900 700 fadeout 123900 600
pic obj\n001_050.bmp 133 254 1 3 0 1 fadein 118700 700 fadeout 123900 600
pic obj\n001_050.bmp 133 344 1 3 0 2 fadein 120300 700 fadeout 123900 600
pic obj\n001_051.bmp 133 164 1 2 0 0 fadein 126200 700 fadeout 129800 600
pic obj\n001_051.bmp 133 254 1 2 0 1 fadein 126200 700 fadeout 129800 600
pic bg\bg_w.bmp 0 0 fadein 132300 1600

screen staffroll_c
length 133900
music bgm\BGM29.ogg
pic bg\bg_b.bmp 0 0
pic obj\n002_000.bmp 627 0 fadein 16300 3000 fadeout 30500 3000
pic obj\n002_001.bmp 627 0 fadein 30500 3000 fadeout 43950 3000
pic obj\n002_002.bmp 627 0 fadein 43950 3000 fadeout 57400 3000
pic obj\n002_003.bmp 627 0 fadein 57400 3000 fadeout 70850 3000
pic obj\n002_004.bmp 627 0 fadein 70850 3000 fadeout 84300 3000
pic obj\n002_005.bmp 627 0 fadein 84300 3000 fadeout 97750 3000
pic obj\n002_006.bmp 627 0 fadein 97750 3000 fadeout 111200 3000
pic obj\n002_007.bmp 627 0 fadein 111200 3000
pic obj\n000_000.bmp 0 0 fadein 16300 3000
pic obj\d000_003.bmp 254 110 alpha 72 fadein 4200 800 fadeout 8000 2600
pic obj\d000_003.bmp 254 110 fadein 8000 2600 fadeout 15200 2000
pic obj\n001_041.bmp 133 254 1 1 0 0 fadein 20500 700 fadeout 24400 600
pic obj\n001_042.bmp 133 164 1 8 0 0 fadein 27800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 209 1 8 0 1 fadein 28600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 254 1 8 0 2 fadein 30100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 299 1 8 0 3 fadein 30800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 344 1 8 0 4 fadein 31600 700 fadeout 38600 600
pic obj\n001_042.bmp 133 389 1 8 0 5 fadein 33100 700 fadeout 38600 600
pic obj\n001_042.bmp 133 434 1 8 0 6 fadein 33800 700 fadeout 38600 600
pic obj\n001_042.bmp 133 479 1 8 0 7 fadein 35300 700 fadeout 38600 600
pic obj\n001_043.bmp 133 164 1 5 0 0 fadein 42300 700 fadeout 50500 600
pic obj\n001_043.bmp 133 209 1 5 0 1 fadein 43900 700 fadeout 50500 600
pic obj\n001_043.bmp 133 254 1 5 0 2 fadein 44700 700 fadeout 50500 600
pic obj\n001_043.bmp 133 299 1 5 0 3 fadein 45500 700 fadeout 50500 600
pic obj\n001_043.bmp 133 344 1 5 0 4 fadein 47100 700 fadeout 50500 600
pic obj\n001_044.bmp 133 164 1 4 0 0 fadein 54300 700 fadeout 60900 600
pic obj\n001_044.bmp 133 209 1 4 0 1 fadein 55100 700 fadeout 60900 600
pic obj\n001_044.bmp 133 254 1 4 0 2 fadein 56700 700 fadeout 60900 600
pic obj\n001_044.bmp 133 299 1 4 0 3 fadein 57500 700 fadeout 60900 600
pic obj\n001_045.bmp 133 164 1 5 0 0 fadein 64600 700 fadeout 72100 600
pic obj\n001_045.bmp 133 254 1 5 0 1 fadein 65400 700 fadeout 72100 600
pic obj\n001_045.bmp 133 299 1 5 0 2 fadein 66200 700 fadeout 72100 600
pic obj\n001_045.bmp 133 344 1 5 0 3 fadein 67800 700 fadeout 72100 600
pic obj\n001_045.bmp 133 434 1 5 0 4 fadein 68600 700 fadeout 72100 600
pic obj\n001_046.bmp 133 164 1 6 0 0 fadein 76000 700 fadeout 83000 600
pic obj\n001_046.bmp 133 254 1 6 0 1 fadein 76900 700 fadeout 83000 600
pic obj\n001_046.bmp 133 344 1 6 0 2 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 389 1 6 0 3 fadein 78500 700 fadeout 83000 600
pic obj\n001_046.bmp 133 434 1 6 0 4 fadein 79300 700 fadeout 83000 600
pic obj\n001_046.bmp 133 479 1 6 0 5 fadein 79300 700 fadeout 83000 600
pic obj\n001_047.bmp 133 164 1 7 0 0 fadein 86000 700 fadeout 92800 600
pic obj\n001_047.bmp 133 209 1 7 0 1 fadein 86800 700 fadeout 92800 600
pic obj\n001_047.bmp 133 254 1 7 0 2 fadein 87600 700 fadeout 92800 600
pic obj\n001_047.bmp 133 299 1 7 0 3 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 344 1 7 0 4 fadein 88500 700 fadeout 92800 600
pic obj\n001_047.bmp 133 389 1 7 0 5 fadein 89300 700 fadeout 92800 600
pic obj\n001_047.bmp 133 434 1 7 0 6 fadein 89300 700 fadeout 92800 600
pic obj\n001_048.bmp 133 164 1 5 0 0 fadein 96600 700 fadeout 104000 600
pic obj\n001_048.bmp 133 254 1 5 0 1 fadein 98200 700 fadeout 104000 600
pic obj\n001_048.bmp 133 344 1 5 0 2 fadein 99000 700 fadeout 104000 600
pic obj\n001_048.bmp 133 389 1 5 0 3 fadein 99800 700 fadeout 104000 600
pic obj\n001_048.bmp 133 434 1 5 0 4 fadein 100600 700 fadeout 104000 600
pic obj\n001_049.bmp 133 164 1 4 0 0 fadein 108800 700 fadeout 113900 600
pic obj\n001_049.bmp 133 254 1 4 0 1 fadein 109600 700 fadeout 113900 600
pic obj\n001_049.bmp 133 344 1 4 0 2 fadein 110400 700 fadeout 113900 600
pic obj\n001_049.bmp 133 434 1 4 0 3 fadein 111300 700 fadeout 113900 600
pic obj\n001_050.bmp 133 164 1 3 0 0 fadein 117900 700 fadeout 123900 600
pic obj\n001_050.bmp 133 254 1 3 0 1 fadein 118700 700 fadeout 123900 600
pic obj\n001_050.bmp 133 344 1 3 0 2 fadein 120300 700 fadeout 123900 600
pic obj\n001_051.bmp 133 164 1 2 0 0 fadein 126200 700 fadeout 129800 600
pic obj\n001_051.bmp 133 254 1 2 0 1 fadein 126200 700 fadeout 129800 600
pic bg\bg_w.bmp 0 0 fadein 132300 1600

screen story
flow prologue
scene scr\se_00_00_00.txt @1
scene scr\se_00_00_01.txt @2
scene scr\se_00_00_02.txt @3
scene scr\se_00_00_03.txt @4
scene scr\se_00_00_04.txt @5
set G 2 = 1
jump main
flow main
set S 5000 = 1
scene scr\se_00_01_00.txt @6
scene scr\se_00_01_01.txt @7
scene scr\se_00_01_02.txt @8
scene scr\se_00_01_03.txt @9
scene scr\se_00_01_04.txt @10
scene scr\se_00_01_06.txt @400
scene scr\se_00_01_06b.txt @11
scene scr\se_00_01_07.txt @12
scene scr\se_00_01_08.txt @13
scene scr\se_00_01_09.txt @14
scene scr\se_00_01_11.txt @15
scene scr\se_00_01_12.txt @16
scene scr\se_00_01_13.txt @17
scene scr\se_00_02_00.txt @18
scene scr\se_00_02_01.txt @19
scene scr\se_00_02_02.txt @20
scene scr\se_00_02_03.txt @21
scene scr\se_00_02_04.txt @22
scene scr\se_00_02_05.txt @23
scene scr\se_00_02_07.txt @301
scene scr\se_00_02_07b.txt @24
scene scr\se_00_02_08.txt @25
scene scr\se_00_02_09.txt @26
scene scr\se_00_03_00.txt @27
scene scr\se_00_03_01.txt @28
scene scr\se_00_03_02.txt @29
scene scr\se_00_03_03.txt @30
scene scr\se_00_03_04.txt @31
scene scr\se_00_03_05.txt @32
scene scr\se_00_03_06.txt @33
scene scr\se_00_03_07.txt @34
set S 5000 = 2
scene scr\se_01_00_00.txt @35
scene scr\se_01_00_01.txt @36
scene scr\se_01_00_02.txt @37
scene scr\se_01_00_04.txt @38
scene scr\se_01_00_05.txt @39
scene scr\se_01_01_00.txt @40
scene scr\se_01_01_01.txt @41
scene scr\se_01_01_02.txt @42
scene scr\se_01_01_03.txt @43
scene scr\se_01_01_04.txt @44
scene scr\se_01_01_05.txt @45
scene scr\se_01_01_06.txt @46
scene scr\se_01_01_07.txt @47
scene scr\se_01_02_00.txt @48
scene scr\se_01_02_01.txt @49
scene scr\se_01_02_02.txt @50
scene scr\se_01_02_03.txt @51
scene scr\se_01_02_04.txt @52
scene scr\se_01_02_05.txt @53
scene scr\se_01_03_00.txt @54
scene scr\se_01_03_01.txt @55
scene scr\se_01_03_02.txt @56
scene scr\se_01_03_03.txt @57
scene scr\se_01_03_04.txt @58
scene scr\se_01_03_06.txt @59
scene scr\se_01_03_07.txt @60
scene scr\se_01_03_08.txt @61
scene scr\se_01_04_00.txt @62
scene scr\se_01_04_01.txt @63
scene scr\se_01_04_02.txt @64
scene scr\se_01_04_03.txt @65
choice obj\p001_000.bmp 2 @66
on 1 goto leave
scene scr\se_01_04_04.txt @70
set S 5000 = 3
scene scr\se_02_00_00.txt @71
scene scr\se_02_00_01.txt @72
scene scr\se_02_00_02.txt @73
scene scr\se_02_00_03.txt @74
scene scr\se_02_00_04.txt @75
scene scr\se_02_00_05.txt @76
scene scr\se_02_00_06.txt @77
scene scr\se_02_00_07.txt @78
scene scr\se_02_01_00.txt @79
scene scr\se_02_01_01.txt @80
scene scr\se_02_01_02.txt @81
scene scr\se_02_01_03.txt @82
scene scr\se_02_01_05.txt @83
scene scr\se_02_01_07.txt @84
scene scr\se_02_01_08.txt @85
scene scr\se_02_01_10.txt @86
scene scr\se_02_01_11.txt @87
scene scr\se_02_01_12.txt @88
scene scr\se_02_01_14.txt @89
scene scr\se_02_01_15.txt @90
scene scr\se_02_01_16.txt @91
scene scr\se_02_01_17.txt @92
scene scr\se_02_02_00.txt @93
scene scr\se_02_02_01.txt @94
scene scr\se_02_02_02.txt @95
choice obj\p001_001.bmp 2 @96
on 1 goto other
scene scr\se_02_02_03.txt @97
goto joined
label other
scene scr\se_02_02_04.txt @97
label joined
scene scr\se_02_02_05.txt @98
scene scr\se_02_02_06.txt @99
choice obj\p001_002.bmp 2 @100
on 1 goto on
scene scr\se_02_02_07.txt @210
staffroll b
scene scr\se_02_02_08.txt @212
end
label on
scene scr\se_02_03_00.txt @102
scene scr\se_02_03_01.txt @103
scene scr\se_02_03_02.txt @104
scene scr\se_02_03_03.txt @105
scene scr\se_02_03_04.txt @220
staffroll c
scene scr\se_02_04_00.txt @222
scene scr\se_02_04_01.txt @223
scene scr\se_02_04_01a.txt @224
scene scr\se_02_04_02.txt @225
scene scr\se_02_04_03.txt @226
scene scr\se_02_04_05.txt @227
scene scr\se_02_04_06.txt @228
end
label leave
scene scr\se_01_04_05.txt @200
staffroll a
scene scr\se_01_04_06.txt @202
end
)";
