# Flash

VNDegen plays Flash games in [Ruffle](https://github.com/ruffle-rs/ruffle), a Flash Player emulator written in Rust.
Its web build runs on the WebKit host that RPG Maker MV/MZ and TyranoScript games use, rendering through WebGL on
Metal. Nothing runs outside the web view, and WebAssembly needs no JIT of VNDegen's own, so it works on iPhone and iPad.
`Engines/Flash/UPSTREAM.md` covers the Ruffle version, VNDegen's patch, the build, and the licenses.

## What a game folder can hold

`EngineFlash` scans the game folder up to four folders down and finds three kinds of entry:

- **A Windows projector**, `Game.exe`: Flash Player's standalone player with the movie appended. It ends with the
  movie's length and the marker `0xFA123456`. VNDegen serves the movie at the projector's own path rather than
  unpacking it, so the movies the game loads resolve beside the `.exe`, and its saves are named after the `.exe`, as on
  the PC. Detection is certain for these.
- **A `.swf` movie.** Detection is likely, so another engine that recognizes its own files wins. A game that ships a
  Mac projector, `Game.app`, has the same movie inside it, which comes last.
- **An HTML page that launches movies**, as collections ship to choose an episode in a browser. A page that links to
  other pages is a menu and comes first. A page that only wraps one movie isn't an entry, since the movie plays better
  on its own.

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
and a link to a `.swf` or projector opens it in the host page.

Ruffle's settings: autoplay on, no splash screen or unmute overlay, letterboxing, no context menu (the right click
belongs to the game), links to websites refused, high quality, which Ruffle would lower on a phone or tablet, and
device text drawn with the browser's fonts, which include Japanese. Ruffle draws with its WebGL renderer. Its default,
wgpu on WebGL, fails to translate a shader on Apple's WebKit and stops the movie, though it works in WebKit on Linux.
Script access is on, so a movie's `fscommand` reaches the page: a projector's `quit`, sent by its Exit button, ends the
session as quitting the projector would. The page belongs to the game, as the scripts of the other web engines' games
do.

Input reaches Ruffle as it would from a PC mouse and keyboard. The session's pointer, from the touch Mouse mode and
controllers, arrives as synthetic mouse events, which `flash.js` turns into pointer events on Ruffle's canvas. Ruffle
reads keys only while it has focus, so the player takes focus again when the game resumes. Flash Player read no
gamepad, so games start with the Keyboard and mouse layout.

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

The `.sol` bytes are the same on both sides. A backup's path maps to the movie in this game folder that the longest end
of its PC path names, so where the game sat on the PC doesn't matter. The same goes for Ruffle on the PC, which keeps
`.sol` files under `ruffle/SharedObjects/localhost/`, and for movies run from a network drive, under `#localWithNet`.
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

On Linux, in Playwright's WebKit with the same host page and Ruffle build, all 14 reach their title and play past it.
Two `.sol` saves that shipped with games load: a game started with one and Ruffle wrote back the same data, with the
endings it records unlocked, where a fresh start writes them locked.

## Known limits

- Ruffle's ActionScript 3 support is less complete than its ActionScript 1 and 2 support. A game that uses an API Ruffle
  lacks may stop, show a black screen, or skip what the API did. Test Game 127 needed VNDegen's patch for one.
- Shared objects are written when the game pauses or quits, as Flash Player wrote them when it quit. A game that ends
  without either, as when the app crashes, keeps the saves from its last pause.
- A launcher page's own layout may suit a desktop browser window more than a phone.
- Microphone input, which one test game offers for voice control, doesn't exist in Ruffle. The game plays without it.
