# PJADV runtime: where the knowledge came from

The PJADV runtime (`Packages/VNDegenKit/Sources/CPJADV`, hosts in `Engines/PJADV/host`) was written for VNDegen. It
derives from no GPL or LGPL code. Its sources:

| Part | Sources |
|---|---|
| Archives (`GAMEDAT PAC2`) | GARbro `ArcFormats/Pajamas/ArcGameDat.cs` (MIT): the header, the name and index tables, and the XOR that scrambles `textdata.bin`. The order the archives are opened in from the game's `archive.ini` |
| Pictures (EPA) | GARbro `ArcFormats/Pajamas/ImageEPA.cs` (MIT): the header, the 16-entry offset table, the planar channels and the paletted alpha |
| Layouts and animations | The game's `.def` files read as text (`srce`/`srcj`/`srcc`, `xpos`, `ypos`, `filepos`, `anime`, `child`, `alpha`, `scale`, `xcenter`, `ycenter`, `show`). `ANM1` and `ANM2` from the files: the header offsets, then the commands decoded by their lengths across every file in the game and checked against what the original draws under Wine (the stage-select portrait sliding in, the buttons' states, the icons' blinking, the erase effect) |
| Script (`PJADV_SF0001`) | The game's `scenario.dat` tokenized: each command's first dword gives its length, operation, flags and category. Operand kinds (immediates, three variable banks, temporaries, the last engine result) and what each command does from how the script uses them: loops, the title and stage-select menus, the option sliders, the scenes. Layer stacking (later loads in front) from the original's logo sequence under Wine. Polled buttons from the menus' wait loops |
| Text (`PJADV_TF0001`, `filename.dat`) | The files: double-NUL-separated strings with `\U<n><hex>h` escapes for UTF-8, and the 32-byte resource names the script indexes from one. English text's quotes and the message window's look from screenshots of the original under Wine |
| The cleaning minigame (`mg02`) | Not in the script. Rebuilt from the help screen, `mg02_ini.txt` (`StageSetting` rows, cursor hot spots) and play of the original under Wine, driven by xdotool and read back from screenshots by template matching (details below). Its pictures are the game's (`gg_*`, `gr_*`, `gp_*`, `mg02_dust*.anm`, `ga_eferase*`) |
| Sound | stb_vorbis (public domain or MIT); WAV read from the RIFF header |
| Text drawing | stb_truetype (public domain or MIT) with the shared Windows font substitutes (`docs/fonts.md`) |
| File name case | `Engines/Shared/casepath.c`, VNDegen's own |

The exe was only run, and its strings read for file and font names. No code was read.

## The minigame, rule by rule

Measured means read from screenshots of the original under Wine at 1920x1080; inferred means chosen to match what
play showed where play couldn't pin the value down.

- **Stages.** Each `StageSetting = upTo, count, depth, points, seconds` row applies to the stages up to its first number,
  the same in all four chapters. Measured: stage 1 has 10 icons and 30 seconds, stage 2 60 icons and 40 seconds, stage
  3 90 icons, 50 seconds and depth 2. `points` shows nowhere and isn't used. `StartTime` isn't used either: stage 1's
  timer starts at 30, its row's `seconds`.
- **Placement.** Measured: every icon's centre is on a 14 by 7 grid, x from 281 to 1711 in steps of 110 and y from 188
  to 908 in steps of 120, across four plays of stage 1 and plays of stages 2 and 3. Types and cells change each play.
  Inferred: cells and types are uniformly random; with more icons than cells, cells are reused.
- **Tools.** Measured: one tool button bottom left; each click moves cleaner, deck brush, broom, mop, cloth and round
  again. The NEXT icon shows the following tool. Right click, the wheel and Space don't change tools. Each icon type
  takes one tool (the help screen): dust ball and cleaner, crayon and deck brush, banana and broom, slime and mop, frog
  mushroom and cloth. A wrong tool does nothing visible. The pig on the help screen has no picture in the game's files
  and never appeared.
- **Depth and wiping.** Measured: at depth 2 one wipe leaves an icon at about half opacity and a second removes it;
  dragging back and forth over an icon in one press counts twice. Inferred: a wipe counts when the pressed cursor
  enters an icon (within 75 pixels of its centre) or has moved 60 pixels over it since the last, at most one per
  120 ms per icon. The original sometimes ignored a second separate drag made soon after the first; VNDegen counts it.
- **Timer.** Measured: whole seconds counting down from the stage's time once GO has shown, two digits with a leading
  zero, at fixed places; it stops on a clear and while paused.
- **Clear and time up.** Measured: CLEAR!! or TIME UP! centred on the screen, then the result panel (50% dim, STAGE
  CLEAR! or STAGE FAILED..., YOUR TIME as the seconds left, BEST TIME as the highest, the stage's thumbnail and GET! on a
  clear, Stage Select). Inferred: the white flash and sparkles' timing.
- **Pause.** Measured: the pause button top left opens a panel over a 50% dim with close, Retry and Stage Select; the
  timer stops.
- **Sounds.** Inferred: which of `SE_201` to `SE_208` plays for which event; Wine preloads them, so file reads can't
  tell, and the test machine has no sound device. `BGM_FC25` is the only music the script never plays, so it is taken
  as the minigame's.
