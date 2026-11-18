# Ikura GDL runtime: where the knowledge came from

The Ikura GDL runtime (`Packages/VNDegenKit/Sources/CIkura`, hosts in `Engines/Ikura/host`) was written for VNDegen.
It derives from no GPL or LGPL code. Its sources:

| Part | Sources |
|---|---|
| Archives (`SM2MPX10`) | GARbro `ArcFormats/Ikura/ArcDRS.cs` (MIT): the entry layout. The patch archives (`SNR0` to `SNR9`) from the exe's strings and a test archive the original read |
| Pictures (`GGP`) | GARbro `ArcFormats/Ikura/ImageGGP.cs` (MIT): the XOR key. PNG decoding by stb_image (public domain or MIT) |
| Sound | stb_vorbis for Ogg, VNDegen's own WAV reader. The volume curve and default (byte 128, -10 dB) from the PC's output recorded under Wine, which matched the decoded music at correlation 1.0 |
| Script format and opcodes | The game's own scripts, disassembled with a tool written for this work, and test scripts the original ran from a patch archive (`SNR0`) under Wine: layer visibility and fills, button drawing, comparisons, fades |
| Messages | Screenshots and frame recordings of the original under Wine: text origin, pitch and wrapping, the shadow, typing speed, the cursor's frames, the name plates' alpha test |
| Fonts | The scripts name Times New Roman; the exe's `CreateFontA` call was read for the weight it asks for. stb_truetype with the shared substitutes (`docs/fonts.md`) |
| 3D dungeon | The ZMZ grid compared with the game's own map pictures; the view's geometry and lighting fitted to screenshots of the original at known positions, and the animation timing to frame recordings |
| Saves (`RAIDYUS.SAV`) | Diffs of files the original wrote under Wine at known points, options changed one at a time, and transplants of single sections between files. Checked both ways: the original's slots load here, ours load there |
| File name case | `Engines/Shared/casepath.c`, VNDegen's own |

The exe was run under Wine for nearly everything. Its machine code was read in two places only, for facts: the
`CreateFontA` call's arguments and the table its sound code passes to `SetVolume`. VNDegen computes the curve that
table follows rather than copying it.

Things that were unsure and how they were settled:

- `0205`: not visibility of a layer's colour but whether the layer shows at all; a layer also needs `0208` before it
  draws (tested with layers that differ in one operation each).
- Buttons draw only while the engine waits for input (a test with no wait drew none).
- The comparison codes: `<=`, `>`, `>=` and `<` were settled by the scripts' own loops and limits (a level-up cap, the
  MP check before a spell).
- The dungeon's camera: two faces of one wall seen from two cells give the distance and focal length; the texture's
  mortar spacing confirmed them.
- The save state's resume point: the last label the main script reached, as both a message and a dungeon save show.
