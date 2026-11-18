# IPAC runtime: where the knowledge came from

The IPAC runtime (`Packages/VNDegenKit/Sources/CIPAC`, hosts in `Engines/IPAC/host`) was written for VNDegen. It
derives from no GPL or LGPL code. Its sources:

| Part | Sources |
|---|---|
| Archive (`IPAC`), `IEL1` packing | GARbro `ArcFormats/Ipac/ArcIPAC.cs` (MIT): the entry layout and LZSS. The window's fill (spaces, where GARbro fills with zeros) from the scripts themselves, whose indentation came out as NUL bytes with a zero fill |
| Pictures (`IES2`) | GARbro `ArcFormats/Ipac/ImageIES.cs` (MIT) |
| Music (`WST2`) | GARbro `ArcFormats/Ipac/AudioWST.cs` (MIT): rewrapping as an MS ADPCM WAV. Decoding by dr_wav (public domain or MIT-0), as in the Wolf RPG runtime |
| Script syntax and commands | The game's own `.org` scripts, and the original engine under Wine with loose test scripts in `DATA/SCR/`, which it reads before the archive |
| Timing (`#WAIT`, `#SPEED`), sprite slots and slides, shake, text speed | Frame-by-frame recordings of the original engine under Wine running test scripts |
| Message window, choices, save and load pages, configuration, backlog, exit dialog | The games' `Parts.PAK` sheets and their colour hit maps, and screenshots of the original under Wine measured pixel by pixel (window translucency from `MesWindowB`, text origin and pitch, the choice rows) |
| Saves (`_DATnnnn.DAT`, `_SYSDAT.DAT`) | Diffs of files the original wrote under Wine from controlled test scripts: the fields' offsets, the message offset, flag bytes, object records, the backlog, the gallery bits. Checked both ways: original's slots load here, ours load there |
| Gallery (`#VCHECK`, `#SCHECK`, `#OBJ_ANIME`, `#OBJ_DRAWW`, `#BG_VISUAL`), IAN frame tables | The game's gallery scripts and sheets, and the original's CG viewer under Wine |
| Text | stb_truetype (public domain or MIT) with the shared Windows font substitutes (`docs/fonts.md`) |
| File name case | `Engines/Shared/casepath.c`, VNDegen's own |

The exe was only run, never disassembled: its strings were read for file names and the save path's format.

Things that were unsure and how they were settled:

- `R C`, `L R`: tested with loose scripts. A slide moves a character one slot toward the named one, which explains both
  the story's uses and the test's.
- Whether a background change clears the characters: it doesn't (tested).
- `X`, `K`, `E` versus `C`, `L`, `R`: the first set swaps at once, the second fades (tested by timing).
- `#MENU`: 1 enables the window's buttons and 0 disables them, as the title script's use and a test showed.
- Flags are bytes: the title's frame counters wrap at 256 and the save files store one byte per flag.
- `RCLK` is consumed when read: the CG viewer's right click otherwise also leaves the gallery page.
