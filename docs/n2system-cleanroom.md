# N2System runtime: clean-room provenance

VNDegen's N2System runtime is written from the game files, permissively licensed format documentation and code, and the original engine's observable behavior under Wine. No GPL, LGPL or unlicensed code was read.

## Sources

| Part | Sources |
|---|---|
| `.npa` archives | GARbro, `ArcFormats/NitroPlus/ArcNPA.cs` (MIT, morkt/GARbro at b09ee45). The header layout and the entry name decoding. libnpa (MIT, FGRE/libnpa at 5bdf666) was cloned for format reference. |
| NSS language and built-in functions | NitroSharp (MIT, CommitteeOfZero/nitrosharp at e386984, and its legacy engine at 3ba2b58~1): the parser, the built-in function argument lists, built-in constants, `select` semantics, dialogue blocks and their code parts. Checked against Saya's own scripts, which are plain text. |
| Engine variables, system scripts, save file names | The game's scripts and `system.ini`, read as data. |
| `NPSF` save containers | `saya.exe` under Wine as a black box: saves made in the game, then the routine that writes `NPSF` disassembled only to learn the container layout, the zlib body, the 256-byte substitution table and the key derived from the file name. Allowed by `cleanroom.md` for file formats. |
| Text rendering details, such as `&.` | Screenshots of `saya.exe` under Wine. |

## Off limits

- libnpengine (FGRE, LGPL-3.0) and anything built on it. Not opened.
- nipa (Wilhansen and FGRE forks, no license). Not opened.
- Any other N2System reimplementation under the GPL or with no license. None was opened.

## Open questions and how they were settled

Filled in as the work lands.
