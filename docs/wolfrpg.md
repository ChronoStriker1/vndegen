# Wolf RPG Editor

VNDegen runs Wolf RPG Editor games on its own Wolf RPG Editor runtime, written for VNDegen in C++20 and MIT licensed
like the rest of the app. No open-source Wolf RPG Editor runtime exists; the original is a closed Windows program built
on DxLib. How the runtime was written, and from which sources, is in `docs/wolfrpg-cleanroom.md`.

## Layout

| Part | Where | What it does |
|---|---|---|
| Runtime | `Packages/VNDegenKit/Sources/CWolfRPG/core/` | Game data and archives, the event interpreter, characters and movement, drawing on the CPU, text, sound, saves |
| C interface | `Packages/VNDegenKit/Sources/CWolfRPG/include/vnwolf.h` | What the app calls: start, step, frame, audio, keys, pause, stop |
| Plugin | `Packages/VNDegenKit/Sources/EngineWolfRPG/` | Detection, the session, input mapping, save locations |
| Presenter | `Packages/VNDegenKit/Sources/FrameHost/` | Shows the runtime's frame through Metal and plays its sound through AVAudioEngine, shared with the other software-composited runtimes |
| Linux test host | `Engines/WolfRPG/host/` | Builds the same sources on Linux with tools that run games headless. `wolfplay`'s `--save SLOT@FRAME` and `--load SLOT@FRAME` write or read `Save/SaveDataNN.sav` after that frame |

The runtime draws each frame on the CPU at the game's resolution (320 x 240 to 960 x 720 in the test games). The
session steps it once per display refresh on the main thread, and the runtime advances the game at its own frame rate
from Game.dat (60, or 30). Refreshes reach it as whole milliseconds that add up to real time. One within a
millisecond of the game's frame time, or of a half, third or quarter of it (only those of 6 ms or more), counts as
exactly that, so a display at a whole multiple of the game's rate runs without a skipped or doubled frame: a 60 fps
game runs a frame every refresh at 60 Hz and every second one at 120 Hz, and a 30 fps game every second and every
fourth. The time that matching sets aside goes back into the game once it reaches 2 ms, so on other displays the game
keeps its own rate (`core/framepace.h`). Sound is mixed at 44.1 kHz and pulled by AVAudioEngine on its audio thread; a decoder thread
of the runtime's own keeps music decoded ahead. Everything runs in the app's process, with no JIT.

## Detection

A game is Wolf RPG Editor when a folder at the top of the game folder, or up to three folders down, holds `Data.wolf`,
`Data/BasicData`, or `Data/BasicData.wolf`, beside `Game.exe` (or `GamePro.exe`) for certainty. A game two folders
down whose data the folder listing doesn't reach is found by `Game.exe` beside `Data` and `GuruguruSMF4.dll`, the MIDI
player every Wolf RPG Editor 2 game ships. When a short listing shows no game, detection looks one level deeper, for `Game.exe` or `GamePro.exe`
three folders down inside wrapper folders, such as Succubus Prison's `sys` folder when its outer folder is kept. A folder
with more than 200 entries in the listing doesn't get that deeper look, so other engines' games don't pay for it at each
library refresh.

## What the runtime reads

- Game data: `Game.dat`, `CommonEvent.dat`, the user, changeable and system databases with their `.project` files,
  `TileSetData.dat`, and the maps (`.mps`). `Game.dat`, `CommonEvent.dat` and `TileSetData.dat` may be encrypted with
  Wolf RPG Editor 2's basic data encryption. Wolf RPG Editor 3.5 compresses common events, databases and maps with
  LZ4, and its maps have their own number of tile layers. Text is Shift-JIS in Wolf RPG Editor 2 and UTF-8 in Wolf RPG Editor 3
  games that say so. Translated games often hold bytes that aren't valid Shift-JIS; they become U+FFFD.
- Archives: `Data.wolf`, or one `.wolf` archive per top-level data folder, as DxLib archives of versions 1 to 6 with
  the encryption keys of Wolf RPG Editor 2, and of version 8 with the key strings of Wolf RPG Editor 2.225 and 3.x,
  chosen by the crypt version Wolf RPG Editor 3 writes into the header. Wolf RPG Editor 3.31 to at least 3.343 write
  crypt version 0x14B, which scrambles the header's addresses and adds a file stream, an AES counter pass and a data
  stream in place of the version 8 keys (`docs/wolfrpg-cleanroom.md`); those archives are read too. Loose files win
  over archives.
- Images: PNG, JPEG and BMP (stb_image). Sound: Ogg Vorbis (stb_vorbis), MP3 (dr_mp3), WAV (dr_wav), and MIDI through
  VNDegen's shared MIDI player.
- File names match without regard to case, through `Engines/Shared/casepath.c`.

## The interpreter

Wolf RPG Editor games are event scripts: map events and common events, numbered commands with integer and string
arguments. The runtime runs the commands the library's games use: variables and strings, conditions, loops and labels,
calling common events with arguments and return values, the database command, the extended variable read (characters,
map positions, pictures, the current map), key input, messages and choices, pictures with their effects, screen colour,
shake and transitions, sound, map changes, move routes, party graphics, map tile changes, and saving and loading.
Battles and menus are common events in each game's own data, so they need nothing of their own. It also runs CSV import
and export (command 251), row copying and sorting (252), Wolf RPG Editor 3's XY arrays (255 and 257), and the string
command's typed text, regular expressions, file export and hidden codes.

An autorun map event (trigger 1) starts again each time its run ends, for as long as its page is the active one, as Wolf
RPG Editor's 自動実行 does. A game stops one by erasing the event or by changing a condition so another page takes over.
Up to 0.54 the runtime started it only once per page activation, so The Succubus of the Forest and a Doctor, whose one
autorun event calls its main loop (opening, interval, stages) by a variable, stayed black after its opening, and Two
Beasts' title ignored keyboard confirm. `Engines/WolfRPG/host/autorun_test.cpp` checks the rule.

A map event whose pages' conditions all fail isn't on the map, so a move route given to it ends at once, and an event
waiting for that route goes on. Before, the route stayed pending, and President Yukino's prologue, which waits for a
route on an event its map hides, stayed black after the move to the town.

A call by name to a common event whose name more than one common event has runs the last of them, as on the PC:
President Yukino keeps an older `■常時立絵` (28) whose portrait files aren't in its archives beside the one it uses (29). A
call to a name no common event has does nothing, also as on the PC. A move to a registered position (target -10001,
the row in X) takes the map and position from row X of the system database's position list (type 7); President Yukino
starts the player's control at "学校前" that way. "×=" with a division on the right multiplies before dividing, so 255
×= 50 ÷ 100 gives 127, which Yukino's portrait opacity setting relies on.

A common event that a parallel event reserves (command 211) starts on the next frame, as the "once-only autorun" the
base system's comments call it. Wolf RPG Editor's standard menu call reserves the menu that way on a new cancel press,
and President Yukino's menu then waits for a key: started in the same frame, it took that press as cancel and closed
before it drew, so the menu never opened.

Common events and strings can read a text file. As on the PC, the runtime takes UTF-8 with or without a byte order mark,
and otherwise Shift-JIS: The Clumsy Witch Seris is a UTF-8 game that keeps its scene text in Shift-JIS. Reading a string
variable as a number gives the number its text starts with, which Two Beasts counts on for its script labels. A choice
right after a message opens with that message still showing.

XY arrays draw as pictures through `<XY_IMAGE/...>` and `<XYN_IMAGE/...>`, The Clumsy Witch Seris's minimap, and saves of
version 159 keep them. `<CUT/Xa-b/Yc-d>` before a file name shows part of each pattern.

Map event pages hold their animation speed, move speed, move frequency and move type in that order. Characters draw from images of 3 or 5 patterns by 4 rows, or with eight-direction images (Game.dat's settings byte 1 is 8) two such blocks side by side, the diagonals in the second, as the manual's material page shows.

### Half-tile movement

Characters keep their position in half tiles, as saves and the variable command's precise coordinates count it. A game
whose Game.dat settings byte 9 is 0 moves its characters half a tile a step, as Wolf RPG Editor's "default character
move width" of 0.5; False Myth 2 is the library's one such game, and the others (byte 9 is 1) keep the whole-tile rules
described below unchanged. Each character's step is also in its save record (`v78`: 1 for half, 2 for whole): with 2
written into a False Myth 2 save, the PC's hero steps a whole tile, so a load takes it from there. Move route steps 48
and 49 switch a character between the two in such a game.

In a half-tile game, as the manual's specification page says, a character can't move while its box overlaps a blocked
quarter of a tile, and can't move into one. A tile's passability bits 0 to 3 block its bottom-right, bottom-left,
top-right and top-left quarters. The hero and party meet the tiles with the lower half of their tile when settings byte 8
is 0 (False Myth 2), and touch events and other characters with their whole tile, as their save records' size (`v8c`,
2 by 2) says; events use their lower half unless their page's square option (0x20) is on. Events placed half a tile up or
left (page options 0x40 and 0x80) start there on entering the map. Touch ranges reach that many tiles both ways, as the
manual's exit picture shows. Player touch needs the hero inside the range from side to side, event touch any overlap; a
confirm press goes through the events in id order and starts the first one the hero stands in or, for one that blocks
the way, faces. All of this was found by walking False Myth 2's hero under Wine into the Sanctuary's partly blocked
tiles, its door and its dungeon sign and reading the position from a save: it stops at the same half tiles in VNDegen
(`Engines/WolfRPG/host/halfstep_test.cpp`, with the game and its saves). Whole-tile games give the same states and
frames as before over the same scripted play (Milky Quest, Drain Dungeon, Journey of Depravity).

A message draws only its text, at the top left that system variables 1 and 2 give when it starts, as on the PC. The
engine has no message window or face of its own: the manual's message page says a game's common events draw them, and
the standard systems' message window event does, reading the face number that `@n` at the start of a message puts in
system variable 0. System string 25 keeps the last message, which Milky Quest shows in a text picture of its own after
it moves the engine's text off the screen. System strings 26 and 27 go before and after every message's text, and in a
message `\>` shows what follows at once up to `\<`, and `\^` closes the message without a key, as the manual's system
variable and special code pages say. False Myth 2 skips its scenes by putting `\>` and `\^` there while X is held. Up to 0.23.3 the runtime drew a window and face of its own over the game's,
which showed as a faint second window edge in Succubus Prison and Drain Dungeon.

Text uses a font file the game ships whose family name matches the one Game.dat names, then VNDegen's Windows font pack
(`docs/fonts.md`), then Hiragino on Apple systems. `\font[n]` switches to the sub fonts Game.dat names.

Wolf RPG Editor's standard systems (2 and 3) draw their menus, shops and status windows with pictures, and the runtime
draws what they use, following the picture page of the official manual:

- Window pictures stretch a 3 by 3 base image to the size the picture's division fields give, and report that size to
  the picture variable command. Their file name can instead be a generated shape: `<SQUARE>`, `<LINE>` and `<LINE-n>`,
  `<TRI-UP>`, `<TRI-DOWN>`, `<TRI-LEFT>` and `<TRI-RIGHT>`, each with `FRAME` for the outline, and the gradients
  `<GRADX-aaa-bbb>` and `<GRADY-aaa-bbb>`.
- Text pictures report their laid-out size, which the standard systems read to centre text and to shrink long names.
  They align with `<L>`, `<C>` and `<R>`, which apply to the rest of their line and the lines after; `\ax[n]` at the end
  of a line widens the text so that a later right-aligned line lines up with it. `\space[n]`, `\-[n]`, `\E`, `\N`,
  `\wE[n]`, `\cE[n]` and `\m[n]` change line spacing, letter spacing, outlines and line height. `\E` outlines at width
  1 and `\wE[n]` at width n; a later `\E` goes back to 1, as False Myth 2's dungeon name shows on the PC. An outlined
  character advances by twice the outline's width, as on the PC: False Myth 2's text with `\E` runs about 2 pixels wider
  per character there, and its labels' `\-[06]` with `\wE[03]` keep their natural width. A size code before any
  character sets that line's height. `\img[file]` and `\imgS[file]` draw an image from the Data folder in the text at
  its own size, scaled by `\isize[n]` percent, and `\v1[n]` to `\v9[n]` give spare variables.
- Text has a dark shadow only while system variable 21 is 1 or more, that many pixels down and right, as the manual's
  system variable page says. The standard systems set it to 1; False Myth 2 leaves it at 0, and the PC draws its text
  without one.
- Wolf draws 320 by 240 games at twice the size, so in those games `\ax`, `\ay`, `\mx`, `\my` and `\-` count in half
  pixels, as the standard systems' own layout assumes.
- Pictures place by the anchors the games use (top left, centre, bottom left, top right, bottom right, top centre),
  move relative to where they are, keep their anchor, size and blend mode on a move, keep their
  zoom or colour when the command says so, and take the effect command's draw position shift and colour pulse. A
  negative zoom mirrors a picture about its anchor. A picture shown from `<SCREENSHOT>` holds the screen as last drawn.
  The effect command's flash adds its colour and fades back over its frames. Its types 0xB0 to 0xE0 are the manual's
  draw shift before zoom, zoom correction (100 based, per direction), angle correction and opacity correction, told
  apart by how False Myth 2 uses them; each eases over the command's frames, and the variable command still reads the
  picture's own values. Its second set's 0x70 (the 0x100 flag) blurs the picture by 1 to 16: three box blur passes of
  half that radius come close to the PC's 8 behind False Myth 2's tutorial panels.
- The effect command's automatic pattern switching animates a pattern of its own: when a single run ends, or a run of 0
  frames stops it, the picture shows the pattern its picture commands gave it again, as the manual's effect page says.
  False Myth 2's enemies idle in a loop and stop it to show a hurt pose; keeping the loop's frame drew a different
  monster that shares the sheet. A divided picture at pattern 0 draws nothing.
- Showing a picture again keeps the effect command's effects on it. False Myth 2's dungeon floor objects take their zoom
  from an effect set on placeholders before the real images are shown in them.
- Free-form pictures (the four-corner flag, 0x4000000) draw their pattern into the quadrilateral the position and fields
  19 to 24 give, as two triangles; a move with the flag moves the corners. False Myth 2's skill chart is such a diamond.
- A picture command over a range (move, erase, delay reset) reaches every shown picture in it, however wide the range:
  False Myth 2's title erases 1 to 999999.

## Controls

Wolf RPG Editor reads gamepads, so the default layout is the gamepad: A confirms (Z), B and Menu cancel (X, which
opens the menu in most games), the Dash button is the sub key (Shift), RT holds Control, which many games read to skip messages, and the D-pad and left stick move. A keyboard
works as on Windows: Z, Return or Space confirm, X or Escape cancel, Shift is sub, and the arrows or the number pad move.

Event commands read the keyboard by Wolf's own key codes, 100 plus the key's DirectInput scan code. Key input on one key stores that key's code while it is down and 0 when it isn't, and games compare the result with the code. Up to and including 0.35.0 it stored 1, so those checks never fired (#86), such as MilkyQuest's Control skip. Key input from a code stores the first key down, by its code. The mouse works through
the view: a pointer, a touch under the Touch and Touch mouse control methods, or a controller's pointer actions move it, and
scrolling turns its wheel. A game reads it through key input, the variable command's "other" values, system variables 71
and 72, and whether it is over a picture. System variable 73 lets it work messages and choices.

On iPhone and iPad, a tap on the game works the engine's own choices and messages even when a game leaves system
variable 73 off, which the PC doesn't do: there a click on such a game's choice does nothing, and the player uses the
keyboard. A touch screen has no keyboard, and without this a player under the Mouse control method, or under Automatic
with a controller connected, couldn't pick from Milky Quest's title menu (#178). The game still gets the click as
before, a click from a Mac's mouse or a controller's pointer works as on the PC, and a game that turns the mouse off
with command 126 turns this off too.

Menus a game builds itself from common events, such as the base system's save and load screens, read only the keys. A
click on one of their slots does nothing on the PC either, so on a touch device the touch gamepad picks the slot (#189).

VNDegen suggests a layout from the keys a game's events check one at a time. `vnwolf_key_checks` reads `CommonEvent.dat` and every map the system database lists, from loose files or `.wolf` archives, and returns each key input on one literal key with the name of its common event (empty for a map event). Under Xbox gamepad, Wolf's router gives LT, View, L3, R3, and the right stick nothing to do, so the keys go on View, LT, L3, and R3, and the right stick stays alone. Keys a button already sends are skipped (Z, X, Shift, Control, and the arrows), and so are function keys, right Control, and keys in events named for debugging or cheats. A key whose event's name is exactly one of a few standard names (`バックログ表示`, `【並】バックログ表示`, `ログ表示`, `メッセージ簡易ログ`, `メッセージウィンドウ消去`, `戦闘高速化`, `メッセージスキップ`, `【並】メッセージスキップ`, `文章スキップ`) gets that label and goes first. The rest follow in the files' order and show only the key. Six of the ten games read get one.

When a game waits for typed text, the hub opens its keyboard on a touch device, and a hardware keyboard types directly.
Return finishes and Backspace deletes. A controller's Confirm finishes and Cancel cancels. Touching the game brings back a keyboard the player hid.

### The hero's image

Game.dat names a starting hero image, but most of the library's games leave it empty and set it with the party graphics command: erase everyone, then insert the image from a string variable (bit 0x01 of the command's second flag byte). An erased party has no members, so that insert makes the hero whatever index it gives. A wrong flag check hid the hero after a new game in those games, up to and including 0.20.0. A save made in VNDegen in that state keeps the empty image until the game runs the command again.

### ANGEL QUEST's mouse

ANGEL QUEST walks the hero toward the pointer by pressing arrow keys with command 125, and blocks those keys only for 5 frames after an event ends. It's an ordinary user of command 126, not evidence about 125 getting past it. Run from the same PC save, the hero steps left and right of the throne room carpet with the pointer on both the PC engine under Wine and VNDegen.

### Commands 125 and 126

Command 126 can stop basic keys, and command 125 presses keys for the next frame. The manual's key input page says a basic
key stopped by 126 can't be pressed by 125 either. VNDegen lets 125 through regardless, and only what the player presses
obeys 126. Two games need this:

- YUME-SAME turns confirm and sub off every frame (`126 517`) and presses confirm with 125 for each click. No click would
  do anything otherwise. YUME-SAME has no PC comparison: its `Game.exe` stops with a buffer overrun under Wine, in every
  locale tried.
- Parade Buster turns cancel off when an attack starts (`126 610`) and, if the click was on its MENU button, presses cancel
  with 125. Under Wine the PC engine (Parade Buster's own `Game.exe`) opens the menu, which settles the question, and
  VNDegen does the same. A cancel key the player presses during that attack stays blocked in VNDegen.

## Saves

Saves are the PC engine's own files in the `Save` folder beside `Game.exe` (`SaveDataNN.sav`, and `System.sav` for
games that keep values across saves), so they move between VNDegen and a PC copy of the game, and Ludusavi backs them
up. Saves the PC engine wrote load in the runtime, and saves the runtime wrote load in the PC engine, checked under
Wine for a Wolf RPG Editor 2 and a Wolf RPG Editor 3 test game, and for The Clumsy Witch Seris (Wolf RPG Editor 3.5,
save version 159 in its compressed and encrypted container) and ANGEL QUEST (Wolf RPG Editor 2, version 123), and for Two Beasts Or Not To Beast! (version 142). False Myth 2's Wolf RPG Editor 3.343 saves (version 145) read and write too, checked against its engine under Wine, and a save made from its own menu in VNDegen loads at the same place after a restart. Two Beasts saves from its phone, and a load differs there: the PC engine shows the phone again from a VNDegen save and a black map with its HUD from its own save, where a right click opens the phone, and VNDegen shows the room from either. Fields
the runtime doesn't use are carried over from the last save read or written. `docs/wolfrpg-save-format.md` describes the
format and how each field was established, and `docs/wolfrpg-cleanroom.md` the version 159 container and additions. A
Wolf RPG Editor 3.5 game with no save of its own gets version 159 saves, as Wolf RPG Editor 3.714 writes, and a game
whose Game.dat has a 22-byte settings block gets version 128, as Wolf RPG Editor 2.21 and 2.23 write. Such a game's
version 125 saves, which VNDegen 0.58.1 wrote, load as before, and the next save made from them is version 128.

In version 128 and 159 saves the pictures on screen go into the save, and a load shows them again, as those engines do: President
Yukino's day bar and portrait are there right after a load, from a PC save or VNDegen's, and VNDegen's frame then
matches the PC engine's. The PC engines load VNDegen's version 128 saves with their pictures
(President Yukino and Drain Dungeon under Wine). Images, text, windows, `<SQUARE>` and `<GRADX>` pictures keep their
position, anchor, pattern, opacity, zoom, angle, colour and blend. The Clumsy Witch Seris (159) keeps its HUD this way: a
save moved between VNDegen and the PC in either direction shows the HUD, minimap and portrait right after the load, as
the PC's own save does in its own engine. Text pictures keep `\f`, `\E`, `<R>` and `\ay` codes.

A picture's angle is in tenths of a degree, clockwise about the picture's centre: Drain Dungeon's engine under Wine draws
an angle of 300 as 30 degrees, with the corners where `Engines/WolfRPG/host/picture_angle_test.cpp` expects them. Up to
0.58.1 the runtime read it as whole degrees. Parade Buster's own scripts agree: its spinning `tri.png` steps by 400 and wraps at
3600, a full turn. Windows and shapes don't turn. Pictures numbered below 0 stay out of saves, as the PC
engine leaves them out. Saves of other versions carry no pictures and a load clears them, as their engines do or as is
safe: ANGEL QUEST's engine (version 123) closes when it loads a save holding a picture record.

## False Myth 2

False Myth 2 (Wolf RPG Editor 3.343) is an action-battle RPG made of 42 crypt version 0x14B archives. It plays in
VNDegen from the joypad question through the prologue, its first battle and tutorial panels, the title, the config
book, the visual novel scenes at the Main Gate and the Sanctuary, the first dungeon (the Moon-Deceiving Forest: its three
floors, eight battles with their results, the jump and the two-member battle tutorials and the scenes between them) to
the story after it and the South Tower, where the menu's Battle Prep, Training, Skills and Members pages open; a save
from its menu loads at the same place after a restart, and the PC under Wine loads it there too. On the Linux host it
plays on from there by the in-game map's objectives: to the North Tower through the East Tower and its corridors, the
scene on the North Tower's second floor, back to the South Tower and the "Unwanted Deathmatch" event on its bridge,
whose battle (a magic tutorial against a 30,000 HP enemy) runs with its turns, enemy actions, magic selection and
retries; equipping a sword in Battle Prep works. That battle wasn't won with scripted input, so the story after it
hasn't been played. Each part was compared
with the game under Wine. Getting there needed, beyond the archives:

- Conditions run their else case whenever the branch has one, also without the else flag; its tutorial body text sits
  in such a case.
- An event waiting for a move route waits for that route only. Its after-battle event waits on a hero route while a
  parallel event gives the hero a new route every frame; waiting for any route kept system variable 13 at 1 and the
  menu closed.
- System variables 100 to 102 (BGM, BGS and SE volume in percent) start at 100, as in the PC's empty saves, and set
  the runtime's volume for each kind of sound. Its config book shows and changes them.
- System variable 3 places the choice window's top centre, as the manual's system variable page says. When a game sets
  system variables 5 and 6, the pause icon's top centre goes to x = Sys5 and y = the text's top + Sys6, where the PC
  draws it in False Myth 2 (the manual calls both relative to Sys1 and Sys2, which doesn't match the PC's x here).
- A character image whose name ends in `T.png` has a standing frame before its walk frames in each block, and one
  ending in `TX.png` a standing frame per walk frame, as the manual's material page says. Its heroine's image (5
  patterns, 8 directions, `T.png`) has 12 columns.
- The party command's specials 0 (close the gaps), 5 (make the party transparent) and 6 (show it again) run. A new
  game puts the heroine in slot 1 and closes the gap. Transparency goes into saves as the hero's hidden flag; PC saves
  made where the hero shows have it 0, and none made while the party was transparent has been read.
- A move route made only of steps that take no time, such as setting the speed, doesn't stop the player walking. A
  parallel event gives the hero such a route every frame.
- Pictures at opacity 0 aren't decoded. Its battles keep a dozen hidden sprite sheets of up to 5760 by 3240 pixels,
  more than the image cache holds, and decoding them every frame took over 500 ms a frame.

- The variable command's "other" value 24 is the game's language, from Game.dat's settings byte 17 (5 is English and
  reads 2, as the manual lists English). Changing that byte in the PC game's own Game.dat switched its dungeon book
  between its English and Japanese layouts, and no other value from 0 to 6 gave the English one.
- Map position chip numbers (4 to 7) count autotiles from 1 and base tiles from 33, as the manual's "32 and up" says,
  and passability there (2 and 3) is 1 where the way is blocked, as the dungeon's warning tile shows on the PC. Its
  dungeon floors are pictures built from these.
- A character's value 20 is its chip plus one for an event drawn as a map chip and -1 for an erased one, and any
  character value of a character that doesn't exist reads -1. Its dungeons pick their objects by the chip and end a
  loop over event IDs on the first missing one; the loop ran forever and the hero walked one step only.
- The move command reads its character from a variable when it names one. Its dungeons move a barrier away once it
  opens.
- The choice window measures as on the PC: 10 and 11 pixels from its edge to the cursor, the text 6 pixels into the
  cursor, rows the base size plus Game.dat's choice spacing (settings byte 12) apart.

Its battle events read keys every other frame, so the Linux host's scripted presses there need to last two frames.
`Engines/WolfRPG/host/tools/wolfdrive` keeps a game running and takes commands on its standard input (run, tap, hold,
shot, save, load, pics, sys, state), which makes long play-throughs against the PC practical.

## Known limits

- The deeper look at detection lists each third-level folder once to find `Game.exe`, whatever else that folder holds.
  A short listing with no Wolf game that reaches this plugin pays for that at each library refresh. It's bounded at 200
  listings, and the time hasn't been measured. The largest case in the library's listing is Livestream: Escape from Hotel
  Izanami, an unrecognized game, with 25 entries in the listing and 5,178 names one level down.
- A load restores variables, the map and the characters, and pictures from version 128 saves, but not the screen tint
  or which music was playing; the map's own music starts. Saved pictures come back without their effects (shake, pulse,
  flicker, automatic patterns, the effect command's zoom and shift), and pictures made from XY arrays, cut with
  `<CUT/...>`, or holding text codes other than `\f`, `\E`, `<R>` and `\ay` are left out of saves and loads. Version 159
  saves leave out window pictures too. No library game meets this at a save point: in 40,000 frames of each game
  that saves pictures (President Yukino, Drain Dungeon, Journey of Depravity, The Clumsy Witch Seris), the only
  effects are the message pause icon's and Seris's title backdrop's automatic patterns, and the only other codes
  (`\c`, `\ax`) are on Seris's XY minimap and labels, which its menu erases before it saves.
- The camera goes into version 128 saves as the PC writes it, so the PC shows a VNDegen save scrolled as it shows its
  own (President Yukino and Journey of Depravity under Wine). VNDegen keeps the hero in the middle after any load.
  The Clumsy Witch Seris's own engine writes 0 there and opens a load at the map's top left, its own saves included,
  where VNDegen centres the hero; other versions' camera isn't written.
- DxLib archive version 7 isn't read. No library game uses it: ANGEL QUEST, once thought to, is a version 3 archive
  with the oldest Wolf RPG Editor key, and plays.
- Wolf RPG Editor Pro's encrypted data (basic data files whose second byte is `P`) isn't read. No library game needs
  it: the two library games with `GamePro.exe`, The Clumsy Witch Seris and Two Beasts Or Not To Beast!, both ship their
  data unencrypted (checked in every basic data file, loose and inside Two Beasts' version 8 archives).
- Save versions 143, 144 and 146 to 158 aren't read: no save of those versions has been seen. Version 145, which Wolf
  RPG Editor 3.343 writes, is (`docs/wolfrpg-save-format.md`). Saves under 200 bytes in the 3.5 container use another
  scheme that isn't read; real saves are never that small.
- Of the crypt versions that encrypt a version 8 header, only 0x14B (Wolf RPG Editor 3.31 to at least 3.343) is read.
  UberWolf changes several steps for 0x15E and later, and no library game uses them. Such a game stops at launch with
  "Cannot read BasicData.wolf: its header is encrypted with a Wolf RPG Editor crypt version later than 3.31's, which
  VNDegen doesn't read yet." A version 8 archive that fails to open for another reason names that reason in the same
  way.
- File downloads (command 260) aren't supported. Two Beasts' update check and the standard "sodeba" tools use them, and nothing needs the result.
- Input a player doesn't have on VNDegen: a game's own gamepad button queries (key input kind 2) read nothing, since VNDegen delivers controllers as keyboard keys, and system variable 38 isn't read, so no key or click reaches an event while the player types. Multiple touch and analog stick queries aren't used by a library game.
- YUME-SAME turns off the keys a touch gamepad or controller presses, so it plays by touch or pointer only: touches outside the gamepad's buttons work as the mouse under every control method that lets them through. A right click needs the Touch mouse method's two-finger tap or a controller's pointer layout. Two Beasts and Parade Buster also take a controller's Cancel for it.
- Hidden string codes run only for what library games use in play: `GET_FILE_EXIST`, `GET_STRING_WIDTH_COUNT`, `GET_STRING_BYTE_COUNT`, `GET_TITLE`, `GET_TITLE_PLUS` (empty), `GET_COMMONEVENT_ID_FROM_NAME`, `GET_MAPEVENT_NAME_FROM_ID`, `GET_MAPEV_FILENAME`, `GET_FILENAME_FROM_PICTURE`, `MAKE_EVENT`, `SET_EVENT_NAME_ID` and `FIND_PATH_DIR4`. Others log once and give empty text; in The Clumsy Witch Seris they are in its debug events. Events `MAKE_EVENT` adds aren't written into saves.
- Writing an event's position through a variable address moves it at once, where the PC walks it. Command 240 (tile passability settings) isn't run: The Clumsy Witch Seris has two uses, neither in the parts played. Move route step 48 (half steps) isn't run in whole-tile games.
- CSV and text files are written as UTF-8 with a byte order mark, as Wolf RPG Editor 3 games write them. The one Shift-JIS game that writes a CSV (YUME-SAME) does so only in a debug event.
- The string command's typed text draws at system variables 40 and 41 at the base font size. No PC comparison: the one library game we typed in on Linux, MARAGON QUEST, doesn't start on the PC under Wine, because its map file names came out of its archive mangled.
- Cancelling typed text leaves the variable as it was, and system variable 39 reads -1. Not compared with the PC.
- Command 126 turning off one keyboard key stops event commands reading that key, but its basic key still works when it maps to one. The runtime gets basic keys from the host without knowing which key caused them. Library games turn off Control, Print Screen, S and the keyboard as a whole, none of which this touches.
- Saves of versions before 159 have no arrays. A Wolf RPG Editor 3.5 game that started from an older save loses its arrays when saved in VNDegen. The Clumsy Witch Seris, the one library game with arrays, writes version 159.
- Command 222 can't write key settings (system variables 52 to 57) into a save, which The Clumsy Witch Seris's configuration tries: they aren't kept between sessions.
- A message's text goes to a place of VNDegen's own, near the bottom left, when a game leaves system variables 1 and 2
  at -1, since the PC's default place isn't known. A new game starts them at -1. So does the pause icon, unless system
  variables 5 and 6 are set. Drain Dungeon, ANGEL QUEST, Milky Quest and Succubus Prison all set 1 and 2. A game that never
  sets them, loaded from a save in which they are 0, such as one an earlier VNDegen made, shows its text at the top left,
  since a save can't tell such a 0 from one the game set.
- Choice window positions follow Wolf RPG Editor 2's defaults unless system variables 3 and 4 give the window's top
  centre. A value of -1 means the default, and any other value counts, so Succubus Prison's -1000 hides the choices
  behind its picture menu. Compared with the PC in False Myth 2 (3 and 4 set); Succubus Prison's `Game.exe` stops with a
  buffer overrun under Wine, and Parade Buster's crashes on its data. The window's padding (system variables 26 and 27
  in Wolf RPG Editor 3) isn't read: False Myth 2 leaves them at 0, and its window measures as the PC's.
- Picture commands with a start delay run at once, and the delay reset command does nothing. The standard systems
  repeat their slide-in moves with delays of 3 and 5 frames toward the same place, so the result looks the same.
- Free-form pictures draw without their angle, and a zoom correction scales them about their centre.
- Not drawn: `<MAP_SCREENSHOT>` pictures, video pictures, and the text codes `\f+`, `\fR`, `\cS`, `\cR`, `\bx` and
  `\se`. `\A+` and `\A-` are read but text is always antialiased.
- Of the effect command's second set (its 0x100 flag) only the blur runs; the others, and effects on target kind 3,
  log once. Its type 0x60 is drawn as a flicker; False Myth 2's uses, with three colour-like values, suggest the
  manual's automatic flash instead, which hasn't been compared with the PC. The picture effects added here (blur,
  corrections, second shift) aren't kept in saves, like the others.
- Pictures numbered 100,000 and above draw below the message window, not above it. Only Test Game 02's debug tool
  uses such numbers.
- Game.dat's picture behaviour setting (Ver1 or Ver2) isn't read: showing a picture that is already shown starts it
  afresh, and `\space` counts whole pixels in every game. Its opacity, zoom, angle, colour, pattern and blend mode
  stay as they were where the command says to keep them: Milky Quest changes the witch's expression by showing her face
  picture again with its opacity kept, and up to 0.41.6 the face vanished and the map showed through her bust (#179).
- System variable 115, the engine version the standard systems read, is set to 2.24 for Shift-JIS games and 3.28 for
  UTF-8 ones, since no Game.dat field is known to hold the real one.
- False Myth 2: the story past the "Unwanted Deathmatch" battle at the South Tower hasn't been played in VNDegen, and no
  shop has been reached. The party command's other specials (warping the party to the hero, sync, remembering graphics, the
  caterpillar switch) aren't run. No point was reached where the party is transparent and the menu can save, so a save
  with the hero hidden hasn't been compared with the PC.
- Whole-tile games keep the old rules: a tile blocks only when all four of its passability bits are set, events placed
  half a tile up or left stand on their tile, and touch ranges aren't read. The manual's half-tile overlap rules for
  them (its specification page's 0.5-tile wider touch) haven't been compared with the PC. In half-tile games a whole
  step from a half position, which the PC refuses (a False Myth 2 save edited to whole steps at 47,45 can turn but not
  walk), isn't refused, and a character's box is checked only where its step ends.
- False Myth 2's dungeons and battles still hitch for a moment every few seconds in the Mac app on the build Mac. A
  step that takes 100 to 300 ms there uses only 8 to 13 ms of the main thread's CPU time, as a normal step does, so the
  thread is waiting rather than working: the mini ran at load 15 to 21 from other threads' compilers during every
  measurement, with the app in the background behind a locked screen (`docs/frame-rate.md`). A Mac at rest wasn't
  available to measure.
- Game.dat's language byte is read only as English (5) or Japanese; the other languages' values aren't known.
- Of the effect command's map effects only the zoom is drawn; the library uses no other.
