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
- Battles: event command 65 through `sg_battle_start`, drawn on the battle background map from behind the party,
  with the command list, targets, skills, items, defend and escape, monster actions, damage as measured on the PC,
  the result panel, level-ups and GAME OVER.
- A software rasterizer draws the 3D scene into the same pixel buffer the other runtimes hand to the frame host.

## The Linux host

`Engines/SmileGame/host` builds with CMake:

- `sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [okuntil:n] [shot:name]`: headless; writes `sgrun.log` and
  PNG frames to `<out>`.
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

## Known limits

- No sound, saves, choices or pictures over the map yet; unknown commands are skipped and logged.
- Battles: event command 65 still has to call `sg_battle_start` (`battle_setup_cmd` decodes its arguments) and branch
  on `sg_battle_result`. No battle BGM or effect animations; states and elements are not applied; critical, skill,
  escape and shared experience rules are inferred (see `docs/smilegame-cleanroom.md`).
- The menu's save page lists slots and dates but doesn't write yet, and doesn't show a slot's map and party.
- Commands that give items or money, change the party, or open a shop or inn are not wired to the menu yet, so
  Sacred's party is the game settings' first member until they are.
- The shop and inn screens were not compared with the PC; the party order page is not in these games' menus.
- Experience above level 6 uses a fitted curve, and a few item, skill and monster fields are inferred (see
  `docs/smilegame-cleanroom.md`).
- Shadows and the sky are not drawn; skinned characters stand in their bind pose.
- Stairs are drawn as plain columns, not as steps.
- The camera is fixed at one angle and distance close to the player's default; events that move the camera are skipped.
- The message window's fill differs between the two games on the PC and is drawn the same in both.
