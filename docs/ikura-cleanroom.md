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
| Raidy II: music names | `0600`'s `RD2%02X` and the routine that sets variable 0 before it; the PC's sound under Wine recorded from a null sink and matched to the decoded tracks by envelope correlation (0.98 for `RD229.OGG`, 0.19 for the track before) |
| Raidy II: movie | `RAIDY2OP.MPG` is MPEG-1 with Layer II sound; the shared `vnvideo` decoder (pl_mpeg, MIT) plays it |
| Raidy II: automap | Saves the original wrote (bit order and plane layout) and test scripts drawing the map with each command alone and with its arguments swapped |
| Raidy II: equipment | Black box only: test scripts set the equipment and base variables, ran `0f03` or `0f04`, and saved; the variables were read from the save files. The tables and formulas reproduce every variable in 98 compared saves |
| Raidy II: backlog | The game's `BACKLOG.SNR` and screenshots of the original's log page |
| Raidy II: saves (`RAIDY2R.SAV`) | Saves the original wrote under Wine and the game's own shipped save, walked field by field; test scripts in `SNR0` saved by both games' exes, whose slots VNDegen's match byte for byte but for the date and thumbnail; checked both ways |
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
  Where several labels name one line, the PC writes the highest, whether the script fell through to it or jumped to
  a lower one (a test script in both games' exes under Wine, and the PC's saves from Raidy II's opening).
- The names in a slot: the PC writes each file as the archive names it, with its extension, though Raidy II's
  scripts name most files bare (its saves at three points in the opening and town, test scripts, the shipped save's
  animation name). Its load page failed with "L2_OP open failed" on a slot that named the script bare.
- The slot's message mode and the two words after it: test scripts in `SNR0` ran `090d`, `090e`, `0911` and `0910`
  alone and in pairs before a message and saved on a right click bound with `0702`; the mode followed `090d`
  (1) and `090e` or `0911` (0) in both exes and never `0910`. The next word was the slot the last `090a` named. In
  Raidy the word after it followed the lines shown: k - 1 after one line k (0 to 20), 0 after lines 0, 1, 2 (also in
  the PC's own save at the intro's third line), 4 after 5, 9 and after 5, 9, 3; Raidy II wrote 0.
- The line in a slot: Raidy II's PC wrote the window's text only while `090d` held and the message had finished; a
  line waiting for its click, named or not, left it empty, as in its saves from the opening. Raidy's PC wrote it while
  the line waited. Both left it empty while the window was hidden.
- The layers in a slot: with layers 1, 3 and 5 swapped round a cycle (Raidy II's opening and a test script), the PC's
  layer records were VNDegen's in drawing order, and its order words the inverse of VNDegen's map; one swap of two
  layers, the only case checked before, looks the same both ways.
- Raidy II's `0212`: a content swap or a renumbering. Test scripts showed the content moves with the number, and the
  PC's saves showed each layer keeping its place in the drawing order; both together are a renumbering.
- Raidy II's window font: Wine draws a proportional face because the prefix has no MS Gothic. A Windows install has
  MS Gothic, so VNDegen draws the MS Gothic substitute the script names.
- A save made after the opening's logo replaced the desert's picture slot loads on the PC under Wine with the logo on
  black: the slot holds the logo, and a loaded layer is built from its slot. VNDegen's slots name the kept picture in
  a block after the state; the PC's own slot with that block appended loaded there as it did without it.
- When the global block is written: under Wine, Raidy II's PC played its ending from a crafted save and kept the
  clear flag (512) out of `RAIDY2R.SAV` until it quit from the title; its global block then matched, byte for byte,
  the one VNDegen writes when it closes after the same ending.
- Raidy II's layers after `0101` reloads their picture slot: the opening loads its logo into the slot the desert
  layer was built from, and the PC under Wine keeps showing the desert behind the logo and the lines that follow.
- Raidy II's `0716`/`0717` trailing arguments: the last one isn't the starting mode (the load page's slot buttons have
  0 and the script expects them enabled). A test script made three `0717` buttons with 1, 2 and 0 before it: the PC
  saved them as the buttons' state flags, as the town map's buttons in its own save show. The PC saved the last
  argument as the button's mode, which VNDegen doesn't follow because of the load page.
