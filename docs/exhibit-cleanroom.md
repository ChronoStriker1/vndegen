# ExHibit runtime: where the knowledge came from

The ExHibit runtime was written for VNDegen. It derives from no GPL or LGPL code. This file lists its sources and,
for the one place where the engine's code was read, what was looked at and what was learned.

## Sources

| Part | Sources |
|---|---|
| Pictures (`.gyu`) | GARbro `ArcFormats/ExHibit/ImageGYU.cs` (MIT): the header, the key, the ten byte swaps from a Mersenne Twister with the old 69069 seeding (GARbro `MersenneTwister.cs`, MIT), LZSS or GYU's bit-coded LZ, the separate alpha plane |
| Script container (`.rld`) | The jialiang/shirogane-analysis write-up (a README only, read as prose, no code): the `\0DLR` header and each command's packed word of opcode, number count and string count. This game's files are version 3 and encrypted, which the write-up doesn't cover; see below |
| Script encryption | `resident.dll`'s script read path, read with Ghidra as described below |
| Script commands | The game's own scripts, the command names in `resident.dll`'s debug strings (a table of `CMD_` names in opcode order), and the original under Wine as a black box |
| Music encryption | `resident.dll`'s Ogg Vorbis open path, read with Ghidra as described below |
| Extra (CG, Scene, Music) | The game's scripts and layout files (`cgMode.rld`, `musicMode.rld`, `def.rld`, `res/c/replay1.rnf`, `res/c/cgmode.type.rnf`), its pictures, and the original under Wine as a black box |
| CG viewer controls | `res/c/cgmode.type.rnf` (the buttons, counter and side arrows, their pictures and places), the gallery pictures' sizes, and the original under Wine for which controls are active |
| Backlog Jump | `res/c/backlog.layout2.rnf` (the Jump buttons, the row rectangles and the highlight picture), `ExHIBIT.ini`'s `N_HISTORY`, and the original under Wine for what Jump does |
| Confirm boxes | `def.rld`'s dialog definitions (the panel picture and place, the YES and NO buttons), and the original under Wine for which actions ask |
| Seen pictures and scenes (`system.dat`) | The original under Wine as a black box: diffs of `system.dat` as it played, and Extra after single registers were cleared or set in the file. The table sizes are the `N_CG`, `N_SCENE` and `N_SOUND` values of `ExHIBIT.ini`, which the file repeats in registers 189 to 191 |
| Emotion icons (`.dlt`) | The header and frame loop of `resident.dll`'s DLT decoder, read with Ghidra as described below; not implemented |

## The script read path in `resident.dll`

Encrypted version 3 scripts can't be read without their key, and no permissive source describes it. The coordinator
approved a minimal look at the engine's script read path to learn the file format. Only these functions were read,
in Ghidra's decompiler output: the script loader (`RetouchSystem::loadRld`), the decrypt routine it calls and its
table setup, the random number generator's seeding and next-number functions, the script command that sets crypt
keys (`cmdDefCryptKey`), and the part of the engine's start-up (`liteInit`) that stores the two keys. No code or
instruction listing was copied; what was learned is this:

- A script's first 16 bytes (the magic, the version, the commands' offset and count) are plain. Everything after them
  is XORed as little-endian 32-bit words, up to 0x3FF0 words; anything past that is plain.
- Word `i` is XORed with entry `i mod 256` of a 256-word table and with the 32-bit key itself.
- The table is the first 256 outputs of a Mersenne Twister seeded with the key, using the old seeding where each
  state word comes from two steps of the 69069 linear congruential generator, which GARbro's MIT `MersenneTwister.cs`
  also implements for GYU pictures.
- There are two keys. Scripts whose name starts with `$`, or that the engine loads as definitions (`def.rld` here),
  use a fixed key, 0xAE85A916. Every other script uses a key the engine builds at start-up from 32 bits hidden in the
  low bits of one column of a bitmap resource in the game's exe, XORed with a value computed from the game's ini file.
  The routine that computes the ini value was not read: the coordinator's rule was to stop at the read path.
- `cmdDefCryptKey` fills other tables from a list of numbers in a script command; it doesn't set the script keys.

Checked against the files: `def.rld` decrypts with the fixed key into commands that parse to the end of the file. In
every script the 256 bytes from offset 16 to the commands at offset 272 are zeros once decrypted, apart from the first
few words of scene scripts, which name their library. `def.rld` holds one more command than its header's count, a call
to `defChara`, so the reader parses to the end of the file.

## The other scripts' key

Because the routine that derives the other key wasn't read, the runtime uses the fallback the coordinator set and finds
the key from the game's own scripts. The 64 words after a script's header are zeros in plain text, so their encrypted
values are the table entries XORed with the key. The runtime tries the fixed key, then any key it found before, and
otherwise searches all 2^32 keys for one whose generator output matches word 5, and checks the rest. The search takes
about 1.5 seconds on eight threads and runs once: the key is kept in the game's data folder. For this game it finds
0x5E8C3301, which decrypts all 76 other scripts so that each parses to its end. Scene scripts' first header word is 1
and their next bytes name the library script whose functions they call.

## The music read path in `resident.dll`

The music files (`res/s/m/*.ogg`) don't start with an Ogg page, while voices and effects are plain. The coordinator
approved the same minimal look at how the engine opens and decrypts an `.ogg`. Only these were read: the Vorbis
decoder's open function, the two functions that feed file bytes to libogg, the decoder's virtual method that the open
function calls on the first block, and the decoder's vtable to find that method. Nothing was copied. What was learned:

- When the decoder opens a file it reads the first 4096 bytes into libogg's buffer and, if the decoder has a key, calls
  a decrypt method on them as 32-bit words. Later reads are passed through as they are.
- The decrypt method takes each little-endian word, rotates it left by 2, 15, 7, 11, 1, 15, 23 and 3 bits in turn (the
  pattern repeats every eight words), and XORs the 32-bit key. It stops after a word count kept in the decoder.

Where the key and the count are set was not read: that lies outside the open path, and the coordinator's rule was to
stop there. Both come from the files instead. The first plain word of an Ogg file is always `OggS`, so a file's key is
its first word rotated left by 2, XORed with `OggS`. The count is 128 words: with it every music file's first pages
pass their Ogg CRC checks, and with any other count from 16 to 1024 they don't. All 21 files open this way.

## The emotion icons (`.dlt`)

The emotion icons that MANPU attaches to characters (sweat drops, exclamation marks) are `.dlt` animations under
`res/g/ch/mp`. They are compressed and no permissive source describes them. The coordinator approved a minimal look at
the read and decode path, with the same rules as above. Only these were read: the decoder's constructor, the routine
that checks a file's header, and the routine that advances to the next frame. Nothing was copied. What was learned:

- The first 32 bytes are a header: a version (0x103), the frame width and height, the frame count, a format word,
  the frame time in milliseconds, a zero, and the signature `DLT` followed by a zero byte.
- Each frame is a record whose type selects how it is applied. Most frames are deltas against the previous frame,
  with an optional separate alpha plane, and some types mark loops and key frames.
- The frame data isn't unpacked by the decoder itself. It calls a decompressor object through a virtual method, and
  that object is chosen elsewhere at run time. The deltas are then applied by several separate blitting routines.

Following the decompressor and the blitters would mean reading a good deal more of the engine for a decorative
effect, so the reading stopped there. The runtime doesn't draw these icons; the scenes play the same without them.
