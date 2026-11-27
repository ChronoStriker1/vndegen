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

## Drawing the map

`fielddraw.c` builds each map once into 8 by 8 cell chunks: terrain with stairs, slopes and sunken animated liquids,
a border of the map's outside chip, and the map objects at rest. A frame culls chunks to the view, draws them front to
back, then the characters, then blended surfaces back to front. The sun's shadows come from a depth map built once per
map; 3D characters are added to it every frame. `r3d.c` rasterizes on up to six threads, each filling a band of rows
in the order triangles were queued, so the image is the same as on one thread.

`sg_D.h` is the camera interface event commands call: `sg_camera_get`, `sg_camera_to` (a pose and a time in ms),
`sg_camera_reset` to the map's default, `sg_camera_moving`, and `sg_camera_command` for command 53's modes 0 and 3.
`sg_actor_motion` plays a named motion on a 3D character.

Frame times on a 14-core Linux VM, 960 by 544, six threads, measured with `sgview` over 300 to 1,200 frames while two
PC games and a test ran beside it (load average about 10): walking Sacred's 150 by 150 field 9.7 ms average, 99th
percentile 16.2 ms; Colette's throne room 8.6 ms and castle town 7.6 ms; Sacred's forest 11.0 ms (99th percentile
17.6 ms) and its castle courtyard, 74,000 triangles in view, 22.2 ms. On one thread the same views take 12 to 38 ms.

## The Linux host

`Engines/SmileGame/host` builds with CMake:

- `sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [okuntil:n] [shot:name]`: headless; writes `sgrun.log` and
  PNG frames to `<out>`.
- `sgplay <game>`: an SDL window with the keyboard (arrows, Enter or Z, Esc or X, Shift).
- `sgtest`: the format reader tests. With `SG_COLETTE` and `SG_SACRED` set to the game folders it also checks values
  read from the games' files.
- `sgtest_D`: the renderer's tests: coverage, the map tail and cameras of both games, the throne room's projection
  against the PC's pixels, and a skinned character's motions.
- `sgview <game> <out.png> <map> <x> <z> <dir> [frames] [hero:name] [hold:vk,ms] [threads:n]`: draws a map at a cell
  and prints frame times.

## Known limits

- No sound, menus, battles, saves, choices or pictures over the map yet; unknown commands are skipped and logged.
- Command 53's modes 1 and 2 (Colette's event rooms) are not mapped; mode 3's target point matches the PC's opening
  scene only roughly.
- Background value 1 (Sacred's field) draws black; the PC never shows it in play.
- The PC darkens wall feet slightly more. Map objects with lights stay in their unlit motion until an event changes
  them; scrolling textures (`uscrollspeed`, the noon clouds) and particle effects are not drawn.
- Sacred's castle courtyard takes about 22 ms a frame on that VM, over the 16.7 ms a 60 fps frame allows.
- The message window's fill differs between the two games on the PC and is drawn the same in both.
