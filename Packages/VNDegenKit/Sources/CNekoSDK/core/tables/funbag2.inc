// Test Game 83, Funbag Fantasy 2. Its story order is in the exe; the flow below was traced under Wine, which scene
// scripts the PC reads through each route and what its saves at each choice hold. Picture names and numbers only.
const char kFunbag2[] = R"(
# Measured under Wine: the art with "Click Any Mouse Button" until a click, then the menu. The menu's help is a circle
# 143 pixels above the button under the pointer. Start opens Prologue and Main Story in the first two rows once the
# prologue has been read (global 2), and Extra its three galleries in its own rows; the main buttons go while one is open.
screen title
menustages
startmenu 2
submenudim hide
pic obj\d000_000.bmp 0 0
# Not yet drawn: the three lace mandalas (obj\d001_000 to _002) that turn slowly over the art on the PC.
pic obj\a002_001.bmp 0 0
pic obj\d000_001.bmp 334 11
pic obj\d000_002.bmp 473 647
pic obj\d000_003.bmp 351 419 stage 1
button start obj\d000_004.bmp 480 363 6 5 0 stage 2 help obj\d000_006.bmp 356 220 6 1 0 0
button load obj\d000_004.bmp 480 409 6 5 1 stage 2 help obj\d000_006.bmp 356 266 6 1 1 0
button submenu:extra obj\d000_004.bmp 480 455 6 5 2 stage 2 help obj\d000_006.bmp 356 312 6 1 2 0
button config obj\d000_004.bmp 480 501 6 5 3 stage 2 help obj\d000_006.bmp 356 358 6 1 3 0
button websurvey obj\d000_004.bmp 480 547 6 5 4 stage 2 help obj\d000_006.bmp 356 404 6 1 4 0
button quit obj\d000_004.bmp 480 593 6 5 5 stage 2 help obj\d000_006.bmp 356 450 6 1 5 0
button start_prologue obj\d000_005.bmp 514 363 5 5 0 stage 2 group start help obj\d000_007.bmp 356 220 5 1 0 0
button start_main obj\d000_005.bmp 514 409 5 5 1 stage 2 group start help obj\d000_007.bmp 356 266 5 1 1 0
button cg obj\d000_005.bmp 514 455 5 5 2 stage 2 group extra help obj\d000_007.bmp 356 312 5 1 2 0
button scene obj\d000_005.bmp 514 501 5 5 3 stage 2 group extra help obj\d000_007.bmp 356 358 5 1 3 0
button music obj\d000_005.bmp 514 547 5 5 4 stage 2 group extra help obj\d000_007.bmp 356 404 5 1 4 0

screen load
# The room behind the menus is one of the 15 the exe lists, picked afresh each time a screen opens, at 46 percent.
pic bg\bg_000_000_000.bmp 0 0 more bg\bg_000_000_000.bmp,bg\bg_000_001_000.bmp,bg\bg_000_002_000.bmp,bg\bg_000_003_000.bmp,bg\bg_000_004_000.bmp,bg\bg_000_005_000.bmp,bg\bg_000_006_000.bmp,bg\bg_000_008_000.bmp,bg\bg_000_009_000.bmp,bg\bg_000_017_000.bmp,bg\bg_000_018_000.bmp,bg\bg_000_020_000.bmp,bg\bg_000_022_000.bmp,bg\bg_000_023_000.bmp,bg\bg_000_025_000.bmp random
pic dim 0 0 alpha 138
pic obj\e001_000.bmp 1103 0
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 22 18 6 1 1 0
slots obj\e002_000.bmp 60 99 510 119 5 2 1
slotthumb 29 10
slotdigits obj\e002_002.bmp 239 6 3 15
slotnew obj\e003_000.bmp -23 -3
# The chapter is S 5000, a cell of the strip: the prologue, then chapters 1 to 4 as the PC's cards name them.
slotchaptervar obj\e004_000.bmp 358 4 5 5000
datedigits obj\e002_002.bmp 0 0
datefield year 251 28 15
# Day, hour and minute show no leading zero on the PC. The month is taken to do the same, as only 11 has been seen.
datefield month 347 28 15 blank
datefield day 406 28 15 blank
datefield hour 251 50 15 blank
datefield minute 310 50 15 blank
button page0 obj\e001_003.bmp 408 31 10 5 0
button page1 obj\e001_003.bmp 470 31 10 5 1
button page2 obj\e001_003.bmp 532 31 10 5 2
button page3 obj\e001_003.bmp 594 31 10 5 3
button page4 obj\e001_003.bmp 656 31 10 5 4
button page5 obj\e001_003.bmp 718 31 10 5 5
button page6 obj\e001_003.bmp 780 31 10 5 6
button page7 obj\e001_003.bmp 842 31 10 5 7
button page8 obj\e001_003.bmp 904 31 10 5 8
button page9 obj\e001_003.bmp 966 31 10 5 9
button nav_save obj\e001_001.bmp 1114 61 9 5 0 help obj\e001_002.bmp 947 15 9 1 0 0
button nav_load obj\e001_001.bmp 1114 131 9 5 1 help obj\e001_002.bmp 947 85 9 1 1 0
button nav_cg obj\e001_001.bmp 1114 201 9 5 2 help obj\e001_002.bmp 947 155 9 1 2 0
button nav_scene obj\e001_001.bmp 1114 271 9 5 3 help obj\e001_002.bmp 947 225 9 1 3 0
button nav_music obj\e001_001.bmp 1114 341 9 5 4 help obj\e001_002.bmp 947 295 9 1 4 0
button nav_config obj\e001_001.bmp 1114 411 9 5 5 help obj\e001_002.bmp 947 365 9 1 5 0
button nav_title obj\e001_001.bmp 1114 481 9 5 6 help obj\e001_002.bmp 947 435 9 1 6 0
button nav_quit obj\e001_001.bmp 1114 551 9 5 7 help obj\e001_002.bmp 947 505 9 1 7 0
button back obj\e001_001.bmp 1114 639 9 5 8 help obj\e001_002.bmp 947 593 9 1 8 0

screen save
# The room behind the menus is one of the 15 the exe lists, picked afresh each time a screen opens, at 46 percent.
pic bg\bg_000_000_000.bmp 0 0 more bg\bg_000_000_000.bmp,bg\bg_000_001_000.bmp,bg\bg_000_002_000.bmp,bg\bg_000_003_000.bmp,bg\bg_000_004_000.bmp,bg\bg_000_005_000.bmp,bg\bg_000_006_000.bmp,bg\bg_000_008_000.bmp,bg\bg_000_009_000.bmp,bg\bg_000_017_000.bmp,bg\bg_000_018_000.bmp,bg\bg_000_020_000.bmp,bg\bg_000_022_000.bmp,bg\bg_000_023_000.bmp,bg\bg_000_025_000.bmp random
pic dim 0 0 alpha 138
pic obj\e001_000.bmp 1103 0
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 22 18 6 1 0 0
slots obj\e002_000.bmp 60 99 510 119 5 2 1
slotthumb 29 10
slotdigits obj\e002_002.bmp 239 6 3 15
slotnew obj\e003_000.bmp -23 -3
# The chapter is S 5000, a cell of the strip: the prologue, then chapters 1 to 4 as the PC's cards name them.
slotchaptervar obj\e004_000.bmp 358 4 5 5000
datedigits obj\e002_002.bmp 0 0
datefield year 251 28 15
# Day, hour and minute show no leading zero on the PC. The month is taken to do the same, as only 11 has been seen.
datefield month 347 28 15 blank
datefield day 406 28 15 blank
datefield hour 251 50 15 blank
datefield minute 310 50 15 blank
button page0 obj\e001_003.bmp 408 31 10 5 0
button page1 obj\e001_003.bmp 470 31 10 5 1
button page2 obj\e001_003.bmp 532 31 10 5 2
button page3 obj\e001_003.bmp 594 31 10 5 3
button page4 obj\e001_003.bmp 656 31 10 5 4
button page5 obj\e001_003.bmp 718 31 10 5 5
button page6 obj\e001_003.bmp 780 31 10 5 6
button page7 obj\e001_003.bmp 842 31 10 5 7
button page8 obj\e001_003.bmp 904 31 10 5 8
button page9 obj\e001_003.bmp 966 31 10 5 9
button nav_save obj\e001_001.bmp 1114 61 9 5 0 help obj\e001_002.bmp 947 15 9 1 0 0
button nav_load obj\e001_001.bmp 1114 131 9 5 1 help obj\e001_002.bmp 947 85 9 1 1 0
button nav_cg obj\e001_001.bmp 1114 201 9 5 2 help obj\e001_002.bmp 947 155 9 1 2 0
button nav_scene obj\e001_001.bmp 1114 271 9 5 3 help obj\e001_002.bmp 947 225 9 1 3 0
button nav_music obj\e001_001.bmp 1114 341 9 5 4 help obj\e001_002.bmp 947 295 9 1 4 0
button nav_config obj\e001_001.bmp 1114 411 9 5 5 help obj\e001_002.bmp 947 365 9 1 5 0
button nav_title obj\e001_001.bmp 1114 481 9 5 6 help obj\e001_002.bmp 947 435 9 1 6 0
button nav_quit obj\e001_001.bmp 1114 551 9 5 7 help obj\e001_002.bmp 947 505 9 1 7 0
button back obj\e001_001.bmp 1114 639 9 5 8 help obj\e001_002.bmp 947 593 9 1 8 0

screen defaults
parts scroll
# CG flags by group: 48000 + 4000 x group + 4 x place, as the PC's cfg.sav holds them.
cgflagsbygroup 4000
# The wait mark has no .alp: its pure green is see-through on the PC.
colorkey 0 255 0
# The PC draws the message window at its .alp as it is. Config's first-run values, as the PC shows them under Wine.
default opacity 100
default text 50
default auto 50
default bgm 60
default se 60
default voice 80
default backvoice 80
default skipall 1
layoutcgslot 102
lastchoice made

# Config, one page, measured under Wine. Its help is a circle above or below the row under the pointer. The screen's
# colour depth and resolution, and asking for the resolution at start, are drawn disabled, as VNDegen sizes the window.
screen config
pic bg\bg_000_000_000.bmp 0 0 more bg\bg_000_000_000.bmp,bg\bg_000_001_000.bmp,bg\bg_000_002_000.bmp,bg\bg_000_003_000.bmp,bg\bg_000_004_000.bmp,bg\bg_000_005_000.bmp,bg\bg_000_006_000.bmp,bg\bg_000_008_000.bmp,bg\bg_000_009_000.bmp,bg\bg_000_017_000.bmp,bg\bg_000_018_000.bmp,bg\bg_000_020_000.bmp,bg\bg_000_022_000.bmp,bg\bg_000_023_000.bmp,bg\bg_000_025_000.bmp random
pic dim 0 0 alpha 138
pic obj\e001_000.bmp 1103 0
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 22 18 6 1 5 0
pic obj\h001_000.bmp 24 89
pic obj\h002_000.bmp 24 454
pic obj\h003_000.bmp 560 89
pic obj\h004_000.bmp 560 259
button cfg_defaults obj\h000_000.bmp 736 31 3 5 0 help obj\h000_001.bmp 696 86 3 1 0 0
button cfg_undo obj\h000_000.bmp 856 31 3 5 1 help obj\h000_001.bmp 816 86 3 1 1 0
button cfg_apply obj\h000_000.bmp 976 31 3 5 2 help obj\h000_001.bmp 936 86 3 1 2 0
button nav_save obj\e001_001.bmp 1114 61 9 5 0 help obj\e001_002.bmp 947 15 9 1 0 0
button nav_load obj\e001_001.bmp 1114 131 9 5 1 help obj\e001_002.bmp 947 85 9 1 1 0
button nav_cg obj\e001_001.bmp 1114 201 9 5 2 help obj\e001_002.bmp 947 155 9 1 2 0
button nav_scene obj\e001_001.bmp 1114 271 9 5 3 help obj\e001_002.bmp 947 225 9 1 3 0
button nav_music obj\e001_001.bmp 1114 341 9 5 4 help obj\e001_002.bmp 947 295 9 1 4 0
button nav_config obj\e001_001.bmp 1114 411 9 5 5 help obj\e001_002.bmp 947 365 9 1 5 0
button nav_title obj\e001_001.bmp 1114 481 9 5 6 help obj\e001_002.bmp 947 435 9 1 6 0
button nav_quit obj\e001_001.bmp 1114 551 9 5 7 help obj\e001_002.bmp 947 505 9 1 7 0
button back obj\e001_001.bmp 1114 639 9 5 8 help obj\e001_002.bmp 947 593 9 1 8 0
radio screen 0 obj\h001_001.bmp 264 124 10 5 0
radio screen 1 obj\h001_001.bmp 381 124 10 5 1
pic obj\h001_001.bmp 264 152 10 5 2 4
pic obj\h001_001.bmp 381 152 10 5 3 4
pic obj\h001_001.bmp 216 180 10 5 4 4
pic obj\h001_001.bmp 323 180 10 5 5 4
pic obj\h001_001.bmp 430 180 10 5 6 4
pic obj\h001_001.bmp 216 208 10 5 7 4
pic obj\h001_001.bmp 323 208 10 5 8 4
pic obj\h001_001.bmp 430 208 10 5 9 4
radio effect 1 obj\h001_002.bmp 264 236 2 5 0
radio effect 0 obj\h001_002.bmp 381 236 2 5 1
radio wait 1 obj\h001_002.bmp 264 264 2 5 0
radio wait 0 obj\h001_002.bmp 381 264 2 5 1
pic obj\h001_010.bmp 88 321
slider opacity obj\h001_006.bmp 88 321
pic obj\h001_005.bmp 88 321
knob opacity obj\h001_004.bmp 73 315 4.08
step opacity -1 obj\h001_003.bmp 52 317 2 5 0
step opacity 1 obj\h001_003.bmp 516 317 2 5 1
number opacity obj\e002_002.bmp 443 293 15 blank
radio confirm_saveload 1 obj\h001_002.bmp 264 348 2 5 0
radio confirm_saveload 0 obj\h001_002.bmp 381 348 2 5 1
group confirm_saveload confirm_overwrite confirm_delete confirm_quick confirm_title confirm_exit
radio tutorial 1 obj\h001_002.bmp 264 376 2 5 0
radio tutorial 0 obj\h001_002.bmp 381 376 2 5 1
pic obj\h001_007.bmp 101 403 1 5 0 4
pic obj\h001_010.bmp 88 518
slider text obj\h001_006.bmp 88 518
pic obj\h001_005.bmp 88 518
knob text obj\h001_004.bmp 73 512 4.08
step text -1 obj\h001_003.bmp 52 514 2 5 0
step text 1 obj\h001_003.bmp 516 514 2 5 1
number text obj\e002_002.bmp 443 490 15 blank
pic obj\h001_010.bmp 88 574
slider auto obj\h001_006.bmp 88 574
pic obj\h001_005.bmp 88 574
knob auto obj\h001_004.bmp 73 568 4.08
step auto -1 obj\h001_003.bmp 52 570 2 5 0
step auto 1 obj\h001_003.bmp 516 570 2 5 1
number auto obj\e002_002.bmp 443 546 15 blank
# The wait time runs from x0.50 to x1.50, its rose in the middle at x1.00 (inferred from the PC's middle place).
range waittime 50 150
pic obj\h001_010.bmp 88 630
pic obj\h001_005.bmp 88 630
knob waittime obj\h001_004.bmp -131 624 4.08
step waittime -1 obj\h001_003.bmp 52 626 2 5 0
step waittime 1 obj\h001_003.bmp 516 626 2 5 1
number waittime obj\e002_002.bmp 435 602 459 474
radio skipall 0 obj\h003_001.bmp 800 124 9 5 0
radio skipall 1 obj\h003_001.bmp 917 124 9 5 1
radio rightclick 0 obj\h003_001.bmp 752 152 9 5 2
radio rightclick 1 obj\h003_001.bmp 859 152 9 5 3
radio rightclick 2 obj\h003_001.bmp 966 152 9 5 4
radio rightclick 3 obj\h003_001.bmp 800 180 9 5 5
radio rightclick 4 obj\h003_001.bmp 917 180 9 5 6
radio deckey 0 obj\h003_001.bmp 800 208 9 5 7
radio deckey 1 obj\h003_001.bmp 917 208 9 5 8
pic obj\h001_010.bmp 624 323
slider bgm obj\h001_006.bmp 624 323
pic obj\h001_005.bmp 624 323
knob bgm obj\h001_004.bmp 609 317 4.08
step bgm -1 obj\h001_003.bmp 588 319 2 5 0
step bgm 1 obj\h001_003.bmp 1052 319 2 5 1
number bgm obj\e002_002.bmp 979 295 15 blank
pic obj\h001_010.bmp 624 379
slider se obj\h001_006.bmp 624 379
pic obj\h001_005.bmp 624 379
knob se obj\h001_004.bmp 609 373 4.08
step se -1 obj\h001_003.bmp 588 375 2 5 0
step se 1 obj\h001_003.bmp 1052 375 2 5 1
number se obj\e002_002.bmp 979 351 15 blank
pic obj\h001_010.bmp 624 435
slider voice obj\h001_006.bmp 624 435
pic obj\h001_005.bmp 624 435
knob voice obj\h001_004.bmp 609 429 4.08
step voice -1 obj\h001_003.bmp 588 431 2 5 0
step voice 1 obj\h001_003.bmp 1052 431 2 5 1
number voice obj\e002_002.bmp 979 407 15 blank
pic obj\h001_010.bmp 624 491
slider backvoice obj\h001_006.bmp 624 491
pic obj\h001_005.bmp 624 491
knob backvoice obj\h001_004.bmp 609 485 4.08
step backvoice -1 obj\h001_003.bmp 588 487 2 5 0
step backvoice 1 obj\h001_003.bmp 1052 487 2 5 1
number backvoice obj\e002_002.bmp 979 463 15 blank
radio mute_backvoice 0 obj\h001_002.bmp 800 518 2 5 0
radio mute_backvoice 1 obj\h001_002.bmp 917 518 2 5 1
radio voice_stop 1 obj\h001_002.bmp 800 546 2 5 0
radio voice_stop 0 obj\h001_002.bmp 917 546 2 5 1
pic obj\h004_003.bmp 949 294 1 5 0 4
pic obj\h004_003.bmp 949 350 1 5 0 0
pic obj\h004_003.bmp 949 406 1 5 0 0
pic obj\h004_003.bmp 949 462 1 5 0 0
pic obj\h004_001.bmp 752 574 4 5 0 0
pic obj\h004_001.bmp 859 574 4 5 1 0
pic obj\h004_001.bmp 966 574 4 5 2 0
pic obj\h004_001.bmp 752 602 4 5 3 0
hovercols 40 124 540 150 obj\h001_008.bmp 29 146 8 0
hovercols 40 152 540 178 obj\h001_008.bmp 29 174 8 1
hovercols 40 180 540 234 obj\h001_008.bmp 29 15 8 2
hovercols 40 236 540 262 obj\h001_008.bmp 29 71 8 3
hovercols 40 264 540 290 obj\h001_008.bmp 29 99 8 4
hovercols 40 292 540 345 obj\h001_008.bmp 29 126 8 5
hovercols 40 348 540 374 obj\h001_008.bmp 29 183 8 6
hovercols 40 376 540 402 obj\h001_008.bmp 29 211 8 7
hovercols 40 488 540 540 obj\h002_001.bmp 29 321 3 0
hovercols 40 545 540 597 obj\h002_001.bmp 29 378 3 1
hovercols 40 600 540 652 obj\h002_001.bmp 29 435 3 2
hovercols 576 124 1080 150 obj\h003_002.bmp 555 146 3 0
hovercols 576 152 1080 206 obj\h003_002.bmp 555 174 3 1
hovercols 576 208 1080 234 obj\h003_002.bmp 555 42 3 2
hovercols 576 292 1080 345 obj\h004_002.bmp 555 126 7 0
hovercols 576 349 1080 401 obj\h004_002.bmp 555 184 7 1
hovercols 576 405 1080 457 obj\h004_002.bmp 555 240 7 2
hovercols 576 461 1080 513 obj\h004_002.bmp 555 296 7 3
hovercols 576 518 1080 544 obj\h004_002.bmp 555 353 7 4
hovercols 576 546 1080 572 obj\h004_002.bmp 555 380 7 5
hovercols 576 574 1080 628 obj\h004_002.bmp 555 407 7 6

# The CG gallery, measured under Wine: a tab for each heroine and one for the rest, each with her figure at the left,
# twelve cards a page.
screen cg
pic bg\bg_000_000_000.bmp 0 0 more bg\bg_000_000_000.bmp,bg\bg_000_001_000.bmp,bg\bg_000_002_000.bmp,bg\bg_000_003_000.bmp,bg\bg_000_004_000.bmp,bg\bg_000_005_000.bmp,bg\bg_000_006_000.bmp,bg\bg_000_008_000.bmp,bg\bg_000_009_000.bmp,bg\bg_000_017_000.bmp,bg\bg_000_018_000.bmp,bg\bg_000_020_000.bmp,bg\bg_000_022_000.bmp,bg\bg_000_023_000.bmp,bg\bg_000_025_000.bmp random
pic dim 0 0 alpha 138
pic obj\e001_000.bmp 1103 0
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 22 18 6 1 2 0
pic sc\sc_000_000_000.bmp -80 0 tab 0
pic sc\sc_000_001_000.bmp -60 0 tab 1
pic sc\sc_000_002_000.bmp -450 0 tab 2
pic sc\sc_000_003_000.bmp -140 0 tab 3
pic sc\sc_000_004_000.bmp -80 0 tab 4
pic obj\f002_000.bmp -170 0 tab 5
button nav_save obj\e001_001.bmp 1114 61 9 5 0 help obj\e001_002.bmp 947 15 9 1 0 0
button nav_load obj\e001_001.bmp 1114 131 9 5 1 help obj\e001_002.bmp 947 85 9 1 1 0
button nav_cg obj\e001_001.bmp 1114 201 9 5 2 help obj\e001_002.bmp 947 155 9 1 2 0
button nav_scene obj\e001_001.bmp 1114 271 9 5 3 help obj\e001_002.bmp 947 225 9 1 3 0
button nav_music obj\e001_001.bmp 1114 341 9 5 4 help obj\e001_002.bmp 947 295 9 1 4 0
button nav_config obj\e001_001.bmp 1114 411 9 5 5 help obj\e001_002.bmp 947 365 9 1 5 0
button nav_title obj\e001_001.bmp 1114 481 9 5 6 help obj\e001_002.bmp 947 435 9 1 6 0
button nav_quit obj\e001_001.bmp 1114 551 9 5 7 help obj\e001_002.bmp 947 505 9 1 7 0
button back obj\e001_001.bmp 1114 639 9 5 8 help obj\e001_002.bmp 947 593 9 1 8 0
button gtab0 obj\f001_000.bmp 31 540 6 5 0
button gtab1 obj\f001_000.bmp 139 540 6 5 1
button gtab2 obj\f001_000.bmp 247 540 6 5 2
button gtab3 obj\f001_000.bmp 31 619 6 5 3
button gtab4 obj\f001_000.bmp 139 619 6 5 4
button gtab5 obj\f001_000.bmp 247 619 6 5 5
button page0 obj\e001_003.bmp 408 31 10 5 0
button page1 obj\e001_003.bmp 470 31 10 5 1
gallerytabs 0
cellsheet obj\f000_000.bmp
cellsize 220 141
cellthumb 32 12
celldigits obj\e002_002.bmp 60 107 2 15
cellcountstep 15
cellcount obj\e002_002.bmp obj\f000_002.bmp 114 107 145 158 2
cell 362 91
cell 602 91
cell 842 91
cell 362 241
cell 602 241
cell 842 241
cell 362 391
cell 602 391
cell 842 391
cell 362 541
cell 602 541
cell 842 541

screen scene
pic bg\bg_000_000_000.bmp 0 0 more bg\bg_000_000_000.bmp,bg\bg_000_001_000.bmp,bg\bg_000_002_000.bmp,bg\bg_000_003_000.bmp,bg\bg_000_004_000.bmp,bg\bg_000_005_000.bmp,bg\bg_000_006_000.bmp,bg\bg_000_008_000.bmp,bg\bg_000_009_000.bmp,bg\bg_000_017_000.bmp,bg\bg_000_018_000.bmp,bg\bg_000_020_000.bmp,bg\bg_000_022_000.bmp,bg\bg_000_023_000.bmp,bg\bg_000_025_000.bmp random
pic dim 0 0 alpha 138
pic obj\e001_000.bmp 1103 0
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 22 18 6 1 3 0
pic sc\sc_000_000_000.bmp -80 0 tab 0
pic sc\sc_000_001_000.bmp -60 0 tab 1
pic sc\sc_000_002_000.bmp -450 0 tab 2
pic sc\sc_000_003_000.bmp -140 0 tab 3
pic sc\sc_000_004_000.bmp -80 0 tab 4
pic obj\f002_000.bmp -170 0 tab 5
button nav_save obj\e001_001.bmp 1114 61 9 5 0 help obj\e001_002.bmp 947 15 9 1 0 0
button nav_load obj\e001_001.bmp 1114 131 9 5 1 help obj\e001_002.bmp 947 85 9 1 1 0
button nav_cg obj\e001_001.bmp 1114 201 9 5 2 help obj\e001_002.bmp 947 155 9 1 2 0
button nav_scene obj\e001_001.bmp 1114 271 9 5 3 help obj\e001_002.bmp 947 225 9 1 3 0
button nav_music obj\e001_001.bmp 1114 341 9 5 4 help obj\e001_002.bmp 947 295 9 1 4 0
button nav_config obj\e001_001.bmp 1114 411 9 5 5 help obj\e001_002.bmp 947 365 9 1 5 0
button nav_title obj\e001_001.bmp 1114 481 9 5 6 help obj\e001_002.bmp 947 435 9 1 6 0
button nav_quit obj\e001_001.bmp 1114 551 9 5 7 help obj\e001_002.bmp 947 505 9 1 7 0
button back obj\e001_001.bmp 1114 639 9 5 8 help obj\e001_002.bmp 947 593 9 1 8 0
button gtab0 obj\f001_000.bmp 31 540 6 5 0
button gtab1 obj\f001_000.bmp 139 540 6 5 1
button gtab2 obj\f001_000.bmp 247 540 6 5 2
button gtab3 obj\f001_000.bmp 31 619 6 5 3
button gtab4 obj\f001_000.bmp 139 619 6 5 4
button gtab5 obj\f001_000.bmp 247 619 6 5 5
button page0 obj\e001_003.bmp 408 31 10 5 0
gallerytabs 0
cellhide
cellsheet obj\f000_000.bmp
cellsize 220 141
cellthumb 32 12
celldigits obj\e002_002.bmp 60 107 2 15
cell 362 91
cell 602 91
cell 842 91
cell 362 241
cell 602 241
cell 842 241
cell 362 391
cell 602 391
cell 842 391
cell 362 541
cell 602 541
cell 842 541
# The two-key scenes replay from the scene before their choice, ask it again, and play the variant taken.
replayflow scr\ep_01_04_02.txt replay1
replayflow scr\ep_01_05_02.txt replay2
replayflow scr\ep_01_07_02.txt replay3

# The music screen, measured under Wine: the tracks in three columns, the player under them and its volume at the left.
screen music
pic bg\bg_000_000_000.bmp 0 0 more bg\bg_000_000_000.bmp,bg\bg_000_001_000.bmp,bg\bg_000_002_000.bmp,bg\bg_000_003_000.bmp,bg\bg_000_004_000.bmp,bg\bg_000_005_000.bmp,bg\bg_000_006_000.bmp,bg\bg_000_008_000.bmp,bg\bg_000_009_000.bmp,bg\bg_000_017_000.bmp,bg\bg_000_018_000.bmp,bg\bg_000_020_000.bmp,bg\bg_000_022_000.bmp,bg\bg_000_023_000.bmp,bg\bg_000_025_000.bmp random
pic dim 0 0 alpha 138
pic obj\e001_000.bmp 1103 0
pic obj\e000_000.bmp 0 0
pic obj\e000_001.bmp 22 18 6 1 4 0
pic obj\g000_000.bmp 15 93
pic obj\g001_000.bmp 283 495
pic obj\g002_000.bmp 135 591
pic obj\g003_000.bmp 397 591
slider music obj\g002_002.bmp 191 627
knob music obj\g002_001.bmp 186 625 1.08
hover 135 591 366 684 obj\g003_002.bmp 8 470 8 1 7 0
button nav_save obj\e001_001.bmp 1114 61 9 5 0 help obj\e001_002.bmp 947 15 9 1 0 0
button nav_load obj\e001_001.bmp 1114 131 9 5 1 help obj\e001_002.bmp 947 85 9 1 1 0
button nav_cg obj\e001_001.bmp 1114 201 9 5 2 help obj\e001_002.bmp 947 155 9 1 2 0
button nav_scene obj\e001_001.bmp 1114 271 9 5 3 help obj\e001_002.bmp 947 225 9 1 3 0
button nav_music obj\e001_001.bmp 1114 341 9 5 4 help obj\e001_002.bmp 947 295 9 1 4 0
button nav_config obj\e001_001.bmp 1114 411 9 5 5 help obj\e001_002.bmp 947 365 9 1 5 0
button nav_title obj\e001_001.bmp 1114 481 9 5 6 help obj\e001_002.bmp 947 435 9 1 6 0
button nav_quit obj\e001_001.bmp 1114 551 9 5 7 help obj\e001_002.bmp 947 505 9 1 7 0
button back obj\e001_001.bmp 1114 639 9 5 8 help obj\e001_002.bmp 947 593 9 1 8 0
button track0 obj\g000_001.bmp 163 108 18 5 0
button track1 obj\g000_001.bmp 163 155 18 5 1
button track2 obj\g000_001.bmp 163 202 18 5 2
button track3 obj\g000_001.bmp 163 249 18 5 3
button track4 obj\g000_001.bmp 163 296 18 5 4
button track5 obj\g000_001.bmp 163 343 18 5 5
button track6 obj\g000_001.bmp 442 108 18 5 6
button track7 obj\g000_001.bmp 442 155 18 5 7
button track8 obj\g000_001.bmp 442 202 18 5 8
button track9 obj\g000_001.bmp 442 249 18 5 9
button track10 obj\g000_001.bmp 442 296 18 5 10
button track11 obj\g000_001.bmp 442 343 18 5 11
button track12 obj\g000_001.bmp 721 108 18 5 12
button track13 obj\g000_001.bmp 721 155 18 5 13
button track14 obj\g000_001.bmp 721 202 18 5 14
button track15 obj\g000_001.bmp 721 249 18 5 15
button track16 obj\g000_001.bmp 721 296 18 5 16
button track17 obj\g000_001.bmp 721 343 18 5 17
button music_prev obj\g003_001.bmp 443 614 7 5 0 help obj\g003_002.bmp 307 470 8 1 0 0
button music_pause obj\g003_001.bmp 513 614 7 5 1 help obj\g003_002.bmp 377 470 8 1 1 0
button music_play obj\g003_001.bmp 583 614 7 5 2 help obj\g003_002.bmp 447 470 8 1 2 0
button music_stop obj\g003_001.bmp 653 614 7 5 3 help obj\g003_002.bmp 517 470 8 1 3 0
button music_next obj\g003_001.bmp 723 614 7 5 4 help obj\g003_002.bmp 587 470 8 1 4 0
button music_repeat obj\g003_001.bmp 793 614 7 5 5 help obj\g003_002.bmp 657 470 8 1 5 0
button music_shuffle obj\g003_001.bmp 863 614 7 5 6 help obj\g003_002.bmp 727 470 8 1 6 0

# The question, as the PC asks it under Wine: the scene darkened to 0.14, the question's plate above two answers, each
# over a pair of crossed swords (the sheet's two halves drawn at one place).
screen dialog
dim 35
pic obj\a003_001.bmp 351 228 2 1 0 0
pic obj\a003_001.bmp 351 228 2 1 1 0
pic obj\a003_001.bmp 695 228 2 1 0 0
pic obj\a003_001.bmp 695 228 2 1 1 0
pic obj\a003_000.bmp 392 51 7 1 0 0
questions resolution save load overwrite exit title back
button yes obj\a003_002.bmp 306 312 2 5 0
button no obj\a003_002.bmp 650 312 2 5 1

# A choice, as on the PC under Wine: the same box as the question's, with the choice's own two options in place of yes
# and no, and its prompt plate. Each choice picture's options are its two columns.
screen choice
pic dim 0 0 alpha 221
pic obj\a003_001.bmp 351 228 2 1 0 0
pic obj\a003_001.bmp 351 228 2 1 1 0
pic obj\a003_001.bmp 695 228 2 1 0 0
pic obj\a003_001.bmp 695 228 2 1 1 0
pic obj\a003_003.bmp 392 51
choicecolumns 2
placefor 2 288 0 306 344

# The backlog shows in the message window itself, older lines in init.ini's log colour.
screen log
inwindow

screen cfgsav
size 96000
tracks 92000
nostring

screen story
# The six choices are obj\l000_000 to _005, two options each. Sex scenes end on l000_000, which only picks the next
# scene; option 0 also counts in S 5001 (option 1 never set it, in a run that took it every time). S 5000 is the
# route's chapter as the PC's saves hold it.
# New Game plays the prologue. Once global 2 is set, Start offers Prologue or Main Story, and Main Story starts at
# the flow `story` (traced: with global 100 set and 2 not, Start went straight to the prologue).
flow prologue
scene scr\ep_00_00_00.txt @1
set G 2 = 1
jump story

flow story
# Global 100 is set in every cfg.sav the PC wrote after any scene past the prologue; where exactly is inferred.
set G 100 = 1
set S 5000 = 1
scene scr\ep_01_01_00.txt @1001
scene scr\ep_01_02_00.txt @1002
scene scr\ep_01_03_00.txt @1003
scene scr\ep_01_04_00.txt @1004
scene scr\ep_01_04_01.txt @1005
choice obj\l000_000.bmp 2 @1006
on 1 goto h1b
set S 5001 += 1
scene scr\ep_01_04_02.txt @1030
goto h1j
label h1b
scene scr\ep_01_04_03.txt @1030
label h1j
scene scr\ep_01_04_04.txt @1031
scene scr\ep_01_05_00.txt @1032
scene scr\ep_01_05_01.txt @1033
choice obj\l000_000.bmp 2 @1034
on 1 goto h2b
set S 5001 += 1
scene scr\ep_01_05_02.txt @1060
goto h2j
label h2b
scene scr\ep_01_05_03.txt @1060
label h2j
scene scr\ep_01_05_04.txt @1061
scene scr\ep_01_06_00.txt @1062
scene scr\ep_01_07_00.txt @1063
scene scr\ep_01_07_01.txt @1064
choice obj\l000_000.bmp 2 @1065
on 1 goto h3b
set S 5001 += 1
scene scr\ep_01_07_02.txt @1090
goto h3j
label h3b
scene scr\ep_01_07_03.txt @1090
label h3j
scene scr\ep_01_08_00.txt @1091
scene scr\ep_01_09_00.txt @1092
# Castellan of Boobshire (chapters 3 and 4) or prefect of Molgrey (chapter 2 and on).
choice obj\l000_001.bmp 2 @1093
on 1 goto molgrey
scene scr\ep_01_09_01.txt @1101
goto boobshire
label molgrey
scene scr\ep_01_09_02.txt @1111
scene scr\ep_01_10_00.txt @1112

# Chapter 2.
set S 5000 = 2
scene scr\ep_02_01_00.txt @2001
scene scr\ep_02_01_01.txt @2002
choice obj\l000_000.bmp 2 @2003
on 1 goto h4b
set S 5001 += 1
scene scr\ep_02_01_02.txt @2030
goto h4j
label h4b
scene scr\ep_02_01_03.txt @2030
label h4j
scene scr\ep_02_01_04.txt @2031
scene scr\ep_02_02_00.txt @2032
scene scr\ep_02_03_00.txt @2033
scene scr\ep_02_04_00.txt @2034
scene scr\ep_02_04_01.txt @2035
scene scr\ep_02_05_00.txt @2036
# Take Lorelia (an ending) or keep to the political career.
choice obj\l000_002.bmp 2 @2037
on 1 goto career
scene scr\ep_02_05_01.txt @2041
scene scr\ep_staff.txt
scene scr\ep_02_05_02.txt @999
end
label career
scene scr\ep_02_06_00.txt @2051
scene scr\ep_02_07_00.txt @2052
scene scr\ep_02_08_00.txt @2053
scene scr\ep_02_09_00.txt @2054
scene scr\ep_02_10_00.txt @2055
scene scr\ep_02_11_00.txt @2056
scene scr\ep_02_12_00.txt @2057
scene scr\ep_02_13_00.txt @2058
scene scr\ep_02_14_00.txt @2059
scene scr\ep_02_15_00.txt @2060
scene scr\ep_02_15_01.txt @2061
scene scr\ep_02_16_00.txt @2062
# Equal castellans (chapter 5) or husband rather than a pet (an ending).
choice obj\l000_003.bmp 2 @2063
on 1 goto husband
scene scr\ep_02_16_01.txt @2071
goto chapter5
label husband
scene scr\ep_02_16_02.txt @2081
scene scr\ep_02_17_00.txt @2082
scene scr\ep_staff.txt
scene scr\ep_02_17_01.txt @999
end

# Chapter 3.
label boobshire
set S 5000 = 3
scene scr\ep_03_01_00.txt @3001
scene scr\ep_03_02_00.txt @3002
scene scr\ep_03_03_00.txt @3003
scene scr\ep_03_04_00.txt @3004
scene scr\ep_03_05_00.txt @3005
scene scr\ep_03_06_00.txt @3006
scene scr\ep_03_07_00.txt @3007
scene scr\ep_03_08_00.txt @3008
scene scr\ep_03_09_00.txt @3009
scene scr\ep_03_10_00.txt @3010
scene scr\ep_03_11_00.txt @3011
scene scr\ep_03_12_00.txt @3012
scene scr\ep_03_13_00.txt @3013
# Don't dispatch a detachment (chapter 4) or let Zebia lead it (an ending). The exe names ep_03_17_00 and _01 in a
# sys_1 patch this copy doesn't have, and the PC goes from ep_03_16_00 to ep_03_18_00.
choice obj\l000_004.bmp 2 @3014
on 1 goto zebialeads
scene scr\ep_03_14_00.txt @3021
scene scr\ep_03_15_01.txt @4000
goto chapter4
label zebialeads
scene scr\ep_03_15_00.txt @3031
scene scr\ep_03_15_02.txt @3032
scene scr\ep_03_16_00.txt @3033
scene scr\ep_03_18_00.txt
scene scr\ep_03_18_01.txt @3052
scene scr\ep_staff.txt
scene scr\ep_03_19_00.txt @999
end

# Chapter 4.
label chapter4
scene scr\ep_04_01_00.txt @4001
scene scr\ep_04_02_00.txt @4002
scene scr\ep_04_03_00.txt @4003
scene scr\ep_04_04_00.txt @4004
scene scr\ep_04_05_00.txt @4005
scene scr\ep_04_05_01.txt @4006
scene scr\ep_04_06_00.txt @4007
scene scr\ep_04_07_00.txt @4008
scene scr\ep_04_08_00.txt @4009
scene scr\ep_04_09_00.txt @4010
scene scr\ep_04_10_00.txt @4011
scene scr\ep_04_11_00.txt @4012
scene scr\ep_staff.txt
scene scr\ep_04_11_01.txt @999
end

# Chapter 5, after the equal castellans.
label chapter5
set S 5000 = 4
scene scr\ep_05_01_00.txt @5001
scene scr\ep_05_01_01.txt @5002
scene scr\ep_05_02_00.txt @5003
# Use Zebia's body or thank her. Thanks goes on to chapters 6 and 7. Her body ends the story only when every one of the
# four sex scenes on this route took option 0: PC saves edited to S 5001 = 1, 2 and 3 went on as thanks does, and a
# real run with 4 ended. Where the exe checks it isn't known, so it's checked here, after her scene.
choice obj\l000_005.bmp 2 @5004
on 1 goto thanks
scene scr\ep_05_02_01.txt @5030
if S 5001 < 4 goto thanked
scene scr\ep_05_03_00.txt @5031
scene scr\ep_05_03_01.txt
scene scr\ep_05_03_02.txt @5051
scene scr\ep_05_03_03.txt @5052
scene scr\ep_05_04_00.txt @5053
scene scr\ep_05_04_02.txt
scene scr\ep_staff.txt
scene scr\ep_05_04_03.txt
end
label thanks
scene scr\ep_05_02_02.txt @5030
label thanked
scene scr\ep_05_03_00.txt @5031
scene scr\ep_05_03_02.txt @5051
scene scr\ep_05_03_03.txt @5052
scene scr\ep_05_04_00.txt @5053
scene scr\ep_05_04_01.txt @5071
scene scr\ep_05_05_00.txt @5072
scene scr\ep_05_06_00.txt @5073
scene scr\ep_05_06_01.txt @5074
scene scr\ep_05_07_00.txt @5075
scene scr\ep_05_07_01.txt @5076
scene scr\ep_05_07_02.txt @6000

# Chapter 6, which has no choices. The exe names ep_06_08_01 and ep_06_09_00b in the missing sys_1 patch.
scene scr\ep_06_01_00.txt @6001
scene scr\ep_06_02_00.txt @6002
scene scr\ep_06_02_01.txt @6003
scene scr\ep_06_02_02.txt @6004
scene scr\ep_06_03_00.txt @6005
scene scr\ep_06_03_01.txt @6006
scene scr\ep_06_04_00.txt @6007
scene scr\ep_06_04_01.txt @6008
scene scr\ep_06_04_02.txt @6009
scene scr\ep_06_05_00.txt @6010
scene scr\ep_06_05_01.txt @6011
scene scr\ep_06_06_00.txt @6012
scene scr\ep_06_06_01.txt @6013
scene scr\ep_06_07_00.txt @6014
scene scr\ep_06_07_01.txt @6015
scene scr\ep_06_08_00.txt @6016
scene scr\ep_06_08_02.txt @6031
scene scr\ep_06_08_03.txt @6032
scene scr\ep_06_09_00.txt @6100
scene scr\ep_06_09_00a.txt @6130
scene scr\ep_06_09_00c.txt @6033
scene scr\ep_06_09_01.txt @6034
scene scr\ep_06_09_02.txt @6035
scene scr\ep_06_09_03.txt @6036
scene scr\ep_06_10_00.txt @6037
scene scr\ep_06_10_01.txt @6038
scene scr\ep_06_10_02.txt @6039
scene scr\ep_06_10_03.txt @6040
scene scr\ep_06_11_00.txt @6041
scene scr\ep_06_12_00.txt @6042
scene scr\ep_06_12_01.txt @6043
scene scr\ep_06_13_00.txt @6044
scene scr\ep_06_13_01.txt @6045
scene scr\ep_06_13_02.txt @6046
scene scr\ep_06_13_03.txt @6047
scene scr\ep_06_14_00.txt @6048
scene scr\ep_06_14_01.txt @6049
scene scr\ep_06_15_00.txt @6050
scene scr\ep_06_15_01.txt @6051
scene scr\ep_06_15_02.txt @6052
scene scr\ep_06_15_03.txt
scene scr\ep_06_15_04.txt @6054
scene scr\ep_06_15_05.txt @6055
scene scr\ep_06_15_06.txt @6056
scene scr\ep_06_15_07.txt @6057
scene scr\ep_06_15_08.txt @6058
scene scr\ep_staff.txt
# Chapter 7, the epilogue.
scene scr\ep_07_00_00.txt @7001
scene scr\ep_07_00_01.txt
scene scr\ep_07_00_02.txt
scene scr\ep_07_00_03.txt
scene scr\ep_07_00_04.txt
end
flow replay1
scene scr\ep_01_04_01.txt @1005
choice obj\l000_000.bmp 2 @1006
on 1 goto b
scene scr\ep_01_04_02.txt @1030
end
label b
scene scr\ep_01_04_03.txt @1030
end
flow replay2
scene scr\ep_01_05_01.txt @1033
choice obj\l000_000.bmp 2 @1034
on 1 goto b
scene scr\ep_01_05_02.txt @1060
end
label b
scene scr\ep_01_05_03.txt @1060
end
flow replay3
scene scr\ep_01_07_01.txt @1064
choice obj\l000_000.bmp 2 @1065
on 1 goto b
scene scr\ep_01_07_02.txt @1090
end
label b
scene scr\ep_01_07_03.txt @1090
end
)";
