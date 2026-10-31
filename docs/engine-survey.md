# Engine survey

Game titles are the library folder names. The per-game list is [engine-survey-games.tsv](engine-survey-games.tsv).

Survey date 2026-10-25. This picks the next engines to port after RPG Maker MV/MZ and names test games from the library on the NAS.

Status since the survey: RPG Maker MV/MZ, TyranoScript, Ren'Py, RPG Maker XP, VX and VX Ace, KiriKiri, Flash, NScripter, Godot, Artemis, Wolf RPG Editor, and AliceSoft System 3 have shipped. NScripter runs on VNDegen's own clean-room runtime instead of ONScripter, and AliceSoft System 3 and 4 get clean-room runtimes instead of xsystem35 and xsystem4. `docs/roadmap.md` has the current state.

The per-game data is in [engine-survey-games.tsv](engine-survey-games.tsv), one row per library folder.

## Summary

The library has 521 folders and 684.5 GB (637.5 GiB). Signatures identify the engine for 446 folders. 75 stay unknown.

| Category | Folders | Size (GB) |
|---|---:|---:|
| `rpg-maker-family` | 160 | 123.7 |
| `vn-engine` | 173 | 310.4 |
| `general-engine` | 102 | 140.7 |
| `custom` | 11 | 33.5 |
| `unknown` | 75 | 76.1 |

Recommended port order after RPG Maker MV/MZ, covering only `vn-engine` and `rpg-maker-family`:

1. TyranoScript
2. Ren'Py
3. KiriKiri
4. RPG Maker XP, VX and VX Ace
5. NScripter
6. AliceSoft System 3.x and 4.x
7. Artemis
8. Wolf RPG Editor

## Recommended port order

Each engine is ranked on three inputs. The first is the number of library games. The second is current popularity, taken from VNDB releases since 2018. The third is whether an open-source runtime exists that already runs on Apple platforms. Feasibility weighs most. An engine without a runtime is a full reimplementation, so many local games do not lift it above an engine with a working iOS build.

| Rank | Engine | Library games (GB) | VNDB releases since 2018 | Runtime to build on | Apple status of that runtime |
|---:|---|---:|---:|---|---|
| 0 | RPG Maker MV/MZ | 89 (83.2) | 1,748 (all RPG Maker) | Own WebKit plugin | Shipped in VNDegen 0.1.0 |
| 1 | TyranoScript | 4 (4.5) | 5,447 | Game's own `tyrano/` JS in WKWebView | Runs in any browser |
| 2 | Ren'Py | 44 (45.9) | 26,235 | Ren'Py itself | Official iOS and macOS support |
| 3 | KiriKiri | 48 (124.0) | 2,857 | krkrsdl2 | Release ships iOS and macOS app bundles |
| 4 | RPG Maker XP, VX, VX Ace | 44 (24.3) | 1,748 (all RPG Maker) | mkxp-z | macOS only |
| 5 | NScripter | 6 (2.9) | 314 | ONScripter (Ogapee, ONScripter-jh, OnscripterYuri) | macOS builds. iOS makefile in Ogapee's tree |
| 6 | AliceSoft System 3.x, 4.x | 13 (5.3) | 57 (System4.X), 9 (System3.X) | xsystem35-sdl2, xsystem4 | No Apple builds published |
| 7 | Artemis | 8 (17.3) | 681 | art3m1s-core | Ships iOS and macOS builds, Metal renderer |
| 8 | Wolf RPG Editor | 23 (14.7) | 131 | None | None |

**1. TyranoScript.** Every TyranoScript game ships its whole engine as HTML and JavaScript in `tyrano/`. The MV/MZ plugin already hosts a web game in WKWebView with WebGL, so this is mostly detection, file serving, and save mapping. It is the second most common engine on VNDB since 2018. The library has 4 games, 2 of them wrapped in Electron. Tyrano's terms allow free and commercial use but forbid redistributing TyranoScript itself. VNDegen ships no copy of it, because each game brings its own. The same host would also cover other HTML games, such as Visual Novel Maker, Twine and plain NW.js or Electron titles.

**2. Ren'Py.** Ren'Py is the most popular engine by a wide margin (26,235 VNDB releases since 2018, 10,339 visual novels on itch.io). It has 44 library games. Ren'Py has official iOS support, but its iOS path builds one Xcode project per game and per Ren'Py version. VNDegen needs a generic host instead. That host is an embedded CPython with Ren'Py's native modules, running the `renpy/` package each game ships. The library splits into 28 games on Ren'Py 8 (Python 3), 3 on 7.4 or later (Python 2) and 13 on 7.3 or older (Python 2). Start with Ren'Py 8. Python 2 support is a second runtime build. Ren'Py is MIT, with LGPL parts (pygame_sdl2, FFmpeg, fribidi). It renders through OpenGL ES 2 via SDL2. Apple still ships OpenGL ES on iOS but has deprecated it, so plan for ANGLE over Metal.

**3. KiriKiri.** KiriKiri has the most library games after RPG Maker (48 games, 124 GB). It is fourth on VNDB since 2018. krkrsdl2 (MIT for its own code, with the Kirikiri Z core under a BSD-style license) publishes `krkrsdl2-ios-appbundle.zip` and `krkrsdl2-macos-appbundle.zip` in its latest release. Two risks need a test before committing. Many commercial games encrypt `data.xp3` with a per-game cipher. Many also ship native plugin DLLs (for example `krmovie.dll`, `extrans.dll`, `emotedriver.dll`) that a port must reimplement. Kirikiroid2 on Android handles many of these games, which suggests both are solvable game by game (inferred, not tested here). krkrsdl3 is the newer SDL3 line and has OpenGL and Vulkan backends.

**4. RPG Maker XP, VX and VX Ace.** There are 44 library games (39 of them VX Ace). mkxp-z (GPL-2.0) is mature on macOS. It renders through OpenGL ES on ANGLE, which its README says is there "to help mkxp switch to Metal". It has no iOS build. An iOS port means building MRI Ruby, SDL2 and ANGLE statically for iOS. MRI is an interpreter, so the no-JIT rule is not a blocker. Some games rely on the RPG Maker RTP and do not ship those assets, so the user must install the RTP. This stays in the RPG Maker family the user already targets.

**5. NScripter.** There are only 6 library games, but NScripter is fifth on VNDB all-time (2,284). ONScripter is GPL-2.0 SDL code that has been ported to many platforms. Ogapee's tree carries a `Makefile.iPhone` and `Makefile.MacOSX`. OnscripterYuri (an ONScripter-jh fork) builds for macOS on arm64 and x64 and adds an OpenGL ES 2 scaler. The effort is small.

**6. AliceSoft System 3.x and 4.x.** There are 13 games. 10 are System 3.x (`System39.exe` or `SYSTEM3.EXE`) and 3 are System 4 (`Rance 02`, `Sengoku Rance`, `Rance IX`). Twelve are installed on disk and range from 4 MB to 787 MB. Rance IX is a 2.6 GB GOG installer. xsystem35-sdl2 and xsystem4 are GPL-2.0, active in October 2026, and use SDL2. They publish Windows and Android (xsystem35) or Linux and Windows (xsystem4) builds. No Apple build exists, but plain SDL code is a modest port.

**7. Artemis.** There are 8 games. Artemis is sixth on VNDB since 2018 (681). art3m1s-core (MPL-2.0, Rust) uses native Metal on Apple and Luau for scripts on iOS. Its host app Art3m1s ships `ios-arm64.ipa` and `macos-arm64.dmg`. It is young. The repository was created in June 2026 and has 9 stars, so compatibility is unproven. Test it before ranking it higher.

**8. Wolf RPG Editor.** There are 23 games, the third largest group in the RPG Maker family. No open-source runtime exists. Community tools decrypt and document the formats (UberWolf, WolfDec, wolf-rpg-formats, WodiLib), but the interpreter itself would be a new reimplementation. The official editor is a Windows DxLib program. This is the largest job on the list, so it goes last despite the game count.

### Not recommended now

These have library games but no open-source runtime. Each would need a reimplementation from reverse engineering.

| Engine | Games | Note |
|---|---:|---|
| NekoSDK | 16 | 45 GB. Third largest VN group. No runtime or reimplementation found |
| YU-RIS | 15 | Only tools exist (yuri decompiler, YuRISTools, YPF tools) |
| Tmr-Hiro ADV System | 5 | None found |
| SystemAoi | 3 | None found |
| Smile Game Builder | 3 | .NET player (`SharpKmyCore.dll`). None found |
| Ikura GDL, IPAC, EAGLS, Bruns, FFD, ExHibit, Eushully AGE, N2System | 1 or 2 each | None found |
| System-NNN | 1 | The author published source at tinyan/SystemNNN with no license file. It is built on DirectX (`nyanDirectXLib`) |
| SRPG Studio | 1 | None found |
| RPG Maker MV/MZ packed in one exe | 3 | Works with the MV/MZ plugin once the user unpacks the Enigma Virtual Box exe on a PC |

Two popular engines have good runtimes but no library games, so nothing can be tested now. siglus_rs (MPL-2.0, Rust, wgpu) runs SiglusEngine, RealLive, AVG32 and UK2 and publishes `SiglusLauncher.ipa` and macOS app bundles. ethornell-rs (BGI/Ethornell, wgpu) publishes macOS and iOS builds but has no license file, so it cannot be reused without the author's permission.

### License note

mkxp-z, ONScripter, xsystem35-sdl2 and xsystem4 are GPL-2.0. EasyRPG Player is GPL-3.0. Linking them into VNDegen makes the combined app subject to the GPL. That is fine for personal builds and sideloading. It conflicts with App Store distribution terms. MIT, BSD-style and MPL-2.0 runtimes (Ren'Py core, krkrsdl2, art3m1s-core, siglus_rs, Ruffle, Godot) avoid this. VNDegen now uses only permissive licenses for new engines, and replaces the GPL runtimes with clean-room MIT ones. The Licenses section of `docs/roadmap.md` has the plan.

## Test picks

These prefer the smallest games and cover each variant that changes the code path. All but the AliceSoft games and Ogress on the Cliff are archives on the NAS and need extracting before use.

| Engine | Game folder | Size (MB) | Why |
|---|---|---:|---|
| RPG Maker MV | Mama's Secret Parent Teacher Conference | 145 | Smallest MV, encrypted assets |
| RPG Maker MV | Mirena's Manor | 188 | Unencrypted assets |
| RPG Maker MV | Ogress on the Cliff ~Next to Tomoe-san!~ | 405 | Installed, `rpg_core.js v1.5.0`, old NW.js (`ffmpegsumo.dll`) |
| RPG Maker MZ | Summer Sisters | 145 | Smallest MZ, encrypted assets |
| RPG Maker MZ | Lustful Dreamers | 237 | Unencrypted assets |
| TyranoScript | Butterfly Affection | 109 | Plain `tyrano/` folder |
| TyranoScript | Welcome to Paradise Island | 885 | Electron `resources/app/` layout |
| TyranoScript | 人外娘生態録 | 926 | Plain layout, Japanese text |
| Ren'Py | Servant's Revenge_ harem corruption | 146 | Ren'Py 8 (`lib/py3-*`) |
| Ren'Py | Give an imp a chance! | 250 | Ren'Py 7.3 or older (Python 2), ships Mac libs |
| Ren'Py | Hypno Mama | 734 | Ren'Py 7.4 or later (Python 2) |
| KiriKiri | Big-Breasted Married Teacher ~Shameful School Training~ | 134 | Smallest, packed `data.xp3` |
| KiriKiri | Haramimura_ When a Wife Becomes a Villager | 167 | Packed `data.xp3`, 8 plugin DLLs |
| KiriKiri | The Creamy Result of Hypnotizing my Annoying Stepmom and Honor Student Stepsister | 628 | Unpacked `data/startup.tjs` |
| RPG Maker VX Ace | Lust Doll | 28 | Unpacked `Data/*.rvdata2` |
| RPG Maker VX Ace | Catch Me in the Demon Lord's Castle | 39 | Packed `Game.rgss3a` |
| RPG Maker VX | Mecha Girl Adventure | 21 | Packed `Game.rgss2a` |
| RPG Maker XP | INMA KA SUGOROKU | 125 | Only XP game, `Game.rgssad` |
| NScripter | The Encyclopedia of Tentacles for Boys Vol.1 | 146 | Plain-text `00.txt` script |
| NScripter | Desire Dungeon | 191 | Plain-text `0.txt` script |
| NScripter | Lilipalace (English Ver.) | 195 | Encoded `nscript.dat` |
| AliceSoft System 3 | Rance_ Quest for Hikari | 4 | System 3.9, installed |
| AliceSoft System 3 | Rance 4.1_ Save the Medicine Plant! | 276 | Older `SYSTEM3.EXE` layout, installed |
| AliceSoft System 4 | Rance 02_ The Rebellious Maidens | 446 | Smallest System 4, installed |
| Artemis | Busty Mom Impregnation Netori | 245 | Single `root.pfs` |
| Artemis | Fantasy Tavern Sextet -Vol.1 New World Days- | 885 | Split `fts1.pfs.001` archive |
| Wolf RPG Editor | MilkyQuest | 26 | Smallest Wolf game |
| Wolf RPG Editor | The Land Where One Can Make Monmusu Pets - Elf Girl Mimia | 60 | Second smallest |
| Wolf RPG Editor | Yumei Club | 62 | Third smallest |

## Library counts by engine

Engines in `vn-engine` and `rpg-maker-family`, by game count.

| Engine | Category | Games | Size (GB) |
|---|---|---:|---:|
| RPG Maker MV | rpg-maker-family | 63 | 55.2 |
| KiriKiri | vn-engine | 48 | 124.0 |
| Ren'Py | vn-engine | 44 | 45.9 |
| RPG Maker VX Ace | rpg-maker-family | 39 | 22.8 |
| RPG Maker MZ | rpg-maker-family | 23 | 26.2 |
| Wolf RPG Editor | rpg-maker-family | 23 | 14.7 |
| NekoSDK | vn-engine | 16 | 45.1 |
| YU-RIS | vn-engine | 15 | 31.7 |
| AliceSoft System 3 | vn-engine | 10 | 1.4 |
| Artemis | vn-engine | 8 | 17.3 |
| NScripter | vn-engine | 6 | 2.9 |
| Tmr-Hiro ADV System | vn-engine | 5 | 10.7 |
| TyranoScript | vn-engine | 4 | 4.5 |
| RPG Maker VX | rpg-maker-family | 4 | 1.4 |
| RPG Maker MV/MZ (packed exe) | rpg-maker-family | 3 | 1.8 |
| AliceSoft System 4 | vn-engine | 3 | 3.9 |
| SystemAoi | vn-engine | 3 | 4.6 |
| Smile Game Builder | rpg-maker-family | 3 | 1.0 |
| Ikura GDL | vn-engine | 2 | 1.5 |
| IPAC | vn-engine | 2 | 1.0 |
| RPG Maker XP | rpg-maker-family | 1 | 0.1 |
| SRPG Studio | rpg-maker-family | 1 | 0.4 |
| EAGLS, Bruns, FFD, ExHibit, Eushully AGE, N2System, System-NNN | vn-engine | 1 each | 16.2 total |

Port-relevant details from the listings follow.
- RPG Maker MV/MZ ships NW.js in 85 of 86 games. 63 of 86 use encrypted assets (`.rpgmvp`, `.png_`, `.ogg_`). None ship Electron.
- Ren'Py games nearly all ship Linux libraries (39 of 44). 5 ship macOS libraries and 3 ship a macOS `.app`. These are desktop builds and do not help on iOS.
- 462 of 521 folders hold only archives (`.7z`, `.rar`, `.zip`). 59 hold loose files, 9 of them next to an archive. Some of those loose files are installers, not installed games. Versions are recorded only where a cheap header read was possible on installed files, or where the file layout implies them (Ren'Py `lib/` layout, AliceSoft `System39`).

## General-purpose and custom engines

These are outside VNDegen's target and are not in the port order.

| Engine | Games | Realistic? |
|---|---:|---|
| Unity | 60 | No. 44 are Mono and 10 are IL2CPP. The other 6 come from VNDB or public knowledge with no file check. Mono games need a reimplementation of the UnityEngine API, which nobody has built. IL2CPP games are compiled x86 code, which cannot run on iOS without an emulator. 6 use Naninovel, a VN framework on Unity, and the same limits apply |
| Flash | 14 | Yes. Ruffle (MIT or Apache-2.0) runs in WKWebView with WebGL. Its compatibility page lists 99% language and 82% API support for ActionScript 1 and 2, and 90% language and 82% API support for ActionScript 3. This is the most practical non-VN addition |
| Adobe AIR | 1 | Partly. The game is SWF content, so Ruffle may run it. AIR-only APIs will not work |
| Godot | 6 | Possible. Godot is MIT and runs on iOS and macOS with a native Metal renderer since 4.4. A Windows `.pck` loads in a matching-version Godot runtime only if it uses no native GDExtension libraries. Warding Witches uses GodotSteam, so it would not load |
| GameMaker | 6 | No. `data.win` needs YoYo's proprietary runner. No open runner exists |
| NW.js or Electron with custom JS | 7 | Checked on 2026-10-26, and none is RPG Maker. 2 are GameCreator, a Chinese HTML5 engine (`script.js` and `asset/assetList.json`). 1 is TyranoScript packed into its `.exe`, which the TyranoScript plugin would run once the app is unpacked from the exe. 1 is a Game Boy ROM in the binjgb emulator. 2 are custom Electron apps, one of which loads its content from a website. 1 is a custom HTML5 engine with Live2D (`data/ENGINE.js`). Each would run case by case in the WKWebView host, with shims for Node.js calls |
| Construct | 1 | Likely. Construct 3 exports HTML5 (`c3runtime.js`), so it runs in WKWebView |
| HSP | 1 | Unlikely. OpenHSP is BSD-3, but the Windows HSP runtime and its plugin DLLs do not map to its mobile variant |
| Pixel Game Maker MV | 1 | No. Proprietary player |
| GZDoom | 1 | Possible. GZDoom is open source and runs on macOS. iOS was not checked |
| Unreal | 2 | No. Native compiled code |
| id Tech | 2 | No. Doom Eternal is native code. The Quake folder holds data files, which existing open Quake ports can run |
| Custom native (including DxLib) | 11 | No. Compiled native code (DxLib, VB6, SDL with bespoke DLLs, Fox Engine, KEX, Heaps on HashLink, Falcom's engine, the DOS Wolfenstein 3D) |

## Unidentified folders

75 folders have no known engine.
- 13 are GOG offline installers (`setup_*_(nnnnn).exe`). Installer contents cannot be listed.
- 9 contain a second archive or an ISO inside the outer archive. Listing stopped at the outer layer.
- 2 are disc images (`.bin` and `.cue`, or `.iso` and `.mds`).
- 1 is a solid RAR4 archive that bsdtar could not list. A header walk found `DATA.tec` files from an unrecognized engine.
- 50 match no signature. These are mostly doujin games with proprietary formats (`.pak` with `.css` readmes, `.bsa` with `Canvas.dll`, `.gax` with `emotedriver.dll`, `Vox.dll` with `stg4_*` files).

Three archive problems sit inside these groups. `Summer Clover.7z` is truncated, because its header points past the end of the 35 MB file. The Princess Defender `.rar` has no archive signature in its first bytes. The three Monster Fighter Boy Ashita folders each hold a password-protected `おまけ*.zip` bonus archive.

## Popularity

### VNDB releases by engine

Counts come from the VNDB API (`POST https://api.vndb.org/kana/release` with `"count": true`), queried on 2026-10-25. Candidate engine names came from the VNDB engine list at https://vndb.org/r/engines. VNDB had 159,740 releases in total and 82,978 from 2018 onward. Caveats follow.
- VNDB tracks visual novels, so it undercounts RPG engines.
- "RPG Maker" is one engine entry, with no split by version.
- The engine field is free text, so spellings split some engines ("AliceSoft System4.X" and "AliceSoft System 3", "Unreal Engine 4" and "Unreal Engine 5").

All releases:

| Rank | Engine | Releases |
|---:|---|---:|
| 1 | Ren'Py | 29,930 |
| 2 | KiriKiri | 8,710 |
| 3 | TyranoScript | 5,945 |
| 4 | Unity | 5,507 |
| 5 | NScripter | 2,284 |
| 6 | RPG Maker | 2,083 |
| 7 | LiveMaker | 1,788 |
| 8 | YU-RIS | 1,003 |
| 9 | Artemis Engine | 829 |
| 10 | Flash Player | 807 |
| 11 | Godot | 599 |
| 12 | BGI/Ethornell | 438 |
| 13 | System-NNN | 411 |
| 14 | NeXAS | 372 |
| 15 | Shiina Rio | 351 |
| 16 | Macromedia Director | 327 |
| 17 | CatSystem2 | 321 |
| 18 | Majiro | 294 |
| 19 | Cocos2d | 281 |
| 20 | SiglusEngine | 265 |
| 21 | RealLive | 249 |
| 22 | Wolf RPG Editor | 239 |
| 23 | Dorian Engine | 234 |
| 24 | Visual Novel Maker | 234 |
| 25 | Malie | 233 |

Releases dated 2018-01-01 or later. The last column keeps only official releases.

| Rank | Engine | Releases | Official |
|---:|---|---:|---:|
| 1 | Ren'Py | 26,235 | 22,645 |
| 2 | TyranoScript | 5,447 | 5,133 |
| 3 | Unity | 5,118 | 4,880 |
| 4 | KiriKiri | 2,857 | 2,195 |
| 5 | RPG Maker | 1,748 | 1,486 |
| 6 | Artemis Engine | 681 | 557 |
| 7 | Godot | 599 | 594 |
| 8 | YU-RIS | 508 | 456 |
| 9 | LiveMaker | 379 | 339 |
| 10 | NScripter | 314 | 216 |
| 11 | NeXAS | 299 | 282 |
| 12 | Cocos2d | 250 | 250 |
| 13 | Dorian Engine | 234 | 234 |
| 14 | Visual Novel Maker | 232 | 231 |
| 15 | BGI/Ethornell | 223 | 189 |
| 16 | GameMaker | 192 | 182 |
| 17 | Light.vn | 185 | 172 |
| 18 | SiglusEngine | 154 | 123 |
| 19 | CatSystem2 | 132 | 106 |
| 20 | Wolf RPG Editor | 131 | 121 |
| 21 | Luca System | 129 | 106 |
| 22 | MAGES. Engine | 116 | 108 |
| 23 | Flash Player | 111 | 76 |
| 24 | ADV Player HD | 96 | 93 |
| 25 | Twine | 92 | 92 |

### itch.io

itch.io counts projects by the engine their authors tag. These counts cover all genres, so they show which engines indie and doujin developers use. Source: https://itch.io/game-development/engines/most-projects, read 2026-10-25.

| Rank | Engine | Projects |
|---:|---|---:|
| 1 | Unity | 391k |
| 2 | Godot | 170k |
| 3 | Construct | 60.3k |
| 4 | GameMaker | 32k |
| 5 | Twine | 27.1k |
| 6 | GDevelop | 23.1k |
| 7 | Ren'Py | 15.4k |
| 8 | Unreal Engine | 15.2k |
| 9 | RPG Maker | 14.3k |
| 17 | TyranoBuilder | 1,425 |
| 28 | EasyRPG | 352 |

The same engines filtered by genre, from the result counts on `https://itch.io/games/genre-visual-novel/made-with-<engine>` and `https://itch.io/games/genre-rpg/made-with-<engine>`:

| Engine | Visual novel genre | RPG genre |
|---|---:|---:|
| Ren'Py | 10,339 | 345 |
| Unity | 5,815 | 8,222 |
| Godot | 3,045 | 4,400 |
| Twine | 2,259 | 986 |
| RPG Maker | 981 | 5,522 |
| TyranoBuilder | 888 | 29 |
| Construct | 394 | 789 |
| GameMaker | 385 | 1,031 |
| Unreal Engine | 171 | 465 |

itch.io does not list Wolf RPG Editor, KiriKiri or NScripter as engines. SteamDB was not used because its terms forbid scraping.

## Port feasibility

This covers every engine in the library and every engine in either VNDB top 25. Status was read from each project's repository, README and latest release assets on 2026-10-25.

| Engine | Best runtime to build on | License | macOS | iOS | Rendering | Notes |
|---|---|---|---|---|---|---|
| RPG Maker MV/MZ | Game's own JS (PixiJS) in WKWebView | Game-supplied | Yes | Yes | WebGL | Shipped in VNDegen 0.1.0 |
| TyranoScript | Game's own `tyrano/` JS in WKWebView | Tyrano terms (free use, no redistribution of Tyrano itself) | Yes | Yes | HTML/WebGL | Same host as MV/MZ |
| Visual Novel Maker, Twine, Dorian | Game's own HTML in WKWebView | Game-supplied | Yes | Yes | HTML/WebGL | Not in library |
| Ren'Py | Ren'Py ([renpy/renpy](https://github.com/renpy/renpy)) | MIT plus LGPL parts | Yes | Yes (per-game Xcode project) | OpenGL ES 2 via SDL2 | Needs a generic host and a Python 2 build for older games |
| KiriKiri 2/Z | [krkrsdl2](https://github.com/krkrsdl2/krkrsdl2), also krkrsdl3 and Kirikiroid2 (Android) | MIT on Kirikiri Z's BSD-style license | Yes | Yes (release app bundle) | SDL2. krkrsdl3 has OpenGL/GLES and Vulkan | Per-game xp3 encryption and plugin DLLs |
| RPG Maker XP/VX/VX Ace | [mkxp-z](https://github.com/mkxp-z/mkxp-z) | GPL-2.0 | Yes | No | OpenGL ES through ANGLE (Metal) | Needs MRI Ruby, SDL2 and ANGLE built for iOS |
| RPG Maker 2000/2003 | [EasyRPG Player](https://github.com/EasyRPG/Player) | GPL-3.0 | Yes (`builds/macos`) | Third-party App Store app (iEasyRPG) and libretro core | Software blitting shown through SDL2/SDL3 | Not in library. EasyRPG has 352 itch.io projects |
| NScripter | [ONScripter](https://github.com/ogapee/onscripter), [ONScripter-jh](https://github.com/jh10001/ONScripter-Jh), [OnscripterYuri](https://github.com/YuriSizuku/OnscripterYuri) | GPL-2.0 | Yes | Old `Makefile.iPhone` in Ogapee's tree | SDL software surfaces. Yuri adds a GLES2 scaler | Mature and small |
| AliceSoft System 3.x | [xsystem35-sdl2](https://github.com/kichikuou/xsystem35-sdl2) | GPL-2.0 | Not published | Not published | SDL2 or SDL3 | Android build published |
| AliceSoft System 4.x | [xsystem4](https://github.com/nunuhara/xsystem4) | GPL-2.0 | Not published | Not published | SDL2, ships GLSL shaders | Android port at kichikuou/xsystem4-android |
| Artemis | [art3m1s-core](https://github.com/Alphaly2K/art3m1s-core) with host [Art3m1s](https://github.com/Alphaly2K/art3m1s) | MPL-2.0 | Yes | Yes (`ios-arm64.ipa`) | Native Metal, GL/ANGLE fallback | Created June 2026. Also artemis-compat (GPL-3.0, Android only) |
| SiglusEngine, RealLive | [siglus_rs](https://github.com/xmoezzz/siglus_rs) | MPL-2.0 | Yes | Yes (`SiglusLauncher.ipa`) | wgpu (Metal) | Not in library. rlvm (GPL-3.0) is an older RealLive option |
| BGI/Ethornell | [ethornell-rs](https://github.com/xmoezzz/ethornell-rs) | No license file | Yes | Yes (unsigned IPA) | wgpu (Metal), WebGL2 on web | Not in library. Reuse needs the author's permission |
| Flash | [Ruffle](https://github.com/ruffle-rs/ruffle) | MIT or Apache-2.0 | Yes | Web build in WKWebView | wgpu desktop, WebGL or WebGPU on web | `general-engine` |
| Macromedia Director | ScummVM Director engine ([scummvm/scummvm](https://github.com/scummvm/scummvm)) | GPL-3.0 | Yes | Yes (ScummVM) | ScummVM backends | Not in library. Supports a list of titles, not Director in general |
| Godot | Godot ([godotengine/godot](https://github.com/godotengine/godot)) | MIT | Yes | Yes | Metal (4.4+ on Apple Silicon), Vulkan, GLES3 | `general-engine`. Needs a version-matched runtime |
| LiveMaker | None. pylivemaker (GPL-3.0) only extracts | | | | | Not in library |
| YU-RIS | None. Only tools | | | | | 15 library games |
| NekoSDK | None found | | | | | 16 library games |
| Wolf RPG Editor | None. Only format tools | | | | | 23 library games |
| System-NNN | Author's source at tinyan/SystemNNN, no license file, DirectX | | | | | 1 library game |
| NeXAS | None. Only tools | | | | | Not in library |
| Shiina Rio, Malie, MAGES. Engine, ADV Player HD, Luca System | None found | | | | | Not in library |
| CatSystem2 | None usable. FelineSystem2 is an inactive remake (last push 2023) | | | | | Not in library |
| Majiro | ReMajiro (AGPL-3.0, Rust), maturity unknown | | | | | Not in library |
| Light.vn | None. Only unpack tools | | | | | Not in library |
| Cocos2d | None. Games are native compiled code | | | | | Not in library |
| Unity (including Naninovel) | None | | | | | Mono needs a UnityEngine reimplementation. IL2CPP is native code |
| GameMaker | None for `data.win` games | | | | | Proprietary runner |
| Smile Game Builder, SRPG Studio, Tmr-Hiro, SystemAoi, Ikura GDL, IPAC, EAGLS, Bruns, FFD, ExHibit, Eushully AGE, N2System | None found | | | | | 1 to 3 library games each |

## Method

the NAS was read only, under `nice -n 19 ionice -c3`.
1. One `find -maxdepth 6` listed every folder.
2. One `du -sb` per folder gave sizes.
3. `bsdtar -tvf` listed `.7z` and `.rar` archives and Python `zipfile` listed `.zip` archives, both without extracting. 485 of 488 archives listed cleanly.
4. A Python RAR4 header walk listed the one solid RAR4 archive that bsdtar refused.
5. Magic bytes from the first 32 bytes explained the two other failures.
6. One installed game had its `rpg_core.js` header read for a version.

A classifier matched file paths against ordered signature rules (for example `js/rmmz_core.js`, `*.rgss3a`, `renpy/`, `*.xp3` or `*.tjs`, `tyrano/`, `*.wolf` or `Data/BasicData/`, `*.ypf`, `*.pfs`, `System39.ain`). Where no rule matched, the VNDB release search filled the engine for 31 titles. Each was kept only when the release title matched the folder. Rows that rely on VNDB, on a series pattern, or on public knowledge say so in the TSV notes column.

Known limits follow.
- Two inferred groups depend on file-layout patterns. NekoSDK is inferred from `.pak` plus a save-folder launcher, and Tmr-Hiro from `.pac` plus `uninst.exe`. VNDB confirms 7 NekoSDK titles and 3 Tmr-Hiro titles. The other members of each group are inferred from the pattern alone.
- Version data is thin because nearly every game is archived.
- Unity Mono or IL2CPP comes from `Managed/` versus `GameAssembly.dll` or `il2cpp_data/`.

The scripts are in the checkout's ignored `.audit/scripts/` folder, with raw listings in `.audit/evidence/`.
