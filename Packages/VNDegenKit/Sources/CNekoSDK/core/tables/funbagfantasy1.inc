// Test Game 82, Funbag Fantasy (MangaGamer's English release, 2016). The oldest layout: backgrounds, CGs and standing
// pictures by commands 10 to 14, and its story order in the exe, traced under Wine (`screen story`).
const char kFunbagFantasy1[] = R"(
screen defaults
layoutcgslot 102             # 82's saves hold a CG in slot 102, and a background and a CG each take the other away

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
if S 200 >= 3 goto c5a     # inside all three times: 11a and its choice; under 3 (checked at 2), straight to 11b
scene scr\ep_00_11b.txt      # also the second option's scene below: told apart by S200 (under 3 here)
goto c5
label c5a
scene scr\ep_00_11a.txt
choice obj\m000_000.bmp 2 cells 4 5  # I promise to return after I reach the capital! / I'll never forget you, Roxanne.
on 1 goto c5b
set S 200 += 2
scene scr\ep_00_11c.txt
goto c5
label c5b
scene scr\ep_00_11b.txt      # with S200 at 3: the first place has it under 3
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
staffroll
scene scr\ep_ai_01.txt
end
label capital
scene scr\ep_02_07a.txt
scene scr\ep_02_07b.txt
scene scr\ep_ss_00.txt       # Shamsiel's ending
staffroll
scene scr\ep_ss_01.txt
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
staffroll
scene scr\ep_lc_01.txt
end
label harem
scene scr\ep_03_10d.txt
scene scr\ep_03_11.txt
scene scr\ep_03_11a.txt
scene scr\ep_03_12.txt
scene scr\ep_03_13.txt
scene scr\ep_03_14.txt
staffroll                    # the harem ending's roll comes before its first scene
scene scr\ep_zh_00.txt
choice obj\m000_000.bmp 3 cells 15 15 15  # Let nature take its course / Let nature take its course / Let nature take its course
scene scr\ep_zh_01.txt       # every option goes on here
scene scr\ep_zh_02.txt
scene scr\ep_zh_03.txt
end
label boan
scene scr\ep_rk_00.txt       # Roxanne's ending
staffroll
scene scr\ep_rk_01.txt
end
)";
