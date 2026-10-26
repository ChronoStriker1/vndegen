<img src="App/Icon/icon-mac.svg" width="96" height="96" alt="VNDegen icon">

# VNDegen

VNDegen runs visual novel and RPG Maker games natively on Mac, iPhone, and iPad. Copy a game's folder into the library and VNDegen detects its engine and lists it in a game hub modeled on Steam's Big Picture mode. Each game shows its artwork, and Play starts it.

The PC games run unmodified. VNDegen does not convert or repack them, and their saves stay compatible with the PC versions.

<p align="center">
  <img src="docs/screenshots/hub-ipad.jpg" width="820" alt="Library home on iPad, with the focused game's hero art, logo, and Play button above a shelf of recently played games">
</p>

<p align="center">
  <img src="docs/screenshots/hub-iphone.jpg" width="260" alt="Library home on iPhone">
  <img src="docs/screenshots/game-page-mac.jpg" width="560" alt="Game page on Mac, with the hero banner, Play, Artwork, Find metadata, and Hide buttons, and a details card">
</p>

## Features

- **Engine detection.** Each engine is a plugin that recognizes its own files, such as `js/rmmz_core.js` for RPG Maker MZ. The game page shows the detected engine and the file that identified it. Games with no detected engine can be assigned one by hand.
- **Artwork and metadata.** VNDegen fetches capsule, hero, logo, header, and icon art plus descriptions, developers, release dates, and genres from the Steam store, SteamGridDB, IGDB, and VNDB. Every artwork slot can be replaced with another source's image or one imported from Photos or Files.
- **One layout per device.** The hub adapts to iPhone portrait and landscape, both iPhone Duo displays, iPad, and Mac windows of any size. Folding the Duo or rotating the iPad keeps your place.
- **Every input method.** Touch, mouse and keyboard, and game controllers all drive both the hub and the games. Each game has its own control method (Automatic, Touch gamepad, Mouse, or Controller), switchable from the in-game menu. Mouse works by touch, where a tap clicks right where it lands, or by a cursor you drag like a trackpad. Controller shows no overlay and ignores touches unless its Mouse checkbox is on. Each game also has its own button mapping: any overlay or controller button can send a gamepad action, a keyboard key, or a mouse click.
- **Hidden games.** Games marked Hide disappear from the library until it is unlocked with Face ID, Touch ID, or an app passcode. The library locks again when the app goes to the background.
- **In-game orientation.** On iPhone, games lock to landscape by default. On iPad the lock applies only when VNDegen is full screen, not in split screen or a window. The setting can be changed globally or per game: Landscape, Portrait, or Follow device.
- **Ludusavi saves.** VNDegen backs up and restores [Ludusavi](https://github.com/mtkennerly/ludusavi) backups on your SMB share, the one Ludusavi's cloud sync uses, so saves move between VNDegen and the PC copies Ludusavi manages. It matches games by itself, offers newer saves before a game starts, and restores can be undone.
- **Hardware rendering.** Web-based engines render through WebGL on Metal. Ren'Py renders through Apple's OpenGL on Mac and through ANGLE's Metal backend on iPhone and iPad, both on Metal.

<p align="center">
  <img src="docs/screenshots/no-socks-rpg-iphone.jpg" width="820" alt="No Socks RPG, an RPG Maker MZ game, running in landscape on iPhone with its in-game menu open, loaded from a Steam save">
</p>

## Engines

| Engine | Detected by | Platforms |
|---|---|---|
| RPG Maker MZ | `js/rmmz_core.js` | Mac, iPhone, iPad |
| RPG Maker MV | `js/rpg_core.js` or `www/js/rpg_core.js` | Mac, iPhone, iPad |
| TyranoScript and TyranoBuilder | `tyrano/` with `data/scenario/`, including NW.js and Electron builds (also inside `app.asar`) | Mac, iPhone, iPad |
| Ren'Py 6.99 to 8 | `renpy/` beside a `game/` folder holding `.rpy`, `.rpyc`, or `.rpa` files, also one or two folders down | Mac, iPhone, iPad |

RPG Maker MV and MZ games ship as HTML5 inside NW.js. VNDegen runs them in WebKit with a small NW.js compatibility layer, so plugins that call Node's `fs`, `path`, or `nw.gui` keep working. The layer also means saves are written as `save/*.rmmzsave` exactly like the PC version, and encrypted images and audio, Effekseer effects, and WebM movies all work.

TyranoScript games run on the same WebKit host. Their saves stay in the game folder as on PC, except for builds that save to browser storage, which VNDegen keeps per game. iOS cannot play Ogg, so VNDegen decodes Ogg Vorbis inside the game's page with libvorbis compiled to WebAssembly, the first time each track plays.

Ren'Py games run on Ren'Py's own engine. The Mac app bundles two official Ren'Py runtimes, 8.5.3 for Ren'Py 8 games and 7.8.7 for Ren'Py 7 and older games, and picks one from the game's `renpy/vc_version.py` or `renpy/__init__.py`, then its `lib/` folder names, then the engine's compiled files. Each game runs in its own process and window, which takes the hub window's place, and the hub comes back when the game quits. Saves and logs go to the game's data folder, and the game folder is never written to. A game copied from a PC with saves in `game/saves/` gets those copied in on its first launch.

On iPhone and iPad, Ren'Py 8.5.3 is built into the app and every Ren'Py game runs on it, inside VNDegen's window. Ren'Py 7 and 6.99 games run on it too, with fixes VNDegen applies in memory, such as dividing integers the Python 2 way in the game's own code. 15 of the 16 Ren'Py 7 games in the maintainer's library reached their main menu this way, and VNDegen ships that list. The page of a Ren'Py 7 game says whether it is known to work and, once it has run, whether it reached its main menu on this device. On Mac the game page's Ren'Py runtime option runs a game on the other runtime.

Ren'Py can't load saves made by a newer Ren'Py. VNDegen runs each game on the newest Ren'Py of its line, so saves made here may not load in the game's PC version, and the game's Saves page warns before a Ludusavi backup when that applies. VNDegen on other devices loads them. The exception is a Mac, which runs Ren'Py 7 games on Ren'Py 7.8.7, so it loads a Ren'Py 7 game's saves from an iPhone or iPad only once that game's Ren'Py runtime option is set to Ren'Py 8.5.3. A Ren'Py 7 game's persistent data, such as unlocked gallery images, is written so its PC version loads it.

`docs/engine-survey.md` covers which other engines are common and how each could be supported.

## Adding games

Put each game in its own folder, named as you want it to appear. The folder name is the game's identity, so the same game copied to several devices keeps its artwork and settings on each.

- **iPhone and iPad:** open the Files app, then On My iPhone (or iPad), VNDegen, Games, and copy the game folders there.
- **Mac:** copy the game folders into `~/Games/VNDegen`. It sits outside `~/Documents` so iCloud Drive does not upload your games.
- **From an archive:** Add game in the library unpacks a zip, RAR, or 7z into the library. You can also drag an archive from Files or Finder onto VNDegen, or open it in VNDegen from the Files Share menu or with Open With on a Mac.
- **Archive copied into VNDegen's folder:** copy a zip, RAR, or 7z into On My iPhone (or iPad), VNDegen, or its Games folder. On a Mac, copy it into `~/Games/VNDegen`. The library lists it under Ready to import, and its Import button opens the import with the archive chosen. "Delete archive after import" starts on for these archives. Use this when choosing an archive, dropping it, or opening it in VNDegen does nothing, which happens on some sideloaded installs.

VNDegen rescans the folder on launch, on pull to refresh, with Rescan in Settings, and when you return to the app.

## Controls

| Action | Controller | Keyboard in the library |
|---|---|---|
| Move | D-pad or left stick | Arrows or WASD |
| Confirm | A | Return or Space |
| Cancel | B | Escape or Backspace |
| Menu | Y or Menu | Tab |
| Dash (in game) | X | |
| Previous and next page | L1 and R1 | Q and E, or Page Up and Page Down |
| VNDegen menu (resume or quit to library) | Options or Home | Backtick (`` ` ``) |

The controller layout is the same in the library and in games. While a game runs, the keyboard and mouse go straight to the game, which uses its own keys. For RPG Maker those are the arrows, Z or Return, X or Escape, Shift, and Q and W. Only the backtick is reserved for VNDegen. A Ren'Py game reads the controller itself in Ren'Py's own layout, except that Options and Home open the VNDegen menu, inside the game. Buttons remapped for the game apply there too. On iPhone and iPad a Ren'Py game takes touches directly, with no touch controls over it, so its game page offers no control method. The small menu button in the corner opens the VNDegen menu.

These are the standard mappings. In a game, the VNDegen menu's Controls section and the game page's Controls section switch the control method and open the button mapping editor. Options, Home, the overlay's Home button, and the backtick always open the VNDegen menu, so a mapping cannot lock you out. A remapped overlay button shows its key, for example "Esc".

<p align="center">
  <img src="docs/screenshots/touch-controls.jpg" width="560" alt="On-screen gamepad overlay with a D-pad, A B X Y buttons, shoulder buttons, and menu buttons over No Socks RPG gameplay, with the party on the beach map">
  <img src="docs/screenshots/unlock.jpg" width="200" alt="Passcode keypad for unlocking hidden games, with a controller focus ring on one key">
</p>

## Metadata sources

Steam and VNDB work without setup. SteamGridDB and IGDB need keys, entered in Settings, Metadata Sources. The keys are stored in the Keychain.

- **SteamGridDB:** an API key from your SteamGridDB profile preferences.
- **IGDB:** the client ID and secret of a Twitch developer application.

## Ludusavi

VNDegen reaches your Ludusavi backups on the SMB share that Ludusavi's cloud sync uses. It has rclone's own SMB code built in, the code Ludusavi's rclone runs on the PCs, so it works the same on iPhone, iPad, and Mac with nothing to install.

Set it up once in Settings, under Ludusavi backups:

1. Enter what Ludusavi's cloud settings show for its SMB remote: the host, the port (445 unless you changed it), and the username and password. Guest is on for a new setup and matches Ludusavi's `guest` username, which logs in with no password.
2. Enter the path from Ludusavi's cloud settings. It is the share, then the folder inside it, such as `games/Ludusavi`.
3. Tap Test Connection. VNDegen lists the folder and shows how many games it found. The test only reads.
4. Turn on Match my Ludusavi settings and enter the retention limits from Ludusavi's Other screen. On a Mac with Ludusavi installed, VNDegen reads them from Ludusavi's config.

The password stays on the device and out of VNDegen's settings file. On iPhone and iPad it is in the Keychain, for this device only. On a Mac it is in a file only your user can read, in VNDegen's Application Support folder, so app updates don't ask for your password. rclone gets it obscured, in a config that lives in memory and never touches an rclone.conf.

VNDegen never uploads with a sync. Ludusavi's upload is `rclone sync`, which deletes whatever the PC lacks. VNDegen only copies its new backup folder and then its mapping.yaml, so it never removes another device's backups.

VNDegen 0.4.0 connected over SSH and rsync. Its settings turn save sync off after the update, with a note in Settings, until you enter the share. VNDegen deletes the SSH key it made then.

Each game then finds its Ludusavi game by itself: by the Steam app ID in its metadata through Ludusavi's manifest, else by title. You can pick another on the game's Saves page. That page lists the game's backups with their date, source device, and kind, says whether the saves here or on the server are newer, and previews what a restore changes.

- **Restore** brings a backup down and copies it into the game's save folder, after keeping a copy of the current saves for Undo.
- **Back up now** writes a full or differential backup in Ludusavi's format, as Ludusavi would, with the same file paths as your existing backups, so Ludusavi restores it on the PC and Steam Deck. A game with no backup yet goes where the PC keeps it, under the Steam library the other backups use. With retention limits set, backups beyond them leave the backup list, as Ludusavi drops them. Their folders stay on the server until Ludusavi on a PC deletes them at its next backup of that game, because another device may still list them. Each device names its backup folders with an ID of its own, and VNDegen checks the server's backup list again right before it adds to it, so a backup from another device is not overwritten.
- **Before a game starts**, VNDegen asks when another device backed up newer saves, or when the saves changed both here and on the server.
- **Automatic backup** when a game closes is available and off by default. It skips a game whose saves changed in both places and leaves the choice to you.

Ludusavi on a PC that syncs its backup folder with rclone sees VNDegen's backups as changes in the cloud. When Ludusavi asks, choose to download them.

## Building

You need a Mac with Xcode 27, [XcodeGen](https://github.com/yonaskolb/XcodeGen) (`brew install xcodegen`), and Go (`brew install go`) for the rclone library that save sync uses. Run `scripts/build.sh` on that Mac:

```sh
scripts/build.sh test      # package tests
scripts/build.sh mac       # macOS app in build/mac/Build/Products/Debug/
scripts/build.sh ios-sim   # iOS simulator app in build/sim/Build/Products/Debug-iphonesimulator/
scripts/build.sh ipa       # unsigned build/VNDegen.ipa
```

Every build first makes rclone's library from `Packages/VNDegenKit/Rclone/` for iPhone, iPad, the simulator, and the Mac (`scripts/rclone-runtime.sh`, kept until its sources or the Go version change). The macOS build downloads the two Ren'Py SDKs once, checks their SHA-256, and copies the trimmed runtimes into the app (`scripts/renpy-runtime.sh`, cached in `~/Library/Caches/VNDegen/renpy-sdk`). iOS builds also download Ren'Py's iOS libraries (renios) and link them into the app. `scripts/build.sh` does that before it builds; in Xcode, run `scripts/renpy-runtime.sh build/renpy ios` once first. `scripts/renpy-compat.sh <folder>` boots every Ren'Py game in a folder and prints which runtime each got and whether it reached its main menu. `scripts/renpy-hook-test.sh` checks VNDegen's Ren'Py hook on both runtimes.

From another machine, `scripts/remote.sh` copies the checkout to a Mac over SSH and runs a command there. For example, `VNDEGEN_MAC=my-mac.local scripts/remote.sh scripts/build.sh ipa`. `scripts/sim.sh` boots a simulator, installs the app, copies a game in, and launches it.

### Installing a release

The [Releases](https://github.com/ChronoStriker1/vndegen/releases) page has a built IPA for iPhone and iPad and a zipped app for Mac, with install steps in each release's notes. `scripts/release.sh <version>` publishes a new one from `main`.

### Installing with AltStore or SideStore

Add this source in AltStore or SideStore to install VNDegen and get updates:

```
https://raw.githubusercontent.com/ChronoStriker1/vndegen/main/altstore/source.json
```

### Installing on iPhone or iPad

`scripts/build.sh ipa` makes an unsigned IPA. Sideloadly or AltStore signs and installs it with a free Apple ID. Apps signed that way expire after 7 days and need to be installed again.

## Project layout

```
App/                      app entry point, icons, Info.plist
Packages/VNDegenKit/
  Sources/VNCore/         shared types: games, the library, the engine plugin contract, input events
  Sources/WebRuntime/     shared host for HTML5 engines: WebKit view, game file serving, Node and NW.js layer
  Sources/EngineRPGMaker/ RPG Maker MV and MZ: detection, key mapping, save locations
  Sources/EngineTyrano/   TyranoScript: detection, Electron app.asar reading, save handling, controller mapping
  Sources/EngineRenPy/    Ren'Py: detection, runtime choice, the macOS helper process, the iOS in-app session, and the in-game hook
  Sources/CRenPy/         starts Python and runs Ren'Py inside the app on iOS
  Sources/RenPyCompat/    command-line boot test for a folder of Ren'Py games
  Sources/HubUI/          the library, game pages, settings, and the player screen
  Sources/GameInput/      game controllers, keyboard, and the touch overlay
  Sources/Metadata/       Steam, SteamGridDB, IGDB, and VNDB
  Sources/Lock/           Face ID, Touch ID, and passcode
  Sources/SaveSync/       Ludusavi backup and restore
  Sources/Rclone/         Swift calls into rclone's library
  Rclone/                 VNDegen's build of rclone: the Go library with the SMB and local backends
  Sources/ArchiveImport/  unpacking zip, RAR, and 7z archives into the library
docs/                     engine survey and screenshots
scripts/                  build, remote build, simulator, and Ren'Py runtime helpers
project.yml               XcodeGen project definition
```

### Adding an engine

An engine is a type conforming to `EnginePlugin` in `VNCore/Engine.swift`:

- `detect(_:)` inspects a folder listing and returns a match with a confidence level.
- `makeSession(_:)` starts the game and returns an `EngineSession`, whose view the hub shows full screen and which receives controller input.
- `saveLocations(folder:dataDirectory:)` tells Ludusavi sync where the engine keeps saves.

Add the new plugin to the `engines` list in `App/VNDegenApp.swift`. The detector picks the most confident match among all registered plugins.

## License

VNDegen's own code is under the MIT License (`LICENSE`). The engines and libraries built into the app keep their own licenses. A build that includes a GPL component is distributed as a whole under that GPL.
