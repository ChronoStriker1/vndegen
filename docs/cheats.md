# Feature request: cheats for games that aren't visual novels

Status: proposal, not started. This page describes the feature so it can be planned. Nothing here is built yet.

## Why

Many games in VNDegen's library aren't visual novels. They're RPGs, strategy games and action games built on RPG Maker, Wolf RPG Editor, AliceSoft System 3 and 4, Godot and Flash, and on a PC players reach for a trainer or Cheat Engine when a grind or a hard fight gets in the way. Neither tool runs on an iPhone or iPad, and on the Mac they don't reach a game running inside VNDegen. VNDegen runs these games' engines itself, so it can change a game's state directly, through the engine's own data rather than by patching memory.

## What the player sees

A Cheats entry in VNDegen's in-game menu, shown only for games whose engine supports it. It has two parts.

**Quick cheats.** Switches and buttons for the common needs. Each engine lists only the ones it can do for real:

- Add gold or money.
- Restore the party's HP and MP, or keep them full.
- Add an item, weapon or armor, picked from the game's own database by name.
- Turn random encounters off.
- Walk through walls.
- Game speed: 1x, 2x, 4x. The engine runs more updates per displayed frame, so music and timing stay consistent.
- Experience multiplier, where the engine computes experience in one place.

**Variables and switches.** For engines whose games keep their state in numbered variables and switches, such as RPG Maker and Wolf RPG Editor, a list of them with the names the game's database gives them, a search field, and in-place editing. This is the general tool for anything the quick cheats don't cover, such as story flags or a stat a game tracks in a variable.

Every change goes through the engine, so a save made afterwards is an ordinary save in the PC format, and it loads on the PC. Cheats are per game and off by default. The game page shows whether a game has cheats turned on.

## Engines and how each could do it

| Engine | Library games (engine survey) | How |
|---|---|---|
| RPG Maker MV and MZ | 89, mostly RPGs | The game runs in VNDegen's web view, so cheats are JavaScript calls on the game's own objects: `$gameParty`, `$gameVariables`, `$gameSwitches`, `$gamePlayer`, and the `$data*` databases for names. |
| RPG Maker XP, VX and VX Ace | 44, mostly RPGs | VNDegen's RGSS runtime runs the game's Ruby, so cheats are Ruby calls on `$game_party`, `$game_variables`, `$game_switches`, `$game_player` and the `$data_*` databases. They need a small call in `vnrgss.h` to run a snippet between frames. |
| Wolf RPG Editor | 23, RPGs and action | VNDegen's own runtime holds the variable and database tables, so cheats read and write them through new calls in `vnwolf.h`, with names from the user and system databases. |
| AliceSoft System 3 and 4 | 13, RPGs and strategy | The runtimes own the VM's variables, so cheats work through their C interfaces. Which variable holds gold or HP differs per game, so quick cheats need a small per-game table. The variable list works for every game. |
| Godot | 6, mixed | Games are compiled GDScript, with no shared notion of gold or HP. Game speed through `Engine.time_scale` is possible. Anything else would be per game and is out of scope at first. |
| Flash | 14, arcade and action | Ruffle has no stable way to reach a game's variables from outside. Game speed only, if Ruffle allows it. |

Visual novel engines (TyranoScript, Ren'Py, KiriKiri, NScripter, Artemis) don't get cheats. Their equivalents, skip and auto, already exist in the games or in the planned Game menu.

## Open questions for the maintainer

1. Should the variable editor be available, or only the quick cheats? The editor is powerful, and a wrong value can break a game's story flags. A save made before editing would be the undo.
2. Should game speed sit with the cheats, or in the Game menu for every engine that can do it?
3. Per-game tables for AliceSoft games, which say which variable holds gold and the like: worth building, or the variable list only?

## Suggested order

1. RPG Maker MV and MZ: the most library games, and the web view already allows script calls.
2. RPG Maker XP, VX and VX Ace on the RGSS runtime.
3. Wolf RPG Editor.
4. Game speed for every engine that supports it.
5. AliceSoft System 3 and 4.

Each engine's work is checked like an engine change: the Mac app and an iPad simulator, a save made after cheating loads on the PC, and a Codex review.
