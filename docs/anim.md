# ANIM

VNDegen runs the ANIM engine's games on a runtime of its own, `Packages/VNDegenKit/Sources/CANIM`, written in C from
the games' files, GARbro's MIT-licensed readers and the original engine run under Wine. It is MIT licensed like the
rest of the app. E-mote characters are drawn through eluna, an MPL-2.0 Rust reimplementation of E-mote, behind
VNDegen's own `vnemote` interface (`Engines/ANIM/UPSTREAM.md`). `docs/anim-cleanroom.md` lists where each part came
from.

## Test games

| Game | Notes |
|---|---|
| Double Date Dilemma (`DDD.exe`, 2022, English) | Boot logos, title, story, choices, voices, music, loops, background voices, movies and the scene loops, E-mote characters, the flowchart and its jumps to either point of view, Save and Load both ways with the PC, quick save, backlog, the Volume screen |
| My Unique Reincarnation Skill 'Magical Sword' Makes Every Busty Girl Yield to Orgasm! (`mgsw.exe`, 2025, fan English patch) | Runs the same script engine; its title and screens are laid out differently and aren't built yet |

The two Stepmother Loves Her Son games were left out under the content rule.

## How the games are built

A game folder holds the exe, `<name>_sce.dat` (the compiled script), `<name>_define.dat` (sprite names, the anime
and expression presets, the flowchart, the gallery lists, the character table), `<name>_system.dat` (the system
save, rewritten in place by the PC), `cg/*.gax` pictures, `cg/psbpack.dat` (the E-mote characters),
`voice/<name>_wave.pck` (voices and sound effects), `bgm/*.eog` and `movie/*.mpg`. The plugin takes a folder as an
ANIM game when `<name>_sce.dat` sits beside `<name>_define.dat`, `<name>_system.dat` and `cg/psbpack.dat`, up to one
folder down.

- Encryption: the three `.dat` files, the `.gax` pictures and the save files share one scheme: `00 00 00 01`, a
  16-byte key, then a rolling XOR over 16-byte blocks.
- Pictures: PNG in 2048 by 2048 textures, the image at the top left. The screens are atlases; their pieces are
  placed where the PC draws them.
- Script: a code block, a table of named labels and a string pool. Instructions carry typed operands (`40` and `41`
  variables, `43` constants), raw numbers, string indexes and addresses. The scripts run the boot sequence, the story
  and the system routines (waits, effects, the scene jump table); the screens are the engine's own.
- E-mote: PSB files, encrypted with the engine's key, drawn at the slot positions and scales the script sets, with
  each character's scale from the define. Expressions are presets of E-mote variables in the define.
- Sound: Ogg Vorbis behind an 8-byte header, in `bgm/` and the voice archive. Movies are MPEG-1.

## Saves

The PC engine's own files in the game folder: `<name>_save.dat` (180 slots of 248,216 bytes, the first 160 on the
Save and Load screens), `qSavedata.dat` (the quick save), `cg/pSaveThumbNNN.png` (256 by 256, the screen at the top)
and `<name>_system.dat` (read lines, the flowchart's passed and read nodes, the last slot). A slot keeps the date, the
comment, the text instruction it was made at, the background, each layer's picture or character, offset, scale and
E-mote variables, and the script's local variables; VNDegen writes those and leaves the rest of the slot empty. Saves
load both ways with the PC.

## Known limits

- E-mote characters stand still in their expressions; their idle motion and the scripts' short pose animations
  (`b9`, `bc`, `be`, `c2` to `c5`) don't play yet, and eluna draws some expressions differently from the PC (head tilt,
  eyes).
- The Settings, Extra (CG, scene and music galleries) and shortcut screens aren't built; volumes aren't kept in the
  system save yet.
- Magical Sword's screens aren't built.
- The PC's screen effects (shake, flashes other than the colour flash) are approximated.
