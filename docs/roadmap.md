# Roadmap

Agreed with the maintainer on 2026-10-25. Engine order comes from `docs/engine-survey.md`. Release notes are in `docs/releases/`.

## Shipped
- RPG Maker MV/MZ on Mac, iPhone, and iPad (0.1.0).
- Archive import from zip, RAR, and 7z (0.1.0, fixes through 0.1.4).
- Ludusavi backup and restore through a folder picked in the app (0.1.0), replaced in 0.4.0.
- TyranoScript (0.2.0, fixes through 0.3.2).
- Control methods and button mapping (0.2.1).
- Ren'Py on macOS (0.3.0, fixes through 0.3.2).
- Ren'Py on iPhone and iPad (0.4.0). What was built is in "Phase 2 on iOS" in `docs/renpy-spike.md`.
- Ludusavi built into the app (0.4.0). VNDegen connects to the backup server itself over SSH with rsync, matches games through Ludusavi's manifest, and builds in Ludusavi's backup features.
- Touch and Cursor mouse modes on iPhone and iPad, and the controller's Mouse option (0.4.0).
- Fixes for Touch mode and for quitting on a landscape iPhone, Ren'Py games two folders deep, and no control method for Ren'Py on iPhone and iPad (0.4.1).
- Xbox gamepad controls by default for the touch gamepad and real controllers, every Xbox button in the mapping editor with an RPG Maker preset, touch buttons that press on touch-down, a cross-shaped D-pad, and the iPad tilt-shift blur fix (0.4.2).
- Ludusavi save sync over SMB, with Ludusavi's SMB settings and rclone's own code built into the app, replacing SSH and rsync (0.4.3).
- A fix for RPG Maker MZ's Ogg decoder reusing IDs, which stopped games with "RangeError: length cannot be negative" (0.4.4).
- RPG Maker XP, VX, and VX Ace on Mac, iPhone, and iPad, built on mkxp-z's libretro core in the app, Apple silicon only on Mac, with the RTPs downloaded and installed in the app (0.5.0). `docs/rgss-spike.md` has the feasibility test and what was built.
- License notices in Settings, Licenses, for every third-party component in the app (0.5.0).
- The real fix for RPG Maker MZ's "RangeError: length cannot be negative": sounds with Ogg names that hold WAV or MP3 data play through the system decoder, and a decoder failure no longer stops the game (0.5.1).
- KiriKiri 2 and KiriKiri Z on Mac, iPhone, and iPad, built on krkrsdl3, with the common ciphers, Windows plugins, and movie formats built in (0.6.0).

## In progress
Each item is planned work on its own branch, not merged into `dev`.
- RPG Maker MZ fixes: a cutscene whose animation stutters and drifts from its sound, and timed battle attacks that miss quick presses.
- Controller mouse emulation: sticks and buttons can move the pointer, scroll, and open the on-screen keyboard, and engines without gamepad support default to a "Keyboard and mouse" layout copied from Steam's template, with a "Mouse" preset to pick instead.

## Next
1. Together, as the next work:
   - NScripter (ONScripter).
   - iPad multitasking: Split View, Slide Over, and an iPad window of its own. A windowed game follows the window's shape, as Follow device does, whatever in-game orientation Settings sets. The hub, the in-game menu, the touch gamepad, and every engine lay out correctly at small and odd window sizes. The iPhone has no split screen, so it needs no change.
2. A controller layout library, like Steam's controller layouts. Each game uses a layout from the library, and the player can pick another, edit it, or save their own. The library starts with VNDegen's built-in layouts, among them "Keyboard and mouse", copied from Steam's template of that name, which engines without gamepad support use by default unless an engine has a better standard layout. It then takes in suggested layouts: on a game's first start, VNDegen reads the game's files for the controls it uses, such as its key bindings and whether it relies on the mouse, and proposes a layout that maps those keys, the mouse pointer, and mouse buttons to the controller, which the player can accept or change. Suggestions may be hard and costly, so they come after the library itself.
3. AliceSoft System 3 and 4 (xsystem35, xsystem4).
4. Artemis.
5. Wolf RPG Editor, which needs a new runtime.
6. Flash, through Ruffle (MIT or Apache-2.0) in the web view the RPG Maker MV/MZ and TyranoScript plugins already use. 14 library games.
7. Godot, on the Godot runtime matching each game's version. A game's `.pck` loads only if it uses no native GDExtension libraries. 6 library games.
8. NekoSDK. No runtime or reimplementation exists, so this is a reimplementation from the game files, starting with a feasibility test. 16 library games, 45 GB.
9. Unity. Before any work, decide with the maintainer whether it is feasible and how to proceed, including converting games to run natively on ARM. IL2CPP games are compiled x86 code. Mono games carry their game code as portable .NET assemblies, but run on Unity's x86 Windows player. 60 library games: 44 Mono, 10 IL2CPP, and 6 not yet checked.
10. After the engine plugins: installing a game from its installer. VNDegen would extract the game files from the installer into a folder an engine plugin can run, rather than running the installer. The approach is to be agreed with the maintainer when this starts.
11. Cleaning up a game's engine files. After a game is added, VNDegen offers to remove the bundled Windows engine files the game no longer needs to play in VNDegen, such as NW.js or Electron runtimes, Ren'Py's Windows and Linux libraries, and engine `.exe` and `.dll` files. Each engine plugin lists exactly which files it may remove. VNDegen shows the files and the space saved, and removes them only after the user confirms. Saves and the game's own data are never touched.
12. Last: save sync through every cloud type Ludusavi offers (Box, Dropbox, Google Drive, OneDrive, FTP, SMB, WebDAV, and a custom rclone remote), with the same settings as Ludusavi.

## Runtime packages
When a game needs a runtime package that VNDegen doesn't bundle, such as the RPG Maker RTP, the app offers to download it from its official source and install it, in addition to importing it from a folder. The `RuntimePackages` module does this: an engine describes each package with its official URL and pinned SHA-256 (`RuntimePackage` in VNCore), and the module downloads, checks, and unpacks it, with an Inno Setup reader for installers like the RPG Maker RTPs'. The RPG Maker 2000 and 2003 RTPs can use it the same way.

## Out of scope for now
- Apple TV.
- iCloud saves.
- GameMaker and the other general-purpose and custom engines in `docs/engine-survey.md` that are not listed under Next.
