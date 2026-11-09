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
# Config as the PC's 初期化 leaves it.
default opacity 100
default text 50
default auto 50
default bgm 60
default se 60
default hse 60
default voice 80
default backvoice 80
default skipall 0
default bulb 1
# The H sound effects, with a volume of their own, are the se_h files.
hseprefix se_h
# The PC keeps a seen-CG flag for each picture by its line's group in cg_view.ini (its heroine; 1,000 flags a group) and
# its place in the group, and flags an entry's every picture once one is seen: after Ayano's and two of Rina's truth 3
# scenes its cfg.sav gained flags 43 to 54 and 2032 to 2053, three whole entries.
cgflagsbygroup 4000
cgwhole
# The window's four buttons show whenever the window does, and the click-wait mark stands still, as on the PC.
inneralways
markstill

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
# Once the prologue has been played through (cfg.sav's global 2), START opens PROLOGUE and MAIN above it instead: MAIN
# begins at truth 1's opening.
screen title
selectedlook
submenudim all
startmenu 2
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
button start_prologue obj\d001_002.bmp 256 509 5 5 0 group start help obj\d002_001.bmp 210 429 5 1 0 0
button start_main obj\d001_002.bmp 256 569 5 5 1 group start help obj\d002_001.bmp 210 489 5 1 1 0
button cg obj\d001_002.bmp 548 450 5 5 2 group bonus help obj\d002_001.bmp 502 370 5 1 2 0
button scene obj\d001_002.bmp 548 510 5 5 3 group bonus help obj\d002_001.bmp 502 430 5 1 3 0
button music obj\d001_002.bmp 548 570 5 5 4 group bonus help obj\d002_001.bmp 502 490 5 1 4 0

screen start
flow mainstory

# Load and save, measured on the PC's load screen: six cards three across, ten pages, the nav bar along the bottom.
screen load
selectedlook
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
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
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0
button pageprev obj\e003_000.bmp 22 331 2 5 0
button pagenext obj\e003_000.bmp 1218 331 2 5 1

screen save
selectedlook
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
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
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0
button pageprev obj\e003_000.bmp 22 331 2 5 0
button pagenext obj\e003_000.bmp 1218 331 2 5 1

# The CG and scene galleries, measured on the PC under Wine: a tab for each heroine and one for the rest along the
# bottom, eight cards a page, and the tab's heroine standing at the right (the others' panel for the last tab).
screen cg
selectedlook
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 2 0
# The tab's heroine stands over the top bar and stops at the bottom one.
pic sc\sc_000_000_000_000_000.bmp 850 0 part 0 0 500 628 tab 0
pic sc\sc_001_000_000_000_000.bmp 850 0 part 0 0 400 628 tab 1
pic sc\sc_002_000_000_000_000.bmp 850 0 part 0 0 500 628 tab 2
pic sc\sc_003_000_000_000_000.bmp 850 0 part 0 0 400 628 tab 3
pic obj\f001_001.bmp 850 0 part 0 0 455 628 tab 4
gallerytabs
button gtab0 obj\f001_000.bmp 147 547 5 5 0
button gtab1 obj\f001_000.bmp 297 547 5 5 1
button gtab2 obj\f001_000.bmp 447 547 5 5 2
button gtab3 obj\f001_000.bmp 597 547 5 5 3
button gtab4 obj\f001_000.bmp 747 547 5 5 4
cellsheet obj\f000_000.bmp
cellsize 205 176
cellthumb 5 6
celldigits obj\e004_001.bmp 40 120 2 11 13
cellcount obj\e004_001.bmp - 143 151 165 176 0 13
cellcountdash 12
cell 90 139
cell 314 139
cell 538 139
cell 762 139
cell 90 337
cell 314 337
cell 538 337
cell 762 337
button page0 obj\e003_001.bmp 698 68 10 5 0
button page1 obj\e003_001.bmp 748 68 10 5 1
button page2 obj\e003_001.bmp 798 68 10 5 2
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0

screen scene
selectedlook
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 3 0
# The tab's heroine stands over the top bar and stops at the bottom one.
pic sc\sc_000_000_000_000_000.bmp 850 0 part 0 0 500 628 tab 0
pic sc\sc_001_000_000_000_000.bmp 850 0 part 0 0 400 628 tab 1
pic sc\sc_002_000_000_000_000.bmp 850 0 part 0 0 500 628 tab 2
pic sc\sc_003_000_000_000_000.bmp 850 0 part 0 0 400 628 tab 3
pic obj\f001_001.bmp 850 0 part 0 0 455 628 tab 4
gallerytabs
button gtab0 obj\f001_000.bmp 147 547 5 5 0
button gtab1 obj\f001_000.bmp 297 547 5 5 1
button gtab2 obj\f001_000.bmp 447 547 5 5 2
button gtab3 obj\f001_000.bmp 597 547 5 5 3
button gtab4 obj\f001_000.bmp 747 547 5 5 4
cellsheet obj\f000_000.bmp
cellsize 205 176
cellthumb 5 6
celldigits obj\e004_001.bmp 40 120 2 11 13
cell 90 139
cell 314 139
cell 538 139
cell 762 139
cell 90 337
cell 314 337
cell 538 337
cell 762 337
button page0 obj\e003_001.bmp 698 68 10 5 0
button page1 obj\e003_001.bmp 748 68 10 5 1
button page2 obj\e003_001.bmp 798 68 10 5 2
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0

# Music, measured on the PC under Wine: eleven tracks down three columns, each listed once heard, and the player bar.
# With nothing playing a track is only chosen and Play starts it. The volume knob runs 1.9 pixels to a unit (60 at
# x 687, 100 at 763).
screen music
selectedlook
trackselects
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 4 0
pic obj\g000_000.bmp 87 141
pic obj\g002_000.bmp 88 503
musictitle obj\g003_000.bmp 112 505 11
knob music obj\g002_001.bmp 573 517 1.9
hovercols 577 512 790 546 obj\g002_003.bmp 538 430 8 7
button track0 obj\g001_000.bmp 87 158 11 5 0
button track1 obj\g001_000.bmp 87 229 11 5 1
button track2 obj\g001_000.bmp 87 300 11 5 2
button track3 obj\g001_000.bmp 87 371 11 5 3
button track4 obj\g001_000.bmp 462 158 11 5 4
button track5 obj\g001_000.bmp 462 229 11 5 5
button track6 obj\g001_000.bmp 462 300 11 5 6
button track7 obj\g001_000.bmp 462 371 11 5 7
button track8 obj\g001_000.bmp 837 158 11 5 8
button track9 obj\g001_000.bmp 837 229 11 5 9
button track10 obj\g001_000.bmp 837 300 11 5 10
button music_prev obj\g002_002.bmp 849 512 7 5 0 help obj\g002_003.bmp 754 430 8 1 0 0
button music_pause obj\g002_002.bmp 900 512 7 5 1 help obj\g002_003.bmp 805 430 8 1 1 0
button music_play obj\g002_002.bmp 951 512 7 5 2 help obj\g002_003.bmp 856 430 8 1 2 0
button music_stop obj\g002_002.bmp 1002 512 7 5 3
button music_next obj\g002_002.bmp 1053 512 7 5 4 help obj\g002_003.bmp 958 430 8 1 4 0
button music_repeat obj\g002_002.bmp 1104 512 7 5 5 help obj\g002_003.bmp 1009 430 8 1 5 0
button music_shuffle obj\g002_002.bmp 1155 512 7 5 6 help obj\g002_003.bmp 1060 430 8 1 6 0
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0

# Config, measured on the PC under Wine: four tabs over one frame, with the side arrows going round them. Each row's help
# stands 69 above it at x 218 while the pointer is in the row's band (found by moving the pointer across its edges).
# Screen mode, colours and resolutions are the PC window's own and are drawn as the PC showed them.
screen config
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 5 0
pic obj\h001_000.bmp 385 66
numbercells 13
button tab obj\h001_001.bmp 392 73 4 5 0
button tab2 obj\h001_001.bmp 525 73 4 5 1
button tab3 obj\h001_001.bmp 658 73 4 5 2
button tab4 obj\h001_001.bmp 791 73 4 5 3
button cfg_apply obj\h000_000.bmp 1122 100 3 5 0 help obj\h000_001.bmp 1058 26 3 1 0 0
button cfg_undo obj\h000_000.bmp 1122 158 3 5 1 help obj\h000_001.bmp 1058 84 3 1 1 0
button cfg_defaults obj\h000_000.bmp 1122 216 3 5 2 help obj\h000_001.bmp 1058 142 3 1 2 0
button tab4 obj\e003_000.bmp 22 331 2 5 0
button tab2 obj\e003_000.bmp 1218 331 2 5 1
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0
pic obj\h002_000.bmp 214 128
radio screen 0 obj\h002_001.bmp 557 163 10 5 0
radio screen 1 obj\h002_001.bmp 671 163 10 5 1
pic obj\h002_001.bmp 557 199 10 5 2 0
pic obj\h002_001.bmp 671 199 10 5 3 0
pic obj\h002_001.bmp 557 235 10 5 4 2
pic obj\h002_001.bmp 671 235 10 5 5 0
pic obj\h002_001.bmp 785 235 10 5 6 0
pic obj\h002_001.bmp 557 271 10 5 7 4
pic obj\h002_001.bmp 671 271 10 5 8 4
pic obj\h002_001.bmp 785 271 10 5 9 0
radio effect 1 obj\h002_002.bmp 557 307 2 5 0
radio effect 0 obj\h002_002.bmp 671 307 2 5 1
# 背景の電球演出, the flickering bulb some backgrounds have (cfg\light_info.ini); the flicker itself isn't drawn yet.
radio bulb 1 obj\h002_002.bmp 557 343 2 5 0
radio bulb 0 obj\h002_002.bmp 671 343 2 5 1
# The PC draws this row's OFF 3 pixels lower than its ON.
radio wait 1 obj\h002_002.bmp 557 383 2 5 0
radio wait 0 obj\h002_002.bmp 671 386 2 5 1
knob opacity obj\g002_001.bmp 321 461 6
number opacity obj\e004_001.bmp 896 422 blank
step opacity -1 obj\h002_003.bmp 282 458 2 5 0
step opacity 1 obj\h002_003.bmp 954 458 2 5 1
radio confirm_saveload 1 obj\h002_002.bmp 557 506 2 5 0
radio confirm_saveload 0 obj\h002_002.bmp 671 506 2 5 1
group confirm_saveload confirm_overwrite confirm_delete confirm_quick confirm_title confirm_exit
radio tutorial 1 obj\h002_002.bmp 557 542 2 5 0
radio tutorial 0 obj\h002_002.bmp 671 542 2 5 1
# ゲームを起動するたびに画面解像度を選択する: the PC window's own, shown unticked.
pic obj\a001_002.bmp 417 588 1 5 0 0
hovercols 219 163 1045 189 obj\h003_000.bmp 218 94 9 0
hovercols 219 199 1045 225 obj\h003_000.bmp 218 130 9 1
hovercols 219 235 1045 297 obj\h003_000.bmp 218 166 9 2
hovercols 219 307 1045 333 obj\h003_000.bmp 218 238 9 3
hovercols 219 343 1045 369 obj\h003_000.bmp 218 274 9 4
hovercols 219 383 1045 409 obj\h003_000.bmp 218 314 9 5
hovercols 219 419 1045 497 obj\h003_000.bmp 218 350 9 6
hovercols 219 506 1045 532 obj\h003_000.bmp 218 438 9 7
hovercols 219 542 1045 568 obj\h003_000.bmp 218 474 9 8

screen config2
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 5 0
pic obj\h001_000.bmp 385 66
numbercells 13
button tab obj\h001_001.bmp 392 73 4 5 0
button tab2 obj\h001_001.bmp 525 73 4 5 1
button tab3 obj\h001_001.bmp 658 73 4 5 2
button tab4 obj\h001_001.bmp 791 73 4 5 3
button cfg_apply obj\h000_000.bmp 1122 100 3 5 0 help obj\h000_001.bmp 1058 26 3 1 0 0
button cfg_undo obj\h000_000.bmp 1122 158 3 5 1 help obj\h000_001.bmp 1058 84 3 1 1 0
button cfg_defaults obj\h000_000.bmp 1122 216 3 5 2 help obj\h000_001.bmp 1058 142 3 1 2 0
button tab obj\e003_000.bmp 22 331 2 5 0
button tab3 obj\e003_000.bmp 1218 331 2 5 1
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0
pic obj\h004_000.bmp 214 128
knob text obj\g002_001.bmp 321 205 6
number text obj\e004_001.bmp 896 166 blank
step text -1 obj\h002_003.bmp 282 202 2 5 0
step text 1 obj\h002_003.bmp 954 202 2 5 1
knob auto obj\g002_001.bmp 321 300 6
number auto obj\e004_001.bmp 896 261 blank
step auto -1 obj\h002_003.bmp 282 297 2 5 0
step auto 1 obj\h002_003.bmp 954 297 2 5 1
# The wait time runs from x0.00 to x2.00 on the PC, its knob 3 pixels to a hundredth.
range waittime 0 200
knob waittime obj\g002_001.bmp 321 395 3
number waittime obj\e004_001.bmp 890 356 907 918
step waittime -1 obj\h002_003.bmp 282 392 2 5 0
step waittime 1 obj\h002_003.bmp 954 392 2 5 1
pic obj\h003_002.bmp 878 357
# Each slider's help while the pointer is in its band; the bands' edges are within 3 pixels of the PC's.
hovercols 219 163 1045 247 obj\h005_000.bmp 218 94 3 0
hovercols 219 258 1045 343 obj\h005_000.bmp 218 189 3 1
hovercols 219 353 1045 439 obj\h005_000.bmp 218 284 3 2

screen config3
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 5 0
pic obj\h001_000.bmp 385 66
numbercells 13
button tab obj\h001_001.bmp 392 73 4 5 0
button tab2 obj\h001_001.bmp 525 73 4 5 1
button tab3 obj\h001_001.bmp 658 73 4 5 2
button tab4 obj\h001_001.bmp 791 73 4 5 3
button cfg_apply obj\h000_000.bmp 1122 100 3 5 0 help obj\h000_001.bmp 1058 26 3 1 0 0
button cfg_undo obj\h000_000.bmp 1122 158 3 5 1 help obj\h000_001.bmp 1058 84 3 1 1 0
button cfg_defaults obj\h000_000.bmp 1122 216 3 5 2 help obj\h000_001.bmp 1058 142 3 1 2 0
button tab2 obj\e003_000.bmp 22 331 2 5 0
button tab4 obj\e003_000.bmp 1218 331 2 5 1
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0
pic obj\h006_000.bmp 214 128
radio skipall 0 obj\h006_001.bmp 557 163 9 5 0
radio skipall 1 obj\h006_001.bmp 671 163 9 5 1
radio rightclick 0 obj\h006_001.bmp 557 199 9 5 2
radio rightclick 1 obj\h006_001.bmp 671 199 9 5 3
radio rightclick 2 obj\h006_001.bmp 785 199 9 5 4
radio rightclick 3 obj\h006_001.bmp 557 235 9 5 5
radio rightclick 4 obj\h006_001.bmp 671 235 9 5 6
radio deckey 0 obj\h006_001.bmp 557 271 9 5 7
radio deckey 1 obj\h006_001.bmp 671 271 9 5 8
radio pause_inactive 1 obj\h002_002.bmp 557 307 2 5 0
radio pause_inactive 0 obj\h002_002.bmp 671 307 2 5 1
hovercols 219 163 1045 189 obj\h007_000.bmp 218 94 4 0
hovercols 219 199 1045 261 obj\h007_000.bmp 218 130 4 1
hovercols 219 271 1045 297 obj\h007_000.bmp 218 202 4 2
hovercols 219 307 1045 333 obj\h007_000.bmp 218 238 4 3

screen config4
pic bg\bg_001_000_000_000_000.bmp 0 0
pic obj\d000_000.bmp 0 0
pic obj\e000_001.bmp 0 628
pic obj\d000_001.bmp 0 0
pic obj\e000_000.bmp 0 0
pic obj\e001_000.bmp 0 0 6 1 5 0
pic obj\h001_000.bmp 385 66
numbercells 13
button tab obj\h001_001.bmp 392 73 4 5 0
button tab2 obj\h001_001.bmp 525 73 4 5 1
button tab3 obj\h001_001.bmp 658 73 4 5 2
button tab4 obj\h001_001.bmp 791 73 4 5 3
button cfg_apply obj\h000_000.bmp 1122 100 3 5 0 help obj\h000_001.bmp 1058 26 3 1 0 0
button cfg_undo obj\h000_000.bmp 1122 158 3 5 1 help obj\h000_001.bmp 1058 84 3 1 1 0
button cfg_defaults obj\h000_000.bmp 1122 216 3 5 2 help obj\h000_001.bmp 1058 142 3 1 2 0
button tab3 obj\e003_000.bmp 22 331 2 5 0
button tab obj\e003_000.bmp 1218 331 2 5 1
button nav_save obj\e002_000.bmp 51 637 9 5 0 help obj\e002_001.bmp 2 556 9 1 0 0
button nav_load obj\e002_000.bmp 181 637 9 5 1 help obj\e002_001.bmp 134 556 9 1 1 0
button nav_cg obj\e002_000.bmp 311 637 9 5 2 help obj\e002_001.bmp 266 556 9 1 2 0
button nav_scene obj\e002_000.bmp 441 637 9 5 3 help obj\e002_001.bmp 398 556 9 1 3 0
button nav_music obj\e002_000.bmp 571 637 9 5 4 help obj\e002_001.bmp 530 556 9 1 4 0
button nav_config obj\e002_000.bmp 701 637 9 5 5 help obj\e002_001.bmp 662 556 9 1 5 0
button nav_title obj\e002_000.bmp 831 637 9 5 6 help obj\e002_001.bmp 794 556 9 1 6 0
button nav_quit obj\e002_000.bmp 961 637 9 5 7 help obj\e002_001.bmp 926 556 9 1 7 0
button back obj\e002_000.bmp 1091 637 9 5 8 help obj\e002_001.bmp 1058 556 9 1 8 0
pic obj\h008_000.bmp 214 128
# The test buttons play what the PC plays (the files it opened under Wine).
knob master obj\g002_001.bmp 351 177 6
number master obj\e004_001.bmp 896 156 blank
step master -1 obj\h002_003.bmp 312 174 2 5 0
step master 1 obj\h002_003.bmp 984 174 2 5 1
check mute_master obj\h008_002.bmp 238 166 1 5 0
pic obj\h010_000.bmp 843 153 1 5 0 4
knob bgm obj\g002_001.bmp 351 240 6
number bgm obj\e004_001.bmp 896 219 blank
step bgm -1 obj\h002_003.bmp 312 237 2 5 0
step bgm 1 obj\h002_003.bmp 984 237 2 5 1
check mute_bgm obj\h008_002.bmp 238 230 1 5 0
pic obj\h010_000.bmp 843 217 1 5 0 4
knob se obj\g002_001.bmp 351 304 6
number se obj\e004_001.bmp 896 283 blank
step se -1 obj\h002_003.bmp 312 301 2 5 0
step se 1 obj\h002_003.bmp 984 301 2 5 1
check mute_se obj\h008_002.bmp 238 294 1 5 0
button test:se:se\se_603d.ogg obj\h010_000.bmp 843 281 1 5 0
knob hse obj\g002_001.bmp 351 368 6
number hse obj\e004_001.bmp 896 347 blank
step hse -1 obj\h002_003.bmp 312 365 2 5 0
step hse 1 obj\h002_003.bmp 984 365 2 5 1
check mute_hse obj\h008_002.bmp 238 358 1 5 0
button test:se:se\se_h21.ogg obj\h010_000.bmp 843 345 1 5 0
knob voice obj\g002_001.bmp 351 432 6
number voice obj\e004_001.bmp 896 411 blank
step voice -1 obj\h002_003.bmp 312 429 2 5 0
step voice 1 obj\h002_003.bmp 984 429 2 5 1
check mute_voice obj\h008_002.bmp 238 422 1 5 0
button test:voice:voice\rin1_0078.ogg obj\h010_000.bmp 843 409 1 5 0
knob backvoice obj\g002_001.bmp 351 496 6
number backvoice obj\e004_001.bmp 896 475 blank
step backvoice -1 obj\h002_003.bmp 312 493 2 5 0
step backvoice 1 obj\h002_003.bmp 984 493 2 5 1
check mute_backvoice obj\h008_002.bmp 238 486 1 5 0
button test:voice:voice\aya9_0019.ogg obj\h010_000.bmp 843 473 1 5 0
radio voice_stop 1 obj\h002_002.bmp 557 544 2 5 0
radio voice_stop 0 obj\h002_002.bmp 671 544 2 5 1
check voice0 obj\h008_001.bmp 376 580 6 5 0
check voice1 obj\h008_001.bmp 490 580 6 5 1
check voice2 obj\h008_001.bmp 604 580 6 5 2
check voice3 obj\h008_001.bmp 718 580 6 5 3
check voice4 obj\h008_001.bmp 832 580 6 5 4
check voice5 obj\h008_001.bmp 946 580 6 5 5
# The volumes' bands are taken as 57 tall around each knob; the PC's edges were probed only to about 6 pixels.
hovercols 219 148 1045 205 obj\h009_000.bmp 218 79 8 0
hovercols 219 211 1045 268 obj\h009_000.bmp 218 142 8 1
hovercols 219 275 1045 332 obj\h009_000.bmp 218 205 8 2
hovercols 219 339 1045 396 obj\h009_000.bmp 218 268 8 3
hovercols 219 403 1045 460 obj\h009_000.bmp 218 331 8 4
hovercols 219 467 1045 524 obj\h009_000.bmp 218 394 8 5
hovercols 219 546 1045 571 obj\h009_000.bmp 218 476 8 6
hovercols 219 582 1045 608 obj\h009_000.bmp 218 512 8 7

# The PC's questions, measured under Wine on "ロードしますか？": the screen behind dimmed to about 14%, the band across
# the middle with the question (a cell of obj\a003_000.bmp, in the order below) and はい and いいえ under it, as the
# story's own questions are (the yabaiplay part's `open ask`).
screen dialog
questions resolution save load overwrite exit title
dim 221
pic obj\a002_000.bmp 0 316
pic obj\a003_000.bmp 438 316 6 1 0 0
button yes obj\a003_001.bmp 522 390 2 5 0
button no obj\a003_001.bmp 650 390 2 5 1

# 96's cfg.sav is the older 96,000 bytes, as 82's: seen CGs from 48000, heard tracks from 92000, no string before them.
# The `run` lines are what the PC's first file holds before the globals (its Config as numbers), so a file VNDegen
# starts doesn't hand the PC a Config of zeros.
screen cfgsav
size 96000
tracks 92000
nostring
run 4 2 1
run 16 2 50
run 24 1 100
run 36 1 100
run 40 2 1
run 48 1 2
run 52 1 1
run 76 1 1
run 80 1 100
run 84 3 60
run 96 2 80
run 104 1 60
run 120 7 1
run 160 1 3
run 168 2 1
run 220 1 1
run 400 100 1

screen story
# New Game, and START > PROLOGUE.
flow main
# The PC keeps its mode in S 105: 2 at the hub, 4 in a scene, 8 at a vote's pick screen, 11 in the tutorial. A PC save
# at the hub or a pick goes on there, its truth in S 2000. A PC save in a scene goes on from the first line below
# naming its scene, so the lines with a chapter of their own come first and the hub's `chapter S 1000` last.
# S 2060 is 1 from a vote's pick until the next truth starts.
# Before each chapter the table sets what the PC's saves hold there: the chapter in S 1000, the mode in S 105, and in
# S 110 the story chapter last begun (seen 1, 100 to 400 and 503 in the PC's saves; 500 is taken as the same kind).
# S 2083 is 1 outside the truths, in the prologue and from the ending's start: a save in the prologue without it goes on
# to the game over on the PC. The PC's save inside chapter 1 already holds truth 1's clock, so each truth starts before
# its opening chapter.
set S 2083 = 1
set S 1000 = 0
set S 105 = 4
chapter 0
# The prologue played through: START offers PROLOGUE and MAIN from now on (cfg.sav under Wine).
set G 2 = 1
jump mainstory
# START > MAIN begins here, at truth 1's opening (the PC opens sc_000_001_000_000 at once).
flow mainstory
resume S 105 == 2 at hub
resume S 105 == 8 at pickresume
set S 2083 = 0
open hubstart 1
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
set S 2060 = 1
open vote 1 -> S 15140
goto picked
label vote2
set S 1000 = 4600
set S 105 = 4
chapter 4600
label pick2
set S 2060 = 1
open vote 2 -> S 15140
goto picked
label vote3
set S 1000 = 4700
set S 105 = 4
chapter 4700
label pick3
set S 2060 = 1
open vote 3 -> S 15140
goto picked
label vote4
set S 1000 = 4800
set S 105 = 4
chapter 4800
label pick4
set S 2060 = 1
open vote 4 -> S 15140
goto picked
label vote5
set S 1000 = 4900
set S 105 = 4
chapter 4900
label pick5
set S 2060 = 1
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
set S 2060 = 0
open hubstart 2
set S 1000 = 100
set S 105 = 4
set S 110 = 100
chapter 100
goto hub
label right2
set S 1000 = 4100
set S 105 = 4
chapter 4100
set S 2060 = 0
open hubstart 3
set S 1000 = 200
set S 105 = 4
set S 110 = 200
chapter 200
goto hub
label right3
set S 1000 = 4200
set S 105 = 4
chapter 4200
set S 2060 = 0
open hubstart 4
set S 1000 = 300
set S 105 = 4
set S 110 = 300
chapter 300
goto hub
label right4
set S 1000 = 4300
set S 105 = 4
chapter 4300
set S 2060 = 0
open hubstart 5
set S 1000 = 400
set S 105 = 4
set S 110 = 400
chapter 400
goto hub
label wrong
set S 1000 = 4010
set S 105 = 4
chapter 4010
end
label ending
# The PC's save before the ending's choice counts vote 5 among the truths done.
set S 2083 = 1
set S 2003 = 5
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
