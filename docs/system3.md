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
  `ASLEEP_<letter>.DAT` for System 3.0. The files are byte for byte what the PC engines write, checked on 38 System 3.9
  and 9 System 3.0 test saves, so Ludusavi backups and PC copies of a game load either way. LE and QE files, such as
  the dungeon `floor_N.map` files one game rewrites, are kept in the PC format too.

## Controls

The games read the keyboard and mouse. The default layout is Keyboard and mouse. Confirm sends Return, Cancel and Menu a
right click, the arrows the arrow keys, and Dash holds Control.

## Testing on Linux

`Engines/System3/host` builds the runtime with CMake. `s3run <game> --ms N [events]` runs a game headless, pressing keys,
clicking and saving frames at the times given, and `s3play <game>` plays it in an SDL window.

## Known limits

Commands no test game uses are read and skipped: CD (sprite copy with effects), CP (flood fill), CT to CZ (alpha
data), the CX shadow modes, ME, MF, MG, MI, MJ and MN (string search and input), NI (number input), the ND
double-precision commands, VJ (weighted step paint), LL and the LX file commands, the network commands, verb and object
menus, and DLL commands. CE transitions show their result after the transition's time rather than animating each
pattern. Text appears all at once rather than at the game's message speed. System 3.0's own music format (`AMUS.DAT`)
isn't played; the test games use CD tracks instead.
