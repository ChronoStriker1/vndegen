# AliceSoft System 3

VNDegen runs AliceSoft's System 3 games on a runtime of its own, `Packages/VNDegenKit/Sources/CSystem3`, written in C
from AliceSoft's published SDKs, the games' files and the original engines' behaviour. It is MIT licensed like the rest
of the app; `docs/system3-cleanroom.md` lists where each part came from. It covers two engines:

- **System 3.5 to 3.9** (`System39.exe`): `System39.ain` with link archives named after the game, such as
  `RanceSA.ALD` for the scenario, `RanceGA.ALD` for CGs, `WA` for sound effects, `MA` for MIDI and `DA` for data.
- **System 3.0** (`SYSTEM3.EXE`, Windows 3.1): `ADISK.DAT` for the scenario and `ACG.DAT`, `BCG.DAT` and `CCG.DAT` for
  CGs. English releases that put their translation in `replacement.txt` show it.

## What it does

- Runs the bytecode of both engines: expressions, variables and arrays, labels, page and function calls, menus,
  message windows, number and string output, data tables, the unit map and its step paint, and the VA sprites.
- Decodes the CGs the games use: PMS (8 and 16 bit), VSP in its 16- and 256-colour forms, and BMP. 256-colour
  palettes keep the banks each CG asks for.
- Plays CD music from the MP3 files `_inmm.ini` lists (line k is CD track k + 2), WAV sound effects, and MIDI music
  through the shared vnmidi player and the SoundFont from Settings.
- Draws text in the Windows font pack's MS Gothic substitute, as fixed full- and half-width cells, with the engines'
  conversion of half-width kana to hiragana. Translated text breaks between words.
- Saves where the PC engines do, in the game's folder: `<prefix>S<letter>.ASD` for System 3.9 (slot 1 is `A`) and
  `ASLEEP_<letter>.DAT` for System 3.0. The files are byte for byte what the PC engines write, checked on 40 System 3.9
  and 9 System 3.0 test saves, so Ludusavi backups and PC copies of a game load either way. Files games write with QE,
  such as Toushin Toshi II's `tt2FS.ALD` of game-wide flags, are kept in the PC format too. A Japanese name ending in
  two letters and `.ALD` takes the archives' prefix, as the English releases renamed those files.

## Controls

The games read the keyboard and mouse. The default layout is Keyboard and mouse. Confirm sends Return, Cancel and Menu a
right click, the arrows the arrow keys, and Dash holds Control.

## Testing on Linux

`Engines/System3/host` builds the runtime with CMake. `s3run <game> --ms N [events]` runs a game headless, pressing keys,
clicking and saving frames at the times given, and `s3play <game>` plays it in an SDL window. The game clock starts at a
fixed date, so a run replays the same way. `--auto` answers menus by their text (`T:pick:Save`, `--prefer`, `--avoid`)
to reach a game's save point, and `--repl FIFO` takes commands one at a time for long scripted sequences.

## Frame rate

The runtime steps once per display refresh and draws on the CPU. Panyon's tactical battle, the most animated screen
among the test games (sprites, unit overlays, scrolling), held its 60 fps target: on the Mac app (Mac mini) every
10-second window averaged 60.0 with a 1% low of 60 and a slowest step of 20 ms; on an iPad simulator, 147 one-second
samples averaged 60.0 with one hitch. A DEBUG build launched with `-frameMonitor YES` logs `S3FRAMES` every 10 seconds.

## Known limits

- Ludusavi's manifest lists none of the 10 test games, so Ludusavi doesn't find their saves on its own.
- Commands no test game uses are read and skipped: CT to CZ (alpha data), the CX shadow modes, ME, MF, MG, MI, MJ and
  MN (string search and input), VJ (weighted step paint), the LX file commands, the network commands, verb and object
  menus, and DLL commands.
- NI, the number input box, takes its default as if OK were pressed; Panyon's shop then buys its default quantity.
- CE transitions show their result after the transition's time, and CD sprite copies show theirs at once, rather than
  animating each step.
- Text appears all at once rather than at the game's message speed.
- LE and QE files stay inside the save and game folders: absolute paths, drives and `..` are refused, and the resolved
  path, symlinks followed, is checked before each read or write. The check and the open are separate steps, so a
  process that swaps a folder for a symlink between them could still redirect the access. Such a process already runs
  as the player and can write the saves itself.
- System 3.0's own music format (`AMUS.DAT`) isn't played; the test games use CD tracks instead.
- Mamatoto: in the scripted tutorial's defensive battle, the Squad Formation screen offers 2 of the 5 commanders a new
  game adds, so the demo's scripted clicks miss and it stops at the game's own end-of-script loop before the first save
  point. Whether the runtime or the demo's timing causes it isn't known yet.
