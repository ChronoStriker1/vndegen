# Construct

Construct 3 (Scirra) games are HTML5. VNDegen runs them on the WebKit host the other web engines use, with the game's
own Construct runtime, so they work on Mac, iPhone and iPad. The plugin lives in `EngineHTMLVN`, beside Visual Novel
Maker and Twine, with the engine ID `construct`. Construct 2 exports (`c2runtime.js`) aren't covered: the library has
none.

## Test game

| Game | Export |
|---|---|
| Transpile Girl Rescue Operation! v2024.03.03 | Construct 3, Windows WebView2, worker mode, Spine 4.1 characters, box2d physics, 2,388 WebM Opus sounds |

It is the library's only Construct game. A search of the library folders for `c3runtime.js`, `c2runtime.js` and
`offlineClient.js`, and of the archives the engine survey couldn't read, found no other.

## What a game folder holds

A Construct 3 export is `index.html`, `data.json` (the project), `scripts/c3runtime.js` (the engine),
`scripts/main.js` and `workermain.js`, with `images/`, `media/` and `fonts/`. The Windows WebView2 export puts these in
`www/`, beside a small wrapper `.exe`, `WebView2Loader.dll`, `WV2ExtManager.dll`, wrapper extensions named `*.ext.dll`
and a `package.json` with the window's settings. The HTML5 export is the web files alone. Windows ignores case, and
Transpile Girl asks for `scripts/` and `fonts/` while its folders are `Scripts/` and `Fonts/`; VNDegen's file lookups
find them whatever the case.

Detection looks for `index.html`, `data.json` and `scripts/c3runtime.js` in `www/` or at the top of the copied folder,
and in folders up to two levels down, because an archive import wraps the game in a folder named after the archive.
The shallowest game wins. At the top of the folder the match is certain, and inside wrapper folders it is likely. Its
version is "Construct 3".

## Running

The page runs as a plain browser page with no Node, as the WebView2 wrapper gives it none. `construct.js`, a page
script, does four things before the game's own scripts run:

- **DOM mode.** A game exported in worker mode runs its engine in a Web Worker with an `OffscreenCanvas`. The script
  hides `OffscreenCanvas` from the page, and Construct's own check then runs the engine in the page itself, as it does
  in browsers without it. Only the page gets VNDegen's localStorage files and input, so the engine has to run there.
- **The WebView2 wrapper.** A WebView2 export talks to its wrapper through `chrome.webview` and waits for the
  wrapper's answer before it starts. The script answers as a wrapper with no extensions, so Steamworks and the other
  wrapper extensions report themselves unavailable, and acknowledges fullscreen requests. Moving and resizing the
  window do nothing.
- **Storage.** Construct keeps the Local Storage plugin's keys and its own savegames in IndexedDB, through a store
  class every export defines as `self.KVStorageContainer`. The script replaces that class with one on the page's
  localStorage, which VNDegen keeps in per-game files, one key per file named `<store>/<key>`, the value as JSON.
- **Sounds load when first played.** See below.

### Sound

The game's sounds are WebM Opus, which WebKit decodes. A project can ask for every sound to be decoded before its
first layout; Transpile Girl does, for 2,388 sounds, about 2.8 hours of audio and about 2 GB once decoded. VNDegen
skips that preload on every device: the page script makes Construct's list of sounds to preload empty, and each sound
is fetched and decoded the first time the game plays it, then kept, as Construct does with any sound it didn't
preload.

Measured on the Mac mini and an iPad Pro 13-inch simulator, from the page starting to load to Construct's first tick:

| | Title, preload kept | Title, preload skipped | Web content memory, kept | Skipped |
|---|---|---|---|---|
| Mac app | 19.0 s | 2.2 s | 2,963 MB footprint | 916 MB footprint |
| iPad simulator | 22.7 s | 2.9 s | 3,019 MB peak resident | 839 MB peak resident |

A sound's first play starts 5 to 30 ms after the game asks for it on the Mac and 5 to 91 ms on the simulator, and
the title music, the largest file, 131 to 253 ms; a replay starts at once. Construct starts a sound once it is
decoded and then plays it whole, so a first play starts that much later and isn't cut short. 2 GB of decoded sound
would likely get the app's web content stopped on an iPhone.

### Input

Construct's Mouse object takes only pointer events whose `pointerType` is `mouse`, and Transpile Girl has no Touch
object, so on a PC a touch screen does nothing. In VNDegen a finger is the mouse: the script marks the primary touch's
pointer events as the mouse's. The pointer the session moves for controllers and the touch modes sends mouse events,
which Construct ignores, so the script sends a matching mouse pointer event for each, as the Flash page does for
Ruffle. While a finger is down the session's own pointer is left alone, so a drag isn't released halfway.

## Saves

Construct writes saves to IndexedDB. On the PC that is the LevelDB database inside the WebView2 profile, which
Ludusavi can't map to files, so, as for Twine, `saveLocations` names the localStorage folder in the app's data
directory. Saves move between VNDegen devices and don't reach a PC.

## Controls

Transpile Girl reads the mouse, Space (reads on), the left, right and down arrows (a minigame), and Escape, and no
gamepad. Games start with "Visual novel" with A as a left click, as Visual Novel Maker does: A clicks what the pointer is on and
reads on, the D-pad and left stick are the arrows, Menu and Y are Escape, and the right stick moves the pointer.

## Engine files

Remove engine files lists the WebView2 wrapper of a game at the top of its folder: the one `.exe` when there is only
one and it is under 10 MB, `WebView2Loader.dll`, `WV2ExtManager.dll` and every `*.ext.dll`. `www/` and `package.json`
stay. A game inside wrapper folders, or an HTML5 export, offers nothing. Transpile Girl's wrapper is 0.9 MB; the
WebView2 runtime itself is installed with Windows.

## Results

Checked at `1a62af57` on 2026-11-17. Evidence is in `.audit/evidence/construct`.

Mac app (DEBUG, its own library, the mini's screen unlocked, through the DEBUG pointer, key and controller hooks):

- Added to the library, recognised as a certain Construct 3 match from `www/scripts/c3runtime.js`, started from the
  library. The title, a new game through the prologue (Space reads on) to the Main screen, a save to slot 01 on the
  game's Save screen, the app quit, started again, and Load, slot 01, back to the Main screen.
- Options from the game's gear, a Go Out story with Hina's voice, and the popcorn minigame, where holding A and
  pushing the right stick drags Hina.

iPad simulator (iPad Pro 13-inch, current iOS, slim, landscape), through the Device panel:

- By touch: Start Game, dialogue, the LOG screen's voice replay, and in the minigame a drag that moves Hina across the
  screen. A save made on the Mac, copied into the iPad's data folder, loads; then a save to slot 02, the app closed
  and opened again, and Load, slot 02, back to the Main screen.
- The touch gamepad, in "Visual novel for Construct": A clicks through the Main screen's tutorial.
- Remove engine files lists the wrapper `.exe`, `WebView2Loader.dll`, `WV2ExtManager.dll` and
  `D3D11Overlay_x64.ext.dll`, 938 kB (not removed).
- Case check (`scripts/case-check.sh`, 2,942 names swapped, so the page's `scripts/` and `fonts/` meet `sCRIPTS/` and
  `fONTS/`): the title, a new game, a load, a save to slot 02 and, after starting again, its load. No file failed to
  load.

Frame rate: Construct draws on vsync, 60 here. The Main screen with Hina's animated figure and the popcorn minigame
held 60.0 on the Mac app (1% low 50.0) and 59.9 on the simulator (1% low 47.6). [frame-rate.md](frame-rate.md) has
the rows.

The game was also run on Linux in Playwright's WebKit and Chromium with the same page script, to compare engines.

## Known limits

- Only the Windows WebView2 export has been played. HTML5 exports are detected and should run the same way; NW.js
  exports are detected but would miss the `nw` object their export type expects.
- Saves don't reach a PC.
- Wrapper extensions such as Steamworks are unavailable, so Steam achievements and overlays do nothing.
- Pressing Space or clicking again and again while a scene fades can leave Transpile Girl waiting for its own
  "Transition End" signal, which never comes, so the story stops on that line. Two of the game's waits are left
  pending. The same input does the same in Chromium, the engine of its PC wrapper, so it is the game's. It took
  presses 0.35 s apart on the Mac and taps 0.6 to 0.7 s apart on the simulator; presses 3 s apart went through.
- On the Save and Load screens the first press on a SAVE or LOAD button only highlights it, and the second acts. Not
  compared with the PC.
