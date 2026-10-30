# AliceSoft System 4 runtime: clean-room provenance

VNDegen runs System 4 games on its own runtime, `Packages/VNDegenKit/Sources/CSystem4`, under VNDegen's MIT license.
It was written without the source of any GPL or LGPL System 4 implementation. No part of xsystem4, libsys4,
alicetools, or their forks was opened, fetched, searched, or asked about, and neither was the stopped `ws/alicesoft`
port. This file lists where each part of the runtime came from, and what was uncertain and how it was settled.

## Sources

- **The games' own files**: the `.ain` code files, the `.ald` and `.afa` archives, the pictures, sounds and saves in
  them. Test Games 19 (Rance 02), 46 (Sengoku Rance) and 56 (Rance IX).
- **AliceSoft's System 4.0 SDK** (`Sys42SDK_20041224.lzh`), a free download AliceSoft published for its user club in
  2004. Its terms allow unmodified redistribution; VNDegen redistributes none of it. The copy used came from a fan
  mirror archived on web.archive.org. Read for how the engine behaves:
  - `DLL開発/Interface/ECmdList42.h`, the enumeration of the VM's instructions 0 to 150 with comments, and
    `EObjType42.h`, the data type numbers.
  - The System 4.2 language manual (`マニュアル/Sys42`): the system functions and their arguments, strings, arrays,
    references and structs.
  - The SACT2 reference (`マニュアル/SACT2/reference`): sprites, Z order, blend rates, text metrics, key codes.
  - The SDK's compiler, `Sys42IDE.exe`, run under Wine as a black box: test programs compiled with its listing output
    on show how each language construct compiles. Its executable's string table gives the instruction mnemonics in
    order, which names instructions 151 to 155, and the system functions in order, which numbers them.
  - The SDK's runtime, `System40.exe`, and its picture compressor, `AliceCG2.exe`, run under Wine as black boxes:
    test programs and test bitmaps in, files out.
- **The games' own executables and DLLs under Wine as black boxes**: Test Game 46's VM DLL (`Sys43VM.dll`) loaded by
  the SDK's runtime, to see the save layout of a later VM.
- **Minimal disassembly**, only where a file format couldn't be settled otherwise: in the SDK's `Sys42VM.dll`, the
  routine that writes a save file's "GD" header and the XOR routine it calls (see Saves); in Test Game 19's
  `StoatSpriteEngine.dll`, the routine that writes MultiSprite_Encode's data, for the order and width of its fields.
- **The PC games under Wine** in a headless gamescope session, to make real saves and load VNDegen's: Test Game 19
  saved on the PC and loaded in VNDegen, and the other way round.
- **Permissive code**: stb_image and stb_vorbis (public domain or MIT), pl_mpeg (MIT), zlib, and FreeType and libwebp
  on the Linux test host only. The text and font code is adapted from VNDegen's own Wolf RPG Editor runtime.
- **Reference outputs**: the ffmpeg and mpg123 command-line programs decoded the games' movies so the movie
  decoder's output could be compared with theirs. Only their output was used; none of their code was read or linked.

## Parts

### The code file (`ain.cpp`)

- Container: "AI2", 4 zero bytes, the plain length, the zlib length, then zlib. Found by reading the header and
  inflating; all three test games use it.
- Sections: four-letter tags followed by their data (VERS, KEYC, CODE, FUNC, GLOB, GSET, STRT, MSG0 or MSG1, MAIN,
  MSGF, HLL0, SWI0, GVER, STR0, FNAM, OJMP, FNCT or DELG, OBJG). Each layout was read from the bytes and checked by
  parsing every section of all three games to the end of the file. Type numbers come from `EObjType42.h`.
- MSG1 (Test Game 56): each message's byte j is stored raised by 0x60 + j. Found by trying a per-byte offset that
  grows with j; every one of the 57,103 messages then decodes as valid CP932.

### Instructions (`opcodes*.h`, `vm.cpp`)

- Numbers 0 to 150 and their names: `ECmdList42.h`. 151 to 155: the IDE's mnemonic table, which lists them in
  reverse order after OBJSWAP.
- Argument counts: solved by constraint search over the code. Every function's first instruction is at the address
  the function table gives, every decode must end exactly at the next function, and every jump must land on an
  instruction. Opcodes whose count stayed ambiguous were settled by the stack checker below and by reading the bytes
  around them.
- Stack effects and meaning, 0 to 155: the SDK compiler's listings for test programs covering every construct.
- 156 and later (instructions the 2004 SDK predates): read from how the games use them. A stack-depth checker
  (abstract interpretation of every function, requiring the same depth on every path into an instruction and the
  declared return size at every return) checked each guess against all three games. What each one was taken to be:
  - 156 to 174: the "lint" type (55) versions of the int instructions, in the int instructions' order.
  - 175 A_FIND, 176 A_REVERSE.
  - 177 to 236: shortcuts the later compilers emit, each standing for a short sequence of older instructions on a
    local, a member of `this`, a global or an immediate, such as "if local < immediate jump" or "this.member =
    local". `opcodes_table.h` names each one; `vm.cpp` shows the sequence it stands for.
  - Uncertain ones and how they were settled: 189 pushes "member != immediate" (used as "is created" and "is
    defined"); 193 and 202 both jump when a member differs from an immediate (their uses are consistent with that);
    198 calls a method on a member of a member of `this` (a first guess, "a member of a global", failed at the save
    screen, where the struct types prove the member reading); 207 is a reference check with no effect on the stack;
    212 assigns the string at (page, index) to a string; 214 is "the string at (page, index) is empty"; 227 assigns
    a function-stack name, used only in error reporting.
- Heap model: pages (globals, a function's locals, a struct, an array) and strings in reference-counted slots whose
  numbers the code handles directly, as the SDK listings show (string variables hold slots, `SH_LOCALCREATE` makes a
  struct, references are page and index pairs and keep their page alive).
- The global initializer: the function named "0" allocates arrays declared with a size; it runs before struct
  globals are constructed.

### System functions (`vm_sys.cpp`)

Numbers 0 to 23 are the IDE's table order; signatures are the SDK manual's. 24 GroupSave, 25 GroupLoad, 26 delete a
save file and 28 copy one were read from the game functions that wrap them (their names and arguments).

### Archives and pictures (`archive.cpp`, `cg.cpp`)

- ALD: a table of 3-byte sector numbers, a map from resource numbers to (disk, index), and a header per file. Read
  from the files and checked by extracting every entry of the test games' archives.
- QNT: zlib-compressed planes in 2x2 blocks, each byte a prediction minus the pixel. The block order and the
  prediction were found by decoding real pictures; the exact rule (the first pixel stored as is) by compressing a
  known 10x6 bitmap with the SDK's `AliceCG2.exe` and comparing every byte.
- PMS (8-bit): the commands (literal, F8 escape, FF and FE copy from one or two rows above, FD repeat a byte, FC
  repeat a pair) from encoding test bitmaps of chosen patterns with `AliceCG2.exe`.
- AJP: a JPEG and an 8-bit PMS mask, each with its first 16 bytes XORed with a fixed key. The key was recovered by
  comparing many files with the fixed JFIF and PMS headers they must start with. All 1,741 pictures in Test Game 19
  decode.
- AFA (Test Game 56): an "AFAH" header naming "AlicArch", then an "INFO" block holding a zlib-compressed index. Each
  index entry is a name length, a padded length, the name, two unused words, the offset from the data block and the
  size. Read from the files and checked by listing every entry of Test Game 56's archives.
- WebP: some later pictures, and some AJP masks, are WebP files. They decode with ImageIO on Apple and libwebp on
  the Linux host.

### Fonts (`fnl.cpp`)

The `.fnl` font libraries were decoded from the game files' bytes. The header is "FNA\0", a zero, the file size, an
index size and the face count. Each face lists its sizes; each size is a cell height, an unused metric, a slot count
and a 10-byte record per slot (advance, file offset, compressed length). The records carry no character codes: the
slot number follows the Shift-JIS code (ASCII, then half-width kana, then JIS X 0208 rows of 94). This was confirmed
because the empty slots in Test Game 46's file line up with the unused cells of JIS X 0208. Each glyph is a zlib
stream of a 1-bit bitmap, bottom row first, rows padded to 32 bits. The runtime box-filters the large stored cells
down to the text size. Every glyph of Test Games 46 and 56 decodes. Test Game 46 picks a face with font type 0 or
256; 256 was taken to be face 1 (the high byte), from the title menu's look.

### Movies (`movie.cpp`, `hll_movie.cpp`)

The `.alm` movies are MPEG program streams: MPEG-1 system streams, or MPEG-2 program streams around MPEG-1 video
(Test Game 56). `movie.cpp` has its own demuxer for the pack and PES headers, written from the public MPEG-1 and
MPEG-2 stream syntax and the files' bytes. pl_mpeg (https://github.com/phoboslab/pl_mpeg, MIT), vendored at commit
c871f2be022ece7ef4f64230b4fb8e1fb9eb6023, decodes the video and the MP2 audio, with one local fix to its MP2
requantization step that follows the MPEG-1 Layer II formula. The resampler and the audio ring are original. The
decoded frames and sound were compared with the reference outputs above. The DrawMovie2, DrawMovie3 and PlayMovie
calls were read from how the games call them.

### Saves (`save.cpp`, `resume.cpp`)

- GD container: "GD", two bytes 1 1, the plain length, then level-9 zlib XORed with the low byte of each output of a
  Mersenne Twister seeded in the old way (each state word the previous times 69069) from 0x12320F. The generator and
  its use came from disassembling the SDK runtime's routine that writes the header (it passes the data to a routine
  that fills a 624-word table and tempers each output); the seed was found by search against a save of known
  content. Level 9 reproduces the PC's files byte for byte.
- Global and group saves: the layout came from saves the SDK runtime wrote for test programs with every kind of
  global (ints, floats, strings, bools, one- and two-dimensional arrays, arrays of strings and structs, nested
  structs). Test Game 46's own system save loads into VNDegen and saves back byte-identical.
- Resume saves ("RSM", version 8): a VM snapshot. The layout came from test programs that each change one thing (a
  global, a local, a string, a struct, an array, call depth, a method call, values left on the stack by the caller),
  saved by the SDK runtime (version 7) and by Test Game 46's VM DLL (version 8). Version 8 adds the `this` page to each
  local page record. Code addresses are stored as the function's name, the call's offset in it and the function's
  CRC, so a load still finds them if the code moved.
- A save Test Game 19 wrote on the PC settled the rest. Its VM, a 2009 `Sys42VM.dll`, writes the GD container without
  the XOR and the resume save as version 7; VNDegen picks that format from the ini's `MainVM`. Struct records name
  the struct's constructor and destructor functions. A string record's length is the PC's buffer, which can run past
  the text's NUL. Saved reference counts leave out the reference each method frame holds on its `this`.
- Save slot details go through VSFile, whose files start "VSF", four zero bytes, then each value as a type byte (1 an
  int, 3 a NUL-terminated string) and its data, as Test Game 19's PC save shows.

### Data the sprite engine hands the game to save (`serial.cpp`, `hll_adv.cpp`)

Test Game 19 keeps the backlog, the back scenes, the character sprites and the MultiSprite state in int arrays inside
its saves. Their layouts were read from the arrays in the PC save:
- Values (`serial.cpp`): an int or bool is itself, a string its bytes one per element and a NUL, a struct its members
  in order, an array its length and elements. The backlog ("ADL"), the back scenes ("ADS") and the character sprites
  ("CSM") are built from these, each after a tag and a version 0.
- MultiSprite ("MST") is a byte stream copied into the ints four bytes at a time. The kind and order of its fields
  came from the routine that writes it (see Sources); the values were checked against the PC save.

A save the PC wrote loads in VNDegen, and VNDegen saves it back with the back scenes and MultiSprite data
byte-identical. A save VNDegen wrote from its own play loads on the PC, which then plays on.

### Sprite engine and the rest of the HLL libraries (`hll_*.cpp`, `gfx.cpp`, `audio.cpp`, `font.cpp`)

- SACT2 behaviour (sprites, Z order, blend rates, draw methods, text metrics and shadows, key codes, sound and music
  channels) follows the SDK's SACT2 reference. StoatSpriteEngine is SACT2's successor with the same names where they
  overlap; its own calls (MultiSprite, CharSprite, ADVLogList, ADVSceneKeeper, transitions) were read from how the
  games call them: their names, argument names, and the game functions that wrap them.
- DrawGraph (`hll_drawgraph.cpp`) follows the SDK's `IGraph.h` and `IGraph2.h` interface comments and the SACT2
  reference's `draw_graph` page; the readings taken where they are silent are commented in the file.
- Encodings these libraries hand the game to store in its saves (MultiSprite_Encode, CharSpriteManager_Save,
  ADVLogList_Save, ADVSceneKeeper_Save) follow the PC's; see the section above.

## Known gaps and open questions

- Four MultiSprite fields keep the PC's values without a known meaning. Moves in progress aren't saved, as on the PC.
- Resume saves are verified on the PC for Test Game 19, and against the SDK runtime and Test Game 46's VM; Test
  Game 56 isn't checked on the PC yet.
