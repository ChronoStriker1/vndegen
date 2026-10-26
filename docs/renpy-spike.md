# Ren'Py feasibility spike

Spike date 2026-10-25, on branch `ws/renpy`. The question was whether VNDegen can run arbitrary Ren'Py games, copied in as folders, natively on macOS and iOS from one app binary. This document records the answer, the experiments behind it, a recommended architecture for each platform, the remaining work, and the risks.

## Answer

Yes on both platforms, with one large caveat on iOS.

- **macOS works today.** VNDegen's new `EngineRenPy` plugin detected Test Game 15 (Ren'Py 8.5.3) and Test Game 30 (Ren'Py 7.4.2) in the library, launched each on a bundled official Ren'Py runtime as a helper process, and both played past the main menu into the story. The hub stayed up and returned when the game quit.
- **iOS boots a real game from Documents.** A simulator app built from Ren'Py's own prebuilt iOS libraries (renios 8.5.3) booted Test Game 15 from the app's `Documents/Games` folder, rendered through ANGLE on Metal, took touch and keyboard input, and reached gameplay. Test Game 30, a Ren'Py 7.4.2 game, also played on that Ren'Py 8 runtime by touch. The app needed a shim to launch, because the iOS 27 SDK refuses apps without the UIScene life cycle and Ren'Py's SDL2 predates it. After the on-screen keyboard appeared, the shimmed window switched to portrait sizing and the game drew into a corner.
- **The caveat is Python 2.** A Ren'Py runtime only runs the `renpy/` Python package of its own release. Ren'Py 7 games run on Python 2 and Ren'Py 8 games on Python 3. macOS can ship both runtimes as separate helper processes. One iOS process can hold only one CPython, so on iOS the Ren'Py 7 games either run on the Ren'Py 8 runtime, which does Python 2 compatibility fixes, or need a second interpreter build with renamed symbols. In the version spread test, Ren'Py 8.5.3 booted 7 of 12 of the library's Ren'Py 7 games, and one shim would likely fix 4 more.

## Evidence

Everything ran on the Mac mini (Apple M4, macOS 27.0.1, Xcode 27.1). The Mac mini had a logged-in console session, so the MacBook was not used. Screenshots and logs are in `.audit/evidence/` in the checkout, which is gitignored.

| Experiment | Result | Evidence |
|---|---|---|
| Test Game 15 (8.5.3) on the 8.5.3 SDK's macOS binary with the game's own `renpy/` | Main menu, then story | `mac-game15-shot-*.png`, `mac-game15-last-log.txt` |
| Test Game 30 (7.4.2) on the Ren'Py 8.5.3 engine | Main menu, then story | `mac-game30-on-8.5.3-shot-*.png` |
| Test Game 30 on the Ren'Py 7.8.7 engine | Main menu, then story | `mac-game30-on-7.8.7-shot-ingame2.png` |
| Test Game 54 (7.5.1) on the Ren'Py 8.5.3 engine | Fails at its splash screen: `ModuleNotFoundError: No module named 'urllib3.packages.six.moves'` | `game54-8-stdout.txt` |
| Test Game 54 on the unpatched 7.8.7 engine | Fails: `SyntaxError: encoding declaration in Unicode string` | Quoted in Version spread |
| Test Game 54 on 7.8.7 with a one-line loader patch | Splash, then its first-run language menu | `mac-game54-on-7.8.7-patched-shot-*.png` |
| 7.8.7 binary with Test Game 54's or Test Game 30's own `renpy/` | Fails: `ImportError: No module named parsersupport` | Quoted in Version spread |
| VNDegen macOS app, `-autolaunch "Test Game 15"` | Helper started on the Ren'Py 8 runtime, game reached the story, quit, app kept running | `app-mac-game15-ingame2.png` |
| VNDegen macOS app, `-autolaunch "Test Game 30"` | Helper started on the Ren'Py 7 runtime, same result | `app-mac-game30-on-7.8.7-ingame2.png` |
| renios 8.5.3 host app in the iOS 27 simulator, game in `Documents/Games` | Fails to launch without UIScene. With the shim, main menu, name entry, and story. Layout breaks after the keyboard | `ios-sim-game15-*.png`, `ios-sim-vndhost-log.txt` |
| Same iOS host app with Test Game 30 (7.4.2) in `Documents/Games` | Main menu, Start by tap, story advancing by tap, layout correct | `ios-sim-game30-*.png` |
| `EngineRenPy` package tests | 7 tests pass | `scripts/build.sh test` |

The macOS screenshots come from inside Ren'Py (`renpy.screenshot`), driven by a probe script copied into scratch copies of the games, because `screencapture` over SSH lacks the Screen Recording permission. The probe is `docs/renpy-spike/zz_vndegen_probe.rpy`. It is never put into a user's library.

## How a distributed Ren'Py game is laid out

The three test games, plus the Ren'Py 8.5.3, 7.8.7, and 7.5.1 SDKs, show this layout:

```
<base>/
  <Name>.exe, <Name>.sh, <Name>.py   launchers. The .py is the real entry point
  renpy/                             the engine's Python package (.py plus .pyc or .pyo)
  game/                              the game: .rpy sources, .rpyc compiled scripts, .rpa archives, assets
  lib/py3-linux-x86_64/              one folder per platform, holding librenpython and a tiny launcher binary
  lib/py3-windows-x86_64/
  lib/py3-mac-universal/             only in builds that include macOS
  lib/python3.12/                    the Python standard library as .pyc files
```

- **Native code.** Each platform folder holds one `librenpython` (`.so`, `.dylib`, or `.dll`) with CPython, every Ren'Py Cython module, pygame, SDL2, FFmpeg, FreeType, HarfBuzz, and the image libraries statically linked. `nm` on the 8.5.3 macOS dylib lists 142 `PyInit_` entries, such as `renpy_display_render`, `renpy_gl2_gl2draw`, and `renpy_pygame_display`. There are no separate extension modules to load.
- **Folder names by era.** Ren'Py 8 uses `py3-*` and `python3.x`. Ren'Py 7.4 to 7.8 use `py2-*` and `python2.7`. Ren'Py 7.3 and older use unprefixed names such as `linux-x86_64` (Test Game 30 has this layout even though its `renpy/__init__.py` says 7.4.2).
- **macOS binaries.** Only builds made with the Mac or PC/Linux/Mac package include them. In the library, 5 of 44 Ren'Py games ship macOS libraries and 3 ship a `.app` (from `docs/engine-survey.md`). Universal (arm64 plus x86_64) binaries start with Ren'Py 7.6.0 and 8.1.0, measured from each release's `sdk.update.json`. Older ones are x86_64 only. The Mac mini has no Rosetta (`arch -x86_64 /usr/bin/true` fails with "Bad CPU type"), so Test Game 30's own 7.4.2 `.app` cannot run there.
- **Versions.** Ren'Py 8.2 and later write `version = '8.5.3.26051504'` to `renpy/vc_version.py`. Older releases set `version_tuple = (7, 4, 2, vc_version)` in `renpy/__init__.py`. Ren'Py 7.5 to 7.8 ship one `renpy/` for both lines with two tuples, such as `(7, 5, 1, ...)` under `if PY2:` and `(8, 0, 1, ...)` otherwise. `EngineRenPy` reads these and picks the line from the `lib/` folder names.
- **`.rpyc` and `.rpa`.** `.rpyc` files are zlib-compressed pickles of the parsed script. `.rpa` files are archives the engine reads at run time. Both are read by the engine version that runs the game, not the one that built it. Ren'Py loads older `.rpyc` files (Test Game 30, built with 7.4.2, loads on 8.5.3), and it recompiles from `.rpy` when sources are present.

## Version spread

The user's library has 28 Ren'Py 8 games and 16 Ren'Py 7 or older games.

**A runtime runs only its own `renpy/` package.** The 7.8.7 macOS binary fails on the `renpy/` packages of 7.5.1 and 7.4.2 with `ImportError: No module named parsersupport`, because the set of compiled modules changes between releases. The same coupling holds on iOS, where the modules are compiled into the app. VNDegen therefore cannot reuse the game's own `renpy/` unless it ships that exact release's native build. What works is the approach Ren'Py's own SDK launcher uses: run the game's `game/` folder on a newer engine. `EngineRenPy` runs `<runtime>/renpy.py <game base>`, which sets the engine directory to the runtime and the game directory to the user's folder.

**A newer engine runs older games, with exceptions.** Ren'Py 8 rewrites Python 2 syntax when it compiles game code (`renpy/compat/fixes.py` handles print statements, `<>`, backticks, and old octal literals). Test Game 30 (7.4.2) runs on both 7.8.7 and 8.5.3. Test Game 54 (7.5.1) shows the two failure kinds:

- On 8.5.3 it fails inside its own bundled `requests` and `urllib3`, whose vendored `six` uses import hooks that Python 3.12 removed. That is a game-specific library problem, not a script problem.
- On 7.8.7 it fails with `SyntaxError: encoding declaration in Unicode string`. 7.8.7's loader compiles a game's `python-packages` modules from text, and Python 2 rejects a text module that declares an encoding. 7.5.1's loader compiled bytes. `scripts/renpy-runtime.sh` restores that in the bundled 7.8.7 with one line, after which Test Game 54 runs.

So VNDegen needs a small, growing set of compatibility patches in its bundled runtimes. Because VNDegen owns the runtime, these patches never touch the user's game folder.

**The library's Ren'Py 7 games on both runtimes.** 14 of the 16 Ren'Py 7 and older games were copied from the NAS for this test, plus the two test games already there. Four of the 14 are RAR archives that neither `bsdtar` nor Homebrew's 7-Zip could unpack ("Unsupported Method"), though their SHA-1 matched the NAS. So 12 games were tested: Test Game 50, Test Game 20, and two more were skipped. `docs/renpy-spike/spread-test.sh` booted each one on the patched 7.8.7 engine and on 8.5.3 with the probe script. A run counts as booting when Ren'Py raised no exception for 30 seconds and the 29-second screenshot shows the game's first interactive screen. That screen is a language picker, a disclaimer, a title screen, or, for Test Game 57, the story. This test shows that a game boots, not that it plays through to the end.

| Game | Own version | Patched 7.8.7 | 8.5.3 |
|---|---|---|---|
| Test Game 57 | 6.99.12 | Boots | Boots |
| Test Game 25 | 6.99.14 | Boots | Boots |
| Test Game 21 | 7.1.0 | Boots | Boots |
| Test Game 31 | 7.3.5 | Boots | Fails, vendored `six` |
| Test Game 17 | 7.3.5 | Boots | Fails, vendored `six` |
| Test Game 12 | 7.3.5 | Boots | Fails, vendored `six` |
| Test Game 34 | 7.3.5 | Boots | Fails, `TypeError` from Python 3 division |
| Test Game 27 | 7.3.5 | Boots | Boots |
| Test Game 24 | 7.3.5 | Boots | Boots |
| Test Game 30 | 7.4.2 | Boots | Boots |
| Test Game 18 | 7.4.11 | Boots | Boots |
| Test Game 54 | 7.5.1 | Boots | Fails, vendored `six` |

The patched 7.8.7 booted all 12, back to 6.99.12. Ren'Py 8.5.3 booted 7 of 12. Four of the five failures are the same `ModuleNotFoundError: No module named 'urllib3.packages.six.moves'` in games from one publisher that bundle the same old `requests`, so one shim in VNDegen's runtime (a `six` that works on Python 3.12) would probably fix all four (inferred, not tested). Test Game 34 fails at `for i in range(0, numCircles/3)` because `/` returns a float on Python 3. Ren'Py's syntax fixes cannot catch that, so it needs a per-game patch or the Python 2 runtime. Results are in `.audit/evidence/spread/`, with screenshots in `contact-ingame2.png` (tile order in `contact-ingame2-order.txt`).

**JoiPlay.** JoiPlay's Ren'Py support is separate Android plugins per runtime. Its July 2024 release lists a Ren'Py 7.1.3 plugin and a Ren'Py 8.2.3 plugin and added version detection for 6.99.14.3 and older ([JoiPlay 1.20.520](https://www.patreon.com/posts/107673676)). A 2023 plugin bundled 8.1.1 and 7.6.1 together ([JoiPlay 1.20.032](https://www.patreon.com/posts/85460458)). Users report that individual games needing an exact version, such as 8.0.3, fail on the newest plugin ([itch.io comment](https://itch.io/post/16652567)). JoiPlay's approach matches the one recommended here: a few runtimes chosen by detected version, not one per release.

## iOS

**Ren'Py's own iOS path.** renios builds one Xcode project per game and per Ren'Py version ([Ren'Py iOS docs](https://www.renpy.org/doc/html/ios.html)). The 8.5.3 renios package holds a prototype Xcode project, `Frameworks/MetalANGLE.xcframework`, and prebuilt static libraries: `prebuilt/debug` for the simulator (x86_64 and arm64, `LC_BUILD_VERSION` platform 7) and `prebuilt/release` for devices (arm64). They include `librenpython.a`, `librenpy.a` (the Cython modules), `libpython3.12.a`, SDL2, FFmpeg, and the codecs. The launcher code in `librenpython.a` (`launcher_main`, `renpython_main`) loads `<bundle>/base/main.py` with the standard library from `<bundle>/base/lib/python3.12`. `main.c` is three lines that hand `launcher_main` to `SDL_UIKitRunApp`.

**A single runtime that loads any game.** Nothing in the iOS runtime ties it to the bundled game. The spike generated a project for an empty host game with `renpy.sh launcher ios_create`, then added about 15 lines to `base/main.py` that set Ren'Py's base directory to the first game folder in `Documents/Games`. The bundled `renpy/` stays the engine and the user's folder becomes the game, as on macOS. The simulator build took 9 seconds and the app is 92 MB.

**Packaging native modules.** CPython 3.13 and later officially support iOS (PEP 730). Its packaging rule is that every binary module must be a framework in the app's `Frameworks` folder, found through `.fwork` marker files ([Using Python on iOS](https://docs.python.org/3/using/ios.html)). Ren'Py avoids the question: it uses its own static CPython 3.12 build and compiles every module into the app binary, so there are no `.so` files to package. The one framework is MetalANGLE. Games that bundle their own native Python modules in `game/python-packages` cannot work on iOS, and Ren'Py's own iOS builds do not support them either. Pure-Python packages work.

**The UIScene requirement.** Built with the iOS 27 SDK, the unmodified app fails to launch. The log says: "Application failed to launch: UIScene life cycle is required for apps built with this SDK." Apple announced this for the first major release after iOS 26 ([Apple forums](https://developer.apple.com/forums/thread/813300)). SDL added UIScene support in SDL 3.4.0 ([SDL issue 12680](https://github.com/libsdl-org/SDL/issues/12680)). Ren'Py 8.5 still uses SDL2, and its 8.5.0 notes say pygame_sdl2 was merged into Ren'Py "to support future development, such as a port to SDL3" ([Ren'Py changelog](https://www.renpy.org/doc/html/changelog.html)). The spike's shim adds a scene manifest to `Info.plist`, a scene delegate, and a swizzle of `-[UIWindow initWithFrame:]` that attaches each new window to the connected scene (`docs/renpy-spike/scene_shim.m`). That was enough to launch and play. When Ren'Py opened the keyboard for name input, SDL's view switched to portrait sizing (Ren'Py logged `physical=(402, 874)`) and stayed there after rotation. Inside VNDegen half of the shim goes away, because VNDegen already adopts scenes and SDL only needs to put its window on VNDegen's scene. The orientation problem still needs a fix in SDL's UIKit window code.

**Save location on iOS.** Ren'Py's iOS launcher ignores `RENPY_PATH_TO_SAVES` and saves straight into `Documents/` with no per-game folder. The simulator run wrote `persistent`, `sync/`, and `security_keys.txt` there. VNDegen's own `main.py` must replace `path_to_saves` with a per-game folder, or every game would share one save directory and clutter the Files app view.

## Embedding and returning to the hub

**macOS: a helper process.** `RenPySession` starts `<runtime>/lib/py3-mac-universal/python <runtime>/renpy.py <game>` with `RENPY_PATH_TO_SAVES` set to the game's data directory. The game opens its own SDL window. The hub's player screen shows a placeholder that says so. Quitting from the game's menu ends the process, and the termination handler calls `onExit`, which returns the hub to the library. Pause sends `SIGSTOP` and `SIGCONT`. A crash or `os._exit` in a game cannot take down the hub. Both Python lines coexist, since each game is its own process.

**iOS: one interpreter for the app's lifetime.** iOS has no helper processes, and an app cannot relaunch itself. CPython cannot be cleanly finalized and re-initialized in one process, and Ren'Py's statically linked modules make that riskier (a judgment, not tested). The options:

- Exit the app after each game. That throws the user to the home screen, which is a poor experience.
- Keep one interpreter alive and run each game inside it. Ren'Py already does something close for its "utter restart". `renpy/bootstrap.py` catches `UtterRestartException` and calls `renpy.reload_all()`, which "resets all modules to the state they were in right after import_all returned" in the same interpreter. Launching a second game is the same operation with a different base directory. This is recommended.

Under that design the hub calls a small C entry point on the main thread instead of `SDL_UIKitRunApp`. It calls `SDL_SetMainReady()`, initializes Python once, and runs VNDegen's `main.py` for the chosen game. Ren'Py's event loop blocks the main thread, but SDL's `UIKit_PumpEvents` runs the main run loop for each frame, so UIKit and SwiftUI should keep processing events (a guess from how SDL2 drives UIKit, not tested in the spike). When the game quits, `main.py` catches Ren'Py's quit, hides the SDL window, and returns to Swift, which shows the hub. The next launch reloads Ren'Py's Python modules and starts the next game.

**Hooks without touching games.** VNDegen owns the runtime, so it can add a file such as `renpy/common/zz_vndegen.rpy` that every game loads. That file can bind the backtick key and the controller's Home button to a VNDegen menu, report quit and pause to the hub, and fix per-game save paths. No game folder is modified.

## Rendering

- **iOS.** Ren'Py renders through MetalANGLE, ANGLE's Metal backend. The simulator log shows `Renderer: 'ANGLE (Metal Renderer: Apple iOS simulator GPU)'` and `Version: 'OpenGL ES 3.0.0 (ANGLE 2.1.0.850c87ba5b74)'`. This meets the project's hardware acceleration and Metal rule. MetalANGLE's README has said since 2023 that the repository is "not in active development" and that ANGLE work belongs in upstream ANGLE, which has its own Metal backend ([MetalANGLE](https://github.com/kakashidinho/metalangle)). A future Ren'Py may switch to it.
- **macOS.** Ren'Py's gl2 renderer uses Apple's OpenGL, which Apple implements on Metal. The log shows `Vendor: 'Apple'`, `Renderer: 'Apple M4'`, `Version: '2.1 Metal - 91.7'`. That is hardware accelerated and Metal underneath, but it depends on a deprecated API. Switching macOS to ANGLE needs a custom Ren'Py build.
- **Frame rate.** The probe counted entries in Ren'Py's `interface.frame_times`, one per drawn frame, while forcing a redraw every frame. On the M4 at 1920 by 1080, Test Game 15 drew 427 to 527 frames per second and Test Game 30 484 to 539. Ren'Py asked for a swap interval of 1, so this rate is above the display's 60 Hz. The window was most likely not throttled to vsync because no one was looking at it on the headless Mac mini (inferred). The number shows the GPU headroom for a typical visual novel scene, about 2 ms per frame, and not the frame pacing a player sees. iOS frame rate was not measured, because the simulator's GPU does not represent a device.

## Saves

| Where | Path |
|---|---|
| Windows | `%APPDATA%/RenPy/<save_directory>/` |
| Linux and Steam Deck | `~/.renpy/<save_directory>/` |
| macOS | `~/Library/RenPy/<save_directory>/` |
| Any platform, also | `<base>/game/saves/` when the folder is writable |
| `RENPY_PATH_TO_SAVES` set (desktop) | `$RENPY_PATH_TO_SAVES/<save_directory>/` |
| iOS launcher default | the app's `Documents/` |

These come from `path_to_saves` in the launcher script that every game ships (such as `<Name>.py`). Ren'Py loads saves from both the user folder and `game/saves`. In the Ludusavi store on the NAS, the two Ren'Py games with backups record paths such as `/home/deck/.renpy/Game-1505403882/1-1-LT1.save`.

`RenPyPlugin.saveLocations` returns two locations:

- `<data directory>/saves`. The session points `RENPY_PATH_TO_SAVES` there. The rule maps any path with a `RenPy` or `.renpy` folder, such as the Windows, macOS, and Linux paths above, to `<save_directory>/<file>`, so it never needs to know the game's `config.save_directory`. Local paths inside the folder map to themselves.
- `<base>/game/saves`, mapping `.../game/saves/<file>` to `<file>`.

The spike's macOS app run wrote `GameData/Test Game 15/saves/Game-1791450490/persistent`, which matches the rule. Phase 1 changed this layout; see Phase 1 on macOS below. Ren'Py 8.5.3 also keeps `tokens/security_keys.txt` beside the save folders (`renpy/savetoken.py`) and asks before loading a save it has no key for: "This save was created on a different device." Ludusavi does not back up that file, so a restored PC save shows Ren'Py's warning once, and the player must accept it.

## Phase 1 on macOS

Units 1 to 4 and the macOS save locations are built. Where they differ from the plan above:

- **Detection.** A folder, or a folder one or two levels inside it, is a Ren'Py game when it holds `renpy/` and a `game/` folder with a `.rpy`, `.rpyc`, or `.rpa` file at any depth, such as `game/scripts/act1/scenes/`. The scan lists `game/` one level at a time, stops at the first script, and skips symlinked folders. Only folders holding a `renpy` entry are candidates, shallowest first, and at most 16 of them are tried. The `game/` walks of folders two levels down stop after 20,000 entries in all.
- **Runtimes.** The macOS build phase in `project.yml` runs `scripts/renpy-runtime.sh build/renpy`, copies both runtimes into `Contents/Resources/RenPy`, and signs each `python` and `librenpython.dylib` with the app's identity. The SDK tarballs are checked against SHA-256 sums in the script and cached in `~/Library/Caches/VNDegen/renpy-sdk`. Nothing is installed into `~/Library/Application Support`.
- **Launcher.** The helper runs VNDegen's `Support/launcher.py` on the runtime's `python`. It runs the runtime's `renpy.py` in memory with three changes: `path_to_saves` points at `<data directory>/saves` (not `RENPY_PATH_TO_SAVES`), Ren'Py's read-only mode (`renpy.macapp`) is on so nothing is compiled or written into the game folder, and the folder holding `zz_vndegen.rpy` joins the search path. Ren'Py's log goes to `<data directory>/logs/renpy.txt`.
- **Saves.** Saves sit directly in `<data directory>/saves`, without the `config.save_directory` level, so `saveLocations` returns one location. It maps Windows, macOS, and Linux user-folder paths and `game/saves` paths into it, and rejects any path with a `.` or `..` component. A first backup records saves as `/VNDegen/<game>/saves/<file>`, which every VNDegen install maps back into its own save folder, and older backups that recorded another install's `GameData/<game>/saves/` path map the same way. A game copied in with saves in `game/saves/` gets them copied into an empty save folder on its first launch. The copy is staged and checked before it replaces the empty folder, and a failed copy stops the launch with an error, so the next launch tries again. A `game/saves` or save folder that can't be listed fails the launch the same way, instead of passing for an empty one. A Ludusavi restore or Undo refuses to write to a path that a symlink already in the save folder leads outside it.
- **Window.** The game window has no title bar and takes the hub window's frame, or opens full screen when the hub is full screen. The runtimes are built for older macOS releases and draw a shorter title bar than the hub, so a content rect computed in the hub came out 4 points short.
- **Quitting.** Quitting from the hub's menu or quitting VNDegen sends the game SIGTERM. `zz_vndegen.rpy` answers it the way Ren'Py's Quit button does once the player confirms, so the game autosaves and writes its persistent data, then exits. A game still running 10 seconds later is killed. The hub's automatic backup waits for the game to exit. Until the game has stopped and that backup is done, the hub refuses to launch the game again, and a stuck backup blocks the relaunch for at most 20 seconds after the game stopped. Restore, Undo, and Back up wait for the game to stop and run after the automatic backup.
- **Controller.** Options and Home (SDL's back and guide) open the VNDegen menu, as the backtick does. The game's button mapping from the hub is passed in `VNDEGEN_BUTTONS` and applied inside Ren'Py: a button mapped to a gamepad action gets Ren'Py's bindings for the standard button with that action, and one mapped to a key or click posts it. At text prompts the backtick opens the menu instead of being typed.
- **Compatibility.** `scripts/renpy-compat.sh` and the `RenPyCompat` tool replace `spread-test.sh`. They boot each game the way the app does, with a probe in `zz_vndegen.rpy` that presses through first-run language pickers and disclaimers, and report the runtime, the reason it was picked, whether the main menu showed, and whether the game folder changed. Each run deletes its output folder first, so the tool refuses an `--out` that is, holds, or sits inside a game folder, and a run folder that leaves `--out` or overlaps a game folder once symlinks are resolved. The launcher adds an import finder for Python 3.12 that asks old `find_module` finders, which fixes the vendored `six` failure: Test Game 54 reaches its main menu on Ren'Py 8 with it and fails with `ModuleNotFoundError: No module named 'urllib3.packages.six.moves'` without it. `scripts/renpy-hook-test.sh` runs a small project on both runtimes to check the SIGTERM quit and the remapped stick.
- **Runtime override.** `RenPyPlugin.setRuntimeOverride` stores a per-game runtime in `<data directory>/renpy.json`. The game page's Ren'Py runtime option, an advanced option on macOS only, sets it.

## Phase 2 on iOS

Built on branch `ws/renpy-ios`. Units 5 to 9 of the plan below, with these results and changes:

- **Linking.** `scripts/renpy-runtime.sh build/renpy ios` downloads renios 8.5.3, checks its SHA-256, merges the prebuilt static libraries renios's own Xcode project links into one library per platform, and makes `RenPyRuntime.xcframework`. The app links it and embeds `MetalANGLE.framework` on iOS only. `scripts/build.sh` runs the script before an iOS build, because Xcode needs the xcframework when it plans the build. The Python side is the Ren'Py 8 runtime the Mac app already bundles, minus its macOS binaries, so the app grows by the libraries and about 32 MB of Python files.
- **One interpreter.** `Sources/CRenPy` starts Python once and runs `launcher.py` for each game. The game runs on the main thread from a run loop block, not a main-queue block, because Ren'Py's event loop runs the main run loop, and inside a main-queue block the main queue would stall for as long as the game runs. On iOS `launcher.py` runs the game the way `renpy.bootstrap.bootstrap()` does, except that the game returns instead of exiting. The next game starts from `renpy.reload_all()`, as an utter restart does, after VNDegen drops the modules the last game brought along in its own folder. A Ren'Py 7 game followed by a Ren'Py 8 game, then three more launches, all ran in one process.
- **Window.** SDL creates its own `UIWindow`, which has no scene and never shows. VNDegen moves SDL's view into the player's view after each display mode change, so the hub's lock screen, orientation lock, and menu button stay over the game. The UIScene shim from the spike is not needed. The portrait layout after the keyboard opened did not recur. The game stays landscape, and SDL moves its view up so a name prompt stays above the keyboard. When iOS takes its background snapshots it lays the window out in portrait, so the player resets the view's size afterwards.
- **What Ren'Py expects of its app.** renios's Xcode project defines the Objective-C classes `Log` and `IAPHelper`, which Ren'Py looks up at startup. VNDegen defines both: `Log` writes to the game's log, and `IAPHelper` reports that payments are off. SDL hears about the app going to the background from VNDegen instead of its own app delegate, and Ren'Py saves and stops drawing there as it does in its own iOS builds. Ren'Py's log objects hold the standard output on iOS, and its module backup pickles them, so VNDegen's standard streams pickle as a call that reopens them.
- **Input and menu.** The game reads touches, the keyboard, and controllers through SDL, as the Mac helper does. The VNDegen menu is the one in `zz_vndegen.rpy`, opened by the backtick, the controller's Options and Home buttons, or the player's corner button, which the session passes on as a request. The hub sends the game no controller input of its own (`EngineSession.drawsOwnMenu`). SDL takes over the keyboard handler and returns the Home button to the system when a game quits, so the hub takes both back afterwards. Quitting from the hub, such as Cancel on the lock screen, asks the game to quit the way its Quit button does. The lock screen pauses the game until it is unlocked.
- **Saves.** The same per-game save folder as macOS, through `launcher.py`'s `path_to_saves`, so Ludusavi save locations are unchanged.

Checked on the iPhone 18 Pro simulator in landscape: Test Game 15 (Ren'Py 8.5.3) and Test Game 30 (Ren'Py 7.4.2) reached their main menus, played by touch, saved to a slot, quit to the library, and loaded the slot after a relaunch. Test Game 54 (7.5.1, the vendored `six` case) and a Ren'Py 7.3.5 game that needs Python 2 division reached their main menus. On the iPad Pro 13-inch simulator Test Game 15 ran in both orientations with the name prompt above the keyboard, and Test Game 30 reached its main menu as the third game in one process.

**Ren'Py 7 games on Ren'Py 8.** `scripts/renpy-compat.sh <folder> --runtime 8` booted the library's 16 Ren'Py 7 and 6.99 games on the macOS Ren'Py 8 runtime, which runs the same Ren'Py and launcher. 14 reached their main menu or, for three whose title screen is a choice menu, their story. One more did after VNDegen started dividing integers the Python 2 way in a Ren'Py 7 game's own code (`launcher.py`'s `use_python2_division`). The last game's achievements read a Steam module from the modified copy of Ren'Py it ships, which VNDegen does not load. `RenPyCompatibility.tested` lists the results by `RenPyLayout.fingerprint`, a hash of the names and sizes of the scripts and archives in `game/`, so the list names no game. All 16 are in the list. Detection finds games unpacked two folders deep since 0.4.1, and the fingerprint reads only `game/`, so the list also matches a game laid out that way. Each run on a device records whether the game reached its main menu or which error stopped it (`RenPyBootRecord`), and the game page shows that, else the library test's result.

**Saves of Ren'Py 7 games on Ren'Py 8.** Tested in each game's own PC engine, run headless:

| Written by | Read by | Save | Persistent data |
|---|---|---|---|
| Ren'Py 8.5.3 | Test Game 30's Ren'Py 7.4.2 | fails, pickle protocol 5 | fails, pickle protocol 5 |
| Ren'Py 8.5.3 with VNDegen's Python 2 pickling | Test Game 30's Ren'Py 7.4.2 | unpickles, but loading fails on Ren'Py 8's rollback state | loads |
| Ren'Py 7.8.7, the Mac runtime | Test Game 30's Ren'Py 7.4.2 | fails, no `renpy.revertable` module | fails, same |
| Ren'Py 7.8.7, the Mac runtime | Test Game 54's Ren'Py 7.5.1 | fails, no `renpy.display.scenelists` module | loads |
| Ren'Py 8.5.3 | Test Game 54's Ren'Py 7.5.1 | fails, pickle protocol 5 | fails, pickle protocol 5 |

Older Ren'Py can't load saves from a newer one, which also holds for macOS, where Ren'Py 7 games run on 7.8.7. So a game's Saves page warns before a Ludusavi backup when the game's own Ren'Py is older than the runtime that wrote its saves. A Ren'Py 7 game on Ren'Py 8 writes its persistent data in pickle protocol 2 with the Ren'Py 7.4 module names, which its PC version loads, so unlocks carry over.

## Licenses

Ren'Py is MIT, with parts derived from LGPL code. Its `LICENSE.txt` (8.5.3 SDK) lists what its binaries can include:

- **MIT or similar.** Ren'Py, brotli, ecdsa, future, libffi, pefile, py2exe, pyjnius, pyobjc (pyobjus), six, urllib3, charset-normalizer.
- **BSD style.** ANGLE, MetalANGLE, aom (with the AOM patent license), libavif, libyuv, libwebp (with a patent grant), assimp, GLEW, idna, pyasn1, pysocks, websockets, xz.
- **zlib and similar.** SDL2, SDL2_image, SDL2_ttf, zlib, FreeType (Ren'Py lists zlib; FreeType's own terms are the FreeType License or GPLv2, at the user's choice), libjpeg-turbo (IJG, BSD, zlib), libpng, tinyfiledialogs, libhydrogen (ISC), bzip2.
- **Python.** CPython (PSF License).
- **Apache 2.0.** requests, rsa.
- **MPL 2.0.** certifi.
- **LGPL.** FFmpeg, FriBidi, chardet, libusb, and parts of pygame_sdl2.
- **Artistic.** zsync, which is only the desktop updater and is not needed.

None is GPL, so unlike mkxp-z or ONScripter, Ren'Py does not force the GPL on VNDegen. Ren'Py's own guidance is to ship its license text and link to its source repositories, and its `LICENSE.txt` names them: `github.com/renpy/renpy` for Ren'Py and `github.com/renpy/renpy-build` for the libraries.

On iOS VNDegen links FFmpeg, FriBidi, and parts of pygame_sdl2 statically, and LGPL 2.1 section 6 then asks that users can relink the app with a modified version of those libraries. VNDegen meets that by being open source: its source and build scripts are public, and `scripts/renpy-runtime.sh` builds the app against renios's libraries, which anyone can replace with their own build from renpy-build. Each bundled runtime, macOS and iOS alike, carries Ren'Py's `LICENSE.txt`, which holds the MIT, BSD, zlib, and LGPL texts. A build distributed without its source, such as through an app store, would need another way to offer relinking, such as the app's object files. This is a reading of the licenses, not legal advice.

## Recommended architecture

**macOS.** Keep the spike's design. Bundle two trimmed official runtimes in the app (`Contents/Resources/RenPy/8` from 8.5.3 at 100 MB, and `RenPy/7` from 7.8.7 at 74 MB, both universal), plus VNDegen's compatibility patches and `zz_vndegen.rpy`. Run each game as a helper process on the runtime for its Python line. Make the game window borderless and full screen over the hub, or the hub's window frame, and give the hub's backtick menu to the game through the `zz_vndegen.rpy` hook. Saves go to the game's data directory through `RENPY_PATH_TO_SAVES`.

**iOS.** One Ren'Py 8 runtime linked into VNDegen from renios's static libraries, with the interpreter started once and kept for the app's lifetime. Each launch runs VNDegen's `main.py`, which points Ren'Py at the game folder and a per-game save folder. Leaving a game hides SDL's window and returns to the hub, and the next game goes through Ren'Py's module reload. Fix SDL2's window and scene handling with a small patch until Ren'Py moves to SDL 3.4 or later. Ren'Py 7 games run on the Ren'Py 8 runtime when they can. Linking a second CPython (renios 7.8.7 exists, 92 MB) needs every symbol in one of the two builds renamed, because both define `Py_*`, SDL, and FFmpeg. The spread test says that work is not worth it yet: 7 of 12 Ren'Py 7 games already boot on Ren'Py 8, a `six` shim likely covers 4 more, and the rest can stay macOS only until a pattern shows up.

## Remaining work

Each unit ends in a check on the real app. The estimates are rough guesses.

1. **Bundle the macOS runtimes.** Copy them into the app with an XcodeGen build phase, sign `librenpython.dylib`, and keep `scripts/renpy-runtime.sh` as the fetch step. About 1 day.
2. **`zz_vndegen.rpy` hook.** Backtick and Home open a VNDegen menu (resume, quit to library), quit reports back, and saves go to the per-game folder. Shared by both platforms. About 2 days.
3. **macOS window integration.** Full screen or hub-sized window, activation, and hiding the hub while the game runs. About 1 day.
4. **Compatibility patches and a regression run.** Turn `docs/renpy-spike/spread-test.sh` into a committed script that boots every Ren'Py game in a folder and reports failures, then patch the common ones, such as the `six` import hook for Python 3.12. 2 to 4 days, then ongoing.
5. **iOS runtime link.** Add renios's static libraries and MetalANGLE to the VNDegen target (as an xcframework or a binary target), plus `base/` (engine `.pyc` and stdlib, 26 MB) as a resource. About 2 days.
6. **iOS in-process session.** C entry point with `SDL_SetMainReady`, one Python initialization, launching a game, returning to the hub on quit, and the reload path for the next game. 3 to 5 days, and the riskiest unit.
7. **SDL2 scene and orientation fix on iOS.** Attach SDL's window to VNDegen's scene, and fix the portrait resize after the keyboard opens. Orientation follows the hub's per-game setting. 2 to 3 days.
8. **Input on iOS.** Map VNDegen's touch overlay and controller events to SDL events. Ren'Py already handles touch, and SDL already handles GameController. About 2 days.
9. **Ren'Py 7 on iOS, only if unit 4's results call for it.** A second runtime with renamed symbols, about 1 to 2 weeks. Otherwise mark failing Ren'Py 7 games as macOS only.
10. **Runtime updates.** A script that rebuilds the bundles when Ren'Py releases, and reruns unit 4's regression set. About 1 day.

## Risks

- **Apple's SDK moves faster than Ren'Py.** The UIScene requirement already broke renios on the iOS 27 SDK. Each new SDK can break SDL2 again until Ren'Py moves to SDL3.
- **Keeping one interpreter alive on iOS is unproven.** Reloading Ren'Py in-process is what Ren'Py does for its utter restart, but a game can leave threads, global state, or a corrupted interpreter behind. A crash in one game takes down the hub on iOS.
- **Game-specific Python breaks on a newer runtime.** Test Game 54's bundled `requests` is one example. Each fix lives in VNDegen's runtime, and some games will need per-game fixes.
- **Games that patch the engine.** Some games may ship a modified `renpy/` or depend on one exact release (a guess, not checked). Running them on the bundled engine drops the changes. The fallback is a matching runtime, which macOS can do with the game's own macOS libraries when they exist, and iOS cannot.
- **Deprecated OpenGL on macOS.** Ren'Py on macOS uses Apple's OpenGL, which Apple deprecated. If Apple removes it, macOS needs an ANGLE build.
- **App size.** The two macOS runtimes add about 175 MB. The whole iOS simulator host app, with the runtime and its 26 MB `base/`, is 92 MB, so VNDegen would grow by roughly that much (inferred, device build not measured). Both can shrink by trimming the standard library and unused codecs.
- **Save tokens.** Restored PC saves trigger Ren'Py's "made on another device" warning once.

## Reproducing

- `scripts/renpy-runtime.sh build/renpy` on a Mac installs the two runtimes into `build/renpy`, checking each SDK against a SHA-256 in the script. The macOS build runs it.
- `scripts/build.sh mac`, then run the app with `-autolaunch "<game folder>"`. With a DEBUG build, `-libraryRoot <path>` keeps the run out of `~/Documents/VNDegen`.
- The iOS host app is not in the repository. It is the renios 8.5.3 prototype generated with `renpy.sh launcher ios_create <empty project> <dest>`, with `docs/renpy-spike/vndegen_main_prefix.py` inserted into `base/main.py` before `if __name__ == "__main__":`, `docs/renpy-spike/scene_shim.m` appended to `Log.m`, and a `UIApplicationSceneManifest` naming `VNDSceneDelegate` in `Info.plist`. Build it with `xcodebuild -target vndhost -configuration Debug -sdk iphonesimulator ARCHS=arm64 CODE_SIGNING_ALLOWED=NO`.

Test games were copied from the NAS read only and extracted on the Mac mini under `~/vndegen-testgames/renpy/`.
