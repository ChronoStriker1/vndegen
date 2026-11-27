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
- Events: sheets chosen by every condition kind the games use; talk, touch, autorun and repeating triggers; messages
  with faces, choices and branches, captions and scrolling text, pictures, fades and tint, variables, switches, items,
  money, the party, moving and turning the player and events, event graphics, moving to another map. Colette plays from
  the title to the bridge guard who opens the way to its first battle, and Sacred from the title to the guard at (35,30)
  in the castle.
- A software rasterizer draws the 3D scene into the same pixel buffer the other runtimes hand to the frame host.

## The Linux host

`Engines/SmileGame/host` builds with CMake:

- `sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [okuntil:n] [shot:name]`: headless; writes `sgrun.log` and
  PNG frames to `<out>`.
- `sgplay <game>`: an SDL window with the keyboard (arrows, Enter or Z, Esc or X, Shift).
- `sgtest`: the format reader tests. With `SG_COLETTE` and `SG_SACRED` set to the game folders it also checks values
  read from the games' files.
- `sgtest_A`: the event command tests; with the game folders set it plays both openings (about five minutes).
  `sgtest_A drive <game> <out> ...` plays a scripted path: `until:field|choice|message`, `upto:<text>`, `pick:N`,
  `to:x,z` (walks there), `talk:U|D|L|R`, `hold:U,ms`, `ok`, `wait:ms`, `shot:name`, `where:tag`.

## Known limits

- No sound, menus, battles, saves yet. Their commands (BGM, sounds, battles, shops, inns, party recovery) are decoded and
  logged; a battle counts as won and an inn as declined.
- The camera command (53) is decoded and logged but not applied; effects (27) and balloons (45) are not drawn, though
  balloons take their time.
- Events never wander or chase the player; the movement type in their sheets is read but not used.
- Command 10's and 16's codes 4 to 7, 11's one-argument form, and 15, 55, 56, 58, 66, 68, 70, 76 and 77 are logged and
  skipped; none is on either opening path.
- Shadows and the sky are not drawn; skinned characters stand in their bind pose.
- Stairs are drawn as plain columns, not as steps.
- The camera is fixed at one angle and distance close to the player's default; events that move the camera are skipped.
