// Test Game 96, Yabai! -Fukushuu Yami Site- (MangaGamer's English release). Its story order is in the exe, traced under
// Wine: five truths, each an interrogation hub (the yabaiplay part) and a vote (the lead's yabaivote), then one of three
// endings. Comments over scenes are the yabai part's, the question tree yabaitree's.
const char kYabai1[] = R"(
screen defaults
parts yabai yabaiplay yabaitree yabaivote
# The message window's plate and its Menu tab stay over the hub and the vote, as on the PC.
partplate
# MS Gothic at init.ini's 25, as the exe names only the Japanese faces; the PC's lines 30 apart, its ink 1 right and 1 up
# from where the runtime puts it (Wine with ＭＳ ゴシック substituted, prologue's sixth line).
textline 30
textshift 1 -1

# The interrogation hub (yabaiplay), as measured under Wine:
#   truth T HOURS FIRST [START]   the hours truth T gives, whom the hub shows first, and its starting hour (8 if left out)
#   lock T HEROINE STAGE NEEDS... that stage opens once each heroine it needs is done
screen hub
truth 1 18 A
truth 2 17 M
truth 3 15 M
truth 4 14 M
truth 5 12 A 7
lock 1 A 3 M R
lock 1 M 3 R
lock 2 A 2 R
lock 2 M 3 A R
lock 3 M 2 A R
lock 3 A 3 R
lock 4 M 2 A R
lock 4 A 3 R
lock 5 A 3 M R

# Title, measured under Wine: the hall, its dark frame and the logo; five buttons and WEB QUESTION along the bottom, each
# with its help box 46 left of and 80 above it. ADDITION opens GRAPHIC, SCENE and MUSIC above it and dims the row.
# PROLOGUE and MAIN (d001_002's first two) belong to START; when the PC offers them isn't known yet.
screen title
selectedlook
submenudim
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\d000_002.bmp 240 55
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
button start obj\d001_000.bmp 256 636 5 5 0 help obj\d002_000.bmp 210 556 6 1 0 0
button load obj\d001_000.bmp 402 636 5 5 1 help obj\d002_000.bmp 356 556 6 1 1 0
button submenu:bonus obj\d001_000.bmp 548 636 5 5 2 help obj\d002_000.bmp 502 556 6 1 2 0
button config obj\d001_000.bmp 694 636 5 5 3 help obj\d002_000.bmp 648 556 6 1 3 0
button quit obj\d001_000.bmp 840 636 5 5 4 help obj\d002_000.bmp 794 556 6 1 4 0
button websurvey obj\d001_001.bmp 1026 636 1 5 0 help obj\d002_000.bmp 980 556 6 1 5 0
button cg obj\d001_002.bmp 548 450 5 5 2 group bonus help obj\d002_001.bmp 502 370 5 1 2 0
button scene obj\d001_002.bmp 548 510 5 5 3 group bonus help obj\d002_001.bmp 502 430 5 1 3 0
button music obj\d001_002.bmp 548 570 5 5 4 group bonus help obj\d002_001.bmp 502 490 5 1 4 0

# Load and save, measured on the PC's load screen: six cards three across, ten pages, the nav bar along the bottom.
screen load
selectedlook
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 1 0
slots obj\e004_000.bmp 90 144 371 224 2 3 1
slotthumb 13 13
slotnew obj\e005_000.bmp -13 -14
slotcommentbutton obj\e004_002.bmp 253 130
slotdigits obj\e004_001.bmp 40 135 3 11 13
datedigits obj\e004_001.bmp 0 0 13
datemark 10 263 26
datemark 10 296 26
datemark 11 307 50
datefield year 219 26 11
datefield month 274 26 11
datefield day 307 26 11 blank
datefield hour 285 50 11 blank
datefield minute 318 50 11
button page0 obj\e003_001.bmp 698 68 10 5 0
button page1 obj\e003_001.bmp 748 68 10 5 1
button page2 obj\e003_001.bmp 798 68 10 5 2
button page3 obj\e003_001.bmp 848 68 10 5 3
button page4 obj\e003_001.bmp 898 68 10 5 4
button page5 obj\e003_001.bmp 948 68 10 5 5
button page6 obj\e003_001.bmp 998 68 10 5 6
button page7 obj\e003_001.bmp 1048 68 10 5 7
button page8 obj\e003_001.bmp 1098 68 10 5 8
button page9 obj\e003_001.bmp 1148 68 10 5 9
button nav_save obj\e002_000.bmp 51 637 9 5 0
button nav_load obj\e002_000.bmp 181 637 9 5 1
button nav_cg obj\e002_000.bmp 311 637 9 5 2
button nav_scene obj\e002_000.bmp 441 637 9 5 3
button nav_music obj\e002_000.bmp 571 637 9 5 4
button nav_config obj\e002_000.bmp 701 637 9 5 5
button nav_title obj\e002_000.bmp 831 637 9 5 6
button nav_quit obj\e002_000.bmp 961 637 9 5 7
button back obj\e002_000.bmp 1091 637 9 5 8
button pageprev obj\e003_000.bmp 22 331 2 5 0
button pagenext obj\e003_000.bmp 1218 331 2 5 1

screen save
selectedlook
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 0 0
slots obj\e004_000.bmp 90 144 371 224 2 3 1
slotthumb 13 13
slotnew obj\e005_000.bmp -13 -14
slotcommentbutton obj\e004_002.bmp 253 130
slotdigits obj\e004_001.bmp 40 135 3 11 13
datedigits obj\e004_001.bmp 0 0 13
datemark 10 263 26
datemark 10 296 26
datemark 11 307 50
datefield year 219 26 11
datefield month 274 26 11
datefield day 307 26 11 blank
datefield hour 285 50 11 blank
datefield minute 318 50 11
button page0 obj\e003_001.bmp 698 68 10 5 0
button page1 obj\e003_001.bmp 748 68 10 5 1
button page2 obj\e003_001.bmp 798 68 10 5 2
button page3 obj\e003_001.bmp 848 68 10 5 3
button page4 obj\e003_001.bmp 898 68 10 5 4
button page5 obj\e003_001.bmp 948 68 10 5 5
button page6 obj\e003_001.bmp 998 68 10 5 6
button page7 obj\e003_001.bmp 1048 68 10 5 7
button page8 obj\e003_001.bmp 1098 68 10 5 8
button page9 obj\e003_001.bmp 1148 68 10 5 9
button nav_save obj\e002_000.bmp 51 637 9 5 0
button nav_load obj\e002_000.bmp 181 637 9 5 1
button nav_cg obj\e002_000.bmp 311 637 9 5 2
button nav_scene obj\e002_000.bmp 441 637 9 5 3
button nav_music obj\e002_000.bmp 571 637 9 5 4
button nav_config obj\e002_000.bmp 701 637 9 5 5
button nav_title obj\e002_000.bmp 831 637 9 5 6
button nav_quit obj\e002_000.bmp 961 637 9 5 7
button back obj\e002_000.bmp 1091 637 9 5 8
button pageprev obj\e003_000.bmp 22 331 2 5 0
button pagenext obj\e003_000.bmp 1218 331 2 5 1

screen story
flow main
# The PC keeps its mode in S 105: 2 at the hub, 4 in a scene, 8 at a vote's pick screen, 11 in the tutorial. A PC save
# at the hub or a pick goes on there, its truth in S 2000. A PC save in a scene goes on from the first line below
# naming its scene, so the lines with a chapter of their own come first and the hub's `chapter S 1000` last.
# Before each chapter the table sets what the PC's saves hold there: the chapter in S 1000, the mode in S 105, and in
# S 110 the story chapter last begun (seen 1, 100 to 400 and 503 in the PC's saves; 500 is taken as the same kind).
resume S 105 == 2 at hub
resume S 105 == 8 at pickresume
set S 1000 = 0
set S 105 = 4
chapter 0
set S 1000 = 1
set S 105 = 4
set S 110 = 1
chapter 1
# The tutorial dialog: "尋問パートの説明を見ますか？" (yes 10, no 20).
open ask 1 -> S 15121
on 1 goto tut1
set S 1000 = 20
set S 105 = 4
chapter 20
goto t1
label tut1
set S 1000 = 10
set S 105 = 11
chapter 10
label t1
open hubstart 1
goto hub
label event
set S 1000 = 50
set S 105 = 4
chapter 50
goto hub
label timeout
set S 1000 = 900
set S 105 = 4
chapter 900
end
label vote
if S 2000 == 1 goto vote1
if S 2000 == 2 goto vote2
if S 2000 == 3 goto vote3
if S 2000 == 4 goto vote4
goto vote5
label vote1
set S 1000 = 4500
set S 105 = 4
chapter 4500
# "ネット投票の説明を見ますか？" (yes 4510, no 4520).
open ask 2 -> S 15121
on 1 goto vtut1
set S 1000 = 4520
set S 105 = 4
chapter 4520
goto pick1
label vtut1
set S 1000 = 4510
set S 105 = 4
chapter 4510
label pick1
open vote 1 -> S 15140
goto picked
label vote2
set S 1000 = 4600
set S 105 = 4
chapter 4600
label pick2
open vote 2 -> S 15140
goto picked
label vote3
set S 1000 = 4700
set S 105 = 4
chapter 4700
label pick3
open vote 3 -> S 15140
goto picked
label vote4
set S 1000 = 4800
set S 105 = 4
chapter 4800
label pick4
open vote 4 -> S 15140
goto picked
label vote5
set S 1000 = 4900
set S 105 = 4
chapter 4900
label pick5
open vote 5 -> S 15140
goto picked
label pickresume
# A PC save at a pick has none of VNDegen's own variables, which the tree reads: the hub's part takes them from the PC's.
open hubsync
if S 2000 == 1 goto pick1
if S 2000 == 2 goto pick2
if S 2000 == 3 goto pick3
if S 2000 == 4 goto pick4
goto pick5
label picktree
open tree
goto pickresume
label picked
on 0 goto wrong
on 9 goto picktree
if S 2000 == 1 goto right1
if S 2000 == 2 goto right2
if S 2000 == 3 goto right3
if S 2000 == 4 goto right4
set S 1000 = 4400
set S 105 = 4
chapter 4400
goto ending
label right1
set S 1000 = 4000
set S 105 = 4
chapter 4000
set S 1000 = 100
set S 105 = 4
set S 110 = 100
chapter 100
open hubstart 2
goto hub
label right2
set S 1000 = 4100
set S 105 = 4
chapter 4100
set S 1000 = 200
set S 105 = 4
set S 110 = 200
chapter 200
open hubstart 3
goto hub
label right3
set S 1000 = 4200
set S 105 = 4
chapter 4200
set S 1000 = 300
set S 105 = 4
set S 110 = 300
chapter 300
open hubstart 4
goto hub
label right4
set S 1000 = 4300
set S 105 = 4
chapter 4300
set S 1000 = 400
set S 105 = 4
set S 110 = 400
chapter 400
open hubstart 5
goto hub
label wrong
set S 1000 = 4010
set S 105 = 4
chapter 4010
end
label ending
set S 1000 = 500
set S 105 = 4
set S 110 = 500
chapter 500
# "セーブを行いますか？": yes opens the save screen.
open ask 3 -> S 15121
set S 1000 = 503
set S 105 = 4
set S 110 = 503
chapter 503
# "復讐の方法を選択してください。"
open ask 4 -> S 15121
on 1 goto arson
on 2 goto release
set S 1000 = 600
set S 105 = 4
chapter 600
staffroll
set S 1000 = 602
set S 105 = 4
chapter 602
end
label arson
set S 1000 = 610
set S 105 = 4
chapter 610
staffroll
set S 1000 = 612
set S 105 = 4
chapter 612
end
label release
set S 1000 = 620
set S 105 = 4
chapter 620
staffroll
set S 1000 = 622
set S 105 = 4
chapter 622
end
# The interrogation hub, last: its chapters are the questions' (1000 to 3999), a replay's from the tree, and in truth 5
# each heroine's devil scene (910 Ayano, 920 Misuzu, 930 Rina), all handed over in S 1000.
label hub
open hub -> S 15120
on 9 goto tree
on 2 goto vote
on 3 goto timeout
on 4 goto event
chapter S 1000
goto hub
label tree
open tree
goto hub
)";
