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
- Runs CE and CD copies step by step, with the patterns and timing of System39.exe: effects 3, 4 and 6 to 23 of CE, the
  ones the library's games use, and CD effects 4 and 1001. CD 1001 moves the destination into the source's palette bank
  rather than copying the picture. The picture at the end is the plain copy, so saves don't depend on an effect.
- Shows the games' number box (NI) over the game, with a keypad and steppers for touch and digits and arrows on a
  keyboard. On a controller, up, down, left and right change the number by 1 and 10, and the D-pad does so under the
  default Keyboard and mouse layout too. Return, Space, Confirm or A is OK, and Escape, a right click, Cancel or B cancels,
  which gives RND 255 and leaves the variable alone, as on the PC. A number outside the range can't be accepted.
- Text skip: Skip until choice, in Game commands, passes message waits until the next choice or left click, which turn
  it off, as the same item does in the PC exes' menu bars (文字送り in System 3.9). On System 3.9 that also passes IK 0 and
  1, the key waits Panyon's messages use. Timed waits, fades and effects still run, as they do on the PC.
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
to reach a game's save point, `--repl FIFO` takes commands one at a time for long scripted sequences, `T:skip:1` turns
the text skip on, and `T:number:V` answers the next number box.

## Comparing with the PC

The PC engines run under Wine as black boxes, for open questions rather than scene by scene. Keep your own copies of
the games, a private `Xvfb -displayfd`, and your own `WINEPREFIX` with its `Documents` link pointed at a scratch folder.

- **System 3.9** runs on Wine 11 directly: `LC_ALL=ja_JP.UTF-8 wine System39.exe` on that Xvfb. `xdotool` sends input and
  `ffmpeg -f x11grab -framerate 30 ... -c:v ffv1` records frame by frame. The window's client area starts below a
  20-pixel menu bar. The SDK's `Sys39c.exe` and `NL4.EXE` under the same prefix build test scenarios, in full-width text.
  Reading a save with `QD` shows what a command did.
- **System 3.0** is a Win16 program, so it needs 32-bit Wine: a Debian container (`wine wine32:i386 xvfb xdotool`), run with
  `--ipc=host` and the Xvfb socket mounted, on a Xvfb started with `-ac`, so `ffmpeg` on the host can still record it. A
  hand-assembled `ADISK.DAT` page tests one command, and Wine's `+relay` log shows its API calls. Rance 4.1's folder has
  no `AGS.DLL`, so use Rance 4.2: both have the same commands.

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
- CE effects 1, 2, 5 and 24 to 54, and CD effects 1 to 3 and 5, show their result at once. No library game uses them.
- IY 2 and 3, which set whether a click stops the text skip, are read and skipped. No library game uses them.
- LE and QE files stay inside the save and game folders: absolute paths, drives and `..` are refused, and the resolved
  path, symlinks followed, is checked before each read or write. The check and the open are separate steps, so a
  process that swaps a folder for a symlink between them could still redirect the access. Such a process already runs
  as the player and can write the saves itself.
- System 3.0's own music format (`AMUS.DAT`) isn't played, and the PC doesn't play it either: both games that ship it
  (Rance 4.1 and 4.2) offer an FM sound source that asks `Y 14,0` for an FM board, SYSTEM3.EXE answers 0, and a test page
  that asks for music with no CD track never opens the file. VNDegen answers the same, so the games stay on CD music.
- Text appears all at once, one message per frame, and so it does on the PC: System 3.9 has no text speed command, and
  SYSTEM3.EXE ignores `Y 13`, the text speed that Rance 4.1 and 4.2's "Making of" credits ask for. All 8 characters
  of a line reach `TextOut` in the same millisecond under Wine with `Y 13` set to 2 and to 5.
