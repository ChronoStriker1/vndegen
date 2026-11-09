// Test Game 82, Funbag Fantasy (MangaGamer's English release, 2016). The oldest layout: backgrounds, CGs and standing
// pictures by commands 10 to 14, and its story order in the exe, traced under Wine (`screen story`).
const char kFunbagFantasy1[] = R"(
screen title
# Measured under Wine: the art with "Click to begin" until a click, then the menu. New Game opens Prologue and Main
# Story once an ending is seen (global 2), and Bonus its galleries; each item's help balloon stands above it.
menustages
selectedlook
submenudim
pic obj\c000_000.bmp 0 0
pic obj\c001_000.bmp 351 42
pic obj\c001_001.bmp 542 668
pic obj\c001_002.bmp 508 482 stage 1 blink 2000   # it pulses on the PC; the period isn't measured yet
startmenu 2
button start obj\c002_000.png 548 432 6 5 0 stage 2 help obj\c003_000.png 587 313 11 1 0 0
button load obj\c002_000.png 548 468 6 5 1 stage 2 help obj\c003_000.png 587 349 11 1 1 0
button submenu:bonus obj\c002_000.png 548 504 6 5 2 stage 2 help obj\c003_000.png 587 385 11 1 2 0
button config obj\c002_000.png 548 540 6 5 3 stage 2 help obj\c003_000.png 587 421 11 1 3 0
button websurvey obj\c002_000.png 548 576 6 5 4 stage 2 help obj\c003_000.png 587 457 11 1 4 0
button quit obj\c002_000.png 548 612 6 5 5 stage 2 help obj\c003_000.png 587 493 11 1 5 0
button start_prologue obj\c002_001.bmp 746 432 5 5 0 stage 2 group start help obj\c003_000.png 769 313 11 1 6 0
button start_main obj\c002_001.bmp 746 468 5 5 1 stage 2 group start help obj\c003_000.png 769 349 11 1 7 0
button cg obj\c002_001.bmp 746 504 5 5 2 stage 2 group bonus help obj\c003_000.png 769 385 11 1 8 0
button scene obj\c002_001.bmp 746 540 5 5 3 stage 2 group bonus help obj\c003_000.png 769 421 11 1 9 0
button music obj\c002_001.bmp 746 576 5 5 4 stage 2 group bonus help obj\c003_000.png 769 457 11 1 10 0

screen start
flow mainstory

screen load
pic obj\d000_000.bmp 0 0
pic obj\d005_000.bmp 1123 0                # the side bar; its place is a guess (translucent, not fitted)
pic obj\d001_001.bmp 0 18
slots obj\d003_000.bmp 60 99 510 119 5 2 1
slotrows 5
slotthumb 29 10
slotthumbframe obj\d004_001.bmp 27 8
slotdigits obj\d003_001.bmp 229 6 3 15
datedigits obj\d003_001.bmp 0 0
datefield year 251 28 15
datefield month 347 28 15 blank
datefield day 406 28 15 blank
datefield hour 347 50 15 blank
datefield minute 406 50 15 blank   # no minute under 10 seen yet, so its leading zero is a guess
slotvar obj\d003_002.bmp 350 6 5 S 5000
slotnew obj\d004_002.bmp -12 -2
button page0 obj\d002_000.bmp 379 25 10 5 0
button page1 obj\d002_000.bmp 441 25 10 5 1
button page2 obj\d002_000.bmp 503 25 10 5 2
button page3 obj\d002_000.bmp 565 25 10 5 3
button page4 obj\d002_000.bmp 627 25 10 5 4
button page5 obj\d002_000.bmp 689 25 10 5 5
button page6 obj\d002_000.bmp 751 25 10 5 6
button page7 obj\d002_000.bmp 813 25 10 5 7
button page8 obj\d002_000.bmp 875 25 10 5 8
button page9 obj\d002_000.bmp 937 25 10 5 9
button nav_save obj\d005_001.bmp 1178 132 9 5 0 help obj\d006_000.bmp 1058 107 8 1 0 0
button nav_load obj\d005_001.bmp 1178 202 9 5 1 help obj\d006_000.bmp 1058 177 8 1 1 0
button nav_cg obj\d005_001.bmp 1178 272 9 5 2 help obj\d006_000.bmp 1058 247 8 1 2 0
button nav_scene obj\d005_001.bmp 1178 342 9 5 3 help obj\d006_000.bmp 1058 317 8 1 3 0
button nav_music obj\d005_001.bmp 1178 412 9 5 4 help obj\d006_000.bmp 1058 387 8 1 4 0
button nav_config obj\d005_001.bmp 1178 482 9 5 5 help obj\d006_000.bmp 1058 457 8 1 5 0
button nav_title obj\d005_001.bmp 1178 552 9 5 6 help obj\d006_000.bmp 1058 527 8 1 6 0
button nav_quit obj\d005_001.bmp 1178 622 9 5 7 help obj\d006_000.bmp 1058 597 8 1 7 0
button back obj\d005_001.bmp 1178 31 9 5 8

screen save
pic obj\d000_000.bmp 0 0
pic obj\d005_000.bmp 1123 0                # the side bar; its place is a guess (translucent, not fitted)
pic obj\d001_000.bmp 0 18
slots obj\d003_000.bmp 60 99 510 119 5 2 1
slotrows 5
slotthumb 29 10
slotthumbframe obj\d004_001.bmp 27 8
slotdigits obj\d003_001.bmp 229 6 3 15
datedigits obj\d003_001.bmp 0 0
datefield year 251 28 15
datefield month 347 28 15 blank
datefield day 406 28 15 blank
datefield hour 347 50 15 blank
datefield minute 406 50 15 blank   # no minute under 10 seen yet, so its leading zero is a guess
slotvar obj\d003_002.bmp 350 6 5 S 5000
slotnew obj\d004_002.bmp -12 -2
button page0 obj\d002_000.bmp 379 25 10 5 0
button page1 obj\d002_000.bmp 441 25 10 5 1
button page2 obj\d002_000.bmp 503 25 10 5 2
button page3 obj\d002_000.bmp 565 25 10 5 3
button page4 obj\d002_000.bmp 627 25 10 5 4
button page5 obj\d002_000.bmp 689 25 10 5 5
button page6 obj\d002_000.bmp 751 25 10 5 6
button page7 obj\d002_000.bmp 813 25 10 5 7
button page8 obj\d002_000.bmp 875 25 10 5 8
button page9 obj\d002_000.bmp 937 25 10 5 9
button nav_save obj\d005_001.bmp 1178 132 9 5 0 help obj\d006_000.bmp 1058 107 8 1 0 0
button nav_load obj\d005_001.bmp 1178 202 9 5 1 help obj\d006_000.bmp 1058 177 8 1 1 0
button nav_cg obj\d005_001.bmp 1178 272 9 5 2 help obj\d006_000.bmp 1058 247 8 1 2 0
button nav_scene obj\d005_001.bmp 1178 342 9 5 3 help obj\d006_000.bmp 1058 317 8 1 3 0
button nav_music obj\d005_001.bmp 1178 412 9 5 4 help obj\d006_000.bmp 1058 387 8 1 4 0
button nav_config obj\d005_001.bmp 1178 482 9 5 5 help obj\d006_000.bmp 1058 457 8 1 5 0
button nav_title obj\d005_001.bmp 1178 552 9 5 6 help obj\d006_000.bmp 1058 527 8 1 6 0
button nav_quit obj\d005_001.bmp 1178 622 9 5 7 help obj\d006_000.bmp 1058 597 8 1 7 0
button back obj\d005_001.bmp 1178 31 9 5 8

screen cg
pic obj\d000_000.bmp 0 0
pic obj\d005_000.bmp 1123 0                # the side bar; its place is a guess (translucent, not fitted)
pic obj\e000_000.bmp 0 18
pic obj\d001_002.bmp 314 8
pic obj\e000_002.bmp 0 113
pic obj\e001_000.bmp 0 198 tab 0
pic obj\e001_001.bmp 0 198 tab 0
pic obj\e002_000.bmp 0 501 tab 0
pic obj\e002_001.bmp 0 501 tab 0
pic obj\e001_000.bmp 0 198 tab 1
pic obj\e001_002.bmp 0 198 tab 1
pic obj\e002_000.bmp 0 501 tab 1
pic obj\e002_002.bmp 0 501 tab 1
pic obj\e001_000.bmp 0 198 tab 2
pic obj\e001_003.bmp 0 198 tab 2
pic obj\e002_000.bmp 0 501 tab 2
pic obj\e002_003.bmp 0 501 tab 2
pic obj\e001_000.bmp 0 198 tab 3
pic obj\e001_004.bmp 0 198 tab 3
pic obj\e002_000.bmp 0 501 tab 3
pic obj\e002_004.bmp 0 501 tab 3
pic obj\e001_000.bmp 0 198 tab 4
pic obj\e001_005.bmp 0 198 tab 4
pic obj\e002_000.bmp 0 501 tab 4
pic obj\e002_005.bmp 0 501 tab 4
gallerytabs
button gtab0 obj\e003_000.bmp 130 146 5 5 0
button gtab1 obj\e003_000.bmp 204 239 5 5 1
button gtab2 obj\e003_000.bmp 226 358 5 5 2
button gtab3 obj\e003_000.bmp 204 475 5 5 3
button gtab4 obj\e003_000.bmp 130 568 5 5 4
cellcard obj\e004_000.bmp
cellsize 220 141
cellthumb 32 12
celldigits obj\d003_001.bmp 60 106 2 15
cellcount obj\d003_001.bmp obj\e005_001.bmp 114 106 145 158 109
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
button page0 obj\d002_000.bmp 379 25 10 5 0
button page1 obj\d002_000.bmp 441 25 10 5 1
button page2 obj\d002_000.bmp 503 25 10 5 2
button nav_save obj\d005_001.bmp 1178 132 9 5 0 help obj\d006_000.bmp 1058 107 8 1 0 0
button nav_load obj\d005_001.bmp 1178 202 9 5 1 help obj\d006_000.bmp 1058 177 8 1 1 0
button nav_cg obj\d005_001.bmp 1178 272 9 5 2 help obj\d006_000.bmp 1058 247 8 1 2 0
button nav_scene obj\d005_001.bmp 1178 342 9 5 3 help obj\d006_000.bmp 1058 317 8 1 3 0
button nav_music obj\d005_001.bmp 1178 412 9 5 4 help obj\d006_000.bmp 1058 387 8 1 4 0
button nav_config obj\d005_001.bmp 1178 482 9 5 5 help obj\d006_000.bmp 1058 457 8 1 5 0
button nav_title obj\d005_001.bmp 1178 552 9 5 6 help obj\d006_000.bmp 1058 527 8 1 6 0
button nav_quit obj\d005_001.bmp 1178 622 9 5 7 help obj\d006_000.bmp 1058 597 8 1 7 0
button back obj\d005_001.bmp 1178 31 9 5 8
viewer obj\g000_000.bmp 0 632
viewerbutton prevvar obj\g000_001.bmp 26 678 2 0 help 0
viewerbutton nextvar obj\g000_001.bmp 244 678 2 1 help 1
viewerbutton auto obj\g000_002.bmp 326 678 3 0 help 2
viewerbutton nextcg obj\g000_002.bmp 396 678 3 1 help 3
viewerbutton return obj\g000_002.bmp 498 678 3 2 help 4
viewerbutton hide obj\g000_003.bmp 582 642 help 5
viewerhelp obj\g002_000.bmp 10 561 6
viewernumber index obj\g001_000.bmp 116 678 24 2
viewernumber total obj\g001_001.bmp 182 684 20 2
viewerauto 8000 0

screen scene
pic obj\d000_000.bmp 0 0
pic obj\d005_000.bmp 1123 0                # the side bar; its place is a guess (translucent, not fitted)
pic obj\e000_001.bmp 0 18
pic obj\d001_002.bmp 314 8
pic obj\e000_002.bmp 0 113
pic obj\e001_000.bmp 0 198 tab 0
pic obj\e001_001.bmp 0 198 tab 0
pic obj\e002_000.bmp 0 501 tab 0
pic obj\e002_001.bmp 0 501 tab 0
pic obj\e001_000.bmp 0 198 tab 1
pic obj\e001_002.bmp 0 198 tab 1
pic obj\e002_000.bmp 0 501 tab 1
pic obj\e002_002.bmp 0 501 tab 1
pic obj\e001_000.bmp 0 198 tab 2
pic obj\e001_003.bmp 0 198 tab 2
pic obj\e002_000.bmp 0 501 tab 2
pic obj\e002_003.bmp 0 501 tab 2
pic obj\e001_000.bmp 0 198 tab 3
pic obj\e001_004.bmp 0 198 tab 3
pic obj\e002_000.bmp 0 501 tab 3
pic obj\e002_004.bmp 0 501 tab 3
pic obj\e001_000.bmp 0 198 tab 4
pic obj\e001_005.bmp 0 198 tab 4
pic obj\e002_000.bmp 0 501 tab 4
pic obj\e002_005.bmp 0 501 tab 4
gallerytabs
button gtab0 obj\e003_000.bmp 130 146 5 5 0
button gtab1 obj\e003_000.bmp 204 239 5 5 1
button gtab2 obj\e003_000.bmp 226 358 5 5 2
button gtab3 obj\e003_000.bmp 204 475 5 5 3
button gtab4 obj\e003_000.bmp 130 568 5 5 4
cellcard obj\e004_000.bmp
cellsize 220 141
cellthumb 32 12
celldigits obj\d003_001.bmp 60 106 2 15
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
button page0 obj\d002_000.bmp 379 25 10 5 0
button page1 obj\d002_000.bmp 441 25 10 5 1
button nav_save obj\d005_001.bmp 1178 132 9 5 0 help obj\d006_000.bmp 1058 107 8 1 0 0
button nav_load obj\d005_001.bmp 1178 202 9 5 1 help obj\d006_000.bmp 1058 177 8 1 1 0
button nav_cg obj\d005_001.bmp 1178 272 9 5 2 help obj\d006_000.bmp 1058 247 8 1 2 0
button nav_scene obj\d005_001.bmp 1178 342 9 5 3 help obj\d006_000.bmp 1058 317 8 1 3 0
button nav_music obj\d005_001.bmp 1178 412 9 5 4 help obj\d006_000.bmp 1058 387 8 1 4 0
button nav_config obj\d005_001.bmp 1178 482 9 5 5 help obj\d006_000.bmp 1058 457 8 1 5 0
button nav_title obj\d005_001.bmp 1178 552 9 5 6 help obj\d006_000.bmp 1058 527 8 1 6 0
button nav_quit obj\d005_001.bmp 1178 622 9 5 7 help obj\d006_000.bmp 1058 597 8 1 7 0
button back obj\d005_001.bmp 1178 31 9 5 8

screen music
pic obj\d000_000.bmp 0 0
pic obj\d005_000.bmp 1123 0                # the side bar; its place is a guess (translucent, not fitted)
pic obj\f000_000.bmp 0 18
pic obj\f001_000.bmp 177 112
button track0 obj\f002_000.bmp 257 201 16 5 0
button track1 obj\f002_000.bmp 467 201 16 5 1
button track2 obj\f002_000.bmp 677 201 16 5 2
button track3 obj\f002_000.bmp 257 241 16 5 3
button track4 obj\f002_000.bmp 467 241 16 5 4
button track5 obj\f002_000.bmp 677 241 16 5 5
button track6 obj\f002_000.bmp 257 281 16 5 6
button track7 obj\f002_000.bmp 467 281 16 5 7
button track8 obj\f002_000.bmp 677 281 16 5 8
button track9 obj\f002_000.bmp 257 321 16 5 9
button track10 obj\f002_000.bmp 467 321 16 5 10
button track11 obj\f002_000.bmp 677 321 16 5 11
button track12 obj\f002_000.bmp 257 361 16 5 12
button track13 obj\f002_000.bmp 467 361 16 5 13
button track14 obj\f002_000.bmp 677 361 16 5 14
button track15 obj\f002_000.bmp 257 401 16 5 15
button music_prev obj\f003_000.bmp 363 539 7 5 0 help obj\f005_000.bmp 337 422 8 1 0 0
button music_play obj\f003_000.bmp 421 539 7 5 1 help obj\f005_000.bmp 395 422 8 1 1 0
button music_pause obj\f003_000.bmp 479 539 7 5 2 help obj\f005_000.bmp 453 422 8 1 2 0
button music_stop obj\f003_000.bmp 537 539 7 5 3 help obj\f005_000.bmp 511 422 8 1 3 0
button music_next obj\f003_000.bmp 595 539 7 5 4 help obj\f005_000.bmp 569 422 8 1 4 0
button music_all obj\f003_000.bmp 653 539 7 5 5 help obj\f005_000.bmp 627 422 8 1 5 0
button music_random obj\f003_000.bmp 711 539 7 5 6 help obj\f005_000.bmp 685 422 8 1 6 0
slider music obj\f004_001.bmp 357 625
button nav_save obj\d005_001.bmp 1178 132 9 5 0 help obj\d006_000.bmp 1058 107 8 1 0 0
button nav_load obj\d005_001.bmp 1178 202 9 5 1 help obj\d006_000.bmp 1058 177 8 1 1 0
button nav_cg obj\d005_001.bmp 1178 272 9 5 2 help obj\d006_000.bmp 1058 247 8 1 2 0
button nav_scene obj\d005_001.bmp 1178 342 9 5 3 help obj\d006_000.bmp 1058 317 8 1 3 0
button nav_music obj\d005_001.bmp 1178 412 9 5 4 help obj\d006_000.bmp 1058 387 8 1 4 0
button nav_config obj\d005_001.bmp 1178 482 9 5 5 help obj\d006_000.bmp 1058 457 8 1 5 0
button nav_title obj\d005_001.bmp 1178 552 9 5 6 help obj\d006_000.bmp 1058 527 8 1 6 0
button nav_quit obj\d005_001.bmp 1178 622 9 5 7 help obj\d006_000.bmp 1058 597 8 1 7 0
button back obj\d005_001.bmp 1178 31 9 5 8

screen config
# One page, measured under Wine. Option cells are 101x24: row 0 off, 2 on, 4 disabled.
pic obj\d000_000.bmp 0 0
pic obj\d005_000.bmp 1123 0                # the side bar; its place is a guess (translucent, not fitted)
pic obj\i000_000.bmp 0 18
pic obj\i001_000.bmp 24 89
pic obj\h002_000.bmp 24 454
pic obj\h003_000.bmp 560 89
pic obj\h004_000.bmp 560 259
button cfg_defaults obj\i005_002.bmp 767 32 3 5 0 help obj\i005_004.bmp 752 86 3 1 0 0
button cfg_undo obj\i005_002.bmp 867 32 3 5 1 help obj\i005_004.bmp 852 86 3 1 1 0
button cfg_apply obj\i005_002.bmp 967 32 3 5 2 help obj\i005_004.bmp 952 86 3 1 2 0
button nav_save obj\d005_001.bmp 1178 132 9 5 0 help obj\d006_000.bmp 1058 107 8 1 0 0
button nav_load obj\d005_001.bmp 1178 202 9 5 1 help obj\d006_000.bmp 1058 177 8 1 1 0
button nav_cg obj\d005_001.bmp 1178 272 9 5 2 help obj\d006_000.bmp 1058 247 8 1 2 0
button nav_scene obj\d005_001.bmp 1178 342 9 5 3 help obj\d006_000.bmp 1058 317 8 1 3 0
button nav_music obj\d005_001.bmp 1178 412 9 5 4 help obj\d006_000.bmp 1058 387 8 1 4 0
button nav_config obj\d005_001.bmp 1178 482 9 5 5 help obj\d006_000.bmp 1058 457 8 1 5 0
button nav_title obj\d005_001.bmp 1178 552 9 5 6 help obj\d006_000.bmp 1058 527 8 1 6 0
button nav_quit obj\d005_001.bmp 1178 622 9 5 7 help obj\d006_000.bmp 1058 597 8 1 7 0
button back obj\d005_001.bmp 1178 31 9 5 8
radio screen 0 obj\h001_001.bmp 264 124 10 5 0
radio screen 1 obj\h001_001.bmp 381 124 10 5 1
# Screen mode (colour depth) and resolution do nothing here; drawn as the PC draws them.
pic obj\h001_001.bmp 264 152 10 5 2 0
pic obj\h001_001.bmp 381 152 10 5 3 0
pic obj\h001_001.bmp 216 180 10 5 4 2
pic obj\h001_001.bmp 323 180 10 5 5 0
pic obj\h001_001.bmp 430 180 10 5 6 0
pic obj\h001_001.bmp 216 208 10 5 7 4
pic obj\h001_001.bmp 323 208 10 5 8 4
pic obj\h001_001.bmp 430 208 10 5 9 4
radio effect 1 obj\h001_002.bmp 264 236 2 5 0
radio effect 0 obj\h001_002.bmp 381 236 2 5 1
radio wait 1 obj\h001_002.bmp 264 264 2 5 0
radio wait 0 obj\h001_002.bmp 381 264 2 5 1
pic obj\h001_005.bmp 88 321
slider opacity obj\h001_006.bmp 88 321
# The numbers are text, MS Gothic at 20 (Ume Gothic here) 15 apart, white with a purple edge, fitted to Wine's shots.
number opacity text 488 311 20 15 255 255 255 76 10 73
number bgm text 1024 313 20 15 255 255 255 76 10 73
number se text 1024 369 20 15 255 255 255 76 10 73
number voice text 1024 425 20 15 255 255 255 76 10 73
number backvoice text 1024 481 20 15 255 255 255 76 10 73
step opacity -1 obj\h001_003.bmp 60 317 2 5 0
step opacity 1 obj\h001_003.bmp 508 317 2 5 1
# One switch for every confirmation on the PC: `confirm` covers every question when the table names no separate ones.
radio confirm 1 obj\h001_002.bmp 264 348 2 5 0
radio confirm 0 obj\h001_002.bmp 381 348 2 5 1
radio tutorial 1 obj\h001_002.bmp 264 376 2 5 0
radio tutorial 0 obj\h001_002.bmp 381 376 2 5 1
pic obj\a001_003.bmp 101 403 1 5 0 0
radio skipall 0 obj\h003_001.bmp 800 124 9 5 0
radio skipall 1 obj\h003_001.bmp 917 124 9 5 1
radio rightclick 0 obj\h003_001.bmp 752 152 9 5 2
radio rightclick 1 obj\h003_001.bmp 859 152 9 5 3
radio rightclick 2 obj\h003_001.bmp 966 152 9 5 4
radio rightclick 3 obj\h003_001.bmp 800 180 9 5 5
radio rightclick 4 obj\h003_001.bmp 917 180 9 5 6
radio deckey 0 obj\h003_001.bmp 800 208 9 5 7
radio deckey 1 obj\h003_001.bmp 917 208 9 5 8
pic obj\h001_005.bmp 624 323
slider bgm obj\h001_006.bmp 624 323
step bgm -1 obj\h001_003.bmp 596 319 2 5 0
step bgm 1 obj\h001_003.bmp 1044 319 2 5 1
check mute_bgm obj\h004_003.bmp 949 292 1 5 0
pic obj\h001_005.bmp 624 379
slider se obj\h001_006.bmp 624 379
step se -1 obj\h001_003.bmp 596 375 2 5 0
step se 1 obj\h001_003.bmp 1044 375 2 5 1
check mute_se obj\h004_003.bmp 949 348 1 5 0
pic obj\h001_005.bmp 624 435
slider voice obj\h001_006.bmp 624 435
step voice -1 obj\h001_003.bmp 596 431 2 5 0
step voice 1 obj\h001_003.bmp 1044 431 2 5 1
check mute_voice obj\h004_003.bmp 949 404 1 5 0
pic obj\h001_005.bmp 624 491
slider backvoice obj\h001_006.bmp 624 491
step backvoice -1 obj\h001_003.bmp 596 487 2 5 0
step backvoice 1 obj\h001_003.bmp 1044 487 2 5 1
check mute_backvoice obj\h004_003.bmp 949 460 1 5 0
radio backvoice_on 1 obj\h001_002.bmp 800 518 2 5 0
radio backvoice_on 0 obj\h001_002.bmp 917 518 2 5 1
radio voice_stop 1 obj\h001_002.bmp 800 546 2 5 0
radio voice_stop 0 obj\h001_002.bmp 917 546 2 5 1
# Individual voices: the three switches and the button to the per-character screen, not measured yet; drawn as the PC has them.
pic obj\h004_001.bmp 752 574 4 5 0 0
pic obj\h004_001.bmp 859 574 4 5 1 0
pic obj\h004_001.bmp 966 574 4 5 2 0
pic obj\h004_001.bmp 752 602 4 5 3 0
# Text speed, auto speed and auto wait, measured under Wine (evidence speed82/): text speed 0-100 types at about
# max(0, 167 - 3 x value) ms a character, default 50; auto speed 0-100 waits about (100 - value) ms a character of the
# line before going on, default 50; auto wait (the PC's help: "length of delay in game scenes") runs x0 to x200, default
# x100, and makes each [ウエイト] wait last its time x value / 100 in manual play (0, 6.0 and 12.07 s for 6 s of waits);
# it doesn't change auto's pace, and the fades aren't stretched. The runtime's handling of these three is the lead's.
pic obj\h001_005.bmp 88 518
slider textspeed obj\h001_006.bmp 88 518
step textspeed -1 obj\h001_003.bmp 60 514 2 5 0
step textspeed 1 obj\h001_003.bmp 508 514 2 5 1
number textspeed text 488 507 20 15 255 255 255 76 10 73
pic obj\h001_005.bmp 88 574
slider autospeed obj\h001_006.bmp 88 574
step autospeed -1 obj\h001_003.bmp 60 570 2 5 0
step autospeed 1 obj\h001_003.bmp 508 570 2 5 1
number autospeed text 488 563 20 15 255 255 255 76 10 73
pic obj\h001_005.bmp 88 630
range autowait 0 200
slider autowait obj\h001_006.bmp 88 630 200
step autowait -1 obj\h001_003.bmp 60 626 2 5 0
step autowait 1 obj\h001_003.bmp 508 626 2 5 1
number autowait text 488 619 20 15 255 255 255 76 10 73
# Help: zones are each row's extent; their exact edges aren't measured yet (hov.py).
hover 24 124 536 147 obj\h001_008.bmp 50 10 8 0
hover 24 152 536 175 obj\h001_008.bmp 50 38 8 1
hover 24 180 536 231 obj\h001_008.bmp 50 68 8 2
hover 24 236 536 259 obj\h001_008.bmp 50 122 8 3
hover 24 264 536 287 obj\h001_008.bmp 50 150 8 4
hover 24 291 536 343 obj\h001_008.bmp 50 177 8 5
hover 24 348 536 371 obj\h001_008.bmp 50 234 8 6
hover 24 376 536 399 obj\h001_008.bmp 50 262 8 7
hover 24 487 536 543 obj\h002_001.bmp 50 373 3 0
hover 24 543 536 599 obj\h002_001.bmp 50 429 3 1
hover 24 599 536 655 obj\h002_001.bmp 50 485 3 2
hover 560 124 1096 147 obj\h003_002.bmp 586 10 3 0
hover 560 152 1096 203 obj\h003_002.bmp 586 38 3 1
hover 560 208 1096 231 obj\h003_002.bmp 586 94 3 2
hover 560 292 1096 347 obj\h004_002.bmp 586 178 7 0
hover 560 348 1096 403 obj\h004_002.bmp 586 235 7 1
hover 560 404 1096 459 obj\h004_002.bmp 586 291 7 2
hover 560 460 1096 513 obj\h004_002.bmp 586 347 7 3
hover 560 518 1096 541 obj\h004_002.bmp 586 404 7 4
hover 560 546 1096 569 obj\h004_002.bmp 586 432 7 5
hover 560 574 1096 625 obj\h004_002.bmp 586 459 7 6

# The staff rolls its exe draws, one for each ending with that ending's four pictures, measured under Wine on Aisis's
# (frames every quarter second): white fading out over 2.2 s from 0.7, the dark column with the logo fading in from 3 s,
# the credits scrolling up from the bottom at 60 pixels a second from 7.79 s, a picture changing every 11.9 s with a
# 0.75 s crossfade, the whole screen crossfading to B100_000 at 55 s, Fin at 59.3 s, and white again from 70.1 s, with M01.
# The other four rolls are assumed to keep Aisis's timing; their pictures are the ones each ending read.
screen staffroll_ai
length 71800
music bgm\M01-m.ogg
pic obj\n001_001.bmp 0 0
pic obj\n000_008.bmp 401 0
pic obj\n000_009.bmp 401 0 fadein 14600 750
pic obj\n000_010.bmp 401 0 fadein 26500 750
pic obj\n000_011.bmp 401 0 fadein 38100 750
pic obj\n002_000.bmp 81 240 move 7790 60 fadein 3000 3000
pic obj\n002_001.bmp 22 720 move 7790 60
pic obj\n002_002.bmp 22 1382 move 7790 60
pic obj\n002_003.bmp 22 1733 move 7790 60
pic obj\n002_004.bmp 22 2171 move 7790 60
pic bg\B100_000.bmp 0 0 fadein 55000 750
pic obj\n001_000.bmp 1008 586 fadein 59300 1000
pic bg\bg_w.bmp 0 0 fadeout 700 2200
pic bg\bg_w.bmp 0 0 fadein 70100 1700

screen staffroll_ss
length 71800
music bgm\M01-m.ogg
pic obj\n001_001.bmp 0 0
pic obj\n000_000.bmp 401 0
pic obj\n000_001.bmp 401 0 fadein 14600 750
pic obj\n000_002.bmp 401 0 fadein 26500 750
pic obj\n000_003.bmp 401 0 fadein 38100 750
pic obj\n002_000.bmp 81 240 move 7790 60 fadein 3000 3000
pic obj\n002_001.bmp 22 720 move 7790 60
pic obj\n002_002.bmp 22 1382 move 7790 60
pic obj\n002_003.bmp 22 1733 move 7790 60
pic obj\n002_004.bmp 22 2171 move 7790 60
pic bg\B100_000.bmp 0 0 fadein 55000 750
pic obj\n001_000.bmp 1008 586 fadein 59300 1000
pic bg\bg_w.bmp 0 0 fadeout 700 2200
pic bg\bg_w.bmp 0 0 fadein 70100 1700

screen staffroll_lc
length 71800
music bgm\M01-m.ogg
pic obj\n001_001.bmp 0 0
pic obj\n000_004.bmp 401 0
pic obj\n000_005.bmp 401 0 fadein 14600 750
pic obj\n000_006.bmp 401 0 fadein 26500 750
pic obj\n000_007.bmp 401 0 fadein 38100 750
pic obj\n002_000.bmp 81 240 move 7790 60 fadein 3000 3000
pic obj\n002_001.bmp 22 720 move 7790 60
pic obj\n002_002.bmp 22 1382 move 7790 60
pic obj\n002_003.bmp 22 1733 move 7790 60
pic obj\n002_004.bmp 22 2171 move 7790 60
pic bg\B100_000.bmp 0 0 fadein 55000 750
pic obj\n001_000.bmp 1008 586 fadein 59300 1000
pic bg\bg_w.bmp 0 0 fadeout 700 2200
pic bg\bg_w.bmp 0 0 fadein 70100 1700

screen staffroll_rk
length 71800
music bgm\M01-m.ogg
pic obj\n001_001.bmp 0 0
pic obj\n000_012.bmp 401 0
pic obj\n000_013.bmp 401 0 fadein 14600 750
pic obj\n000_014.bmp 401 0 fadein 26500 750
pic obj\n000_015.bmp 401 0 fadein 38100 750
pic obj\n002_000.bmp 81 240 move 7790 60 fadein 3000 3000
pic obj\n002_001.bmp 22 720 move 7790 60
pic obj\n002_002.bmp 22 1382 move 7790 60
pic obj\n002_003.bmp 22 1733 move 7790 60
pic obj\n002_004.bmp 22 2171 move 7790 60
pic bg\B100_000.bmp 0 0 fadein 55000 750
pic obj\n001_000.bmp 1008 586 fadein 59300 1000
pic bg\bg_w.bmp 0 0 fadeout 700 2200
pic bg\bg_w.bmp 0 0 fadein 70100 1700

screen staffroll_zh
length 71800
music bgm\M01-m.ogg
pic obj\n001_001.bmp 0 0
pic obj\n000_016.bmp 401 0
pic obj\n000_017.bmp 401 0 fadein 14600 750
pic obj\n000_018.bmp 401 0 fadein 26500 750
pic obj\n000_019.bmp 401 0 fadein 38100 750
pic obj\n002_000.bmp 81 240 move 7790 60 fadein 3000 3000
pic obj\n002_001.bmp 22 720 move 7790 60
pic obj\n002_002.bmp 22 1382 move 7790 60
pic obj\n002_003.bmp 22 1733 move 7790 60
pic obj\n002_004.bmp 22 2171 move 7790 60
pic bg\B100_000.bmp 0 0 fadein 55000 750
pic obj\n001_000.bmp 1008 586 fadein 59300 1000
pic bg\bg_w.bmp 0 0 fadeout 700 2200
pic bg\bg_w.bmp 0 0 fadein 70100 1700

screen defaults
layoutcgslot 102             # 82's saves hold a CG in slot 102, and a background and a CG each take the other away
default opacity 100          # the PC's message window is solid until Config changes it
default textspeed 50
default autospeed 50
default autowait 100
default backvoice_on 1
# The text asks for Verdana (face 8, DejaVu Sans here, chosen by the lead against real Verdana). Size 19 and gap 0.64 are
# Verdana-at-24's widths plus the PC's own gap, 0.2 over its face's advances, measured under Wine; lines 30 apart, the
# first baseline at 588 and the ink from x 213 as there.
textfont 8
textsize 19
textgap 0.64
textline 30
textshift 1 0                 # ink from x 213 and the first baseline at 588, as under Wine (rendered and checked)

screen choice
# Measured under Wine to the pixel: the dialogs' frame, "Choose an option" from the titles' strip, and the options from
# m000_000's strip of all sixteen, two or three of them from 212 down, 56 apart, whatever their count.
pic obj\j000_000.bmp 438 144
pic obj\j001_000.bmp 542 124 7 1 6 0
place 212 56
choicecolumns 16
close 280 0.42            # from frames under Wine: about 75% size and half faded 0.12 s after the press

screen story
# Story order, traced from the PC under Wine: the scene scripts it opened, in order, through every route, and saves at
# every choice for the variables. The options are cells of obj\m000_000.bmp, a sheet of all sixteen; their text is noted.
# S200 counts Roxanne's points. S1000 is set by the Royal Guard, and nothing seen reads it. S5000 is the chapter
# a save card names (d003_002's cells: 0 Prologue, 1 Boobs of Boan, 2 A Treasonous Affair, 3 Complications in the
# Capital, 4 The Depths of Desire).
flow main                    # New Game, and its Prologue once an ending is seen
scene scr\ep_00_00.txt
jump mainstory
flow mainstory               # New Game > Main Story, offered once an ending is seen
set S 5000 = 1               # Boobs of Boan: 0 (Prologue) in ep_00_00's saves, 1 in ep_00_03a's
scene scr\ep_00_01.txt
scene scr\ep_00_02.txt
scene scr\ep_00_03.txt
scene scr\ep_00_03a.txt
scene scr\ep_00_04.txt
scene scr\ep_00_04a.txt
scene scr\ep_00_05.txt
scene scr\ep_00_06.txt
scene scr\ep_00_06a.txt
scene scr\ep_00_07.txt
scene scr\ep_00_07a.txt
scene scr\ep_00_08.txt
choice obj\m000_000.bmp 2 cells 0 1  # Cum inside Roxanne / Pull out of Roxanne
on 1 goto c1b
set S 200 += 1
scene scr\ep_00_08a.txt
goto c1
label c1b
scene scr\ep_00_08b.txt
label c1
scene scr\ep_00_08c.txt
choice obj\m000_000.bmp 2 cells 2 3  # Shamsiel's tit-fuck feels better! / Roxanne's pussy feels better!
on 1 goto c2b
scene scr\ep_00_08d.txt
goto c2
label c2b
scene scr\ep_00_08e.txt
label c2
scene scr\ep_00_08f.txt
scene scr\ep_00_09.txt
choice obj\m000_000.bmp 2 cells 0 1  # Cum inside Roxanne / Pull out of Roxanne
on 1 goto c3b
set S 200 += 1
scene scr\ep_00_09a.txt
goto c3
label c3b
scene scr\ep_00_09b.txt
label c3
scene scr\ep_00_09c.txt
scene scr\ep_00_10.txt
choice obj\m000_000.bmp 2 cells 0 1  # Cum inside Roxanne / Pull out of Roxanne
on 1 goto c4b
set S 200 += 1
scene scr\ep_00_10a.txt
goto c4
label c4b
scene scr\ep_00_10b.txt
label c4
scene scr\ep_00_10c.txt
scene scr\ep_00_11.txt
if S 200 >= 3 goto c5a     # inside all three times: 11a and its choice; under 3 (checked at 1 and 2), straight on to 11d:
goto c5                      # the PC opens 11b's file there but plays none of it (79 lines from 11's end landed in 11d)
label c5a
scene scr\ep_00_11a.txt
choice obj\m000_000.bmp 2 cells 4 5  # I promise to return after I reach the capital! / I'll never forget you, Roxanne.
on 1 goto c5b
set S 200 += 2
scene scr\ep_00_11c.txt
goto c5
label c5b
scene scr\ep_00_11b.txt
label c5
scene scr\ep_00_11d.txt
scene scr\ep_00_12.txt
if S 200 >= 5 goto route3    # Boan Castle is offered only with S200 at 5 (checked: not at 2 or 3)
choice obj\m000_000.bmp 2 cells 6 7  # Be assigned to the Royal Guard / Be stationed as a commander
on 1 goto commander
goto guard
label route3
choice obj\m000_000.bmp 3 cells 6 7 8  # Be assigned to the Royal Guard / Be stationed as a commander / Return to Boan Castle
on 1 goto commander
on 2 goto boan
label guard
set S 1000 = 1
scene scr\ep_00_12a.txt
scene scr\ep_00_12c.txt      # also on the commander's way: told apart by S1000 (1 here, 0 there)
set S 5000 = 4               # The Depths of Desire, in saves at ep_02_07
scene scr\ep_02_00.txt
scene scr\ep_02_01.txt
scene scr\ep_02_02.txt
scene scr\ep_02_03.txt
scene scr\ep_02_03a.txt
scene scr\ep_02_04.txt
scene scr\ep_02_05.txt
scene scr\ep_02_06.txt
scene scr\ep_02_07.txt
choice obj\m000_000.bmp 2 cells 9 10  # Search for the sanctum / Return to the capital
on 1 goto capital
scene scr\ep_02_07c.txt
scene scr\ep_02_08.txt
scene scr\ep_02_09.txt
scene scr\ep_02_10.txt
scene scr\ep_ai_00.txt       # Aisis's ending
staffroll ai
scene scr\ep_ai_01.txt
set G 2 = 1                  # an ending seen: New Game offers Prologue and Main Story (checked under Wine)
end
label capital
scene scr\ep_02_07a.txt
scene scr\ep_02_07b.txt
scene scr\ep_ss_00.txt       # Shamsiel's ending
staffroll ss
scene scr\ep_ss_01.txt
set G 2 = 1                  # an ending seen: New Game offers Prologue and Main Story (checked under Wine)
end
label commander
scene scr\ep_00_12b.txt
scene scr\ep_00_12c.txt      # also on the Royal Guard's way: told apart by S1000 (0 here)
set S 5000 = 2               # A Treasonous Affair: the one title left for ep_01
scene scr\ep_01_00.txt
scene scr\ep_01_01.txt
scene scr\ep_01_01a.txt
scene scr\ep_01_02.txt
scene scr\ep_01_02a.txt
scene scr\ep_01_03.txt
scene scr\ep_01_04.txt
scene scr\ep_01_04a.txt
scene scr\ep_01_05.txt
scene scr\ep_01_06.txt
scene scr\ep_01_07.txt
scene scr\ep_01_07a.txt
scene scr\ep_01_08.txt
scene scr\ep_01_09.txt
scene scr\ep_01_10.txt
set S 5000 = 3               # Complications in the Capital, in saves at ep_03_10
scene scr\ep_03_00.txt
scene scr\ep_03_01.txt
scene scr\ep_03_02.txt
scene scr\ep_03_02a.txt
scene scr\ep_03_03.txt
scene scr\ep_03_04.txt
scene scr\ep_03_05.txt
scene scr\ep_03_06.txt
scene scr\ep_03_07.txt
scene scr\ep_03_08.txt
scene scr\ep_03_09.txt
scene scr\ep_03_09a.txt
scene scr\ep_03_10.txt
choice obj\m000_000.bmp 2 cells 11 12  # Squeeze her boobs / Suck on her boobs
on 1 goto c8b
scene scr\ep_03_10a.txt
goto c8
label c8b
scene scr\ep_03_10b.txt
label c8
scene scr\ep_03_10c.txt
choice obj\m000_000.bmp 2 cells 13 14  # Class has finished / Call in an additional teacher
on 1 goto harem
scene scr\ep_lc_00.txt       # Lucelia's ending
staffroll lc
scene scr\ep_lc_01.txt
set G 2 = 1                  # an ending seen: New Game offers Prologue and Main Story (checked under Wine)
end
label harem
scene scr\ep_03_10d.txt
scene scr\ep_03_11.txt
scene scr\ep_03_11a.txt
scene scr\ep_03_12.txt
scene scr\ep_03_13.txt
scene scr\ep_03_14.txt
staffroll zh                 # the harem ending's roll comes before its first scene
scene scr\ep_zh_00.txt
choice obj\m000_000.bmp 3 cells 15 15 15  # Let nature take its course / Let nature take its course / Let nature take its course
scene scr\ep_zh_01.txt       # every option goes on here
scene scr\ep_zh_02.txt
scene scr\ep_zh_03.txt
set G 2 = 1                  # an ending seen: New Game offers Prologue and Main Story (checked under Wine)
end
label boan
scene scr\ep_rk_00.txt       # Roxanne's ending
staffroll rk
scene scr\ep_rk_01.txt
set G 2 = 1                  # an ending seen: New Game offers Prologue and Main Story (checked under Wine)
end
)";
