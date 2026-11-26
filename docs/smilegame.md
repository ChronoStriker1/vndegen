# Smile Game Builder

Smile Game Builder (SmileBoom) makes 3D RPGs from editor data alone: a game carries no code of its own, and the
player (`sgb_rpgplayer.exe`, a .NET program over the mixed-mode `SharpKmyCore.dll` and the native `kmyCore.dll`)
interprets the data. VNDegen runs these games on a runtime of its own, `Packages/VNDegenKit/Sources/CSmileGame`,
written in C from the games' files and the original player's behaviour under Wine. `docs/smilegame-cleanroom.md`
records every layout and where it came from.

The runtime is in its first stage: it has a Linux runner and no app plugin yet.

## Test games

| Game | Player | Maps | Notes |
|---|---|---|---|
| Princess Colette's Great Adventure (Smoothly Animated Pixel Art RPG) | 1.8.1.0 | 122 | 2D pixel-art characters on 3D chip terrain. Japanese only |
| Sacred (セイクリッド～砕かれた光の冠～) ver1.04 | 1.0.6.26 | 81 | 3D FBX characters, pictures over the map, a row title menu. Japanese only |

Both games use the same data format.

## What runs

- The package, every `.sgr` file, resources from the game and its resource packs, PNG pictures and the game's font.
- The title screen: picture, overlay, the three items as a column or a row, the cursor. New game only.
- Maps: chip terrain in 3D with the map's light colour, map objects and event graphics from FBX, 2D character sheets
  drawn upright, the player walking cell by cell (stairs let it change height), talking to the event in front.
- Events: sheets chosen by switch, local switch and variable conditions; autorun scripts on entering a map; messages
  with pages, scrolling text, fades, variables, moving to another map.
- A software rasterizer draws the 3D scene into the same pixel buffer the other runtimes hand to the frame host.

## The Linux host

`Engines/SmileGame/host` builds with CMake:

- `sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [okuntil:n] [shot:name]`: headless; writes `sgrun.log` and
  PNG frames to `<out>`.
- `sgplay <game>`: an SDL window with the keyboard (arrows, Enter or Z, Esc or X, Shift).
- `sgtest`: the format reader tests. With `SG_COLETTE` and `SG_SACRED` set to the game folders it also checks values
  read from the games' files.

## Known limits

- No sound, menus, battles, saves, choices or pictures over the map yet; unknown commands are skipped and logged.
- Shadows and the sky are not drawn; skinned characters stand in their bind pose.
- Stairs are drawn as plain columns, not as steps.
- The camera is fixed at one angle and distance close to the player's default; events that move the camera are skipped.
- The message window's fill differs between the two games on the PC and is drawn the same in both.
