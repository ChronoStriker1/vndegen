# AliceSoft System 4

VNDegen runs AliceSoft's System 4 games on its own runtime, `Packages/VNDegenKit/Sources/CSystem4`, written in C++20
for VNDegen and MIT licensed like the rest of the app. It was written clean-room: `docs/system4-cleanroom.md` lists
where every part came from. The Swift side, `EngineAliceSoft/System4Session.swift`, runs it inside VNDegen's window
through the shared FrameHost module and maps the controls.

## Games

| Test game | Status |
|---|---|
| Rance 02 (Test Game 19) | Title, opening movie, play, camp menus, saves and loads. Saves move between VNDegen and the PC game in both directions. |
| Sengoku Rance (Test Game 46) | Title, opening movie, story scenes, battles, the strategy map, saves and loads. Saves move between VNDegen and the PC game in both directions. |
| Rance IX (Test Game 56) | Logos, warning, opening movie, title, the prologue's story scenes, saves and loads. Its battles and maps are 3D: at the first one, the chapter 1 map, the game stops with a message (see Known limits), and its game page says so beforehand. |

## What the runtime does

- **Code.** It loads the compiled `.ain` (the games' "AI2" zlib container, and the SDK compiler's XORed files) and
  runs it on a stack VM over a heap of reference-counted pages and strings: instructions 0 to 257, including the
  later compilers' shortcut instructions and delegates. The VM runs on its own thread and hands control back to the
  host at each frame or input poll.
- **Libraries.** The HLL libraries the test games call: StoatSpriteEngine and SACT2 (sprites, text, CGs, sound,
  music, transitions, MultiSprite, CharSprite, the backlog and saved scenes), DrawGraph (the whole drawing
  library), SengokuRanceFont, MsgLogManager and MsgLogViewer, DrawMovie2, DrawMovie3 and PlayMovie, VSFile, File,
  FileOperation, MsgSkip, Math, SystemService and SystemServiceEx, InputString, PassRegister, Confirm3,
  OutputLog, IbisInputEngine, AliceLogo3 and 4 (skipped) and DrawPluginManager. Rance IX adds PartsEngine (numbered
  parts with three input states, text, construction steps, motions, click messages, activities, saves),
  ChipmunkSpriteEngine (the frame, transitions and text measures), KiwiSoundEngine, MainEXFile, CommonSystemData,
  AnteaterADVEngine (Stoat's backlog and scene keeper), AFAFactory and CGManager. A call the runtime doesn't have
  is logged once and returns zero. A call to SealEngine or MarmotModelEngine, Rance IX's 3D libraries, other than
  the few each 2D frame makes, stops the game with a message over the dimmed screen until the player quits.
- **Files.** `.ald` and `.afa` archives (pictures by name, and numbered music and sound), `.ex` data files, QNT
  (versions 0 to 2), AJP, 8-bit PMS and WebP pictures,
  including WebPs drawn over a base picture, Ogg and WAV sound, `.alm` MPEG-1 movies with MP2 sound, and `.fnl` font
  libraries. A movie keeps half a second of sound decoded ahead of what has played, and its picture follows the
  sound when the game clock falls behind, so a slow frame doesn't cut the sound.
- **Text.** The game's own `.fnl` font library where it has the character, scaled down from its large bitmaps, then
  VNDegen's Windows font pack (`vnfont_resolve`) for MS Gothic and MS Mincho. Strings are CP932, as on the PC.
- **Saves.** The PC engine's own files in the game's save folder (the ini's `SaveFolder`, `SaveData` by default),
  which `saveLocations` gives Ludusavi:
  - resume saves ("RSM" snapshots of the VM, version 7, 8 or 9 by the game's VM),
  - global and group saves (layout 5, or 7 for Rance IX),
  - the GD container around both (zlib, XORed for Sys43VM games),
  - the slot details VSFile writes, and the read-message file.
  The data the sprite engine hands the game to keep (backlog, back scenes, character sprites, MultiSprite) uses the
  PC library's encodings. Each was checked against saves the PC games wrote under Wine.
- **Input.** The runtime takes Windows virtual-key codes and mouse positions. The games only take a click on a
  button the pointer has rested on, and read the keyboard and mouse once a frame, so the session paces input: a
  touch's press waits until the game has finished three frames with the pointer there, each key or button change
  waits until the game has read the last one, and a press stays down at least 100 ms (a release 50 ms). A game that
  stops finishing frames holds input back at most half a second per change. Pausing drops waiting input and lets go
  of every key and button the game holds. Confirm is Enter, cancel and the menu
  are the right mouse button, the arrows move, page up and down turn the wheel, dash is Ctrl, and the touch mouse
  and touch gamepad send the same. The default layout is keyboard and mouse.

## Development

The Linux test host in `Engines/System4/host` builds the runtime with CMake and a few tools:
- `s4play`, which runs a game headless from a script of frame-numbered clicks, keys and screenshots;
- `s4dis`, a disassembler (`--func`, `--op`, `--lib` for a library's signatures, `--struct`);
- `s4cg` (pictures), `s4save` (save dumps and round trips), `s4movie`, `s4fnl` and `s4drawtest` (DrawGraph formulas).

Environment variables turn on logs:
- `S4_TRACE`: every call with its arguments.
- `S4_HLLTRACE`: every library call.
- `S4_HLLMISSING`: library functions the game declares that the runtime lacks.
- `S4_TEXTLOG` and `S4_MSDUMP`: text drawing and MultiSprite state.
- `S4_AUDIOLOG`: the mix's level, in `s4play`.
- `S4_WAV`: the mix written to a WAV file, in `s4play`.
- `S4_HITCH=n`: in `s4play`, every nth frame takes 200 ms of real time, as a slow frame on a busy machine does.
- `S4_MUTE`: the mix is computed but the output is silent, for timing checks where nobody should hear the game.
- `S4_INPUTLOG`: every key and mouse button change the host sends, with the pointer and the game clock.

The log always notes, every 10 s of game time, frame steps over 100 ms (the clock drops the rest) and audio renders
that took over half their length. When a movie closes it notes the frames shown per second against the movie's rate,
the longest gap between new frames, and how often its sound ran dry. A DEBUG app launched with `-frameMonitor YES`
logs `S4FRAMES` every 10 seconds: display refreshes per second on average and over the slowest 1%, and the slowest
step.
- `S4_FONTS`: the font pack folder on Linux.

## Frame rate

The runtime steps once per display refresh and draws on the CPU. Each test game's heaviest scenes, measured in the
Mac app on a Mac mini (muted with `S4_MUTE`, `-frameMonitor YES`, nothing else heavy running):

| Scene | Target | Result |
|---|---|---|
| Rance 02 opening movie (227 s) | 30 fps movie, 60 Hz display | 6787 of 6810 frames shown, 29.89 a second; longest gap 67 ms; display 59.8 fps average over 25 ten-second windows, median 1% low 42 |
| Sengoku Rance opening movie (170 s) | 29.97 fps movie, 60 Hz display | 5094 frames, 29.85 a second; longest gap 67 ms; display 59.9 fps average over 21 windows, median 1% low 49 |
| Sengoku Rance battle (250 s) | 60 fps | 60.0 average (59.8 to 60.0 per window), median 1% low 60, lowest 41, slowest step 36 ms |

The movies' sound never ran dry in these runs. On an iPad simulator (Mac mini, load average about 10) the Rance 02
opening showed 6806 of 6810 frames, 29.98 a second, its sound never ran dry, and the display averaged 59.9 fps with a
median 1% low of 60. Sengoku Rance's battle on the iPad simulator averaged 59.2 fps over 200 s while other builds
pushed the Mac mini's load average to about 80, with a median 1% low of 37.

## Known limits

- Rance IX's 3D scenes (SealEngine and MarmotModelEngine), which include its battles and maps, don't run. The game
  stops with a message where the first one starts (in the first chapter); saves made before then load.
- Rance IX's Flat animations (the Flash-like `.flat` files) aren't played: each reports itself finished at once, so
  the title screen has no logo animation. Numerals (numbers drawn from digit pictures), colour multiply and add on
  parts, and Parts_SetPartsConstructionSurfaceArea aren't drawn yet.
- Rance IX's message lines sit closer than on the PC with VNDegen's fallback fonts, which are wider than MS Gothic,
  so a line wraps sooner.
- Music loops from its start; the loop points in the `.bgi` files aren't read.
- Only the libraries and calls the test games use are built. Other System 4 games may call functions the runtime
  doesn't have yet; each is logged once.
- `CG_RotateRGB` isn't bound: the SDK doesn't say what its channel swap does.
- A Rance IX save moves between VNDegen and the PC, and the story carries on from it, but the screen shows only the
  message text and the system bar until the scene redraws. PartsEngine keeps the scene's parts in data of its own
  inside the save, which the PC and VNDegen lay out differently. Saves made and loaded in VNDegen restore the whole
  screen.
- Four MultiSprite fields keep the PC's values without a known meaning, and moves in progress aren't saved, as on the
  PC.
- The read-message file's bit order within a byte is assumed; the PC file seen had no bit set.
- The PC engine's window menu bar isn't drawn, and VNDegen's Game commands don't list it. Sengoku Rance (Test Game
  46) has a "Message Skip" item there, but the game turns that item off itself (`msgSkipSetEnable`, which calls
  `MsgSkip.SetEnable`), and its dialogue window has its own Skip button, which skips without the menu's mode. Rance
  02's and Rance IX's menu bars hold only Screenshot, Restart, Exit and About.
- Movies are MPEG-1 video with MP2 sound, which is what the test games ship. A damaged `.alm` could still crash
  pl_mpeg's decoder, whose upstream has open issues with corrupt input.
- Input is paced for the games: a busy game can hold a key change back up to half a second, and a click after a new
  pointer move up to about a second, so a ten-letter name typed on the on-screen keyboard takes a second or two.
- The openings can't be skipped the first time a game runs; they skip with a tap once they have played through. The
  games' scripts decide this, so the PC does the same: Rance 02's `game_main` passes skip only when the opening is
  registered as seen, and Sengoku Rance's `ムービー再生` takes the same flag.
- Rance 02's copy names its movie `Rance02.alm` while the game asks for `ランス０２.alm`. When the named movie is
  missing and its folder holds exactly one movie, that one plays.
