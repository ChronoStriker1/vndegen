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
Battles and menus are common events in each game's own data, so they need nothing of their own.

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
- Two Beasts Or Not To Beast! reads its version 8 archives, starts, and stops at its first-start notice, which waits
  for a mouse click on its OK button. Mouse input and CSV import (command 251), which it uses next, are not supported.
- Save versions 143 to 158 (Wolf RPG Editor 3.3x) aren't read: no save of those versions has been seen. Saves under 200
  bytes in the 3.5 container use another scheme that isn't read; real saves are never that small.
- Wolf RPG Editor 3 commands not supported yet, which The Clumsy Witch Seris uses: arrays (command 255, its minimap and
  building labels, which sit on the wrong buildings), the regular expression string operation (its voice lines pick
  their folder with it, so system voices are silent), and `<XYN_IMAGE>` pictures.
- Wolf RPG Editor Pro's encrypted data isn't read. One library game is a Pro game.
- Mouse input, CSV import (command 251) and file downloads (260) aren't supported. The test games don't use them; two
  library games need the mouse, one to pick its control scheme on its first screen and one for its action battles.
- Typing text on the keyboard (a string command mode) isn't supported: the command keeps the text already in the
  variable after a frame. Games that offer a default, such as a name, go on with it.
- Message and choice window positions follow Wolf RPG Editor 2's defaults; games that move them through system
  variables other than the choice window's top may place them differently.
- Picture commands with a start delay run at once, and the delay reset command does nothing. The standard systems
  repeat their slide-in moves with delays of 3 and 5 frames toward the same place, so the result looks the same.
- Free-form pictures (the four-corner flag) draw as plain rectangles. Only Test Game 02's developer debug tool uses
  them.
- Not drawn, as no test game uses them: the `<CIRCLE>` shape, `<SCREENSHOT>`, `<MAP_SCREENSHOT>`, `<CUT/...>` and
  `<XY_IMAGE/...>` pictures, video pictures, the centre bottom anchor, and the text codes `\f+`, `\fR`, `\wE`, `\cS`,
  `\cR`, `\img`, `\isize`, `\bx` and `\se`. `\A+` and `\A-` are read but text is always antialiased.
- Pictures numbered 100,000 and above draw below the message window, not above it. Only Test Game 02's debug tool
  uses such numbers.
- Game.dat's picture behaviour setting (Ver1 or Ver2) isn't read: showing a picture that is already shown starts it
  afresh, and `\space` counts whole pixels in every game.
- System variable 115, the engine version the standard systems read, is set to 2.24 for Shift-JIS games and 3.28 for
  UTF-8 ones, since no Game.dat field is known to hold the real one.
- The picture flash effect tints a picture toward its colour and leaves it there instead of returning to normal.
- Of the effect command's map effects only the zoom is drawn; the library uses no other.
