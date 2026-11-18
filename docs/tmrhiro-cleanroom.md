# Tmr-Hiro runtime: clean-room provenance

VNDegen's Tmr-Hiro runtime is written from the game files, permissively licensed format code, and the original engine's
observable behaviour under Wine. No GPL, LGPL or unlicensed code was read; none exists for this engine.

## Sources

| Part | Sources |
|---|---|
| `.pac` archives | GARbro, `ArcFormats/Tmr-Hiro/ArcPAC.cs` (MIT, morkt/GARbro at b09ee45): the index layout in both versions, the nibble swap of script bodies and the test for it (a scene that starts with its 6-byte scene number record). The `_N` override order came from the exe's file name strings and the games' patch archives. |
| GRD pictures | GARbro's `ImageGRD.cs` (MIT): the header, plane order, RLE, the Huffman tree and LZ77. Checked by decoding every picture of the five games. |
| Music | GARbro's `AudioTmr.cs` (MIT): the 9-byte header of raw PCM. |
| Script records and opcodes | The games' own scripts read as data, and the PC engine under Wine for what an opcode does when the scripts don't settle it. |
| `_system` and the built-in screens | The settings in `_system` read as data, with the parts in `Agrd.pac`, compared with screenshots of the PC engine under Wine: the title, the map, the message window, the bars, the save, load and system screens, the confirm dialogs and the gallery. |
| Sprite placement | A PC screenshot under Wine, fitted to a pixel: the canvas stands at its offset unscaled and scales about its centre. Three sprites stand centre, left, right, from the layer order in PC saves. |
| Message window | PC screenshots under Wine: the window band's colour and opacity, the name and text positions, the cell widths and the read text colour. |
| Slot saves | Saves the PC engine wrote under Wine at different points, compared byte by byte; then VNDegen's saves loaded in the PC engine, and hybrids of the two, to see what the engine needs. The exe was not disassembled; four values in a save were looked up in its data section to learn that they are vtable pointers, which the engine doesn't need back. |
| `savedata.000` | The PC engine's file before and after reading lines, seeing CGs and entering a scene. |
| Voice settings per character | `[CHARACTER]` in `_system` read as data, and the PC engine's `savedata.000` diffed before and after changing them on its voice page. |
| Picture effects and layers | PC screenshots under Wine at the same records, fitted channel by channel: sepia is the grey average plus 40 red and 30 green, negative inverts each channel. |
| Movies | The shared vnvideo decoder (pl_mpeg and dr_mp3); the movie folder and the skip on click from the PC engine under Wine. |

## Libraries

stb_truetype and stb_vorbis (public domain or MIT), the shared `vnfont` and `casepath` code.

## Open questions and how they were settled

- Which records start the button wait: the wait starts after the last button record; the record after it only runs when
  the wait ends some other way.
- Condition operators: `==` and `!=` are certain from the scripts; the other four are guessed from where the scripts use
  them and haven't mattered in the first test game.
- Message text layout (Kyonyuu Elf Oyako Saimin and Welcome to the Mizuryuu kei land): measured from PC screenshots at
  1280x720. The first line starts at TEXT_DRAW_POS_Y plus the ruby height plus one, lines are TEXT_DRAW_NEXT plus that
  apart, and the name sits at NP_DRAW_POS_Y. Choices are centred half a row above SELECT_POSY.
- Elf's sound names ending in `_` stand for the `_A` and `_B` variants in `se.pac`; VNDegen plays `_A`. Which one the
  PC engine picks wasn't settled.
- Slot flags at offset 656: all 1 at a map, some 0 inside scenes, in both games. VNDegen keeps a loaded slot's flags
  and writes all 1 for a new game. The PC engine loads either.
- Hitozuma's damaged records: stepped through one line at a time under Wine; the PC engine goes from the record before
  a damaged one to the record after it.
- The 2014 slot size and defaults: a PC save from Hitozuma under Wine, diffed against VNDegen's, then VNDegen's loaded
  in the PC engine. Read marks and gallery flags in `savedata.000` sit at the same offsets as in the 2017 build.
- Scenes stored without their scene number record (Quickening's route scenes, Conception's views) are told apart
  by their call and jump records, whose first two bytes are 1 to 3 plain or 0x10 to 0x30 swapped. The test agrees with
  the scene number record on all 1,724 scenes that have one across the five games.
- Quickening's route map jumps to a label its scene doesn't have; VNDegen carries on at the next record, which only
  builds an empty placeholder button, so the map looks the same either way.
- `TEXT_BUTTON_TYPE_01=3` is a side menu at the right edge, `TWM_WIDTH_01` wide, shown when the pointer reaches it
  and kept while it stays over the items; an item's x counts from the panel's left edge. Quickening's and Conception's
  PC engines show their SAVE, LOAD, SYSTEM, TITLE and END there (Conception's manual says so too). Quickening's sheet
  doesn't line up with its settings, so its items show cut pieces of the words on the PC, and VNDegen draws the same.
- The map bar: the 2017 and later builds take it from `[MAPMENU03]`, not `[MAPMENU]`. Welcome to the Mizuryuu kei
  land's PC engine shows the `[MAPMENU03]` items (no quit button, which only `[MAPMENU]` has), and Quickening's, whose
  `MODE_MENU_TYPE01` is 2, shows no map bar at all, on its character maps and select maps alike, as with its side bar in
  the story. VNDegen shows the map bar only for type 1.
- Quickening's missing scene and its voice stored as `C04a11020002 `: a VNDegen save loaded in the PC engine just
  before each. The PC goes from the scene before the missing call to the map after it without a message, and at the
  voice line it reads nothing from `vic.pac` (seen with strace on its reads; it reads the voices on the lines either
  side).
- Line wrapping: a line holds `TEXT_MAX` half-width cells, counted from the narration indent, or for a spoken line
  from just after its opening bracket, and never past the window. A comma, full stop or closing bracket that would start
  a line stays at the end of the one before. Measured on the PC under Wine on long lines in Quickening (68 cells,
  spoken) and Conception (62 cells, narration and spoken).
- `c04a_name`, `SEH_V_09_I01_A` to `I03_A` are named by Welcome to the Mizuryuu kei land's scripts but are in none of its
  archives, so the PC engine plays nothing there either.
