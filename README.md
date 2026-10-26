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
- **Every input method.** Touch, mouse and keyboard, and game controllers all drive both the hub and the games. On a touch screen without a controller, an on-screen overlay works as a gamepad or as a virtual mouse cursor.
- **Hidden games.** Games marked Hide disappear from the library until it is unlocked with Face ID, Touch ID, or an app passcode. The library locks again when the app goes to the background.
- **In-game orientation.** On iPhone, games lock to landscape by default. On iPad the lock applies only when VNDegen is full screen, not in split screen or a window. The setting can be changed globally or per game: Landscape, Portrait, or Follow device.
- **Ludusavi saves.** VNDegen reads and writes [Ludusavi](https://github.com/mtkennerly/ludusavi) backups, so saves move between VNDegen and the PC copies Ludusavi manages. Restores can be undone.
- **Hardware rendering.** Web-based engines render through WebGL on Metal.

<p align="center">
  <img src="docs/screenshots/no-socks-rpg-iphone.jpg" width="820" alt="No Socks RPG, an RPG Maker MZ game, running in landscape on iPhone with its in-game menu open, loaded from a Steam save">
</p>

## Engines

| Engine | Detected by | Platforms |
|---|---|---|
| RPG Maker MZ | `js/rmmz_core.js` | Mac, iPhone, iPad |
| RPG Maker MV | `js/rpg_core.js` or `www/js/rpg_core.js` | Mac, iPhone, iPad |

RPG Maker MV and MZ games ship as HTML5 inside NW.js. VNDegen runs them in WebKit with a small NW.js compatibility layer, so plugins that call Node's `fs`, `path`, or `nw.gui` keep working. The layer also means saves are written as `save/*.rmmzsave` exactly like the PC version, and encrypted images and audio, Effekseer effects, and WebM movies all work.

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

The controller layout is the same in the library and in games. While a game runs, the keyboard and mouse go straight to the game, which uses its own keys. For RPG Maker those are the arrows, Z or Return, X or Escape, Shift, and Q and W. Only the backtick is reserved for VNDegen.

<p align="center">
  <img src="docs/screenshots/touch-controls.jpg" width="560" alt="On-screen gamepad overlay with a D-pad, A B X Y buttons, shoulder buttons, and menu buttons over No Socks RPG gameplay, with the party on the beach map">
  <img src="docs/screenshots/unlock.jpg" width="200" alt="Passcode keypad for unlocking hidden games, with a controller focus ring on one key">
</p>

## Metadata sources

Steam and VNDB work without setup. SteamGridDB and IGDB need keys, entered in Settings, Metadata Sources. The keys are stored in the Keychain.

- **SteamGridDB:** an API key from your SteamGridDB profile preferences.
- **IGDB:** the client ID and secret of a Twitch developer application.

## Ludusavi

Point VNDegen at your Ludusavi backup folder in Settings. On a Mac it reads the folder from Ludusavi's own configuration. On iPhone and iPad, pick the folder in the file picker. That can be a network share connected in the Files app.

Each game page then lists that game's Ludusavi backups with their date and source device:

- **Restore** copies a backup into the game's save folder, after keeping a copy of the current saves for Undo.
- **Back up now** writes a new backup in Ludusavi's own format, using the same file paths as your existing backups so Ludusavi restores it correctly on the other machines.
- **Automatic backup** when a game closes is available and off by default.

VNDegen never changes or deletes existing backups.

## Building

You need a Mac with Xcode 27 and [XcodeGen](https://github.com/yonaskolb/XcodeGen) (`brew install xcodegen`). Run `scripts/build.sh` on that Mac:

```sh
scripts/build.sh test      # package tests
scripts/build.sh mac       # macOS app in build/mac/Build/Products/Debug/
scripts/build.sh ios-sim   # iOS simulator app in build/sim/Build/Products/Debug-iphonesimulator/
scripts/build.sh ipa       # unsigned build/VNDegen.ipa
```

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
  Sources/HubUI/          the library, game pages, settings, and the player screen
  Sources/GameInput/      game controllers, keyboard, and the touch overlay
  Sources/Metadata/       Steam, SteamGridDB, IGDB, and VNDB
  Sources/Lock/           Face ID, Touch ID, and passcode
  Sources/SaveSync/       Ludusavi backup and restore
  Sources/ArchiveImport/  unpacking zip, RAR, and 7z archives into the library
docs/                     engine survey and screenshots
scripts/                  build, remote build, and simulator helpers
project.yml               XcodeGen project definition
```

### Adding an engine

An engine is a type conforming to `EnginePlugin` in `VNCore/Engine.swift`:

- `detect(_:)` inspects a folder listing and returns a match with a confidence level.
- `makeSession(_:)` starts the game and returns an `EngineSession`, whose view the hub shows full screen and which receives controller input.
- `saveLocations(folder:dataDirectory:)` tells Ludusavi sync where the engine keeps saves.

Add the new plugin to the `engines` list in `App/VNDegenApp.swift`. The detector picks the most confident match among all registered plugins.
