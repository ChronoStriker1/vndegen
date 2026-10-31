# YU-RIS runtime: clean-room provenance

VNDegen's YU-RIS runtime (`Packages/VNDegenKit/Sources/CYuRIS`, the `EngineYuRIS` plugin and the Linux host in
`Engines/YuRIS/host`) is VNDegen's own code under the MIT license. No other runtime for YU-RIS exists. It was written
without reading the code of any GPL, LGPL or unlicensed YU-RIS tool, such as yuri, YurisKeyFinder (GPL-3.0),
YPF-Manager, RxYuris or yu-ris-ext-ybn. This file lists where each part's knowledge came from.

## Sources

| Source | License or status | What it was used for |
|---|---|---|
| YU-RIS SDK β4.7 0.479/0.14 and β4.8 0.495/0.32 from yu-ris.net (MD5s match the download page): the YU-RIS command manual (HTML, Japanese), the ERIS manual and the ERIS system scripts as source, the engine and its compiler | The author's own published SDK, free to use for games under the YU-RIS/ERIS license. Nothing from it is copied into VNDegen | Command and keyword names and behaviour, variable kinds, layers, tasks and timing; the compiler and engine as black boxes under Wine |
| The games' own files (`.ypf` archives, `ysbin/*.ybn`, CGs, sound, `yscfg.dat`) | The player's copies | Archive, script, variable and label layouts, read from the files |
| The original engines run under Wine as black boxes | The games' engines and the SDK's engine | The save file format and command behaviour the manual leaves open |
| GARbro, `ArcFormats/YuRis/ArcYPF.cs` and `ImageYCG.cs` (morkt/GARbro b09ee45, 2023-10-27) | MIT | The `.ypf` directory layout, its name-length swap table, and the YSTB XOR sections |
| YuRISTools, `YuRISLib/Package/YPF.cs`, `CheckSum.cs` and `YuRISLib/Script/*.cs` (fengberd/YuRISTools 0c5ac2d, 2021-06-04) | MIT | The YSCM, YSTB and YSER layouts, MurmurHash2 as the archive name hash, and the `M` string argument form |

## How each part was built

Filled in as parts land.

## Open questions and how they were settled

- **Archive name lengths.** The `.ypf` name length byte is scrambled with a swap table that differs between games:
  Saimin Gakushū uses a table GARbro doesn't list. The runtime doesn't use a table. It takes each entry's length from
  the candidate whose decrypted name matches the entry's stored MurmurHash2, which was confirmed on every archive of
  the 5 test games.
- **Archive entry tail.** After the name, type, flags and sizes, an entry has a 4- or 8-byte offset and a 4-byte
  checksum. The width doesn't follow the archive version (MA☆KO HUNTER's version 500 archives have 4-byte offsets,
  Saimin Gakushū's version 500 archives 8-byte ones), so the runtime picks the width whose entries end exactly at the
  directory size in the header.
- **Script key.** The XOR key differs per game (`0x96AC6FD3` in two games, three others each have their own). The
  first argument entry's data offset is always 0, so its encrypted offset field is the key.
