# Engines

VNDegen runs each game on its own engine, unmodified. Each engine is a plugin that recognizes its own files, such as `js/rmmz_core.js` for RPG Maker MZ. The game page shows the detected engine and the file that identified it. You can assign an engine by hand to a game VNDegen doesn't detect.

Settings, Engines, Plugins lists every engine plugin built into the app, the engine versions it covers, what runs its games and under which license, how many of your games use it, and whether it runs on this device. Godot doesn't run on an Intel Mac, for example. Plugins are compiled into the app, so they can't be added or removed.

Web-based engines render through WebGL on Metal. Ren'Py and Godot 3 render through Apple's OpenGL on Mac and through ANGLE's Metal backend on iPhone and iPad. Godot 4 renders on Metal.

| Engine | Detected by | Platforms |
|---|---|---|
| [RPG Maker MZ](#rpg-maker-mv-and-mz) | `js/rmmz_core.js` | Mac, iPhone, iPad |
| [RPG Maker MV](#rpg-maker-mv-and-mz) | `js/rpg_core.js` or `www/js/rpg_core.js` | Mac, iPhone, iPad |
| [TyranoScript and TyranoBuilder](#tyranoscript) | `tyrano/plugins/kag/kag.js` at the top of the game, or in an NW.js (`package.nw`) or Electron (`resources/app`, also inside `app.asar`) build | Mac, iPhone, iPad |
| [Ren'Py 6.99 to 8](#renpy) | `renpy/` beside a `game/` folder holding `.rpy`, `.rpyc`, or `.rpa` files, also one or two folders down | Mac, iPhone, iPad |
| [RPG Maker XP, VX, and VX Ace](#rpg-maker-xp-vx-and-vx-ace) | `Game.ini` beside `Game.rgssad`, `Game.rgss2a`, or `Game.rgss3a`, or beside `Data/Scripts.rxdata`, `.rvdata`, or `.rvdata2`, also one or two folders down | Mac, iPhone, iPad |
| [KiriKiri 2 and KiriKiri Z](#kirikiri) | `data.xp3`, a `data/` folder holding `startup.tjs`, or an archive appended to the game's `.exe`, beside the `.exe` and up to two folders down | Mac, iPhone, iPad |
| [Artemis](#artemis) | a `.pfs` archive, such as `root.pfs`, whose first bytes are `pf2`, `pf6`, or `pf8`, or a `system.ini` that names the script to boot, up to two folders down | Mac, iPhone, iPad |
| [Godot 3 and Godot 4](#godot) | a `.pck` file, usually beside the game's `.exe`, or a pack inside the `.exe`, up to two folders down | Mac with Apple silicon, iPhone, iPad |
| [Flash (ActionScript 1, 2, and 3)](#flash) | A Flash projector `.exe` with the movie appended, a `.swf` movie, or an HTML page that launches movies, up to four folders down | Mac, iPhone, iPad |
| [NScripter](#nscripter) | `nscript.dat`, or `0.txt` or `00.txt` beside an `.nsa`, `.ns2`, or `.sar` archive or NScripter's own files, up to two folders down | Mac, iPhone, iPad |
| [Wolf RPG Editor 2 and 3](#wolf-rpg-editor) | `Data.wolf`, `Data/BasicData`, or `Data/BasicData.wolf`, up to two folders down. `Game.exe` or `GamePro.exe` beside it makes the match certain | Mac, iPhone, iPad |
| [AliceSoft System 3.0 to 3.9](#alicesoft-system-3) | `System39.ain` beside a scenario archive such as `RanceSA.ALD`, or `ADISK.DAT`, up to two folders down. `SYSTEM3.EXE` beside `ADISK.DAT` makes the match certain | Mac, iPhone, iPad |
| [AliceSoft System 4](#alicesoft-system-4) | an `.ain` code file beside `AliceStart.ini`, `System40.ini`, or `System40.exe`, up to two folders down | Mac, iPhone, iPad |

[engine-survey.md](engine-survey.md) covers which other engines are common and how each could be supported. [development.md](development.md#adding-an-engine) describes how to add one.

## RPG Maker MV and MZ

RPG Maker MV and MZ games ship as HTML5 inside NW.js. VNDegen runs them in WebKit with a small NW.js compatibility layer, so plugins that call Node's `fs`, `path`, or `nw.gui` keep working. The layer also means saves land where the PC version puts them, in `save/` beside `index.html` (`www/save/*.rpgsave` for MV, `save/*.rmmzsave` for MZ), and encrypted images and audio, Effekseer effects, and WebM movies all work.

WebKit can't decode Ogg Vorbis, so MZ uses the decoder it ships with, and MV games that ship only Ogg audio, as most do, play it through VNDegen's own Ogg decoder, the one TyranoScript uses.

Up to 0.13.0, MV games on the community core (`rpg_managers.js` "community-1.3b") saved to the game's top-level `save/` in VNDegen, because the layer lacked the `fs.rmdirSync` that core uses to test `www/`. The first launch of such a game after updating moves those saves into `www/save/` and adds them to the game's slot index there, unless a slot is in both folders or the move would hide a save, in which case both folders stay. Early MV cores, such as 1.2.0, save to the game's top-level `save/`, as they do on the PC.

MV and MZ games also have a Cheats page in the VNDegen menu, described in [controls.md](controls.md#cheats).

## TyranoScript

TyranoScript games run on the same WebKit host. Their saves stay in the game folder as on PC, except for builds that save to browser storage, which VNDegen keeps per game. iOS cannot play Ogg, so VNDegen decodes Ogg Vorbis inside the game's page with libvorbis compiled to WebAssembly, the first time each track plays.

## Ren'Py

Ren'Py games run on Ren'Py's own engine. The Mac app bundles two official Ren'Py runtimes, 8.5.3 for Ren'Py 8 games and 7.8.7 for Ren'Py 7 and older games, and picks one from the game's `renpy/vc_version.py` or `renpy/__init__.py`, then its `lib/` folder names, then the engine's compiled files. Each game runs in its own process and window, which takes the hub window's place, and the hub comes back when the game quits. Saves and logs go to the game's data folder, and the game folder is never written to. A game copied from a PC with saves in `game/saves/` gets those copied in on its first launch.

On iPhone and iPad, Ren'Py 8.5.3 is built into the app and every Ren'Py game runs on it, inside VNDegen's window. Ren'Py 7 and 6.99 games run on it too, with fixes VNDegen applies in memory, such as dividing integers the Python 2 way in the game's own code. 15 of the 16 Ren'Py 7 games in the maintainer's library reached their main menu this way, and VNDegen ships that list. The page of a Ren'Py 7 game says whether it is known to work and, once it has run, whether it reached its main menu on this device. On Mac the game page's Ren'Py runtime option runs a game on the other runtime.

Ren'Py can't load saves made by a newer Ren'Py. VNDegen runs each game on the newest Ren'Py of its line, so saves made here may not load in the game's PC version, and the game's Saves page warns before a Ludusavi backup when that applies. VNDegen on other devices loads them. The exception is a Mac, which runs Ren'Py 7 games on Ren'Py 7.8.7, so it loads a Ren'Py 7 game's saves from an iPhone or iPad only once that game's Ren'Py runtime option is set to Ren'Py 8.5.3. A Ren'Py 7 game's persistent data, such as unlocked gallery images, is written so its PC version loads it.

[renpy-spike.md](renpy-spike.md) records how the runtimes were chosen and built.

## RPG Maker XP, VX, and VX Ace

RPG Maker XP, VX, and VX Ace games run on VNDegen's own RGSS runtime, which runs the game's Ruby scripts on MRI Ruby, an interpreter with no JIT, and draws each frame on the CPU for the app to show through Metal. The VNDegen menu and touch controls work as for the other engines. Packed games (`Game.rgss3a` and its older forms) and unpacked ones both work, as do MIDI music, through the General MIDI SoundFont in Settings, and the Windows API calls the test games make, which the runtime emulates. Saves go to the game's data folder in the PC format, so saves copied in with a PC game still load and VNDegen's saves load on PC, and the game folder is never written to. Games that need a Windows DLL VNDegen doesn't emulate lose that DLL's feature, and a game whose scripts need something the runtime lacks stops with an error, which the game page then shows. [rgss.md](rgss.md) has the details and known limits.

### The RTP

Many RPG Maker games use the RTP, a set of stock graphics, sounds, and fonts that the PC player installs once instead of every game shipping it. Its license doesn't let VNDegen include it. When a game needs an RTP that isn't installed, its page offers Download and install, which fetches the official installer from the RPG Maker website (rpgmakerweb.com), checks it against the SHA-256 VNDegen expects, shows the RTP's license to accept, and unpacks it inside the app on Mac, iPhone, and iPad. The RTP is then shared by every game that needs it. Import from folder takes an RTP folder copied from a PC instead, such as `C:\Program Files (x86)\Common Files\Enterbrain\RGSS3\RPGVXAce`. The XP RTP is 23 MB to download, VX's 37 MB, and VX Ace's 195 MB. Nothing downloads until you tap the button.

XP, VX, and VX Ace games have the same Cheats page as MV and MZ, described in [controls.md](controls.md#cheats).

## KiriKiri

KiriKiri games run on krkrsdl3, an SDL3 port of the KiriKiri engine, which VNDegen builds into the app. The game draws inside VNDegen's window, composited on the CPU and shown through Metal, so the VNDegen menu and touch controls work as for the other engines. Saves stay in the game's `savedata/` folder as on PC. A game that saves to the Windows AppData or Documents folder saves to its data folder instead, and Ludusavi backups from Windows or a Steam Deck restore there.

Movies play, in the formats KiriKiri games ship: MPEG-1 and MPEG-2, WMV, and H.264 and HEVC, which decode in hardware. Encrypted archives work without any per-game setting: VNDegen recognizes the common ciphers by the checksums inside the archive, and tells you when a game uses one it doesn't know. Common Windows plugins are built in, including `extrans`, `layerExDraw`, `layerExSave`, and Steam builds' `krkrsteam`.

A KiriKiri game's window menu bar appears under Game commands in the VNDegen menu, described in [controls.md](controls.md#game-commands).

Known limits: sound effects and music stored as MP3 don't play, AlphaMovie (`.amv`) animations don't show, and a KiriKiri game that is still starting with a question on screen when the Mac logs out or shuts down doesn't save its settings from that moment. [kirikiri-spike.md](kirikiri-spike.md) records how the port was chosen.

## Artemis

Artemis games run on art3m1s-core, an open-source Rust reimplementation of the Artemis engine, built into the app with Lua 5.1 and no JIT. The game draws inside VNDegen's window through the core's Metal renderer, so the VNDegen menu and touch controls work as for the other engines. Saves are the PC engine's own files, in both of the versions it writes, so a save made in VNDegen loads in the PC version at the same page and the other way round. They go to the game's data folder, under the folder name the PC version uses in AppData or beside the game, so Ludusavi backups from Windows restore there. Pages break where the PC version breaks them. Known limits: movies and HLSL shader effects don't play, and in games that ship bitmap fonts (`.rft`) only ASCII comes from those fonts, with MS Gothic for the rest. [artemis.md](artemis.md) has the details.

## Godot

Godot games run on Godot itself, which VNDegen builds into the app twice: Godot 4.7 runs every Godot 4 game, and Godot 3.6 every Godot 3 game. A game's pack loads unchanged, from its `.pck` or from inside its `.exe`, and games exported with older Godot versions run too, such as Godot 4.2 to 4.5 and Godot 3.3 and 3.5. The game draws inside VNDegen's window: Godot 4 through Metal on Mac, iPhone, and iPad, and Godot 3 through Apple's OpenGL on Mac and through ANGLE's Metal backend on iPhone and iPad. Controllers reach games as an Xbox controller, and touches and the mouse as Godot's mouse.

Saves go where the PC version keeps them: `user://` is under the Windows AppData folder's stand-in in the game's data folder, so Ludusavi backups from Windows, a Steam Deck, or Godot's Linux and Mac builds restore there, and games that save beside their `.exe` save in their own folder. Games built with Steam through GodotSteam run as Steam does offline, with the player signed in, and games that also ship Epic Online Services for their Epic Games Store build run without it. Packs encrypted with a key built into the game's own copy of Godot run too: VNDegen finds the key in the game's `.exe`.

Games that can't run say why on their page: games that need other GDExtension or GDNative libraries, which are Windows code, and encrypted games whose key VNDegen doesn't find. Godot C# games aren't supported. [godot.md](godot.md) has the details and the known limits. The Cheats page in the VNDegen menu runs Godot games at 2x or 4x speed.

## Flash

Flash games run in Ruffle, a Flash Player written in Rust, built for the web and run on the same WebKit host with WebGL. VNDegen plays a game's Windows projector (`Game.exe` with the movie appended) without unpacking it, a `.swf` movie, or a collection's own HTML menu page. Movies a game loads, and its saves, are named as they are beside the PC executable. Saves are the `.sol` files Flash Player writes, kept in the game's data folder by the movie that made them, so saves from Flash Player or Ruffle on a PC restore through Ludusavi and load. ActionScript 3 support in Ruffle is less complete than for ActionScript 1 and 2, so some AS3 games may stop or misbehave. [flash.md](flash.md) has the details. The Cheats page in the VNDegen menu runs Flash games at 2x or 4x speed.

## NScripter

NScripter games run on VNDegen's own NScripter runtime, written in C++ for VNDegen from NScripter's documentation and the games' files, MIT licensed like the rest of the app. It draws on the CPU and shows its frames through Metal inside VNDegen's window. It reads the engine's archives, including bitmaps and entries its packer compressed, and plays Ogg, WAV, MP3 and NBZ sound, CD tracks from a game's `CD` folder, and MPEG-1 movies. Games that script part of their logic in Lua (NSLua) run on an embedded Lua 5.1. Saves are the PC engine's own files, byte for byte, in the game's save folder, so they move between VNDegen and a PC copy of the game, and Ludusavi backs them up. English translations whose patched engine wraps text between words get the same word wrap. Known limits: of the effect DLLs only the ones a test game uses are drawn, others crossfade, and NSLua's direct drawing functions aren't emulated. [nscripter.md](nscripter.md) has the details.

## Wolf RPG Editor

Wolf RPG Editor games run on VNDegen's own Wolf RPG Editor runtime, written in C++ for VNDegen from the games' files, permissively licensed format notes and the original engine's behaviour, MIT licensed like the rest of the app. It draws on the CPU and shows its frames through Metal inside VNDegen's window. It reads the games' data, maps and databases, loose or packed in Wolf RPG Editor 2's encrypted `.wolf` archives, runs their event scripts, and plays Ogg, MP3, WAV and MIDI sound, MIDI through a bundled General MIDI SoundFont or one the player chooses in Settings. Text uses a font the game ships, then VNDegen's Windows font pack. Saves are the PC engine's own files in the game's `Save` folder, so Ludusavi backs them up and they move between VNDegen and a PC copy of the game. Known limits: Wolf RPG Editor 3.5's compressed data, newer archive versions, and Pro's encrypted data aren't read yet. [wolfrpg.md](wolfrpg.md) has the details.

Wolf RPG Editor games have a Cheats page too, with what each game keeps where the cheats can find it, described in [cheats.md](cheats.md#built-wolf-rpg-editor).

## AliceSoft System 3

AliceSoft's System 3 games run on VNDegen's own System 3 runtime, written in C for VNDegen from AliceSoft's published System 3.5 and 3.9 SDKs, the games' files, and the original engines' observable behaviour, MIT licensed like the rest of the app. It runs both the System 3.5 to 3.9 games (`System39.exe`) and the older Windows 3.1 games (`SYSTEM3.EXE`), drawing on the CPU and showing its frames through Metal inside VNDegen's window. CD music plays from the MP3 files the game's `_inmm.ini` lists, MIDI music through the bundled SoundFont or the player's own, and the English releases of `SYSTEM3.EXE` games that get their text from `replacement.txt` show it. Saves are the PC engines' own files in the game's folder, byte for byte what `System39.exe` and `SYSTEM3.EXE` write, so they move between VNDegen and a PC copy of the game. Ludusavi's manifest doesn't list the test games yet, so Ludusavi doesn't back them up on its own. [system3.md](system3.md) has the details and the known limits. The Cheats page in the VNDegen menu edits the game's variables and runs it at 2x or 4x speed, described in [cheats.md](cheats.md#built-alicesoft-system-3-and-4).

## AliceSoft System 4

AliceSoft's System 4 games run on VNDegen's own System 4 runtime, written in C++ for VNDegen from AliceSoft's System 4.0 SDK, the games' files and the PC engine's observable behaviour, MIT licensed like the rest of the app. It runs the games' compiled code on its own virtual machine, draws on the CPU and shows its frames through Metal inside VNDegen's window. It reads the games' `.ald` archives and their QNT, AJP, PMS and WebP pictures, plays Ogg and WAV sound and the MPEG-1 movies, and draws text with the game's own `.fnl` font library where it has one, then VNDegen's Windows font pack. Saves are the PC engine's own files in the game's `SaveData` folder, checked against saves the PC games wrote: a save moves between VNDegen and a PC copy of the game in both directions, and Ludusavi backs them up. Known limits: games built on PartsEngine and the libraries that came with it (with `.ex` data files), such as Rance IX, aren't supported yet; their game page says so. [system4.md](system4.md) has the details and the known limits. The Cheats page in the VNDegen menu edits the game's globals, adds gold in the games whose globals name it, and runs the game at 2x or 4x speed, described in [cheats.md](cheats.md#built-alicesoft-system-3-and-4).
