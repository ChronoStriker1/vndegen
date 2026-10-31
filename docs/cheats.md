# Feature request: cheats for games that aren't visual novels

Status: approved, in progress on `feature/cheats-request`. RPG Maker MV and MZ, and XP, VX and VX Ace, are built, see below.

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

Visual novel engines (TyranoScript, Ren'Py, KiriKiri, NScripter, Artemis) don't get cheats. Their equivalents, skip and auto, already exist in the games or in the Game commands entry of the VNDegen menu.

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
- At 4x every displayed frame runs four engine updates, and MZ runs more after a slow frame to catch up, so a game whose updates are expensive draws fewer frames. No Socks RPG, with 87 VisuStella plugins, drew about 24 frames a second at 4x on the Mac mini and 26 on an iPad simulator, while its game ran at about 3.2x. Rookie Knight Rathi held 60.

### Frame rate

Measured on the map with the DEBUG input script's `fps` step and a count of `Scene_Map.update` calls, 30 s each on the Mac and 20 s each on the iPad Pro 13-inch simulator. The Mac mini's load average and other work are in the evidence's `.env` files. RPG Maker MV and MZ target 60.

| Game | Device | Speed | Drawn fps | 1% low | Map updates per second |
|---|---|---|---|---|---|
| Rookie Knight Rathi (MV) | Mac | 1x | 60.0 | 52.6 | 60.0 |
| Rookie Knight Rathi (MV) | Mac | 4x | 60.0 | 52.6 | 240.2 |
| No Socks RPG (MZ) | Mac | 1x | 56.1 | 26.3 | 56.6 |
| No Socks RPG (MZ) | Mac | 4x | 23.9 | 14.1 | 190.8 |
| Rookie Knight Rathi (MV) | iPad simulator | 1x | 56.2 | 24.4 | 59.2 |
| Rookie Knight Rathi (MV) | iPad simulator | 4x | 59.6 | 35.7 | 240.0 |
| No Socks RPG (MZ) | iPad simulator | 1x | 51.5 | 29.4 | 52.4 |
| No Socks RPG (MZ) | iPad simulator | 4x | 25.7 | 17.9 | 206.4 |

Simulator timing isn't iPad timing, and the Mac mini ran other threads' builds and simulators during several of these runs.

## Built: RPG Maker XP, VX and VX Ace

The same Cheats page, with the same four tabs and settings, for every game on VNDegen's RGSS runtime. Variables take whole numbers from -99,999,999 to 99,999,999, the range RPG Maker's own event commands use, and text. Other values are shown and left alone.

How it works:

- `vnrgss.h` gains `vnrgss_run_ruby` and `vnrgss_ruby_result`. The app queues Ruby, and the runtime runs it on the game thread while the game waits for its next frame. That is between two frames, with the game's objects in a consistent state, and it works while the game is paused under VNDegen's menu. The app polls for the result and waits for it however long the game takes, since a queued change can't be withdrawn. A request from a game that has ended answers "The game ended."
- `rgss-cheats.rb` (EngineRGSS's resources) loads at the game's first frame. A game's scripts can still be loading then, and can alias or replace methods afterwards, and no section name says when they're done. So the cheats wrap methods the way RGSS scripts wrap theirs: a method on the class that calls the one it replaced. A later script's alias chains through the wrap, and a call that reaches the same wrap again, as through such an alias, goes straight to the replaced method, so a cheat applies once and nothing loops. A wrap on `Graphics.update` wraps again, every 60 frames, whatever a later script replaced outright. Methods aren't written into Marshal saves, so saves stay ordinary. What it wraps differs by version:

| | XP | VX | VX Ace |
|---|---|---|---|
| Keep HP and MP full | `Game_Actor#hp=` and `sp=` | `hp=` and `mp=` | `hp=` and `mp=` |
| No random encounters | `Game_Player#encounter_count`, which the map scene waits to reach 0 | the same | `Game_Player#encounter` |
| Walk through walls | `Game_Player#passable?`, within the map | `debug_through?`, as Control in a playtest | `debug_through?` |
| Experience | `Game_Actor#exp=`, through which battles and events add it | `gain_exp` | `final_exp_rate` |

- One-shot cheats call the game's own methods: `gain_gold`, `gain_item` (XP's `gain_weapon` and `gain_armor` too), `recover_all`, and `$game_variables` and `$game_switches`, which also refresh the map. They wait until a game is started or loaded: a title scene starting clears that, and a map scene starting sets it.
- Game speed is `vnrgss_set_speed`. Each step lets the game draw that many times as many frames, and the runtime composites only the frames that will be shown.
- Text a player types reaches Ruby as its characters' numbers, `[…].pack('U*')`, so no text runs as code. A string literal won't do: Ruby interpolates `#{` in a double-quoted string even when the `#` is an escape.

Checked against Test Games 14 and 55 (VX Ace), 33 (VX) and 37 (XP).

### Known limits

- Keep HP and MP full doesn't stop a knockout state an event adds directly, as in MV and MZ.
- XP's walk through walls replaces `Game_Player#passable?`, so it ignores what a game's own override of that method checks.
- A setting a later script breaks by replacing a method outright stops working until the next re-wrap, up to 60 frames later.
- Each wrap passes calls through while it runs, so if a game's version of a setter sets the same value on another actor from inside it, that second actor isn't kept full.
- The editor can put a game into a state its own save can't restore. In Test Game 37, turning on switch 1 makes the game drop data its save expects, so a save made afterwards fails to load on the PC as well as here. The page says a save made before editing is the undo.

### Frame rate

Test Game 55, on the MacBook (120 Hz display) at a load average of 550 to 610, 40 s at each speed:

| Speed | Drawn fps | 1% low | Map updates per second |
|---|---|---|---|
| 1x | 60.0 | 45 to 51 | 60.0 |
| 4x | 114 to 119 | 29 to 70 | 240 |
