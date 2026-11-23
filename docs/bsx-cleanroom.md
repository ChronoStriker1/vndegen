# BSX runtime: where the knowledge came from

The BSX runtime (`Packages/VNDegenKit/Sources/CBSX`, hosts in `Engines/BSX/host`) was written for VNDegen. It derives
from no GPL or LGPL code. Its sources:

| Part | Sources |
|---|---|
| Archives (`.bsa`) | GARbro `ArcFormats/Bishop/ArcBSA.cs` (MIT): the header, the two record layouts and the folder records |
| Pictures | GARbro `ArcFormats/Bishop/ImageBSG.cs` and `ArcBSC.cs` (MIT): the `BSS-Graphics` header, the colour and compression modes, RLE, LZ with its delta, the `BSS-Composition` part list. Bottom-up rows, palette-less hit tables, the message window's parts and how bust-up names map to body and face files from the game's files and screenshots of the original under Wine |
| Script container and bytecode | The games' `bsxx.dat`: the header and tables from their offsets and sizes; the opcodes and operand encoding from tokenizing all of Mesu Kyoushi 1's and 2's code and reading how the scripts use each (the day loop, the map and card code, the eyecatch, the scenes). The comparisons from the day loop's logic |
| Commands | The scripts' arguments (picture names, kinds, positions, sound names, durations), `bsxx.cfg`, and screenshots of the original under Wine for the window, bust-ups, transitions, the eyecatch tint, STRIP and VIEW |
| Engine screens | `bsxx.cfg`'s sections, the screens' pictures, `define.bsa` (`deal`, `deal2`, `status_layout`), `map.dat`, the game's manual, and screenshots of the original under Wine |
| Map | `map.dat` read from the file; the hit table's region numbers matched to the places by their positions; the result from the script's checks after `map` |
| Save files | Diffs of quick saves the original wrote under Wine at chosen points, saves edited to jump to a label and loaded in the original, and loading VNDegen's saves in it; the opacity byte found by swapping blocks between a working and a failing save |
| `common.dat` | Its sizes against the script's choice and message counts, and diffs before and after play. The fresh file each of the four games' PCs writes under Wine when it quits with no `common.dat` (it writes none earlier), which VNDegen's matches byte for byte. The settings: each option changed in the PC's settings screen (and the BGV room's volumes) in batches, the PC quit and its file diffed, in all four games; the PC then shown VNDegen's file, its settings screen within 1.4 grey levels of VNDegen's. The voice modes: a save VNDegen made at a voiced line loaded in the PC with music muted, the sound recorded through a click mid-voice into an unvoiced line, for modes 0, 2 and 3, and the runner's sound matched |
| Sound | stb_vorbis (public domain or MIT) |
| Text | stb_truetype (public domain or MIT) with the shared Windows font substitutes (`docs/fonts.md`), condensed to the cfg's character width as the original draws MS Gothic |
| BSXScript 3.1 | Mesu Kyoushi 4's script tokenized with the 3.0 model; the new opcodes' operands from where decoding broke and what follows (`3e`'s string ids are voice names), their meaning from context (`2a` after impacts, `30` names in the loop voice folders). `bsxx.cfg`'s `[bu color adjustment]` comment, whose Shift-JIS survives in fragments that spell the blend names. The shake, text placement, VIEW defaults, the map's menu bar and the save layout from recordings, screenshots and saves of the original under Wine, compared numerically with VNDegen's |
| BSXScript 3.3 and Lua | Mesu Kyoushi 3's script and `embed.bsa` scripts read as data. The exe's packed image unpacked as data to look for the Lua modules (below), its strings read for the Lua version (5.2.0) and the host function names, and the running original's memory searched for the module text, data only. Achievements, the slot viewer and the save layout from the original under Wine: VNDegen's `common.dat` unlocks shown by the PC, its saves loaded in the PC, screenshots compared numerically |
| Bust-up editor | `museum_buex_layout`, `bsxx.cfg`'s `[free-sight screen]`, `[bustup setting]` and `sfx bg`, the bust-up archives' names and composition headers, the PC's `buex_dsNN.dat` files and screens under Wine compared numerically with VNDegen's |
| Museum | `cg.dat` and `scene.dat` read from the files against the pictures' names; `common.dat`'s seen, scene and playlist records from diffs of the PC's file before and after unlocking and editing playlists under Wine, and VNDegen's records loaded in the PC; `@museum_state` from `bsxx.dat`'s persistent variable table and its value in `common.dat`; `bgv_layout` and `bsxx.cfg`'s museum sections; the viewer, playlist and BGV screens, their button frames, the thumbnail dimming and the `m` blur masks from screenshots of the original under Wine compared numerically with VNDegen's; the completion rates from the PC's achievements with records set and cleared |
| Movies | Engines/Shared/vnvideo (pl_mpeg, MIT) |
| File name case | `Engines/Shared/casepath.c`, VNDegen's own |

The exe was only run, and its strings read for file names (`save\no%03d`, `common.dat`). No code was read.

Unpacking Mesu Kyoushi 3's exe. `mk3ie.exe` is packed with MPRESS. avast/RetDec's MPRESS plugin (MIT) describes the
section data: `.MPRESS1` starts with the image size shifted right by 12 (u16) and the packed size (u32); MPRESS packs
with LZMA or with MATCODE's LZMAT, which is GPL and isn't used here. In the LZMA mode two bytes follow, lp | pb << 4
and lc, then a raw LZMA stream. VNDegen's public-domain LZMA SDK decoder (`RuntimePackages/CLzmaDec`) decodes Mesu
Kyoushi 3's section exactly, from the PE section table alone, without the unpacking stub. The decoded image was only
searched as data. Mesu Kyoushi 3 turned out not to need it (the Lua modules it would have held aren't in the release),
so VNDegen doesn't unpack it; the route stays here for another game. The Lua modules were looked for in the running
original under Wine by reading its process memory for their text, data only, with no code read, traced or stepped.

Things that were unsure and how they were settled:

- Which save field is the program counter: a save edited to a label's offset loaded in the original and ran that scene.
- The save id: it isn't kept in `common.dat` or any file and doesn't change when `bsxx.dat` or `bsxx.ini` change, so
  VNDegen reads it from the game's own saves, with the four Mesu Kyoushi games' ids built in.
- What STRIP shows: the original cycles normal, underwear and nude, which for each character is the highest wear it
  has at or below the strip table's 0, 4 and 6.
- Command `24`: it comes right before the eyecatch's red picture, and the original shows the background tinted red,
  so it multiplies the next picture.
- Command `26`: it sets the message window's visibility around transitions.
- Auto saves: the original wrote `no000` on reaching the map; the manual says choices save too. Mesu Kyoushi 1's PC wrote
  `no000` again on each of three map visits in a row, and Mesu Kyoushi 1's and 4's took a manual save in slot 1; VNDegen had
  spread its auto saves over the slots below `no begin` and refused manual saves there. Mesu Kyoushi 4's
  original wrote `no000` with mode 9 as its custom map opened. Both PCs' map auto saves picture the drawn map; VNDegen
  had saved before drawing it, so Mesu Kyoushi 1's picture came out black on the Mac. Mesu Kyoushi 4's PC also wrote
  mode 9 for a manual save from the map's menu bar and loaded it onto the map with no text; VNDegen had written the save
  screen's phase as mode 1 and loaded the last line over the map.
- Picture record sizes: Mesu Kyoushi 4's saves hold each picture's size (an icon frame is 80x80), the frame count (2
  for icons), and a background's two parts only with a face; 3.0's 800x600 matched because its pictures were
  full-screen.
- The save tail: three flags at bytes 3, 7 and 11 match the window, menu bar and information window layers that `26`
  hides (all clear in a scene, window and information window set on Mesu Kyoushi 4's map).
- The word after the message position: Mesu Kyoushi 4's saves hold the history ring's write position there, Mesu
  Kyoushi 1's hold 0, so 3.0 saves keep writing 0.
- The window flag in the tail follows the script's `26 0`, not the window a phase hides: Mesu Kyoushi 1's map save
  keeps it clear.
- The 3.1 text placement: the original's glyph rows sit 6 px lower and 20 px apart against VNDegen's 24 px, with a size
  of 14 and a height of 20. Mesu Kyoushi 3's height of 32 left its glyphs the same as 4's, so the glyph size follows
  the size and the height is the pitch.
- The achievement flag: VNDegen's `common.dat` with ID00 and ID04 set to 1 showed both unlocked in the original.
- Slot numbers: the original shows `no000` as No. 0.
- Repeated cfg keys: Mesu Kyoushi 3 sets `sort button0` twice, and the original's radio sits at the second value.
- Lamps: the original draws Mesu Kyoushi 3's lamp diamonds under the button text.
- The strip rate: Mesu Kyoushi 3's PC unlocked ID46 (strip rate 100) with every wear byte set and kept it locked with
  one cleared, so the rate counts the wear records' bytes.
- Playlist entries: a VNDegen playlist holding page 1 index 0 and page 0 index 2 showed those two scenes, in order, in
  the PC's playlist.
- The editor's state file: diffs of `buex_dsNN.dat` the PC wrote after each single change under Wine (Mesu Kyoushi 1
  and 3); every one loads in VNDegen and is written back identical, and its thumbnail compares with VNDegen's at 2.7
  (Mesu Kyoushi 1) and 8.8 (Mesu Kyoushi 3, mostly the bust-ups' sharpness) mean difference.
- The dress screen, under Wine (Mesu Kyoushi 4 at Miyu's first line, Mesu Kyoushi 3 from a save): saves made after each
  change gave the record bytes (下着 on 普段着 wrote ch5 byte 1 = 4; 全服装一括 at 全裸 and 下着 wrote byte 0 = 6 and 5;
  全体 at 下着 and 全裸 wrote 5 and 6 in the play head's 14th byte, and 個別 wrote 0; Mesu Kyoushi 3's 全裸, row 4 with
  `ngng convert table` 0, 1, 2, 3, 5, wrote 6). A save kept its bust-up as `bu05f4012b12` and, loaded later, the
  next close-up came up in underwear and the page showed 下着. No script line in Mesu Kyoushi 3 or 4 names a bust-up
  with an undress digit above 0, so the dress state is the only thing that undresses a bust-up. The figure's place
  came from diffing a page with and without it: `co054012` (332x576 at 102,100 in a 550x700 canvas) at 339,134. A
  save VNDegen wrote at 全裸 on 普段着 loaded on the PC with Miyu undressed and the page at 全裸.
- The editor's rows: Mesu Kyoushi 1's layout comments name its first two rows the other way round; its panel labels
  and saved states show the same pose, view, undress, outfit, underwear, face order as Mesu Kyoushi 3.
- The editor's close-up placement: scanning offsets against the PC's thumbnails gave the canvas centring and, from a
  bust and a hip state, the sight's line `H + top + sight * (height - H)`; Mesu Kyoushi 1 put the face position at the
  top.
- Thumbnail brightness: the PC's thumbnails fit 0.49 times VNDegen's full thumbnail plus a constant, in Mesu Kyoushi 1
  and 3, and come to full brightness under the pointer.
