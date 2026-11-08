// Test Game 92's menus, a raw string that tables.cpp includes and menuTable() hands out.
// Test Game 92's menus start from Ore o Hoshigaru Tsuma to Gibo's, the same template, and are fitted to the PC game's
// screens one by one.
const char kAniMother[] = R"(
screen title
pic sys_5\obj\d000_000.bmp 0 -110
pic sys_5\obj\d002_000.bmp 0 558
pic sys_5\obj\d001_000.bmp 111 2
button versioninfo obj\d004_000.bmp 1122 7 1 5 0 help sys_5\obj\d005_000.bmp 0 686 1 12 0 11
# The manual and the web survey open in a browser on the PC; here the manual opens in the app and the survey only after
# asking.
button manual sys_5\obj\d003_000.bmp 885 592 11 5 8 help sys_5\obj\d005_000.bmp 0 686 1 12 0 7
button websurvey sys_5\obj\d003_000.bmp 1000 638 11 5 10 help sys_5\obj\d005_000.bmp 0 686 1 12 0 10
button anime sys_5\obj\d003_000.bmp 433 638 11 5 5 help sys_5\obj\d005_000.bmp 0 686 1 12 0 5
button start sys_5\obj\d003_000.bmp 129 592 11 5 0 help sys_5\obj\d005_000.bmp 0 686 1 12 0 0
button load sys_5\obj\d003_000.bmp 318 592 11 5 1 help sys_5\obj\d005_000.bmp 0 686 1 12 0 1
button quickload sys_5\obj\d003_000.bmp 507 592 11 5 2 help sys_5\obj\d005_000.bmp 0 686 1 12 0 2
button config sys_5\obj\d003_000.bmp 696 592 11 5 7 help sys_5\obj\d005_000.bmp 0 686 1 12 0 8
button quit sys_5\obj\d003_000.bmp 811 638 11 5 9 help sys_5\obj\d005_000.bmp 0 686 1 12 0 9
button cg sys_5\obj\d003_000.bmp 55 638 11 5 3 help sys_5\obj\d005_000.bmp 0 686 1 12 0 3
button scene sys_5\obj\d003_000.bmp 244 638 11 5 4 help sys_5\obj\d005_000.bmp 0 686 1 12 0 4
button music sys_5\obj\d003_000.bmp 622 638 11 5 6 help sys_5\obj\d005_000.bmp 0 686 1 12 0 6

screen load
helplast
selectedlook
pic obj\e000_000.bmp 0 0
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
pic obj\e002_000.bmp 0 0 1 2 0 1
lockdim
slots obj\e007_000.bmp 26 106 546 134 4 2
slotlabel obj\e008_000.bmp 3 9
slotdigits obj\e008_001.bmp 33 4
slotthumb 8 11
datedigits obj\e009_000.bmp 193 58
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
slotlock obj\e010_000.bmp 446 52
slotdelete obj\e011_000.bmp 473 52
emptycards 80
dategap 13
button page0 obj\e006_000.bmp 282 34 10 5 0
button page1 obj\e006_000.bmp 332 34 10 5 1
button page2 obj\e006_000.bmp 382 34 10 5 2
button page3 obj\e006_000.bmp 432 34 10 5 3
button page4 obj\e006_000.bmp 482 34 10 5 4
button page5 obj\e006_000.bmp 532 34 10 5 5
button page6 obj\e006_000.bmp 582 34 10 5 6
button page7 obj\e006_000.bmp 632 34 10 5 7
button page8 obj\e006_000.bmp 682 34 10 5 8
button page9 obj\e006_000.bmp 732 34 10 5 9
button page10 obj\e006_001.bmp 782 34 1 5 0
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9

screen save
helplast
selectedlook
pic obj\e000_000.bmp 0 0
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
pic obj\e002_000.bmp 0 0 1 2 0 0
lockdim
slots obj\e007_000.bmp 26 106 546 134 4 2
slotlabel obj\e008_000.bmp 3 9
slotdigits obj\e008_001.bmp 33 4
slotthumb 8 11
datedigits obj\e009_000.bmp 193 58
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
slotlock obj\e010_000.bmp 446 52
slotdelete obj\e011_000.bmp 473 52
dategap 13
button page0 obj\e006_000.bmp 282 34 10 5 0
button page1 obj\e006_000.bmp 332 34 10 5 1
button page2 obj\e006_000.bmp 382 34 10 5 2
button page3 obj\e006_000.bmp 432 34 10 5 3
button page4 obj\e006_000.bmp 482 34 10 5 4
button page5 obj\e006_000.bmp 532 34 10 5 5
button page6 obj\e006_000.bmp 582 34 10 5 6
button page7 obj\e006_000.bmp 632 34 10 5 7
button page8 obj\e006_000.bmp 682 34 10 5 8
button page9 obj\e006_000.bmp 732 34 10 5 9
button page10 obj\e006_001.bmp 782 34 1 5 0
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9

screen cg
helplast
selectedlook
# The CG viewer's panel, measured under Wine.
viewer obj\j000_000.bmp 0 558
viewerhelp obj\j008_000.bmp 830 679 13
viewerbutton bgmplay obj\j005_000.bmp 246 616
viewerbutton bgmmute obj\h024_000.bmp 292 616
viewerslider obj\j004_000.bmp 155 641 12
viewernumber volume obj\g005_000.bmp 194 617 14 3
viewerbutton zoomout obj\j003_000.bmp 388 644 3 0 help 9
viewerbutton zoomreset obj\j003_000.bmp 424 644 3 1 help 10
viewerbutton zoomin obj\j003_000.bmp 460 644 3 2 help 11
viewernumber zoom obj\j002_000.bmp 421 611 13 1
viewernumber zoomfrac obj\j002_001.bmp 444 613 11 2
viewerbutton set:hair sys_5\obj\j009_000.bmp 544 639 2 0 helppic obj\h004_004.bmp 400 679 2 0
viewerlook set:hair 0 0 1 2
viewerbutton set:hero sys_5\obj\j009_000.bmp 640 639 2 0 helppic obj\h004_004.bmp 400 679 2 1
viewerlook set:hero 0 0 1 2
viewerbutton replay obj\j001_000.bmp 721 643 3 1 help 2
viewerbutton prevcg obj\j001_001.bmp 808 641 5 0 help 4
viewerbutton prevvar obj\j001_001.bmp 863 641 5 1 help 5
viewerbutton nextvar obj\j001_001.bmp 1008 641 5 2 help 6
viewerbutton nextcg obj\j001_001.bmp 1063 641 5 3 help 7
viewerbutton auto obj\j001_001.bmp 1123 641 5 4 help 8
viewerbutton return obj\j001_000.bmp 1182 643 3 2 help 3
viewerbutton hide obj\l001_000.bmp 1240 588 2 0 help 0
viewernumber index obj\j002_000.bmp 926 630 13 2
viewernumber total obj\j002_001.bmp 966 648 11 2
viewerauto 8000 0
# The galleries' room, and each tab's two mothers, standing sprites the exe reads from sc.pak (traced under Wine),
# 73 pixels further left a tab.
pic obj\f000_001.bmp 0 0
pic sc\sc_001_002_000_001_002.bmp -324 -58 tab 0
pic sc\sc_000_002_000_001_005.bmp 617 -65 tab 0
pic sc\sc_001_002_000_001_000.bmp -397 -58 tab 1
pic sc\sc_000_002_000_001_000.bmp 544 -65 tab 1
pic sc\sc_001_002_000_001_005.bmp -470 -58 tab 2
pic sc\sc_000_002_000_001_002.bmp 471 -65 tab 2
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
pic sys_5\obj\f001_000.bmp 0 0 1 3 0 0
cellhide
cellsheet obj\f003_000.bmp
cellsize 186 156
cellthumb 10 29
cell 279 82
cell 478 82
cell 677 82
cell 279 245
cell 478 245
cell 677 245
cell 279 408
cell 478 408
cell 677 408
celllabel obj\e008_000.bmp -2 8
celldigits obj\e008_001.bmp 28 3 3 18
cellcount obj\f004_000.bmp obj\f004_001.bmp 100 125 128 141
gallerytabs 1
cellshift 0 80
cellshift 2 -80
button page0 obj\e006_000.bmp 282 34 10 5 0
button gtab0 obj\f002_000.bmp 8 654 3 5 0
button gtab1 obj\f002_000.bmp 384 654 3 5 1
button gtab2 obj\f002_000.bmp 760 654 3 5 2
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9

screen anime
helplast
selectedlook
# The anime gallery: entries of movies from anime_view.ini, which play looping in the CG viewer's panel without its art
# switches.
viewer obj\j000_000.bmp 0 558
viewerhelp obj\j008_000.bmp 830 679 13
viewerbutton bgmplay obj\j005_000.bmp 246 616
viewerbutton bgmmute obj\h024_000.bmp 292 616
viewerslider obj\j004_000.bmp 155 641 12
viewernumber volume obj\g005_000.bmp 194 617 14 3
viewerbutton zoomout obj\j003_000.bmp 388 644 3 0 help 9
viewerbutton zoomreset obj\j003_000.bmp 424 644 3 1 help 10
viewerbutton zoomin obj\j003_000.bmp 460 644 3 2 help 11
viewernumber zoom obj\j002_000.bmp 421 611 13 1
viewernumber zoomfrac obj\j002_001.bmp 444 613 11 2
viewerbutton replay obj\j001_000.bmp 721 643 3 1 help 2
viewerbutton prevcg obj\j001_001.bmp 808 641 5 0 help 4
viewerbutton prevvar obj\j001_001.bmp 863 641 5 1 help 5
viewerbutton nextvar obj\j001_001.bmp 1008 641 5 2 help 6
viewerbutton nextcg obj\j001_001.bmp 1063 641 5 3 help 7
viewerbutton auto obj\j001_001.bmp 1123 641 5 4 help 8
viewerbutton return obj\j001_000.bmp 1182 643 3 2 help 3
viewerbutton hide obj\l001_000.bmp 1240 588 2 0 help 0
viewernumber index obj\j002_000.bmp 926 630 13 2
viewernumber total obj\j002_001.bmp 966 648 11 2
viewerauto 8000 0
# The galleries' room, and each tab's two mothers, standing sprites the exe reads from sc.pak (traced under Wine),
# 73 pixels further left a tab.
pic obj\f000_001.bmp 0 0
pic sc\sc_001_002_000_001_002.bmp -324 -58 tab 0
pic sc\sc_000_002_000_001_005.bmp 617 -65 tab 0
pic sc\sc_001_002_000_001_000.bmp -397 -58 tab 1
pic sc\sc_000_002_000_001_000.bmp 544 -65 tab 1
pic sc\sc_001_002_000_001_005.bmp -470 -58 tab 2
pic sc\sc_000_002_000_001_002.bmp 471 -65 tab 2
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
pic sys_5\obj\f001_000.bmp 0 0 1 3 0 2
cellhide
cellsheet obj\f003_000.bmp
cellsize 186 156
cellthumb 10 29
cell 279 82
cell 478 82
cell 677 82
cell 279 245
cell 478 245
cell 677 245
cell 279 408
cell 478 408
cell 677 408
celllabel obj\e008_000.bmp -2 8
celldigits obj\e008_001.bmp 28 3 3 18
gallerytabs 1
cellshift 0 80
cellshift 2 -80
button page0 obj\e006_000.bmp 282 34 10 5 0
button gtab0 obj\f002_000.bmp 8 654 3 5 0
button gtab1 obj\f002_000.bmp 384 654 3 5 1
button gtab2 obj\f002_000.bmp 760 654 3 5 2
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9

screen scene
helplast
selectedlook
# The galleries' room, and each tab's two mothers, standing sprites the exe reads from sc.pak (traced under Wine),
# 73 pixels further left a tab.
pic obj\f000_001.bmp 0 0
pic sc\sc_001_002_000_001_002.bmp -324 -58 tab 0
pic sc\sc_000_002_000_001_005.bmp 617 -65 tab 0
pic sc\sc_001_002_000_001_000.bmp -397 -58 tab 1
pic sc\sc_000_002_000_001_000.bmp 544 -65 tab 1
pic sc\sc_001_002_000_001_005.bmp -470 -58 tab 2
pic sc\sc_000_002_000_001_002.bmp 471 -65 tab 2
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
pic sys_5\obj\f001_000.bmp 0 0 1 3 0 1
cellhide
cellsheet obj\f003_000.bmp
cellsize 186 156
cellthumb 10 29
cell 279 82
cell 478 82
cell 677 82
cell 279 245
cell 478 245
cell 677 245
cell 279 408
cell 478 408
cell 677 408
celllabel obj\e008_000.bmp -2 8
celldigits obj\e008_001.bmp 28 3 3 18
cellcount obj\f004_000.bmp obj\f004_001.bmp 100 125 128 141
gallerytabs 1
cellshift 0 80
cellshift 2 -80
button page0 obj\e006_000.bmp 282 34 10 5 0
button gtab0 obj\f002_000.bmp 8 654 3 5 0
button gtab1 obj\f002_000.bmp 384 654 3 5 1
button gtab2 obj\f002_000.bmp 760 654 3 5 2
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9

screen music
helplast
selectedlook
hover 343 676 534 708 obj\g006_000.bmp 868 666 8 7
pic obj\g000_000.bmp 0 0
# Line art of the mothers pans down behind the list, each sketch in turn, from a random one (measured under Wine).
drift obj\g000_001.bmp -170 -791 -17
drift obj\g000_002.bmp -300 -1035 -16
drift obj\g000_005.bmp 211 -1098 -135
drift obj\g000_003.bmp -32 -815 -18
drift obj\g000_006.bmp 145 -1022 -25
driftmotion 30 2250 1000 120
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
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
button track0 obj\g002_000.bmp 84 164 8 5 0
button track1 obj\g002_000.bmp 84 267 8 5 1
button track2 obj\g002_000.bmp 84 370 8 5 2
button track3 obj\g002_000.bmp 84 473 8 5 3
button track4 obj\g002_000.bmp 599 164 8 5 4
button track5 obj\g002_000.bmp 599 267 8 5 5
button track6 obj\g002_000.bmp 599 370 8 5 6
button track7 obj\g002_000.bmp 599 473 8 5 7
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9

screen config
helplast
selectedlook
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 587 352 804 402 obj\h004_000.bmp 350 669 9 7
hover 399 204 559 261 obj\h004_000.bmp 350 669 9 4
hover 47 204 210 261 obj\h004_000.bmp 350 669 9 2
hover 222 204 386 261 obj\h004_000.bmp 350 669 9 3
hover 47 88 559 117 obj\h004_000.bmp 350 669 9 0
hover 47 130 559 197 obj\h004_000.bmp 350 669 9 1
hover 587 416 1098 632 obj\h004_000.bmp 350 669 9 8
hover 587 90 1098 344 obj\h004_000.bmp 350 669 9 6
hover 47 271 559 600 obj\h004_000.bmp 350 669 9 5
pic obj\h000_000.bmp 0 0
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\h005_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 413 25 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 544 25 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 816 33 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 910 33 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 1004 33 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9
radio screen 0 obj\h006_000.bmp 172 91 2 5 0
radio screen 1 obj\h006_000.bmp 294 91 2 5 1
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
radio camera 1 obj\h008_000.bmp 615 377 2 5 0
radio camera 0 obj\h008_000.bmp 696 377 2 5 1
# How CGs are drawn: the game's own detailed art, or Waffle's earlier style, from change_texture_list.ini.
radio express 1 obj\h014_000.bmp 656 595 2 5 0
radio express 0 obj\h014_000.bmp 913 595 2 5 1
slider opacity obj\h009_000.bmp 155 307
number opacity obj\h011_000.bmp 415 279
step opacity -1 obj\h010_000.bmp 121 311 2 5 0
step opacity 1 obj\h010_000.bmp 471 311 2 5 1
check confirm_saveload obj\h013_000.bmp 603 117 8 5 0
check confirm_overwrite obj\h013_000.bmp 603 145 8 5 1
check confirm_delete obj\h013_000.bmp 603 173 8 5 2
check confirm_quick obj\h013_000.bmp 603 201 8 5 3
check confirm_title obj\h013_000.bmp 603 229 8 5 4
check confirm_exit obj\h013_000.bmp 603 257 8 5 5
check tutorial obj\h013_000.bmp 603 285 8 5 6
pic obj\h013_000.bmp 603 313 8 5 7 4

screen config2
helplast
selectedlook
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 584 290 804 347 sys_5\obj\h004_002.bmp 362 669 8 5
hover 584 88 803 144 sys_5\obj\h004_002.bmp 362 669 8 0
hover 812 88 1033 144 sys_5\obj\h004_002.bmp 362 669 8 1
hover 584 226 803 282 sys_5\obj\h004_002.bmp 362 669 8 3
hover 812 226 1033 283 sys_5\obj\h004_002.bmp 362 669 8 4
hover 48 94 560 157 obj\h004_001.bmp 380 669 3 0
hover 584 153 1099 210 sys_5\obj\h004_002.bmp 362 669 8 2
hover 48 167 560 238 obj\h004_001.bmp 380 669 3 1
hover 48 533 560 602 obj\h004_001.bmp 380 669 3 2
pic obj\h000_000.bmp 0 0
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic sys_5\obj\h015_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 413 25 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 544 25 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9
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
# Switching some H scenes' CGs to animation: on the PC under Wine neither the first movie scene, from its start, nor a
# flipbook changed with it, so it's kept and changes nothing here either.
radio cg_anim 1 obj\h008_000.bmp 617 378 2 5 0
radio cg_anim 0 obj\h008_000.bmp 698 378 2 5 1
# Moving the animation's camera by hand: a movie zooms with the wheel and the panel, and follows the pointer.
radio anim_manual 1 obj\h008_000.bmp 845 378 2 5 0
radio anim_manual 0 obj\h008_000.bmp 926 378 2 5 1
hover 584 350 803 411 sys_5\obj\h004_002.bmp 362 669 8 6
hover 812 350 1033 411 sys_5\obj\h004_002.bmp 362 669 8 7
# Wait time, ×0.70 to ×1.30 in steps of 0.01, stretches waits and fades; the bar is full at ×2.00.
range waittime 70 130
slider waittime obj\h009_000.bmp 155 568 200
number waittime obj\h011_000.bmp 409 540 429 449
step waittime -1 obj\h010_000.bmp 119 570 2 5 0
step waittime 1 obj\h010_000.bmp 469 570 2 5 1

screen config3
helplast
selectedlook
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 585 90 806 145 obj\h004_003.bmp 372 669 8 6
hover 46 94 562 159 obj\h004_003.bmp 372 669 8 0
hover 46 168 562 236 obj\h004_003.bmp 372 669 8 1
hover 46 401 562 471 obj\h004_003.bmp 372 669 8 4
hover 46 480 562 550 obj\h004_003.bmp 372 669 8 5
hover 46 245 562 392 obj\h004_003.bmp 372 669 8 2
hover 585 153 1100 625 obj\h004_003.bmp 372 669 8 7
pic obj\h000_000.bmp 0 0
# The side panel is the left edge of the room h000, the exe's own placement, which it reads for every menu (traced under
# Wine), and a frame of bars lies over the top and bottom.
pic obj\h000_000.bmp 1138 0 part 0 0 142 720
pic obj\e001_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\h021_000.bmp 43 85
button tab obj\h003_000.bmp 282 25 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 413 25 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 544 25 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 833 32 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 919 32 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 1005 32 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save sys_5\obj\e004_000.bmp 1152 93 10 5 0 help sys_5\obj\e005_000.bmp 868 45 1 10 0 0
button nav_load sys_5\obj\e004_000.bmp 1152 135 10 5 1 help sys_5\obj\e005_000.bmp 868 87 1 10 0 1
button nav_cg sys_5\obj\e004_000.bmp 1152 177 10 5 2 help sys_5\obj\e005_000.bmp 868 129 1 10 0 2
button nav_scene sys_5\obj\e004_000.bmp 1152 219 10 5 3 help sys_5\obj\e005_000.bmp 868 171 1 10 0 3
button nav_anime sys_5\obj\e004_000.bmp 1152 261 10 5 4 help sys_5\obj\e005_000.bmp 868 213 1 10 0 4
button nav_music sys_5\obj\e004_000.bmp 1152 303 10 5 5 help sys_5\obj\e005_000.bmp 868 255 1 10 0 5
button nav_config sys_5\obj\e004_000.bmp 1152 345 10 5 6 help sys_5\obj\e005_000.bmp 868 297 1 10 0 6
button nav_title sys_5\obj\e004_000.bmp 1152 387 10 5 7 help sys_5\obj\e005_000.bmp 868 339 1 10 0 7
button nav_quit sys_5\obj\e004_000.bmp 1152 429 10 5 8 help sys_5\obj\e005_000.bmp 868 381 1 10 0 8
button back sys_5\obj\e004_000.bmp 1152 513 10 5 9 help sys_5\obj\e005_000.bmp 868 465 1 10 0 9
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
button test:voice:voice\aki0_0048.ogg obj\h023_000.bmp 533 407 1 5 0
button test:voice:voice\yui9_0003.ogg obj\h023_000.bmp 533 485 1 5 0
check voice0 obj\h022_000.bmp 671 528 4 5 0
check voice1 obj\h022_000.bmp 881 528 4 5 1

screen log
pic obj\m001_000.bmp 1185 110
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1185 110 500
# Four entries a page, each under a rule, measured under Wine.
rows obj\m000_000.bmp 77 69 165 4 141 36
button log_close obj\l001_000.bmp 1240 22 2 5 0
button log_top obj\m003_000.bmp 1180 34 4 5 0
button log_up obj\m003_000.bmp 1180 68 4 5 1
button log_down obj\m003_000.bmp 1180 622 4 5 2
button log_bottom obj\m003_000.bmp 1180 655 4 5 3

screen dialog
# Nothing behind the question darkens.
dim 0
pic obj\b001_000.bmp 0 130
pic obj\i000_000.bmp 422 171 1 8 0 0
button yes obj\b003_000.bmp 449 261 2 5 0
button no obj\b003_000.bmp 449 350 2 5 1
# The box that stops the question, whichever answer follows, as on the PC.
button noask obj\i001_000.bmp 498 444 1 5 0

screen choice
pic obj\b001_000.bmp 0 130
pic obj\n000_000.bmp 414 173
place 263 90

screen start
pic sys_5\obj\d005_000.bmp 390 150
button start_prologue obj\d007_000.bmp 449 268 2 5 0
button start_main obj\d007_000.bmp 449 350 2 5 1

screen defaults
# Manual animation control, measured under Wine: the panel at the screen's bottom right, its buttons to zoom out, reset
# and zoom in, the zoom's digits beside the panel's point, the buttons' help right-aligned where the bar's sits, and the
# zoom from ×1.00 to ×2.50, 0.05 a wheel notch.
animpanel sys_5\obj\l010_000.bmp 1225 557
animbuttons sys_5\obj\l010_001.bmp 1240 674 635 596
animdigits sys_5\obj\l010_002.bmp 1239 1252 1261 570
animhelp sys_5\obj\l010_004.bmp 862 508
animzoom 100 250 5
# Version info, the PC's message box, as it shows for the copy measured under Wine; which installed parts each bonus
# line needs isn't known.
versionline 【俺を欲しがる二人の母アニメ追加版】
versionline ファイルバージョン：2, 0, 0, 0
versionline
versionline ○特典ストーリー：メイド編
versionline ○アニメ追加版！
manual sys_5\manual\index.html manual\index.html
websurvey sys_5\webアンケート.url webアンケート.url
default onahole 1
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

screen defaults
# How the PC sets the message window's text, measured under Wine against the same lines here, with VNDegen's stand-in
# fonts in place of the Windows ones.
# The PC creates its text font at a cell of 22 (Wine's font log), not init.ini's 25, so its glyphs are that size.
# Each glyph two pixels further on than its advance, lines 29 apart, and the first glyph 2 pixels in.
textsize 22
textgap 2
textline 29
textshift 2 0
)";
