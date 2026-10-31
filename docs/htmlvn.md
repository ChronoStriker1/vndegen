# Visual Novel Maker and Twine

Visual Novel Maker (Degica) and Twine games are HTML and JavaScript. VNDegen runs them on the WebKit host that RPG
Maker MV/MZ, TyranoScript and Flash use, with the game's own scripts. Both engines live in the `EngineHTMLVN` module,
as two plugins with their own engine IDs, `visualnovelmaker` and `twine`. Nothing new runs outside the web view, so
they work on Mac, iPhone and iPad. Dorian Engine has no Windows releases and isn't covered.

This is the design written at the start of the work (2026-10-31). The sections below it record what was built.

## Test games

Free official releases from itch.io, all in the library:

| Game | Engine | Packaging |
|---|---|---|
| Siren's Call 1.131 (Windows) | Visual Novel Maker | Electron, `resources/app/`, Live2D libraries, one WebM movie |
| EVISCERATETHISGIRL.COM 2.1 | Twine 2, SugarCube 2.36.1 | one `index.html` beside its `assets/`. The game never saves: it replaces SugarCube's UI and turns autosave off |
| Forgotten Fiction | Twine 2, SugarCube 2.34.1 | one `Forgotten Fiction.html` beside its pictures and sounds |
| Asher (Windows) | Twine 2, Harlowe 2.1.0 | a Unity player with Zen Fulcrum's Embedded Browser, the story and pictures packed in `Asher_Data/Resources/browser_assets`. Set aside as a test game by the brief |

The Love ritual demo (NW.js, Live2D title, WebM overlays) was looked at for the design and isn't in the library, so
NW.js builds are covered by package tests only.

## Visual Novel Maker

### What a game folder holds

A Windows export is NW.js (older Visual Novel Maker, such as Love ritual) or Electron (1.2 and later, such as Siren's
Call). Either way the game is `index.html`, `data/ENGINE.js`, `data/SCRIPTS.js`, `data/*.json.js` for the project's
data, and `resources/` for pictures, sound, movies and Live2D models. The web root is the folder that holds `Game.exe`
for NW.js, `resources/app/` for Electron, or `resources/app.asar` and `package.nw/` where a game was packed that way.

Detection looks for `index.html` with `data/ENGINE.js` and `data/SCRIPTS.js` in one of those web roots, in the copied
folder and in folders up to two levels down, because an archive import wraps the game in a folder named after the
archive. The shallowest game wins. The match is certain, with `data/ENGINE.js` as evidence. ENGINE.js carries no
version number, so the match has none.

### Running

The session uses the NW.js runtime for NW.js builds and the Electron runtime for Electron builds, with the game's own
folder (the one holding `Game.exe`) as the shim's working directory, so relative paths resolve as on the PC. A small
page script, `vnm.js`, fills the gaps the shim leaves:

- Electron builds look for `--user-data-dir=` in `process.argv` when a game keeps its data in AppData. The script adds
  one that names the game folder.

Visual Novel Maker picks `.m4a` and `.mp4` files when it sees Safari, and it lists only the files a game ships, so the
session reports the Chrome user agent that TyranoScript uses, and Ogg Vorbis audio goes through the in-page decoder.
WebM movies play in WebKit. The user agent moves into `WebRuntime` so both engines share it.

### Saves

`GameStorage` in ENGINE.js writes `<key>.vndata` files, such as `SaveGame_1.vndata`, `SaveGame_1_Header.vndata`,
`settings.vndata` and `globalData.vndata`, to the folder the project's System record names in `gameDataPath`:

- 0, the working directory, the default and what both test games use: beside `Game.exe`.
- 1, AppData: NW.js's `nw.App.dataPath` or Electron's user data folder. In VNDegen both point at the game folder too.
- 2, internal storage, meant for phones. It writes to the root of the file system, which the shim refuses, so these
  games can't save. No test game uses it.

`saveLocations` returns the game folder, and its rule maps any backed-up path whose file name ends in `.vndata` to
that name, so a Ludusavi backup from Windows restores beside `Game.exe` whichever of the first two folders it came
from. Web exports, which keep saves in localStorage, get the per-game localStorage files TyranoScript uses.

### Controls

Visual Novel Maker reads the keyboard and the mouse. Its gamepad code waits for a `Gamepad` object that nothing sets,
so it reads no gamepad, and the game starts with the "Keyboard and mouse" layout. Its menus are mouse-only: Enter reads
on, Space and H hide the text window, Control skips, and Escape asks to quit or does nothing in the test games.

## Twine

### What a game folder holds

A Twine 2 game is one HTML file holding the story format's code and `<tw-storydata>` with the passages. Twine 1 games
hold `<div id="storeArea">` instead. Windows releases ship that file loose, wrapped in NW.js or Electron (still a
loose file in `package.nw/` or `resources/app/`), or, as Asher does, inside a Unity player that shows it in an
embedded Chromium.

Detection reads `.html` and `.htm` files in the library's three-level listing, shallowest first and at most 16 of
them, skipping Chromium's license pages, and scans each for those markers through its first 4 MB, stopping at the first
match. It also opens Unity Embedded Browser packs, `*_Data/Resources/browser_assets`, and checks their `index.html`.
The match is certain, and its version is the story format's, such as "Harlowe 2.1.0" or "SugarCube 2.36.1".

### Embedded Browser packs

`browser_assets` is a .NET `BinaryWriter` stream: the string `zfbRes_v1`, an Int32 entry count, then for each entry
its path (from `/`), an Int64 offset from the start of the file and an Int32 length, followed by the files. The
`WebRuntime` game folder serves a pack as a web root, as it serves an Electron `app.asar`, so the story's relative
picture paths resolve inside it. Nothing is unpacked.

### Running

The page runs as a plain browser page with no Node, whatever the wrapper was. Its localStorage, where Harlowe and
SugarCube keep their saves, goes to per-game files in the app's data folder, as for TyranoScript browser builds.
Twine pages scroll, unlike the other web engines' canvases, so a new `WebGameConfiguration` option lets the page
scroll by touch on iPhone and iPad. A page script, `twine.js`, turns the mouse wheel VNDegen sends (LB and RB in the
"Keyboard and mouse" layout) into scrolling.

### Saves

Twine games keep saves in the browser's localStorage. On the PC that is Chromium's LevelDB inside the wrapper's
profile, which Ludusavi can't map to files, so `saveLocations` names the localStorage folder in the app's data
directory, which moves saves between VNDegen devices only, as for TyranoScript browser builds.

### Controls

Twine reads the mouse and touch, and some games the keyboard. Games start with the "Keyboard and mouse" layout.

## Status (2026-10-31, work stopped)

Built and committed: the `EngineHTMLVN` module with both plugins, registration in the app, Settings' Plugins entries,
Embedded Browser packs in `WebRuntime`, `WebGameConfiguration.scrollsPage` and `chromeUserAgent`, the shared
localStorage save rule in `WebStorage` (TyranoScript uses it now), `twine.js`, package tests, and README entries.
Version detection for Visual Novel Maker isn't possible, since ENGINE.js holds no version.

Verified:
- `build.sh test` on the MacBook passes, including 15 new engine tests, 3 pack tests and the TyranoScript tests.
- `build.sh mac` succeeded on the MacBook.
- Mac app, on the Mac mini with its screen locked, so through logs only with `-ignoreOcclusion YES`: Siren's Call is
  detected one folder down, boots, starts Live2D's Cubism core and its intro scene, and draws at 60 fps (1% low 40 fps
  over 5 s, with other threads' builds running). EVISCERATETHISGIRL.COM is detected, opens on SugarCube's Main Menu
  and uses localStorage through VNDegen's files.
- Linux, in Playwright's Chromium with VNDegen's shim and a bridge on disk (`~/vndegen-src/htmlvn-tools/harness.mjs`,
  not in the repo): EVISCERATETHISGIRL.COM reaches its Main Menu. Playwright's WebKit needs Ubuntu libraries CachyOS
  lacks, so WebKit itself wasn't run on Linux.

Not done yet, in order:
1. Forgotten Fiction on Linux and in the app: play, save to a slot through SugarCube's Saves dialog, load.
2. Siren's Call past its intro to the title, a new game, a save and a load, on the Mac app, with the saves checked as
   `.vndata` files beside `Game.exe`.
3. `build.sh ios-sim` (it was stopped part way) and both games on an iPad simulator: touch, the touch gamepad, a
   controller, LB and RB scrolling a Twine page, and touch scrolling. One game on an iPhone simulator.
4. The case check (`scripts/case-check.sh`) on both games on the Mac and an iPad simulator.
5. Frame rate of Siren's Call's Live2D and WebM scenes on the Mac app and an iPad simulator.
6. `build.sh ipa`, run once with the engine's cached pieces deleted (this engine has none of its own).
7. A Codex review, then `docs/htmlvn.md` and the README updated with the results.

Known limits so far: a Visual Novel Maker game set to internal storage (`gameDataPath` 2) can't save, and an Electron
build set to AppData stops at its first save, since `process.argv` holds no `--user-data-dir`. No test game uses
either. SugarCube's "Save to disk" and "Load from disk" aren't wired. Twine saves don't reach a PC.

## Estimate

Detection, serving, saves and the two scripts: 1 day. Embedded Browser packs: half a day. Tests, the Mac and iPad
simulator checks of all four games, the case check, the frame rate check of Love ritual's Live2D title and WebM
overlays, and an iPhone check: 1 to 2 days.
