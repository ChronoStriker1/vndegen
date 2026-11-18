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
- `c04a_name`, `SEH_V_09_I01_A` to `I03_A` are named by Welcome to the Mizuryuu kei land's scripts but are in none of its
  archives, so the PC engine plays nothing there either.
