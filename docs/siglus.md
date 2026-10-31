# SiglusEngine and RealLive

VNDegen runs VisualArt's SiglusEngine and RealLive games on siglus_rs, an open-source Rust reimplementation of both engines (https://github.com/xmoezzz/siglus_rs, MPL-2.0). `Engines/Siglus/UPSTREAM.md` lists the pinned commit, the licenses, how the engine is built, and every change VNDegen made to it.

Status, 2026-10-31: the plugins, the engine framework and its build are in place. No SiglusEngine or RealLive game is in the library yet, so no game has run in VNDegen.

## Design

### The runtime

siglus_rs pinned at `5f358ef1360b0439e92aed2b749e8f25d8721954` (its 1.0.0 tag, 2026-10-08). It has two runtimes:

- SiglusEngine runs in `siglus_scene_vm`, which draws with wgpu. Upstream's iOS app hands it a `UIView` whose layer is a `CAMetalLayer`, and wgpu's Metal backend presents to that layer. Upstream's macOS app opens its own winit window, which VNDegen can't embed, so VNDegen's own crate builds the same host from an `NSView` instead.
- RealLive runs in the `reallive` crate, which composites each frame on the CPU and returns RGBA pixels. VNDegen presents those frames through the shared `FrameHost` module, as the NScripter and Wolf RPG runtimes do.

Both play sound through kira, which uses cpal and Core Audio on Apple platforms.

The project was created in October 2026 and has 356 commits. Its release build for Linux succeeds with the system Rust 1.99.1 in three minutes, and its README shows Summer Pockets REFLECTION BLUE, Rewrite's benchmark and anemoi running on SiglusEngine, and Little Busters! and Tomoyo After on RealLive. How well it runs the English releases is untested until a game arrives.

### VNDegen's C interface

`Engines/Siglus/vnsiglus` is a crate of VNDegen's own (MIT). It depends on `siglus_scene_vm` and `reallive` and exports one table of C functions, `vnsiglus_get_api_v1`, declared in `Packages/VNDegenKit/Sources/CSiglus/include/vnsiglus.h`. Swift finds the table with `dlsym` at run time, as EngineArtemis finds art3m1s-core's, so the package builds and tests without the engine. The table covers:

- a SiglusEngine game on a view: create from a `UIView` or `NSView`, step one frame, resize, the mouse, keys, text, pause and resume, the game's message boxes, and destroy
- a RealLive game: open, step, the RGBA frame, the mouse, keys, text, pause and resume, and close
- the default font, which the host passes as the font pack's MS PGothic substitute

### Linking

The engine is built as a dynamic framework, `VNSiglus.framework`, in `build/siglus/VNSiglus.xcframework` for iOS devices, the iOS simulator, and macOS (arm64 and x86_64). The app links and embeds it as it does KiriKiri's. A second Rust static library next to art3m1s-core would put two copies of Rust's standard library, with the same symbol names, into one binary. A framework keeps its copy to itself. iOS allows embedded frameworks, and the framework runs in the app's process, so the iOS rules hold: no JIT, Metal rendering, no separate process, and no SDL.

`scripts/siglus-engine.sh` builds the framework on a Mac with the pinned Rust toolchain and writes the license notices to `build/siglus/licenses/`. `scripts/build.sh` runs it and lists `siglus` among its licensed runtimes.

### Plugins

`EngineSiglus` holds two plugins on the same engine, so the library shows each game's real engine:

- `SiglusPlugin` (`siglus`): a `Scene.pck` or `Gameexe.dat` in the game folder or one folder down. Certain.
- `RealLivePlugin` (`reallive`): a `Gameexe.ini` with a `SEEN.TXT` beside it or in a folder directly below it, such as `DAT/`, in the game folder or one folder down. AVG32, RealLive's predecessor, uses the same names, and its `SEEN.TXT` starts with `PACL`, so such a folder isn't taken for RealLive. Certain.

A folder with both kinds of files is a SiglusEngine game, since some SiglusEngine games ship a `Gameexe.ini` too.

AVG32 and UK2, which siglus_rs also runs, aren't detected: no library game or English test game uses them.

### Saves

- SiglusEngine writes the PC engine's own save files, `savedata/0000.sav` and up, `global.sav`, `read.sav` and `config.sav`, in `savedata/` beside the game, where the PC version keeps them. Ludusavi backups map onto that folder.
- RealLive writes siglus_rs's own format (`RLRSSAV1`), which the PC version can't read. VNDegen keeps those saves in `savedata_rs/` in the game's data folder, and the plugin's backup warning says so. The PC version's `SAVEDATA/` files are left alone.

### Input

Both engines read a mouse and keyboard, so games start with the "Keyboard and mouse" layout. Touches act as the mouse. The keyboard reaches the game through the host view, as in Artemis. Through VNDegen's buttons, Confirm presses Return, Cancel presses Escape, Menu right clicks, Dash holds Control, and the page buttons turn the mouse wheel.

A SiglusEngine game's message box shows as a system alert. Confirm, Return or Space picks its first button, and Cancel, Escape or E its last. VNDegen's on-screen keyboard types into a game's text field when one has focus, and presses keys otherwise.

While VNDegen's menu is open, the game stops and its sound is muted. Kira 0.9, the engines' mixer, can't pause, so sounds keep playing unheard. Each engine writes its global save then, since iOS can end a paused app without warning.

### File name case

`siglus_scene_vm` already resolves game paths without regard to case (`resource.rs`, `resolve_windows_case_insensitive_path`), and the `reallive` crate reads files through its `game_fs` layer, which VNDegen patches to do the same. Paths that bypass both are patched as the case check finds them.

## License review

Every crate in the iOS and macOS dependency tree was listed with `cargo license` (246 crates). Most are MIT, Apache-2.0, BSD, Zlib, ISC, Unicode-3.0, or MPL-2.0, including Symphonia's audio decoders. These parts of siglus_rs can't ship, and VNDegen's copy leaves them out:

- `crates/na_wmv_player`, the WMV and WMA decoder. Its WMA Pro files carry `SPDX-License-Identifier: LGPL-2.1-or-later`, and its comments describe the video decoder as an integer-for-integer translation of FFmpeg's `libavcodec/vc1dsp.c`, which is LGPL. Without it, `.wmv` movies don't play.
- `crates/siglus_scene_vm/assets/fonts/default.ttf`, which is Microsoft's MS PGothic, compiled into the engine. VNDegen passes the font pack's substitute (Ume P Gothic) at run time instead.
- `crates/reallive/src/dt00` and `crates/reallive/src/pt00`, machine translations of decompiled Key DLLs: Tomoyo After's dungeon game (`dt00.dll`) and Little Busters!' baseball game (`PT00.DLL`). That code has no license of its own. Without it, those minigames' DLL calls return 0. This is a question for the user (see below).
- egui and egui-wgpu, used only by upstream's desktop windows. egui's font crate bundles fonts under the Ubuntu Font License, which isn't on the allowed list.
- ureq and its root certificates (webpki-roots, CDLA-Permissive-2.0), used only to post to Twitter.

The `reallive` crate cites rlvm (GPL-3.0) twice, as the source of its reading of the HIK animation format and of a parabola in one effect, and its tests can run against rlvm's test data. The crate's code isn't otherwise attributed to rlvm. Per the brief, rlvm itself was not read.

`theora-rs` describes itself as a rewrite of libtheora 1.2.0, whose license is BSD-3-Clause, so its notice ships with libtheora's license text.

## Estimate

- Linux: the vendored copy, VNDegen's crate and its tests, detection tests: half a day. Done before any game arrives.
- Apple: the framework build, the Swift plugin, and the iPad simulator smoke test: one to two days.
- Per game, once games arrive: title, play, save and load on the Mac and iPad, the case check, and a movie frame rate check: about a day each for the first SiglusEngine and RealLive games, then less.

## Test games

None are in the library. VNDB's engine field for the English Windows releases on Steam:

| Engine | Game | VNDB release |
|---|---|---|
| SiglusEngine | planetarian HD | r96042 |
| SiglusEngine | Summer Pockets (not REFLECTION BLUE, which is Luca System) | r71942 |
| SiglusEngine | Rewrite+ | r46419 |
| SiglusEngine | Harmonia (not the Full HD Edition, which is Luca System) | r86010 |
| RealLive | Planetarian ~the reverie of a little planet~ Steam Edition | r34973 |
| RealLive | CLANNAD Steam Edition | r66916 |
| RealLive | Tomoyo After ~It's a Wonderful Life~ English Edition | r21869 |

planetarian ~Snow Globe~ on Steam is Luca System.

## Known limits

- No game has run yet. Everything about games is untested.
- `.wmv` movies don't play, because the WMV and WMA decoder is LGPL. What a game does when the engine can't open its movie hasn't been checked. `.omv` (Ogg Theora) and `.mpg` (MPEG-1 and MPEG-2) movies use decoders VNDegen ships.
- Tomoyo After's dungeon game and Little Busters!' baseball game don't run (see the open question below).
- Sound keeps playing, muted, while VNDegen's menu is open.
- RealLive saves don't move to or from the PC version.
- SiglusEngine games read a gamepad on the PC. VNDegen sends them keys and clicks instead, so a game's own gamepad prompts don't show.
- Network features, such as Rewrite's Twitter posting, do nothing.

## Open question for the user

The decompiled minigame DLLs (`dt00`, `pt00`) are left out until the user decides. The options are to leave them out, so Tomoyo After's dungeon game doesn't run, or to ship them, accepting code translated from Key's binaries in a public app. The recommendation is to leave them out.
