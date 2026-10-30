# System 3 runtime: clean-room provenance

VNDegen's System 3 runtime (`Packages/VNDegenKit/Sources/CSystem3`, the `EngineAliceSoft` plugin's System 3 side,
and the Linux host in `Engines/System3/host`) is VNDegen's own code under the MIT license. It was written without
reading xsystem35, xsystem35-sdl2, system3-sdl2, alicetools or any fork of them, and without reading the abandoned
`ws/alicesoft` branch. This file lists where each part's knowledge came from.

## Sources

| Source | License or status | What it was used for |
|---|---|---|
| AliceSoft System 3.9 SDK (`Sys39SDK.zip`): the command manual (Japanese, with a partial English translation), sample scenario sources and objects, `Sys39c.exe`, `NL4.EXE`, `System39.exe` | AliceSoft's own published SDK. Its terms forbid redistributing it, so nothing from it is copied into VNDegen | Command names, arguments and behaviour; the compiler and linker as black boxes; the engine as a black box for saves |
| AliceSoft System 3.5 SDK (`Sys35SDK.7z`): manual and `SYS35C.EXE` | As above | Older command encodings that some games use, through the compiler as a black box |
| The games' own files (`.ALD` archives, `System39.ain`, CGs, sound, `_inmm.ini`) | The player's copies | Archive, table, image and sound layouts, read from the files |
| `System39.exe` run under Wine as a black box | AliceSoft's engine | The save file format, from saves it wrote for test scenarios built with the SDK |
| GARbro, `ArcFormats/AliceSoft/ImagePMS.cs` (morkt/GARbro, one commit, 698d509c, 2017-11-26) | MIT, with no credit to or trace of another project | The PMS image layout and its row codes, checked against the games' CGs |
| stb_truetype 1.26 (nothings/stb 003501f) | Public domain or MIT | Glyph rasterizing, vendored in `third_party/` |
| dr_mp3 (mackron/dr_libs a466dbe), with minimp3 | Unlicense or MIT-0; minimp3 CC0 | MP3 decoding for CD music tracks, vendored in `third_party/` |
| VNDegen's shared `casepath`, `vnfont` and `vnmidi` | VNDegen's own code | File name case, Windows font substitutes, MIDI music |

## How each part was built

- **Bytecode.** The encoding of each command was learned by writing one-line probe scripts for every command in the
  SDK manual, compiling each with `Sys39c.exe` (and `SYS35C.EXE` for the older forms) under Wine, and reading the
  object files. Expressions are postfix with saturating 16-bit arithmetic, as the manual's operator pages say.
  A disassembler built from that table (kept outside the repository) decodes nearly every scenario page of the eight
  System 3.9 test games; the commands left are the ones the games actually use.
- **ALD archives and `System39.ain`.** Read from the files: the archive is in 256-byte sectors with a pointer table and
  a link map; the `.ain` is every byte after the tag rotated by two bits, with tagged sections.
- **PMS.** From GARbro's MIT decoder, rewritten in C and checked against the games' CGs.
- **VSP.** From the files: the header's bounding box in 8-pixel columns and its 4-bit palette, and the column and
  bit-plane data, checked by decoding the games' CGs. See the note below.
- **Saves.** Test scenarios built with the SDK's compiler and linker set known values (variables, strings, arrays,
  windows, fonts and colours, calls and pushed variables) and saved with `QD` on the SDK's `System39.exe` under Wine.
  The runtime's saves for the same scenarios are byte-identical to the 38 files the original wrote. The same runs
  showed that `LD` resumes after the `QD` that made the save.
- **Everything else** (windows, menus, text, palettes and fades, sound commands) follows the SDK manual.

## Exposure note

While looking for a permissive VSP decoder, the author read `ALDExplorer/VSPFormat.cs` from UserUnknownFactor's
ALDExplorer. The repository is MIT-licensed and the file credits no other project, but its comments look ported from
older C code, which could be a GPL decoder. The copy was deleted the same day and not used, and the coordinator was
told. The VSP decoder was then written from the game files as described above.

## Open questions and how they were settled

- **VA.** Used only by one test game's minigame screens and in neither SDK manual. The 3.9 compiler accepts `VA` 0 to 8,
  each with three arguments; the games' scripts show what each sets.
- **String count in saves.** The save counts strings from string 0: 5000 by default, and `MZ` n gives n + 1.
- **The save's tail.** 256 bytes of `ff`, then one zero byte per label call and two per page call on the stack, their
  count, and `INFS`. Every test save matched this; what the bytes mean was not needed.
