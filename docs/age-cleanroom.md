# AGE runtime: where the knowledge came from

The AGE runtime was written for VNDegen. It derives from no GPL or LGPL code. This file lists its sources.

## Sources

| Part | Sources |
|---|---|
| Archives and catalogs (`SYS4INI.BIN`, `.AAI`, `.ALF`) | GARbro `ArcFormats/Eushully/ArcALF.cs` (MIT): the LZSS directory at 0x134 (0x114 in an append catalog) and its archive-name and file records. The selector byte, the settings trailer and the bank sizes in the header were read from the game's files, then checked against OpenMaidEngine's notes |
| Pictures (AGF) | GARbro `ArcFormats/Eushully/ImageAGF.cs` (MIT): the packed BMP header, the pixel section and the `ACIF` alpha plane |
| Script container | The game's own scripts: the header, the three tables, the inverted string pool and the operand pairs were worked out from the 732 scripts; OpenMaidEngine's notes gave the operand counts, and every script parses with them to its string pool |
| Opcode meanings | OpenMaidEngine's opcode notes and engine notes (`vm-map/opcodes.toml`, `docs/`, MIT, read at commit `8a88073e`), read as hints with the coordinator's approval and confirmed against Kamidori's scripts and the original under Wine. Its C# source was not opened |
| Saves | OpenMaidEngine's notes on the `S4SD` container, layout 3, the shared profile, `RT.DAT` and the history tail; each was confirmed by decoding saves the original wrote under Wine and by the original loading saves VNDegen wrote |
| Movies | The shared vnvideo decoder (pl_mpeg, MIT) |
| Sound | stb_vorbis (public domain or MIT) and dr_wav (public domain or MIT-0), as in the other native runtimes |
| Fonts | The shared Windows substitutes and stb_truetype |

## OpenMaidEngine

[OpenMaidEngine](https://github.com/concopuffs/OpenMaidEngine) is an MIT-licensed reimplementation of AGE in C#,
targeting Himegari Dungeon Meister (SYS4422). Its notices say its first opcode catalog was informed by Kelebek1's
Eushully-Decompiler, which has no license, and that it was not developed under a formal clean-room process.
The coordinator approved reading its MIT opcode notes and docs (not its source) as hints, the way GARbro is used.
Eushully-Decompiler was never opened. No text, table or name from OpenMaidEngine is copied into VNDegen's code; the
runtime's names and structures are its own, and its operand-count table was generated from the counts and then
checked against all 732 Kamidori scripts.

## Checked against the original

The original `age.exe` ran under Wine in its own prefix, offline, on its own X display, as a black box:

- **Screens compared.** The title, a prologue page, the battle map's HUD and unit menu, the turn counter's rule, and
  the Load screen.
- **Saves.** A numbered save the original wrote decodes with the container and layout above and loads in the runtime
  at the same turn. A save the runtime wrote is listed by the original with its date, play time and thumbnail, and
  loads there at the same turn.
- **History tail.** The original's tail decodes to 203 entries and 928 records that consume it exactly.

## Opcodes confirmed

- **Confirmed against the game's behaviour.** Text, waits, hotspots, the
  scene coroutine, sound, movies, the graphics object model, transitions, saves and the history were each seen to work
  in the scenes listed in `age.md`.
- **Hints only.** The opcodes the tested scenes don't reach keep OpenMaidEngine's described behaviour and are
  unconfirmed until a scene uses them.
