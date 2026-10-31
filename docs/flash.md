# Flash

VNDegen plays Flash games in [Ruffle](https://github.com/ruffle-rs/ruffle), a Flash Player emulator written in Rust.
Its web build runs on the WebKit host that RPG Maker MV/MZ and TyranoScript games use, rendering through WebGL on
Metal. Nothing runs outside the web view, and WebAssembly needs no JIT of VNDegen's own, so it works on iPhone and iPad.
`Engines/Flash/UPSTREAM.md` covers the Ruffle version, VNDegen's patch, the build, and the licenses.

## What a game folder can hold

`EngineFlash` scans the game folder up to four folders down and finds three kinds of entry. Detection itself reads the
library's usual three levels. Only when those show no movie does it look further, below the deepest folders and at no
more than 2,000 entries, for a projector an archive wrapped in folders.

- **A Windows projector**, `Game.exe`: Flash Player's standalone player with the movie appended. It ends with the
  movie's length and the marker `0xFA123456`. VNDegen serves the movie at the projector's own path rather than
  unpacking it, so the movies the game loads resolve beside the `.exe`, and its saves are named after the `.exe`, as on
  the PC. Detection is certain for these.
- **A `.swf` movie.** Detection is likely, so another engine that recognizes its own files wins. A game that ships a
  Mac projector, `Game.app`, has the same movie inside it, which comes last.
- **An HTML page that launches movies**, as collections ship to choose an episode in a browser. A page that links to
  other pages with movies is a menu and comes first, the one that links to the most of them leading, since episode
  pages often link back to it. A page that plays several movies is an entry too. A page that names one movie, however
  many times, isn't, since the movie plays better on its own.

The game starts with the first: the menu page, then the shallowest projector, then the shallowest and largest movie.
Smaller movies beside the main one are usually loaders and mods, and deeper ones are scenes it loads. When a game has
other entries no deeper than the one it starts with, its page offers them under Start with, such as a save-editing
loader or a gallery unlock movie beside the game.

## How it runs

The session opens `vndegen://game/__vndegen/flash.html?movie=<path>`. `/__vndegen/` is the host page from
`EngineFlash`'s resources, and `/__vndegen/ruffle/` is Ruffle's build, which the app bundles as `Ruffle`. Everything
else is the game folder. A launcher page opens at its own path instead.

Two user scripts run at the start of every page: `flash-page.js`, with Ruffle's settings, and Ruffle itself. On the host
page, `flash.js` loads the movie. A movie resolves the URLs it loads against its own folder, as the projector does,
not against the page as Flash did in a browser. On a game's own HTML page, Ruffle replaces each `<embed>` and `<object>`,
`flash-page.js` copies the page's style rules for those elements to Ruffle's, so its movies keep their size and place,
and a link to a `.swf` or projector opens it in the host page, with its query. Every projector in the folder is served as
its movie, so a menu can open any of them, and in whatever case the page asks for it.

Ruffle's settings: autoplay on, no splash screen or unmute overlay, letterboxing, no context menu (the right click
belongs to the game), links to websites refused, high quality, which Ruffle would lower on a phone or tablet, and
device text drawn with the browser's fonts, which include Japanese. Ruffle draws with its WebGL renderer. Its default,
wgpu on WebGL, fails to translate a shader on Apple's WebKit and stops the movie, though it works in WebKit on Linux.
Script access is on, so a movie's `fscommand` reaches the page: a projector's `quit`, sent by its Exit button, ends the
session as quitting the projector would. The page belongs to the game, as the scripts of the other web engines' games
do.

`flash-page.js` also ties every Ruffle player on a page to the session, whether the host page's or one a launcher
embeds: pausing suspends them, a projector's `quit` ends the session, and the session's pointer reaches them.
Input reaches Ruffle as it would from a PC mouse and keyboard. The session's pointer, from the touch Mouse mode and
controllers, arrives as synthetic mouse events, which `flash.js` turns into pointer events on Ruffle's canvas. Ruffle
reads keys only while it has focus, so the player takes focus again when the game resumes. Flash Player read no
gamepad, so games start with the Keyboard and mouse layout.

## Fonts

Most movies embed their fonts. Device text, drawn in a font the computer has, goes through the browser
(`deviceFontRenderer: 'canvas'`), so it gets VNDegen's Windows font substitutes from `docs/fonts.md` under their Windows
names: a movie that asks for MS Gothic gets Ume Gothic. Flash's generic fonts, `_sans`, `_serif`, and `_typewriter` and
their Japanese names `_ゴシック`, `_明朝`, and `_等幅`, become one family each, as Japanese Windows drew them: Latin text in
Arial, Times New Roman, or Courier New (the device's own where it has them, else Liberation), and other text in the MS UI
Gothic, MS PMincho, or MS Gothic substitute. `FlashFonts` builds those families from `FontPack`, and they load before
the movie does. Many movies name fonts from an Office install, such as the HG and DF Japanese fonts, which Windows
itself lacked, and drew them in the system's UI font. Every font Ruffle asks the browser for falls back to the `_sans`
family here for the same reason.

## Saves

Flash games save to shared objects. Ruffle keeps them in localStorage, Base64-encoded under
`<host>/<local path>/<name>`, where the local path is the movie's path, or the shorter one the movie asked for and
empty for `/`. `SharedObjectStore` keeps each one as the `.sol` file Flash Player writes, under `SharedObjects/` in the
game's data folder:

| Saved by | PC path | Here |
|---|---|---|
| `getLocal("save")` in `sub/game.swf` | `%APPDATA%/Macromedia/Flash Player/#SharedObjects/<id>/localhost/<PC path to the game>/sub/game.swf/save.sol` | `SharedObjects/sub/game.swf/save.sol` |
| `getLocal("save", "/")` | `.../#SharedObjects/<id>/localhost/save.sol` | `SharedObjects/save.sol` |
| A projector, `getLocal("save")` | `.../localhost/<PC path to the game>/Game.exe/save.sol` | `SharedObjects/Game.exe/save.sol` |

The `.sol` bytes are the same on both sides. Ruffle names a shared object by the movie's URL, which percent-encodes a
path such as `My Game/game.swf`, and the file here has the plain name, as Flash Player's does. Pages encode a path in
different ways, so `flash-page.js` gives every key one encoding before it reaches storage, the one VNDegen gives movie
URLs, and the save reads back under the key Ruffle asks for. A movie may also ask for a shorter local path, a folder
above it, and save there. A shared object's name with slashes in it starts with `#`, as in `game.swf/#saves/slot1.sol`.
A folder of the local path that starts with `#` starts with `%23` on disk instead, so the two never meet, and a folder
whose name already starts that way, `%23` or `%2523` and so on, gets one more `25`. Every other folder keeps its name. A backup's path maps to the movie in this game folder that the longest end
of its PC path names, so where the game sat on the PC doesn't matter. A save under a folder that holds a movie maps to that folder. The same goes for Ruffle on the PC, which keeps
`.sol` files under `ruffle/SharedObjects/localhost/` with percent-encoded folder names, and for movies run from a network drive, under `#localWithNet`.
Other localStorage keys go to `localStorage/` beside them, as for TyranoScript.

Flash Player writes shared objects when the movie asks with `flush()` or when it quits, and most games never ask.
Ruffle writes them when the page hides. VNDegen makes it write them when the game pauses, as when the VNDegen menu opens
or the app leaves the screen, when the page is hidden, and before the session closes. The web host now lets a page
finish such writes before it closes: `WebGameSession` runs the page's `beforeQuit` hook through the bridge, for at most
five seconds, then tears the page down.

## Tested games

The 14 Flash games in the maintainer's library, Test Games 114 to 127:

| Kind | Games |
|---|---|
| ActionScript 3, `.swf` | 114, 116, 117, 118, 119, 121, 122, 123, 124 |
| ActionScript 3, projector | 127 |
| ActionScript 1 and 2, projector | 115, 125, 126 |
| A collection: an HTML menu over AS2 and AS3 movies, and an AS3 projector | 120 |

All 14 reach their title and play in the Mac app and on an iPad Pro 13-inch simulator, and Test Game 122 also on an
iPhone 18 Pro simulator. The 12 that save, save and load there. Test Game 120, a movie collection, and Test Game 125,
whose movies use no shared objects, save nothing on the PC either. Before that, all 14 reached their title and played in Playwright's WebKit on Linux
with the same host page and Ruffle build.

The case check (`scripts/case-check.sh`) passed with all 14, every file and folder name's case swapped, on a
case-sensitive volume: each reached its title and started on the iPad simulator and in the Mac app, with no file
missed, and the 12 that save saved and loaded. Saves of a movie are named with the movie's path as the disk spells it.

Saves were checked by adding a value to each game's `.sol` file, starting the game again, and quitting: Ruffle wrote
every file back with the value, so it had read it. The `.sol` files that four games ship from the PC loaded in the Mac
app with the endings they record unlocked.

Frame rates, each over 10 seconds of an animated scene, as frames drawn to the canvas:

| Where | 24 fps movies | 30 fps movies |
|---|---|---|
| Mac app, MacBook, unlocked display at 60 Hz | 24.0 to 24.1 average, 1% low 19.6 | 30.0 to 30.1 average, 1% low 20.0 |
| iPad Pro 13-inch simulator, Mac mini | 24.0 to 24.1 average, one game 22.7 after a 308 ms load hitch | 30.0 to 30.1 average |
| iPhone 18 Pro simulator, Mac mini | 23.5 average | |

A 24 fps movie on a 60 Hz display shows each frame for two or three refreshes, so its 1% low of 19.6 fps is that 50 ms
cadence, not a dropped frame. The Mac mini ran other threads' builds and simulators during the simulator runs, and
simulator timing isn't a real device's.

## Review

Codex `gpt-6.1-sol` at xhigh reviewed the branch in four rounds, and every finding was fixed or declined with a reason
recorded here. Round 4's own fixes, to how saves map between keys, files, and PC paths and to the page's storage
wrapper, were checked by the coordinator, not re-reviewed by Codex.

## Known limits

- A movie's own right-click menu items, which an ActionScript 2 movie adds through `ContextMenu.customItems`, don't show. VNDegen turns Ruffle's context menu off, because a right click belongs to the game, and the Game menu can't list them either: Ruffle keeps the list private to its player element, so listing it needs a patched Ruffle build. In the library this hides Usashitsuke's Return to title. Its Quit is covered by VNDegen's menu, and its Fullscreen doesn't apply.
- Ruffle's ActionScript 3 support is less complete than its ActionScript 1 and 2 support. A game that uses an API Ruffle
  lacks may stop, show a black screen, or skip what the API did. Test Game 127 needed VNDegen's patch for one.
- Shared objects are written when the game pauses or quits, as Flash Player wrote them when it quit. A game that ends
  without either, as when the app crashes, keeps the saves from its last pause.
- A launcher page's own layout may suit a desktop browser window more than a phone.
- Flash Player keeps a name with slashes in it, saved for `/`, as `localhost/#name/...`, where a folder at the top of a
  PC drive whose name starts with `#` would also be. A PC save there maps as such a name, unless a movie of the game
  follows it with another folder between them, as the game's own folder would be.
- Two shared objects of a movie whose names differ only in case share one file on a disk that ignores case, as they
  did with Flash Player on Windows.
- The `pagehide` that writes saves also reaches a launcher page's own `pagehide` handlers, if it has any, whenever the
  game pauses. None of the test games has one.
- Microphone input, which one test game offers for voice control, doesn't exist in Ruffle. The game plays without it.
