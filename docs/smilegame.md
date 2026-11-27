# Smile Game Builder

Smile Game Builder (SmileBoom) makes 3D RPGs from editor data alone: a game carries no code of its own, and the
player (`sgb_rpgplayer.exe`, a .NET program over the mixed-mode `SharpKmyCore.dll` and the native `kmyCore.dll`)
interprets the data. VNDegen runs these games on a runtime of its own, `Packages/VNDegenKit/Sources/CSmileGame`,
written in C from the games' files and the original player's behaviour under Wine. `docs/smilegame-cleanroom.md`
records every layout and where it came from.

The runtime runs in the app on Mac, iPhone and iPad through the `EngineSmileGame` plugin, and on Linux through the
runners below.

## Princess Colette's Great Adventure

VNDegen plays Princess Colette's Great Adventure (Smoothly Animated Pixel Art RPG) on Mac, iPhone and iPad from the
title to its ending: the opening, the throne room and the castle town, the dungeon scenes in which Colette joins your
party, the world map with its wandering monsters, the east cave and its boss, the inn, the rumour-monger's quest
through コムコムの穴 with its bridges, the public square, the witch's forest where メイリ joins, the サキュバスタワー and
its boss (サキュバス joins), the sewer and the gargoyle, the cat's mansion with its fishing where ニャンニャ joins, the
walls of ice, fire and lightning, the North Tower (幽玄の塔) to its hall, the former king's battle, ユグドラシル, the
last scene in the throne room and the staff roll, with the scenes between them and their replays in the log room. It
was played that far in VNDegen. The optional places (the cape, the catacombs, the golem cave) and the post-game after
the staff roll (the tower's upper floors, the prison scene and ドラコ) haven't been played. The game is in Japanese, as
on the PC.

- **Saves** are the PC version's own files in the game's `savedata` folder, so a save moves both ways between VNDegen
  and the PC copy, and Ludusavi backs them up as it does on Windows. Save from the game's menu (Escape, X, or Y and
  Menu on a controller) or from Game commands in the VNDegen menu, and load from the title's Continue or from Game
  commands.
- **Controls** start on the RPG Maker layout: A confirms, B cancels, Y and Menu open the game's menu, X dashes. On a
  touch screen, a tap outside the touch gamepad confirms, as a click does on the PC, and holding a finger on the map
  opens the menu.
- **What isn't there yet:** the optional dungeons and the post-game haven't been played in VNDegen, so they may show
  problems. Battle effects play simpler than on the PC, and the camera doesn't swing in on the fighter who acts. A few battle rules (critical hits, skill damage, escaping) are close estimates of
  the PC's. Known limits below lists the rest.

Sacred (セイクリッド～砕かれた光の冠～) is recognised and starts, and plays through its castle and first dungeon, but
it isn't part of this release.

## Test games

| Game | Player | Maps | Notes |
|---|---|---|---|
| Princess Colette's Great Adventure (Smoothly Animated Pixel Art RPG) | 1.8.1.0 | 122 | 2D pixel-art characters on 3D chip terrain. Japanese only |
| Sacred (セイクリッド～砕かれた光の冠～) ver1.04 | 1.0.6.26 | 81 | 3D FBX characters, pictures over the map, a row title menu. Japanese only |

Both games use the same data format.

## What runs

- The package, every `.sgr` file, resources from the game and its resource packs, PNG pictures and the game's font.
- The title screen: picture, overlay, the three items as a column or a row, the cursor. New game, Continue (lit when
  a save exists, and where the cursor starts; it opens the save page over the darkened title to load a slot) and
  Config.
- Maps: chip terrain in 3D with the map's light colour, map objects and event graphics from FBX, 2D character sheets
  drawn upright, the player walking cell by cell (stairs let it change height), talking to the event in front.
  Events face their sheet's direction, wander or walk toward the player at their sheet's speed and frequency, and a
  chaser whose trigger is 4 starts its event when it reaches the player (Colette's world-map monsters).
- Events: sheets chosen by every condition kind the games use; talk, touch, event-touch, autorun, repeating and
  parallel triggers (parallel ones run in the one event runner, as repeating ones do); messages with faces, choices
  and branches, captions (fading on Sacred's player version) and scrolling text, pictures with zoom and colour,
  balloons and effects over characters, fades and tint, variables (random ones too), switches, items, money, the
  party, learned skills, the encounter switch, moving, turning and placing the player and events, event graphics,
  moving to another map, stopping a sound effect (76), Colette's floor names in a band across the screen, and the
  eye-level camera of its event rooms (command 53's mode 1 and maps whose camera record has kind 1). Colette plays
  from the title to the staff roll (the story variables at 19 and 17), and Sacred from the title
  through the castle, the town's weapon shop and the forest into its first dungeon, the royal cave, and through the
  fairy's scene there.
- Data tables: heroes, battle commands, items, skills, monsters and effects, and the game settings' menu text, sounds
  and windows. One party state (`Party` in `sg_B.h`, `g->b.party`) holds the heroes with their stats, the reserve,
  items, obtained items, money, hero looks and play time; event commands change it, the menu shows it, and saves write
  and read it, with each hero's state (poison) and the skills learned by level-ups and command 56.
- The field menu (Esc or X): items, skills, equipment, status, saving to a slot with each slot's map and party, quit
  and config, laid out as the PC draws them, with faces, and the menu sounds. Shop (37) and inn (38, with the PC's
  choice window, music and fade) commands open their screens and branch on the result; 59 recovers the party.
- Battles: event command 65 starts a battle and waits for it, then branches on the result as the PC does; maps with an
  encounter table start battles from steps while the encounter switch (command 66) is on. A poisoned member puts the
  poison balloon over the player on the map. The battle is drawn on the battle background map from behind the party,
  with the entry flash and intro, the command list, targets, skills, items, defend and escape, monster actions, damage,
  states, drain and effects from `Effect.sgr` with their sounds, learned skills beside the hero table's, the battle BGM
  and victory music, the result seen from the front, level-ups and GAME OVER. 3D heroes play their battle motions.
  `vnsmilegame_scene` reports `battle`.
- A software rasterizer draws the 3D scene into the same pixel buffer the other runtimes hand to the frame host.
- Saves: the PC's own `savedata/Save_NN.sgs` files, read and written field for field, with everything the runtime does
  not model yet kept and written back unchanged, so saves move both ways between VNDegen and the original. Pictures on
  screen and skills learned by command are saved and restored as the PC does. `saveDir`
  in the config is the folder holding the saves; it defaults to the game's `savedata`.
- Sound: the title and map BGM and ambient sound, with the files' loop points (after a battle the map's BGM goes on from
  where it stopped), through `vnsmilegame_audio` as stereo floats at `audioRate`. Commands 21 (BGM), 22 (sound) and 64
  (music effect) play through the same mixer.
- Camera command 53 (modes 0 and 3) and motions from commands 14 and 69 and from each event sheet.

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

Round 3 stopped each 3D character from flushing the triangle queue (the raster threads waited for every character)
and lets the raster threads take bands from a shared counter, four per thread, with the same pixels as one thread.
At load average 12, six threads, 300 frames: Sacred's castle at (35,10) 11.4 ms (99th percentile 17.0 ms), the
courtyard at (25,40), 68,000 triangles, 13.1 ms (18.7 ms), the field at (120,66) 6.1 ms and the forest 7.0 ms. What
is left on the CPU is transforming and queueing triangles on one thread (about 5 ms in the courtyard) and the
rasterizer itself; a Metal path would take both off the CPU.

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
  D, which would move the player.
- Touches and clicks outside the touch gamepad do what the PC's left mouse button does: a tap confirms when the finger
  comes up, and holding about 0.6 s on the map opens the menu. `vnsmilegame_pointer` takes the presses.
- Game commands in the VNDegen menu has Save and Load with the 40 slots the PC's save page shows, written and read
  with `vnsmilegame_save` and `vnsmilegame_load`. Save is refused at the title and during a battle, where the PC can't
  save either; a load closes the game's menu and leaves a battle.
- The game's font comes from its package. `VNSmileGameConfig.fallbackFont` carries the MS PGothic substitute
  (`docs/fonts.md`); characters the game font lacks (Colette's text has 嚙) are drawn from it on the same baseline.
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

Colette past the first area, a DEBUG build of this round's branch on the Mac mini, 2026-11-27, measured the same way
from runner saves (another delegate's build ran beside some of it; load averages are given). Target 60.

| Scene | Mac app | iPad Pro 13 simulator, gamepad hidden |
|---|---|---|
| East cave B1F | 60.0, 59.9 to 60.0, 3.6 ms (load 7) | not measured |
| コムコムの穴 B2F, the suspension bridge | 60.0, 59.9 to 60.0, 3.4 ms (load 7) | not measured |
| Inn, the stay | 59.7 to 60.0, 37.2 to 60.0, 6.7 ms (load 6) | not measured |
| Event room (a log room replay) | 60.0, 59.9 to 60.0, 5.5 ms | 60.0, 60.0, 2.6 ms (load 20 to 35) |
| サキュバスタワー 6F, four heroes | 60.0, 59.9 to 60.0, 2.8 to 4.5 ms (load 17) | not measured |
| Battle with three heroes | 60.0, 59.9 to 60.0, 7.0 ms (load 4) | 60.0, 60.0, 5.3 ms (load 25 to 33) |

With the touch gamepad shown, the iPad simulator's world map ran at 44.6 to 46.1 frames a second with the runtime at
3.4 ms, the same gamepad compositing cost as below.

Colette's first release, DEBUG builds of 208c06cf on the Mac mini, 2026-11-27, measured the same way (Mac app with no
other app or build running, load average 3 to 5; one simulator booted at a time). The world map is the cell below
the castle where the first monster reaches the player, the battle the command list of that battle. Target 60.

| Scene | Mac app | iPad Pro 13 simulator | iPad, touch gamepad shown | iPhone 18 Pro simulator |
|---|---|---|---|---|
| Throne room | 59.6 to 60.0, 33.3 to 60.0, 6.7 ms | 59.7 to 60.0, 37.5 to 60.0, 3.8 ms | 34.0 to 35.4, 19.8 to 20.8, 4.5 ms | 59.8 to 60.0, 42.9 to 60.0, 3.9 ms |
| Castle town | 60.0, 60.0, 7.0 ms | 59.8 to 60.0, 42.2 to 60.0, 4.1 ms | 40.2 to 41.4, 20.3 to 23.2, 4.2 ms | not measured |
| World map | 60.0, 60.0, 7.0 ms | 59.7 to 60.0, 37.5 to 60.0, 4.2 ms | 43.4 to 46.2, 18.9 to 24.5, 4.2 ms | 60.0, 60.0, 4.3 ms |
| First battle | 59.7 to 60.0, 37.5 to 60.0, 7.4 ms | 59.9 to 60.0, 49.7 to 60.0, 4.6 ms | 47.4 to 50.5, 20.3 to 26.3, 4.5 ms | 60.0, 60.0, 4.5 ms |

The runtime's own time stays under 8 ms everywhere. On the iPad simulator each display tick takes about 5.5 ms with
the touch gamepad hidden and 16 to 24 ms with it shown, so the drop to 34 to 50 frames a second there is the
simulator compositing the gamepad's translucent buttons over the game's view, not the runtime. That cost was not
measured on a real iPad, and other engines' rows in `docs/frame-rate.md` don't say whether their simulator numbers
were taken with the gamepad shown.

## The Linux host

`Engines/SmileGame/host` builds with CMake:

- `sgrun` and `sgplay` take the fallback font from `SG_FALLBACK_FONT`.
- `sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [okuntil:n] [shot:name] [save:slot] [load:slot]`: headless;
  writes `sgrun.log` and PNG frames to `<out>`, which is also its save folder.
- `sgplay <game>`: an SDL window with the keyboard (arrows, Enter or Z, Esc or X, Shift).
- `sgtest`: the format reader tests. With `SG_COLETTE` and `SG_SACRED` set to the game folders it also checks values
  read from the games' files.
- `sgbattle <game> <out> [seed:n] party:Name[@level],... troop:<script GUID hex>|monsters:Name,... [bg:<map>]
  [wait:ms] [key:vk] [shot:name] [attack:n] [until:phase]`: starts the battle command of an event script (for
  example `troop:4ff8bc66` for Colette's first slime on the world map, `troop:c426b710` for Sacred's practice battle)
  headless; `sgbattleplay` takes the same arguments with `-` as `<out>` and opens an SDL window.
- `sgtest_C`: the battle tests against numbers seen in PC battles.
- `sgtest_H`: battles from the story: skill and monster fields, map encounter tables, Colette's first wandering
  monster from its message to the world map with the PC's rewards and level-ups, Sacred's practice battle won and
  lost, and a random encounter in Sacred's first dungeon. `sgtest_H drive <game> <out> ...` plays a scripted path
  like `sgtest_A drive`, with `warp:<map>,x,z,dir`, `var:i=v`, `sw:i=v`, `join:<hero>`, `hp:i=v`, `seed:n`,
  `script:<GUID hex>`, `phase:<battle phase>`, `fight:attack|defend|escape`, `path:x,z` (prints the walk) and
  `until:battle`.
- `sgtest_B`: the data table tests against values from the PC's menus and the game files. `sgtest_B shots
  colette|sacred <game> <out>` writes every menu page as PNG for comparison with the PC.
- `sgtest_D`: the renderer's tests: coverage, the map tail and cameras of both games, the throne room's projection
  against the PC's pixels, and a skinned character's motions.
- `sgview <game> <out.png> <map> <x> <z> <dir> [frames] [hero:name] [hold:vk,ms] [threads:n]`: draws a map at a cell
  and prints frame times.
- `sgtest_A`: the event command tests; with the game folders set it plays both openings (about five minutes).
  `sgtest_A drive <game> <out> ...` plays a scripted path: `until:field|choice|message`, `upto:<text>`, `pick:N`,
  `to:x,z` (walks there), `talk:U|D|L|R`, `hold:U,ms`, `ok`, `wait:ms`, `shot:name`, `where:tag`, `save:N`, `load:N`.
- `sgtest_G`: the joined paths: new-game hero stats against the PC's saves, a load keeping the party, shop and inn
  branches, the BGM command, the menu's save page, and Sacred's castle after a load (queen, king, camera).
- `sgtest_J`: the first release's fixes: fallback glyphs, a case-swapped `sgb_rpgplayer.exe`, a load closing the
  menu, the title's Continue and Config, and taps and holds.
- `sgtest_L`: Colette past the first area: the inn's stay and its refusal, the inn's bed, crossing コムコムの穴's
  suspension bridge, walking under its stone bridge, a stair's side, the player standing on stairs, and command 76.
- `sgtest_M`: Colette's second half against the PC: a talk pressed mid-step, the fishing spot and its first catch, an
  opened chest, the fourth hero's place in battle, the experience curve, the noon sky's edge and the staff roll's
  sprites.
- `sgtest_I`: the field against the PC: sheet facing, turn, step and place codes, random variables, the encounter
  switch, learned skills and pictures in saves (byte for byte against PC saves), the help bar after a load, caption
  fades per player version and Colette's opening music.

## Known limits

- Colette's optional places (ヤーマ岬, the クサレィ catacombs, the golem cave) and its post-game (the North Tower's
  floors 6 to 15 on the way back, the prison scene, ドラコ) haven't been played in VNDegen.
- In battles the PC stands the first two heroes a little nearer the camera than the others; the runtime keeps all four
  on one line, so ダーク and コレット stand about 20 pixels higher than on the PC.
- Effects play their sheet's cells in order; the keyframe timeline inside each effect (positions, scales, screen
  flashes) is not read. The camera does not push towards the acting fighter as the PC's does.
- Critical hits, skill damage, healing, escape chance, defend and charge, the chance that a state lands and the
  recovery from sleep, paralysis, confusion and charm are inferred (see `docs/smilegame-cleanroom.md`). A random
  encounter always brings one monster; the PC sometimes brings two スライミー.
- Effects (27) play their picture sheet's cells in order over the effect's length; the PC composes layered,
  keyframed sprites with flashes (Sacred's 46光全体 is a rune circle and a pillar of light), and the key format is
  only partly read. Balloons draw the right bubble and icon with a simple pop, not the PC's keyframes.
- Events wander and chase with step speeds and pauses measured at -3, 0 and 3 and interpolated between; the PC's
  choice of direction when a chaser is blocked, and whether events walk during menus, were not pinned down.
- Commands 55 (experience), 58, 68 and 77 are logged and skipped, and 56's message for forgetting a skill is not done;
  Colette uses none of them, and Sacred only in its debug and CG rooms and its ending. Command 76 stops a sound effect.
- The inn was compared with the PC in Colette's town; the shop was, in Sacred's weapon shop. The shop's hero face is cropped at a fixed place that fits Sacred's faces. The party order page is not in
  these games' menus. Where a slot's page shows a second to fourth hero is a guess (two columns); both games have one
  hero there.
- A few item, skill and monster fields are inferred (see `docs/smilegame-cleanroom.md`).
- A hero's magic is taken from the hero tables; saves keep whatever the PC stored in that field (0 for every hero
  seen).
- Sound-effect and music-effect levels and the music effect pausing the BGM follow the BGM measurement and common
  practice; they were not measured on the PC.
- Command 53's mode 1 (Colette's event rooms) is fitted to the public square scene; its other pitches weren't
  compared. Mode 2's eye height and view angle are fitted to one scene, and the map's sky value 2 draws black behind
  it where the PC shows sky. Mode 3's target point matches the
  PC's opening scene only roughly.
- Background value 1 (Sacred's field) draws black; the PC never shows it in play.
- Colette's cat's mansion bookshelf shows white book spines where the PC's are coloured, and the North Tower's warp
  gates have a round shadow under them that the PC doesn't draw.
- The poison balloon shows over the player all the time; the PC shows it only now and then.
- The PC darkens wall feet slightly more. Map objects with lights stay in their unlit motion until an event changes
  them; scrolling textures (`uscrollspeed`, the noon clouds) and particle effects are not drawn.
- Sacred's castle courtyard averages about 13 ms a frame on that VM, but its 99th percentile (about 19 ms) is over the
  16.7 ms a 60 fps frame allows while other work runs; the rest needs the Metal path or a parallel transform stage.
- The game's own button guide in its menu shows Xbox B (red) deciding and A (green) cancelling; VNDegen's RPG Maker
  layout has A confirm and B cancel, as in the other RPG engines.
- Holding a finger on the map opens the menu without the PC's quirk of then choosing the first menu item when the
  finger comes up.
- The poison balloon's bubbles bob in a simple loop instead of the effect's keyframes. It shows for poison only; the
  other states end with the battle.
- Townspeople who wander can step onto an invisible door or exit cell, such as the castle door or the town's pier,
  and wait there while the player stands in the way; step back and they move on. Whether the PC lets them was not
  checked.
- The message window's fill differs between the two games on the PC and is drawn the same in both.
