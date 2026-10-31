# Feature request: cheats for games that aren't visual novels

Status: approved, in progress on `feature/cheats-request`. RPG Maker MV and MZ are built, see below.

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

## Decisions (maintainer, 2026-10-31)

1. The variable and switch editor is included. A save made before editing is the undo, and the editor says so.
2. Game speed sits with the cheats, in every engine that can do it.
3. AliceSoft System 3 and 4 get per-game tables that say which variable holds gold, HP and the like, for the library games that have them. The variable list works for every game.

## Suggested order

1. RPG Maker MV and MZ: the most library games, and the web view already allows script calls.
2. RPG Maker XP, VX and VX Ace on the RGSS runtime.
3. Wolf RPG Editor.
4. Game speed for every engine that supports it.
5. AliceSoft System 3 and 4.

Each engine's work is checked like an engine change: the Mac app and an iPad simulator, a save made after cheating loads on the PC, and a Codex review.

## Built: RPG Maker MV and MZ

The Cheats entry sits in the VNDegen menu, between Resume (or Game commands) and Quit to library, for every MV and MZ game. Its page has four tabs:

- **Quick**: add 1,000, 10,000 or 100,000 gold (the game's own maximum still applies), restore the party's HP and MP, and the settings that stay on: keep HP and MP full, no random encounters, walk through walls, game speed 1x, 2x or 4x, and experience 1x, 2x, 5x or 10x.
- **Items**: the game's items, weapons and armor by name, with how many the party has, and +1 or +10.
- **Variables** and **Switches**: every entry in the game's System database, with its name, a search by name or number, and editing in place. Whole numbers and text are editable. Other values a script keeps in a variable, such as arrays, are shown and left alone, since setting them from a text field could break the game.

How each works, in `Packages/VNDegenKit/Sources/EngineRPGMaker/Resources/rpgmaker-cheats.js`:

- One-shot cheats call the game's own methods: `$gameParty.gainGold`, `gainItem`, `recoverAll`, `$gameVariables.setValue` and `$gameSwitches.setValue`. They wait until a game is started or loaded, because the title screen's game objects are replaced when one is.
- Settings that stay on wrap engine methods once, when `SceneManager.run` starts the boot scene, which both MV and MZ call after every plugin has loaded: `Game_BattlerBase.setHp`, `setMp` and `paySkillCost` for actors, `Game_Player.canEncounter`, `Game_Player.isThrough`, and `Game_Actor.finalExpRate`. None of them write anything a save keeps, so turning one off leaves no trace in the next save.
- Game speed in MZ multiplies `SceneManager.determineRepeatNumber`, the count of engine updates each displayed frame runs. MV runs an update for each 1/60 s its clock has moved, so its clock (`_getTimeInMsWithoutMobileSafari`) runs faster. Either way every update runs whole, input included, so the game's timers, animations and waits stay in step. Music plays at its normal speed.
- The library keeps the settings per game (`Game.cheats`). The game page lists them while any is on, and the session starts with them.

Checked against Rookie Knight Rathi (MV 1.6.1), Mama's Secret Parent Teacher Conference (MV 1.5.2) and No Socks RPG (MZ 1.9.0).

### Known limits

- Keep HP and MP full stops damage, skill costs and HP or MP the game takes away. It doesn't stop a state that knocks an actor out directly, such as an event's Change State to Knockout, or a plugin that sets HP or MP fields without the engine's setters.
- Walk through walls uses the player's through flag, as an event's Through option does. Followers still follow, and vehicles are unchanged.
- A plugin that replaces one of the wrapped methods after the boot scene starts, rather than when it loads, replaces the cheat with it. No library game was found doing so.
- Variables and switches past the length of the game's System lists aren't listed, since MV's and MZ's `setValue` ignores them.
- At 4x a heavy scene runs four engine updates per displayed frame, so a game that is already slow on a device draws fewer frames, though its timing stays right.
