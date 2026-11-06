# Engines

VNDegen runs each game on its own engine, unmodified. Each engine is a plugin that recognizes its own files, such as `js/rmmz_core.js` for RPG Maker MZ. The game page shows the detected engine and the file that identified it. You can assign an engine by hand to a game VNDegen doesn't detect.

Settings, Engines, Plugins lists every engine plugin built into the app, the engine versions it covers, what runs its games and under which license, how many of your games use it, and whether it runs on this device. Godot doesn't run on an Intel Mac, for example. Plugins are compiled into the app, so they can't be added or removed.

Web-based engines render through WebGL on Metal. Ren'Py and Godot 3 render through Apple's OpenGL on Mac and through ANGLE's Metal backend on iPhone and iPad. Godot 4 renders on Metal.

| Engine | Detected by | Platforms |
|---|---|---|
| [RPG Maker MZ](#rpg-maker-mv-and-mz) | `js/rmmz_core.js`, also inside up to three wrapper folders, as a likely match | Mac, iPhone, iPad |
| [RPG Maker MV](#rpg-maker-mv-and-mz) | `js/rpg_core.js` or `www/js/rpg_core.js`, also inside up to three wrapper folders, as a likely match | Mac, iPhone, iPad |
| [TyranoScript and TyranoBuilder](#tyranoscript) | `tyrano/plugins/kag/kag.js` at the top of the game, or in an NW.js (`package.nw`) or Electron (`resources/app`, also inside `app.asar`) build, also inside up to two wrapper folders, as a likely match | Mac, iPhone, iPad |
| [Ren'Py 6.99 to 8](#renpy) | `renpy/` beside a `game/` folder holding `.rpy`, `.rpyc`, or `.rpa` files, also one or two folders down | Mac, iPhone, iPad |
| [RPG Maker XP, VX, and VX Ace](#rpg-maker-xp-vx-and-vx-ace) | `Game.ini` beside `Game.rgssad`, `Game.rgss2a`, or `Game.rgss3a`, or beside `Data/Scripts.rxdata`, `.rvdata`, or `.rvdata2`, also one or two folders down | Mac, iPhone, iPad |
| [KiriKiri 2 and KiriKiri Z](#kirikiri) | `data.xp3`, a `data/` folder holding `startup.tjs`, or an archive appended to the game's `.exe`, beside the `.exe` and up to two folders down, or three when the folder holds little else | Mac, iPhone, iPad |
| [Artemis](#artemis) | a `.pfs` archive, such as `root.pfs`, whose first bytes are `pf2`, `pf6`, or `pf8`, or a `system.ini` that names the script to boot, up to two folders down | Mac, iPhone, iPad |
| [Godot 3 and Godot 4](#godot) | a `.pck` file, usually beside the game's `.exe`, or a pack inside the `.exe`, up to two folders down | Mac with Apple silicon, iPhone, iPad |
| [Flash (ActionScript 1, 2, and 3)](#flash) | A Flash projector `.exe` with the movie appended, a `.swf` movie, or an HTML page that launches movies, up to four folders down | Mac, iPhone, iPad |
| [NScripter](#nscripter) | `nscript.dat`, or `0.txt` or `00.txt` beside an `.nsa`, `.ns2`, or `.sar` archive or NScripter's own files, up to two folders down | Mac, iPhone, iPad |
| [Wolf RPG Editor 2 and 3](#wolf-rpg-editor) | `Data.wolf`, `Data/BasicData`, or `Data/BasicData.wolf`, up to two folders down. `Game.exe` or `GamePro.exe` beside it makes the match certain | Mac, iPhone, iPad |
| [AliceSoft System 3.0 to 3.9](#alicesoft-system-3) | `System39.ain` beside a scenario archive such as `RanceSA.ALD`, or `ADISK.DAT`, up to two folders down. `SYSTEM3.EXE` beside `ADISK.DAT` makes the match certain | Mac, iPhone, iPad |
| [AliceSoft System 4](#alicesoft-system-4) | an `.ain` code file beside `AliceStart.ini`, `System40.ini`, or `System40.exe`, up to two folders down | Mac, iPhone, iPad |
| [NekoSDK](#nekosdk) | `cfg.pak` beside `scr.pak`, up to two folders down | Mac, iPhone, iPad |
| [Unity](#unity), opened outside VNDegen | `Game_Data` holding `globalgamemanagers`, `mainData`, or `data.unity3d`, up to two folders down, as a likely match when `Game.exe` or `UnityPlayer.dll` is beside it and a weak one otherwise | Mac only, through the game's own Mac version or the player's CrossOver. iPhone and iPad: the game's page says why not |

[engine-survey.md](engine-survey.md) covers which other engines are common and how each could be supported. [development.md](development.md#adding-an-engine) describes how to add one.

## RPG Maker MV and MZ

RPG Maker MV and MZ games ship as HTML5 inside NW.js. VNDegen runs them in WebKit with a small NW.js compatibility layer, so plugins that call Node's `fs`, `path`, `util`, or `nw.gui` keep working. The layer also means saves land where the PC version puts them, in `save/` beside `index.html` (`www/save/*.rpgsave` for MV, `save/*.rmmzsave` for MZ), and encrypted images and audio, Effekseer effects, and WebM movies all work.

WebKit can't decode Ogg Vorbis, so MZ uses the decoder it ships with, and MV games that ship only Ogg audio, as most do, play it through VNDegen's own Ogg decoder, the one TyranoScript uses.

Up to 0.13.0, MV games on the community core (`rpg_managers.js` "community-1.3b") saved to the game's top-level `save/` in VNDegen, because the layer lacked the `fs.rmdirSync` that core uses to test `www/`. The first launch of such a game after updating moves those saves into `www/save/` and adds them to the game's slot index there, unless a slot is in both folders or the move would hide a save, in which case both folders stay. Early MV cores, such as 1.2.0, save to the game's top-level `save/`, as they do on the PC.

iOS can end WebKit's GPU process, which loses the page's WebGL context. MZ games and MV games on PIXI 4 recover from that with their map, sprites, and battles drawn as before. The earliest MV cores use PIXI 2, and those games stay black until the game restarts: The Midsummer Night's Sabbat and Queena's Legend in the library.

MV and MZ games also have a Cheats page in the VNDegen menu, described in [controls.md](controls.md#cheats).

A game can name its files with Windows backslashes, as the Demon Queen game does for 277 of its 313 video references, and they load as they do on the PC. [development.md](development.md#file-name-case) has the details.

WebKit ignores the alpha channel of VP9 WebM clips, so a see-through clip would show as a black or white box. The first time a game plays one, VNDegen converts it in the background to HEVC with alpha, which Apple's system plays with transparency, keeps it in a cache outside the game folder, and serves it at the clip's original address, so the game and its movie plugin don't change. The clip stays hidden until it's ready, so its first play can wait, and later plays start straight away. Settings has a Clear video cache button. [video.md](video.md) has the details.

Known limits for those clips, from a scan of the movies in the library's 118 MV and MZ games and No Socks RPG on 2026-11-01:

- Converting uses a lot of memory while it runs: about 160 MB on a Mac and up to about 390 MB on an iPad simulator. Real iPad speed and memory haven't been measured.
- Four games were checked through their own plugin commands: Cornelica, Holding Hands, Fallen Kunoichi, and No Socks RPG. The other seven with clips weren't played through. Five Nights with Maids and Lovey-Dovey Lockdown name no clips in their data, so they may not use them.
- Only VP9 clips with 8-bit 4:2:0 video and Opus or no sound are converted. VP8 clips with alpha and VP9 clips in profile 2 or 3 aren't, and no library game has one.
- `siru.webm` (50 by 200) in Trinity Dungeon is the library's only clip under 64 pixels on a side, and it hasn't been played through the converter.
- The scan could not see inside the exe of Futanari Remedies, Succubus Farm, or Treasure Hunter Koo and the Ancient King, which keep their files there.

Other looping WebM clips reload at their end rather than use WebKit's loop, which stops about 500 ms at each turn and never finishes on an iPad simulator for a WebM with Vorbis sound. Alpha clips and MP4s keep WebKit's loop. [frame-rate.md](frame-rate.md#looping-clips-in-tyranoscript-rpg-maker-mv-and-mz) has the numbers and known limits.

## TyranoScript

TyranoScript games run on the same WebKit host. Their saves stay in the game folder as on PC, except for builds that save to browser storage, which VNDegen keeps per game. iOS cannot play Ogg, so VNDegen decodes Ogg Vorbis inside the game's page with libvorbis compiled to WebAssembly, the first time each track plays. Electron builds of TyranoScript 5.2 and later, which reach Node through the `window.studio_api` their preload script sets up, get the same modules from VNDegen.

The Skip and Auto buttons a game draws work at any moment, including while a line types or the game waits, and light up while they run. A second tap turns them off. Skip also passes unread text, unless a game's own setting screen has stored the player's choice. On PC, most of these games end Skip at the first unread line. This changes only the running engine, so saves stay compatible.

A looping video reloads at its end instead of using WebKit's own loop, whose seek to the start never finishes on an iPad simulator for a WebM with Vorbis sound. On a Mac the picture holds for about 315 ms at each loop. [frame-rate.md](frame-rate.md#looping-clips-in-tyranoscript-rpg-maker-mv-and-mz) has the numbers.

## Ren'Py

Ren'Py games run on Ren'Py's own engine. The Mac app bundles two official Ren'Py runtimes, 8.5.3 for Ren'Py 8 games and 7.8.7 for Ren'Py 7 and older games, and picks one from the game's `renpy/vc_version.py` or `renpy/__init__.py`, then its `lib/` folder names, then the engine's compiled files. Each game runs in its own process and window, which takes the hub window's place, and the hub comes back when the game quits. Saves and logs go to the game's data folder, and the game folder is never written to. A game copied from a PC with saves in `game/saves/` gets those copied in on its first launch.

On iPhone and iPad, Ren'Py 8.5.3 is built into the app and every Ren'Py game runs on it, inside VNDegen's window. Ren'Py 7 and 6.99 games run on it too, with fixes VNDegen applies in memory, such as dividing integers the Python 2 way in the game's own code. 15 of the 16 Ren'Py 7 games in the maintainer's library reached their main menu this way, and VNDegen ships that list. The page of a Ren'Py 7 game says whether it is known to work and, once it has run, whether it reached its main menu on this device. On Mac the game page's Ren'Py runtime option runs a game on the other runtime.

Ren'Py 8.5.3 runs Python 3.12, and games built on Ren'Py 8.3 or older were written for Python 3.9, so on every platform VNDegen also lets `random.randrange` and `random.randint` take whole-number floats again, as those versions did. `os.startfile`, which exists only on Windows, is there and does nothing, for games that name it in a screen, such as Countryside Cumsluts.

Ren'Py can't load saves made by a newer Ren'Py. VNDegen runs each game on the newest Ren'Py of its line, so saves made here may not load in the game's PC version, and the game's Saves page warns before a Ludusavi backup when that applies. VNDegen on other devices loads them. The exception is a Mac, which runs Ren'Py 7 games on Ren'Py 7.8.7, so it loads a Ren'Py 7 game's saves from an iPhone or iPad only once that game's Ren'Py runtime option is set to Ren'Py 8.5.3. A Ren'Py 7 game's persistent data, such as unlocked gallery images, is written so its PC version loads it.

[renpy-spike.md](renpy-spike.md) records how the runtimes were chosen and built.

## RPG Maker XP, VX, and VX Ace

RPG Maker XP, VX, and VX Ace games run on VNDegen's own RGSS runtime, which runs the game's Ruby scripts on MRI Ruby, an interpreter with no JIT, and draws each frame on the CPU for the app to show through Metal. The VNDegen menu and touch controls work as for the other engines. Packed games (`Game.rgss3a` and its older forms) and unpacked ones both work, as do MIDI music, through the General MIDI SoundFont in Settings, and the Windows API calls the test games make, which the runtime emulates. VX Ace movies play, and a game's message boxes stop it until you press OK, as on PC. Saves go to the game's data folder in the PC format, so saves copied in with a PC game still load and VNDegen's saves load on PC, and the game folder is never written to. Games that need a Windows DLL VNDegen doesn't emulate lose that DLL's feature, and a game whose scripts need something the runtime lacks stops with an error, which the game page then shows. [rgss.md](rgss.md) has the details and known limits.

### The RTP

Many RPG Maker games use the RTP, a set of stock graphics, sounds, and fonts that the PC player installs once instead of every game shipping it. Its license doesn't let VNDegen include it. When a game needs an RTP that isn't installed, its page offers Download and install, which fetches the official installer from the RPG Maker website (rpgmakerweb.com), checks it against the SHA-256 VNDegen expects, shows the RTP's license to accept, and unpacks it inside the app on Mac, iPhone, and iPad. The RTP is then shared by every game that needs it. Import from folder takes an RTP folder copied from a PC instead, such as `C:\Program Files (x86)\Common Files\Enterbrain\RGSS3\RPGVXAce`. The XP RTP is 23 MB to download, VX's 37 MB, and VX Ace's 195 MB. Nothing downloads until you tap the button.

XP, VX, and VX Ace games have the same Cheats page as MV and MZ, described in [controls.md](controls.md#cheats).

## KiriKiri

KiriKiri games run on krkrsdl3, an SDL3 port of the KiriKiri engine, which VNDegen builds into the app. The game draws inside VNDegen's window, composited on the CPU and shown through Metal, so the VNDegen menu and touch controls work as for the other engines. Saves stay in the game's `savedata/` folder as on PC. A game that saves to the Windows AppData or Documents folder saves to its data folder instead, and Ludusavi backups from Windows or a Steam Deck restore there.

Movies play, in the formats KiriKiri games ship: MPEG-1 and MPEG-2, WMV, and H.264 and HEVC, which decode in hardware. Encrypted archives work without any per-game setting: VNDegen recognizes the common ciphers by the checksums inside the archive, and tells you when a game uses one it doesn't know. Common Windows plugins are built in, including `extrans`, `layerExDraw`, `layerExSave`, and Steam builds' `krkrsteam`.

Taimanin Kurenai's movies are plain MPEG-1 files. Its archive also has a warning entry with an incorrect length and checksum. When the movies exceed the encryption check's byte budget, that incomplete warning no longer causes an unsupported-encryption report.

AlphaMovie (`.amv`) clips play, the battle animations of the Violated Hero games.

A KiriKiri game's window menu bar appears under Game commands in the VNDegen menu, described in [controls.md](controls.md#game-commands). In a game's own Display menu, the choice between fitting the picture inside the window, filling the window and cutting off the edges, and drawing it unscaled in the center works, as do smoothing, screen effects, the draw thread count, and the maximum frame rate, and the game's `.cfu` file keeps them for the next launch. The items that can't apply are left out: the window sizes, since VNDegen sizes the window, the display method and screen resolutions, since VNDegen draws through Metal and never changes the screen's mode, and the middle zoom, which games enable only for their own stand-in for full screen on Windows.

Known limits:

- Six games in the maintainer's library can't start, and say that their encryption isn't supported: OPPAI Ero App Academy, OPPAI Succubus Academy, Motto! Haramase! Honoo no Oppai Isekai Oppai Maid Gakuen!, the Bunny Gakuen and Spy Gakuen games of the same series, and Sabbat of the Witch. They install their cipher from an encrypted module inside the `.exe`. OPPAI Ero App Academy was checked by running it, and the others by reading their archives. OPPAI Academy Big, Bouncy, Booby Babes! is probably the seventh, inferred from its series, since its installer couldn't be read.
- Sound effects and music stored as MP3 don't play. No game that could be checked needs it: a scan of the library's 60 KiriKiri games in November 2026 found no `.mp3` in any archive and no script naming one, only bonus tracks outside the game data of three games. The scan couldn't search scripts in the six games above and OPPAI Academy, and five of those six also hide their file names.
- A KiriKiri game that is still starting with a question on screen when the Mac logs out or shuts down doesn't save its settings from that moment.
- A game three folders down, such as Violated Hero IV (Partial E) in its wrapper folders, is found only when the library's three-level listing of its folder has at most 200 entries. Such a listing with no game nearer the top has each folder at its last level listed once at every library refresh, as Wolf RPG Editor and RPG Maker detection do. Over the library's 728 titles this changed only that game's detection, and added about 3% to detection time on the Linux test host (3.43 s to 3.52 s). It hasn't been measured on an iPad.

[kirikiri-spike.md](kirikiri-spike.md) records how the port was chosen.

## Artemis

Artemis games run on art3m1s-core, an open-source Rust reimplementation of the Artemis engine, built into the app with Lua 5.1 and no JIT. The game draws inside VNDegen's window through the core's Metal renderer, so the VNDegen menu and touch controls work as for the other engines. Saves are the PC engine's own files, in both of the versions it writes, so a save made in VNDegen loads in the PC version at the same page and the other way round. They go to the game's data folder, under the folder name the PC version uses in AppData or beside the game, so Ludusavi backups from Windows restore there. Pages break where the PC version breaks them. Movies play, MPEG-1 and Ogg Theora, fullscreen and on layers, and HLSL screen effects compile to Metal. Known limits: in games that ship bitmap fonts (`.rft`) only ASCII comes from those fonts, with MS Gothic for the rest. [artemis.md](artemis.md) has the details.

## Godot

Godot games run on Godot itself, which VNDegen builds into the app twice: Godot 4.7 runs every Godot 4 game, and Godot 3.6 every Godot 3 game. A game's pack loads unchanged, from its `.pck` or from inside its `.exe`, and games exported with older Godot versions run too, such as Godot 4.2 to 4.5 and Godot 3.3 and 3.5. The game draws inside VNDegen's window: Godot 4 through Metal on Mac, iPhone, and iPad, and Godot 3 through Apple's OpenGL on Mac and through ANGLE's Metal backend on iPhone and iPad. Controllers reach games as an Xbox controller, and touches and the mouse as Godot's mouse.

Saves go where the PC version keeps them: `user://` is under the Windows AppData folder's stand-in in the game's data folder, so Ludusavi backups from Windows, a Steam Deck, or Godot's Linux and Mac builds restore there, and games that save beside their `.exe` save in their own folder. Games built with Steam through GodotSteam run as Steam does offline, with the player signed in, and games that also ship Epic Online Services for their Epic Games Store build run without it. Packs encrypted with a key built into the game's own copy of Godot run too: VNDegen finds the key in the game's `.exe`.

Games that can't run say why on their page: games that need other GDExtension or GDNative libraries, which are Windows code, and encrypted games whose key VNDegen doesn't find. Godot C# games aren't supported. [godot.md](godot.md) has the details and the known limits. The Cheats page in the VNDegen menu runs Godot games at 2x or 4x speed.

## Flash

Flash games run in Ruffle, a Flash Player written in Rust, built for the web and run on the same WebKit host with WebGL. VNDegen plays a game's Windows projector (`Game.exe` with the movie appended) without unpacking it, a `.swf` movie, or a collection's own HTML menu page. Movies a game loads, and its saves, are named as they are beside the PC executable. Saves are the `.sol` files Flash Player writes, kept in the game's data folder by the movie that made them, so saves from Flash Player or Ruffle on a PC restore through Ludusavi and load. ActionScript 3 support in Ruffle is less complete than for ActionScript 1 and 2, so some AS3 games may stop or misbehave. [flash.md](flash.md) has the details. The Cheats page in the VNDegen menu runs Flash games at 2x or 4x speed.

## NScripter

NScripter games run on VNDegen's own NScripter runtime, written in C++ for VNDegen from NScripter's documentation and the games' files, MIT licensed like the rest of the app. It draws on the CPU and shows its frames through Metal inside VNDegen's window. It reads the engine's archives, including bitmaps and entries its packer compressed, and plays Ogg, WAV, MP3 and NBZ sound, CD tracks from a game's `CD` folder, and MPEG-1 movies. Games that script part of their logic in Lua (NSLua) run on an embedded Lua 5.1. Saves are the PC engine's own files, byte for byte, in the game's save folder, so they move between VNDegen and a PC copy of the game, and Ludusavi backs them up. English translations whose patched engine wraps text between words get the same word wrap. Known limits: of the effect DLLs only the ones a test game uses are drawn, others crossfade, and NSLua's direct drawing functions aren't emulated. [nscripter.md](nscripter.md) has the details.

## Wolf RPG Editor

Wolf RPG Editor games run on VNDegen's own Wolf RPG Editor runtime, written in C++ for VNDegen from the games' files, permissively licensed format notes and the original engine's behaviour, MIT licensed like the rest of the app. It draws on the CPU and shows its frames through Metal inside VNDegen's window. It reads the games' data, maps and databases, loose or packed in `.wolf` archives, including Wolf RPG Editor 3.5's compressed data, runs their event scripts, and plays Ogg, MP3, WAV and MIDI sound, MIDI through a bundled General MIDI SoundFont or one the player chooses in Settings. Text uses a font the game ships, then VNDegen's Windows font pack. Saves are the PC engine's own files in the game's `Save` folder, so Ludusavi backs them up and they move between VNDegen and a PC copy of the game. Games take the mouse, with touch working as a mouse, and typed text. Known limits: Pro's encrypted data and DxLib archive version 7 aren't read, and no library game needs them. [wolfrpg.md](wolfrpg.md) has the details.

Wolf RPG Editor games have a Cheats page too, with what each game keeps where the cheats can find it, described in [cheats.md](cheats.md#built-wolf-rpg-editor).

## AliceSoft System 3

AliceSoft's System 3 games run on VNDegen's own System 3 runtime, written in C for VNDegen from AliceSoft's published System 3.5 and 3.9 SDKs, the games' files, and the original engines' observable behaviour, MIT licensed like the rest of the app. It runs both the System 3.5 to 3.9 games (`System39.exe`) and the older Windows 3.1 games (`SYSTEM3.EXE`), drawing on the CPU and showing its frames through Metal inside VNDegen's window. CD music plays from the MP3 files the game's `_inmm.ini` lists, MIDI music through the bundled SoundFont or the player's own, and the English releases of `SYSTEM3.EXE` games that get their text from `replacement.txt` show it. Saves are the PC engines' own files in the game's folder, byte for byte what `System39.exe` and `SYSTEM3.EXE` write, so they move between VNDegen and a PC copy of the game. Ludusavi's manifest doesn't list the test games yet, so Ludusavi doesn't back them up on its own. [system3.md](system3.md) has the details and the known limits. The Cheats page in the VNDegen menu edits the game's variables and runs it at 2x or 4x speed, described in [cheats.md](cheats.md#built-alicesoft-system-3-and-4).

## AliceSoft System 4

AliceSoft's System 4 games run on VNDegen's own System 4 runtime, written in C++ for VNDegen from AliceSoft's System 4.0 SDK, the games' files and the PC engine's observable behaviour, MIT licensed like the rest of the app. It runs the games' compiled code on its own virtual machine, draws on the CPU and shows its frames through Metal inside VNDegen's window. It reads the games' `.ald` and `.afa` archives, `.ex` data files and their QNT, AJP, PMS and WebP pictures, plays Ogg and WAV sound and the MPEG-1 movies, and draws text with the game's own `.fnl` font library where it has one, then VNDegen's Windows font pack. Saves are the PC engine's own files in the game's `SaveData` folder, checked against saves the PC games wrote: a save moves between VNDegen and a PC copy of the game in both directions, and Ludusavi backs them up. Known limits: Rance IX, built on PartsEngine, plays its story scenes but not its 3D battles and maps, where it stops with a message, and its saves move to and from the PC without the scene's pictures; its game page notes the 3D. [system4.md](system4.md) has the details and the known limits. The Cheats page in the VNDegen menu edits the game's globals, adds gold in the games whose globals name it, and runs the game at 2x or 4x speed, described in [cheats.md](cheats.md#built-alicesoft-system-3-and-4).

## NekoSDK

NekoSDK games run on VNDegen's own NekoSDK runtime, written in C++ for VNDegen from the games' files and the PC game's observable behaviour, MIT licensed like the rest of the app. No other runtime for these games exists. It runs their story scripts, flow scripts with choices and variables, draws on the CPU and shows its frames through Metal inside VNDegen's window, and plays Ogg voices, music and sound effects. Two games play so far, The Alchemist & His Battle-Scarred Homunculus and Yarinaoshi no Christmas. The other 14 NekoSDK games in the library are detected and say on their page that they can't run yet.

A NekoSDK game keeps its menus in its `.exe`, so VNDegen rebuilds them from a table for each game, drawn from the game's own art. Saves are the PC engine's own files, under `AppData/Roaming` in the game's data folder as on Windows, so they move between VNDegen and a PC copy of the game. Known limits:

- Unlocked pictures and music are read from and written to the PC's `cfg.sav`, so they move with a backup. Unlocked scenes don't move with a PC backup yet: the Scene gallery shows only those played in VNDegen (#46). The PC's Config settings don't: VNDegen keeps its own.
- Config changes text speed, auto speed, what the right button does, the confirmation questions, volumes, Individual Voices, and the music screen's volume, and in The Alchemist the window's opacity. Options that have no meaning here or aren't built, such as resolution, effects and read-only skip, are shown as disabled. Config's moving preview picture is still.
- A save's lock can't be set, and its comment and the "NEW" mark on the latest save aren't there. The bar's Arrange button isn't there, and neither is Graphics' choice of Original or Arrange art.
- The noise effect some scenes of The Alchemist ask for isn't drawn. A repeating animation saved while it runs plays out once after a load.
- Yarinaoshi no Christmas's Manual and Version info buttons aren't there.
- Menu backgrounds that change on the PC stay the same here. The help pictures that come up on hover on menu screens aren't drawn, and neither is the "Do not show again" box on a confirmation.

[nekosdk-cleanroom.md](nekosdk-cleanroom.md) has where the runtime's knowledge came from and the save format.

## Unity

VNDegen has no Unity engine of its own. Unity's terms don't let another app include its player, and a Windows game's code and shaders are built for Windows, so running them inside VNDegen would take a new engine, which [unity.md](unity.md) found isn't worth building now. On a Mac, Play opens the game outside VNDegen instead: its own Mac version when it ships one (Open Mac version), otherwise the Windows version through the player's own CrossOver, in one bottle VNDegen keeps apart from the player's bottles (Play in CrossOver). Saves sync from the game's `persistentDataPath`, and its PlayerPrefs with Ludusavi's registry data. [unity.md](unity.md#mac-launcher) has how it works and what was checked. On iPhone and iPad, Play stays unavailable and the page says why.

A game's page names the Unity version, read from the first bytes of its data file, including Unity China's versions such as `2021.3.21f1c1`, and whether its code ships as .NET assemblies (Mono, `Game_Data/Managed`) or compiled for Windows (IL2CPP, `GameAssembly.dll` or `Game_Data/il2cpp_data`). It also finds a Mac build the game ships, a `.app` whose `Contents/Resources/Data` holds Unity's data files, in any case, as SiNiSistar 2 does.

A Unity match is never more than likely and the Unity plugin is asked last, so any other engine that matches a folder keeps it. That matters for games such as an RPG Maker game in a wrapper folder, only a likely match, that ships a Unity tool beside it. In a scan of the library's 728 folders and archives on 2026-11-05, 121 Unity games went from no engine to Unity and no game changed from another engine. Her New Memory, a Ren'Py game that ships a Unity tool for Steam Workshop, stays Ren'Py.
