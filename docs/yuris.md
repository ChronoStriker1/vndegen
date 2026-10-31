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
| How to Raise Your Slime Girl | Japanese | 466 | `*.ypf` in the game folder, archive version 466 | Format coverage for the older engine |

Every test game is 1280x720. Their CGs are PNG, with BMP in the 466 game, and their sound is Ogg Vorbis.

## How YU-RIS games are built

A game is an `.exe` (the engine), a `yscfg.dat` of engine settings, and `.ypf` archives, or loose `data/` files during
development. `ysbin/` inside an archive holds the compiled project:

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
task runs, then the frame is drawn, about 60 times a second. The game's whole interface, from the title screen and
message window to the save and load screens, is YU-RIS script, most of it ERIS, the system script library that ships
with the SDK. So a runtime that implements the commands runs the game's own menus.

## Design

The runtime follows the shape of the System 3 runtime, which works well with FrameHost:

- `CYuRIS`, a C runtime with no threads and no platform code. Its interface, `include/vnyuris.h`, matches `vns3.h`:
  open a game folder, call `vny_frame` once per display refresh, read RGBA pixels and a frame serial, send Windows
  virtual keys and mouse events, pull float stereo audio on the audio thread, pause and quit.
- `EngineYuRIS`, the Swift plugin: detection, a session on `FrameView` and `PCMAudioOutput`, input routing, save
  locations, and the Keyboard and mouse layout, as `EngineAliceSoft` has.
- `Engines/YuRIS/host`, a CMake build for Linux with a headless runner (scripted clicks and keys, frame dumps, save
  and load) and an SDL player, used for most of the testing.

Runtime parts:

| Part | What it covers |
|---|---|
| Files | `.ypf` archives of every layout the test games use, loose files, the engine's file priority, file name case through `casepath` |
| Loader | YSCM, YSTB (with the per-game key), YSVR, YSLB, YSTL, YSCF |
| VM | Expressions, INT, FLT and STR values and arrays up to 8 dimensions, global, local, system and per-call variables, IF, LOOP, GOSUB with parameters and return values, GO, labels, tasks with priorities, WAIT, macros as compiled |
| Graphics | Layer tree with layer sets, Z order, position, scale, rotation, alpha, blend modes and clipping; PNG, JPEG and BMP through stb_image; the CGACT effects the test games use; screenshots for save thumbnails |
| Text | FONT and TEXT through stb_truetype and the shared Windows font pack, Shift-JIS and UTF-8 strings, wrapping between English words |
| Sound | SOUND with Ogg Vorbis (stb_vorbis) and WAV, volume, pan, fades and loops |
| Movies | MOVIE for MA☆KO HUNTER's MPEG-1 files, through pl_mpeg as the NScripter runtime does |
| Input | INPUT, MOUSE and the system variables for keys and the pointer |
| Saves | SAVE and LOAD in the PC engine's file format, written where the PC version keeps them, checked against saves the original engine writes under Wine |

Only commands and options the test games use get built. The rest are listed under Known limits.

## Estimate

Rough, for one developer working with the Linux host first:

| Step | Time |
|---|---|
| Files, loader and a disassembler that reads every script of the 5 test games | 2 to 3 days |
| VM, tasks and the commands the ERIS title screen needs | 1 to 2 weeks |
| Graphics, text and sound to the title screens and first scenes | 1 to 2 weeks |
| Saves in the PC format, checked under Wine | 3 to 5 days |
| Movies, the remaining commands the games use, and per-game fixes | 1 to 2 weeks |
| Swift plugin, Mac and iPad and iPhone builds, case check, frame rate check of the movies | 1 week |

That is 5 to 8 weeks in total. The biggest risk is CGACT, which has dozens of image effects; how many the games
actually use decides a large part of the graphics time.

## Known limits

Filled in as the runtime lands.
