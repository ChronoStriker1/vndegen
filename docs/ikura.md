# Ikura GDL

VNDegen runs Ikura GDL games on a runtime of its own, `Packages/VNDegenKit/Sources/CIkura`, written in C from the
games' files, GARbro's MIT-licensed readers for the archives and pictures, and the original engine's behaviour under
Wine. It is MIT licensed like the rest of the app. `docs/ikura-cleanroom.md` lists where each part came from.

## Test games

| Game | Engine build | Notes |
|---|---|---|
| Lightning Warrior Raidy (G-Collections English release) | `RaidyUS.exe`, 2007-12 | Intro, dungeon floors with battles, menus, saves |

Lightning Warrior Raidy II is left out (see the content rule in the project's private notes). Raidy III ships as a
Setup Factory installer, which VNDegen can't unpack. The game is 800x600.

The disc is a BIN/CUE image; the library's import reads it and keeps only the game's folder (`docs/library.md`).

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
  eye. Turns and steps take 500 ms. The compass needle and the map marker turn with the view.

## Saves

The PC engine keeps every slot in one file in the game folder, `RAIDYUS.SAV` for Raidy, and so does VNDegen:

- A header with the slot count (50), a table of slot offsets, the global block (flags and variables that last across
  games, such as the extras' unlocks) and a 64-word settings block (effect speed, skip mode, auto and text speed,
  sound switches and volumes, voiced characters).
- Each slot: the date, the line on screen, a 112x84 thumbnail and an 8544-byte state: the script and label to resume
  at, the BGM, the flags, the variables, the picture names, every layer and every button with its binding. A load
  restores the state, runs `LOAD.SNR` (which rebuilds the dungeon from the variables) and resumes.
- Slots saved by the PC engine under Wine load here on the same screen, and slots saved here list and load in the PC
  engine on the same screen.

## What runs

- The logo, the title, Continue, Options and the extras menu.
- The intro and the story's events: messages with name plates and voices, choices, BGM and effects.
- The dungeon: walking and turning, the status panel, the map with its marker, the compass, events in cells, the
  potion, equipment and status menus, and battles with their menus and messages.
- The message window's buttons, Skip and Auto, hiding the window with a right click, the save and load pages with
  their confirm dialogs, and the options page with its toggles and sliders.

Games start on the "Visual novel" controller layout.

## Known limits

- The backlog (the window's Log button and the wheel) isn't done.
- Read marks aren't kept, so Skip skips all text whatever the skip setting says. Auto waits for the voice, then a
  delay of twice the auto speed setting in milliseconds.
- Effect mode (Nowait, Fast) doesn't shorten the scripts' fades.
- Layers drawn as rotated or skewed quads (`0204`) are drawn as their bounding box; Raidy uses quads only for
  full-screen fades.
- The texture filtering and lighting of the 3D view are close to, not identical with, the PC's.
