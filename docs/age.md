# Eushully AGE

VNDegen runs Eushully's AGE engine ("ARCGameEngine"), SYS4 generation, on a runtime of its own,
`Packages/VNDegenKit/Sources/CAGE`, written in C from the games' files, GARbro's MIT archive and picture readers,
OpenMaidEngine's MIT opcode notes read as hints and checked against the game, and the original engine under Wine. It is
MIT licensed like the rest of the app. `docs/age-cleanroom.md` lists where each part came from.

## Test games

| Game | Engine build | Notes |
|---|---|---|
| Kamidori Alchemy Meister (English, Aroduc 1.1 patch) | `age.exe` 4.33, scripts `SYS4433`, catalog `S4IC433` | Logos and opening movie, title, New Game options, the prologue, the tutorial battle map through the exam, the town hub, city map, workshop and party screens, Back Log, Save and Load, the original's saves both ways |

The game is 1024x576.

## How the games are built

A game folder holds `age.exe`, `SYS4INI.BIN` (the catalog), `DATA1.ALF` to `DATA5.ALF` (plain concatenated archives),
append packs (`APPENDnn.AAI` with `APPENDnn.ALF`) and, in the English release, a `patch\` folder the patched exe reads
first. The plugin takes a folder as an AGE game when `SYS4INI.BIN` is beside `DATA1.ALF` or `age.exe`, up to one folder
down.

- **Catalog.** `SYS4INI.BIN` (`S4IC433`) holds the global bank sizes, an LZSS-packed directory of archive names and
  80-byte file records, and a trailer of engine settings (`SCREENX`, `FONT`, `SAVEVERSION`, `WHEELKEYUP` and the
  rest). Each `.AAI` (`S4AC433`) is the same directory for one append pack, addressed by its selector byte. A resource
  is a packed id: the high byte selects the catalog, the low 24 bits index its records. A file of the record's name in
  `patch\` or the game folder replaces the archived one.
- **Pictures.** AGF, an LZSS-packed BMP header and bottom-up pixels (8 or 24 bit) with an optional `ACIF` alpha
  plane. Some of the English patch's pictures are plain BMP files named `.AGF`.
- **Sound.** Ogg Vorbis and WAV in the archives; music is `BGMnnn.OGG` by number.
- **Movies.** MPEG-1 program streams with Layer II sound (logos, opening, ending) or Layer I sound (battle cut-ins).
- **Scripts.** Compiled SYS4 (`SYS4433 `): a 60-byte header with the local bank counts and three tables, then 32-bit
  words: an opcode and a fixed number of (type, value) operand pairs, then a string pool with every byte inverted.
  Operand types are immediates, inline strings, global and local integers and strings, and local references. The root
  script is catalog entry 0, `SYSTEM4.BIN`; it calls the data initialisers, the append autoruns, the logo and opening,
  and the title. The game's systems (story, battle map, workshop, menus, saving) are all scripts; the engine provides
  the script machine and its services.

## What the runtime does

- **Script machine.** 40 call frames with their own locals and 256-entry return stacks, preloaded script slots, the
  append launch queue, the scene coroutine of the hide-window button, value switches, integer queues and stacks, and
  every one of the 269 opcodes Kamidori uses.
- **Graphics.** A slot-addressed surface store and a retained object map drawn in handle order, with source
  rectangles, anchors, one-shot and cyclic scale, rotation, translation, colour and cell channels, the range transform
  the battle map's camera uses, four blend modes, render targets, queued alpha, pattern and blur transitions, and the
  legacy blocking fades and strip reveals.
- **Text.** ADV layouts that rasterise each line into the layout's surface and publish one glyph object per character
  at the configured delay, the fixed-cell aspect mode, wrapping with closing punctuation kept on the line, outline and
  shadow modes, the wait indicator, ruby, surface strings and formatted numbers, and decimal glyph styles. Fonts are the
  shared Windows substitutes (`docs/fonts.md`).
- **Input.** Logical actions from keys, mouse buttons and the wheel as the game maps them, the mouse and joystick
  callbacks, hotspots with enter, leave and click callbacks and bound keys, and the text wait's click, skip and auto.
- **Sound.** Thirteen channels (effects, music on 11, voices on 12), fades, delayed starts, routes and volumes.
- **Movies.** Through the shared MPEG-1 decoder, modal (logos and opening) or into a surface (cut-ins); the shared
  pl_mpeg decodes the cut-ins' Layer I sound (`Engines/Shared/UPSTREAM.md`).
- **History.** The native backlog records, its navigation and the History screen's renderer.

## Saves

The PC engine keeps its files in `%LOCALAPPDATA%\Eushully\神採りアルケミーマイスター\`: `SYS4REG.INI`, and in `SAVE\`
the shared profile `SAVE.DAT`, the read-text database `RT.DAT`, and numbered slots `SAVEnn.DAT` with their `SAVEnn.STH`
thumbnails. VNDegen keeps the same files in the game's data folder, `saves/` and `saves/SAVE/`, so Ludusavi backups
map both ways.

- **Container.** `S4SD`, the writer's engine version, the game id, the logical length, a time stamp, play time and
  the save version, then the payload with two CRCs, LZSS-packed and spread by a multiplier transform, with two more
  CRCs over the result.
- **Numbered slots.** Layout 3: the saved frames up to the one the game marked, as script ids with their message,
  call and return table indices; music and effect channels; the surface records; the saved prefix of each global bank;
  every retained graphics object as the engine's 0x2e4-byte record; the range transform; and the history tail.
- **Loading** clears the banks, restores the objects and sound, runs `CALLBACK_LOAD.BIN`, then starts each saved
  script from its entry so its prologue runs, and resumes it at the call or message its tables record when it reaches
  the restore opcode.
- **Profile.** `SAVE.DAT` keeps the cells the scripts store (slot titles, settings), the unlock tables of every
  resource opened (the CG and scene galleries read these), and `RT.DAT` the read flags per script message.

`SYS4REG.INI` keeps the PC engine's 44 keys in its own order. VNDegen reads and writes the sound volumes and routes,
the message speed, window transparency, Auto times and read-text skip there, and leaves the other keys as it found
them; without a file it starts from the engine's registered defaults.

A slot the original wrote under Wine loads in VNDegen at the same point, and a slot VNDegen wrote loads in the
original.

## Known limits

- The menu bar, the AGERC dialogs (version, settings, exit) and the developer menus are not there.
- Text is drawn with the shared substitute fonts through stb_truetype, so glyph shapes and some advances differ
  slightly from Windows GDI's.
- The resource-reload records of numbered saves are written empty; the game marks none in the tested scenes.

## Linux host

`Engines/AGE/host` builds `agrun`, which plays a game headless from a list of commands (`wait:ms`, `click:x,y`,
`rclick:x,y`, `move:x,y`, `key:vk`, `wheel:n`, `pages:n`, `shot:name`, `shots:every,count`, `state`) and writes PNGs;
`-` reads more commands from standard input. `AG_TRACE=op,op,...` logs those opcodes with their operands. `agplay`
plays a game in an SDL window.
