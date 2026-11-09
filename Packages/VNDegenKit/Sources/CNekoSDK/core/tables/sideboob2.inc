// Test Game 87's (Funbag Fantasy: Sideboob Story 2) table, a raw string that tables.cpp includes and menuTable() hands
// out. Its story order is in its exe, so the table gives it.
const char kSideboob2[] = R"(
screen title
# Measured under Wine. The background holds the frame and the copyright, and the logo goes over it. Each button's help,
# a card from d002_000, shows 88 pixels above it, the submenus' from d002_001 beside them. A submenu dims the five
# buttons (their last row) and shows no help for them, and the right button closes it.
submenudim
pic obj\d000_000.bmp 0 0
pic obj\d000_001.bmp 268 55
button start obj\d001_000.bmp 530 422 5 5 0 help obj\d002_000.bmp 382 334 6 1 0 0
button load obj\d001_000.bmp 530 471 5 5 1 help obj\d002_000.bmp 382 383 6 1 1 0
button submenu:extras obj\d001_000.bmp 530 520 5 5 2 help obj\d002_000.bmp 382 432 6 1 2 0
button config obj\d001_000.bmp 530 569 5 5 3 help obj\d002_000.bmp 382 481 6 1 3 0
button quit obj\d001_000.bmp 530 618 5 5 4 help obj\d002_000.bmp 382 530 6 1 4 0
# Once chapter 2 has begun (G 2), Start offers the prologue or chapter 2.
startmenu 2
button start_prologue obj\d001_002.bmp 756 422 6 5 0 group start help obj\d002_001.bmp 608 334 6 1 0 0
button start_main obj\d001_002.bmp 756 471 6 5 1 group start help obj\d002_001.bmp 608 383 6 1 1 0
button cg obj\d001_002.bmp 756 422 6 5 2 group extras help obj\d002_001.bmp 608 334 6 1 2 0
button sprites obj\d001_002.bmp 756 471 6 5 3 group extras help obj\d002_001.bmp 608 383 6 1 3 0
button scene obj\d001_002.bmp 756 520 6 5 4 group extras help obj\d002_001.bmp 608 432 6 1 4 0

# Start's Chapter 2 starts the story table's flow of that name.
screen start
flow chapter2

# Save and load, measured under Wine: nine cards a page on ten pages, each card a save's number, thumbnail, date and
# time, and NEW on the one saved last. The side bar's help steps 48 pixels a button while the buttons step 52. Still
# to add: the card's "Save Date" label and its chapter name, which the runtime can't draw yet, and the backdrop the
# paper's torn edges show, which no picture in the archives matches.
screen load
pic obj\e000_000.bmp 0 3
pic obj\e000_001.bmp 75 14 7 1 1 0
pic obj\e001_000.bmp 1154 0
slots obj\e003_000.bmp 30 94 369 203 3 3 1
slotlabel obj\e003_001.bmp 5 5
slotdigits obj\e003_002.bmp 39 5 3
slotthumb 7 32
slotnew obj\e003_010.bmp 289 -18
datedigits obj\e003_004.bmp 203 59
dateslash obj\e003_003.bmp
datecolon obj\e003_006.bmp
datedash obj\e003_005.bmp
timeat 270 86
button page0 obj\e002_000.bmp 416 18 10 5 0
button page1 obj\e002_000.bmp 486 18 10 5 1
button page2 obj\e002_000.bmp 556 18 10 5 2
button page3 obj\e002_000.bmp 626 18 10 5 3
button page4 obj\e002_000.bmp 696 18 10 5 4
button page5 obj\e002_000.bmp 766 18 10 5 5
button page6 obj\e002_000.bmp 836 18 10 5 6
button page7 obj\e002_000.bmp 906 18 10 5 7
button page8 obj\e002_000.bmp 976 18 10 5 8
button page9 obj\e002_000.bmp 1046 18 10 5 9
button nav_save obj\e001_001.bmp 1168 103 10 5 0 help obj\e001_002.bmp 1044 15 10 1 0 0
button nav_load obj\e001_001.bmp 1168 155 10 5 1 help obj\e001_002.bmp 1044 63 10 1 1 0
button nav_cg obj\e001_001.bmp 1168 207 10 5 2 help obj\e001_002.bmp 1044 111 10 1 2 0
button nav_sprites obj\e001_001.bmp 1168 259 10 5 3 help obj\e001_002.bmp 1044 159 10 1 3 0
button nav_scene obj\e001_001.bmp 1168 311 10 5 4 help obj\e001_002.bmp 1044 207 10 1 4 0
button nav_music obj\e001_001.bmp 1168 363 10 5 5 help obj\e001_002.bmp 1044 255 10 1 5 0
button nav_config obj\e001_001.bmp 1168 415 10 5 6 help obj\e001_002.bmp 1044 303 10 1 6 0
button nav_title obj\e001_001.bmp 1168 467 10 5 7 help obj\e001_002.bmp 1044 351 10 1 7 0
button nav_quit obj\e001_001.bmp 1168 519 10 5 8 help obj\e001_002.bmp 1044 399 10 1 8 0
button back obj\e001_001.bmp 1168 623 10 5 9 help obj\e001_002.bmp 1044 575 10 1 9 0

screen save
pic obj\e000_000.bmp 0 3
pic obj\e000_001.bmp 75 14 7 1 0 0
pic obj\e001_000.bmp 1154 0
slots obj\e003_000.bmp 30 94 369 203 3 3 1
slotlabel obj\e003_001.bmp 5 5
slotdigits obj\e003_002.bmp 39 5 3
slotthumb 7 32
slotnew obj\e003_010.bmp 289 -18
datedigits obj\e003_004.bmp 203 59
dateslash obj\e003_003.bmp
datecolon obj\e003_006.bmp
datedash obj\e003_005.bmp
timeat 270 86
button page0 obj\e002_000.bmp 416 18 10 5 0
button page1 obj\e002_000.bmp 486 18 10 5 1
button page2 obj\e002_000.bmp 556 18 10 5 2
button page3 obj\e002_000.bmp 626 18 10 5 3
button page4 obj\e002_000.bmp 696 18 10 5 4
button page5 obj\e002_000.bmp 766 18 10 5 5
button page6 obj\e002_000.bmp 836 18 10 5 6
button page7 obj\e002_000.bmp 906 18 10 5 7
button page8 obj\e002_000.bmp 976 18 10 5 8
button page9 obj\e002_000.bmp 1046 18 10 5 9
button nav_save obj\e001_001.bmp 1168 103 10 5 0 help obj\e001_002.bmp 1044 15 10 1 0 0
button nav_load obj\e001_001.bmp 1168 155 10 5 1 help obj\e001_002.bmp 1044 63 10 1 1 0
button nav_cg obj\e001_001.bmp 1168 207 10 5 2 help obj\e001_002.bmp 1044 111 10 1 2 0
button nav_sprites obj\e001_001.bmp 1168 259 10 5 3 help obj\e001_002.bmp 1044 159 10 1 3 0
button nav_scene obj\e001_001.bmp 1168 311 10 5 4 help obj\e001_002.bmp 1044 207 10 1 4 0
button nav_music obj\e001_001.bmp 1168 363 10 5 5 help obj\e001_002.bmp 1044 255 10 1 5 0
button nav_config obj\e001_001.bmp 1168 415 10 5 6 help obj\e001_002.bmp 1044 303 10 1 6 0
button nav_title obj\e001_001.bmp 1168 467 10 5 7 help obj\e001_002.bmp 1044 351 10 1 7 0
button nav_quit obj\e001_001.bmp 1168 519 10 5 8 help obj\e001_002.bmp 1044 399 10 1 8 0
button back obj\e001_001.bmp 1168 623 10 5 9 help obj\e001_002.bmp 1044 575 10 1 9 0

# Config, four tabs, measured under Wine (fitted to the PC's shots, hover areas found by halving to each edge).
# Its sliders are a rose knob on a fixed track, x0 + 6 a step, which the runtime can't draw yet: their values, digits
# and arrows are here. Text and auto speed run 0 to 100 on the PC.
screen config
pic obj\e000_000.bmp 0 3
pic obj\e000_001.bmp 75 14 7 1 6 0
pic obj\e001_000.bmp 1154 0
button nav_save obj\e001_001.bmp 1168 103 10 5 0 help obj\e001_002.bmp 1044 15 10 1 0 0
button nav_load obj\e001_001.bmp 1168 155 10 5 1 help obj\e001_002.bmp 1044 63 10 1 1 0
button nav_cg obj\e001_001.bmp 1168 207 10 5 2 help obj\e001_002.bmp 1044 111 10 1 2 0
button nav_sprites obj\e001_001.bmp 1168 259 10 5 3 help obj\e001_002.bmp 1044 159 10 1 3 0
button nav_scene obj\e001_001.bmp 1168 311 10 5 4 help obj\e001_002.bmp 1044 207 10 1 4 0
button nav_music obj\e001_001.bmp 1168 363 10 5 5 help obj\e001_002.bmp 1044 255 10 1 5 0
button nav_config obj\e001_001.bmp 1168 415 10 5 6 help obj\e001_002.bmp 1044 303 10 1 6 0
button nav_title obj\e001_001.bmp 1168 467 10 5 7 help obj\e001_002.bmp 1044 351 10 1 7 0
button nav_quit obj\e001_001.bmp 1168 519 10 5 8 help obj\e001_002.bmp 1044 399 10 1 8 0
button back obj\e001_001.bmp 1168 623 10 5 9 help obj\e001_002.bmp 1044 575 10 1 9 0
button tab obj\j002_000.bmp 435 34 4 5 0
button tab2 obj\j002_000.bmp 568 34 4 5 1
button tab3 obj\j002_000.bmp 701 34 4 5 2
button tab4 obj\j002_000.bmp 834 34 4 5 3
button cfg_apply obj\j000_000.bmp 992 100 3 5 0 help obj\j001_000.bmp 848 12 3 1 0 0
button cfg_undo obj\j000_000.bmp 992 158 3 5 1 help obj\j001_000.bmp 848 70 3 1 1 0
button cfg_defaults obj\j000_000.bmp 992 216 3 5 2 help obj\j001_000.bmp 848 128 3 1 2 0
pic obj\j003_000.bmp 53 106
radio screen 0 obj\j003_001.bmp 423 153 12 5 0
radio screen 1 obj\j003_001.bmp 537 153 12 5 1
pic obj\j003_001.bmp 423 189 12 5 2 4
pic obj\j003_001.bmp 537 189 12 5 3 4
pic obj\j003_001.bmp 423 225 12 5 4 4
pic obj\j003_001.bmp 537 225 12 5 5 4
pic obj\j003_001.bmp 651 225 12 5 6 4
pic obj\j003_001.bmp 423 261 12 5 7 4
pic obj\j003_001.bmp 537 261 12 5 8 4
pic obj\j003_001.bmp 651 261 12 5 9 4
radio effect 1 obj\j013_000.bmp 423 297 2 5 0
radio effect 0 obj\j013_000.bmp 537 297 2 5 1
radio wait 1 obj\j013_000.bmp 423 333 2 5 0
radio wait 0 obj\j013_000.bmp 537 333 2 5 1
number opacity obj\j003_004.bmp 762 366
step opacity -1 obj\j003_003.bmp 148 408 2 5 0
step opacity 1 obj\j003_003.bmp 820 408 2 5 1
radio confirm_saveload 1 obj\j013_000.bmp 423 456 2 5 0
radio confirm_saveload 0 obj\j013_000.bmp 537 456 2 5 1
group confirm_saveload confirm_overwrite confirm_delete confirm_quick confirm_title confirm_exit
radio tutorial 1 obj\j013_000.bmp 423 492 2 5 0
radio tutorial 0 obj\j013_000.bmp 537 492 2 5 1
radio waitskip 1 obj\j013_000.bmp 423 528 2 5 0
radio waitskip 0 obj\j013_000.bmp 537 528 2 5 1
pic obj\a001_002.bmp 280 571 1 5 0 4
hover 134 140 870 180 obj\j004_000.bmp 34 62 9 0
hover 134 185 870 216 obj\j004_000.bmp 34 98 9 1
hover 134 221 870 287 obj\j004_000.bmp 34 134 9 2
hover 134 292 870 324 obj\j004_000.bmp 34 206 9 3
hover 134 328 870 360 obj\j004_000.bmp 34 242 9 4
hover 134 364 870 446 obj\j004_000.bmp 34 278 9 5
hover 134 451 870 483 obj\j004_000.bmp 34 365 9 6
hover 134 487 870 519 obj\j004_000.bmp 34 401 9 7
hover 134 521 870 553 obj\j004_000.bmp 34 434 9 8

screen config2
pic obj\e000_000.bmp 0 3
pic obj\e000_001.bmp 75 14 7 1 6 0
pic obj\e001_000.bmp 1154 0
button nav_save obj\e001_001.bmp 1168 103 10 5 0 help obj\e001_002.bmp 1044 15 10 1 0 0
button nav_load obj\e001_001.bmp 1168 155 10 5 1 help obj\e001_002.bmp 1044 63 10 1 1 0
button nav_cg obj\e001_001.bmp 1168 207 10 5 2 help obj\e001_002.bmp 1044 111 10 1 2 0
button nav_sprites obj\e001_001.bmp 1168 259 10 5 3 help obj\e001_002.bmp 1044 159 10 1 3 0
button nav_scene obj\e001_001.bmp 1168 311 10 5 4 help obj\e001_002.bmp 1044 207 10 1 4 0
button nav_music obj\e001_001.bmp 1168 363 10 5 5 help obj\e001_002.bmp 1044 255 10 1 5 0
button nav_config obj\e001_001.bmp 1168 415 10 5 6 help obj\e001_002.bmp 1044 303 10 1 6 0
button nav_title obj\e001_001.bmp 1168 467 10 5 7 help obj\e001_002.bmp 1044 351 10 1 7 0
button nav_quit obj\e001_001.bmp 1168 519 10 5 8 help obj\e001_002.bmp 1044 399 10 1 8 0
button back obj\e001_001.bmp 1168 623 10 5 9 help obj\e001_002.bmp 1044 575 10 1 9 0
button tab obj\j002_000.bmp 435 34 4 5 0
button tab2 obj\j002_000.bmp 568 34 4 5 1
button tab3 obj\j002_000.bmp 701 34 4 5 2
button tab4 obj\j002_000.bmp 834 34 4 5 3
button cfg_apply obj\j000_000.bmp 992 100 3 5 0 help obj\j001_000.bmp 848 12 3 1 0 0
button cfg_undo obj\j000_000.bmp 992 158 3 5 1 help obj\j001_000.bmp 848 70 3 1 1 0
button cfg_defaults obj\j000_000.bmp 992 216 3 5 2 help obj\j001_000.bmp 848 128 3 1 2 0
pic obj\j007_000.bmp 53 106
number text obj\j003_004.bmp 762 150
step text -1 obj\j003_003.bmp 148 192 2 5 0
step text 1 obj\j003_003.bmp 820 192 2 5 1
number auto obj\j003_004.bmp 762 245
step auto -1 obj\j003_003.bmp 148 287 2 5 0
step auto 1 obj\j003_003.bmp 820 287 2 5 1
number waittime obj\j003_004.bmp 756 340 780 798
step waittime -1 obj\j003_003.bmp 148 382 2 5 0
step waittime 1 obj\j003_003.bmp 820 382 2 5 1
hover 134 150 870 228 obj\j008_000.bmp 34 62 3 0
hover 134 233 870 323 obj\j008_000.bmp 34 157 3 1
hover 134 328 870 419 obj\j008_000.bmp 34 252 3 2

screen config3
pic obj\e000_000.bmp 0 3
pic obj\e000_001.bmp 75 14 7 1 6 0
pic obj\e001_000.bmp 1154 0
button nav_save obj\e001_001.bmp 1168 103 10 5 0 help obj\e001_002.bmp 1044 15 10 1 0 0
button nav_load obj\e001_001.bmp 1168 155 10 5 1 help obj\e001_002.bmp 1044 63 10 1 1 0
button nav_cg obj\e001_001.bmp 1168 207 10 5 2 help obj\e001_002.bmp 1044 111 10 1 2 0
button nav_sprites obj\e001_001.bmp 1168 259 10 5 3 help obj\e001_002.bmp 1044 159 10 1 3 0
button nav_scene obj\e001_001.bmp 1168 311 10 5 4 help obj\e001_002.bmp 1044 207 10 1 4 0
button nav_music obj\e001_001.bmp 1168 363 10 5 5 help obj\e001_002.bmp 1044 255 10 1 5 0
button nav_config obj\e001_001.bmp 1168 415 10 5 6 help obj\e001_002.bmp 1044 303 10 1 6 0
button nav_title obj\e001_001.bmp 1168 467 10 5 7 help obj\e001_002.bmp 1044 351 10 1 7 0
button nav_quit obj\e001_001.bmp 1168 519 10 5 8 help obj\e001_002.bmp 1044 399 10 1 8 0
button back obj\e001_001.bmp 1168 623 10 5 9 help obj\e001_002.bmp 1044 575 10 1 9 0
button tab obj\j002_000.bmp 435 34 4 5 0
button tab2 obj\j002_000.bmp 568 34 4 5 1
button tab3 obj\j002_000.bmp 701 34 4 5 2
button tab4 obj\j002_000.bmp 834 34 4 5 3
button cfg_apply obj\j000_000.bmp 992 100 3 5 0 help obj\j001_000.bmp 848 12 3 1 0 0
button cfg_undo obj\j000_000.bmp 992 158 3 5 1 help obj\j001_000.bmp 848 70 3 1 1 0
button cfg_defaults obj\j000_000.bmp 992 216 3 5 2 help obj\j001_000.bmp 848 128 3 1 2 0
pic obj\j009_000.bmp 53 106
radio skipall 0 obj\j009_001.bmp 423 153 11 5 0
radio skipall 1 obj\j009_001.bmp 537 153 11 5 1
radio rightclick 0 obj\j009_001.bmp 423 189 11 5 2
radio rightclick 1 obj\j009_001.bmp 537 189 11 5 3
radio rightclick 2 obj\j009_001.bmp 651 189 11 5 4
radio rightclick 3 obj\j009_001.bmp 423 225 11 5 5
radio rightclick 4 obj\j009_001.bmp 537 225 11 5 6
radio deckey 0 obj\j009_001.bmp 423 261 11 5 7
radio deckey 1 obj\j009_001.bmp 537 261 11 5 8
radio pause_inactive 1 obj\j009_001.bmp 423 297 11 5 9
radio pause_inactive 0 obj\j009_001.bmp 537 297 11 5 10
hover 134 144 870 177 obj\j010_000.bmp 34 62 4 0
hover 134 182 870 249 obj\j010_000.bmp 34 98 4 1
hover 134 255 870 286 obj\j010_000.bmp 34 170 4 2
hover 134 291 870 323 obj\j010_000.bmp 34 206 4 3

screen config4
pic obj\e000_000.bmp 0 3
pic obj\e000_001.bmp 75 14 7 1 6 0
pic obj\e001_000.bmp 1154 0
button nav_save obj\e001_001.bmp 1168 103 10 5 0 help obj\e001_002.bmp 1044 15 10 1 0 0
button nav_load obj\e001_001.bmp 1168 155 10 5 1 help obj\e001_002.bmp 1044 63 10 1 1 0
button nav_cg obj\e001_001.bmp 1168 207 10 5 2 help obj\e001_002.bmp 1044 111 10 1 2 0
button nav_sprites obj\e001_001.bmp 1168 259 10 5 3 help obj\e001_002.bmp 1044 159 10 1 3 0
button nav_scene obj\e001_001.bmp 1168 311 10 5 4 help obj\e001_002.bmp 1044 207 10 1 4 0
button nav_music obj\e001_001.bmp 1168 363 10 5 5 help obj\e001_002.bmp 1044 255 10 1 5 0
button nav_config obj\e001_001.bmp 1168 415 10 5 6 help obj\e001_002.bmp 1044 303 10 1 6 0
button nav_title obj\e001_001.bmp 1168 467 10 5 7 help obj\e001_002.bmp 1044 351 10 1 7 0
button nav_quit obj\e001_001.bmp 1168 519 10 5 8 help obj\e001_002.bmp 1044 399 10 1 8 0
button back obj\e001_001.bmp 1168 623 10 5 9 help obj\e001_002.bmp 1044 575 10 1 9 0
button tab obj\j002_000.bmp 435 34 4 5 0
button tab2 obj\j002_000.bmp 568 34 4 5 1
button tab3 obj\j002_000.bmp 701 34 4 5 2
button tab4 obj\j002_000.bmp 834 34 4 5 3
button cfg_apply obj\j000_000.bmp 992 100 3 5 0 help obj\j001_000.bmp 848 12 3 1 0 0
button cfg_undo obj\j000_000.bmp 992 158 3 5 1 help obj\j001_000.bmp 848 70 3 1 1 0
button cfg_defaults obj\j000_000.bmp 992 216 3 5 2 help obj\j001_000.bmp 848 128 3 1 2 0
pic obj\j011_000.bmp 53 106
number master obj\j003_004.bmp 800 142
step master -1 obj\j003_003.bmp 186 180 2 5 0
step master 1 obj\j003_003.bmp 858 180 2 5 1
check mute_master obj\j014_000.bmp 92 177 1 5 0
number bgm obj\j003_004.bmp 800 220
step bgm -1 obj\j003_003.bmp 186 258 2 5 0
step bgm 1 obj\j003_003.bmp 858 258 2 5 1
check mute_bgm obj\j014_000.bmp 92 255 1 5 0
number se obj\j003_004.bmp 800 298
step se -1 obj\j003_003.bmp 186 336 2 5 0
step se 1 obj\j003_003.bmp 858 336 2 5 1
check mute_se obj\j014_000.bmp 92 333 1 5 0
number hse obj\j003_004.bmp 800 376
step hse -1 obj\j003_003.bmp 186 414 2 5 0
step hse 1 obj\j003_003.bmp 858 414 2 5 1
check mute_hse obj\j014_000.bmp 92 411 1 5 0
number voice obj\j003_004.bmp 800 454
step voice -1 obj\j003_003.bmp 186 492 2 5 0
step voice 1 obj\j003_003.bmp 858 492 2 5 1
check mute_voice obj\j014_000.bmp 92 489 1 5 0
number backvoice obj\j003_004.bmp 800 532
step backvoice -1 obj\j003_003.bmp 186 570 2 5 0
step backvoice 1 obj\j003_003.bmp 858 570 2 5 1
check mute_backvoice obj\j014_000.bmp 92 567 1 5 0
radio voice_stop 1 obj\j013_000.bmp 423 633 2 5 0
radio voice_stop 0 obj\j013_000.bmp 537 633 2 5 1
button tabvoices obj\j015_000.bmp 777 620 1 5 0
hover 134 144 870 211 obj\j016_000.bmp 72 62 8 0
hover 134 217 870 289 obj\j016_000.bmp 72 140 8 1
hover 134 294 870 366 obj\j016_000.bmp 72 217 8 2
hover 134 372 870 444 obj\j016_000.bmp 72 295 8 3
hover 134 450 870 522 obj\j016_000.bmp 72 373 8 4
hover 134 527 870 599 obj\j016_000.bmp 72 450 8 5
hover 134 607 700 656 obj\j016_000.bmp 72 542 8 6


screen defaults
# No quick saves: its ten pages of nine cards run to 90, all ordinary.
quickslots 91 98
# Config as the PC starts it, read from its first-run screens. Text and auto speed are 50 of 100 there, which the
# runtime's four steps can't hold yet.
default opacity 80
default bgm 60
default se 60
default hse 60
default voice 80
default backvoice 80
# Standing pictures: the slot is chosen by the second number of 位置, x by the first, and 14 deletes by slot, from 21
# PC saves under Wine.
layoutslotby p1
layoutslots 200 300 400 500 600
layoutx -512 -256 0 256 512

screen story
# Traced from the PC under Wine through both choices and both endings, in the order of the exe's scene list.
# S 101 is 1 on route B, the second choice's second option, as the PC's saves hold it. Each `@` is the PC's own number
# for that place, which its saves hold as flowNode, read from 67 PC saves under Wine: 10 a scene within a chapter, the
# same number for both of a choice's options (280, 1180), a chapter's last scene carrying the next chapter's base, and
# 9999 for each ending's last scene. The staff rolls have none: the PC doesn't save during them. S 4000 is the chapter
# the load screen names, set as each chapter starts.
flow main
scene scr\ep_000_001_000.txt @5
set S 4000 = 1
scene scr\ep_000_002_000.txt @20
scene scr\ep_000_003_000.txt @30
scene scr\ep_000_004_000.txt @40
scene scr\ep_000_005_000.txt @50
scene scr\ep_000_006_000.txt @60
scene scr\ep_000_007_000.txt @70
scene scr\ep_000_008_000.txt @80
scene scr\ep_000_009_000.txt @90
scene scr\ep_000_010_000.txt @200
jump chapter2
# Start's Chapter 2 entry, which the title offers once G 2 is 1: the PC sets it as chapter 2 begins, and with it 0 Start
# plays the prologue (checked under Wine by clearing it in cfg.sav).
flow chapter2
set S 4000 = 2
set G 2 = 1
scene scr\ep_001_000_000.txt @220
scene scr\ep_001_001_000.txt @230
scene scr\ep_001_002_000.txt @240
scene scr\ep_001_003_000.txt @250
choice obj\l001_000.bmp 2 @251
on 1 goto viciousness
scene scr\ep_001_003_001.txt @280
goto chapter2
label viciousness
scene scr\ep_001_003_002.txt @280
label chapter2
scene scr\ep_001_004_000.txt @290
scene scr\ep_001_005_000.txt @300
scene scr\ep_001_006_000.txt @310
scene scr\ep_001_007_000.txt @320
scene scr\ep_001_008_000.txt @330
scene scr\ep_001_009_000.txt @340
scene scr\ep_001_010_000.txt @350
scene scr\ep_001_011_000.txt @360
scene scr\ep_001_012_000.txt @370
scene scr\ep_001_013_000.txt @380
scene scr\ep_001_014_000.txt @390
scene scr\ep_001_015_000.txt @400
scene scr\ep_001_016_000.txt @410
scene scr\ep_001_017_000.txt @420
scene scr\ep_001_018_000.txt @430
scene scr\ep_001_019_000.txt @440
scene scr\ep_001_020_000.txt @450
scene scr\ep_001_021_000.txt @460
scene scr\ep_001_022_000.txt @470
scene scr\ep_001_023_000.txt @480
scene scr\ep_001_024_000.txt @490
scene scr\ep_001_025_000.txt @500
scene scr\ep_001_026_000.txt @510
scene scr\ep_001_027_000.txt @520
scene scr\ep_001_028_000.txt @530
scene scr\ep_001_029_000.txt @540
scene scr\ep_001_030_000.txt @550
scene scr\ep_001_031_000.txt @560
scene scr\ep_001_032_000.txt @570
scene scr\ep_001_033_000.txt @580
scene scr\ep_001_034_000.txt @590
scene scr\ep_001_035_000.txt @600
scene scr\ep_001_036_000.txt @610
scene scr\ep_001_037_000.txt @620
scene scr\ep_001_038_000.txt @630
scene scr\ep_001_039_100.txt @640
scene scr\ep_001_040_000.txt @650
scene scr\ep_001_041_000.txt @660
scene scr\ep_001_042_000.txt @670
scene scr\ep_001_043_000.txt @680
scene scr\ep_001_044_100.txt @690
scene scr\ep_001_045_000.txt @700
scene scr\ep_001_046_100.txt @710
scene scr\ep_001_047_000.txt @720
scene scr\ep_001_048_000.txt @730
scene scr\ep_001_049_000.txt @740
scene scr\ep_001_050_100.txt @750
scene scr\ep_001_051_000.txt @760
scene scr\ep_001_052_100.txt @770
scene scr\ep_001_053_000.txt @780
scene scr\ep_001_054_000.txt @790
scene scr\ep_001_055_000.txt @800
scene scr\ep_001_056_000.txt @810
scene scr\ep_001_057_100.txt @820
scene scr\ep_001_058_000.txt @1000
set S 4000 = 3
scene scr\ep_002_000_000.txt @1020
scene scr\ep_002_001_000.txt @1030
scene scr\ep_002_002_000.txt @1040
scene scr\ep_002_003_100.txt @1050
scene scr\ep_002_004_000.txt @1060
scene scr\ep_002_005_100.txt @1070
scene scr\ep_002_006_000.txt @1080
scene scr\ep_002_007_100.txt @1090
scene scr\ep_002_008_100.txt @1100
scene scr\ep_002_009_000.txt @1110
scene scr\ep_002_010_000.txt @1120
scene scr\ep_002_011_000.txt @1130
scene scr\ep_002_012_100.txt @1140
scene scr\ep_002_013_000.txt @1150
choice obj\l001_001.bmp 2 @1151
on 1 goto think
scene scr\ep_002_014_000.txt @1180
goto join
label think
set S 101 = 1
scene scr\ep_002_015_000.txt @1180
label join
scene scr\ep_002_016_000.txt @1181    # on both routes: S 101 tells them apart
if S 101 == 1 goto chapter3b
scene scr\ep_002_017_000.txt @1200
scene scr\ep_002_018_100.txt @1210
scene scr\ep_002_019_000.txt @1220
scene scr\ep_002_020_100.txt @1230
scene scr\ep_002_021_000.txt @1240
scene scr\ep_002_022_000.txt @1250
scene scr\ep_002_023_000.txt @1260
scene scr\ep_002_024_100.txt @1270
scene scr\ep_002_025_000.txt @1280
scene scr\ep_002_026_000.txt @1290
scene scr\ep_002_027_000.txt @1300
scene scr\ep_002_028_000.txt @1310
scene scr\ep_002_029_100.txt @1320
scene scr\ep_002_030_100.txt @1330
scene scr\ep_002_031_000.txt @1340
scene scr\ep_002_032_000.txt @1350
scene scr\ep_002_033_000.txt @1360
scene scr\ep_002_034_000.txt @1370
scene scr\ep_002_035_000.txt @1380
scene scr\ep_002_036_100.txt @1390
scene scr\ep_002_037_000.txt @1400
scene scr\ep_002_038_000.txt @1410
scene scr\ep_002_039_000.txt @1420
scene scr\ep_002_040_000.txt @1430
scene scr\ep_002_041_000.txt @1440
scene scr\ep_002_042_000.txt @1450
scene scr\ep_002_043_100.txt @1460
scene scr\ep_002_044_000.txt @1470
scene scr\ep_002_045_100.txt @1480
scene scr\ep_002_046_000.txt @1490
scene scr\ep_002_047_000.txt @1500
scene scr\ep_002_048_100.txt @1510
scene scr\ep_002_049_000.txt @1520
scene scr\ep_002_050_000.txt @1530
scene scr\ep_002_051_000.txt @1540
scene scr\ep_002_052_000.txt @1550
scene scr\ep_002_053_000.txt @1560
scene scr\ep_002_054_000.txt @1570
scene scr\ep_002_055_000.txt @1580
scene scr\ep_002_056_000.txt @1590
scene scr\ep_002_057_000.txt @1600
scene scr\ep_002_058_100.txt @1610
scene scr\ep_002_059_000.txt @1620
scene scr\ep_002_060_000.txt @1630
scene scr\ep_002_061_000.txt @1640
scene scr\ep_002_062_000.txt @1650
scene scr\ep_002_063_000.txt @1660
scene scr\ep_002_064_000.txt @1670
scene scr\ep_002_065_000.txt @1680
scene scr\ep_002_066_000.txt @1690
scene scr\ep_002_067_100.txt @1700
scene scr\ep_002_068_100.txt @1710
scene scr\ep_002_069_000.txt @2000
set S 4000 = 5
scene scr\ep_003_000_000.txt @2020
scene scr\ep_003_001_000.txt @2030
scene scr\ep_003_002_000.txt @2040
scene scr\ep_003_003_000.txt @2050
scene scr\ep_003_004_100.txt @2060
scene scr\ep_003_005_000.txt @2070
scene scr\ep_003_006_100.txt @2080
scene scr\ep_003_007_000.txt @2090
scene scr\ep_003_008_000.txt @2100
scene scr\ep_003_009_100.txt @2110
scene scr\ep_003_010_100.txt @2120
scene scr\ep_003_011_000.txt @2130
scene scr\ep_003_012_000.txt @2140
scene scr\ep_003_013_000.txt @2150
scene scr\ep_003_014_000.txt @2160
scene scr\ep_003_015_100.txt @2170
scene scr\ep_003_016_000.txt @2180
scene scr\ep_003_017_000.txt @2190
scene scr\ep_003_018_000.txt @2200
scene scr\ep_003_019_000.txt @2210
scene scr\ep_003_020_000.txt @2220
scene scr\ep_003_021_000.txt @2230
scene scr\ep_003_022_000.txt @2240
scene scr\ep_003_023_000.txt @2250
scene scr\ep_003_024_000.txt @2260
scene scr\ep_003_025_000.txt @2270
scene scr\ep_003_026_000.txt @2280
scene scr\ep_003_027_000.txt @2290
scene scr\ep_003_028_000.txt @2300
scene scr\ep_003_029_100.txt @2310
scene scr\ep_003_030_000.txt @2320
scene scr\ep_003_031_000.txt @2330
scene scr\ep_003_032_000.txt @2340
scene scr\ep_003_033_000.txt @2350
scene scr\ep_003_034_100.txt @2360
scene scr\ep_003_035_000.txt @2370
scene scr\ep_003_036_100.txt @2380
scene scr\ep_003_037_000.txt @2390
scene scr\ep_003_038_000.txt @2400
scene scr\ep_003_039_100.txt @2410
scene scr\ep_003_040_000.txt @2420
scene scr\ep_003_041_000.txt @2430
scene scr\ep_003_042_000.txt @2440
scene scr\ep_003_043_100.txt @2450
scene scr\ep_003_044_000.txt @2460
scene scr\ep_003_045_000.txt @2470
scene scr\ep_003_046_100.txt @2480
scene scr\ep_003_047_000.txt @2490
scene scr\ep_003_048_000.txt @2500
scene scr\ep_003_049_000.txt @2510
scene scr\ep_003_050_000.txt @2520
scene scr\ep_003_051_000.txt @2530
scene scr\ep_003_052_000.txt @2540
scene scr\ep_003_053_100.txt @2550
scene scr\ep_003_054_000.txt @2560
scene scr\staff_a.txt
set S 4000 = 6
scene scr\ep_005_000_000.txt @4020
scene scr\ep_005_001_100.txt @4030
scene scr\ep_005_002_100.txt @4040
scene scr\ep_005_003_100.txt @4050
scene scr\ep_005_004_000.txt @4060
scene scr\ep_005_005_100.txt @4070
scene scr\ep_005_006_100.txt @4080
scene scr\ep_005_007_100.txt @4090
scene scr\ep_005_008_100.txt @4100
scene scr\ep_005_009_000.txt @4110
scene scr\ep_005_010_100.txt @4120
scene scr\ep_005_011_000.txt @4130
scene scr\ep_005_012_100.txt @4140
scene scr\ep_005_013_101.txt @4150
scene scr\ep_005_014_100.txt @9999
end
label chapter3b
set S 4000 = 4
scene scr\ep_004_000_000.txt @3020
scene scr\ep_004_001_100.txt @3030
scene scr\ep_004_002_000.txt @3040
scene scr\ep_004_003_100.txt @3050
scene scr\ep_004_004_000.txt @3060
scene scr\ep_004_005_000.txt @3070
scene scr\ep_004_006_000.txt @3080
scene scr\ep_004_007_000.txt @3090
scene scr\ep_004_008_000.txt @3100
scene scr\ep_004_009_100.txt @3110
scene scr\ep_004_010_000.txt @3120
scene scr\ep_004_011_000.txt @3130
scene scr\ep_004_012_000.txt @3140
scene scr\ep_004_013_000.txt @3150
scene scr\ep_004_014_000.txt @3160
scene scr\ep_004_015_000.txt @3170
scene scr\ep_004_016_100.txt @3180
scene scr\ep_004_017_000.txt @3190
scene scr\ep_004_018_100.txt @3200
scene scr\ep_004_019_100.txt @3210
scene scr\ep_004_020_000.txt @3220
scene scr\ep_004_021_100.txt @3230
scene scr\staff_b.txt
scene scr\ep_004_022_000.txt @9999
end

# The cfg.sav the PC writes on its first run under Wine, its settings before the globals.
screen cfgsav
run 4 2 1
run 16 2 50
run 24 1 80
run 36 1 100
run 40 2 1
run 48 1 2
run 52 1 1
run 76 1 1
run 80 1 100
run 84 3 60
run 96 2 80
run 104 1 60
run 108 1 1
run 116 1 60
run 120 8 1
run 168 1 1
run 244 1 32
run 400 100 1
)";
