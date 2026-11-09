// Test Game 90's (Mankitsu Happening) table, a raw string that tables.cpp includes and menuTable() hands out. Measured
// on the PC game under Wine (set 1). Its story order is in its exe, so the table gives it.
const char kMankitsu[] = R"(
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
# Text and auto speed run 0 to 100 and start at 50, as its Config shows. Their pace isn't timed on 90's PC yet: Test
# Game 87's, the same Config, stands in (unconfirmed).
default text 50
textfade 50
textpoints text 0 184.9 10 153.7 25 101.8 40 52.6 50 17.6 60 4.9 75 2.6 90 1.5 100 0
default auto 50
autocurve auto 104 1.04 527 1
autowithvoice
hseprefix se_h
# The window's own buttons always show, as on the PC under Wine.
default bar_always 1

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

# Main Story starts after the prologue, just before the first select screen.
screen start
flow chapter2

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
# radios 4 left, and the sound tab's panel 20 higher than the others'. Tab 1's lower panel, the questionnaire's
# standing-picture switch, isn't built yet. Generated by set 1's gencfg90.py.
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


screen dialog
# Measured under Wine: the screen dimmed to 100 of 256 of its brightness, the question, and Yes and No.
questions resolution save load overwrite exit title
dim 155
pic obj\a003_000.bmp 325 228 6 1 0 0
button yes obj\a004_000.bmp 339 293 2 5 0
button no obj\a004_000.bmp 339 380 2 5 1

screen story
# Traced under Wine (set 1): the opening, then ten half-weeks of the select screen, each pick playing the scenes the part
# gives (`chapter S 10904`), then the ending of the heroine with four hearts and her route. G 80, 81 and 82 are set as
# Kururu's, Otona's and Rei's routes end, and once all three are, a route goes on into the shared ending, which sets G 83
# (the title's six silhouettes, checked by editing cfg.sav under Wine). Each route skips one of its scripts on the PC
# runs, ep_002_005, ep_003_005 and ep_004_004, each with another heroine in it; what plays them isn't known yet. The
# ending with no heroine at four hearts is ep_001_029 alone, then the title (seen under Wine). S 4000 names the route on
# a save's card: 1 in every PC save of the common route so far; where it becomes 2 to 5 is taken from the card's labels
# and not yet read from PC saves (unconfirmed).
flow main
scene scr\ep_000_000_000.txt
scene scr\ep_000_000_001.txt
scene scr\ep_000_000_002.txt
scene scr\ep_000_000_003.txt
set S 4000 = 1
scene scr\ep_001_000_000.txt
scene scr\ep_001_001_000.txt
scene scr\ep_001_001_001.txt
scene scr\ep_001_002_000.txt
scene scr\ep_001_003_000.txt
scene scr\ep_001_003_001.txt
scene scr\ep_001_004_000.txt
scene scr\ep_001_005_000.txt
scene scr\ep_001_005_001.txt
scene scr\ep_001_006_000.txt
jump weeks

# Main Story from the title starts with a scene of three lines that New Game's way doesn't play, its PC number 14 (set
# 1, under Wine), and then the first select screen.
flow chapter2
set S 4000 = 1
scene scr\ep_001_006_001.txt @14
jump weeks

flow weeks
# Each half-week's select screen carries the PC's number for it, which its saves there hold (set 1, PC saves at all
# ten); S 3000 and 3001 move on after the pick's scenes.
open select @15
chapter S 10904
set S 3001 = 1
open select @61
chapter S 10904
set S 3000 = 1
set S 3001 = 0
open select @85
chapter S 10904
set S 3001 = 1
open select @111
chapter S 10904
set S 3000 = 2
set S 3001 = 0
open select @143
chapter S 10904
set S 3001 = 1
open select @147
chapter S 10904
set S 3000 = 3
set S 3001 = 0
open select @172
chapter S 10904
set S 3001 = 1
open select @177
chapter S 10904
set S 3000 = 4
set S 3001 = 0
open select @197
chapter S 10904
set S 3001 = 1
open select @211
chapter S 10904
if S 10906 == 1 goto kururu
if S 10906 == 2 goto otona
if S 10906 == 3 goto rei
end
label kururu
set S 4000 = 2
scene scr\ep_002_000_000.txt
scene scr\ep_002_000_001.txt
scene scr\eye_catch1.txt
scene scr\ep_002_001_000.txt
scene scr\ep_002_001_001.txt
scene scr\ep_002_002_000.txt
scene scr\ep_002_003_000.txt
scene scr\eye_catch1.txt
scene scr\ep_002_004_000.txt
scene scr\ep_002_004_001.txt
scene scr\eye_catch1.txt
scene scr\ep_002_006_000.txt
scene scr\ep_002_006_001.txt
scene scr\eye_catch1.txt
scene scr\ep_002_007_000.txt
scene scr\ep_002_008_000.txt
scene scr\ep_002_008_001.txt
scene scr\eye_catch1.txt
scene scr\ep_002_009_000.txt
scene scr\ep_002_009_001.txt
scene scr\eye_catch1.txt
scene scr\ep_002_009_002.txt
scene scr\ep_002_010_000.txt
scene scr\ep_002_011_000.txt
scene scr\ep_002_012_000.txt
scene scr\ep_002_013_000.txt
scene scr\ep_002_013_001.txt
scene scr\ep_002_013_002.txt
scene scr\ep_002_014_000.txt
set G 80 = 1
goto shared
label otona
set S 4000 = 3
scene scr\ep_003_000_000.txt
scene scr\ep_003_000_001.txt
scene scr\eye_catch2.txt
scene scr\ep_003_001_000.txt
scene scr\ep_003_002_000.txt
scene scr\ep_003_003_000.txt
scene scr\ep_003_003_001.txt
scene scr\eye_catch2.txt
scene scr\ep_003_004_000.txt
scene scr\ep_003_004_001.txt
scene scr\ep_003_004_002.txt
scene scr\eye_catch2.txt
scene scr\ep_003_006_000.txt
scene scr\ep_003_006_001.txt
scene scr\eye_catch2.txt
scene scr\ep_003_007_000.txt
scene scr\ep_003_007_001.txt
scene scr\eye_catch2.txt
scene scr\ep_003_008_000.txt
scene scr\ep_003_009_000.txt
scene scr\ep_003_010_000.txt
scene scr\eye_catch2.txt
scene scr\ep_003_011_000.txt
scene scr\ep_003_011_001.txt
scene scr\eye_catch2.txt
scene scr\ep_003_012_000.txt
set G 81 = 1
goto shared
label rei
set S 4000 = 4
scene scr\ep_004_000_000.txt
scene scr\ep_004_001_000.txt
scene scr\ep_004_002_000.txt
scene scr\ep_004_002_001.txt
scene scr\eye_catch3.txt
scene scr\ep_004_003_000.txt
scene scr\ep_004_003_001.txt
scene scr\ep_004_005_000.txt
scene scr\ep_004_005_001.txt
scene scr\eye_catch3.txt
scene scr\ep_004_006_000.txt
scene scr\ep_004_006_001.txt
scene scr\eye_catch3.txt
scene scr\ep_004_007_000.txt
scene scr\ep_004_008_000.txt
scene scr\ep_004_009_000.txt
scene scr\ep_004_009_001.txt
scene scr\eye_catch3.txt
scene scr\ep_004_010_000.txt
scene scr\ep_004_011_000.txt
scene scr\ep_004_011_001.txt
scene scr\ep_004_012_000.txt
scene scr\ep_004_013_000.txt
scene scr\ep_004_014_000.txt
scene scr\ep_004_014_001.txt
scene scr\ep_004_015_000.txt
scene scr\ep_004_015_001.txt
set G 82 = 1
label shared
if G 80 == 0 goto done
if G 81 == 0 goto done
if G 82 == 0 goto done
set S 4000 = 5
scene scr\ep_005_000_000.txt
scene scr\ep_005_000_001.txt
scene scr\ep_005_001_000.txt
scene scr\ep_005_002_000.txt
set G 83 = 1
label done
end
)";
