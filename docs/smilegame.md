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
- Data tables: heroes, battle commands, items, skills, monsters and effects, and the game settings' menu text, sounds
  and windows. The party, inventory, money and play time with stats as the PC computes them.
- The field menu (Esc or X): items, skills, equipment, status, save slots, quit and config, laid out as the PC draws
  them, with faces; the shop and inn screens (`shop_open`, `inn_open` in `sg_B.h`).
- Battles: event command 65 through `sg_battle_start`, drawn on the battle background map from behind the party,
  with the command list, targets, skills, items, defend and escape, monster actions, damage as measured on the PC,
  the result panel, level-ups and GAME OVER.
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

## The Linux host

`Engines/SmileGame/host` builds with CMake:

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
- `sgtest_B`: the data table tests against values from the PC's menus and the game files. `sgtest_B shots
  colette|sacred <game> <out>` writes every menu page as PNG for comparison with the PC.
- `sgtest_D`: the renderer's tests: coverage, the map tail and cameras of both games, the throne room's projection
  against the PC's pixels, and a skinned character's motions.
- `sgview <game> <out.png> <map> <x> <z> <dir> [frames] [hero:name] [hold:vk,ms] [threads:n]`: draws a map at a cell
  and prints frame times.
- `sgtest_A`: the event command tests; with the game folders set it plays both openings (about five minutes).
  `sgtest_A drive <game> <out> ...` plays a scripted path: `until:field|choice|message`, `upto:<text>`, `pick:N`,
  `to:x,z` (walks there), `talk:U|D|L|R`, `hold:U,ms`, `ok`, `wait:ms`, `shot:name`, `where:tag`.

## Known limits

- Battles play, but command 65 doesn't start them yet: in the story a battle counts as won until it calls
  `sg_battle_start` (`battle_setup_cmd` decodes its arguments) and branches on `sg_battle_result`. No battle BGM or
  effect animations; states and elements are not applied; critical, skill, escape and shared experience rules are
  inferred (see `docs/smilegame-cleanroom.md`).
- Commands for sound, shops, inns and party recovery are decoded and logged but not yet wired to the audio and menu
  code, so only the title and map music plays and an inn counts as declined.
- The items, money and party that commands change are kept apart from the menu's party state until the two are
  joined, so Sacred's menu shows the game settings' first member.
- The camera command (53) is decoded but not yet wired to the camera; effects (27) and balloons (45) are not drawn,
  though balloons take their time.
- Events never wander or chase the player; the movement type in their sheets is read but not used.
- Command 10's and 16's codes 4 to 7, 11's one-argument form, and 15, 55, 56, 58, 66, 68, 70, 76 and 77 are logged and
  skipped; none is on either opening path.
- The menu's save page lists slots and dates but doesn't write yet, and doesn't show a slot's map and party.
- The shop and inn screens were not compared with the PC; the party order page is not in these games' menus.
- Experience above level 6 uses a fitted curve, and a few item, skill and monster fields are inferred (see
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
- Sacred's castle courtyard takes about 22 ms a frame on that VM, over the 16.7 ms a 60 fps frame allows.
- The message window's fill differs between the two games on the PC and is drawn the same in both.
