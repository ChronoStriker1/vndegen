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
| `System39.exe` run under Wine as a black box | AliceSoft's engine | The save file format, from saves it wrote for test scenarios built with the SDK, and the behaviour of commands the manual leaves open |
| `SYSTEM3.EXE` (the Windows 3.1 engine of the System 3.0 games) run under 32-bit Wine in a local Docker image as a black box | AliceSoft's engine | The System 3.0 save format and command behaviour, from hand-assembled test pages |
| The English releases' `replacement.txt` and `GAIJI.DAT` | The player's copies | Translated text, and the custom characters of the save lists, read from the files |
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
  The runtime's saves for the same scenarios are byte-identical to the 40 files the original wrote. The same runs
  showed that `LD` resumes after the `QD` that made the save.
- **Commands the manual leaves open**, each settled by an SDK test page run on `System39.exe` and read back from the
  save it wrote: `N&`, `N|` and `N^` write their result into the second variable and leave the first alone; `VG`
  outside the unit map gives 65535 and `VS` there changes nothing; `ND` values read out truncated and modulo 65536;
  `VH`'s step paint for movers larger than one unit; the `VA` sprite moves, by recording a sprite's state over time;
  `LE` and `QE` variable files; key input during timed waits.
- **System 3.0.** `ADISK.DAT` and the CG `.DAT` archives were read from the files. The bytecode follows the System 3.5
  manual where it agrees, and the rest was settled with test pages assembled by hand into a copy of `ADISK.DAT` and run on
  `SYSTEM3.EXE` under 32-bit Wine in a local Docker image, which also wrote the reference saves: the runtime's
  `ASLEEP_<slot>.DAT` files match the original's on 9 test pages, including how it pops a pushed variable.
- **CE and CD.** SDK test scenarios ran each effect on `System39.exe` under Wine with a slowed step time, recorded at 30
  frames a second, over a source whose cells are all different colours. The step counts and patterns come from those
  frames, and the default times from the SDK's table (`cetime.html`), which the same recordings confirmed. They differ
  from that table in places: effect 4 has width / 2 steps, 7 and 8 move 16 pixels a step, and 17 has 16 steps. CD effect
  1001 was read from a source of all 256 colours over two backgrounds.
- **NI and the text skip.** The same way: what `NI` leaves in the variable and RND after OK and Escape, from saves; what
  the 文字送り menu item passes and where it stops, from markers drawn between waits. Rance 4.2's Skip until choice item
  stops at a choice the same way.
- **`Y 13` and `Y 14`.** `Y 14`'s answers come from a hand-assembled page on SYSTEM3.EXE and the save it wrote. Wine's
  `+relay` log shows `Y 13` changes nothing: a line's `TextOut` calls come in one millisecond, and `AMUS.DAT` is never
  opened by a test page that plays music with no CD track.
- **Everything else** (windows, menus, text, palettes and fades, sound commands) follows the SDK manual.

## Exposure note

While looking for a permissive VSP decoder, the author read `ALDExplorer/VSPFormat.cs` from UserUnknownFactor's
ALDExplorer. The repository is MIT-licensed and the file credits no other project, but its comments look ported from
older C code, which could be a GPL decoder. The copy was deleted the same day and not used, and the coordinator was
told. The VSP decoder was then written from the game files as described above.

## Open questions and how they were settled

- **VA.** In neither SDK manual. The 3.9 compiler accepts it with a subcommand and three arguments; what each
  subcommand does came from the games' scripts and from `System39.exe` running test pages under Wine.
- **String count in saves.** The save counts strings from string 0: 5000 by default, and `MZ` n gives n + 1.
- **The save's tail.** 256 bytes of `ff`, then one zero byte per label call and two per page call on the stack, their
  count, and `INFS`. Every test save matched this; what the bytes mean was not needed.
