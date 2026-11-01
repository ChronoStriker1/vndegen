# Development

How to build VNDegen, how the source is laid out, and how to add an engine. [engines.md](engines.md) describes how each engine runs today, and [roadmap.md](roadmap.md) the agreed plan.

## Building

You need a Mac with Xcode 27, [XcodeGen](https://github.com/yonaskolb/XcodeGen) (`brew install xcodegen`), and Go (`brew install go`) for the rclone library that save sync uses. Run `scripts/build.sh` on that Mac:

```sh
scripts/build.sh test      # package tests
scripts/build.sh mac       # macOS app in build/mac/Build/Products/Debug/
scripts/build.sh ios-sim   # iOS simulator app in build/sim/Build/Products/Debug-iphonesimulator/
scripts/build.sh ipa       # unsigned build/VNDegen.ipa
```

`scripts/build.sh test` also runs the file name lookups' unit tests on a case-sensitive disk image.

### What a build makes first

- **KiriKiri.** Every app build first builds the KiriKiri engine into `build/kirikiri` with `scripts/kirikiri-engine.sh`, which does nothing when the engine is up to date. Its dependencies come from vcpkg, cached in `~/Library/Caches/VNDegen`, so the first build takes a while and later ones skip it.
- **Godot.** Every app build first builds Godot 3 and 4 into `build/godot` with `scripts/godot-engine.sh`, which does nothing when its inputs haven't changed. The Godot sources and SCons are cached in `~/Library/Caches/VNDegen/godot`, and each build folder gets its own work folder there, so builds of different branches don't rebuild each other's objects. A first build in a new folder compiles Godot from scratch, which can take a Mac's load past 500, so run it where nothing else is going. `VNDEGEN_GODOT_CACHE` moves the cache.
- **Alpha clips.** Every build makes the VP9 decoder the RPG Maker clip converter links, with `scripts/alpha-video.sh`, from the libvpx source in `Engines/Shared/third_party`. It builds the decoder alone.
- **rclone.** Every build first makes rclone's library from `Packages/VNDegenKit/Rclone/` for iPhone, iPad, the simulator, and the Mac (`scripts/rclone-runtime.sh`, kept until its sources or the Go version change).
- **Ren'Py.** The macOS build downloads the two Ren'Py SDKs once, checks their SHA-256, and copies the trimmed runtimes into the app (`scripts/renpy-runtime.sh`, cached in `~/Library/Caches/VNDegen/renpy-sdk`). iOS builds also download Ren'Py's iOS libraries (renios) and link them into the app. `scripts/build.sh` does that before it builds. In Xcode, run `scripts/renpy-runtime.sh build/renpy ios` once first.
- **Ruby.** Every build also makes the Ruby interpreter the RPG Maker XP, VX, and VX Ace runtime links, `Packages/VNDegenKit/build/RGSSRuby.xcframework`, with `scripts/rgss-ruby.sh`, which does nothing when it is up to date. It downloads Ruby's release, checks its SHA-256, applies the patches in `Engines/RGSSRuntime/patches`, and builds it static for the Mac, iPhone and iPad, and the simulator. The first build takes about 15 minutes. `Engines/RGSSRuntime/host` builds the runtime for Linux with a headless test tool, after `scripts/rgss-ruby.sh build/rgss linux`.

### License notices

Each bundled runtime writes the license notices of the code it links into `build/<runtime>/licenses/`: `scripts/rclone-runtime.sh` for rclone and its Go modules, `scripts/renpy-runtime.sh` for Ren'Py and its libraries, and `scripts/app-licenses.sh` for code built into the app itself. `scripts/build.sh` stops an app build when one of them wrote none. Where the texts come from is in `scripts/licenses/README.md`.

### Building from another machine

`scripts/remote.sh` copies the checkout to a Mac over SSH and runs a command there. For example, `VNDEGEN_MAC=my-mac.local scripts/remote.sh scripts/build.sh ipa`. `scripts/sim.sh` boots a simulator, installs the app, copies a game in, and launches it.

### Testing helpers

- `scripts/renpy-compat.sh <folder>` boots every Ren'Py game in a folder and prints which runtime each got and whether it reached its main menu.
- `scripts/renpy-hook-test.sh` checks VNDegen's Ren'Py hook on both runtimes.
- `scripts/case-check.sh` sets up the file name case check described [below](#file-name-case).

A DEBUG build takes `-libraryRoot <folder>`, which points the app at a separate library so test runs never touch `~/Games/VNDegen`, and `-autolaunch "<game folder name>"`, which starts a game directly.

### Installing your own build

`scripts/build.sh ipa` makes an unsigned IPA. Sideloadly or AltStore signs and installs it with a free Apple ID. Apps signed that way expire after 7 days and need to be installed again.

### Releases

`scripts/release.sh <version>` publishes a new release from `main` to the [Releases](https://github.com/ChronoStriker1/vndegen/releases) page and adds it to the AltStore source. `scripts/check-public.sh` must pass first, and `release.sh` runs it. Each release's notes are in `releases/`. Packaged builds (`ipa` and `release`) delete the previous built app first, because Xcode keeps files in a built app that the project no longer makes. Compiled objects stay cached.

## Project layout

```
App/                      app entry point, icons, Info.plist
Packages/VNDegenKit/
  Sources/VNCore/         shared types: games, the library, the engine plugin contract, input events
  Sources/WebRuntime/     shared host for HTML5 engines: WebKit view, game file serving, Node and NW.js layer
  Sources/FrameHost/      shows CPU-drawn engine frames through Metal and plays their PCM audio
  Sources/EngineRPGMaker/ RPG Maker MV and MZ: detection, key mapping, save locations
  Sources/EngineTyrano/   TyranoScript: detection, Electron app.asar reading, save handling, controller mapping
  Sources/EngineRenPy/    Ren'Py: detection, runtime choice, the macOS helper process, the iOS in-app session, and the in-game hook
  Sources/CRenPy/         starts Python and runs Ren'Py inside the app on iOS
  Sources/RenPyCompat/    command-line boot test for a folder of Ren'Py games
  Sources/EngineKiriKiri/ KiriKiri: detection, save locations, and the in-process session on the engine framework
  Sources/CKiriKiri/      vnkk.h, the KiriKiri engine's C interface
  Sources/EngineRGSS/     RPG Maker XP, VX, and VX Ace: detection, the RTPs they need, saves, and the in-process session
  Sources/CRGSS/          VNDegen's RGSS runtime, in C, with vnrgss.h as its interface
  Sources/EngineArtemis/  Artemis: detection, save locations, input, audio, and the in-process session on the engine library
  Sources/CArtemis/       art3m1s_api.h, the Artemis engine's C interface, and the Ogg Vorbis decoder for its audio
  Sources/EngineGodot/    Godot 3 and 4: detection from the pack, save locations, and the in-process session
  Sources/CGodot/         vngd.h, the Godot engines' C interface
  Sources/EngineFlash/    Flash: detection, the projector and movie loading, fonts, and saves for Ruffle
  Sources/EngineNScripter/ NScripter: detection, input, Game commands, saves, and the in-process session
  Sources/CNScripter/     VNDegen's NScripter runtime in C++
  Sources/EngineWolfRPG/  Wolf RPG Editor: detection, input, saves, and the in-process session
  Sources/CWolfRPG/       VNDegen's Wolf RPG Editor runtime in C++
  Sources/EngineAliceSoft/ AliceSoft System 3 and 4: detection, save locations, input, and both sessions
  Sources/CSystem3/       VNDegen's System 3 runtime in C, with vns3.h, its C interface
  Sources/CSystem4/       VNDegen's AliceSoft System 4 runtime in C++, with vns4.h, its C interface
  Sources/RuntimePackages/ downloading, checking, and unpacking runtime packages such as the RTPs, with an Inno Setup reader
  Sources/FontPack/       the Windows font substitutes: the Swift API, and the Chinese and Korean font downloads
  Sources/CVNFont/        links to Engines/Shared/vnfont.[ch], the table reader every engine shares
  Sources/CVNMidi/        links to Engines/Shared/vnmidi.[ch], the MIDI player native engines share
  Sources/CVNVideo/       links to Engines/Shared/vnvideo.[ch], the movie decoder native engines share, and its libraries
  Sources/CVNAlpha/       links to Engines/Shared/vnalpha.[h,mm], which converts VP9 clips with alpha for RPG Maker MV and MZ
  Sources/HubUI/          the library, game pages, settings, and the player screen
  Sources/GameInput/      game controllers, keyboard, and the touch overlay
  Sources/ControllerUI/   controller focus ring, the on-screen keyboard, and controller scrolling
  Sources/Metadata/       Steam, SteamGridDB, IGDB, and VNDB
  Sources/Lock/           Face ID, Touch ID, and passcode
  Sources/SaveSync/       Ludusavi backup and restore
  Sources/Rclone/         Swift calls into rclone's library
  Rclone/                 VNDegen's build of rclone: the Go library with the SMB and local backends
  Sources/ArchiveImport/  unpacking zip, RAR, and 7z archives into the library
Engines/RGSSRuntime/      the RGSS runtime's Ruby preludes, Ruby patches, Linux test host, and tools
Engines/Artemis/          the vendored art3m1s-core engine and its save format tools; UPSTREAM.md lists every change
Engines/KiriKiri/         the vendored krkrsdl3 engine and VNDegen's C interface to it; UPSTREAM.md lists every change
Engines/Godot/            VNDegen's Godot modules, patches, and test host; UPSTREAM.md lists the pinned sources
Engines/Flash/            Ruffle's web player, built by scripts/ruffle-web.sh, with its patches and licenses
Engines/NScripter/        the NScripter runtime's Linux test host and tools
Engines/WolfRPG/          the Wolf RPG Editor runtime's Linux test host
Engines/System3/          the System 3 runtime's Linux test host: a headless runner and an SDL player
Engines/System4/          the System 4 runtime's Linux test host and tools; UPSTREAM.md lists the vendored libraries
Engines/Shared/           code and data every engine shares: the font table reader and the bundled fonts in fonts/,
                          the MIDI player and SoundFonts, the movie decoder, and the case-insensitive path lookup
docs/                     user and engine docs, the engine survey, release notes, and screenshots
scripts/                  build, remote build, simulator, release, license, and runtime helpers
project.yml               XcodeGen project definition
```

## Adding an engine

An engine is a type conforming to `EnginePlugin` in `VNCore/Engine.swift`:

- `detect(_:)` inspects a folder listing and returns a match with a confidence level.
- `makeSession(_:)` starts the game and returns an `EngineSession`, whose view the hub shows full screen and which receives controller input.
- `saveLocations(folder:dataDirectory:)` tells Ludusavi sync where the engine keeps saves.
- `engineFileList(_:)`, with `listsEngineFiles`, names the PC engine files a game can do without, for Remove engine files. It's an allowlist, and optional. List a file only after checking that nothing in VNDegen reads it, including detection, and only from what real games ship. It must not read the files it names, which may not be downloaded. `VNCore/EngineFiles.swift` does the rest, and decides there which kinds of folder are refused.

Add the new plugin to the `engines` list in `App/VNDegenApp.swift`. The detector picks the most confident match among all registered plugins. Then add its entry to `PluginInfo.catalog` in `HubUI/Plugins.swift`, which Settings' Plugins page reads for the engine versions it covers, what runs its games, and its license.

An engine that looks fonts up by Windows name resolves them with `vnfont_resolve` from `Engines/Shared/vnfont.h`. It compiles `vnfont.c` into the engine and passes `vnfont_init` the table and folders that `FontPack.table` and `FontPack.folders` give. See [fonts.md](fonts.md).

An engine that plays MIDI music uses vnmidi from `Engines/Shared/vnmidi.h`. It compiles `vnmidi.c`, or depends on the `CVNMidi` package target, and loads the General MIDI SoundFont from `MIDISoundFont.url`, the player's from Settings or the bundled one, with `MIDISoundFont.fallbackURL` through `vnmidi_font_load_fallback`. See [midi.md](midi.md).

An engine that plays movies uses vnvideo from `Engines/Shared/vnvideo.h`, through the `CVNVideo` package target, for MPEG-1 and for Ogg Theora with Opus sound. See [video.md](video.md).

### File name case

Windows ignores case in file and folder names, so Windows games often ask for `Img/Title.PNG` when the disk has `img/title.png`. iPhone and iPad disks, and case-sensitive Mac volumes, do not ignore case. Every engine plugin must find a game's files, folders, and archive members whatever case the game uses, for reads, writes, existence checks, and folder listings. A write to a name that exists in another case must replace that file rather than create a second one. Use the lookup of the engine's family rather than a new one:

- Swift code, including the web engines' `GameFolder`, uses `FileCase` in `VNCore/FileCase.swift`.
- C and C++ engines call `casepath_resolve` in `Engines/Shared/casepath.c` from their file layer, as the RGSS runtime's `fs.c` and KiriKiri's `unix_file.cpp` do.
- Ren'Py's Python side uses `EngineRenPy/Support/filecase.py`, which `launcher.py` installs. `scripts/renpy-case-test.sh` tests it.

Each looks a name up as written first and reads a folder's listing only when that misses, so games that use the right case pay nothing.

Before a new engine plugin or a change to one ships, it passes the case check on its test games. `scripts/case-check.sh setup` copies each game into a library on a case-sensitive disk image with every name's case swapped. Each game must reach its title, start, save, and load there, on the Mac app and on an iPad simulator, both started with `-libraryRoot` on that library. The script's header has the commands.

### Frame rate

Games that aren't strictly visual novels, such as RPG Maker games and others with real-time movement, battles, or animation, and visual novels with video, such as movies, animated clips, or Live2D, must reach the frame rate they target and hold it. Before a new engine plugin or a change to one ships, show for each such test game that it reaches its target (read it from the game or engine: 60 fps for RPG Maker MV and MZ, the game's `Graphics.frame_rate` in RGSS, and so on) and holds it on average through normal play, including the heaviest scene you can reach. In a visual novel, measure its video scenes. Scenes of text, still images, and transitions need no measurement.

Report the target, the average, the 1% low, and how long you measured, on the Mac app and an iPad simulator, with what else was running on the machine. If a game falls short, find out whether VNDegen or the plugin causes it before blaming the machine: profile the app (Instruments' Time Profiler or Metal System Trace, or WebKit's timeline for web engines), compare runs under lighter load, and compare with the game on its own engine where you can. Simulator timing is not device timing, so say which one a number comes from.
