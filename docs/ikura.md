# Ikura GDL

VNDegen runs Ikura GDL games on a runtime of its own, `Packages/VNDegenKit/Sources/CIkura`, written in C from the
games' files, GARbro's MIT-licensed readers for the archives and pictures, and the original engine's behaviour under
Wine. It is MIT licensed like the rest of the app. `docs/ikura-cleanroom.md` lists where each part came from.

## Test games

| Game | Engine build | Notes |
|---|---|---|
| Lightning Warrior Raidy (G-Collections English release) | `RaidyUS.exe`, 2007-12 | Intro, dungeon floors with battles, menus, saves |
| Lightning Warrior Raidy II: Temple of Desire (G-Collections English release) | `Raidy_2_US.exe`, 2011-03 | Opening movie, town, shops, dungeon, animated scenes, backlog, gallery, saves |

Raidy III ships as a Setup Factory installer, which VNDegen can't unpack. Both games are 800x600.

Raidy's disc is a BIN/CUE image; the library's import reads it and keeps only the game's folder (`docs/library.md`).
Raidy II is a plain folder.

## How the games are built

A game folder holds the exe and extensionless archives: `SNR` (scripts), `GRP` (pictures), `SE`, `BGM`, `VOICE`
and `DATA` (dungeon maps). The plugin takes a folder as an Ikura game when `SNR` and `GRP` are side by side, up to two
folders down.

- An archive is `SM2MPX10`, an entry count, the data's start, and 20-byte entries (a 12-byte name, an offset and a
  size), stored as is. The engine reads `SNR9` to `SNR0` (and the same for the other kinds) before `SNR`, so patch
  archives override files. VNDegen does the same.
- Pictures (`GGP`) are PNG files XOR-ed with an 8-byte key stored in their header. Music and voices are Ogg Vorbis,
  effects 16-bit PCM WAV.
- Scripts (`.SNR`) are compiled: a header (label table size, code size, message count), a table of label offsets,
  then instructions of a 16-bit opcode, a 16-bit length and arguments. An argument with the top bit set names a
  variable; literals are 31-bit signed. Names in arguments are 12-byte fields that need not end in a NUL.
- A condition (`0407`) compares two values with one of six codes: 0 `==`, 1 `<`, 2 `<=`, 3 `>`, 4 `>=`, 5 `!=`.
- The engine runs `START.SNR` at boot, `LOAD.SNR` after a save is loaded and `CLOSE.SNR` when the window closes.
  Everything else, the title, the message window, menus, the battle system and the map, is the game's scripts.
- Pictures go into numbered slots; layers show a slot's region, fade (`020b`) and move (`020c`, `0211`); scripts
  draw on a layer's surface (`0103`, `0104`) or into another picture (`0105`, `0106`). Buttons sit on layers with four
  state pictures and draw only while the engine waits for input.
- Messages (`0908`) are a stream of tags: clear, name plate, voice, text, a variable's value, and wait for a click.
  Text is drawn in Times New Roman (the shared Liberation Serif substitute) with the engine's drop shadow and word
  wrap, 50 ms a character at the default speed.
- The dungeon (`08xx`) is an 81x81 grid from `DATA/*.ZMZ`: cells at odd coordinates, walls, doors and the entrance
  gate between them. The PC draws it with Direct3D; VNDegen raycasts the same view: a 90 degree field of view, the
  eye at the middle of walls three quarters of a cell high, floor, ceiling, wall and door textures, and a light at the
  eye. Turns and steps take 500 ms. The compass needle and the map marker turn with the view. Raidy II's build maps
  wall and door textures the other way round.

## What Raidy II's engine build adds

Raidy II's exe reads the same archives, pictures, sound and scripts; about sixty commands are new.

- Pictures carry a table of cells in the GGP header (offset and size at `0x1c`): each cell is a destination, a
  source and a size, 12 bytes, and cell 0 is the whole picture. Layers (`0215`), buttons (`0716`, `0717`, `071f`,
  `0721`), button states (`071d`), tips (`0722`) and draws (`010a`-`010d`: onto a layer or a picture, blended or
  copied) name a cell. A negative cell number places the cell at its own destination; a button cell of -1 has no
  picture. Scripts name pictures and sounds without the archive's extension.
- Layers keep a drawing order. `0212` swaps what two layer numbers refer to, and each layer keeps its place in the
  order; the order is saved. `0213` shows or hides a range, `0214` and `0216` make built layers from a picture region
  or a fill, `0217` and `0218` jump when a layer is shown or hidden. The PC redraws only at a fade's `0009`/`000a`
  pair or while it waits for input, never during a timed `000b` wait. A built layer keeps the picture it was built
  from: loading another picture into its slot (`0101`) leaves it as it was until `0207` or `0208` builds it again, so
  the opening's desert stays behind the logo, which is loaded into the same slot.
- `0008` calls through a switch table, `040b` reads a variable indirectly and `040d` copies a run of them, `0309`
  copies flags, `0108` loads the picture a text slot names, `0109` frees a range of pictures, `0719` and `071c` set
  the mode of or clear a range of buttons, and a variable reference in a destination writes the variable it names.
- Music: every script has a routine that takes the track in variable 476, copies it into variable 0 and plays
  `0600 RD2%02X`. The PC fills the name from variable 0 in decimal (track 29 plays `RD229.OGG`, matched by its
  sound under Wine), zero-padded to the width.
- The opening movie (`0e00`), `RAIDY2OP.MPG` in the game folder, is MPEG-1 with Layer II sound, played by the shared
  `vnvideo` decoder into a screen rectangle; a click skips it.
- Animated scenes (`0d00`-`0d05`) are `GANM0100` files in `GRP`: a program of show-frame, hold and loop steps and
  800x600 frames, each a run-length key frame or a delta over the frame it names. They play on layer 5 in 40 ms
  units. `0d02 2` finishes the loop and stops, `0d02 0` unloads, `0d03` reports when it has stopped, `0d04` pauses
  around the save menus and `0d05` waits a frame.
- The automap (`0c00`-`0c06`, `0809`): fourteen planes of 40x40 bits, most significant bit first, set where a cell is
  unexplored. `0c03` resets them, `0c04` marks a rectangle explored, `0809` marks the player's cell and draws it, and
  `0c06` and `0c05` draw a floor's map from cells or pixel positions: explored cells from the finished map, unexplored
  ones from blank parchment.
- Equipment (`0f00`, `0f01`, `0f03`, `0f04`): the item tables are in the exe. The hands share a twenty-entry table
  (twelve weapons, three of them two-handed, then eight shields); there are ten armours and six accessories. `0f03`
  works out hit, attack, defence and speed from the level, the base values and the equipment, `0f04` equips or
  removes, and `0f00` and `0f01` keep and restore all of it while a shop lets Raidy try something on.
- The backlog (`0914`, `0915`, `0509`, `0b04`) keeps the last hundred messages with their speaker, named from
  `DATA/NAMELIST.TXT`, and voice. Button type 7 is a vertical scrollbar (`0712`, `0714`, `071e`).
- The message window's Skip and Auto toggles are bound with `071a` and `071b`; `0916` clears the window; `0715` waits
  for a time or input.
- Read marks: a script names its slot in the marks table with `090a` at its start, and `0908`'s first argument is the
  line's number in that script. `0a0b` (in `START.SNR`) loads the table and `0a0a` (in `CLOSE.SNR`, run when the game
  quits) writes it back, as VNDegen also does when the game closes (Saves, below). Scripts without a slot, such as
  the battles, count as read. Settings 1 and 2 are the skip setting (0 Skip All, 1 Read Only) and Nonstop. With Read
  Only, Skip shows an unread line and waits for a click, then goes on skipping; Skip ends at a choice or other input
  wait unless Nonstop is on.
- A button bound with a call returns to the input wait it interrupted, so a page such as the save page stays open after
  the action. `070c` and `070d` keep and restore the cancel and key bindings along with the buttons' modes. A jump
  from a button drops the current message only when it interrupts the message itself.
- The window font is "ＭＳ ゴシック" (MS Gothic), bold, drawn with the shared substitute and emboldened as Windows does.

## Saves

The PC engine keeps every slot in one file in the game folder, `RAIDYUS.SAV` for Raidy and `RAIDY2R.SAV` for
Raidy II, and so does VNDegen:

- A header with the slot count (50), a table of slot offsets, the global block (flags and variables that last across
  games, such as the extras' unlocks) and a 64-word settings block (effect speed, skip mode, auto and text speed,
  sound switches and volumes, voiced characters).
- The PC writes the global block only when the game quits, from `CLOSE.SNR` (`0a03`), and when a slot is loaded. A
  player who leaves through VNDegen's menu doesn't run `CLOSE.SNR`, so when the game closes VNDegen does what that
  script writes: the global block and, in Raidy II, the read marks. Without this the ending's clear flag and gallery
  unlocks since the last in-game quit were lost.
- Each slot: the date, the line on screen, a 112x84 thumbnail and an 8544-byte state: the script and label to resume
  at, the BGM, the flags, the variables, the picture names, every layer and every button with its binding. A load
  restores the state, runs `LOAD.SNR` (which rebuilds the dungeon from the variables) and resumes.
- The PC rebuilds a loaded layer from its picture slot, so a layer that kept its picture after `0101` reloaded the slot
  comes back with the new picture: the PC's own save from the opening, after the logo, loads with the logo on black.
  VNDegen adds a block after the state when a layer keeps its picture: `KEPT`, a count, then each such layer's number
  and the name of the picture it keeps (12 bytes). A load rebuilds those layers from those pictures, so the opening's
  save loads with the desert, as live play shows it. The PC skips the block. Slots without it, the PC's and those
  saved before the block was added, load as the PC loads them.
- Raidy II's header gives the settings block at `0xdc` and the global block from `0x1dc` to `0x25d`. The read marks
  follow the global block: a count of slots (200), their size in bytes, then 128 bytes a slot, line n at byte n / 8,
  bit `0x80 >> (n % 8)`. Raidy's PC keeps no read marks, and its file has no such block. Its slot state adds the
  automap after the variables, a 972-byte button record (its 840 extra bytes are written with the values a fresh PC
  save holds) and the animation's state at the end.
- The save file is found whatever the case of its name in the folder.
- Slots saved by the PC engine under Wine load here on the same screen, and slots saved here list and load in the PC
  engine on the same screen, for both games.

## What runs

- The logo, the title, Continue, Options and the extras menu.
- The intro and the story's events: messages with name plates and voices, choices, BGM and effects.
- The dungeon: walking and turning, the status panel, the map with its marker, the compass, events in cells, the
  potion, equipment and status menus, and battles with their menus and messages.
- The message window's buttons, Skip and Auto, hiding the window with a right click, the save and load pages with
  their confirm dialogs, and the options page with its toggles and sliders.

Games start on the "Visual novel" controller layout.

## Known limits

- Raidy's backlog (the window's Log button and the wheel) isn't done; Raidy II's is.
- Raidy: Skip passes every line and ends at a choice whatever the skip setting and Nonstop say. Its PC keeps no read
  marks in its save; under Wine its Read Only skip passed lines not read in that run, but stopped after a choice with
  Nonstop on, and wasn't worked out further.
- Auto waits for the voice, then a delay of twice the auto speed setting in milliseconds.
- Effect mode (Nowait, Fast) doesn't shorten the scripts' fades.
- Layers drawn as rotated or skewed quads (`0204`) are drawn as their bounding box; Raidy uses quads only for
  full-screen fades.
- The texture filtering and lighting of the 3D view are close to, not identical with, the PC's.
- Raidy II: the arguments of `0716`/`0717` after the cells, `000e`, `0010`, `0011` and `0724` are accepted and do
  nothing; what they do wasn't found. `090b`, at the end of most scenes, and `090c` are accepted and do nothing.
- Raidy II was played by hand only on the first dungeon floor. The rest was run headless: each of its 135 story
  scripts started from a save built from the shipped unlock save's dungeon slot and auto-played 250 steps with no
  error, missing file or sanitizer report, and the ending (`L2_ED`), from the line before it, plays through the staff
  roll to the title as the PC's does under Wine, its global block then the PC's byte for byte. Later floors weren't
  walked or fought through.
