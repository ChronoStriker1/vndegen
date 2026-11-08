// The runtime's own test game (NekoSDKStoryTableTests), and an example of a story table: a game with no flow scripts,
// whose order of scenes the table gives, with the test part, `selftest`, for screens and a chapter.
const char kStoryTest[] = R"(
screen defaults
parts selftest
lastchoice made            # saves hold 2 once a choice is made, as Test Game 83's do

screen story
flow main
open selftestwait 1 -> S 12 # a screen before any scene, so a save there names none
scene scr\a.txt @30        # the same number as the last scene: a PC save picks between them by its scene
open selftest 3 -> S 5     # a game part's screen; its result to scenario variable 5
on 3 goto three
set S 6 = 1                # skipped: the screen gave 3
label three
if S 5 == 3 goto yes
set S 7 = 1                # skipped: the condition holds
label yes
set S 8 += S 5
chapter 2                  # the selftest part gives scr\c2.txt
on 3 goto kept             # the screen's result still, across the chapter and a load
set S 11 = 1               # skipped
label kept
open selftestwait 2 -> S 9 # waits for two presses
on 2 goto two
set S 10 = 1               # skipped
label two
scene scr\b.txt @20       # two lines of text, then the choice; @20 is the PC's number for the place
choice obj\n001_000.bmp 2 @25
on 1 goto second
set S 13 = 1               # skipped: the second option is taken
label second
scene scr\e.txt @30
end
)";
