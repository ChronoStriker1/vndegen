// Test Game 94's menus, a raw string that tables.cpp includes and menuTable() hands out.
// Test Game 94's menus start from Ore o Hoshigaru Tsuma to Gibo's, the same template, and its title is its own: the art
// with a blinking prompt until a click, then a dimmed main menu whose gallery entry opens a submenu beside it, measured
// under Wine.
const char kRinjin[] = R"(
screen title
menustages
# The art is the one GLB_TITLE_BG (55) numbers until an ending (51 to 54) is reached, then any of the four at random.
pic obj\d000_000.bmp 0 0 more obj\d000_001.bmp,obj\d000_002.bmp,obj\d000_003.bmp pickby 55 randomif 51,52,53,54
pic obj\d002_000.bmp 0 0
pic obj\d004_000.bmp 0 690
pic obj\d003_000.bmp 251 422 stage 1 blink 2000
pic dim 0 0 alpha 215 stage 2
pic obj\d005_000.bmp 135 121 stage 2
# Version information opens a message box on the PC, and the manual and the web survey a browser; here the manual opens
# in the app and the survey only after asking. This copy has no webアンケート.url, and its PC opens nothing for it.
button versioninfo obj\d008_000.bmp 1118 10 1 5 0 stage 2 help obj\d008_001.bmp 0 666 1 1 0 0
button manual obj\d006_000.bmp 150 465 8 5 5 stage 2 help obj\d007_000.bmp 0 666 1 8 0 5
button websurvey obj\d006_000.bmp 150 577 8 5 7 stage 2 help obj\d007_000.bmp 0 666 1 8 0 7
button start obj\d006_000.bmp 150 185 8 5 0 stage 2 help obj\d007_000.bmp 0 666 1 8 0 0
button load obj\d006_000.bmp 150 241 8 5 1 stage 2 help obj\d007_000.bmp 0 666 1 8 0 1
button quickload obj\d006_000.bmp 150 297 8 5 2 stage 2 help obj\d007_000.bmp 0 666 1 8 0 2
button submenu:gallery obj\d006_000.bmp 150 353 8 5 3 stage 2 help obj\d007_000.bmp 0 666 1 8 0 3
button config obj\d006_000.bmp 150 409 8 5 4 stage 2 help obj\d007_000.bmp 0 666 1 8 0 4
button quit obj\d006_000.bmp 150 521 8 5 6 stage 2 help obj\d007_000.bmp 0 666 1 8 0 6
# Past the prologue (GLB_PROLOGUE_OVER, 1000) Start opens where to start from, and after an ending the routes, each with
# its choices made for it, and Momoka's story.
startmenu 1000
button start_prologue obj\d006_001.bmp 362 185 3 5 0 stage 2 group start help obj\d007_001.bmp 0 666 1 3 0 0
button start_main obj\d006_001.bmp 362 241 3 5 1 stage 2 group start help obj\d007_001.bmp 0 666 1 3 0 1
button submenu:start/route obj\d006_001.bmp 362 297 3 5 2 stage 2 group start if 51,52,53,54 help obj\d007_001.bmp 0 666 1 3 0 2
button start_sub obj\d006_004.bmp 362 353 1 5 0 stage 2 group start if 51,52,53,54 help obj\d007_005.bmp 0 666 1 1 0 0
button route1 obj\d006_002.bmp 574 297 4 5 0 stage 2 group start/route help obj\d007_002.bmp 0 666 1 4 0 0
button route2 obj\d006_002.bmp 574 353 4 5 1 stage 2 group start/route help obj\d007_002.bmp 0 666 1 4 0 1
button route3 obj\d006_002.bmp 574 409 4 5 2 stage 2 group start/route help obj\d007_002.bmp 0 666 1 4 0 2
button route4 obj\d006_002.bmp 574 465 4 5 3 stage 2 group start/route help obj\d007_002.bmp 0 666 1 4 0 3
pic obj\d007_004.bmp 734 307 1 4 0 0 stage 2 group start/route if 51
pic obj\d007_004.bmp 734 363 1 4 0 1 stage 2 group start/route if 52
pic obj\d007_004.bmp 734 419 1 4 0 2 stage 2 group start/route if 53
pic obj\d007_004.bmp 734 475 1 4 0 3 stage 2 group start/route if 54
button cg obj\d006_003.bmp 362 353 3 5 0 stage 2 group gallery help obj\d007_003.bmp 0 666 1 3 0 0
button scene obj\d006_003.bmp 362 409 3 5 1 stage 2 group gallery help obj\d007_003.bmp 0 666 1 3 0 1
button music obj\d006_003.bmp 362 465 3 5 2 stage 2 group gallery help obj\d007_003.bmp 0 666 1 3 0 2
# The staff roll after an ending is Kazuki's, Touka's (Momoka's story) or, in the shop's extra, Kanamori's, by
# SITE_STAFFROLL (104), and a click ends it once an ending was reached before.
screen staffroll
movie 104 movie\staffroll_kazuki movie\staffroll_touka sys_1\movie\staffroll_kanamori
skipif 51 52 53 54
screen load
helplast
selectedlook
pic obj\e000_000.bmp 0 0
pic obj\e003_000.bmp 1118 0
pic obj\e001_000.bmp 0 0
pic obj\e002_000.bmp 0 0 1 2 0 1
lockdim
slots obj\e007_000.bmp 26 95 546 140 4 2
slotlabel obj\e008_000.bmp 6 11
slotdigits obj\e008_001.bmp 36 6
slotthumb 15 26
datedigits obj\e009_000.bmp 233 68
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
dategap 8
slotlock obj\e012_000.bmp 446 63
slotdelete obj\e013_000.bmp 476 63
slotprogress obj\e010_000.bmp 279 37 5
slotnew obj\e014_000.bmp 5 104
slotcomment 184 111 330 18 クリックでコメントを入力
button page0 obj\e006_000.bmp 410 26 10 5 0
button page1 obj\e006_000.bmp 460 26 10 5 1
button page2 obj\e006_000.bmp 510 26 10 5 2
button page3 obj\e006_000.bmp 560 26 10 5 3
button page4 obj\e006_000.bmp 610 26 10 5 4
button page5 obj\e006_000.bmp 660 26 10 5 5
button page6 obj\e006_000.bmp 710 26 10 5 6
button page7 obj\e006_000.bmp 760 26 10 5 7
button page8 obj\e006_000.bmp 810 26 10 5 8
button page9 obj\e006_000.bmp 860 26 10 5 9
button page10 obj\e006_001.bmp 910 26 1 5 0
button nav_save obj\e004_000.bmp 1172 94 9 5 0 help obj\e005_000.bmp 948 666 1 9 0 0
button nav_load obj\e004_000.bmp 1172 135 9 5 1 help obj\e005_000.bmp 948 666 1 9 0 1
button nav_cg obj\e004_000.bmp 1172 176 9 5 2 help obj\e005_000.bmp 948 666 1 9 0 2
button nav_scene obj\e004_000.bmp 1172 217 9 5 3 help obj\e005_000.bmp 948 666 1 9 0 3
button nav_music obj\e004_000.bmp 1172 258 9 5 4 help obj\e005_000.bmp 948 666 1 9 0 4
button nav_config obj\e004_000.bmp 1172 299 9 5 5 help obj\e005_000.bmp 948 666 1 9 0 5
button nav_title obj\e004_000.bmp 1172 340 9 5 6 help obj\e005_000.bmp 948 666 1 9 0 6
button nav_quit obj\e004_000.bmp 1172 381 9 5 7 help obj\e005_000.bmp 948 666 1 9 0 7
button back obj\e004_000.bmp 1172 463 9 5 8 help obj\e005_000.bmp 948 666 1 9 0 8

screen save
helplast
selectedlook
pic obj\e000_000.bmp 0 0
pic obj\e003_000.bmp 1118 0
pic obj\e001_000.bmp 0 0
pic obj\e002_000.bmp 0 0 1 2 0 0
lockdim
slots obj\e007_000.bmp 26 95 546 140 4 2
slotlabel obj\e008_000.bmp 6 11
slotdigits obj\e008_001.bmp 36 6
slotthumb 15 26
datedigits obj\e009_000.bmp 233 68
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
dategap 8
slotlock obj\e012_000.bmp 446 63
slotdelete obj\e013_000.bmp 476 63
slotprogress obj\e010_000.bmp 279 37 5
slotnew obj\e014_000.bmp 5 104
slotcomment 184 111 330 18 クリックでコメントを入力
# The Q-Save page takes only quick saves.
pageoff 10
button page0 obj\e006_000.bmp 410 26 10 5 0
button page1 obj\e006_000.bmp 460 26 10 5 1
button page2 obj\e006_000.bmp 510 26 10 5 2
button page3 obj\e006_000.bmp 560 26 10 5 3
button page4 obj\e006_000.bmp 610 26 10 5 4
button page5 obj\e006_000.bmp 660 26 10 5 5
button page6 obj\e006_000.bmp 710 26 10 5 6
button page7 obj\e006_000.bmp 760 26 10 5 7
button page8 obj\e006_000.bmp 810 26 10 5 8
button page9 obj\e006_000.bmp 860 26 10 5 9
button page10 obj\e006_001.bmp 910 26 1 5 0
button nav_save obj\e004_000.bmp 1172 94 9 5 0 help obj\e005_000.bmp 948 666 1 9 0 0
button nav_load obj\e004_000.bmp 1172 135 9 5 1 help obj\e005_000.bmp 948 666 1 9 0 1
button nav_cg obj\e004_000.bmp 1172 176 9 5 2 help obj\e005_000.bmp 948 666 1 9 0 2
button nav_scene obj\e004_000.bmp 1172 217 9 5 3 help obj\e005_000.bmp 948 666 1 9 0 3
button nav_music obj\e004_000.bmp 1172 258 9 5 4 help obj\e005_000.bmp 948 666 1 9 0 4
button nav_config obj\e004_000.bmp 1172 299 9 5 5 help obj\e005_000.bmp 948 666 1 9 0 5
button nav_title obj\e004_000.bmp 1172 340 9 5 6 help obj\e005_000.bmp 948 666 1 9 0 6
button nav_quit obj\e004_000.bmp 1172 381 9 5 7 help obj\e005_000.bmp 948 666 1 9 0 7
button back obj\e004_000.bmp 1172 463 9 5 8 help obj\e005_000.bmp 948 666 1 9 0 8

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
viewerbutton set:hair obj\j009_000.bmp 544 639 2 0 helppic obj\h004_004.bmp 400 679 2 0
viewerlook set:hair 0 0 1 2
viewerbutton set:hero obj\j009_000.bmp 640 639 2 0 helppic obj\h004_004.bmp 400 679 2 1
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
# Each tab's place and couple, measured under Wine: the room with Kazuki and his wife in front of him, and the
# apartment block with Kanamori and her in front of him.
pic obj\f000_000.bmp 0 0 tab 0
pic obj\f002_001.bmp -74 -39 tab 0
pic obj\f002_000.bmp 58 -39 tab 0
pic obj\f000_001.bmp 0 0 tab 1
pic obj\f002_003.bmp 52 -39 tab 1
pic obj\f002_002.bmp -93 -39 tab 1
pic obj\f003_001.bmp 0 567
pic obj\e003_000.bmp 1118 0
pic obj\e001_000.bmp 0 0
pic obj\f001_000.bmp 0 0 1 2 0 0
cellhide
cellsheet obj\f005_000.bmp
cellsize 180 150
cellthumb 10 25
cell 476 136
cell 675 136
cell 874 136
cell 516 299
cell 715 299
cell 914 299
cell 476 462
cell 675 462
cell 874 462
celllabel obj\e008_000.bmp 6 11
celldigits obj\e008_001.bmp 36 6 3 18
# The pictures seen in the card's numerals, then a small slash and the total.
cellcount obj\e008_001.bmp obj\f006_001.bmp 95 120 131 144 2
celltotal obj\f006_000.bmp 3
gallerytabs 0
# The gallery pages are 56 apart, wider than the save pages, as many as a tab needs.
button page0 obj\e006_000.bmp 410 26 10 5 0
button page1 obj\e006_000.bmp 466 26 10 5 1
button page2 obj\e006_000.bmp 522 26 10 5 2
button page3 obj\e006_000.bmp 578 26 10 5 3
button page4 obj\e006_000.bmp 634 26 10 5 4
button gtab0 obj\f003_000.bmp 57 601 2 5 0
button gtab1 obj\f003_000.bmp 231 601 2 5 1
# 拘り表現 and 通常表現, Config's こだわり表現.
button express1 obj\f004_000.bmp 231 655 2 5 0
button express0 obj\f004_000.bmp 311 655 2 5 1
button nav_save obj\e004_000.bmp 1172 94 9 5 0 help obj\e005_000.bmp 948 666 1 9 0 0
button nav_load obj\e004_000.bmp 1172 135 9 5 1 help obj\e005_000.bmp 948 666 1 9 0 1
button nav_cg obj\e004_000.bmp 1172 176 9 5 2 help obj\e005_000.bmp 948 666 1 9 0 2
button nav_scene obj\e004_000.bmp 1172 217 9 5 3 help obj\e005_000.bmp 948 666 1 9 0 3
button nav_music obj\e004_000.bmp 1172 258 9 5 4 help obj\e005_000.bmp 948 666 1 9 0 4
button nav_config obj\e004_000.bmp 1172 299 9 5 5 help obj\e005_000.bmp 948 666 1 9 0 5
button nav_title obj\e004_000.bmp 1172 340 9 5 6 help obj\e005_000.bmp 948 666 1 9 0 6
button nav_quit obj\e004_000.bmp 1172 381 9 5 7 help obj\e005_000.bmp 948 666 1 9 0 7
button back obj\e004_000.bmp 1172 463 9 5 8 help obj\e005_000.bmp 948 666 1 9 0 8

screen scene
helplast
selectedlook
# Each tab's place and couple, measured under Wine: the room with Kazuki and his wife in front of him, and the
# apartment block with Kanamori and her in front of him.
pic obj\f000_000.bmp 0 0 tab 0
pic obj\f002_001.bmp -74 -39 tab 0
pic obj\f002_000.bmp 58 -39 tab 0
pic obj\f000_001.bmp 0 0 tab 1
pic obj\f002_003.bmp 52 -39 tab 1
pic obj\f002_002.bmp -93 -39 tab 1
pic obj\f003_001.bmp 0 567
pic obj\e003_000.bmp 1118 0
pic obj\e001_000.bmp 0 0
pic obj\f001_000.bmp 0 0 1 2 0 1
cellhide
cellsheet obj\f005_000.bmp
cellsize 180 150
cellthumb 10 25
cell 476 136
cell 675 136
cell 874 136
cell 516 299
cell 715 299
cell 914 299
cell 476 462
cell 675 462
cell 874 462
celllabel obj\e008_000.bmp 6 11
celldigits obj\e008_001.bmp 36 6 3 18
gallerytabs 0
# The gallery pages are 56 apart, wider than the save pages, as many as a tab needs.
button page0 obj\e006_000.bmp 410 26 10 5 0
button page1 obj\e006_000.bmp 466 26 10 5 1
button page2 obj\e006_000.bmp 522 26 10 5 2
button page3 obj\e006_000.bmp 578 26 10 5 3
button page4 obj\e006_000.bmp 634 26 10 5 4
button gtab0 obj\f003_000.bmp 57 601 2 5 0
button gtab1 obj\f003_000.bmp 231 601 2 5 1
# 拘り表現 and 通常表現, Config's こだわり表現.
button express1 obj\f004_000.bmp 231 655 2 5 0
button express0 obj\f004_000.bmp 311 655 2 5 1
button nav_save obj\e004_000.bmp 1172 94 9 5 0 help obj\e005_000.bmp 948 666 1 9 0 0
button nav_load obj\e004_000.bmp 1172 135 9 5 1 help obj\e005_000.bmp 948 666 1 9 0 1
button nav_cg obj\e004_000.bmp 1172 176 9 5 2 help obj\e005_000.bmp 948 666 1 9 0 2
button nav_scene obj\e004_000.bmp 1172 217 9 5 3 help obj\e005_000.bmp 948 666 1 9 0 3
button nav_music obj\e004_000.bmp 1172 258 9 5 4 help obj\e005_000.bmp 948 666 1 9 0 4
button nav_config obj\e004_000.bmp 1172 299 9 5 5 help obj\e005_000.bmp 948 666 1 9 0 5
button nav_title obj\e004_000.bmp 1172 340 9 5 6 help obj\e005_000.bmp 948 666 1 9 0 6
button nav_quit obj\e004_000.bmp 1172 381 9 5 7 help obj\e005_000.bmp 948 666 1 9 0 7
button back obj\e004_000.bmp 1172 463 9 5 8 help obj\e005_000.bmp 948 666 1 9 0 8

screen music
helplast
selectedlook
# The room, the player and the ten tracks, measured under Wine, with a help line for each control and the volume.
hover 733 583 896 607 obj\g007_000.bmp 868 666 8 7
pic obj\g000_000.bmp 0 0
pic obj\e003_000.bmp 1118 0
pic obj\e001_000.bmp 0 0
pic obj\g001_000.bmp 0 0
pic obj\g003_000.bmp 356 555
slider music obj\g005_000.bmp 734 584
number music obj\g006_000.bmp 940 591 11
button music_prev obj\g004_000.bmp 380 575 7 5 0 help obj\g007_000.bmp 868 666 1 8 0 0
button music_play obj\g004_000.bmp 424 575 7 5 1 help obj\g007_000.bmp 868 666 1 8 0 1
button music_pause obj\g004_000.bmp 468 575 7 5 2 help obj\g007_000.bmp 868 666 1 8 0 2
button music_stop obj\g004_000.bmp 512 575 7 5 3 help obj\g007_000.bmp 868 666 1 8 0 3
button music_next obj\g004_000.bmp 556 575 7 5 4 help obj\g007_000.bmp 868 666 1 8 0 4
button music_repeat obj\g004_000.bmp 600 575 7 5 5 help obj\g007_000.bmp 868 666 1 8 0 5
button music_shuffle obj\g004_000.bmp 644 575 7 5 6 help obj\g007_000.bmp 868 666 1 8 0 6
button track0 obj\g002_000.bmp 51 95 10 5 0
button track1 obj\g002_000.bmp 51 181 10 5 1
button track2 obj\g002_000.bmp 51 267 10 5 2
button track3 obj\g002_000.bmp 51 353 10 5 3
button track4 obj\g002_000.bmp 51 439 10 5 4
button track5 obj\g002_000.bmp 551 95 10 5 5
button track6 obj\g002_000.bmp 551 181 10 5 6
button track7 obj\g002_000.bmp 551 267 10 5 7
button track8 obj\g002_000.bmp 551 353 10 5 8
button track9 obj\g002_000.bmp 551 439 10 5 9
button nav_save obj\e004_000.bmp 1172 94 9 5 0 help obj\e005_000.bmp 948 666 1 9 0 0
button nav_load obj\e004_000.bmp 1172 135 9 5 1 help obj\e005_000.bmp 948 666 1 9 0 1
button nav_cg obj\e004_000.bmp 1172 176 9 5 2 help obj\e005_000.bmp 948 666 1 9 0 2
button nav_scene obj\e004_000.bmp 1172 217 9 5 3 help obj\e005_000.bmp 948 666 1 9 0 3
button nav_music obj\e004_000.bmp 1172 258 9 5 4 help obj\e005_000.bmp 948 666 1 9 0 4
button nav_config obj\e004_000.bmp 1172 299 9 5 5 help obj\e005_000.bmp 948 666 1 9 0 5
button nav_title obj\e004_000.bmp 1172 340 9 5 6 help obj\e005_000.bmp 948 666 1 9 0 6
button nav_quit obj\e004_000.bmp 1172 381 9 5 7 help obj\e005_000.bmp 948 666 1 9 0 7
button back obj\e004_000.bmp 1172 463 9 5 8 help obj\e005_000.bmp 948 666 1 9 0 8

screen config
helplast
selectedlook
# Help for each settings group, mapped on a grid of points under Wine, edges within 20 pixels.
hover 581 342 730 414 obj\h004_000.bmp 350 669 9 7
hover 410 198 565 270 obj\h004_000.bmp 350 669 9 4
hover 50 198 210 270 obj\h004_000.bmp 350 669 9 2
hover 210 198 370 270 obj\h004_000.bmp 350 669 9 3
hover 50 85 565 126 obj\h004_000.bmp 350 669 9 0
hover 770 342 930 414 obj\h004_004.bmp 350 669 2 0
hover 930 342 1090 414 obj\h004_004.bmp 350 669 2 1
hover 50 126 565 198 obj\h004_000.bmp 350 669 9 1
hover 581 414 1090 630 obj\h004_000.bmp 350 669 9 8
hover 581 85 1090 342 obj\h004_000.bmp 350 669 9 6
hover 50 270 565 606 obj\h004_000.bmp 350 669 9 5
pic obj\h000_000.bmp 0 0
# The side panel and the frame, as on every menu (traced under Wine).
pic obj\e003_000.bmp 1118 0
pic obj\e001_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\h005_000.bmp 42 88
button tab obj\h003_000.bmp 240 37 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 348 37 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 456 37 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 799 27 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 895 27 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 991 27 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save obj\e004_000.bmp 1172 94 9 5 0 help obj\e005_000.bmp 948 666 1 9 0 0
button nav_load obj\e004_000.bmp 1172 135 9 5 1 help obj\e005_000.bmp 948 666 1 9 0 1
button nav_cg obj\e004_000.bmp 1172 176 9 5 2 help obj\e005_000.bmp 948 666 1 9 0 2
button nav_scene obj\e004_000.bmp 1172 217 9 5 3 help obj\e005_000.bmp 948 666 1 9 0 3
button nav_music obj\e004_000.bmp 1172 258 9 5 4 help obj\e005_000.bmp 948 666 1 9 0 4
button nav_config obj\e004_000.bmp 1172 299 9 5 5 help obj\e005_000.bmp 948 666 1 9 0 5
button nav_title obj\e004_000.bmp 1172 340 9 5 6 help obj\e005_000.bmp 948 666 1 9 0 6
button nav_quit obj\e004_000.bmp 1172 381 9 5 7 help obj\e005_000.bmp 948 666 1 9 0 7
button back obj\e004_000.bmp 1172 463 9 5 8 help obj\e005_000.bmp 948 666 1 9 0 8
radio screen 0 obj\h006_000.bmp 172 95 3 5 0
radio screen 1 obj\h006_000.bmp 294 95 3 5 1
# FULL 16 is full screen in 16-bit colour on the PC, here the same full screen.
radio screen 2 obj\h006_000.bmp 416 95 3 5 2
pic obj\h007_000.bmp 172 137 6 5 0 4
pic obj\h007_000.bmp 294 137 6 5 1 4
pic obj\h007_000.bmp 416 137 6 5 2 4
pic obj\h007_000.bmp 172 169 6 5 3 4
pic obj\h007_000.bmp 294 169 6 5 4 4
pic obj\h007_000.bmp 416 169 6 5 5 4
radio effect 1 obj\h008_000.bmp 50 233 2 5 0
radio effect 0 obj\h008_000.bmp 131 233 2 5 1
radio wait 1 obj\h008_000.bmp 226 233 2 5 0
radio wait 0 obj\h008_000.bmp 307 233 2 5 1
radio waitskip 1 obj\h008_000.bmp 402 233 2 5 0
radio waitskip 0 obj\h008_000.bmp 483 233 2 5 1
radio camera 1 obj\h008_000.bmp 615 397 2 5 0
radio camera 0 obj\h008_000.bmp 696 397 2 5 1
# CG表現の切り替え: 拘り表現 or 通常表現, which the message window's こだわり表現 button switches too.
radio express 1 obj\h014_000.bmp 656 615 2 5 0
radio express 0 obj\h014_000.bmp 913 615 2 5 1
slider opacity obj\h009_000.bmp 155 311
number opacity obj\h011_000.bmp 415 283
step opacity -1 obj\h010_000.bmp 119 313 2 5 0
step opacity 1 obj\h010_000.bmp 469 313 2 5 1
check confirm_saveload obj\h013_000.bmp 603 121 8 5 0
check confirm_overwrite obj\h013_000.bmp 603 151 8 5 1
check confirm_delete obj\h013_000.bmp 603 181 8 5 2
check confirm_quick obj\h013_000.bmp 603 211 8 5 3
check confirm_title obj\h013_000.bmp 603 241 8 5 4
check confirm_exit obj\h013_000.bmp 603 271 8 5 5
check tutorial obj\h013_000.bmp 603 301 8 5 6
pic obj\h013_000.bmp 603 331 8 5 7 4

screen config2
helplast
selectedlook
# Help for each settings group, mapped on a grid of points under Wine, edges within 20 pixels.
# The count colour has no help on the PC.
hover 581 85 810 150 obj\h004_002.bmp 33 669 5 0
hover 810 85 1050 150 obj\h004_002.bmp 33 669 5 1
hover 581 222 810 294 obj\h004_002.bmp 33 669 5 3
hover 810 222 1050 294 obj\h004_002.bmp 33 669 5 4
hover 50 85 565 150 obj\h004_001.bmp 380 669 3 0
hover 581 150 1090 222 obj\h004_002.bmp 33 669 5 2
hover 50 174 565 246 obj\h004_001.bmp 380 669 3 1
hover 50 534 565 606 obj\h004_001.bmp 380 669 3 2
pic obj\h000_000.bmp 0 0
# The side panel and the frame, as on every menu (traced under Wine).
pic obj\e003_000.bmp 1118 0
pic obj\e001_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\h015_000.bmp 42 88
button tab obj\h003_000.bmp 240 37 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 348 37 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 456 37 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 799 27 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 895 27 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 991 27 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save obj\e004_000.bmp 1172 94 9 5 0 help obj\e005_000.bmp 948 666 1 9 0 0
button nav_load obj\e004_000.bmp 1172 135 9 5 1 help obj\e005_000.bmp 948 666 1 9 0 1
button nav_cg obj\e004_000.bmp 1172 176 9 5 2 help obj\e005_000.bmp 948 666 1 9 0 2
button nav_scene obj\e004_000.bmp 1172 217 9 5 3 help obj\e005_000.bmp 948 666 1 9 0 3
button nav_music obj\e004_000.bmp 1172 258 9 5 4 help obj\e005_000.bmp 948 666 1 9 0 4
button nav_config obj\e004_000.bmp 1172 299 9 5 5 help obj\e005_000.bmp 948 666 1 9 0 5
button nav_title obj\e004_000.bmp 1172 340 9 5 6 help obj\e005_000.bmp 948 666 1 9 0 6
button nav_quit obj\e004_000.bmp 1172 381 9 5 7 help obj\e005_000.bmp 948 666 1 9 0 7
button back obj\e004_000.bmp 1172 463 9 5 8 help obj\e005_000.bmp 948 666 1 9 0 8
radio text 0 obj\h016_000.bmp 64 126 4 5 0
radio auto 0 obj\h016_000.bmp 64 204 4 5 0
radio text 1 obj\h016_000.bmp 186 126 4 5 1
radio auto 1 obj\h016_000.bmp 186 204 4 5 1
radio text 2 obj\h016_000.bmp 308 126 4 5 2
radio auto 2 obj\h016_000.bmp 308 204 4 5 2
radio text 3 obj\h016_000.bmp 430 126 4 5 3
radio auto 3 obj\h016_000.bmp 430 204 4 5 3
radio skipall 0 obj\h018_000.bmp 617 116 2 5 0
radio skipall 1 obj\h018_000.bmp 698 116 2 5 1
radio deckey 0 obj\h019_000.bmp 845 116 2 5 0
radio deckey 1 obj\h019_000.bmp 926 116 2 5 1
radio rightclick 0 obj\h020_000.bmp 710 159 5 5 0
radio rightclick 1 obj\h020_000.bmp 832 159 5 5 1
radio rightclick 2 obj\h020_000.bmp 954 159 5 5 2
radio rightclick 3 obj\h020_000.bmp 710 191 5 5 3
radio rightclick 4 obj\h020_000.bmp 832 191 5 5 4
radio pause_inactive 1 obj\h008_000.bmp 617 254 2 5 0
radio pause_inactive 0 obj\h008_000.bmp 698 254 2 5 1
# The PC's Electric Onahole drives a USB toy. VNDegen keeps the choice and does nothing with it.
radio onahole 1 obj\h008_000.bmp 845 254 2 5 0
radio onahole 0 obj\h008_000.bmp 926 254 2 5 1
radio count_color 1 obj\h008_000.bmp 617 318 2 5 0
radio count_color 0 obj\h008_000.bmp 698 318 2 5 1
# Wait time, ×0.70 to ×1.30 in steps of 0.01, stretches waits and fades; the bar is full at ×2.00.
range waittime 70 130
slider waittime obj\h009_000.bmp 155 573 200
number waittime obj\h011_000.bmp 409 545 429 449
step waittime -1 obj\h010_000.bmp 119 575 2 5 0
step waittime 1 obj\h010_000.bmp 469 575 2 5 1

screen config3
helplast
selectedlook
# Help for each settings group, mapped on a grid of points under Wine, edges within 20 pixels.
hover 581 85 810 150 obj\h004_003.bmp 372 669 8 6
hover 50 85 565 150 obj\h004_003.bmp 372 669 8 0
hover 50 174 565 246 obj\h004_003.bmp 372 669 8 1
hover 50 390 565 462 obj\h004_003.bmp 372 669 8 4
hover 50 486 565 558 obj\h004_003.bmp 372 669 8 5
hover 50 246 565 390 obj\h004_003.bmp 372 669 8 2
hover 581 150 1090 630 obj\h004_003.bmp 372 669 8 7
pic obj\h000_000.bmp 0 0
# The side panel and the frame, as on every menu (traced under Wine).
pic obj\e003_000.bmp 1118 0
pic obj\e001_000.bmp 0 0
pic obj\h001_000.bmp 0 0
pic obj\h021_000.bmp 42 88
button tab obj\h003_000.bmp 240 37 3 5 0 help obj\h003_001.bmp 830 669 1 3 0 0
button tab2 obj\h003_000.bmp 348 37 3 5 1 help obj\h003_001.bmp 830 669 1 3 0 1
button tab3 obj\h003_000.bmp 456 37 3 5 2 help obj\h003_001.bmp 830 669 1 3 0 2
button cfg_defaults obj\h002_000.bmp 799 27 3 5 0 help obj\h002_001.bmp 882 669 1 3 0 0
button cfg_undo obj\h002_000.bmp 895 27 3 5 1 help obj\h002_001.bmp 882 669 1 3 0 1
button cfg_apply obj\h002_000.bmp 991 27 3 5 2 help obj\h002_001.bmp 882 669 1 3 0 2
button nav_save obj\e004_000.bmp 1172 94 9 5 0 help obj\e005_000.bmp 948 666 1 9 0 0
button nav_load obj\e004_000.bmp 1172 135 9 5 1 help obj\e005_000.bmp 948 666 1 9 0 1
button nav_cg obj\e004_000.bmp 1172 176 9 5 2 help obj\e005_000.bmp 948 666 1 9 0 2
button nav_scene obj\e004_000.bmp 1172 217 9 5 3 help obj\e005_000.bmp 948 666 1 9 0 3
button nav_music obj\e004_000.bmp 1172 258 9 5 4 help obj\e005_000.bmp 948 666 1 9 0 4
button nav_config obj\e004_000.bmp 1172 299 9 5 5 help obj\e005_000.bmp 948 666 1 9 0 5
button nav_title obj\e004_000.bmp 1172 340 9 5 6 help obj\e005_000.bmp 948 666 1 9 0 6
button nav_quit obj\e004_000.bmp 1172 381 9 5 7 help obj\e005_000.bmp 948 666 1 9 0 7
button back obj\e004_000.bmp 1172 463 9 5 8 help obj\e005_000.bmp 948 666 1 9 0 8
# Six volumes, each with its mute, measured under Wine.
slider master obj\h009_000.bmp 154 128
number master obj\h011_000.bmp 415 100
step master -1 obj\h010_000.bmp 119 130 2 5 0
step master 1 obj\h010_000.bmp 469 130 2 5 1
check mute_master obj\h024_000.bmp 479 99 1 5 0
slider bgm obj\h009_000.bmp 154 206
number bgm obj\h011_000.bmp 415 178
step bgm -1 obj\h010_000.bmp 119 208 2 5 0
step bgm 1 obj\h010_000.bmp 469 208 2 5 1
check mute_bgm obj\h024_000.bmp 479 177 1 5 0
slider se obj\h009_000.bmp 154 284
number se obj\h011_000.bmp 415 256
step se -1 obj\h010_000.bmp 119 286 2 5 0
step se 1 obj\h010_000.bmp 469 286 2 5 1
check mute_se obj\h024_000.bmp 479 255 1 5 0
slider hse obj\h009_000.bmp 154 362
number hse obj\h011_000.bmp 415 334
step hse -1 obj\h010_000.bmp 119 364 2 5 0
step hse 1 obj\h010_000.bmp 469 364 2 5 1
check mute_hse obj\h024_000.bmp 479 333 1 5 0
slider voice obj\h009_000.bmp 154 441
number voice obj\h011_000.bmp 415 413
step voice -1 obj\h010_000.bmp 119 443 2 5 0
step voice 1 obj\h010_000.bmp 469 443 2 5 1
check mute_voice obj\h024_000.bmp 479 411 1 5 0
slider backvoice obj\h009_000.bmp 154 519
number backvoice obj\h011_000.bmp 415 491
step backvoice -1 obj\h010_000.bmp 119 521 2 5 0
step backvoice 1 obj\h010_000.bmp 469 521 2 5 1
check mute_backvoice obj\h024_000.bmp 479 489 1 5 0
# クリック時、ボイスを止める.
radio voice_stop 1 obj\h008_000.bmp 617 116 2 5 0
radio voice_stop 0 obj\h008_000.bmp 698 116 2 5 1
# Individual voices: 高宮桃華, 金森ユウジ, and the other women (type 2) and men (type 3), whose labels lie the other
# way round in the sheet, beside the panel's portraits.
check voice0 obj\h022_000.bmp 687 311 4 5 0
check voice1 obj\h022_000.bmp 903 311 4 5 1
check voice2 obj\h022_000.bmp 687 479 4 5 3
check voice3 obj\h022_000.bmp 903 479 4 5 2
# Each ♪ plays the sample the PC reads for it under Wine; master and music have none, as on the PC.
pic obj\h023_000.bmp 533 99 1 5 0 4
pic obj\h023_000.bmp 533 177 1 5 0 4
button test:se:se\se_11.ogg obj\h023_000.bmp 533 255 1 5 0
button test:se:se\hse_002_004.ogg obj\h023_000.bmp 533 333 1 5 0
button test:voice:voice\tou0_0055.ogg obj\h023_000.bmp 533 411 1 5 0
button test:voice:voice\tou9_0003.ogg obj\h023_000.bmp 533 489 1 5 0

screen log
pic obj\m001_000.bmp 1185 110
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1185 110 500
# Four entries a page, each under a rule, measured under Wine. The text is the message window's size: under Wine,
# with a stand-in for MS Gothic, the log's letters and the window's came out the same size.
rows obj\m000_000.bmp 77 69 165 4 101 35
button log_close obj\l001_000.bmp 1240 22 2 5 0
button log_top obj\m003_000.bmp 1180 34 4 5 0
button log_up obj\m003_000.bmp 1180 68 4 5 1
button log_down obj\m003_000.bmp 1180 622 4 5 2
button log_bottom obj\m003_000.bmp 1180 655 4 5 3

screen dialog
# Measured under Wine: the screen behind at 35 of 256, the mouse beside each answer, a left click away from them for yes
# and a right click for no, and a box that stops the question, whichever answer follows.
dim 221
mousechoice
pic obj\i000_000.bmp 417 107 1 8 0 0
pic obj\b003_000.bmp 393 192 2 1 0 0
pic obj\b003_000.bmp 393 305 2 1 1 0
button yes obj\b002_000.bmp 467 195 2 5 0
button no obj\b002_000.bmp 467 308 2 5 1
button noask obj\i001_000.bmp 498 429 1 5 0

# A choice, measured under Wine: the scene darkened, its prompt, and the options with a mouse icon each, the left
# button's by the first and the right button's by the second, which take them. Once an ending is reached each option
# names the routes that take it, red for one cleared.
screen choice
pic dim 0 0 alpha 162
pic obj\n000_000.bmp 483 107
pic obj\b003_000.bmp 393 192 2 1 0 0
pic obj\b003_000.bmp 393 305 2 1 1 0
place 195 113
mousechoice
routes obj\e011_000.bmp 818 30 51 52 53 54

screen start
pic obj\d005_000.bmp 390 150
button start_prologue obj\d007_000.bmp 449 268 2 5 0
button start_main obj\d007_000.bmp 449 350 2 5 1

screen defaults
# Version info, the PC's message box, as it shows for the copy measured under Wine; which installed parts each bonus
# line needs isn't known.
versionline 【隣人に壊されていく俺の妻】
versionline ファイルバージョン：1, 0, 0, 0
versionline
versionline ○オフィシャル通販特典
manual manual\index.html
websurvey webアンケート.url
default onahole 1
default bar_always 1
default opacity 100
# SENARIO_STATUS in the macro list holds the route, as the PC's saves show.
routevar 4000

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
# The PC creates its text font at a cell of 21 (Wine's font log), not init.ini's 24, so its glyphs are that size.
# Each glyph two pixels further on than its advance, lines 28 apart, the first glyph 2 pixels in and the baseline 1 higher.
textsize 21
textgap 2
textline 28
textshift 2 -1
)";
