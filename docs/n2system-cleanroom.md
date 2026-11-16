# N2System runtime: clean-room provenance

VNDegen's N2System runtime is written from the game files, permissively licensed format documentation and code, and the original engine's observable behavior under Wine. No GPL, LGPL or unlicensed code was read.

## Sources

| Part | Sources |
|---|---|
| `.npa` archives | GARbro, `ArcFormats/NitroPlus/ArcNPA.cs` (MIT, morkt/GARbro at b09ee45). The header layout and the entry name decoding. libnpa (MIT, FGRE/libnpa at 5bdf666) was cloned for format reference. |
| `.npa` encryption | GARbro's `ArcNPA.cs` (MIT) for the scheme: the first 0x1000 bytes plus the name length of each stored entry go through a 256-byte table, minus a per-entry key byte from a per-game name key, the name bytes, the archive key and the unpacked size, minus the position. GARbro keeps each game's table and name key in a binary data file, not in its source, so Hanachirasu's were recovered from the game's own archives by known plaintext: the PNG, JPEG and Ogg headers at the start of 2,712 entries leave exactly one name key byte (33) with a consistent table, and that table fills all 256 values. Checked by decoding every image and script in the game. |
| NSS language and built-in functions | NitroSharp (MIT, CommitteeOfZero/nitrosharp at e386984, and its legacy engine at 3ba2b58~1): the parser, the built-in function argument lists, built-in constants, `select` semantics, dialogue blocks and their code parts. Checked against Saya's own scripts, which are plain text. |
| Engine variables, system scripts, save file names | The game's scripts and `system.ini`, read as data. |
| `NPSF` save containers | `saya.exe` under Wine as a black box: saves made in the game, then the routine that writes `NPSF` disassembled only to learn the container layout, the zlib body, the 256-byte substitution table and the key derived from the file name. Allowed by `cleanroom.md` for file formats. |
| `script.npf`, `frames.npf`, `bklg.npf` and the record order | `hanachirasu.exe` under Wine as a black box: slots saved by the game, then rebuilt and edited slots loaded back to see what the engine accepts and how it resumes (audio recorded from the engine to find where music resumed). The save and load routines and the structures they write were disassembled only as far as needed to learn the file layout, including the script scanner whose record order `script.npf` depends on (`scan.c` follows its rules, checked against the record lists of the PC's saves and against every Hanachirasu script). Allowed by `cleanroom.md` for file formats. |
| `MouseOver` and `MouseClick` opacity on hover | NitroSharp's `Choice.cs` (MIT), checked against `hanachirasu.exe` under Wine on the title. |
| Text rendering details, such as `&.` | Screenshots of `saya.exe` under Wine. |

## Off limits

- libnpengine (FGRE, LGPL-3.0) and anything built on it. Not opened.
- nipa (Wilhansen and FGRE forks, no license). Not opened.
- Any other N2System reimplementation under the GPL or with no license. None was opened.

## Open questions and how they were settled

- `&.` shows as `.` and `&8` as `&8` (Wine screenshot); objects are scoped per script context, since the menu script ends with `Delete("*")` and the scene survives.
- Which commands the English release added (`FileExists`, `MoviePlay`, `AssocArray`, `SetEnableBacklog`): the game's own scripts say so in comments, and the names appear nowhere in NitroSharp's lists.
- A typo in `hana_05_marker_1.nss` (`WIPE("fade"), 500);`): the PC engine plays on past it, so the parser skips a stray comma.
- Sonicomi was checked from its files and its exe's strings only: Squirrel scripts and a 3D engine, not N2System.
