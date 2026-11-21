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
| E-mote characters | eluna (MPL-2.0, https://github.com/xmoezzz/eluna, `Engines/ANIM/third_party/eluna_rs`), behind VNDegen's `vnemote` C interface; the vertex, blend and stencil handling follows eluna's player (MIT). The PSB key was found from the game's own PSB files with eluna's key search. Positions, the global scale (`66`), the slot defaults (`53`) and the character scale table in `_define.dat` from the scripts and the define, checked against screenshots of the original under Wine |
| E-mote fidelity | Fixes to eluna (listed in `Engines/ANIM/UPSTREAM.md`) found from eluna's own source, the PSB data (mesh combinator meshes, frames, masks) and side-by-side screenshots of the same saves in VNDegen and the original under Wine, with the original's idle held still by an idle name it doesn't know. The placement of characters and event models (100 pixels times the table's percentage) was fitted to those screenshots |
| Idle motion | The define's type-10 records (an idle name, then variable, minimum and maximum), the idle name the saves keep per layer, and 5 to 30 fps recordings of the original under Wine, with and without input, measured per character with optical flow |
| Save and Load cards | DELETE, Reset Saves and the hover labels from the button atlas; what they do from save files diffed before and after using them in the original under Wine |
| Expressions | `_define.dat`'s presets (a name and variable values), matched to the values the original's saves store |
| Saves | Saves the original made under Wine at chosen points, diffed: the date, comment, text position, background, layers, local variables, the quick save file and the thumbnails; VNDegen's saves loaded in the original and the original's in VNDegen |
| Text | stb_truetype (public domain or MIT) with the shared Windows font substitutes (`docs/fonts.md`) for the exe's Arial. Glyph advances rounded up and the wrap width from line widths and wrap points measured on screenshots of the original under Wine |
| File name case | `Engines/Shared/casepath.c`, VNDegen's own |

The exe was only run, and its strings read for file names (`cg\psbpack.dat`, `%s_save.dat`) and the fonts it asks
for. No code was read.

Sources not used: FreeMote is licensed CC BY-NC-SA 4.0, which isn't permissive, so its code wasn't opened.
