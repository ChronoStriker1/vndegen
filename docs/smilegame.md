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

## The Linux host

`Engines/SmileGame/host` builds with CMake:

- `sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [okuntil:n] [shot:name] [save:slot] [load:slot]`: headless;
  writes `sgrun.log` and PNG frames to `<out>`, which is also its save folder.
- `sgplay <game>`: an SDL window with the keyboard (arrows, Enter or Z, Esc or X, Shift).
- `sgtest`: the format reader tests. With `SG_COLETTE` and `SG_SACRED` set to the game folders it also checks values
  read from the games' files.
- `sgtest_B`: the data table tests against values from the PC's menus and the game files. `sgtest_B shots
  colette|sacred <game> <out>` writes every menu page as PNG for comparison with the PC.

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
- Shadows and the sky are not drawn; skinned characters stand in their bind pose.
- Stairs are drawn as plain columns, not as steps.
- The camera is fixed at one angle and distance close to the player's default; events that move the camera are skipped.
- The message window's fill differs between the two games on the PC and is drawn the same in both.
