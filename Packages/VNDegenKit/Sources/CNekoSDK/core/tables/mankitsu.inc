// Test Game 90's (Mankitsu Happening) table, raw strings that tables.cpp includes and menuTable() hands out. Measured on
// the PC game under Wine (set 1). Its story order is in its exe, so the table gives it. kMankitsuShared is every
// screen the game and its bonus story disc share; kMankitsuGame is the game's own title and story, and
// kMankitsuBonus the bonus story's ("School Swimsuit DLC", a release of its own with the same screens).
const char kMankitsuShared[] = R"(
screen defaults
# Faces, scene-change slides and the select screen (core/mankitsu.cpp).
parts mankitsu scroll
# A flipbook shows each picture for its command's parameter 16 and plays the sound its anime_cfg.ini line names, as 90's
# PC does (set 1: an anger mark's 41 pictures took 1.3 s at 30 ms, and a file trace reads the sound with the first picture).
fliptime
flipsound
# The text as the PC sets it: MS Gothic at a cell of 22, not init.ini's 25 (Wine's font log), each glyph 3 past its
# advance and lines 31 apart, the first glyph 1 in and 1 up. Fitted under Wine with Ume Gothic mapped to the game's
# Japanese font name: a name and a line end on the PC's pixels, and the next line starts on its row (set 1).
textsize 22
textgap 3
textline 31
textshift 1 -1
# Config as 90's PC starts it (its first-run cfg.sav under Wine): the window at 80.
default opacity 80
default bgm 60
default se 60
default hse 60
default voice 80
default backvoice 80
default text 50
textfade 50
textpoints text 1 296 21 181 30 147 40 88.3 50 29.6 60 9.4 71 5.4 81 4.1 91 3.1 100 0
default auto 50
autocurve auto 104 1.04 527 1
autowithvoice
hseprefix se_h
# The window's own buttons always show, as on the PC under Wine.
default bar_always 1
# The lower row (Save, Load, Config, Title, End) shows while the pointer is at or below y 632: not at 630, there at 635,
# at any x (PC shots under Wine).
underbelow 632
# In a Scene replay the PC draws these four disabled and they take no press; only Exit works (a PC shot under Wine).
replayoff Save Load Config Title
# The title's three bonus stories start the story with S 10907 naming one (see `screen title` and `flow main`).
routevar 10907
# A standing picture loaded by command 13 takes its slot from its second number, 200 and 100 for each, and its place
# across from its first: the bonus disc's PC save with three standing holds them in slots 200, 500 and 600 for second
# numbers 0, 3 and 4, and draws them in that order (lead and set 1, under Wine). No script has command 14. The five
# places are 256 apart about the middle: with them that scene is the PC's to the pixel.
layoutslotby p1
layoutslots 200 300 400 500 600
layoutx -512 -256 0 256 512
outfitswitch outfit 000:003:004 001:002:003 002:003:004 003:001:002 004:001:002 005:001:002

# Save and load, measured under Wine (set 1, each piece fitted on a PC shot): nine cards a page on ten pages, each a
# save's number, thumbnail, its route by S 4000 (prologue, common, Kururu's, Otona's, Rei's, harem), the date and time at
# places of their own around the card's painted slashes and colon, NEW on the one saved last, and a comment button. The
# mankitsu part draws the moving trims and the screen's name plate over the paper.
screen load
part mankitsu
pic obj\e000_000.bmp 0 0
slots obj\e003_000.bmp 30 118 368 202 3 3 1
slotdigits obj\e003_001.bmp 39 9 3
slotthumb 8 34
slotchaptervar obj\e003_003.bmp 125 7 6 4000
slotnew obj\e003_004.bmp 262 -15
slotcommentbutton obj\e003_002.bmp 251 104
datedigits obj\e003_001.bmp 200 56
datefield year 200 56
datefield month 272 56
datefield day 316 56
datefield hour 272 79
datefield minute 316 79
button page0 obj\e002_000.bmp 627 15 10 5 0
button page1 obj\e002_000.bmp 681 15 10 5 1
button page2 obj\e002_000.bmp 735 15 10 5 2
button page3 obj\e002_000.bmp 789 15 10 5 3
button page4 obj\e002_000.bmp 843 15 10 5 4
button page5 obj\e002_000.bmp 897 15 10 5 5
button page6 obj\e002_000.bmp 951 15 10 5 6
button page7 obj\e002_000.bmp 1005 15 10 5 7
button page8 obj\e002_000.bmp 1059 15 10 5 8
button page9 obj\e002_000.bmp 1113 15 10 5 9
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0

screen save
part mankitsu
pic obj\e000_000.bmp 0 0
slots obj\e003_000.bmp 30 118 368 202 3 3 1
slotdigits obj\e003_001.bmp 39 9 3
slotthumb 8 34
slotchaptervar obj\e003_003.bmp 125 7 6 4000
slotnew obj\e003_004.bmp 262 -15
slotcommentbutton obj\e003_002.bmp 251 104
datedigits obj\e003_001.bmp 200 56
datefield year 200 56
datefield month 272 56
datefield day 316 56
datefield hour 272 79
datefield minute 316 79
button page0 obj\e002_000.bmp 627 15 10 5 0
button page1 obj\e002_000.bmp 681 15 10 5 1
button page2 obj\e002_000.bmp 735 15 10 5 2
button page3 obj\e002_000.bmp 789 15 10 5 3
button page4 obj\e002_000.bmp 843 15 10 5 4
button page5 obj\e002_000.bmp 897 15 10 5 5
button page6 obj\e002_000.bmp 951 15 10 5 6
button page7 obj\e002_000.bmp 1005 15 10 5 7
button page8 obj\e002_000.bmp 1059 15 10 5 8
button page9 obj\e002_000.bmp 1113 15 10 5 9
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0

# Config, four tabs, each fitted on a shot of 90's PC under Wine (set 1): Test Game 87's layout moved 20 down with its
# radios 4 left, and the sound tab's panel 20 higher than the others'. Generated by set 1's gencfg90.py.
screen config
part mankitsu
pic obj\e000_003.bmp 0 0
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0
button tab obj\j002_000.bmp 435 34 4 5 0
button tab2 obj\j002_000.bmp 568 34 4 5 1
button tab3 obj\j002_000.bmp 701 34 4 5 2
button tab4 obj\j002_000.bmp 834 34 4 5 3
button cfg_apply obj\j000_000.bmp 992 120 3 5 0 help obj\j001_000.bmp 848 32 3 1 0 0
button cfg_undo obj\j000_000.bmp 992 178 3 5 1 help obj\j001_000.bmp 848 90 3 1 1 0
button cfg_defaults obj\j000_000.bmp 992 236 3 5 2 help obj\j001_000.bmp 848 148 3 1 2 0
pic obj\j003_000.bmp 53 128
radio screen 0 obj\j003_001.bmp 419 173 12 5 0
radio screen 1 obj\j003_001.bmp 533 173 12 5 1
pic obj\j003_001.bmp 419 209 12 5 2 0
pic obj\j003_001.bmp 533 209 12 5 3 2
pic obj\j003_001.bmp 419 245 12 5 4 2
pic obj\j003_001.bmp 533 245 12 5 5 0
pic obj\j003_001.bmp 647 245 12 5 6 0
pic obj\j003_001.bmp 419 281 12 5 7 4
pic obj\j003_001.bmp 533 281 12 5 8 0
pic obj\j003_001.bmp 647 281 12 5 9 0
radio effect 1 obj\j013_000.bmp 419 317 2 5 0
radio effect 0 obj\j013_000.bmp 533 317 2 5 1
radio wait 1 obj\j013_000.bmp 419 353 2 5 0
radio wait 0 obj\j013_000.bmp 533 353 2 5 1
knob opacity obj\j003_002.bmp 184 429 6
number opacity obj\j003_004.bmp 762 386
step opacity -1 obj\j003_003.bmp 147 427 2 5 0
step opacity 1 obj\j003_003.bmp 819 427 2 5 1
radio confirm_saveload 1 obj\j013_000.bmp 419 476 2 5 0
radio confirm_saveload 0 obj\j013_000.bmp 533 476 2 5 1
group confirm_saveload confirm_overwrite confirm_delete confirm_quick confirm_title confirm_exit
radio tutorial 1 obj\j013_000.bmp 419 512 2 5 0
radio tutorial 0 obj\j013_000.bmp 533 512 2 5 1
radio waitskip 1 obj\j013_000.bmp 419 548 2 5 0
radio waitskip 0 obj\j013_000.bmp 533 548 2 5 1
pic obj\a001_002.bmp 286 590 1 5 0 0
hovercols 134 160 870 200 obj\j004_000.bmp 34 82 9 0
hovercols 134 205 870 236 obj\j004_000.bmp 34 118 9 1
hovercols 134 241 870 307 obj\j004_000.bmp 34 154 9 2
hovercols 134 312 870 344 obj\j004_000.bmp 34 226 9 3
hovercols 134 348 870 380 obj\j004_000.bmp 34 262 9 4
hovercols 134 384 870 466 obj\j004_000.bmp 34 298 9 5
hovercols 134 471 870 503 obj\j004_000.bmp 34 385 9 6
hovercols 134 507 870 539 obj\j004_000.bmp 34 421 9 7
hovercols 134 541 870 573 obj\j004_000.bmp 34 454 9 8
# The Questionnaire Privilege panel under the first tab, from sys_3.pak: the heroines' standing pictures as normal, in
# underwear or nude, with a sample of each under the pointer. The PC shows no sample over the
# chosen button; here each button shows its own, and not the row's help card with it. The mankitsu part draws the
# panel, sys_3\j200_000.bmp at 53, 653, since it lies over the bottom trim.
radio outfit 0 sys_3\j200_001.bmp 419 672 3 5 0
radio outfit 1 sys_3\j200_001.bmp 533 672 3 5 1
radio outfit 2 sys_3\j200_001.bmp 647 672 3 5 2
hovercols 419 672 523 700 sys_3\j200_003.bmp 586 360 3 0
hovercols 533 672 637 700 sys_3\j200_003.bmp 586 360 3 1
hovercols 647 672 751 700 sys_3\j200_003.bmp 586 360 3 2
hovercols 94 660 908 708 sys_3\j200_002.bmp 2 570 1 0

screen config2
part mankitsu
pic obj\e000_003.bmp 0 0
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0
button tab obj\j002_000.bmp 435 34 4 5 0
button tab2 obj\j002_000.bmp 568 34 4 5 1
button tab3 obj\j002_000.bmp 701 34 4 5 2
button tab4 obj\j002_000.bmp 834 34 4 5 3
button cfg_apply obj\j000_000.bmp 992 120 3 5 0 help obj\j001_000.bmp 848 32 3 1 0 0
button cfg_undo obj\j000_000.bmp 992 178 3 5 1 help obj\j001_000.bmp 848 90 3 1 1 0
button cfg_defaults obj\j000_000.bmp 992 236 3 5 2 help obj\j001_000.bmp 848 148 3 1 2 0
pic obj\j007_000.bmp 53 128
knob text obj\j003_002.bmp 184 213 6
number text obj\j003_004.bmp 762 170
step text -1 obj\j003_003.bmp 147 211 2 5 0
step text 1 obj\j003_003.bmp 819 211 2 5 1
knob auto obj\j003_002.bmp 184 308 6
number auto obj\j003_004.bmp 762 265
step auto -1 obj\j003_003.bmp 147 306 2 5 0
step auto 1 obj\j003_003.bmp 819 306 2 5 1
pic obj\j007_001.bmp 419 450 3 5 0 2
pic obj\j007_001.bmp 533 450 3 5 1 0
pic obj\j007_001.bmp 647 450 3 5 2 0
range waittime 20 180
knob waittime obj\j003_002.bmp 184 403 3
number waittime obj\j003_004.bmp 756 361 780 797
step waittime -1 obj\j003_003.bmp 147 401 2 5 0
step waittime 1 obj\j003_003.bmp 819 401 2 5 1

screen config3
part mankitsu
pic obj\e000_003.bmp 0 0
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0
button tab obj\j002_000.bmp 435 34 4 5 0
button tab2 obj\j002_000.bmp 568 34 4 5 1
button tab3 obj\j002_000.bmp 701 34 4 5 2
button tab4 obj\j002_000.bmp 834 34 4 5 3
button cfg_apply obj\j000_000.bmp 992 120 3 5 0 help obj\j001_000.bmp 848 32 3 1 0 0
button cfg_undo obj\j000_000.bmp 992 178 3 5 1 help obj\j001_000.bmp 848 90 3 1 1 0
button cfg_defaults obj\j000_000.bmp 992 236 3 5 2 help obj\j001_000.bmp 848 148 3 1 2 0
pic obj\j009_000.bmp 53 128
radio skipall 0 obj\j009_001.bmp 419 173 11 5 0
radio skipall 1 obj\j009_001.bmp 533 173 11 5 1
radio rightclick 0 obj\j009_001.bmp 419 209 11 5 2
radio rightclick 1 obj\j009_001.bmp 533 209 11 5 3
radio rightclick 2 obj\j009_001.bmp 647 209 11 5 4
radio rightclick 3 obj\j009_001.bmp 419 245 11 5 5
radio rightclick 4 obj\j009_001.bmp 533 245 11 5 6
radio deckey 0 obj\j009_001.bmp 419 281 11 5 7
radio deckey 1 obj\j009_001.bmp 533 281 11 5 8
radio pause_inactive 1 obj\j009_001.bmp 419 317 11 5 9
radio pause_inactive 0 obj\j009_001.bmp 533 317 11 5 10

screen config4
part mankitsu
pic obj\e000_003.bmp 0 0
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0
button tab obj\j002_000.bmp 435 34 4 5 0
button tab2 obj\j002_000.bmp 568 34 4 5 1
button tab3 obj\j002_000.bmp 701 34 4 5 2
button tab4 obj\j002_000.bmp 834 34 4 5 3
button cfg_apply obj\j000_000.bmp 992 120 3 5 0 help obj\j001_000.bmp 848 32 3 1 0 0
button cfg_undo obj\j000_000.bmp 992 178 3 5 1 help obj\j001_000.bmp 848 90 3 1 1 0
button cfg_defaults obj\j000_000.bmp 992 236 3 5 2 help obj\j001_000.bmp 848 148 3 1 2 0
pic obj\j011_000.bmp 53 108
knob master obj\j003_002.bmp 222 182 6
number master obj\j003_004.bmp 800 143 18
step master -1 obj\j003_003.bmp 185 181 2 5 0
step master 1 obj\j003_003.bmp 857 181 2 5 1
check mute_master obj\j014_000.bmp 88 173 1 5 0
pic obj\j012_000.bmp 899 143 1 5 0 4
knob bgm obj\j003_002.bmp 222 260 6
number bgm obj\j003_004.bmp 800 221 18
step bgm -1 obj\j003_003.bmp 185 259 2 5 0
step bgm 1 obj\j003_003.bmp 857 259 2 5 1
check mute_bgm obj\j014_000.bmp 88 250 1 5 0
button test:bgm:bgm\bgm01.ogg obj\j012_000.bmp 899 221 1 5 0
knob se obj\j003_002.bmp 222 338 6
number se obj\j003_004.bmp 800 299 18
step se -1 obj\j003_003.bmp 185 337 2 5 0
step se 1 obj\j003_003.bmp 857 337 2 5 1
check mute_se obj\j014_000.bmp 88 328 1 5 0
button test:se:se\se_277.ogg obj\j012_000.bmp 899 299 1 5 0
knob hse obj\j003_002.bmp 222 416 6
number hse obj\j003_004.bmp 800 377 18
step hse -1 obj\j003_003.bmp 185 415 2 5 0
step hse 1 obj\j003_003.bmp 857 415 2 5 1
check mute_hse obj\j014_000.bmp 88 406 1 5 0
button test:se:se\se_h20a.ogg obj\j012_000.bmp 899 377 1 5 0
knob voice obj\j003_002.bmp 222 494 6
number voice obj\j003_004.bmp 800 455 18
step voice -1 obj\j003_003.bmp 185 493 2 5 0
step voice 1 obj\j003_003.bmp 857 493 2 5 1
check mute_voice obj\j014_000.bmp 88 483 1 5 0
button test:voice:voice\kur0_0001.ogg obj\j012_000.bmp 899 455 1 5 0
knob backvoice obj\j003_002.bmp 222 572 6
number backvoice obj\j003_004.bmp 800 533 18
step backvoice -1 obj\j003_003.bmp 185 571 2 5 0
step backvoice 1 obj\j003_003.bmp 857 571 2 5 1
check mute_backvoice obj\j014_000.bmp 88 561 1 5 0
button test:voice:voice\kur9_0032.ogg obj\j012_000.bmp 899 533 1 5 0
radio voice_stop 1 obj\j013_000.bmp 419 633 2 5 0
radio voice_stop 0 obj\j013_000.bmp 533 633 2 5 1
button tabvoices obj\j015_000.bmp 736 623 1 5 0

screen configvoices
cancel tab4
pic obj\e000_003.bmp 0 0
pic obj\e000_001.bmp 4 6 6 1 5 0
pic obj\j002_000.bmp 435 34 4 5 0 0
pic obj\j002_000.bmp 568 34 4 5 1 0
pic obj\j002_000.bmp 701 34 4 5 2 0
pic obj\j002_000.bmp 834 34 4 5 3 2
pic obj\j000_000.bmp 992 120 3 5 0 0
pic obj\j000_000.bmp 992 178 3 5 1 0
pic obj\j000_000.bmp 992 236 3 5 2 0
pic obj\j011_000.bmp 53 108
pic obj\e001_000.bmp 1172 104 9 5 0 0
pic obj\e001_000.bmp 1172 162 9 5 1 0
pic obj\e001_000.bmp 1172 220 9 5 2 0
pic obj\e001_000.bmp 1172 278 9 5 3 0
pic obj\e001_000.bmp 1172 336 9 5 4 0
pic obj\e001_000.bmp 1172 394 9 5 5 2
pic obj\e001_000.bmp 1172 452 9 5 6 0
pic obj\e001_000.bmp 1172 510 9 5 7 0
pic obj\e001_000.bmp 1172 652 9 5 8 0
pic obj\j017_002.bmp 0 0
pic obj\j017_000.bmp 96 13
check voice0 obj\j017_001.bmp 318 222 8 5 0 2 3 0 1
check voice1 obj\j017_001.bmp 555 222 8 5 1 2 3 0 1
check voice2 obj\j017_001.bmp 792 222 8 5 2 2 3 0 1
check voice3 obj\j017_001.bmp 318 395 8 5 3 2 3 0 1
check voice4 obj\j017_001.bmp 555 395 8 5 4 2 3 0 1
check voice5 obj\j017_001.bmp 792 395 8 5 5 2 3 0 1
check voice6 obj\j017_001.bmp 318 568 8 5 6 2 3 0 1
check voice7 obj\j017_001.bmp 555 568 8 5 7 2 3 0 1
button tab4 obj\k000_001.bmp 1135 22 1 5 0

# The backlog, measured under Wine (set 1, each piece fitted on a PC shot): a rule each 166 pixels with the name and text
# in yellow, a voiced line's button at the right, the scroll track and its thumb, the close button and four arrows, over
# the scene and the whole message window, its buttons, gauge and face, dimmed.
screen log
dim 180
pic obj\k000_002.bmp 1185 110
scroll obj\k000_003t.bmp obj\k000_003m.bmp obj\k000_003b.bmp 1185 110 500
rows obj\k000_000.bmp 127 52 166 4 140 34
button log_close obj\k000_001.bmp 1230 19 1 5 0
button log_top obj\k001_000.bmp 1185 31 4 5 0
button log_up obj\k001_000.bmp 1185 61 4 5 1
button log_down obj\k001_000.bmp 1185 633 4 5 2
button log_bottom obj\k001_000.bmp 1185 663 4 5 3
keepwindow buttons
edgesoff
logvoice obj\h000_001.bmp 6 5 3 1125 35
lognamey -30

screen dialog
# Measured under Wine: the screen dimmed to 100 of 256 of its brightness, the question, and Yes and No.
questions resolution save load overwrite exit title
dim 155
pic obj\a003_000.bmp 325 228 6 1 0 0
button yes obj\a004_000.bmp 339 293 2 5 0
button no obj\a004_000.bmp 339 380 2 5 1
)";

const char kMankitsuGame[] = R"(
# The Appendix, measured under Wine (set 1, each piece fitted on a PC shot). CG and Scene: five tabs (the three heroines,
# the sub-heroines, the others), each with its standing pictures at the left over the screen's name plate, the fifth
# a large picture at half size; nine cards a page, a card past the tab's last entry not drawn, a locked one an empty
# dashed card with its number; CG cards count the pictures seen.
screen cg
part mankitsu
pic obj\f000_000.bmp 0 0
pic obj\f001_000.bmp 0 400
pic obj\f000_001.bmp 468 74
pic obj\e000_001.bmp 4 6 6 1 2 0
pic sc\sc_000_000_000_000_000.bmp -66 2 tab 0
pic sc\sc_001_000_000_000_000.bmp -130 2 tab 1
pic sc\sc_002_000_000_000_000.bmp -66 2 tab 2
pic sc\sc_005_000_000_000_000.bmp -236 2 tab 3
pic sc\sc_004_000_000_000_000.bmp 114 2 tab 3
pic sc\sc_003_000_000_000_000.bmp -66 2 tab 3
pic sc\sc_007_000_000_002_000.bmp -130 2 tab 4 scale 50
cellsheet obj\f002_000.bmp
cellsize 180 174
cellthumb 6 33
cell 514 109
cell 728 109
cell 942 109
cell 514 318
cell 728 318
cell 942 318
cell 514 527
cell 728 527
cell 942 527
celllabel obj\f002_001.bmp 4 4
celldigits obj\f003_000.bmp 38 4 2 14
cellcount obj\f003_000.bmp obj\f003_001.bmp 91 127 127 139 0
cellcountstep 14
celltotal obj\f003_002.bmp 7 12
gallerytabs
cellhide
button page0 obj\e002_000.bmp 627 15 10 5 0
button page1 obj\e002_000.bmp 681 15 10 5 1
button page2 obj\e002_000.bmp 735 15 10 5 2
button gtab0 obj\f001_001.bmp 24 493 7 5 0
button gtab1 obj\f001_001.bmp 174 493 7 5 1
button gtab2 obj\f001_001.bmp 324 493 7 5 2
button gtab3 obj\f001_001.bmp 99 598 7 5 3
button gtab4 obj\f001_001.bmp 249 598 7 5 6
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0

# As the CG screen, without the counts.
screen scene
part mankitsu
pic obj\f000_000.bmp 0 0
pic obj\f001_000.bmp 0 400
pic obj\f000_001.bmp 468 74
pic obj\e000_001.bmp 4 6 6 1 3 0
pic sc\sc_000_000_000_000_000.bmp -66 2 tab 0
pic sc\sc_001_000_000_000_000.bmp -130 2 tab 1
pic sc\sc_002_000_000_000_000.bmp -66 2 tab 2
pic sc\sc_005_000_000_000_000.bmp -236 2 tab 3
pic sc\sc_004_000_000_000_000.bmp 114 2 tab 3
pic sc\sc_003_000_000_000_000.bmp -66 2 tab 3
pic sc\sc_007_000_000_002_000.bmp -130 2 tab 4 scale 50
cellsheet obj\f002_000.bmp
cellsize 180 174
cellthumb 6 33
cell 514 109
cell 728 109
cell 942 109
cell 514 318
cell 728 318
cell 942 318
cell 514 527
cell 728 527
cell 942 527
celllabel obj\f002_001.bmp 4 4
celldigits obj\f003_000.bmp 38 4 2 14
gallerytabs
cellhide
button page0 obj\e002_000.bmp 627 15 10 5 0
button page1 obj\e002_000.bmp 681 15 10 5 1
button page2 obj\e002_000.bmp 735 15 10 5 2
button gtab0 obj\f001_001.bmp 24 493 7 5 0
button gtab1 obj\f001_001.bmp 174 493 7 5 1
button gtab2 obj\f001_001.bmp 324 493 7 5 2
button gtab3 obj\f001_001.bmp 99 598 7 5 3
button gtab4 obj\f001_001.bmp 249 598 7 5 6
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0

# Music: thirteen tracks in two columns, seven seals with their help cards 104 above (none for pause and stop), the
# volume knob, whose left edge runs from x 83 to 219, and the playing track's name in the strip. The mankitsu part
# draws the trims and the name plate.
screen music
part mankitsu
pic obj\e000_000.bmp 0 0
pic obj\i000_000.bmp 39 113
knob music obj\i001_001.bmp 83 608 1.36
musictitle obj\i003_000.bmp 365 614 13
button track0 obj\i002_000.bmp 215 160 13 5 0
button track1 obj\i002_000.bmp 215 211 13 5 1
button track2 obj\i002_000.bmp 215 262 13 5 2
button track3 obj\i002_000.bmp 215 313 13 5 3
button track4 obj\i002_000.bmp 215 364 13 5 4
button track5 obj\i002_000.bmp 215 415 13 5 5
button track6 obj\i002_000.bmp 215 466 13 5 6
button track7 obj\i002_000.bmp 605 160 13 5 7
button track8 obj\i002_000.bmp 605 211 13 5 8
button track9 obj\i002_000.bmp 605 262 13 5 9
button track10 obj\i002_000.bmp 605 313 13 5 10
button track11 obj\i002_000.bmp 605 364 13 5 11
button track12 obj\i002_000.bmp 605 415 13 5 12
button music_prev obj\i001_000.bmp 700 598 7 5 0 help obj\i004_000.bmp 554 494 8 1 0 0
button music_pause obj\i001_000.bmp 756 598 7 5 1
button music_play obj\i001_000.bmp 812 598 7 5 2 help obj\i004_000.bmp 666 494 8 1 2 0
button music_stop obj\i001_000.bmp 868 598 7 5 3
button music_next obj\i001_000.bmp 924 598 7 5 4 help obj\i004_000.bmp 778 494 8 1 4 0
button music_repeat obj\i001_000.bmp 980 598 7 5 5 help obj\i004_000.bmp 834 494 8 1 5 0
button music_shuffle obj\i001_000.bmp 1036 598 7 5 6 help obj\i004_000.bmp 890 494 8 1 6 0
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0

screen title
# Measured under Wine. The mankitsu part draws the backgrounds, silhouettes, logo and trims; the buttons are 40 pixels
# apart, each with its help card 104 above it at x 600, and a submenu takes the buttons' place. New Game offers the
# prologue or the main story once the prologue has been seen (G 2, found by clearing globals in cfg.sav under Wine);
# before that it starts at once. The Web Questionnaire button opens nothing: the game names no page for it.
part mankitsu
submenudim hide
startmenu 2
button start obj\d001_000.bmp 746 382 6 5 0 help obj\d002_000.bmp 600 278 12 1 0 0
button load obj\d001_000.bmp 746 422 6 5 2 help obj\d002_000.bmp 600 318 12 1 2 0
button submenu:extras obj\d001_000.bmp 746 462 6 5 3 help obj\d002_000.bmp 600 358 12 1 3 0
button config obj\d001_000.bmp 746 502 6 5 4 help obj\d002_000.bmp 600 398 12 1 4 0
button quit obj\d001_000.bmp 746 542 6 5 5 help obj\d002_000.bmp 600 438 12 1 5 0
button websurvey obj\d001_001.bmp 866 620 1 5 0 help obj\d002_000.bmp 720 516 12 1 6 0
button start_prologue obj\d001_002.bmp 746 382 5 5 0 group start help obj\d002_000.bmp 600 278 12 1 7 0
button start_main obj\d001_002.bmp 746 422 5 5 1 group start help obj\d002_000.bmp 600 318 12 1 8 0
button cg obj\d001_002.bmp 746 382 5 5 2 group extras help obj\d002_000.bmp 600 278 12 1 9 0
button scene obj\d001_002.bmp 746 422 5 5 3 group extras help obj\d002_000.bmp 600 318 12 1 10 0
button music obj\d001_002.bmp 746 462 5 5 4 group extras help obj\d002_000.bmp 600 358 12 1 11 0
# The additional edition (the release's second exe, with sys_2.pak): three book covers, each playing its bonus story and
# coming back here, with a help card over it. Fitted on PC shots of that exe under Wine, exact. The mankitsu part draws
# the Information box and its ticker. Without sys_2.pak none of it is there. That exe's small label beside the
# copyright isn't drawn: no picture of the game's holds it.
button route1 sys_2\obj\d400_001.bmp 759 147 3 5 0 help sys_2\obj\d400_002.bmp 579 61 3 1 0 0
button route2 sys_2\obj\d400_001.bmp 919 147 3 5 1 help sys_2\obj\d400_002.bmp 739 61 3 1 1 0
button route3 sys_2\obj\d400_001.bmp 1079 147 3 5 2 help sys_2\obj\d400_002.bmp 899 61 3 1 2 0

# Main Story starts after the prologue, just before the first select screen.
screen start
flow chapter2
after 2

screen story
# Traced under Wine (set 1): the opening, then ten half-weeks of the select screen, each pick playing the scenes the part
# gives (`chapter S 10904`), then the ending of the heroine with four hearts and her route. G 80, 81 and 82 are set as
# Kururu's, Otona's and Rei's routes end, and once all three are, a route goes on into the shared ending, which sets G 83
# (the title's six silhouettes, checked by editing cfg.sav under Wine). Each route has one script with another heroine in it,
# ep_002_005 (Rei), ep_003_005 (Kururu) and ep_004_004 (Otona), which plays only when she has two hearts or more, as PC
# runs showed (the first two with her at two, one and none; Rei's route's with Otona at two and at none). The
# ending with no heroine at four hearts is ep_001_029 alone, then the title (seen under Wine). S 4000 names the route on
# a save's card.
flow main
# A bonus story from the title's covers: its one script, then the title (a file trace of the second exe under Wine).
if S 10907 == 1 goto bonus1
if S 10907 == 2 goto bonus2
if S 10907 == 3 goto bonus3
scene scr\ep_000_000_000.txt @1
scene scr\ep_000_000_001.txt @2
scene scr\ep_000_000_002.txt @3
scene scr\ep_000_000_003.txt @4
scene scr\ep_001_000_000.txt @5
scene scr\ep_001_001_000.txt @6
scene scr\ep_001_001_001.txt @7
scene scr\ep_001_002_000.txt @8
scene scr\ep_001_003_000.txt @9
scene scr\ep_001_003_001.txt @10
scene scr\ep_001_004_000.txt @11
scene scr\ep_001_005_000.txt @12
scene scr\ep_001_005_001.txt @13
set S 4000 = 1
scene scr\ep_001_006_000.txt @14
set G 2 = 1
jump weeks
label bonus1
scene sys_2\scr\ep_011_000_000.txt
end
label bonus2
scene sys_2\scr\ep_011_000_001.txt
end
label bonus3
scene sys_2\scr\ep_011_000_002.txt
end

# Main Story from the title starts with a scene of three lines that New Game's way doesn't play, its PC number 14 (set
# 1, under Wine), and then the first select screen.
flow chapter2
set S 4000 = 1
scene scr\ep_001_006_001.txt @14
jump weeks

flow weeks
label p0
open select 0 @15
goto play
label p1
open select 1 @61
goto play
label p2
open select 2 @85
goto play
label p3
open select 3 @111
goto play
label p4
open select 4 @143
goto play
label p5
open select 5 @147
goto play
label p6
open select 6 @172
goto play
label p7
open select 7 @177
goto play
label p8
open select 8 @197
goto play
label p9
open select 9 @211
goto play
label play
chapter S 10904
if S 10906 == 1 goto kururu
if S 10906 == 2 goto otona
if S 10906 == 3 goto rei
if S 3001 == 1 goto half2
if S 3000 == 0 goto p1
if S 3000 == 1 goto p3
if S 3000 == 2 goto p5
if S 3000 == 3 goto p7
if S 3000 == 4 goto p9
end
label half2
if S 3000 == 0 goto p2
if S 3000 == 1 goto p4
if S 3000 == 2 goto p6
if S 3000 == 3 goto p8
end
label kururu
set S 4000 = 2
scene scr\ep_002_000_000.txt @301
scene scr\ep_002_000_001.txt @302
scene scr\eye_catch1.txt @303
scene scr\ep_002_001_000.txt @304
scene scr\ep_002_001_001.txt @305
scene scr\ep_002_002_000.txt @306
scene scr\ep_002_003_000.txt @307
scene scr\eye_catch1.txt @308
scene scr\ep_002_004_000.txt @309
scene scr\ep_002_004_001.txt @310
scene scr\eye_catch1.txt @311
# With Rei at two hearts or more: seen with her at two, and not at one or none.
if S 3012 < 2 goto kururu5
scene scr\ep_002_005_000.txt @351
scene scr\ep_002_005_001.txt @352
scene scr\eye_catch1.txt @353
label kururu5
scene scr\ep_002_006_000.txt @316
scene scr\ep_002_006_001.txt @317
scene scr\eye_catch1.txt @318
scene scr\ep_002_007_000.txt @319
scene scr\ep_002_008_000.txt @320
scene scr\ep_002_008_001.txt @325
scene scr\eye_catch1.txt @326
scene scr\ep_002_009_000.txt @327
scene scr\ep_002_009_001.txt @328
scene scr\eye_catch1.txt @329
scene scr\ep_002_009_002.txt @330
scene scr\ep_002_010_000.txt @331
scene scr\ep_002_011_000.txt @332
scene scr\ep_002_012_000.txt @333
scene scr\ep_002_013_000.txt @334
scene scr\ep_002_013_001.txt @335
scene scr\ep_002_013_002.txt @336
scene scr\ep_002_014_000.txt @600
set G 80 = 1
goto shared
label otona
set S 4000 = 3
scene scr\ep_003_000_000.txt @401
scene scr\ep_003_000_001.txt @402
scene scr\eye_catch2.txt @403
scene scr\ep_003_001_000.txt @404
scene scr\ep_003_002_000.txt @405
scene scr\ep_003_003_000.txt @406
scene scr\ep_003_003_001.txt @407
scene scr\eye_catch2.txt @408
scene scr\ep_003_004_000.txt @409
scene scr\ep_003_004_001.txt @410
scene scr\ep_003_004_002.txt @411
scene scr\eye_catch2.txt @412
# With Kururu at two hearts or more: seen with her at two, and not at none.
if S 3010 < 2 goto otona5
scene scr\ep_003_005_000.txt @416
scene scr\ep_003_005_001.txt @417
scene scr\eye_catch2.txt @418
label otona5
scene scr\ep_003_006_000.txt @421
scene scr\ep_003_006_001.txt @422
scene scr\eye_catch2.txt @423
scene scr\ep_003_007_000.txt @424
scene scr\ep_003_007_001.txt @425
scene scr\eye_catch2.txt @426
scene scr\ep_003_008_000.txt @427
scene scr\ep_003_009_000.txt @428
scene scr\ep_003_010_000.txt @429
scene scr\eye_catch2.txt @430
scene scr\ep_003_011_000.txt @431
scene scr\ep_003_011_001.txt @432
scene scr\eye_catch2.txt @433
scene scr\ep_003_012_000.txt @600
set G 81 = 1
goto shared
label rei
set S 4000 = 4
scene scr\ep_004_000_000.txt @501
scene scr\ep_004_001_000.txt @502
scene scr\ep_004_002_000.txt @503
scene scr\ep_004_002_001.txt @504
scene scr\eye_catch3.txt @505
scene scr\ep_004_003_000.txt @506
scene scr\ep_004_003_001.txt @507
# With Otona at two hearts or more: seen with her at two, and not with her at none.
if S 3011 < 2 goto rei4
scene scr\ep_004_004_000.txt @511
scene scr\ep_004_004_001.txt @512
scene scr\eye_catch3.txt @513
label rei4
scene scr\ep_004_005_000.txt @516
scene scr\ep_004_005_001.txt @517
scene scr\eye_catch3.txt @518
scene scr\ep_004_006_000.txt @519
scene scr\ep_004_006_001.txt @520
scene scr\eye_catch3.txt @521
scene scr\ep_004_007_000.txt @522
scene scr\ep_004_008_000.txt @523
scene scr\ep_004_009_000.txt @524
scene scr\ep_004_009_001.txt @525
scene scr\eye_catch3.txt @526
scene scr\ep_004_010_000.txt @527
scene scr\ep_004_011_000.txt @528
scene scr\ep_004_011_001.txt @529
scene scr\ep_004_012_000.txt @530
scene scr\ep_004_013_000.txt @531
scene scr\ep_004_014_000.txt @532
scene scr\ep_004_014_001.txt @533
scene scr\ep_004_015_000.txt @534
scene scr\ep_004_015_001.txt @600
set G 82 = 1
label shared
if G 80 == 0 goto done
if G 81 == 0 goto done
if G 82 == 0 goto done
set S 4000 = 5
scene scr\ep_005_000_000.txt @601
scene scr\ep_005_000_001.txt @602
scene scr\ep_005_001_000.txt @603
scene scr\ep_005_002_000.txt @9999
set G 83 = 1
label done
end
)";

const char kMankitsuBonus[] = R"(
# The bonus story disc (Mankitsu Happening Tokuten Story), measured on its own exe under Wine (set 1): the game's title
# with the disc's logo and three silhouettes, no Web Questionnaire button, and New Game playing its four scripts in
# order, then the title (a file trace). Its PC numbers are 1, 2, 3 and 9999, from PC saves made in each script.
# Its PC greys the Music buttons everywhere, on the title's Appendix submenu and every menu's side.
screen defaults
buttonsoff nav_music music
emptytabsoff

screen title
part mankitsu
submenudim hide
button start obj\d001_000.bmp 746 382 6 5 0 help obj\d002_000.bmp 600 278 12 1 0 0
button load obj\d001_000.bmp 746 422 6 5 2 help obj\d002_000.bmp 600 318 12 1 2 0
button submenu:extras obj\d001_000.bmp 746 462 6 5 3 help obj\d002_000.bmp 600 358 12 1 3 0
button config obj\d001_000.bmp 746 502 6 5 4 help obj\d002_000.bmp 600 398 12 1 4 0
button quit obj\d001_000.bmp 746 542 6 5 5 help obj\d002_000.bmp 600 438 12 1 5 0
button cg obj\d001_002.bmp 746 382 5 5 2 group extras help obj\d002_000.bmp 600 278 12 1 9 0
button scene obj\d001_002.bmp 746 422 5 5 3 group extras help obj\d002_000.bmp 600 318 12 1 10 0
button music obj\d001_002.bmp 746 462 5 5 4 group extras help obj\d002_000.bmp 600 358 12 1 11 0

# Its CG and Scene screens: the game's without the standing pictures, opening on the fifth tab, the only one with
# entries; the other four are greyed, as on its PC.
screen cg
part mankitsu
pic obj\f000_000.bmp 0 0
pic obj\f001_000.bmp 0 400
pic obj\f000_001.bmp 468 74
pic obj\e000_001.bmp 4 6 6 1 2 0
cellsheet obj\f002_000.bmp
cellsize 180 174
cellthumb 6 33
cell 514 109
cell 728 109
cell 942 109
cell 514 318
cell 728 318
cell 942 318
cell 514 527
cell 728 527
cell 942 527
celllabel obj\f002_001.bmp 4 4
celldigits obj\f003_000.bmp 38 4 2 14
cellcount obj\f003_000.bmp obj\f003_001.bmp 91 127 127 139 0
cellcountstep 14
celltotal obj\f003_002.bmp 7 12
gallerytabs 4
cellhide
button page0 obj\e002_000.bmp 627 15 10 5 0
button page1 obj\e002_000.bmp 681 15 10 5 1
button page2 obj\e002_000.bmp 735 15 10 5 2
button gtab0 obj\f001_001.bmp 24 493 7 5 0
button gtab1 obj\f001_001.bmp 174 493 7 5 1
button gtab2 obj\f001_001.bmp 324 493 7 5 2
button gtab3 obj\f001_001.bmp 99 598 7 5 3
button gtab4 obj\f001_001.bmp 249 598 7 5 6
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0

screen scene
part mankitsu
pic obj\f000_000.bmp 0 0
pic obj\f001_000.bmp 0 400
pic obj\f000_001.bmp 468 74
pic obj\e000_001.bmp 4 6 6 1 3 0
cellsheet obj\f002_000.bmp
cellsize 180 174
cellthumb 6 33
cell 514 109
cell 728 109
cell 942 109
cell 514 318
cell 728 318
cell 942 318
cell 514 527
cell 728 527
cell 942 527
celllabel obj\f002_001.bmp 4 4
celldigits obj\f003_000.bmp 38 4 2 14
gallerytabs 4
cellhide
button page0 obj\e002_000.bmp 627 15 10 5 0
button page1 obj\e002_000.bmp 681 15 10 5 1
button page2 obj\e002_000.bmp 735 15 10 5 2
button gtab0 obj\f001_001.bmp 24 493 7 5 0
button gtab1 obj\f001_001.bmp 174 493 7 5 1
button gtab2 obj\f001_001.bmp 324 493 7 5 2
button gtab3 obj\f001_001.bmp 99 598 7 5 3
button gtab4 obj\f001_001.bmp 249 598 7 5 6
button nav_save obj\e001_000.bmp 1172 104 9 5 0 help obj\e001_002.bmp 1026 0 9 1 0 0
button nav_load obj\e001_000.bmp 1172 162 9 5 1 help obj\e001_002.bmp 1026 58 9 1 1 0
button nav_cg obj\e001_000.bmp 1172 220 9 5 2 help obj\e001_002.bmp 1026 116 9 1 2 0
button nav_scene obj\e001_000.bmp 1172 278 9 5 3 help obj\e001_002.bmp 1026 174 9 1 3 0
button nav_music obj\e001_000.bmp 1172 336 9 5 4 help obj\e001_002.bmp 1026 232 9 1 4 0
button nav_config obj\e001_000.bmp 1172 394 9 5 5 help obj\e001_002.bmp 1026 290 9 1 5 0
button nav_title obj\e001_000.bmp 1172 452 9 5 6 help obj\e001_002.bmp 1026 348 9 1 6 0
button nav_quit obj\e001_000.bmp 1172 510 9 5 7 help obj\e001_002.bmp 1026 406 9 1 7 0
button back obj\e001_000.bmp 1172 652 9 5 8 help obj\e001_002.bmp 1026 548 9 1 8 0

screen story
flow main
scene scr\ep_010_000_000.txt @1
scene scr\ep_010_000_001.txt @2
scene scr\ep_010_000_002.txt @3
scene scr\ep_010_000_003.txt @9999
end
)";
