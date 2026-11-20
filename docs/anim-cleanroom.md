# ANIM runtime: where the knowledge came from

The ANIM runtime (`Packages/VNDegenKit/Sources/CANIM`, hosts in `Engines/ANIM/host`) was written for VNDegen. It
derives from no GPL or LGPL code. Its sources:

| Part | Sources |
|---|---|
| Encryption of `<name>_sce.dat`, `_define.dat`, `_system.dat` and `cg/*.gax` | GARbro `ArcFormats/Crowd/ImageGAX.cs` (MIT): the `00 00 00 01` tag, the 16-byte key and the rolling XOR. That it also covers the three script files was found by decoding them |
| Pictures (`.gax`) | The same GARbro file: PNG inside the encryption. Pictures are 2048 by 2048 textures with the image at the top left |
| Voice and sound archive (`voice/<name>_wave.pck`) | GARbro `ArcFormats/Crowd/ArcPCK.cs` (MIT): a count, 12-byte records, Shift-JIS names |
| Sound (`.eog`) | GARbro `ArcFormats/Crowd/AudioEOG.cs` (MIT): an 8-byte `CRM` header before Ogg Vorbis. Decoded with stb_vorbis (public domain or MIT) |
| E-mote archive (`cg/psbpack.dat`) | Read from the file: a 64-bit count, (offset, size, 0) records and the names |
| Script container | Read from the decoded `_sce.dat`: the code size and the end of the label table, the code, (name, address) labels and the string pool |
| Bytecode | Tokenizing all of Double Date Dilemma's and Magical Sword's code. The string pool is in code order, so each string's index marks where an operand is, and the labels mark where instructions start; a sweep that must land on every such point settled each opcode's operand layout. The typed operands (`40` and `41` with a variable number, `43` with a constant) and the meaning of each opcode from how the scripts use them: the boot sequence, the waits, the click wait, the scene changes, the choices |
| Sprite names | The decoded `_define.dat`: 524-byte records that map a sprite name to its picture file |
| Title screen | The title's pictures and their frames found in the atlases; the button positions and the hover frames from a screenshot of the original under Wine, matched numerically |
| Text | stb_truetype (public domain or MIT) with the shared Windows font substitutes (`docs/fonts.md`) for the exe's Arial |
| File name case | `Engines/Shared/casepath.c`, VNDegen's own |

The exe was only run, and its strings read for file names (`cg\psbpack.dat`, `%s_save.dat`) and the fonts it asks
for. No code was read.

Sources not used: FreeMote is licensed CC BY-NC-SA 4.0, which isn't permissive, so its code wasn't opened.
