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
