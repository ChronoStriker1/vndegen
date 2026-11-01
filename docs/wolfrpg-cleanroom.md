# Wolf RPG Editor runtime: sources

VNDegen's Wolf RPG Editor runtime is its own code under VNDegen's MIT license. No open-source Wolf RPG Editor runtime exists. This file lists, for each part of the runtime, where its knowledge of the file formats and engine behavior came from, and the license of each source. The rule is in `README.md`: the app contains only permissively licensed code, and nothing is copied or ported from GPL, LGPL or unlicensed code.

## Sources

| Source | License | Used for |
|---|---|---|
| [wolf-rpg-formats](https://github.com/djytw/wolf-rpg-formats) (Kaitai Struct descriptions), commit `5c70e643693e08f99bf299a2ca598f7eaa02c542` | MIT | Layouts of Game.dat, CommonEvent.dat, the databases and their .project files, maps, tilesets, event commands, and move routes |
| [UberWolf](https://github.com/Sinflower/UberWolf), `UberWolfLib/` only, commit `f4191375d289171b1f29dbc5ca320133b2b4b96a` | MIT (each file carries the MIT header) | The same data layouts, cross-checked, the table of Wolf RPG Editor 2 archive key strings, and the encryption of basic data files (`WolfRPG/FileCoder.hpp`, `WolfCrypt/WolfDataDecrypt.hpp`, `WolfCrypt/WolfRng.hpp`: a 10-byte header in place of the magic number, three seeds at fixed header positions, and three passes of Microsoft C `rand() >> 12` over every first, second and fifth byte), reimplemented in `data.cpp` |
| [WodiLib](https://github.com/kameske/WodiLib), commit `f8d74b5565f38cc550b37e26b63e105c4691be2d` | MIT | Event command parameters and flag bits, operator codes, and variable address ranges |
| [WolfTL](https://github.com/Sinflower/WolfTL), commit `bfc38fc2735ab4f7f7ddbec8d57b7fef65f0712f` | MIT | Built and run on the test games to see what a parser of these formats produces. No code used |
| README of [Wolf_RPG_Decompyler](https://github.com/Daviid-P/Wolf_RPG_Decompyler) | No license; prose only was read | Which archive versions and key strings Wolf RPG Editor versions use |
| The test games' own files, and the original engine run under Wine as a black box | | Checking each format against real data, and engine behavior |
| The LZ4 project's block format description, `lz4_Block_format.md` | BSD-2-Clause | The LZ4 decoder and encoder in `data.cpp` and `savefile.cpp`, written from the description. No LZ4 code is used |
| DxLib's own archive source, DxLib 3.19f and 3.25a from dxlib.xsrv.jp | DxLib's own notice (no standard license) | Read only by one agent, with the coordinator's approval (2026-11-01), who wrote the prose section "DxLib archive versions 7 and 8" below and then deleted the source. A second agent that never saw DxLib wrote the version 8 reader in `dxa.cpp` from that section and the archive bytes. No DxLib code is in VNDegen |
| `GamePro.exe` of The Clumsy Witch Seris, Wolf RPG Editor 3.714 (file version 3.714.2026.728, SHA-256 `7bd0b31f…79fce4`) | The game's own engine | Disassembled with the coordinator's approval (2026-11-01), limited to the save path: the routine that writes a save file, the body writer and its section writers, the two routines that encrypt a save, and the check value routine. The encrypt routine was also run on test buffers in an emulator to confirm the keystream. The findings are written as prose below ("Wolf RPG Editor 3.5 saves"); no listing is kept in the repository |

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

## DxLib archive versions 7 and 8

This section was written by an agent that read DxLib's own archive source, downloaded from dxlib.xsrv.jp: DxLib 3.19f (`DxLibMake3_19f.zip` and `DxLib_VC3_19f.zip`, archiver 1.07, which writes version 7) and DxLib 3.25a (`DxLibMake3_25a.zip` and `DxLib_VC3_25a.zip`, archiver 1.10, which writes version 8). It describes the bytes in prose and copies no code. DxLib 3.19d was the first release to write version 7. DxLib 3.19f reads versions 1 to 7, and DxLib 3.25a reads only version 8.

The rules were checked by decoding archives made with DxLib's own archivers 1.07 and 1.10, run under Wine. The test archives covered a given key string and the default key, compressed and uncompressed tables, an unencrypted version 8 archive, nested directories, and every combination of LZ with whole or head-and-tail Huffman coding. Every file came out byte for byte equal to its source. The short-key rule and the code page fallback were not exercised.

All numbers are little-endian. Unless a rule below says otherwise, versions 7 and 8 keep the version 6 rules above. That covers name entries, directory records, the meaning of the file table fields, all-one-bits markers, and the LZ format.

### Telling the versions apart

A version 8 header is never encrypted. It starts with `DX` and the u16 8 in the clear.

A version 7 header is always encrypted, and the encoder has no option to turn encryption off. To recognise one, XOR the first four bytes with a version 7 archive key (below). If the result is `DX` followed by the u16 7, the archive is version 7. If not, try the 12-byte keys of versions 1 to 6. DxLib 3.19f tries the version 7 key first and then the 12-byte key.

Both versions derive their keys from the whole key string, so a reader needs the original string, not the 12-byte form used for versions 1 to 6.

### Keys

**Version 7.** The key is 32 bytes, the SHA-256 digest of a byte string. The archive key is the digest of the whole key string. With no key string, it is the digest of the five ASCII bytes `DXARC`. The byte at stream position p is XORed with key byte p mod 32.

**Version 8.** The key is 7 bytes. To make it from a byte string:

1. If the string is shorter than 4 bytes, append the 8 ASCII bytes `DXBDXARC`.
2. Take the bytes at even positions (0, 2, 4 and so on) as one string, and the bytes at odd positions as another.
3. Take the CRC-32 of each. This is the common CRC-32 of zlib and PNG: reflected polynomial 0xEDB88320, initial value all one bits, result XORed with all one bits.
4. Key bytes 0 to 3 are the even-position CRC as a little-endian u32. Key bytes 4 to 6 are the low three bytes of the odd-position CRC, little-endian.

The archive key comes from the key string cut to its first 63 bytes. With no key string, the key string is `DXBDXARC`. (A comment in the source calls the default "DXLIBARC", but the bytes spell `DXBDXARC`.) The byte at stream position p is XORed with key byte p mod 7.

**Per-file keys, both versions.** Each file's data has its own key. Build a byte string from these parts, with no separators:

1. The key string, cut to its first 63 bytes. In version 7 with no key string, this part is empty. In version 8 with no key string, it is `DXBDXARC`.
2. The upper-case name of the file. This is the first of the two name fields in its name entry, up to its zero byte or the end of the field.
3. The upper-case name of the directory that holds the file, then of that directory's parent, and so on outward. The root directory adds nothing.

Then derive a key from that string the same way as the archive key for that version: SHA-256 in version 7, the CRC pair in version 8. For example, the file `Sub/Deep/data.dat` in an archive with key string `K` uses the string `KDATA.DATDEEPSUB`. The names are the stored bytes in the archive's code page.

Test values. The `abc` row is computed from the rules above, not taken from an encoder.

| Input | Version | Key |
|---|---|---|
| `DXARC` (version 7, no key string) | 7 | `AB F4 D7 B6 F8 C3 3D 83 77 10 EB 8B AE 5C 59 10 7C 73 05 1D 82 C9 FD 32 2B 62 08 AD 24 36 D0 6E` |
| `DXBDXARC` (version 8, no key string) | 8 | `5C AE 2B B1 23 B3 D9` |
| `abc` (shorter than 4, becomes `abcDXBDXARC`) | 8 | `FE 1C 65 D3 70 A2 BF` |
| `WLFRPrO!p(;s5((8P@((UFWlu$#5(=` | 7 | `98 4D 00 FB 4B DD 8C CF 02 6B 61 0C A1 DA 52 9F F9 4B C3 9C 40 50 F7 63 88 C5 EF 1D 7C 17 0E B7` |
| `WLFRPrO!p(;s5((8P@((UFWlu$#5(=` | 8 | `56 F0 6C A7 87 CF 0E` |
| `DXBDXARCDATA.DATDEEPSUB` (a per-file string) | 8 | `E4 FF E8 79 61 8E DF` |

### Header

Version 7's header is 48 bytes, the same fields as version 6. Version 8's header is 64 bytes. In both, the data start equals the header size.

| Offset | Version 7 | Version 8 |
|---:|---|---|
| 0x00 | 2 bytes `DX` | 2 bytes `DX` |
| 0x02 | u16 version (7) | u16 version (8) |
| 0x04 | u32 size of the tables, unpacked | the same |
| 0x08 | u64 offset in the file where file data starts | the same |
| 0x10 | u64 offset in the file where the tables start | the same |
| 0x18 | u64 offset of the file table, from the start of the tables | the same |
| 0x20 | u64 offset of the directory table, from the start of the tables | the same |
| 0x28 | u64 code page | u32 code page |
| 0x2C | | u32 flags |
| 0x30 | | u8 Huffman size in KiB |
| 0x31 | | 15 reserved bytes, zero |

Flags bit 0 (0x1) means nothing in the archive is encrypted. Bit 1 (0x2) means the tables are stored without compression. DxLib defines no other flag bits and tests only these two.

Wolf RPG Editor 3.x archives keep a u16 "crypt version" of their own at offset 0x2E. In DxLib's layout, those two bytes are the upper half of the flags field. In the Wolf RPG Editor 3.x test archives the flags field reads 0x013A0000, so both DxLib flag bits are clear.

The Huffman size applies to file data (below). 0 means no file uses Huffman coding. 255 means a file's data is Huffman-coded whole. Any other value n means only the first and last n × 1024 bytes are Huffman-coded.

Version 8 names use the code page in the header. DxLib falls back to Shift-JIS (932) when the code page is one it does not know. The archives in this project carry 932.

### Encryption layout

| Part | Version 7 | Version 8 |
|---|---|---|
| Header | archive key, stream starting at position 0 | not encrypted |
| Tables | archive key, one stream starting at position 0 that runs across the name, file and directory tables | archive key, one stream starting at position 0 that covers the whole table block as stored (after compression) |
| A file's stored bytes | that file's key, one stream starting at the position equal to the file's unpacked size | the same, and the stream runs on across all of the file's stored parts |

In version 8, when flags bit 0 is set, skip every XOR.

### Tables

Version 7 tables are never compressed. Read the table size from the header, take that many bytes at the table start, and XOR them. File table entries are 64 bytes, as in version 6.

Version 8 file table entries are 72 bytes: the eight 8-byte fields of version 6, then a ninth 8-byte field, the Huffman-coded size of the file's data. All one bits there means the file has no Huffman stage.

How to read the version 8 table block:

- If flags bit 1 is set, take the table size's worth of bytes at the table start and XOR them.
- Otherwise the block runs from the table start to the end of the file. Its stored length is not recorded anywhere else. XOR it, Huffman-decode it (below), then LZ-decode the result. The output is the table size's worth of bytes: the name table, the file table and the directory table, at the offsets in the header.

### File data

A file's stored bytes start at the data start plus the file's data address. Files of size 0 have no stored bytes. The encoder pads each file's stored bytes to a multiple of 4. Readers do not need the padding, because each file entry gives its own address.

Version 7: if the packed size is all one bits, read the unpacked size's worth of bytes and XOR them. Otherwise read the packed size's worth of bytes, XOR them, and LZ-decode the result.

Version 8 adds a Huffman stage. Let L be the packed size if it is not all one bits, or the unpacked size otherwise. L is the length of the data the Huffman stage covers. Let H be the Huffman-coded size and K the header's Huffman size times 1024.

- No Huffman stage (H is all one bits): read L bytes and XOR them. If the packed size is not all one bits, LZ-decode them.
- Whole Huffman (the header's Huffman size is 255, or L is at most 2 × K): read H bytes, XOR them, and Huffman-decode them into L bytes.
- Head and tail Huffman (the header's Huffman size is not 255, and L is more than 2 × K): read H + L − 2 × K bytes and XOR them as one stream. The first H bytes Huffman-decode to 2 × K bytes, which are the first K bytes and then the last K bytes of the L-byte data. The L − 2 × K bytes after them are the middle of the data, stored as is. Put the first K decoded bytes, the middle, and the last K decoded bytes together.
- After either Huffman case, if the packed size is not all one bits, LZ-decode the L bytes.

So the encoder applies LZ, then Huffman, then XOR, and a reader undoes them in the reverse order. The tables use the same order. The encoder chooses which files get LZ and which get Huffman from the file extension, but a reader needs only the two size fields.

### Huffman coding

A Huffman stream is a header followed by coded data. The header is read as bits, most significant bit of each byte first:

| Bits | Meaning |
|---|---|
| 6 | n |
| n + 1 | decoded size in bytes |
| 6 | m |
| m + 1 | size of the coded data in bytes (not needed to decode) |
| 256 groups | one group per byte value 0 to 255: 3 bits c, then 1 sign bit, then 2 × (c + 1) bits of magnitude |

The weight of byte value 0 is its magnitude. Each later weight is the previous weight plus its magnitude, or minus it when the sign bit is 1. Weights fit in 16 bits. The encoder scales each byte value's count to the range 0 to 65535 (count × 65535 ÷ total, rounded down), so a byte value that occurs can still get weight 0. The coded data starts at the first byte boundary after the header.

The decoder rebuilds the code tree from the weights, so it must build it exactly the way the encoder did. There are 511 nodes. Nodes 0 to 255 are the byte values, with their weights, and all 256 take part, including those with weight 0. The inner nodes are numbered 256 to 510 in the order they are made. To make each one, look at the nodes that do not yet have a parent. Take the one with the lowest weight, with ties going to the lowest node number. Then take the lowest of the rest by the same rule. The new node's weight is the sum of the two. The first one taken is its 0 child and the second is its 1 child. Node 510 is the root.

The coded data is read least significant bit of each byte first, the opposite order from the header. For each output byte, start at the root and follow the 0 or 1 child for each bit until reaching a node numbered below 256. That number is the output byte. Stop after the decoded size.

## Wolf RPG Editor 3.5 data files

Sources: UberWolf's `FileCoder`, `CommonEvents`, `Map` and `Database` (MIT), checked against The Clumsy Witch Seris.

Wolf RPG Editor 3.5 compresses its data files with LZ4. Where a file is compressed, the bytes from a given offset on are the unpacked size (u32), the packed size (u32), and one LZ4 block. The part before that offset stays as it is.

- CommonEvent.dat: compressed when the version byte after the magic number is 0x93 or 0xCC, from the byte after the version. Each command then ends with a byte count and that many bytes, which the runtime skips.
- The databases (`.dat`): compressed when the byte at offset 10 is 0xC4, from offset 11. The version byte at the end lies inside the compressed part.
- Maps (`.mps`): the u32 at offset 20 is the map version, 0x64 before 3.5. From 0x65 the file is compressed from offset 25. From 0x67, two u32 follow the event count, the second being the map's number of tile layers, and commands end with the extra bytes as in CommonEvent.dat.

## Wolf RPG Editor 3.5 saves (version 0x9F)

Source: the disassembly of `GamePro.exe` named in the table above, with three saves the engine wrote under Wine as test data. The MT19937 seed function is the one UberWolf names `genMTSeed` (MIT).

### Container

A save written by Wolf RPG Editor 3.714 has 3 in header byte 8, where older saves have 0. The header is 20 random bytes apart from these:

| Byte | Meaning |
|---:|---|
| 0, 1, 5 | Three seeds, not all zero |
| 3 | The sum, modulo 256, of every byte from offset 20 to the end, before encryption (the sizes and the packed body) |
| 4, 7 | The high and low byte of a 16-bit check value (below) |
| 6 | `U` (0x55) |
| 8 | 3 |

From offset 20 the file holds the unpacked size (u32), the packed size (u32), and the save body as one LZ4 block. The packed size is the file size minus 28.

To write one: build the body, compute the check value from it, compress it, put the two sizes in front, set byte 3 to the byte sum, XOR with the stream, then swap the blocks. To read one, undo the steps in the other order. Both steps are their own inverse.

- The stream: seed the standard 32-bit Mersenne Twister (MT19937) with a value made from the three seeds. Put x = seed0 shifted left 16, OR seed1 shifted left 8, OR seed5. Then y = x XOR (x shifted left 13), z = y XOR (y shifted right 17), and the MT seed is z XOR (z shifted left 5), all in 32 bits. Take the low byte of each of the first 128 outputs. The byte at file position p, for p from 20 on, is XORed with stream byte p mod 128.
- The swap: the 20 bytes starting at 20 + (header byte 11 mod 30) change places with the 20 bytes starting at 80 + (header byte 14 mod 40).
- The check value: XOR together every byte of the unpacked body. With h as header byte 9 and x as that XOR, the value depends on h mod 3. For 0 it is (h XOR x) × 7 + 0x11. For 1 it is ((h shifted right 4, OR h shifted left 4) XOR x XOR 0x55) + h. For 2 it is ((NOT h XOR x) + 0x33) × 3, XORed with h. All in 16 bits.

The engine uses another scheme, with 0xFF in byte 8, when the body is under 200 bytes. No real save is that small, and VNDegen doesn't read it.

### Body

The body keeps the layout of version 142 (`docs/wolfrpg-save-format.md`) with these additions, all confirmed by parsing the three saves and writing them again unchanged:

- Before the map section, the arrays of event command 255: a u32 count of numbered arrays, each a u32 number and its rows, then a u32 count of named arrays, each a string and its rows. The rows are a u16 count, and each row is a u16 count of u32 values.
- In the map section, a u32 layer count after the map's width and height, and the tiles cover that many layers. The count is written even when the tiles aren't. After each map event, a byte and, when it is 1, a string with a u16 length. Wolf RPG Editor 3.714 writes 1 and the event's name for games made with editor version 364 or later.
- In each character: two u32 after the byte `unknown4`; a u32 before each string of the list that version 0x8A added; and at the end a u32 count (4 in every save seen) of lists of u32 pairs, a u32, a u32 flag followed by six u32 when it is 1, a u32, and a byte flag followed by eight u32 when it is not 0.
- In the system section, after `v108a`, a u32 and a u32 count of entries (each a u32, a string with a u16 length, and four u32) take the place of `v108s2` and `v108b`. At the end come a u32 count of lists of (u32, string) pairs, a u32 count of string pairs, four u32, and a u32 count followed by one u32 when the count is not 0.
- At the end of the variables section, a u32 count of (u32, byte) entries.
- In each picture: six u32 after `b`, eight u32 more before the lists that version 0x89 added, and at the end two u32 and a byte flag followed by 13 u32 when it is set.
- After the pictures, before the CDB, a u32 count of entries, each a string and three u32.

What the new fields mean isn't known. VNDegen carries them over from the last save it read, and writes the values the engine wrote in the test saves when it has none.

### Uncertain points

- The engine writes the two u32 in each character before `unknown5`, and VNDegen keeps them there. The eight extra u32 in each picture lie somewhere in the run between `v72b` and `v89Lists`; VNDegen keeps them after `v87`. Neither placement matters while those fields are only carried over.
- Saves of versions 0x8F to 0x9E, from Wolf RPG Editor 3.3x, haven't been seen. VNDegen refuses them rather than guess.

