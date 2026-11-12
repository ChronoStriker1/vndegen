# YU-RIS

VNDegen runs YU-RIS games on a runtime of its own, `Packages/VNDegenKit/Sources/CYuRIS`, written in C from the YU-RIS
SDK's manual, the games' files and the original engine's behaviour. It is MIT licensed like the rest of the app.
`docs/yuris-cleanroom.md` lists where each part came from. No other runtime for YU-RIS exists, open or otherwise.

This file starts as the design checkpoint and becomes the engine notes as parts land.

## Test games

| Game | Language | Script version | Archives | Notes |
|---|---|---|---|---|
| MA☆KO HUNTER | English | 474 | `pac/*.ypf`, archive version 500 | MPEG-1 movies stored as `ED.ypf`, `op.ypf` and `hal_ED.ypf` |
| Saimin Seishidou -Secret Lesson- | English | 488 | `pac/*.ypf`, archive version 487 and 488 | |
| Another Way of Gettin' Paid (GOG) | English and Chinese | 481 | `pac/*.ypf`, archive version 481 | Text in UTF-8, one `_` command per language |
| Saimin Gakushū: Secret Desire (GOG) | English | 555 | `pac/*.ypf`, archive version 500 | Story text is `.txt` files in `sc.ypf` that the game's own scripts parse |
| How to Raise Your Slime Girl | Japanese | 466 | `*.ypf` in the game folder, archive version 466 | Set aside; was format coverage for the older engine |
| Tales of the Yokai Incident Inspectors | Japanese | 466 | `*.ypf` in the game folder, archive version 466 | No `yscfg.dat`. The library's coverage for the older engine |

Every test game is 1280x720. Their CGs are PNG, with BMP in the 466 game, and their sound is Ogg Vorbis.

## How YU-RIS games are built

A game is an `.exe` (the engine), usually a `yscfg.dat` of engine settings, and `.ypf` archives, or loose `data/` files
during development. The plugin takes a folder as a YU-RIS game when it has a `yscfg.dat` and an archive beside it or in
`pac/`, or a `ysbin.ypf` there; some games ship without `yscfg.dat`, and the runtime reads the screen size from
`ysbin/yscfg.ybn` anyway. `ysbin/` inside an archive holds the compiled project:

- `ysc.ybn` (YSCM): the command table. Each command has a name and named keywords with type codes, so the bytecode
  refers to commands and keywords by index and the table says what they are. The table differs between engine
  versions (100 commands in 466, 119 in 481 and 488, 121 in 555), so the runtime dispatches by name, not by number.
- `yst00000.ybn` and on (YSTB): one compiled script file each, encrypted with a 4-byte XOR key per game. Each holds
  instructions (command index and argument count), an argument table (keyword index, type, size, offset), the
  argument data, and line numbers.
- `ysv.ybn` (YSVR): every variable, global and per-file local, with type, scope, array dimensions and initial value.
- `ysl.ybn` (YSLB): labels with the script file and instruction they point to.
- `yst_list.ybn` (YSTL): the source file name of each script number.
- `yse.ybn` (YSER): error messages. `yscfg.ybn` (YSCF): screen size and project settings.

Arguments are postfix expressions: a byte opcode, a 16-bit length and a payload, such as `B` (8-bit number), `W`,
`I`, `L` (16, 32, 64-bit), `F` (double), `M` (string), `H` (variable), `V` (array element, its indexes follow and `)`
closes them), and operators (`+ - * / %`, comparisons, logic, unary minus).

The engine runs the scripts as cooperative tasks. Each task runs until a wait or displayed text, then every other
task runs, then the frame is drawn, as many times a second as the game's `FPS` command asks, 60 unless it says
otherwise. Waits and animations that count frames depend on that rate. The game's whole interface, from the title screen and
message window to the save and load screens, is YU-RIS script, most of it ERIS, the system script library that ships
with the SDK. So a runtime that implements the commands runs the game's own menus.

## Design

- `CYuRIS`, a C runtime with no threads and no platform code. Its interface, `include/vnyuris.h`, matches `vns3.h`:
  open a game folder, call `vny_frame` once per display refresh with the real milliseconds since the last, read RGBA
  pixels and a frame serial, send Windows virtual keys and mouse events, pull float stereo audio on the audio thread,
  pause and quit. `vny_frame` runs as many script steps as the game's frame rate fits into the time it is given,
  carrying the remainder to the next call, so a 120 Hz display runs the game at the same speed as a 60 Hz one. Sound
  plays from the audio thread and movies follow the milliseconds, so neither depends on the step rate.
- `EngineYuRIS`, the Swift plugin: detection, a session on `FrameView` and `PCMAudioOutput`, input routing as the System 3
  plugin does, the game's `save` folder as its save location, and the Visual novel controller layout to start with.
- `Engines/YuRIS/host`, a CMake build for Linux: `yrrun` runs a game headless with scripted clicks and keys, writes
  frames and, with `--wav`, the sound it plays, and with `--hz 120` passes time as a 120 Hz display would; `yrdis` disassembles a game's compiled scripts.

The runtime draws on the CPU into a 32-bit frame and redraws only when a layer changes. ERIS sends every button's
attributes again each frame, so a CG command that changes nothing doesn't count. On the Linux host a frame of Another
Way of Gettin' Paid's first scene takes 1.8 ms of script and 1.6 ms of drawing.

## What runs

| Game | State |
|---|---|
| Another Way of Gettin' Paid | Language picker, title, story with text, backgrounds, characters, music and voice, backlog, Config, save and load, on Linux, the Mac app and an iPad simulator. PC saves move both ways |
| How to Raise Your Slime Girl | Title, story with Japanese text, choices, music, save and load, on Linux, the Mac app and an iPad simulator. PC saves move both ways. Set aside since |
| MA☆KO HUNTER | Title, story in English, the map, the opening movie with its sound, backlog, save and load, on Linux and an iPad simulator. PC saves move both ways. Its title and menu buttons are images in Japanese, as on the PC |
| Saimin Seishidou -Secret Lesson- | Title, story in English, its brand movie, backlog, save and load, on Linux and an iPad simulator. PC saves move both ways. The English is compiled into the game's scripts; the `.trans` files beside the exe are the translator's project files, and the title and menu buttons are images in Japanese, as on the PC |
| Tales of the Yokai Incident Inspectors | Title, story with Japanese text, choices, backlog, save and load, on Linux and an iPad simulator |
| Saimin Gakushū: Secret Desire | Language picker, title, brand movie, story in English, choices, backlog, save and load, on Linux and an iPad simulator. PC saves move both ways. Its engine (555) reads the story's text files with TEXT[FILE=] and the game's own scripts parse them, with two PRODUCT helpers VNDegen builds to match the game's script versions |

## Saves

Saves are the PC engine's `.sd` files in the game's `save` folder (`$_SAVE_PATH` is `save\`). A file starts with `YSSD`,
a format number (192 for engine 0.466, 312 for 0.474 to 0.479, 480 for 0.488, the 0.495 SDK and 555, 481 for Another
Way of Gettin' Paid's engine), the record count and a table of 1024 data numbers. Each record is a variable (type,
dimensions, values), a layer image (the engine's own layer header, 1520 bytes in format 192, 1536 in 312,
1600 in 480 and 1680 in engine 555, then BGRA pixels), or a group of
global variables by class (`SAVE[DNO=n G_INT2=1]`; the class is the flag byte in `ysv.ybn`). Engines 481 and 555
compress variables with Snappy and images with zlib; the others use zlib throughout. Formats from 480 on store each variable's
id. `SAVE[MEM=n]` keeps a log snapshot in memory, and `SAVE[MEM=99999]` writes all 500 snapshots into data number
1024 of the next file, each an embedded save file.

The script position ERIS saves (`TASKINFO SCRIPTPOS`) is the PC engine's string: an 8-byte prefix, then bytes as
letter pairs: the open IF blocks, the GOSUB frames with their return point, the loops with their counters, the resume
instruction and the script's file name. Formats 192 and 312 have a 16-bit GOSUB count and one more byte before the
resume point.

Checked under Wine, both ways: Saimin Gakushū (engine 555), Another Way of Gettin' Paid (481), Saimin Seishidou (480),
MA☆KO HUNTER (312)
and How to Raise Your Slime Girl (192). A slot
saved by the original engine loads in VNDegen and play continues, and a slot VNDegen saved shows in the original's
load screen with its thumbnail, loads and plays on.

## Debug switches

`YR_TRACE=script:from-to` logs each instruction in a range with its argument values; `YR_LOG_CMDS=CG,GOSUB` logs
every use of the named commands with the step number and game time; `YR_TRACE_GOSUB=1` logs calls with their parameters; `YR_WATCH=<id>` logs stores to
one variable; `YR_DUMP_LAYERS=1` and `YR_DUMP_TASKS=1` log the layer tree and the tasks at close; `YR_PROFILE=1` logs
script and drawing time per frame.

## Known limits

- YMV, FLASH, MIDI, CDDA and E-mote (EMOTE) aren't built. MOVIE plays MPEG-1 and Ogg Theora through the shared vnvideo decoder, scaled to its area without smoothing.
- PRODUCT 21 and 24 list a folder's files in archive order, 22 and 23 split and measure scenario lines, and 29 answers
  that files are packed. The rest answer 0 or an empty string: 12 and 34 (save copies and folders), 51 and 52 (the
  debug log), 80 and 81, 10201 and 10205 (typing a save comment, so a comment can't be typed), 20011 to 20014 (end
  roll and window checks) and 21000.
- SOUND's PITCH is kept but not applied. Volume follows a square curve, close to the PC's but not measured.
- CG masks (MID) aren't drawn. CGACT's ROTATION, DIFFUSION, WHIRLPOOL, RASTER and TRIM aren't built; none of the
  running games uses them. Rule transitions use a fixed soft edge.
- WINDOW and WINDOWINFO answer for one window the size of the game; window position, menus and sub-windows aren't
  there. DIALOG answers as if its first button were pressed.
- Not yet: Saint Dorei Gakuen 3 (engine 494) keeps its scripts in `pac/update1.ypf` and its story text in a WORD
  argument form the reader doesn't decode yet. The library's Saint Dorei Gakuen 2 (479) has its scripts as loose files,
  twice, without `ysv.ybn` or `yst_list.ybn`, which looks like an unpacked copy. Neither has been run.
