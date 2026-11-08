// The Alchemist and His Battle-Scarred Homunculus's menus, a raw string that tables.cpp includes and menuTable() hands out.
// Test Game 95.
const char kAlchemist[] = R"(
screen title
pic obj\c000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\d000_001.bmp 0 1038
pic obj\d001_000.bmp 0 0
pic obj\d002_000.bmp 968 97
pic obj\d004_000.bmp 924 1044
button start obj\d003_000.bmp 1072 523 7 5 0 help obj\d006_000.bmp 1072 469 1 1 0 0
button load obj\d003_000.bmp 1292 523 7 5 1 help obj\d006_001.bmp 1292 469 1 1 0 0
button quickload obj\d003_000.bmp 1512 523 7 5 2 help obj\d006_002.bmp 1334 469 1 1 0 0
button cg obj\d003_000.bmp 1072 657 7 5 3 help obj\d006_003.bmp 1072 603 1 1 0 0
button scene obj\d003_000.bmp 1292 657 7 5 5 help obj\d006_004.bmp 1292 603 1 1 0 0
button music obj\d003_000.bmp 1512 657 7 5 6 help obj\d006_006.bmp 1512 603 1 1 0 0
button config obj\d003_001.bmp 1207 800 3 5 0 help obj\d006_007.bmp 1207 746 1 1 0 0
button quit obj\d003_001.bmp 1427 800 3 5 2 help obj\d006_009.bmp 1427 746 1 1 0 0

screen load
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 128
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\e001_000.bmp 0 0 1 2 0 1
slots obj\e007_000.bmp 41 178 786 220 4 2
slotlabel obj\e008_000.bmp 18 9
slotdigits obj\e008_001.bmp 58 4
slotthumb 19 34
datedigits obj\e009_000.bmp 287 65
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
slotlock obj\e011_000.bmp 15 139
slotnew obj\e012_000.bmp -6 -6
slotcomment 278 157 452 20 Click to set a comment
slotdelete obj\e011_001.bmp 672 19
dategap 21
button page0 obj\e005_000.bmp 458 64 11 5 0
button page1 obj\e005_000.bmp 558 64 11 5 1
button page2 obj\e005_000.bmp 658 64 11 5 2
button page3 obj\e005_000.bmp 758 64 11 5 3
button page4 obj\e005_000.bmp 858 64 11 5 4
button page5 obj\e005_000.bmp 958 64 11 5 5
button page6 obj\e005_000.bmp 1058 64 11 5 6
button page7 obj\e005_000.bmp 1158 64 11 5 7
button page8 obj\e005_000.bmp 1258 64 11 5 8
button page9 obj\e005_000.bmp 1358 64 11 5 9
button page10 obj\e005_000.bmp 1458 64 11 5 10
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap help obj\e004_000.bmp 1500 7 1 1 0 0
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap help obj\e004_001.bmp 1508 84 1 1 0 0
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap help obj\e004_002.bmp 1546 161 1 1 0 0
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap help obj\e004_004.bmp 1530 238 1 1 0 0
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap help obj\e004_005.bmp 1538 315 1 1 0 0
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap help obj\e004_006.bmp 1570 392 1 1 0 0
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap help obj\e004_007.bmp 1578 469 1 1 0 0
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap help obj\e004_008.bmp 1534 546 1 1 0 0
button back obj\e003_000.bmp 1738 831 10 5 9 swap help obj\e004_009.bmp 1628 777 1 1 0 0

screen save
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 128
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\e001_000.bmp 0 0 1 2 0 0
slots obj\e007_000.bmp 41 178 786 220 4 2
slotlabel obj\e008_000.bmp 18 9
slotdigits obj\e008_001.bmp 58 4
slotthumb 19 34
datedigits obj\e009_000.bmp 287 65
dateslash obj\e009_002.bmp
datecolon obj\e009_003.bmp
datedash obj\e009_001.bmp
slotlock obj\e011_000.bmp 15 139
slotnew obj\e012_000.bmp -6 -6
slotcomment 278 157 452 20 Click to set a comment
slotdelete obj\e011_001.bmp 672 19
dategap 21
button page0 obj\e005_000.bmp 458 64 11 5 0
button page1 obj\e005_000.bmp 558 64 11 5 1
button page2 obj\e005_000.bmp 658 64 11 5 2
button page3 obj\e005_000.bmp 758 64 11 5 3
button page4 obj\e005_000.bmp 858 64 11 5 4
button page5 obj\e005_000.bmp 958 64 11 5 5
button page6 obj\e005_000.bmp 1058 64 11 5 6
button page7 obj\e005_000.bmp 1158 64 11 5 7
button page8 obj\e005_000.bmp 1258 64 11 5 8
button page9 obj\e005_000.bmp 1358 64 11 5 9
button page10 obj\e005_000.bmp 1458 64 11 5 10
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap help obj\e004_000.bmp 1500 7 1 1 0 0
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap help obj\e004_001.bmp 1508 84 1 1 0 0
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap help obj\e004_002.bmp 1546 161 1 1 0 0
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap help obj\e004_004.bmp 1530 238 1 1 0 0
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap help obj\e004_005.bmp 1538 315 1 1 0 0
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap help obj\e004_006.bmp 1570 392 1 1 0 0
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap help obj\e004_007.bmp 1578 469 1 1 0 0
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap help obj\e004_008.bmp 1534 546 1 1 0 0
button back obj\e003_000.bmp 1738 831 10 5 9 swap help obj\e004_009.bmp 1628 777 1 1 0 0
screen cg
# The CG viewer's panel, measured under Wine.
viewer obj\j999_000.bmp 276 842
viewerhelp obj\j006_001.bmp 1410 1027 12
viewerbutton art obj\f999_001.bmp 320 949 helppic obj\j999_001.bmp 847 1027
viewerbutton bgm obj\j006_000.bmp 641 949 2 1
viewerbgm 1 2 0 0
viewerslider obj\j005_000.bmp 620 911 11
viewernumber volume obj\j003_001.bmp 734 875 20 3
viewerbutton zoomout obj\j004_000.bmp 840 949 3 0 help 8
viewerbutton zoomreset obj\j004_000.bmp 904 949 3 1 help 9
viewerbutton zoomin obj\j004_000.bmp 968 949 3 2 help 10
viewernumber zoom obj\j003_000.bmp 900 888 26 1
viewernumber zoomfrac obj\j003_001.bmp 938 896 20 2
viewerbutton replay obj\j002_000.bmp 1069 949 help 1
viewerbutton prevcg obj\j002_001.bmp 1228 949 6 0 help 3
viewerbutton prevvar obj\j002_001.bmp 1294 949 6 1 help 4
viewerbutton nextvar obj\j002_001.bmp 1549 949 6 2 help 5
viewerbutton nextcg obj\j002_001.bmp 1615 949 6 3 help 6
viewerbutton auto obj\j002_001.bmp 1712 949 6 4 help 7
viewerbutton return obj\j002_001.bmp 1807 949 6 5 help 2
viewerbutton hide obj\l001_003.bmp 1849 871 2 0 help 0
viewernumber index obj\j003_000.bmp 1396 923 26 2
viewernumber total obj\j003_001.bmp 1469 962 20 2
viewerauto 8000 0
pic bg\bg_003_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\f004_000.bmp 1000 0
pic obj\f000_000.bmp 0 0 1 2 0 0
cellsheet obj\f002_000.bmp
cellsize 272 234
cellthumb 7 48
celllabel obj\e008_000.bmp 5 22
celldigits obj\e008_001.bmp 45 17 3 18
cellcount obj\f003_000.bmp obj\f003_001.bmp 161 194 193 206
cell 147 156
cell 432 156
cell 717 156
cell 204 411
cell 489 411
cell 774 411
cell 147 665
cell 432 665
cell 717 665
button page0 obj\e005_000.bmp 473 949 11 5 0
button page1 obj\e005_000.bmp 573 949 11 5 1
button page2 obj\e005_000.bmp 673 949 11 5 2
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap help obj\e004_000.bmp 1500 7 1 1 0 0
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap help obj\e004_001.bmp 1508 84 1 1 0 0
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap help obj\e004_002.bmp 1546 161 1 1 0 0
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap help obj\e004_004.bmp 1530 238 1 1 0 0
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap help obj\e004_005.bmp 1538 315 1 1 0 0
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap help obj\e004_006.bmp 1570 392 1 1 0 0
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap help obj\e004_007.bmp 1578 469 1 1 0 0
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap help obj\e004_008.bmp 1534 546 1 1 0 0
button back obj\e003_000.bmp 1738 831 10 5 9 swap help obj\e004_009.bmp 1628 777 1 1 0 0

screen scene
pic bg\bg_003_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\f004_000.bmp 1000 0
pic obj\f000_000.bmp 0 0 1 2 0 1
cellsheet obj\f002_000.bmp
cellsize 272 234
cellthumb 7 48
celllabel obj\e008_000.bmp 5 22
celldigits obj\e008_001.bmp 45 17 3 18
cellcount obj\f003_000.bmp obj\f003_001.bmp 161 194 193 206
cell 147 156
cell 432 156
cell 717 156
cell 204 411
cell 489 411
cell 774 411
cell 147 665
cell 432 665
cell 717 665
button page0 obj\e005_000.bmp 473 949 11 5 0
button page1 obj\e005_000.bmp 573 949 11 5 1
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap help obj\e004_000.bmp 1500 7 1 1 0 0
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap help obj\e004_001.bmp 1508 84 1 1 0 0
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap help obj\e004_002.bmp 1546 161 1 1 0 0
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap help obj\e004_004.bmp 1530 238 1 1 0 0
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap help obj\e004_005.bmp 1538 315 1 1 0 0
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap help obj\e004_006.bmp 1570 392 1 1 0 0
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap help obj\e004_007.bmp 1578 469 1 1 0 0
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap help obj\e004_008.bmp 1534 546 1 1 0 0
button back obj\e003_000.bmp 1738 831 10 5 9 swap help obj\e004_009.bmp 1628 777 1 1 0 0

screen music
hover 410 97 816 152 obj\g006_007.bmp 462 156
pic obj\g000_000.bmp 0 0
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\g001_000.bmp 0 0
pic obj\g004_000.bmp 410 97
slider music obj\g004_001.bmp 473 100
number music obj\g005_000.bmp 651 44
button music_prev obj\g003_000.bmp 858 64 7 5 0 help obj\g006_000.bmp 854 156 1 1 0 0
button music_play obj\g003_000.bmp 958 64 7 5 1 help obj\g006_001.bmp 954 156 1 1 0 0
button music_pause obj\g003_000.bmp 1058 64 7 5 2
button music_stop obj\g003_000.bmp 1158 64 7 5 3
button music_next obj\g003_000.bmp 1258 64 7 5 4 help obj\g006_004.bmp 1254 156 1 1 0 0
button music_repeat obj\g003_000.bmp 1358 64 7 5 5 help obj\g006_005.bmp 1354 156 1 1 0 0
button music_shuffle obj\g003_000.bmp 1458 64 7 5 6 help obj\g006_006.bmp 1419 156 1 1 0 0
button track0 obj\g002_000.bmp 42 219 10 5 0
button track1 obj\g002_000.bmp 42 333 10 5 1
button track2 obj\g002_000.bmp 42 447 10 5 2
button track3 obj\g002_000.bmp 42 561 10 5 3
button track4 obj\g002_000.bmp 42 675 10 5 4
button track5 obj\g002_000.bmp 42 789 10 5 5
button track6 obj\g002_000.bmp 42 903 10 5 6
button track7 obj\g002_000.bmp 558 219 10 5 7
button track8 obj\g002_000.bmp 558 333 10 5 8
button track9 obj\g002_000.bmp 558 447 10 5 9
button track10 obj\g002_001.bmp 558 561 8 5 0
button track11 obj\g002_001.bmp 558 675 8 5 1
button track12 obj\g002_001.bmp 558 789 8 5 2
button track13 obj\g002_001.bmp 558 903 8 5 3
button track14 obj\g002_001.bmp 1074 219 8 5 4
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap help obj\e004_000.bmp 1500 7 1 1 0 0
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap help obj\e004_001.bmp 1508 84 1 1 0 0
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap help obj\e004_002.bmp 1546 161 1 1 0 0
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap help obj\e004_004.bmp 1530 238 1 1 0 0
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap help obj\e004_005.bmp 1538 315 1 1 0 0
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap help obj\e004_006.bmp 1570 392 1 1 0 0
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap help obj\e004_007.bmp 1578 469 1 1 0 0
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap help obj\e004_008.bmp 1534 546 1 1 0 0
button back obj\e003_000.bmp 1738 831 10 5 9 swap help obj\e004_009.bmp 1628 777 1 1 0 0

screen dialog
pic obj\b001_000.bmp 0 270
pic obj\i000_000.bmp 660 380 1 8 0 0
button yes obj\b003_000.bmp 720 476 2 5 0
button no obj\b003_000.bmp 1005 476 2 5 1
# The box that stops the question, whichever answer follows, as on the PC.
button noask obj\i001_000.bmp 767 707 1 5 0

screen choice
pic obj\n000_000.bmp 0 259
pic obj\n001_000.bmp 785 335
place 413 132

screen start
pic obj\d007_000.bmp 0 0
pic obj\d007_001.bmp 0 342
button start_prologue obj\d007_002.bmp 600 340 2 5 0
button start_main obj\d007_002.bmp 840 340 2 5 1

screen config
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 62 190 794 263 obj\h004_000.bmp 46 102
hover 62 271 794 422 obj\h004_001.bmp 46 217
hover 62 430 298 543 obj\h004_002.bmp 46 376
hover 310 430 546 543 obj\h004_003.bmp 297 343
hover 558 430 794 543 obj\h004_004.bmp 546 343
hover 62 551 794 1028 obj\h004_005.bmp 46 497
hover 825 190 1559 629 obj\h004_006.bmp 812 102
hover 825 639 1060 751 obj\h004_007.bmp 810 552
hover 1073 639 1559 751 obj\l999_001.bmp 1058 552
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\h000_000.bmp 0 0
pic obj\h005_000.bmp 53 181
button tab obj\h002_000.bmp 432 70 3 5 0 help obj\h002_001.bmp 428 156 1 1 0 0
button tab2 obj\h002_000.bmp 608 70 3 5 1 help obj\h002_002.bmp 602 156 1 1 0 0
button tab3 obj\h002_000.bmp 784 70 3 5 2 help obj\h002_003.bmp 780 156 1 1 0 0
button cfg_defaults obj\h001_000.bmp 1010 70 3 5 0 help obj\h001_001.bmp 1006 156 1 1 0 0
button cfg_undo obj\h001_000.bmp 1186 70 3 5 1 help obj\h001_002.bmp 1182 156 1 1 0 0
button cfg_apply obj\h001_000.bmp 1362 70 3 5 2 help obj\h001_003.bmp 1358 156 1 1 0 0
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap help obj\e004_000.bmp 1500 7 1 1 0 0
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap help obj\e004_001.bmp 1508 84 1 1 0 0
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap help obj\e004_002.bmp 1546 161 1 1 0 0
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap help obj\e004_004.bmp 1530 238 1 1 0 0
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap help obj\e004_005.bmp 1538 315 1 1 0 0
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap help obj\e004_006.bmp 1570 392 1 1 0 0
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap help obj\e004_007.bmp 1578 469 1 1 0 0
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap help obj\e004_008.bmp 1534 546 1 1 0 0
button back obj\e003_000.bmp 1738 831 10 5 9 swap help obj\e004_009.bmp 1628 777 1 1 0 0
radio screen 0 obj\h006_000.bmp 236 208 2 5 0
radio screen 1 obj\h006_000.bmp 418 208 2 5 1
pic obj\h007_000.bmp 236 297 6 5 0 4
pic obj\h007_000.bmp 418 297 6 5 1 4
pic obj\h007_000.bmp 599 297 6 5 2 4
pic obj\h007_000.bmp 236 355 6 5 3 4
pic obj\h007_000.bmp 418 355 6 5 4 4
pic obj\h007_000.bmp 599 355 6 5 5 4
radio effect 1 obj\h008_000.bmp 63 482 2 5 0
radio effect 0 obj\h008_000.bmp 181 482 2 5 1
radio wait 1 obj\h008_000.bmp 312 482 2 5 0
radio wait 0 obj\h008_000.bmp 430 482 2 5 1
radio waitskip 1 obj\h008_000.bmp 561 482 2 5 0
radio waitskip 0 obj\h008_000.bmp 679 482 2 5 1
radio camera 1 obj\h008_000.bmp 827 687 2 5 0
radio camera 0 obj\h008_000.bmp 945 687 2 5 1
radio art 1 obj\h999_000.bmp 1077 687 2 5 0
radio art 0 obj\h999_000.bmp 1318 687 2 5 1
slider opacity obj\h009_000.bmp 208 612
number opacity obj\h011_000.bmp 573 568
step opacity -1 obj\h010_000.bmp 140 613 2 5 0
step opacity 1 obj\h010_000.bmp 680 613 2 5 1
check confirm_saveload obj\h013_000.bmp 842 245 8 5 0
check confirm_overwrite obj\h013_000.bmp 842 291 8 5 1
check confirm_delete obj\h013_000.bmp 842 337 8 5 2
check confirm_quick obj\h013_000.bmp 842 383 8 5 3
check confirm_title obj\h013_000.bmp 842 429 8 5 4
check confirm_exit obj\h013_000.bmp 842 475 8 5 5
check tutorial obj\h013_000.bmp 842 521 8 5 6
pic obj\h013_000.bmp 842 567 8 5 7 4

screen config2
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 61 189 794 270 obj\h004_008.bmp 46 168
hover 61 279 794 361 obj\h004_009.bmp 46 254
hover 64 749 789 868 obj\h004_010.bmp 46 659
hover 825 189 1184 301 obj\h004_011.bmp 810 102
hover 1200 189 1559 301 obj\h004_012.bmp 1186 102
hover 825 310 1559 459 obj\h004_013.bmp 810 256
hover 825 469 1184 581 obj\h004_014.bmp 810 382
hover 1200 469 1559 581 obj\h004_015.bmp 1186 382
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\h000_000.bmp 0 0
pic obj\h015_000.bmp 53 181
button tab obj\h002_000.bmp 432 70 3 5 0 help obj\h002_001.bmp 428 156 1 1 0 0
button tab2 obj\h002_000.bmp 608 70 3 5 1 help obj\h002_002.bmp 602 156 1 1 0 0
button tab3 obj\h002_000.bmp 784 70 3 5 2 help obj\h002_003.bmp 780 156 1 1 0 0
button cfg_defaults obj\h001_000.bmp 1010 70 3 5 0 help obj\h001_001.bmp 1006 156 1 1 0 0
button cfg_undo obj\h001_000.bmp 1186 70 3 5 1 help obj\h001_002.bmp 1182 156 1 1 0 0
button cfg_apply obj\h001_000.bmp 1362 70 3 5 2 help obj\h001_003.bmp 1358 156 1 1 0 0
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap help obj\e004_000.bmp 1500 7 1 1 0 0
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap help obj\e004_001.bmp 1508 84 1 1 0 0
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap help obj\e004_002.bmp 1546 161 1 1 0 0
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap help obj\e004_004.bmp 1530 238 1 1 0 0
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap help obj\e004_005.bmp 1538 315 1 1 0 0
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap help obj\e004_006.bmp 1570 392 1 1 0 0
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap help obj\e004_007.bmp 1578 469 1 1 0 0
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap help obj\e004_008.bmp 1534 546 1 1 0 0
button back obj\e003_000.bmp 1738 831 10 5 9 swap help obj\e004_009.bmp 1628 777 1 1 0 0
radio text 0 obj\h016_000.bmp 251 208 4 5 0
radio text 1 obj\h016_000.bmp 373 208 4 5 1
radio text 2 obj\h016_000.bmp 495 208 4 5 2
radio text 3 obj\h016_000.bmp 617 208 4 5 3
radio auto 0 obj\h016_000.bmp 251 294 4 5 0
radio auto 1 obj\h016_000.bmp 373 294 4 5 1
radio auto 2 obj\h016_000.bmp 495 294 4 5 2
radio auto 3 obj\h016_000.bmp 617 294 4 5 3
radio skipall 0 obj\h018_000.bmp 889 238 2 5 0
radio skipall 1 obj\h018_000.bmp 1007 238 2 5 1
radio deckey 0 obj\h019_000.bmp 1263 238 2 5 0
radio deckey 1 obj\h019_000.bmp 1381 238 2 5 1
radio rightclick 0 obj\h020_000.bmp 1005 334 5 5 0
radio rightclick 1 obj\h020_000.bmp 1187 334 5 5 1
radio rightclick 2 obj\h020_000.bmp 1368 334 5 5 2
radio rightclick 3 obj\h020_000.bmp 1005 392 5 5 3
radio rightclick 4 obj\h020_000.bmp 1187 392 5 5 4
radio pause_inactive 1 obj\h008_000.bmp 889 521 2 5 0
radio pause_inactive 0 obj\h008_000.bmp 1007 521 2 5 1
pic obj\h008_000.bmp 1263 521 2 5 0 4
pic obj\h008_000.bmp 1381 521 2 5 1 4
# Wait time, ×0.70 to ×1.30 in steps of 0.01, stretches waits and fades; the bar is full at ×2.00.
range waittime 70 130
slider waittime obj\h009_000.bmp 208 814 200
number waittime obj\h011_000.bmp 563 771 597 623
step waittime -1 obj\h010_000.bmp 140 816 2 5 0
step waittime 1 obj\h010_000.bmp 680 816 2 5 1

screen config3
# Help shown over each settings group, its edges found to the pixel under Wine by moving the pointer across them.
hover 61 189 794 316 obj\h004_016.bmp 46 135
hover 61 325 794 454 obj\h004_017.bmp 46 272
hover 61 464 794 592 obj\h004_018.bmp 46 409
hover 61 600 794 728 obj\h004_019.bmp 46 546
hover 61 737 794 865 obj\h004_020.bmp 46 683
hover 61 874 794 1000 obj\h004_021.bmp 46 820
hover 825 189 1183 299 obj\h004_022.bmp 810 102
hover 825 310 1557 1000 obj\h004_023.bmp 810 223
pic bg\bg_002_000_000_000_000.bmp 0 0
pic obj\e000_000.bmp 0 0 alpha 110
pic obj\e000_001.bmp 0 0
pic obj\e002_000.bmp 1596 0
pic obj\h000_000.bmp 0 0
pic obj\h021_000.bmp 53 181
button tab obj\h002_000.bmp 432 70 3 5 0 help obj\h002_001.bmp 428 156 1 1 0 0
button tab2 obj\h002_000.bmp 608 70 3 5 1 help obj\h002_002.bmp 602 156 1 1 0 0
button tab3 obj\h002_000.bmp 784 70 3 5 2 help obj\h002_003.bmp 780 156 1 1 0 0
button cfg_defaults obj\h001_000.bmp 1010 70 3 5 0 help obj\h001_001.bmp 1006 156 1 1 0 0
button cfg_undo obj\h001_000.bmp 1186 70 3 5 1 help obj\h001_002.bmp 1182 156 1 1 0 0
button cfg_apply obj\h001_000.bmp 1362 70 3 5 2 help obj\h001_003.bmp 1358 156 1 1 0 0
button nav_save obj\e003_000.bmp 1658 61 10 5 0 swap help obj\e004_000.bmp 1500 7 1 1 0 0
button nav_load obj\e003_000.bmp 1666 138 10 5 1 swap help obj\e004_001.bmp 1508 84 1 1 0 0
button nav_cg obj\e003_000.bmp 1674 215 10 5 2 swap help obj\e004_002.bmp 1546 161 1 1 0 0
button nav_scene obj\e003_000.bmp 1682 292 10 5 4 swap help obj\e004_004.bmp 1530 238 1 1 0 0
button nav_music obj\e003_000.bmp 1690 369 10 5 5 swap help obj\e004_005.bmp 1538 315 1 1 0 0
button nav_config obj\e003_000.bmp 1698 446 10 5 6 swap help obj\e004_006.bmp 1570 392 1 1 0 0
button nav_title obj\e003_000.bmp 1706 523 10 5 7 swap help obj\e004_007.bmp 1578 469 1 1 0 0
button nav_quit obj\e003_000.bmp 1714 600 10 5 8 swap help obj\e004_008.bmp 1534 546 1 1 0 0
button back obj\e003_000.bmp 1738 831 10 5 9 swap help obj\e004_009.bmp 1628 777 1 1 0 0
slider master obj\h009_000.bmp 208 255
number master obj\h011_000.bmp 552 212
step master -1 obj\h010_000.bmp 140 257 2 5 0
step master 1 obj\h010_000.bmp 680 257 2 5 1
check mute_master obj\h024_000.bmp 656 201 1 5 0
pic obj\h023_000.bmp 746 199 1 5 0 0
slider bgm obj\h009_000.bmp 208 392
number bgm obj\h011_000.bmp 552 349
step bgm -1 obj\h010_000.bmp 140 394 2 5 0
step bgm 1 obj\h010_000.bmp 680 394 2 5 1
check mute_bgm obj\h024_000.bmp 656 338 1 5 0
pic obj\h023_000.bmp 746 336 1 5 0 0
slider se obj\h009_000.bmp 208 529
number se obj\h011_000.bmp 552 486
step se -1 obj\h010_000.bmp 140 531 2 5 0
step se 1 obj\h010_000.bmp 680 531 2 5 1
check mute_se obj\h024_000.bmp 656 475 1 5 0
pic obj\h023_000.bmp 746 473 1 5 0 0
bar obj\h009_000.bmp 208 666 80
pic obj\h011_000.bmp 552 623 10 1 0 0
pic obj\h011_000.bmp 578 623 10 1 8 0
pic obj\h011_000.bmp 604 623 10 1 0 0
pic obj\h010_000.bmp 140 668 2 5 0 4
pic obj\h010_000.bmp 680 668 2 5 1 4
pic obj\h024_000.bmp 656 612 1 5 0 4
pic obj\h023_000.bmp 746 610 1 5 0 0
slider voice obj\h009_000.bmp 208 803
number voice obj\h011_000.bmp 552 760
step voice -1 obj\h010_000.bmp 140 805 2 5 0
step voice 1 obj\h010_000.bmp 680 805 2 5 1
check mute_voice obj\h024_000.bmp 656 749 1 5 0
pic obj\h023_000.bmp 746 747 1 5 0 0
bar obj\h009_000.bmp 208 940 80
pic obj\h011_000.bmp 552 897 10 1 0 0
pic obj\h011_000.bmp 578 897 10 1 8 0
pic obj\h011_000.bmp 604 897 10 1 0 0
pic obj\h010_000.bmp 140 942 2 5 0 4
pic obj\h010_000.bmp 680 942 2 5 1 4
pic obj\h024_000.bmp 656 886 1 5 0 4
pic obj\h023_000.bmp 746 884 1 5 0 0
pic obj\h008_000.bmp 889 238 2 5 0 4
pic obj\h008_000.bmp 1007 238 2 5 1 4
check voice0 obj\h022_000.bmp 919 868 4 5 0
check voice2 obj\h022_000.bmp 1207 955 4 5 2
check voice3 obj\h022_000.bmp 1384 955 4 5 3

screen log
dim 216
pic obj\m001_000.bmp 1771 163
scroll obj\m002_000t.bmp obj\m002_000m.bmp obj\m002_000b.bmp 1774 163 756
# Four entries a page, each under a rule, in the message window's face and size, placed from the Wine capture with
# the two faces' metrics.
rows obj\m000_000.bmp 117 58 250 4 125 56
button log_close obj\l001_003.bmp 1851 21 2 5 0
button log_top obj\m003_000.bmp 1770 51 4 5 0
button log_up obj\m003_000.bmp 1770 102 4 5 1
button log_down obj\m003_000.bmp 1770 930 4 5 2
button log_bottom obj\m003_000.bmp 1770 980 4 5 3

screen cfgsav
# VNDegen 0.26.1 set GLB_ENDING (1050) at the prologue's end in place of GLB_PROLOGUE_OVER (1000); an ending means the
# prologue was seen.
implies 1050 1000
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
run 200 1 1
run 212 1 1
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
# The official English release sets its text in Lucida Sans Unicode (Wine's font log), here Open Sans, at the size and
# letter gap that best fit the word widths of the publisher's Windows screenshots, lines as far apart as there, and the
# first glyph one gap in. Rendered here and laid over three screenshot lines, the baseline sits 3 lower.
textfont 7
textsize 22
textgap 5.93
textline 42
textshift 6 3
)";
