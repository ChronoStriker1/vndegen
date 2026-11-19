# EAGLS runtime: where the knowledge came from

The EAGLS runtime (`Packages/VNDegenKit/Sources/CEAGLS`, hosts in `Engines/EAGLS/host`) was written for VNDegen. It
derives from no GPL or LGPL code. Its sources:

| Part | Sources |
|---|---|
| Archives (`.pak`/`.idx`) | GARbro `ArcFormats/Eagls/ArcEAGLS.cs` (MIT): the index key string, the C runtime `rand` stream seeded from the index's last four bytes, the entry layout |
| Script encryption | GARbro `ArcFormats/Eagls/ArcEAGLS.cs` (MIT): `EAGLS_SYSTEM`, every second byte from offset 3600, seeded from the last byte |
| Script syntax and commands | The game's own scripts: what each command does was read from how the scripts use it (the title loop, the window and character scripts, the sound and movie scripts), and checked against the original under Wine (screenshots of the title, messages, choices, the window, the name entry) |
| Pictures | Plain BMP; click maps as the scripts and screens use them (the grey level is the id) |
| Text layout | Screenshots of the original under Wine with the MS Gothic substitute installed in the prefix, measured to the pixel: the em size from `150`'s fourth argument, the advance, the line gap, the outline and the grey |
| Message window, system menu, Save and Load pages, confirm dialogs, backlog, animation settings | The game's sheets (`MesWin*`, `MesParts`, `Save*`, `Load*`, `CHECK*`, `HISTMAIN*`, `Option*`) and their click maps, the positions in `EAGLS.ini`, and screenshots of the original under Wine |
| Settings | The PC's Windows property sheet under Wine, redrawn with its Text and Sound options |
| Save files | The exe's save and load file routines and its variable-table insert, read with Ghidra only for the file layout: the banner, the two packed blocks, the XOR with the index key, the seed byte, the 997- and 97-slot hash tables and their probing. Everything else in the state block from diffs of saves the original wrote under Wine, and from loading altered saves in it |
| LZSS | The usual 4 KB window starting at 0xFEE, as GARbro's `LzssStream` (MIT) reads it |
| Sound | dr_wav (public domain or MIT-0) streaming the music from the loose WAVs, stb_vorbis (public domain or MIT) for voices |
| Movies | VNDegen's shared `vnvideo` on pl_mpeg (MIT) |
| Text | stb_truetype (public domain or MIT) with the shared Windows font substitutes (`docs/fonts.md`) |
| File name case | `Engines/Shared/casepath.c`, VNDegen's own |

The exe was otherwise only run. Its strings were read for file names and the trace options, and its save and load file
routines, the variable-table insert and its hash were read with Ghidra to learn the save file's layout, which is a
memory dump of the engine's state. No other code was read.

Things that were unsure and how they were settled:

- Which byte offset a save's position holds: the start of the current message's `#name` line (checked against two
  saves and by loading).
- Variable names ignore case: the scripts write `_MOVName` and `_MovName` for one variable.
- `58` returns 1 when the strings are equal: the sound script plays a name only when it isn't `"0"`.
- `34` is consumed when read: the title and the name entry poll it every frame.
- `123` loops until its condition holds: the wait script loops on `_Timer > _WaitTime`.
- Loops yield: the name confirmation polls the mouse in a loop with no wait; without a frame's wait it would never
  draw.
- `136` opens Save, `137(0)` the title's Load, `137(1)` Load, `146` and `147(0)` the settings: from the map's and the
  title's buttons and their pictures.
