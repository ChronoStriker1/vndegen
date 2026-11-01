# Feature request: cheats for games that aren't visual novels

Status: approved, in progress on `feature/cheats-request`. RPG Maker MV and MZ, RPG Maker XP, VX and VX Ace, Wolf RPG Editor, and AliceSoft System 3 and 4 are built, and game speed for Godot and Flash, see below.

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
3. AliceSoft System 3 and 4 get the variable list, and the Variables tab gets a find by value, which finds the variable that holds gold or HP in any game. System 3 games name their variables `VAR0000` and on, or not at all, so per-game tables would mean reading each game's script. System 4 gets gold for the library games whose globals name it. (Revised by the maintainer the same day: tables for System 3 were the first plan.)

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
- Game speed in MZ sets `SceneManager.determineRepeatNumber`, the count of engine updates each displayed frame runs, to the speed. MZ's extra updates to catch up after a slow frame are multiplied only when that frame spent under half its time on its own work, as on a screen below 60 Hz. Otherwise they kept each next frame slow too. MV runs an update for each 1/60 s its clock has moved, so its clock (`_getTimeInMsWithoutMobileSafari`) runs faster. Either way every update runs whole, input included, so the game's timers, animations and waits stay in step. Music plays at its normal speed.
- The library keeps the settings per game (`Game.cheats`). The game page lists them while any is on, and the session starts with them.

Checked against Rookie Knight Rathi (MV 1.6.1), Mama's Secret Parent Teacher Conference (MV 1.5.2) and No Socks RPG (MZ 1.9.0).

### Known limits

- Keep HP and MP full stops damage, skill costs and HP or MP the game takes away. It doesn't stop a state that knocks an actor out directly, such as an event's Change State to Knockout, or a plugin that sets HP or MP fields without the engine's setters.
- Walk through walls uses the player's through flag, as an event's Through option does. Followers still follow, and vehicles are unchanged.
- A plugin that replaces one of the wrapped methods after the boot scene starts, rather than when it loads, replaces the cheat with it. No library game was found doing so.
- Variables and switches past the length of the game's System lists aren't listed, since MV's and MZ's `setValue` ignores them.
- At 4x a frame on a 60 Hz screen runs four engine updates, so a game whose updates cost much draws fewer frames and runs below 4x. On its map 4, No Socks RPG draws 38 frames a second at 4x on an iPad simulator and its game runs at about 2.5x. Each update adds about 3.5 ms to the one texture upload of each frame, which looks like the game's own canvas drawing landing in that upload. The Mac holds 60 at 4x when it is otherwise quiet. Rookie Knight Rathi holds 60.

### Frame rate

Measured on the map with the DEBUG input script's `fps` step and a count of `Scene_Map.update` calls, 30 s each on the Mac and 20 s each on the iPad Pro 13-inch simulator. The Mac mini's load average and other work are in the evidence's `.env` files. RPG Maker MV and MZ target 60.

| Game | Device | Speed | Drawn fps | 1% low | Map updates per second |
|---|---|---|---|---|---|
| Rookie Knight Rathi (MV) | Mac | 1x | 60.0 | 52.6 | 60.0 |
| Rookie Knight Rathi (MV) | Mac | 4x | 60.0 | 52.6 | 240.2 |
| No Socks RPG (MZ) | Mac | 1x | 56.1 | 26.3 | 56.6 |
| No Socks RPG (MZ) | Mac | 4x | 60.0 | 52.6 | 240.1 |
| Rookie Knight Rathi (MV) | iPad simulator | 1x | 56.2 | 24.4 | 59.2 |
| Rookie Knight Rathi (MV) | iPad simulator | 4x | 59.6 | 35.7 | 240.0 |
| No Socks RPG (MZ) | iPad simulator | 1x | 51.5 | 29.4 | 52.4 |
| No Socks RPG (MZ) | iPad simulator | 4x | 38.2 | 33.3 | 152.8 |

The No Socks RPG rows at 4x were measured again on 2026-11-01 after the change to catch-up updates, on the Mac with a load average of 7 and on the simulator with 4. Before it they were 23.9 fps with 190.8 updates a second on the Mac, at a load average of 65, and 23 fps with 184 updates a second on the simulator. Simulator timing isn't iPad timing, and the Mac mini ran other threads' builds and simulators during several of these runs.

## Built: RPG Maker XP, VX and VX Ace

The same Cheats page, with the same four tabs and settings, for every game on VNDegen's RGSS runtime. Variables take whole numbers from -99,999,999 to 99,999,999, the range RPG Maker's own event commands use, and text. Other values are shown and left alone.

How it works:

- `vnrgss.h` gains `vnrgss_run_ruby` and `vnrgss_ruby_result`. The app queues Ruby, and the runtime runs it on the game thread while the game waits for its next frame. That is between two frames, with the game's objects in a consistent state, and it works while the game is paused under VNDegen's menu. The app polls for the result and waits for it however long the game takes, since a queued change can't be withdrawn. A request from a game that has ended answers "The game ended."
- `rgss-cheats.rb` (EngineRGSS's resources) loads at the game's first frame. A game's scripts can still be loading then, and can alias or replace methods afterwards, and no section name says when they're done. So the cheats wrap methods the way RGSS scripts wrap theirs: a method on the class that calls the one it replaced. A later script's alias chains through the wrap, and a call that reaches the same wrap again, as through such an alias, goes straight to the replaced method, so a cheat applies once and nothing loops. A wrap of a method the class inherits reaches it through `super`, so a later script's change to the superclass still applies. Every second while the game runs, the app queues `VNDegenCheats.install`, which wraps classes a later script defined and methods a later script replaced outright. It runs at the runtime's frame boundary, which no game script can replace. Methods aren't written into Marshal saves, so saves stay ordinary. What it wraps differs by version:

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
- A setting a later script breaks by replacing a method outright stops working until the next install, up to a second later.
- Each wrap passes calls through while it runs, so if a game's version of a setter sets the same value on another actor from inside it, that second actor isn't kept full.
- The editor can put a game into a state its own save can't restore. In Test Game 37, turning on switch 1 makes the game drop data its save expects, so a save made afterwards fails to load on the PC as well as here. The page says a save made before editing is the undo.

### Frame rate

Test Game 55, on the MacBook (120 Hz display) at a load average of 550 to 610, 40 s at each speed:

| Speed | Drawn fps | 1% low | Map updates per second |
|---|---|---|---|
| 1x | 60.0 | 45 to 51 | 60.0 |
| 4x | 114 to 119 | 29 to 70 | 240 |

## Built: Wolf RPG Editor

Wolf RPG Editor has no gold, items, party or switches of its own. Games keep their state in normal variables and in their databases, laid out as each game likes. So what the Cheats page offers depends on the game:

- **Every game**: the Variables tab, with the game's normal variables and the names its system database gives them (type 14), walk through walls, and game speed.
- **Games built on the editor's standard systems**, recognized by the changeable database's field names: gold (type 6, field 0, 所持金), items, weapons and armor by name (counts in types 7 to 9, names in the user database's types 2 to 4), and restoring the party's HP and SP (type 0, fields 6 to 9) for the members type 6 lists. Test Games 02 and 52 are such games. Test Game 53 keeps its own layout, and its money is normal variable 17, which the Variables tab edits.

How it works, in `Packages/VNDegenKit/Sources/CWolfRPG/core/cheats.cpp` and `vnwolf.h`:

- The runtime is VNDegen's own, so the cheats are calls on its `Game`: they read and write the same variables and database cells the game's events do, which the save writes as it always does.
- Walk through walls lets the hero pass where `canMove` would stop it, within the map.
- Game speed is `vnwolf_set_speed`: each step advances that many times as many of the game's frames, and draws the last. Sound fades a command starts take that much less time too, so they end with the waits they go with.
- After each change the runtime checks map event pages again, as it does when an event changes a variable, so a page whose condition the change meets shows.
- Gold is held between 0 and 999,999,999, and an item's count at 99 or what the game already allows above it.
- A game counts as using the standard systems only when every field these cheats read and write is where they expect it, as a number.

Checked: on CachyOS through the runtime with Test Games 02, 52 and 53; on the Mac app by controller with Test Game 52; on an iPad simulator by touch with Test Games 02 and 53; and on an iPhone simulator with Test Game 53, where the search found its money variable. Test Game 52's save made after cheating loads in the Windows engine (Wolf RPG Editor 3.282) under Wine with 1,010 yen, 10 Sedatives and full HP.

Test Game 52 on the MacBook (120 Hz display, load average about 30), 30 s at each speed, from the session's `-frameMonitor` log:

| Speed | Display ticks per second | New frames drawn per second | Slowest step |
|---|---|---|---|
| 1x | 120 | 57.7 (the game's 60) | 3.6 ms |
| 4x | 120 | 119.8 to 120 | 15 to 19 ms |

### Known limits

- Keeping HP and SP full, no random encounters, and the experience rate aren't offered: the standard systems run battles, encounters and experience in their own common events, and change HP in a battle copy of the party (the changeable database's type 10) that a cheat on type 0 wouldn't reach.
- Restore fills HP and SP to the hero's base maximum (fields 7 and 9). Equipment that raises the maximum raises it further only in the standard system's own calculation, so the restored value can be below what the status screen shows as the maximum.
- Spare variables, string variables and the databases themselves aren't listed.
- A standard system translated so far that its database field names changed isn't recognized, and gets the variables only.

## Built: game speed for Godot

Godot games keep their gold, items and party in their own scripts, each in its own way, so their Cheats page has game speed only: 1x, 2x or 4x, on the Quick tab, working from the title screen on.

It is Godot's own speed control, the one the Godot editor's Game view has (`Engine::set_user_time_scale`), through `vngd_set_speed` in `vngd.h`:

- It multiplies whatever time scale the game sets itself, so a game's slow motion still works, and the game reads back its own value.
- It runs more physics steps per frame, each as long as before, so collisions and physics behave as at 1x. The most physics steps a frame may run rises with the speed, so a frame running behind still catches up.
- Each frame's `_process` time, and so timers, tweens and animations, advance that many times as far.
- Godot 4.7 has the control. Godot 3.6 doesn't, and `Engines/Godot/3/patches/06-user-time-scale.patch` adds the same thing to its `Engine` and main loop.

Checked on CachyOS with Godot built from the pinned sources and VNDegen's patches, and a count of physics steps and game time added to the main loop for the check only, each 5 s of real time:

| Game | Speed | Physics steps | Game time | Step |
|---|---|---|---|---|
| Barely Working (Godot 3.6) | 1x | 300 | 5.0 s | 1/60 s |
| Barely Working (Godot 3.6) | 4x | 1,200 | 20.0 s | 1/60 s |
| Nitz's Debt (Godot 4.7) | 1x | 300 | 5.0 s | 1/60 s |
| Nitz's Debt (Godot 4.7) | 4x | 1,200 | 20.0 s | 1/60 s |

In the Mac app, the page set Nitz's Debt to 2x and 4x and Barely Working from 2x to 4x, and each change reached the engine. A game set to 4x started at 4x after a relaunch.

### Known limits

- A game that counts frames rather than time in `_process`, or that times itself with `Time.get_ticks_msec()` (`OS.get_ticks_msec()` in Godot 3), runs those parts at normal speed.
- Music and sounds play at their normal speed.

## Built: game speed for Flash

Ruffle has no way to reach a movie's variables from outside, so Flash games' Cheats page has game speed only: 1x, 2x or 4x, on the Quick tab.

Ruffle has no speed setting either. Its web build advances a movie by the time between the animation frames it asks the page for: it runs as many of the movie's frames as fit in that time, and moves the movie's ActionScript timers on by it. `flash-page.js` wraps `requestAnimationFrame` before Ruffle loads, so each animation frame tells Ruffle that the time since the last one, times the speed, has passed. Every callback in one animation frame gets the same time, and the time never goes back when the speed changes. A page the game opens next, such as a movie a launcher page opens, starts at the same speed.

Checked in the Mac app with Overthrow! The Demon Queen, a 24 fps movie, on a 60 Hz display. Over 5 s, the time the animation frames reported against the real time, and the frames Ruffle drew per second:

| Speed | Animation frame time / real time | Frames Ruffle drew per second |
|---|---|---|
| 1x | 1.000 | 24.1 |
| 2x | 2.000 | 47.9 |
| 4x | 4.001 | 59.8 (the display's limit) |

Set to 4x on the page while the movie ran, it measured 3.999 and 59.1.

### Known limits

- `getTimer()` reads the real clock, so a game that moves by the time `getTimer()` says has passed runs those parts at normal speed.
- A timeline's streamed sound, mostly in animated scenes, holds its movie clip to the sound, which plays at normal speed. Such a scene runs at normal speed.
- Ruffle runs at most 5 of a movie's frames per animation frame, and fewer when the movie's frames take long to run. A 60 fps movie at 4x on a 60 Hz display needs 4, and a 120 Hz display halves that.
- Music and sounds play at their normal speed.

## Built: AliceSoft System 3 and 4

Both runtimes are VNDegen's own, so their Cheats pages are calls on the runtime's variables, which the games' saves keep as always.

- **System 3** (`vns3.h`): the Variables tab lists the game's variables, 0 to 65,535 each, with the names `System39.ain` gives them, and game speed. Nine of the ten library games name them `VAR0000` and on, and Kichikuou Rance's fan translation names 24, none of them gold or HP. A `SYSTEM3.EXE` game (Rance 4.1 and 4.2) names none, and the tab lists the 512 variables its saves keep.
- **System 4** (`vns4.h`): the Variables tab lists the game's globals by name: whole numbers to edit, a bool taking 0 or 1, and decimals and text to read. Gold is on the Quick tab for the library games whose globals say which one holds the party's money: `GOLD` in Rance 02, `◆金` in Sengoku Rance and `g_nPartyMoney` in Evenicle. Rance IX, which the runtime doesn't run yet, keeps its money in a structure, so it would get the variables and speed only. The VM runs on a thread of its own, so the cheats work while it waits for the host's next step, which is whenever the game isn't in the middle of a long load.
- **Find by value**, on every engine's Variables tab: type the number the game shows, such as your gold, and the list keeps the variables that hold it. Change the number in the game, by buying something, then find the new number: the list keeps those of them that hold it, usually one. Show all clears it. The page keeps what it found while the game runs.
- **Game speed** runs the runtime's clock that many times as fast, so its waits, fades, animations and timers pass sooner. Music plays at normal speed, and a System 4 movie keeps to its sound on a clock of its own, so it plays at normal speed too.

Checked on CachyOS through the runtimes:

- System 3, Kichikuou Rance (`System39.ain`) and Rance 4.1 (`SYSTEM3.EXE`): 806 and 512 variables listed, a variable set to 4,242 and one held to 65,535, and the clock running 3,840 ms for 960 ms of frames at 4x. Kichikuou Rance's save written after the change, in its own `KICHIKUSA.ASD`, loaded with the new value.
- System 4, Rance 02 and Sengoku Rance: 218 and 107 globals listed, gold found and raised by 1,000, a bool refusing 4,242, and a global of another type refused.

Checked in the app on an iPad simulator (the page and the runtime calls are the same on every device): in Kichikuou Rance, finding 24 kept the two variables holding it, and after one changed to 30, finding 30 kept that one. In Rance 02, the Quick tab showed Gold 0, +10,000 made it 10,000, and the Variables tab's search found `GOLD` at 10,000.

### Known limits

- Party HP, items and the like live in each game's own structures, not in single variables, and aren't offered. Find by value reaches one that is a single variable.
- A System 4 global that is a structure, an array or a reference isn't listed.
- In System 3, the game's clock is also where it reads the time of day, which runs that much faster too.
