# RPG Maker XP, VX and VX Ace feasibility spike

Spike date 2026-10-26, on branch `ws/rgss`. The questions were whether mkxp-z can run the library's RGSS games (RPG Maker XP, VX and VX Ace) on macOS and on the iPhone simulator, how the RPG Maker RTP should work, and what mkxp-z's GPL means for VNDegen. This document records the answers, the experiments behind them, a recommended architecture, and the remaining work. This reading of licenses is not legal advice.

## Answer

Yes, through mkxp-z's libretro core rather than its standalone app.

- **The libretro core is the better base.** mkxp-z has an open pull request ([#255](https://github.com/mkxp-z/mkxp-z/pull/255), branch `libretro` in `white-axe/mkxp-z`, by an mkxp-z maintainer, last pushed 2026-06-30, discussed through 2026-09) that builds mkxp-z as a libretro core. It compiles Ruby 3.3 to WebAssembly and back to C with wasm2c, so Ruby is portable C with no JIT and no OS calls of its own. The core does not own the main loop, the window, or the GL context. The host calls `retro_run` once a frame, hands it a GL context and framebuffer, and answers its environment, file system, audio and input calls. The Ruby VM lives in a sandbox the host can destroy and recreate, so one app process can run game after game. The standalone app instead owns an SDL window and an MRI VM that can't be restarted, and on iOS its SDL2 and ANGLE would collide with the SDL2 and MetalANGLE that Ren'Py already links.
- **All four test games run on macOS in the core,** rendered by ANGLE on Metal inside a small headless host written for this spike. Test Games 33, 37 and 14 reach their title menus, and Test Game 55 starts on its map, which is how that game opens. Test Game 33 loaded a save copied from the PC, Test Game 37 played into its story from New Game, and Test Game 55 saved, then loaded that save in a fresh run. Each needed fixes, listed under Evidence, and every fix is general rather than per game.
- **The same four results hold on the iPhone simulator.** The core, ANGLE and the host built for the arm64 simulator and ran there. Each checked frame is byte-identical to the macOS run at the same frame, including Test Game 55's Load screen listing the save it wrote in the simulator.
- **The RTP can't ship with VNDegen.** Its license forbids redistribution. Three of the four test games need it. The user downloads the official RTP, and VNDegen extracts it. The official downloads are Inno Setup installers, which innoextract unpacks on any platform.
- **The GPL applies to the combined app.** mkxp-z is GPL-2.0-or-later. An IPA or Mac app that contains it must be distributed under the GPL with its complete source, which VNDegen's public repository already provides. VNDegen's own code is now under MIT, which keeps a later App Store build possible, as long as that build leaves out the GPL engines. No permissively licensed RGSS player is usable today.

## Built

The plugin followed the recommended architecture. What changed from the plan, and what was checked:

- **Engine.** `scripts/rgss-engine.sh` builds the core and ANGLE from the pinned sources in `Engines/RGSS/UPSTREAM.md` into an arm64 `RGSS.xcframework` for macOS, iOS, and the simulator, about 58 MB a slice before compression. The first build took about an hour on the Mac mini, most of it stage 1. Stage 1 now runs on a Mac too: its Makefile wants GNU sed and grep, Ruby 3.3 or older as the build machine's Ruby, and bash for its `echo`, since macOS's `/bin/sh` turns the `\x` escapes it writes into raw bytes. The app copies the framework in and opens it at run time instead of linking it, so the universal Mac app builds and an Intel Mac shows these games as unsupported.
- **Frontend fixes found in the app.** The core reads RGSS's buttons from the libretro joypad only, so the frontend presses the joypad buttons for RPG Maker's keys. RGSS reads input once a frame, so a release sooner than 50 ms after its press waits, and a mouse press stays down until the core has seen it. The core caches GL state, so drawing the frame into the view saves and restores what it changes; without that, text drawn into bitmaps went missing on iOS. On iOS ANGLE's Metal layer doesn't follow its parent layer's size, so the view sizes it after each layout.
- **Preload script.** Stage 1 packs only the preload scripts its Makefile names, so `zz_vndegen.rb` ships in the framework and each launch copies it into the engine's `Scripts/Preload` folder.
- **RTP.** The game page offers Download and install and Import from folder for each RTP a game's `Game.ini` names and isn't installed. The installers carry no license text, so the app shows the agreement from the download page, kept in `EngineRGSS/Resources`, and installs only after Accept.
- **Checked on macOS (Mac mini, DEBUG build, its own library).** All four test games reach their title. Test Game 55 saved through its menu and loaded after a quit to the library and a new launch in the same process. Test Game 33 loaded its PC save. Test Games 37 and 14 played from New Game. Controller input, the hardware keyboard, and the VNDegen menu's Quit to library worked. The three RTPs installed from the official downloads through the game page, a cancel during unpacking left nothing behind, and each install matched innoextract's output file for file.
- **Checked on the iPhone 18 Pro simulator in landscape**, through T3's device tools: the same four titles with the RTPs installed in the app, Test Game 55's save and load through the touch gamepad, and Test Game 37 played from New Game by touch.

## Evidence

Everything ran on the Mac mini (Apple M4, macOS 27.0.1, Xcode 27.1). Logs and frames are in `.audit/evidence/rgss-spike/` in the checkout, which is gitignored. The test games were copied from the NAS read only and extracted on the Mac mini under `~/vndegen-testgames/rgss/`.

The host is `docs/rgss-spike/harness.m` with `vfs.c`. It loads the core with `dlopen`, makes an ANGLE EGL context on the Metal backend with an offscreen framebuffer, answers the core's environment calls, implements the libretro file system interface on POSIX files, plays scripted controller input, and writes chosen frames as PNG. It runs the core as fast as it can, which also measures speed. `docs/rgss-spike/run.sh` sets up a run with the RTPs and the spike's preload script.

| Experiment | Result | Evidence |
|---|---|---|
| CI-built macOS core (`650cb08`), any game | Crashes building the file name cache. On Apple platforms mkxp-z converts names with iconv's `utf-8-mac`, which the bundled GNU libiconv lacks, so `iconv_open` fails and the next `iconv` call crashes. Upstream [#397](https://github.com/mkxp-z/mkxp-z/pull/397) is open for the same code | `tg55-mac.log` |
| Standalone mkxp-z (`dev`, `37a04d1`) built on the Mac mini | Same `libiconv` crash under `createPathCache` for Test Games 14, 33 and 37. Test Game 55 stayed up | crash reports `bundle_mkxp-z-*.ips` on the Mac mini |
| Core built here with `mkxp-z-iconv.patch` (skip the conversion when iconv can't do it) | Loads games | |
| Test Game 55 (VX Ace, unpacked, no RTP) | Stops in a script that names a DLL by path (`System\name`). mkxp-z's Win32API emulation raises `NameError` on that name. With the preload fix, starts on its map and plays | `tg55-mac-899.png` |
| Test Game 55 save, then load in a new run | `Save01.rvdata2` (118 KB) written to the save folder. The next run's Load screen lists it and loads it | `tg55-save-01250.png`, `tg55-load-00250.png`, `tg55-load-00899.png` |
| Test Game 33 (VX, `Game.rgss2a`, VX RTP) | Title menu with the RTP's window skin and font. Continue loads the PC save in the game folder into the map | `tg33-mac-899.png`, `tg33-load-1499.png` |
| Test Game 37 (XP, `Game.rgssad`, XP RTP) | Title menu. New Game stopped: the game converts its Shift-JIS CSV data with kernel32's `MultiByteToWideChar`, which the emulation lacks. With the preload fix, plays into the story | `tg37-mac-899.png`, `tg37-new-02399.png` |
| Test Game 14 (VX Ace, `Game.rgss3a`, VX Ace RTP) | Two stops. `load_data` of a missing optional file trapped the sandbox instead of raising `Errno::ENOENT`. Then Ruby 3 rejects a class variable read at the top level, which Ruby 1.9 allowed. With the preload fix and `ruby-rgss-cvar.patch` in a stage 1 built here, reaches its title menu | `tg14-mac-1499.png` |
| Speed, headless, 3,000 extra frames on an animated map, on a Mac shared with other builds | Test Game 33 about 4.5 ms a frame and Test Game 55 about 5.4 ms, so 185 to 220 frames a second against the 60 a game needs. The same runs took twice as long or more while other threads were building, so these are rough figures | `p33.log`, `p55.log` |
| Audio | Every game produced 48 kHz stereo samples with music (RMS 1,150 to 3,900). The core embeds the public domain GMGSx soundfont, so the RTPs' MIDI music plays | run logs |
| ANGLE (chromium/8037, mkxp-z's Meson build) for macOS and the iOS simulator | Builds. The iOS branch of mkxp-z's ANGLE build names `SystemInfo_ios.mm`, which ANGLE renamed to `SystemInfo_ios.cpp` | `angle-meson.build` |
| Core and host for the iOS simulator (arm64, iOS 18), run with `simctl spawn` in an iPhone 18 Pro simulator | Renderer "ANGLE Metal Renderer: Apple iOS simulator GPU". Test Game 33's title, Test Game 37's story after New Game, Test Game 14's title, and Test Game 55's save, Load screen and loaded map all match the macOS frames byte for byte. Audio matches too | `sim-tg*.log`, `sim-tg33-899.png`, `sim-tg14-1499.png`, `sim-tg55-load-250.png` |

The fixes, all in `docs/rgss-spike/`:

- `mkxp-z-iconv.patch` skips the NFD to NFC file name conversion when iconv can't do it. APFS keeps names as written, so names copied from Windows are already NFC.
- `zz_vndegen.rb`, a preload script, passes Win32API only the DLL's file name, raises `Errno::ENOENT` from `load_data` before the core opens a missing file, and implements `MultiByteToWideChar` and `WideCharToMultiByte` with Ruby's encodings. The `load_data` guard stands in for a fix in the core's C++ binding, where a Ruby exception raised under `rb_file_open` reaches the sandbox as an out-of-bounds trap.
- `ruby-rgss-cvar.patch` makes Ruby warn, as Ruby 2 did, instead of raising on class variable access from the top level. It joins the core's existing `ruby-compat.patch`, which already restores other Ruby 1.8 and 1.9 behavior.

## RTP

The RTP is the set of stock graphics, sounds and fonts that RPG Maker games may use without shipping them. `Game.ini` names it: `RTP=RPGVXAce` (VX Ace), `RTP=RPGVX` (VX), or `RTP1=Standard` (XP).

| Test game | `Game.ini` | Needs it | Why |
|---|---|---|---|
| 55 (VX Ace) | `RTP=` (empty) | No | Ships every asset it uses |
| 14 (VX Ace) | `RPGVXAce` | Yes | Its archive has no tilesets |
| 33 (VX) | `RPGVX` | Yes | Its archive has 2 system images and no tilesets or audio |
| 37 (XP) | `Standard` | Yes | Its archive has no tilesets or autotiles |

The archive contents come from `rgssad.py list`. The three games ran with the RTPs mounted.

**Redistribution.** The RTP license on [rpgmakerweb.com/run-time-package](https://www.rpgmakerweb.com/run-time-package) grants a license "to use the RTP SOFTWARE" and says the licensee "shall not sell, distribute, assign, lease, sublicense, encumber, or otherwise transfer the RTP SOFTWARE" without the publisher's written consent (VX Ace version dated 2012-03-15, with the same restriction in the XP and VX versions). VNDegen can't bundle it.

**Downloads.** All three are free from Degica and are Inno Setup 5 installers. innoextract 1.9 (zlib license) unpacked each on the Mac mini.

| RTP | Download | Installer | Unpacked |
|---|---|---|---|
| XP | `https://assets.rpgmakerweb.com/xp_rtp104e.exe` | Inno Setup 5.2.3 | 22 MB |
| VX | `https://assets.rpgmakerweb.com/vx_rtp102e.zip`, holding `RPGVX_RTP/Setup.exe` | Inno Setup 5.1.10 | 42 MB |
| VX Ace | `https://assets.rpgmakerweb.com/RPGVXAce_RTP.zip`, holding `RTP100/Setup.exe` and `Setup-1.bin` | Inno Setup 5.4.2 | 195 MB |

**On the device.** The user downloads the official file in Safari or a desktop browser, then imports it into VNDegen from Files, by drag and drop, or with Open in, the same ways archives import today. VNDegen unpacks the installer into its own `RTP/<name>` folder (`Standard`, `RPGVX`, `RPGVXAce`), which the core mounts for each game that names it. That needs an Inno Setup 5 reader in the app. innoextract is C++ with Boost, LZMA, zlib and bzip2, so it is a port, not a drop-in. A smaller first step accepts an RTP folder copied from a PC into `RTP/` through Files. The game page warns when a game names an RTP that isn't installed.

**Fonts.** The VX Ace RTP's font is VL Gothic and the VX RTP's is UmePlus Gothic, both freely redistributable. VNDegen can bundle them, so text looks right in games that ship no fonts, even without the RTP. XP games use Windows system fonts. mkxp-z substitutes its built-in Liberation Sans for any font it doesn't find.

## Licenses

| Component | License |
|---|---|
| mkxp-z, including the libretro core | GPL-2.0-or-later |
| Ruby 3.3 | Ruby license or BSD-2-Clause |
| ANGLE | BSD-3-Clause |
| PhysFS, SDL_sound, theoraplay, zlib | zlib |
| OpenAL Soft, FluidSynth | LGPL |
| FreeType | FreeType License or GPL-2.0-or-later |
| libogg, libvorbis, libtheora, FLAC, Opus | BSD-3-Clause |
| pixman, libyaml | MIT |
| uchardet | MPL-1.1, GPL-2.0-or-later or LGPL-2.1-or-later |
| GMGSx soundfont | Public domain |

What this means:

- **The app becomes a GPL work.** An IPA or Mac app that contains the core is one combined work, and distributing it means offering its complete corresponding source under the GPL. VNDegen's repository is public and its build scripts fetch and patch every runtime, which covers that once the RGSS build script and patches are committed. Each release also needs the license texts in the app.
- **VNDegen's own license.** At the time of the spike the repository had none, which means all rights reserved. The maintainer has since licensed VNDegen's own code under MIT, and builds that include mkxp-z are distributed as a whole under GPL-2.0. Under GPL-2.0, FreeType is taken under its GPL option, and Apache-2.0 code such as OpenSSL stays out, which the libretro build already does. The core's EGL headers carry an Apache-2.0 notice. They are API declarations, compiled in but not linked as a library.
- **App Store.** The FSF holds that Apple's App Store usage rules are further restrictions that the GPL forbids ([FSF, 2010](https://www.fsf.org/blogs/licensing/more-about-the-app-store-gpl-enforcement)), and VLC left the App Store in 2011 over a GPL complaint until it was relicensed. mkxp-z has many authors, so it can't be relicensed. A future App Store build would have to leave out the RGSS engine, which the plugin design allows, and any other GPL engine such as ONScripter. The sideloaded IPA and the Mac app, distributed with source, are the normal route for GPL apps on iOS. Ren'Py's static LGPL libraries are already covered by publishing the source.
- **Alternatives.** No maintained, permissively licensed RGSS player exists. tapir (MIT or Apache-2.0) was abandoned in 2017 and runs only on Linux. URGE (MIT) is active but Windows only. Other iOS RGSS ports, such as mkxp-z-apple-mobile and RPGViewer-MKXP-Z, are GPL forks of the standalone mkxp-z. They keep one MRI VM for the process, so switching games needs a relaunch, and they link several Ruby versions to cover XP and VX games.

## Recommended architecture

**One engine for both platforms.** The mkxp-z libretro core, pinned to a commit of the pull request's branch and patched by VNDegen, runs inside the app on macOS and iOS alike. macOS does not need a helper process, because the core can be unloaded and its Ruby sandbox recreated for the next game.

- **Runtime build.** `scripts/rgss-runtime.sh` fetches the pinned source, applies VNDegen's patches, builds stage 1 (Ruby through wasm2c, platform independent) and stage 2 for each Apple slice, builds ANGLE with the same Meson files, and makes one `RGSSRuntime.xcframework`. The framework is dynamic and exports only VNDegen's own C entry points, so its ANGLE never meets the MetalANGLE that Ren'Py links. Stage 1 needs WASI SDK 30 and Binaryen. It took 10 minutes in Docker on a 32-core Linux machine and can run on the Mac too. Results are cached like the Ren'Py runtimes.
- **`CRGSS`.** The libretro frontend in C: an EGL window surface on the session view's `CAMetalLayer` through ANGLE, the core's framebuffer, the VFS, audio into an `AVAudioEngine` source node, the input state, core options, and the save and system folders. Its core is the spike's host without the PNG writer.
- **`EngineRGSS`.** Detection by `Game.ini` next to `Game.rgssad`, `Game.rgss2a`, `Game.rgss3a`, or `Data/Scripts.rxdata`, `.rvdata` or `.rvdata2`. The session drives `retro_run` from a display link on a thread that owns the GL context. Controller and touch overlay buttons map to the libretro joypad buttons the core reads as RGSS's C (confirm), B (cancel and menu) and A (dash), keys to the core's keyboard callback, and pointer events to its mouse. The hub keeps its own touch overlay and menu, as for RPG Maker MV and MZ.
- **Saves.** The core reads the game folder and the save folder as one, and writes only to the save folder. Saves go to the game's data directory, and PC saves in the game folder stay readable, as Test Game 33's did. The Ludusavi location maps a PC path ending in `<game>/Save*.rxdata`, `.rvdata` or `.rvdata2` to the save folder.
- **Compatibility layer.** VNDegen's preload script and Ruby patches, and a command-line runner like `RenPyCompat` that boots every RGSS game in a folder and reports which reach their title, as the spike's host does.

## Remaining work

- **A real device.** Speed and memory are measured only on the M4 and in the simulator.
- **The library run.** A runner that boots all 44 of the library's RGSS games and reports which reach their title, like `RenPyCompat`, is not built yet.
- **Japanese text in games with no fonts.** The core's fallback font, Liberation Sans, has no Japanese glyphs. XP games use the Windows system fonts, and VX and VX Ace games without their RTP's font would show empty boxes. Bundling VL Gothic, which is freely licensed, as the engine's fallback would cover them. Not tested.
- **Upstream.** The iconv and static-core patches, and a fix for Ruby exceptions raised inside the core's bindings, which `zz_vndegen.rb` works around for `load_data`, could go to the libretro pull request.

## Risks

- **The libretro branch is not merged.** It is 1,064 commits from one author, and upstream has not reviewed it. VNDegen would pin a commit and carry patches. If the branch stalls, VNDegen maintains a fork.
- **Ruby 3.3 runs games written for Ruby 1.8 and 1.9.** The core's compatibility patch and VNDegen's cover the cases seen so far. Others will show up game by game.
- **Win32API and DLLs.** Games that call Windows DLLs get emulated functions or no-ops. Some features of some games will not work.
- **Speed.** wasm2c Ruby is slower than native MRI. The test games' maps ran at three to four times real time on the M4 headless. Older iPhones may not keep up with script-heavy games. Not measured on a device, and the simulator's GPU path says nothing about device speed.
- **App size.** The stripped core is 54 MB without link-time optimization (43 MB in the CI's optimized build) and ANGLE about 10 MB, or roughly 25 MB compressed in the IPA.
- **GPL.** The decision above has to be made before release.

## Reproducing

- Stage 1: `docs/rgss-spike/stage1-docker.sh` in `ubuntu:24.04` with the libretro branch checked out at `/src`, after adding `ruby-rgss-cvar.patch` to `libretro/Makefile` after `ruby-prng-time.patch`.
- Stage 2 on a Mac: `meson setup build -Dlibretro=true -Dlibretro_save_states=true --cross-file <file>` then `ninja -C build`, with `mkxp-z-iconv.patch` applied. `meson-iossim-arm64.txt` is the simulator cross file.
- ANGLE and the host: a Meson project made of `angle-meson.build` with mkxp-z's `subprojects/` copied beside it, with `SystemInfo_ios.mm` renamed to `.cpp` in the ANGLE build file for iOS.
- `rgssad.py list|extract|scripts` lists RGSS archives, extracts a file, or dumps a game's scripts as `.rb` files.
