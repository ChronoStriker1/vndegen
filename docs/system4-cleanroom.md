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
- **Minimal disassembly** of the SDK's `Sys42VM.dll`, only where a file format couldn't be settled otherwise: the
  routine that writes a save file's "GD" header and the XOR routine it calls (see Saves).
- **Permissive code**: stb_image and stb_vorbis (public domain or MIT), pl_mpeg (MIT), zlib, FreeType on the Linux
  test host only. The text and font code is adapted from VNDegen's own Wolf RPG Editor runtime.

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

### Sprite engine and the rest of the HLL libraries (`hll_*.cpp`, `gfx.cpp`, `audio.cpp`, `font.cpp`)

- SACT2 behaviour (sprites, Z order, blend rates, draw methods, text metrics and shadows, key codes, sound and music
  channels) follows the SDK's SACT2 reference. StoatSpriteEngine is SACT2's successor with the same names where they
  overlap; its own calls (MultiSprite, CharSprite, ADVLogList, ADVSceneKeeper, transitions) were read from how the
  games call them: their names, argument names, and the game functions that wrap them.
- Encodings these libraries hand the game to store in its saves (MultiSprite_Encode, CharSpriteManager_Save,
  ADVLogList_Save, ADVSceneKeeper_Save) are VNDegen's own for now. See Known gaps.

## Known gaps and open questions

- The encodings above should match the PC's, because the game stores them in its saves. VNDegen can't yet run the PC
  game's sprite library to learn them.
- Resume saves are verified against the SDK runtime and Test Game 46's VM, not yet against Test Games 19 and 56 on
  the PC.
