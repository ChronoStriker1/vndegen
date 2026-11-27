# Smile Game Builder

Smile Game Builder (SmileBoom) makes 3D RPGs from editor data alone: a game carries no code of its own, and the
player (`sgb_rpgplayer.exe`, a .NET program over the mixed-mode `SharpKmyCore.dll` and the native `kmyCore.dll`)
interprets the data. VNDegen runs these games on a runtime of its own, `Packages/VNDegenKit/Sources/CSmileGame`,
written in C from the games' files and the original player's behaviour under Wine. `docs/smilegame-cleanroom.md`
records every layout and where it came from.

The runtime runs in the app on Mac, iPhone and iPad through the `EngineSmileGame` plugin, and on Linux through the
runners below.

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
- Data tables: heroes, battle commands, items, skills, monsters and effects, and the game settings' menu text, sounds
  and windows. The party, inventory, money and play time with stats as the PC computes them.
- The field menu (Esc or X): items, skills, equipment, status, save slots, quit and config, laid out as the PC draws
  them, with faces; the shop and inn screens (`shop_open`, `inn_open` in `sg_B.h`).
- A software rasterizer draws the 3D scene into the same pixel buffer the other runtimes hand to the frame host.
- Saves: the PC's own `savedata/Save_NN.sgs` files, read and written field for field, with everything the runtime does
  not model yet kept and written back unchanged, so saves move both ways between VNDegen and the original. `saveDir`
  in the config is the folder holding the saves; it defaults to the game's `savedata`.
- Sound: the title and map BGM and ambient sound, with the files' loop points, through `vnsmilegame_audio` as stereo
  floats at `audioRate`. Event commands and menus call the sound functions in `sg_E.h`.

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

## In the app (round 2, F)

`EngineSmileGame` takes a folder as a Smile Game Builder game when `data.sgbpack`, starting with `YUKARPKG`, is beside
`sgb_rpgplayer.exe`, up to two folders down, ignoring case: Colette's archive has the game at its top, Sacred's two
folders down beside the download site's pictures. The session runs `vnsmilegame_frame` from the display link on the main
thread, at most once per 15 ms, so a 120 Hz display doesn't draw the scene twice per game frame, and hands the frame to
`FrameView` through the shared frame host; sound goes through `PCMAudioOutput` at 44.1 kHz.

- Saves are the PC's `savedata/Save_NN.sgs` and `settings.dat` beside the game, which is where Ludusavi looks on
  Windows, so a backup restores into the folder the runtime reads.
- Keys: Return, Space and Z confirm, Escape and X cancel and open the menu, Shift dashes, the arrows and WASD move. A
  game starts on the RPG Maker layout: A confirms, B cancels, Y and Menu press X, X holds Shift, the D-pad and left
  stick move. The player reads no other key, so LB, RB, LT, RT and View start unbound instead of pressing Q, W, A, S and
  D, which would move the player. Touches outside the touch gamepad do nothing, since the player takes no mouse input.
- Game commands in the VNDegen menu has Save and Load with the 40 slots the PC's save page shows, written and read
  with `vnsmilegame_save` and `vnsmilegame_load`, until the game's own save page writes.
- The game's font comes from its package. `VNSmileGameConfig.fallbackFont` carries the MS PGothic substitute
  (`docs/fonts.md`) for text the game font can't draw; the runtime doesn't read it yet.
- With `-frameMonitor YES` a DEBUG build logs `SGFRAMES` every 10 seconds: display ticks, frames drawn, the 1% low,
  the slowest step, the average work per tick, the scene, and `runtime=`, the average time `vnsmilegame_frame` takes.

Frame times in the app, DEBUG builds of 834d294e on the Mac mini (M4), 2026-11-26, from saves at the same cells as the
Linux numbers above (Colette Map_069 14,11 and Map_070 3,9; Sacred Map_007 35,10 and Map_016 15,22), the player idle
with NPCs and events running. Each cell is frames drawn a second, the 1% low and the runtime's own milliseconds a
frame, over the 10 s windows after the first one following the load (20 to 30 s). Nothing else was building; load
average 4 to 9. Target 60.

| Scene | Mac app | iPad Pro 13 simulator | iPhone 18 Pro simulator |
|---|---|---|---|
| Colette, throne room | 59.6 to 60.0, 33.3 to 59.9, 6.9 ms | 44.0 to 44.9, 16.7 to 24.3, 3.5 ms | 60.0, 60.0, 3.6 ms |
| Colette, castle town | 60.0, 59.9 to 60.0, 7.0 ms | 48.6 to 49.0, 25.2 to 26.1, 4.0 ms | 60.0, 60.0, 4.2 ms |
| Sacred, forest (52,000 triangles) | 60.0, 60.0, 7.5 ms | 47.5 to 49.2, 25.3 to 25.8, 4.5 ms | 59.9 to 60.0, 50.0 to 60.0, 4.7 ms |
| Sacred, castle courtyard (55,000) | 59.8 to 60.0, 42.9 to 60.0, 7.7 ms | 50.8 to 50.9, 26.7, 5.9 ms | 58.8 to 60.0, 17.6 to 60.0, 6.1 ms |

The rasterizer takes under 8 ms everywhere. On the iPad simulator each display tick takes 15 to 17 ms in all, so
about 11 ms goes to presenting the 960 by 544 frame into the simulator's 2752 by 2064 drawable; the iPhone simulator's
present takes about 1 ms and the Mac app's under 1 ms. That cost is the simulator's, not the runtime's: an M-series
iPad has the Mac mini's cores and uploads a 2 MB texture in well under a millisecond, so it should hold 60. Older
A-series iPhones and iPads are slower per core and were not measured. A Metal path isn't needed for these scenes;
it would take moving `r3d.c`'s triangle setup and raster stage to a Metal render pass fed from the same chunk lists,
with the shadow map as a depth texture, and keeping the software path for Linux.

## The Linux host

`Engines/SmileGame/host` builds with CMake:

- `sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [okuntil:n] [shot:name] [save:slot] [load:slot]`: headless;
  writes `sgrun.log` and PNG frames to `<out>`, which is also its save folder.
- `sgplay <game>`: an SDL window with the keyboard (arrows, Enter or Z, Esc or X, Shift).
- `sgtest`: the format reader tests. With `SG_COLETTE` and `SG_SACRED` set to the game folders it also checks values
  read from the games' files.
- `sgtest_B`: the data table tests against values from the PC's menus and the game files. `sgtest_B shots
  colette|sacred <game> <out>` writes every menu page as PNG for comparison with the PC.
- `sgtest_D`: the renderer's tests: coverage, the map tail and cameras of both games, the throne room's projection
  against the PC's pixels, and a skinned character's motions.
- `sgview <game> <out.png> <map> <x> <z> <dir> [frames] [hero:name] [hold:vk,ms] [threads:n]`: draws a map at a cell
  and prints frame times.

## Known limits

- No battles, choices or pictures over the map yet; unknown commands are skipped and logged. Sound commands
  are not wired to the interpreter yet, so only the title and map music plays.
- The menu's save page lists slots and dates but doesn't write yet, and doesn't show a slot's map and party.
- Commands that give items or money, change the party, or open a shop or inn are not wired to the menu yet, so
  Sacred's party is the game settings' first member until they are.
- The shop and inn screens were not compared with the PC; the party order page is not in these games' menus.
- Experience above level 3 uses a fitted curve, and a few item, skill and monster fields are inferred (see
  `docs/smilegame-cleanroom.md`).
- A save made from a new game started in VNDegen has its heroes' stats at zero, since the hero tables and level
  growth are not read yet; saves that started as PC saves keep their heroes' values.
- Colette's opening calls for music by a resource pack's GUID; the original plays music none of the game's shipped
  files match there, and VNDegen keeps the title music.
- Sound-effect and music-effect levels and the music effect pausing the BGM follow the BGM measurement and common
  practice; they were not measured on the PC.
- Command 53's modes 1 and 2 (Colette's event rooms) are not mapped; mode 3's target point matches the PC's opening
  scene only roughly.
- Background value 1 (Sacred's field) draws black; the PC never shows it in play.
- The PC darkens wall feet slightly more. Map objects with lights stay in their unlit motion until an event changes
  them; scrolling textures (`uscrollspeed`, the noon clouds) and particle effects are not drawn.
- Sacred's castle courtyard takes about 22 ms a frame on that VM, over the 16.7 ms a 60 fps frame allows; in the app
  on an M4 it takes 7.7 ms.
- The game's own button guide in its menu shows Xbox B (red) deciding and A (green) cancelling; VNDegen's RPG Maker
  layout has A confirm and B cancel, as in the other RPG engines.
- The runtime reads no mouse or touches, so under Automatic on a touch device the touch gamepad shows once the
  player touches the screen; nothing else happens on a touch outside it.
- Loading from Game commands while the game's field menu is open leaves the menu open over the loaded map.
- A new game's save picks its layout from `sgb_rpgplayer.exe`'s version, read by its exact lower-case name, so a copy
  whose exe name differs in case on a case-sensitive volume writes Colette's saves in the 1.0 layout.
- The message window's fill differs between the two games on the PC and is drawn the same in both.
