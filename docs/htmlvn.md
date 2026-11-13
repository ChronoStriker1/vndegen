# Visual Novel Maker and Twine

Visual Novel Maker (Degica) and Twine games are HTML and JavaScript. VNDegen runs them on the WebKit host that RPG
Maker MV and MZ, TyranoScript and Flash use, with the game's own scripts. Both engines live in the `EngineHTMLVN`
module, as two plugins with their own engine IDs, `visualnovelmaker` and `twine`. Nothing new runs outside the web view,
so they work on Mac, iPhone and iPad. Dorian Engine has no Windows releases and isn't covered.

## Test games

Free official releases from itch.io, all in the library:

| Game | Engine | Packaging |
|---|---|---|
| Siren's Call 1.131 (Windows) | Visual Novel Maker | Electron, `resources/app/`, Live2D characters, one WebM splash movie |
| EVISCERATETHISGIRL.COM 2.1 | Twine 2, SugarCube 2.36.1 | one `index.html` beside its `assets/`. It replaces SugarCube's UI and never saves |
| Forgotten Fiction | Twine 2, SugarCube 2.34.1 | one `Forgotten Fiction.html` beside its pictures and sounds. It removes SugarCube's UI bar, so it offers no way to save |
| Asher (Windows) | Twine 2, Harlowe 2.1.0 | a Unity player with Zen Fulcrum's Embedded Browser, the story and pictures packed in `Asher_Data/Resources/browser_assets`. It has its own Save and Load passages |

## Visual Novel Maker

### What a game folder holds

A Windows export is NW.js (older Visual Novel Maker) or Electron (1.2 and later, such as Siren's Call). Either way the
game is `index.html`, `data/ENGINE.js`, `data/SCRIPTS.js`, `data/*.json.js` for the project's data, and `resources/`
for pictures, sound, movies and Live2D models. The web root is the folder that holds `Game.exe` for NW.js,
`resources/app/` for Electron, or `resources/app.asar` and `package.nw/` where a game was packed that way. A folder with
those files and no `package.json` is a web export.

Detection looks for `index.html` with `data/ENGINE.js` and `data/SCRIPTS.js` in one of those web roots, in the copied
folder and in folders up to two levels down, because an archive import wraps the game in a folder named after the
archive. The shallowest game wins. At the top of the folder the match is certain, and inside wrapper folders it is
likely, as for RPG Maker and TyranoScript. ENGINE.js carries no version number, so the match has none.

### Running

The session uses the NW.js runtime for NW.js builds and the Electron runtime for Electron builds, with the game's own
folder (the one holding `Game.exe`) as the shim's working directory, so relative paths resolve as on the PC.

Visual Novel Maker picks `.m4a` and `.mp4` files when it sees Safari, and it lists only the files a game ships, so the
session reports the Chrome user agent TyranoScript uses, and Ogg Vorbis audio goes through the in-page decoder. WebM
movies and Live2D (Cubism 2 and 4) play in WebKit.

### Saves

`GameStorage` in ENGINE.js writes `<key>.vndata` files, such as `SaveGame_0.vndata`, `SaveGame_0_Header.vndata`,
`settings.vndata` and `globalData.vndata`, to the folder the project's System record names in `gameDataPath`:

- 0, the working directory, the default and what Siren's Call uses: beside `Game.exe`.
- 1, AppData: NW.js's `nw.App.dataPath`, which the shim points at the game folder, or Electron's `--user-data-dir`
  switch in `process.argv`, which the shim doesn't set.
- 2, internal storage, meant for phones. It writes to the root of the file system, which the shim refuses.

`saveLocations` returns the game folder, and its rule maps any backed-up path whose file name ends in `.vndata` to that
name, so a Ludusavi backup from Windows restores beside `Game.exe` whichever of the first two folders it came from. Web
exports, which keep saves in localStorage, get the per-game localStorage files TyranoScript uses.

### Controls

Visual Novel Maker reads the keyboard and the mouse. Its gamepad code waits for a `Gamepad` object that nothing sets,
so it reads no gamepad. Games start with "Visual novel for VN Maker": "Visual novel" with A as a left click, which the
plugin asks for through `EnginePlugin.defaultControlChanges`. A reads on and presses the button under the pointer, X holds
Control, which skips in games that skip on it, the right stick moves the pointer, and LB and RB turn the wheel. Siren's Call reads on only with a
click: its prologue waits for a mouse button by its own script, and in dialogue the engine gets Return (key code 13,
its confirm key, at keydown and keyup) and the waiting message's handler runs, yet the line doesn't move on, since each
line ends in the game's own `RocketEndOfMessage` event. VNDegen's key events carry what the engine reads, the key code
through jQuery; nothing in ENGINE.js or SCRIPTS.js reads `key`, `code` or `isTrusted`.

### Engine files

Remove engine files lists the Electron or NW.js files of a game at the top of its folder, the same lists TyranoScript
uses. The web root, `package.json`, and the `.vndata` saves stay. A game inside wrapper folders, or a web export,
offers nothing. By those lists Siren's Call has 202 MB of Electron, counted by hand on its folder, including the renamed runtime `Game.exe`. In the library it sits inside a wrapper folder, so it offers nothing there.

## Twine

### What a game folder holds

A Twine 2 game is one HTML file holding the story format's code and `<tw-storydata>` with the passages. Twine 1 games
hold `<div id="storeArea">` instead. Windows releases ship that file loose, wrapped in NW.js or Electron (still a loose
file in `package.nw/` or `resources/app/`), or, as Asher does, inside a Unity player that shows it in an embedded
Chromium.

Detection reads `.html` and `.htm` files in the library's three-level listing, shallowest first and at most 16 of them,
skipping Chromium's license pages, and scans each for those markers through its first 4 MB, stopping at the first
match. It also opens Unity Embedded Browser packs, `*_Data/Resources/browser_assets`, and checks their `index.html`.
The match is certain when the story is at the top of the folder, in an NW.js or Electron web root there, or in an
Embedded Browser pack there, and likely inside other folders. Its version is the story format's, such as
"Harlowe 2.1.0" or "SugarCube 2.36.1".

A Unity player whose pack holds a story is a Twine game: Twine's match is certain and Unity's is never more than likely,
so Twine wins, and the game plays in VNDegen on every device instead of showing Unity's page.

### Embedded Browser packs

`browser_assets` is a .NET `BinaryWriter` stream: the string `zfbRes_v1`, an Int32 entry count, then for each entry its
path (from `/`), an Int64 offset from the start of the file and an Int32 length, followed by the files. The `WebRuntime`
game folder serves a pack as a web root, as it serves an Electron `app.asar`, so the story's relative picture paths
resolve inside it. Nothing is unpacked.

### Running

The page runs as a plain browser page with no Node, whatever the wrapper was. Its localStorage, where Harlowe and
SugarCube keep their saves, goes to per-game files in the app's data folder, as for TyranoScript browser builds. Twine
pages scroll, unlike the other web engines' canvases, so `WebGameConfiguration.scrollsPage` lets the page scroll by
touch on iPhone and iPad, and tells the NW.js shim not to put the page back at the top, which it does for one-screen
games after a text field scrolled them. A page script, `twine.js`, turns the mouse wheel VNDegen sends, which LB and RB
turn, into scrolling.

A page's `alert()`, `confirm()` and `prompt()`, which Harlowe before 3.2 uses for `(alert:)`, `(confirm:)` and
`(prompt:)` and Twine 1 stories for a name, show as the system's alert, with a text field for `prompt()`, as NW.js
and Electron show them on the PC. The game gets no input while one is open. Touch, the mouse and the keyboard answer
it; a controller can't.

### Saves

Twine games keep saves in the browser's localStorage. On the PC that is Chromium's LevelDB inside the wrapper's
profile, which Ludusavi can't map to files, so `saveLocations` names the localStorage folder in the app's data
directory, which moves saves between VNDegen devices only, as for TyranoScript browser builds.

### Controls

Twine reads the mouse and touch, and some games the keyboard. Games start with the "Mouse" layout, picked by playing
Forgotten Fiction with both on the iPad simulator's touch gamepad: with "Mouse", A clicks the link under the pointer,
which a tap on the page also moves there; with "Keyboard and mouse", A is Space, which SugarCube's passage links
ignore. Both turn the wheel with LB and RB.

## Results

Checked at `84f4047`, with the scroll fix from the commit after it checked again on the iPad simulator.

Mac app, on the Mac mini, with its screen locked all evening, so through the DEBUG input script's taps, the debug
controller hooks and the app log, with `-ignoreOcclusion YES`, and no screenshots. Logs are in
`.audit/evidence/htmlvn/v2/mac-logs`.

- All four games are recognised as the right engine and start from the library. Asher is a certain Twine match from
  its Embedded Browser pack; the other three sit in wrapper folders and are likely matches.
- Siren's Call: the content warning, the WebM splash, the main menu, a new game through its prologue to the first
  dialogue scene, a save to slot 1 through the game's own Save menu, and, after quitting and starting again, Load Game
  from the main menu back to the same line. The saves are `SaveGame_0.vndata`, `SaveGame_0_Header.vndata`,
  `globalData.vndata` and `settings.vndata` beside `Game.exe`. With the right stick of the debug controller on Load Game,
  A opened the load menu.
- Forgotten Fiction: the menu and three passages, a save to slot 1 in SugarCube's Saves dialog, and after quitting
  and starting again a load from the same dialog back to the same passage. The game removes SugarCube's UI bar, so
  the dialog was opened with `UI.saves()` from the input script; the Save and Load buttons were clicked.
- EVISCERATETHISGIRL.COM: the Main Menu, Play, and its first passages.
- Asher: a new game, three passages, a save to slot A through its own Save passage, and after quitting and starting
  again Load Game, slot A and Resume Game back to the same passage.

Mac app again on 2026-11-12, with the mini's screen unlocked, through the same hooks and with a capture of the app's
window at each game's title and in its story, all looked at: every page draws, none black, stretched or cut off.

- Siren's Call: on a first start, its "work of fiction" screen and the studio logo splash draw; the main menu, a new
  game through the prologue and a choice to the first dialogue scene, a save to slot 1, the game's own Quit, and
  after starting the app again Load Game back to the same line. Its music plays at the main menu.
- Forgotten Fiction: its music starts on the first passage; a save and a load as before. Its pages fit the window at
  any size, down to 800 by 450, so it has nothing to scroll.
- EVISCERATETHISGIRL.COM: Play to its fifth passage, where its music starts. With the window at 1280 by 500, its About
  page (770 high) scrolls down with LB and up with RB, to the bottom.
- Asher: a save to slot A and, after starting the app again, Load Game, slot A and Resume Game back to the same
  passage. It ships no sound. With the window at 1280 by 500, its About page (882 high) scrolls with LB and RB.

iPad simulator (iPad Pro 13-inch, current iOS), through the Device panel:

- Siren's Call by touch: the main menu, Load Game and slot 1, which loaded the save made in the Mac app at its line.
  On the touch gamepad A, shown as Left click, reads on in the prologue and in dialogue, and RT does too.
- Forgotten Fiction by touch, and with the touch gamepad in both layouts tried for Twine.
- Asher, in a window 1000 by 520 points (`-layoutSize`), so that its About page runs past the bottom: a swipe scrolls
  the page, and RB and LB on the touch gamepad scroll it up and down. Before the fix above the page jumped back to the
  top after every scroll.
- Case check (`scripts/case-check.sh`) on all four games, with every file and folder name's case swapped on a
  case-sensitive volume: Siren's Call reaches its title, loads the save stored as `sAVEgAME_0.VNDATA` and saves over
  those files in place; Forgotten Fiction starts, its sprites and music load, and it saves and loads; EVISCERATETHISGIRL.COM
  and Asher reach their title and first passage with their pictures. Names inside Asher's pack keep their case, as
  for every archive.

Frame rate: Siren's Call has no Live2D models (the engine loads Cubism, but its characters are still pictures), so its
video is the 10-second VP9 splash. It plays at 30.0 fps, its rate, on the Mac app (1% low 24.0) and the iPad simulator
(1% low 28.0), with the page at 60 fps. [frame-rate.md](frame-rate.md) has the rows.

## Known limits

- Siren's Call reads on only with a mouse click. Its prologue waits for a mouse button by its own script, and in
  dialogue neither Return nor Control (X, skip) moves it on, although the engine gets both keys: each line ends in the
  game's own `RocketEndOfMessage` event. A left click on A works around it for the controller. Not compared with the PC.
- A Visual Novel Maker Electron game set to keep its data in AppData stops at its first save, since `process.argv`
  holds no `--user-data-dir`. One set to internal storage can't save. No test game uses either.
- SugarCube's "Save to disk" and "Load from disk" aren't wired.
- Twine saves don't reach a PC.
- Pictures a story loads from the web, such as Asher's title decoration, don't show, since games don't reach the
  network.
- For the same reason, a frame inside a passage that shows a web page, such as an embedded YouTube player, stays
  empty. Only the page itself leaving for a website, or the player clicking a link inside such a frame, opens the
  browser.
