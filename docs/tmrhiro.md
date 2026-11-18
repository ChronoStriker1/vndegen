# Tmr-Hiro ADV System

VNDegen runs Tmr-Hiro ADV System games on a runtime of its own, `Packages/VNDegenKit/Sources/CTmrHiro`, written in C
from the games' files, GARbro's MIT-licensed Tmr-Hiro readers and the original engine's behaviour under Wine. It is MIT
licensed like the rest of the app. `docs/tmrhiro-cleanroom.md` lists where each part came from. No other runtime for
the engine exists, open or otherwise. The exe calls itself "-ADV GAME ENGINE SYSTEM-"; Lune, Marigold and BEe-light
games use it.

## Test games

| Game | Engine build | Archives | Notes |
|---|---|---|---|
| Welcome to the Mizuryuu kei land~ A Naughty theme park with your family! | 2017-06 | `*.pac`, GRD pictures packed with Huffman and LZ77 | English in `srp_2.pac`. Plays through both routes |
| Kyonyuu Elf Oyako Saimin | 2018-11 | the same | English in `srp_2.pac`. Plays through every route to its ending |
| Kyonyuu Hitozuma Onna Kyoushi Saimin | 2014-12 | GRD pictures packed with RLE | English in `srp.pac`. Plays through the route select and days to an ending |
| Spooky Pregnant School: The Quickening | 2018-11 | the same as Elf, plus `movie/*.mpg` | English in `srp_3.pac`. MPEG-1 movies with MP3 sound. Plays through the prologue, the route maps and the routes |
| Spooky Pregnant School: The Conception | 2016-03 | GRD pictures packed with RLE, in a `DATA` folder | English in `srp_2.pac`. Not yet checked |

All five are 1280x720.

## How the games are built

A game folder holds the exe, `Agrd.pac` to `Hgrd.pac` and `grd.pac` (pictures), `srp.pac` (scripts and settings),
`mus.pac` (music), `se.pac` and `sse.pac` (sound effects), `vic.pac` (voices) and `save/`. A `.pac` is an entry count,
a name length, the data start, and per entry a fixed-size name, a 64-bit offset and a 32-bit size; nothing is
encrypted. An archive named `X_2.pac` to `X_10.pac` overrides `X.pac` entry by entry, which is how the English patches
add their scripts. The plugin takes a folder as a Tmr-Hiro game when it has `srp.pac` and `Agrd.pac` side by side, up to
two folders down.

- Pictures (GRD): a 32-byte header with the canvas size, bit depth, the picture's rectangle on the canvas (counted from
  the bottom) and the packed size of each plane, then the alpha, red, green and blue planes, bottom-up. Planes are RLE,
  Huffman then RLE, or Huffman then LZ77. Character sprites are 900 by 2048 canvases; a face is a second GRD drawn over
  the body at its own rectangle, as `_charafilemacro` lists.
- Music is raw 44.1 kHz 16-bit stereo PCM behind a 9-byte header; sound effects and voices are Ogg Vorbis.
- Scripts: each scene is a list of records, each a size, a 16-bit class and a 16-bit opcode, and a body whose bytes have
  their nibbles swapped when the scene starts with its scene number record; scenes an English patch adds can be stored
  plain. Class 0x00 is text (`Name,Text,Voice`), 0x10 flow (labels, jumps, calls, conditions, title
  buttons, choices, the map), 0x20 pictures, 0x30 sound, 0x40 interface and the gallery, 0x50 scene state and
  variables. `start` is the first scene. `＄α` to `＄ε` in text stand for the names `PLAYER_NAME01` to `05` in `_system`.
- A condition (0x0004) or assignment (0x5001) record holds an operator, the variable's kind and a one-byte index, and a
  value. When the byte before the index is non-zero it names a variable kind, and the value is that kind's index, as in
  `var2[0x6f] = var2[0x74]` or `var2[7] < var2[8]`.
- `srp.pac` also holds `_system` (screen size, the message window, and the position and source rectangle of every part of
  the built-in screens), `_filemacro` and `_charafilemacro` (picture names made of a base and parts), `_var` (the CG and
  scene lists of the gallery) and `_charaview`.

Spooky Pregnant School: The Conception adds systems the other games don't use:

- A clock picture over the story (0x01f4 names it, 0x01f5 shows or hides it) and a stamina gauge on the map (0x01ff
  turns it on, the map's kind 0x30 element places it and fills its bar from `glo040`, var2[40]).
- Two buttons over the text, "use items" and the status orb, from `sys_window_button` (0x50ca and 0x50cc define them,
  0x50cb and 0x50cd show them). They call `miniscenario2` and `miniscenario3`, which return to the same line.
- Map buttons it shows or hides through var3: in a map whose header (0x002b) starts with 1, the k-th element record
  after the header (0x0033 or 0x0034) is hidden while var3[k] is 0. Clearing the buttons (0x0028) sets var3 to 1, and
  the maps then hide the characters already visited. var3[0] to var3[99] are the 100 flag bytes of a save slot.
- Item screens built as maps: a 0x0033 record of form 0x10/0x10 is slot N's position, and one of form 0x21 is an item
  button with a hover card and sounds, shown in the slot that var3 at its index holds. The scripts fill var3 from the
  item slots var2[0x6f] to var2[0x73] and the count in var2[0x6e]. A 0x0034 record of kind 0x10/0x30 names the
  right-click label.
- Screen captures used as backgrounds: 0x0385 N captures the scene (without the message window) as `capdataN`, and
  choosing a map button captures the map, without the hover card, as `capdata001`.
- A roulette (0x005e) set up by `_roulette_select`: a click starts the needle, a later click slows it to a stop, and
  its angle in percent of a turn goes to var2[0], which the script turns into a stamina cost.
- Picture layers (0x006e) stack in the order they are first set, their x counts from a canvas centred on the screen,
  and a background record clears them.

The engine draws its own title buttons, message window, button bars, choices, save and load pages, backlog, system
screen, confirm dialogs and gallery from parts in `Agrd.pac`, placed by `_system`. VNDegen draws them the same way, so
they look as on the PC.

## Saves

The PC engine keeps everything in the game folder's `save/`: `savedata.NNN` slots of 43,121 bytes with a
`savedataNNN.bmp` thumbnail, and `savedata.000` for settings, read marks and gallery flags. VNDegen reads and writes
the same files there:

- Slots 001 to 100 are the ten save pages, 101 to 110 the auto saves the engine writes on entering a map or a choice,
  and 999 the quick save. The 2016 to 2018 engine builds write 43,121-byte slots; the 2014 build (Hitozuma), whose
  settings have no `[INHERITSAVE]` section, writes the same layout cut off at 34,767 bytes. A slot is a fixed layout: the time, the scene title, the script variables, the call frames
  (scene, record, scene number), the picture layers and their transforms, and the sound channels, around the engine's
  default object state. A slot from
  the PC engine under Wine loads in VNDegen, and a slot from VNDegen loads in the PC engine.
- Read marks (a bit per record in a block per scene) and the gallery's seen CGs and scenes are read from
  `savedata.000` and written back into it in place. When a game has never run on the PC and has no `savedata.000`,
  VNDegen keeps them in `save/vndegen-marks.bin` and merges them into `savedata.000` once the PC engine has written one.

## What runs

- The attention and logo screens, brand and title voices, the title pan and the title buttons.
- Welcome to the Mizuryuu kei land: the protagonist map, both routes to their endings and the staff roll.
- Kyonyuu Elf Oyako Saimin: the route maps, each character's route through the endings and the staff roll.
- Kyonyuu Hitozuma Onna Kyoushi Saimin: the route select, the days and an ending through the staff roll.
- Spooky Pregnant School: The Quickening: the opening choices, the digest of the first game, the prologue, the route
  maps and the routes, with the game's movies.
- Movies in the game's `movie` folder (MPEG-1 with MP3 or MP2 sound) through the shared vnvideo decoder. A click skips
  one, as on the PC.
- Picture layers over the scene with their move, scale and fade (record 0x006e with a picture that isn't a character),
  background pans and zooms (0x000c), and the sepia, grey and negative background effects (record 0x0001).
- Choices, sprites, event CGs, flashes, the ecstasy navigator, eyecatches, music, sound loops and voices.
- The message window with read text in its own colour, buttons inside it where a game places them there, the top and
  bottom button bars and the right-edge side menu, as each game's `TEXT_BUTTON_TYPE` places them (save, load, system, title, quit,
  quick save and load, backlog, auto, skip and hide), right click to hide the window, the wheel for the backlog.
- Save, load, auto save pages, the backlog with voice replay, the system screen's settings that change play (text and
  auto speed, the window's colour, opacity and base, the volumes, and each character's voice on or off and its
  volume), confirm dialogs.
- The EXTRA menu: CG gallery, scene replay, music room, and the 100% and 0% switches.
- Text uses the shared MS Gothic substitute (Ume Gothic), at the fixed cell widths MS Gothic has.
- Arrow keys and Enter work the title, choices and the map, so a controller does too.

## Known limits

- The character viewer in the EXTRA menu does nothing.
- The mouse wheel opens the backlog. The PC engine's default, rewinding the scene, isn't there.
- The system screen shows the screen mode and resolution settings; they do nothing, since the app sets the window.
- Transitions are crossfades. The PC engine's mosaic and wipe transitions aren't drawn.
- The map's slide animations are skipped.
- Name entry isn't shown. In Kyonyuu Elf Oyako Saimin the protagonist keeps the name `_system` gives him (Ingwill).
  Kyonyuu Hitozuma Onna Kyoushi Saimin's PC engine opens a Windows dialog with two name fields at the start; VNDegen
  goes on as if its 決定 button were pressed with the default names. The English text of both games is the same either
  way, since Hitozuma's patch writes the name (Wataru) into the lines.
- Kyonyuu Hitozuma Onna Kyoushi Saimin's English patch damaged 86 script records (dialogue, CG changes and sound
  starts written over as text). The PC engine skips them without showing anything, and so does VNDegen; the lines and
  pictures they held are missing on both.
- Spooky Pregnant School: The Quickening's black route calls a scene, `0526_K2_0040_000`, that none of its archives
  hold, and one voice line, `C04a11020002`, is stored with a trailing space in its name. The PC engine passes the call
  by and plays nothing for that line, and so does VNDegen.
- Spooky Pregnant School: The Conception names a sound effect, `SE_b01_0206`, that `se.pac` doesn't hold (it stops at
  0205), and one line of Shin's in `N0_0100_014` names its voice `00300001` without a character prefix. Neither is in
  the game's archives, so nothing plays there, on the PC either.
- The H voice loops that name three variants (record 0x0193) play the first one.
- Spooky Pregnant School: The Conception's item tutorial (`N1_0031_011`) repeats its line until stamina reaches 100,
  and the way on is to drink Dr. Eri's stamina drink from the "use items" button over the text, twice. The PC engine
  under Wine never responds to that button while text is shown, in the English or the Japanese script, so the tutorial
  loops there. VNDegen does what the script means: a click on the button over the text opens the item screen, as the
  same button does on the map.
- Spooky Pregnant School: The Quickening asks at the start whether to read the save data of the first game. The PC
  engine then opens a Windows folder picker; VNDegen doesn't read the other game's saves and goes
  on to the game's own question about starting without them.
- VNDegen doesn't create `savedata.000` itself. Text and auto speed, the window's opacity, the volumes and the
  characters' voice settings go into it when the PC engine has written one; otherwise they last until the game closes. The system screen's other settings last
  until the game closes either way.
