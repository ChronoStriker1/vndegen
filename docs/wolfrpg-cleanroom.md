# Wolf RPG Editor runtime: sources

VNDegen's Wolf RPG Editor runtime is its own code under VNDegen's MIT license. No open-source Wolf RPG Editor runtime exists. This file lists, for each part of the runtime, where its knowledge of the file formats and engine behavior came from, and the license of each source. The rule is in `README.md`: the app contains only permissively licensed code, and nothing is copied or ported from GPL, LGPL or unlicensed code.

## Sources

| Source | License | Used for |
|---|---|---|
| [wolf-rpg-formats](https://github.com/djytw/wolf-rpg-formats) (Kaitai Struct descriptions), commit `5c70e643693e08f99bf299a2ca598f7eaa02c542` | MIT | Layouts of Game.dat, CommonEvent.dat, the databases and their .project files, maps, tilesets, event commands, and move routes |
| [UberWolf](https://github.com/Sinflower/UberWolf), `UberWolfLib/` only, commit `f4191375d289171b1f29dbc5ca320133b2b4b96a` | MIT (each file carries the MIT header) | The same data layouts, cross-checked, and the table of Wolf RPG Editor 2 archive key strings |
| [WodiLib](https://github.com/kameske/WodiLib), commit `f8d74b5565f38cc550b37e26b63e105c4691be2d` | MIT | Event command parameters and flag bits, operator codes, and variable address ranges |
| [WolfTL](https://github.com/Sinflower/WolfTL), commit `bfc38fc2735ab4f7f7ddbec8d57b7fef65f0712f` | MIT | Built and run on the test games to see what a parser of these formats produces. No code used |
| README of [Wolf_RPG_Decompyler](https://github.com/Daviid-P/Wolf_RPG_Decompyler) | No license; prose only was read | Which archive versions and key strings Wolf RPG Editor versions use |
| The test games' own files, and the original engine run under Wine as a black box | | Checking each format against real data, and engine behavior |

Not used: the DxLib archiver source that UberWolf bundles in `3rdParty/DXLib`. It carries no license. During the feasibility test, before the licensing rule reached this thread, it was read and an archive reader was written after it. When the rule arrived, that reader was moved out of the repository, never committed, and is kept out for good; the local DxLib copy was deleted. The archive format was then written down as prose (below), from what had been learned of it. A separate agent that had never seen the DxLib source or the discarded reader wrote `src/dxa.cpp` from that prose and the bytes of the test archives alone, and its corrections to the description, each checked against those bytes, are folded in below.

## DxLib archive format (.wolf files)

Wolf RPG Editor packs game data with DxLib's archiver. Wolf RPG Editor 2 uses archive versions 1 to 6. This is the format as the runtime reads it. All numbers are little-endian.

### Encryption

Archives of versions 1 to 6 are XORed with a 12-byte key. The key comes from a key string of up to 12 bytes: the string is repeated to fill 12 bytes, then each byte is transformed:

| Byte | Transform |
|---:|---|
| 0 | bitwise NOT |
| 1 | swap the two 4-bit halves |
| 2 | XOR 0x8A |
| 3 | swap the two halves, then NOT |
| 4 | NOT |
| 5 | XOR 0xAC |
| 6 | NOT |
| 7 | rotate right by 3 bits, then NOT |
| 8 | rotate right by 5 bits |
| 9 | XOR 0x7F |
| 10 | swap the two halves, then XOR 0xD6 |
| 11 | XOR 0xCC |

Wolf RPG Editor 2 key strings, as 12 bytes in hex: `0F 53 E1 3E 04 37 12 17 60 0F 53 E1` (versions up to 2.02a), `4C D9 2A B7 28 9B AC 07 3E 77 EC 4C` (2.10), and `38 50 40 28 72 4F 21 70 3B 73 35 38` (2.20 to 2.24). A reader tries each and keeps the one whose decrypted header starts with `DX`.

A byte at position p of a stream is XORed with key byte p mod 12. Which position a byte has depends on the version:
- Versions 1 to 4: the position is the byte's offset in the archive file.
- Versions 5 and 6: the header is a stream starting at position 0. The tables (below) are one stream starting at position 0. Each file's stored bytes are a stream starting at the position equal to the file's unpacked size.

### Header

At offset 0. In version 6 the address fields are 64-bit; in versions 1 to 5 they are 32-bit.

| Field | Meaning |
|---|---|
| 2 bytes | `DX` |
| u16 | archive version |
| u32 | size of the tables |
| address | offset in the file where file data starts |
| address | offset in the file where the tables start |
| address | offset of the file table, from the start of the tables |
| address | offset of the directory table, from the start of the tables |
| code page | version 6: u64; versions 4 and 5: u32; absent before version 4 |

File data addresses are relative to the data start, which equals the header size in the test archives (24 bytes in version 3, 48 in version 6). The tables are read as one block: the name table first, then the file table, then the directory table.

### Names

Each name entry starts with a u16 giving its length in 4-byte units and a u16 checksum. Then come the upper-case form of the name and the name as written, each padded to that many 4-byte units, so an entry takes 4 + 8 × length bytes. A zero byte ends a name that is shorter than its field; a name that fills its field has none. The root directory's name entry has length 0. Names are Shift-JIS; the version 6 code page field holds 932.

### File table

Each entry, in version 6: name entry offset in the tables, attributes, three 64-bit timestamps, data address, unpacked size, and packed size, each 8 bytes, 64 bytes in all. In versions 2 to 5 the same fields are 4 bytes except the timestamps, which stay 8 bytes each, 44 bytes in all. Version 1 has no packed size, 40 bytes in all. The attribute bit 0x10 marks a directory. For a file, the data address is relative to the start of file data. For a directory, it is the offset of its directory record in the directory table. A packed size of all one bits means the file is stored unpacked.

### Directory table

Each record has four address-sized fields: the offset of the directory's own entry from the start of the file table, the offset of its parent's record from the start of the directory table, the number of entries it contains, and the offset of its first entry from the start of the file table. The root directory's record is at offset 0. Its parent field is all one bits and its own field is 0. A directory's entries are consecutive in the file table.

### Compression

A packed file starts with a 9-byte header: the unpacked size (u32), the packed size including this header (u32), and an escape byte. After it, each byte that is not the escape byte is copied to the output. The escape byte followed by itself outputs the escape byte. The escape byte followed by another byte c starts a back-reference:
- If c is greater than the escape byte, subtract 1 from c.
- The length is c shifted right by 3. If bit 2 of c is set, add the next byte shifted left by 5. Then add 4.
- The low two bits of c give the size of the distance field that follows: 0 for one byte, 1 for two bytes, 2 for three bytes. The distance is that value plus 1.
- Copy `length` bytes, one at a time, from `distance` bytes back in the output. The copy may overlap the bytes it is writing.
