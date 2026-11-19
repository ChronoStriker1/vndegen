# ExHibit runtime: where the knowledge came from

The ExHibit runtime was written for VNDegen. It derives from no GPL or LGPL code. This file lists its sources and,
for the one place where the engine's code was read, what was looked at and what was learned.

## Sources

| Part | Sources |
|---|---|
| Pictures (`.gyu`) | GARbro `ArcFormats/ExHibit/ImageGYU.cs` (MIT): the header, the key, the ten byte swaps from a Mersenne Twister with the old 69069 seeding (GARbro `MersenneTwister.cs`, MIT), LZSS or GYU's bit-coded LZ, the separate alpha plane |
| Script container (`.rld`) | The jialiang/shirogane-analysis write-up (a README only, read as prose, no code): the `\0DLR` header and each command's packed word of opcode, number count and string count. This game's files are version 3 and encrypted, which the write-up doesn't cover; see below |
| Script encryption | `resident.dll`'s script read path, read with Ghidra as described below |
| Script commands | The game's own scripts, and the original under Wine as a black box |

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
every script the 256 bytes from offset 16 to the commands at offset 272 are zeros once decrypted. The file holds one
more command than its header's count, a call to `defChara`, so the reader parses to the end of the file.

## The other scripts' key

Because the routine that derives the other key wasn't read, the runtime uses the fallback the coordinator set and finds
the key from the game's own scripts. The 64 words after a script's header are zeros in plain text, so their encrypted
values are the table entries XORed with the key. The runtime tries the fixed key, then any key it found before, and
otherwise searches all 2^32 keys for one whose generator output matches word 5, and checks the rest. The search takes
about 1.5 seconds on eight threads and runs once: the key is kept in the game's data folder. For this game it finds
0x5E8C3301, which decrypts all 76 other scripts so that each parses to its end. Scene scripts' first header word is 1
and their next bytes name the library script whose functions they call.
