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
| Linux test host | `Engines/WolfRPG/host/` | Builds the same sources on Linux with tools that run games headless |

The runtime draws each frame on the CPU at the game's resolution (320 x 240 to 960 x 720 in the test games). The
session steps it once per display refresh on the main thread, and the runtime advances the game at its own frame rate
from Game.dat (60, or 30). Sound is mixed at 44.1 kHz and pulled by AVAudioEngine on its audio thread; a decoder thread
of the runtime's own keeps music decoded ahead. Everything runs in the app's process, with no JIT.

## Detection

A game is Wolf RPG Editor when a folder at the top of the game folder, or up to two folders down, holds `Data.wolf`,
`Data/BasicData`, or `Data/BasicData.wolf`, beside `Game.exe` (or `GamePro.exe`) for certainty. A game deeper than the
folder listing reaches is found by `Game.exe` beside `Data` and `GuruguruSMF4.dll`, the MIDI player every Wolf RPG
Editor 2 game ships.

## What the runtime reads

- Game data: `Game.dat`, `CommonEvent.dat`, the user, changeable and system databases with their `.project` files,
  `TileSetData.dat`, and the maps (`.mps`). `Game.dat`, `CommonEvent.dat` and `TileSetData.dat` may be encrypted with
  Wolf RPG Editor 2's basic data encryption. Wolf RPG Editor 3.5 compresses common events, databases and maps with
  LZ4, and its maps have their own number of tile layers. Text is Shift-JIS in Wolf RPG Editor 2 and UTF-8 in Wolf RPG Editor 3
  games that say so. Translated games often hold bytes that aren't valid Shift-JIS; they become U+FFFD.
- Archives: `Data.wolf`, or one `.wolf` archive per top-level data folder, as DxLib archives of versions 1 to 6 with
  the encryption keys of Wolf RPG Editor 2, and of version 8 with the key strings of Wolf RPG Editor 2.225 and 3.x,
  chosen by the crypt version Wolf RPG Editor 3 writes into the header. Loose files win over archives.
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

Common events and strings can read a text file. As on the PC, the runtime takes UTF-8 with or without a byte order mark,
and otherwise Shift-JIS: The Clumsy Witch Seris is a UTF-8 game that keeps its scene text in Shift-JIS. Reading a string
variable as a number gives the number its text starts with, which Two Beasts counts on for its script labels. A choice
right after a message opens with that message still showing.

XY arrays draw as pictures through `<XY_IMAGE/...>` and `<XYN_IMAGE/...>`, The Clumsy Witch Seris's minimap, and saves of
version 159 keep them. `<CUT/Xa-b/Yc-d>` before a file name shows part of each pattern.

Map event pages hold their animation speed, move speed, move frequency and move type in that order. Characters draw from images of 3 or 5 patterns by 4 rows, or with eight-direction images (Game.dat's settings byte 1 is 8) two such blocks side by side, the diagonals in the second, as the manual's material page shows.

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
  `\cE[n]` and `\m[n]` change line spacing, letter spacing, outlines and line height.
- Wolf draws 320 by 240 games at twice the size, so in those games `\ax`, `\ay`, `\mx`, `\my` and `\-` count in half
  pixels, as the standard systems' own layout assumes.
- Pictures place by the anchors the games use (top left, centre, bottom left, top right, bottom right, top centre),
  move relative to where they are, keep their anchor, size and blend mode on a move, keep their
  zoom or colour when the command says so, and take the effect command's draw position shift and colour pulse.

## Controls

Wolf RPG Editor reads gamepads, so the default layout is the gamepad: A confirms (Z), B and Menu cancel (X, which
opens the menu in most games), the Dash button is the sub key (Shift), and the D-pad and left stick move. A keyboard
works as on Windows: Z, Return or Space confirm, X or Escape cancel, Shift is sub, and the arrows or the number pad move.

Event commands read the keyboard by Wolf's own key codes, 100 plus the key's DirectInput scan code. The mouse works through
the view: a pointer, a touch under the Touch and Touch mouse control methods, or a controller's pointer actions move it, and
scrolling turns its wheel. A game reads it through key input, the variable command's "other" values, system variables 71
and 72, and whether it is over a picture. System variable 73 lets it work messages and choices.

When a game waits for typed text, the hub opens its keyboard on a touch device, and a hardware keyboard types directly.
Return finishes and Backspace deletes. A controller's Confirm finishes and Cancel cancels. Touching the game brings back a keyboard the player hid.

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
save version 159 in its compressed and encrypted container) and ANGEL QUEST (Wolf RPG Editor 2, version 123). Fields
the runtime doesn't use are carried over from the last save read or written. `docs/wolfrpg-save-format.md` describes the
format and how each field was established, and `docs/wolfrpg-cleanroom.md` the version 159 container and additions. A
Wolf RPG Editor 3.5 game with no save of its own gets version 159 saves, as Wolf RPG Editor 3.714 writes.

## Known limits

- A load restores variables, the map and the characters, but not pictures, the screen tint, or which music was
  playing. Wolf RPG Editor 2 and 3.2x show no saved pictures after a load either; the map's own music starts.
- After a save moves between VNDegen and the PC in either direction, The Clumsy Witch Seris shows its HUD, minimap and
  building labels again only once the menu has been opened and closed. Wolf RPG Editor 3.5 restores pictures from a
  save, and VNDegen neither writes pictures into saves nor restores them. Saves made and loaded within VNDegen behave
  as before.
- DxLib archive version 7 isn't read. No library game uses it: ANGEL QUEST, once thought to, is a version 3 archive
  with the oldest Wolf RPG Editor key, and plays.
- Wolf RPG Editor Pro's encrypted data (basic data files whose second byte is `P`) isn't read. No library game needs
  it: the two library games with `GamePro.exe`, The Clumsy Witch Seris and Two Beasts Or Not To Beast!, both ship their
  data unencrypted (checked in every basic data file, loose and inside Two Beasts' version 8 archives).
- Save versions 143 to 158 (Wolf RPG Editor 3.3x) aren't read: no save of those versions has been seen. Saves under 200
  bytes in the 3.5 container use another scheme that isn't read; real saves are never that small.
- File downloads (command 260) aren't supported. Two Beasts' update check and the standard "sodeba" tools use them, and nothing needs the result.
- Input a player doesn't have on VNDegen: a game's own gamepad button queries (key input kind 2) read nothing, since VNDegen delivers controllers as keyboard keys, and system variable 38 isn't read, so no key or click reaches an event while the player types. Multiple touch and analog stick queries aren't used by a library game.
- YUME-SAME turns off the keys a touch gamepad or controller presses, so it plays by touch or pointer only: touches outside the gamepad's buttons work as the mouse under every control method that lets them through. A right click needs the Touch mouse method's two-finger tap or a controller's pointer layout. Two Beasts and Parade Buster also take a controller's Cancel for it.
- Hidden string codes run only for what library games use in play: `GET_FILE_EXIST`, `GET_STRING_WIDTH_COUNT`, `GET_STRING_BYTE_COUNT`, `GET_TITLE`, `GET_TITLE_PLUS` (empty), `GET_COMMONEVENT_ID_FROM_NAME`, `GET_MAPEVENT_NAME_FROM_ID`, `GET_MAPEV_FILENAME`, `GET_FILENAME_FROM_PICTURE`, `MAKE_EVENT`, `SET_EVENT_NAME_ID` and `FIND_PATH_DIR4`. Others log once and give empty text; in The Clumsy Witch Seris they are in its debug events. Events `MAKE_EVENT` adds aren't written into saves.
- Writing an event's position through a variable address moves it at once, where the PC walks it. Command 240 (tile passability settings) isn't run: The Clumsy Witch Seris has two uses, neither in the parts played. Move route step 48 isn't run.
- CSV and text files are written as UTF-8 with a byte order mark, as Wolf RPG Editor 3 games write them. The one Shift-JIS game that writes a CSV (YUME-SAME) does so only in a debug event.
- The string command's typed text draws at system variables 40 and 41 at the base font size. No PC comparison: the one library game we typed in on Linux, MARAGON QUEST, doesn't start on the PC under Wine, because its map file names came out of its archive mangled.
- Cancelling typed text leaves the variable as it was, and system variable 39 reads -1. Not compared with the PC.
- Command 126 turning off one keyboard key stops event commands reading that key, but its basic key still works when it maps to one. The runtime gets basic keys from the host without knowing which key caused them. Library games turn off Control, Print Screen, S and the keyboard as a whole, none of which this touches.
- Saves of versions before 159 have no arrays. A Wolf RPG Editor 3.5 game that started from an older save loses its arrays when saved in VNDegen. The Clumsy Witch Seris, the one library game with arrays, writes version 159.
- Command 222 can't write key settings (system variables 52 to 57) into a save, which The Clumsy Witch Seris's configuration tries: they aren't kept between sessions.
- Message and choice window positions follow Wolf RPG Editor 2's defaults; games that move them through system
  variables other than the choice window's top may place them differently.
- Picture commands with a start delay run at once, and the delay reset command does nothing. The standard systems
  repeat their slide-in moves with delays of 3 and 5 frames toward the same place, so the result looks the same.
- Free-form pictures (the four-corner flag) draw as plain rectangles. Only Test Game 02's developer debug tool uses
  them.
- Not drawn, as no test game uses them: `<SCREENSHOT>` and `<MAP_SCREENSHOT>` pictures, video pictures, and the text codes `\f+`, `\fR`, `\wE`, `\cS`,
  `\cR`, `\img`, `\isize`, `\bx` and `\se`. `\A+` and `\A-` are read but text is always antialiased.
- Pictures numbered 100,000 and above draw below the message window, not above it. Only Test Game 02's debug tool
  uses such numbers.
- Game.dat's picture behaviour setting (Ver1 or Ver2) isn't read: showing a picture that is already shown starts it
  afresh, and `\space` counts whole pixels in every game.
- System variable 115, the engine version the standard systems read, is set to 2.24 for Shift-JIS games and 3.28 for
  UTF-8 ones, since no Game.dat field is known to hold the real one.
- The picture flash effect tints a picture toward its colour and leaves it there instead of returning to normal.
- Of the effect command's map effects only the zoom is drawn; the library uses no other.
