# IPAC

VNDegen runs IPAC games on a runtime of its own, `Packages/VNDegenKit/Sources/CIPAC`, written in C from the games'
files, GARbro's MIT-licensed IPAC readers and the original engine's behaviour under Wine. It is MIT licensed like the
rest of the app. `docs/ipac-cleanroom.md` lists where each part came from. No other runtime for the engine exists. The
engine names itself only by its archive signature, `IPAC`; Vanadis's Mamono Musume to no Seikatsu series uses it.

## Test games

| Game | Engine build | Notes |
|---|---|---|
| Mamono Musume to no Seikatsu: Lamia no Baai (魔物娘との性活～ラミアの場合～) | `Lamia_girl.exe`, 2009-07 | Japanese only. Plays through every route to its endings, the CG gallery and the scene recall |

The second library game in the series, Alraune no Baai, is left out (see the content rule in the project's private
notes). The game is 800x600.

## How the games are built

A game folder holds the exe and `DATA/`: `BG.PAK`, `BG2.PAK` and so on (backgrounds and event CGs), `Chr.PAK`
(characters), `Parts.PAK` (the interface, transition masks and gallery sheets), `Scr.PAK` (scripts), `SE.PAK` and
`Voice1.PAK` to `Voice4.PAK`, and `DATA/BGM/BGMnn.WST`. The plugin takes a folder as an IPAC game when `DATA/Scr.PAK`
and `DATA/Parts.PAK` are side by side, up to one folder down.

- An archive is `IPAC`, a 16-bit entry count, and 44-byte entries (a 32-byte Shift-JIS name, an offset and a size).
  An entry is stored as is, or as `IEL1`, its unpacked size and LZSS (a 4 KB window that starts filled with spaces,
  at 0xFEE). The engine also reads loose files from `DATA/BG/`, `DATA/CHR/`, `DATA/PARTS/` and `DATA/SCR/` before the
  archives, and so does VNDegen.
- Pictures (`IES2`) are 800x600 or smaller: 24-bit BGR rows top-down followed by an 8-bit alpha plane, or 8-bit with
  a palette. Characters are full-screen canvases placed at offsets -200, 0 and +200 for the left, centre and right.
- Sound: `WST2` music is stereo 44.1 kHz MS ADPCM with its coefficients in the header; voices and effects are RIFF
  WAV, mostly MS ADPCM. VNDegen decodes both with dr_wav.
- Scripts (`.org`) are Shift-JIS text: `#COMMAND args` lines, `[picture]` to change the background, `:label`,
  `-A000100008` voice lines, `【name】`, plain text lines that make up a message until a blank line, `?` followed by the
  options of a choice and `1 { ... }` branches, `<script:label>` jumps, and one-letter lines for the three character
  slots: `L`, `C` and `R` show a character with a fade, `K`, `X` and `E` swap the left, centre or right one at once,
  and `R C` slides a character one slot toward another. `~base$reading$` is ruby. The engine runs at 60 frames a
  second: `#WAIT` counts frames and `#SPEED n` makes a transition take 256/n frames. Flags (`F000` to `F999`) and
  system flags (`W000` up) are bytes.
- The title and the gallery pages are scripts that read the colour under the pointer from a click map (`#CLICK`,
  `CLK`). The message window, choices, save and load pages, configuration, backlog and exit dialog are the engine's
  own, drawn from sheets in `Parts.PAK` whose hit maps (`MesWindowA`, `DataA`, `configA`, `SysEndA`) mark each button by
  colour. VNDegen reads the same maps, so the screens look and work as on the PC.

## Saves

The PC engine keeps saves in `Documents/<brand>/<title>/`, here `Documents/Ｖａｎａｄｉｓ/魔物娘との性活～ラミアの場合～/`,
and VNDegen keeps them in the same folder under the game's data directory:

- `_DAT0000.DAT` to `_DAT0099.DAT` are the 100 slots (five pages of two columns, numbered down each column, the last
  being the quick save). A slot is `IEL1` and a 6.9 MB state block: the message's name, text and voice, the scene name,
  the script and the byte offset of the message's first line, the background, transition mask, character slots,
  objects, the music, the flags and the backlog. VNDegen writes and reads those fields. A slot from the PC engine
  under Wine loads in VNDegen on the same message, and a slot from VNDegen lists and loads in the PC engine.
- `_SYSDAT.DAT` (5.5 MB, not packed) holds the settings, the system flags, the seen CGs and scenes of the gallery, read
  marks and the name table. VNDegen reads and writes the settings (sound on and off and volumes, text speed, skip
  unread, auto and its speed, waiting for voices, window transparency), the system flags and the gallery bits in place,
  and keeps the rest.
- The save list shows each slot's scene name and its file's time, as the PC engine does.

## What runs

- The logo, the title with its hover buttons and alternate pictures, Continue and Config from the title, and the
  extras page after a clear.
- Text with the PC's layout (16-pixel glyphs on a 24-pixel pitch, a hanging indent after an opening quote, ruby),
  speaker plates, the translucent window with its opaque ribbon, the window's buttons (Save, Load, Q.Save, Q.Load,
  Skip, Auto, Voice, Config, Log), hiding the window with a right click, and the backlog on the wheel.
- Characters in three slots with fades and slides, transition masks, screen shake, objects that move, choices.
- The save and load pages with their confirm dialogs, the configuration page's toggles and sliders, and the exit
  dialog.
- The CG gallery with its variant viewer and the scene recall, from the PC's seen flags.

Games start on the "Visual novel" controller layout.

## Known limits

- The configuration's font page (font colour, antialiasing, font choice) is drawn but its settings do nothing; text
  always uses the MS Gothic substitute.
- Read marks aren't kept, so Skip skips all text, as the PC does with "未読も飛ばす" on, its default.
- The "return to title" confirmation is VNDegen's own, since the PC uses a Windows message box.
- `#MOVIE` is not supported; no library game uses it.
