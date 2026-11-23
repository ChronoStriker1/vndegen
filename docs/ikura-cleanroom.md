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
| Raidy II: cells, layers, buttons | The game's scripts and GGP headers; test scripts in `SNR0` run by the original under Wine and recorded at 20 frames a second (layer swaps and the drawing order, range show and hide, jumps on a layer's state, switch calls, indirect variables, flag copies) |
| Raidy II: animations (`GAN`) | GARbro `ArcFormats/Ikura/ArcGAN.cs` (MIT): the frame index and the run-length key and delta frames. The program's timing (40 ms units) and the layer they draw on from recordings of the original under Wine; the decoded frames matched its output to a mean difference of 0.12/255 |
| Raidy II: movie | `RAIDY2OP.MPG` is MPEG-1 with Layer II sound; the shared `vnvideo` decoder (pl_mpeg, MIT) plays it |
| Raidy II: automap | Saves the original wrote (bit order and plane layout) and test scripts drawing the map with each command alone and with its arguments swapped |
| Raidy II: equipment | Black box only: test scripts set the equipment and base variables, ran `0f03` or `0f04`, and saved; the variables were read from the save files. The tables and formulas reproduce every variable in 98 compared saves |
| Raidy II: backlog | The game's `BACKLOG.SNR` and screenshots of the original's log page |
| Raidy II: saves (`RAIDY2R.SAV`) | Saves the original wrote under Wine and the game's own shipped save, walked field by field; checked both ways |
| Raidy II: read marks | The shipped save's marks block against each script's `090a` slot and line numbers; lines cleared in the block and played under Wine (where Skip stopped, when the PC wrote the block back), and the skip setting and Nonstop changed one at a time. Raidy's PC, run the same way, wrote no marks and skipped unread lines |

The exe was run under Wine for nearly everything. Its machine code was read in two places only, for facts: the
`CreateFontA` call's arguments and the table its sound code passes to `SetVolume`. VNDegen computes the curve that
table follows rather than copying it.

For Raidy II the exe's machine code wasn't read at all.

Things that were unsure and how they were settled:

- `0205`: not visibility of a layer's colour but whether the layer shows at all; a layer also needs `0208` before it
  draws (tested with layers that differ in one operation each).
- Buttons draw only while the engine waits for input (a test with no wait drew none).
- The comparison codes: first read from the scripts' own loops and limits (a level-up cap, the MP check before a
  spell), which left 1 and 5 the wrong way round: Raidy II's music routine never changed track. A test `START.SNR` in
  `SNR0` then ran every code on 1:2, 2:2 and 3:2 and saved a slot, in both games' exes under Wine; the slot's
  variables gave 0 `==`, 1 `<`, 2 `<=`, 3 `>`, 4 `>=`, 5 `!=` in both.
- The dungeon's camera: two faces of one wall seen from two cells give the distance and focal length; the texture's
  mortar spacing confirmed them.
- The save state's resume point: the last label the main script reached, as both a message and a dungeon save show.
- Raidy II's `0212`: a content swap or a renumbering. Test scripts showed the content moves with the number, and the
  PC's saves showed each layer keeping its place in the drawing order; both together are a renumbering.
- Raidy II's window font: Wine draws a proportional face because the prefix has no MS Gothic. A Windows install has
  MS Gothic, so VNDegen draws the MS Gothic substitute the script names.
- When the global block is written: under Wine, Raidy II's PC played its ending from a crafted save and kept the
  clear flag (512) out of `RAIDY2R.SAV` until it quit from the title; its global block then matched, byte for byte,
  the one VNDegen writes when it closes after the same ending.
- Raidy II's layers after `0101` reloads their picture slot: the opening loads its logo into the slot the desert
  layer was built from, and the PC under Wine keeps showing the desert behind the logo and the lines that follow.
- Raidy II's `0716`/`0717` trailing arguments: the last one isn't the starting mode (the load page's slot buttons have
  0 and the script expects them enabled); both are accepted and ignored.
