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
- Controller mouse emulation (0.6.1): sticks and buttons can move the pointer, turn the mouse wheel, and open the on-screen keyboard. KiriKiri and TyranoScript games start with "Keyboard and mouse", the layout of Steam's template, and the mapping editor also offers "Mouse", "RPG Maker controls", and "Xbox gamepad".
- An RPG Maker MZ fix (0.6.1): looping WebM movies made from GIFs, which WebKit takes for shorter than they are, now play at their stated length, so they no longer stutter or drift from their sound. Movie frames are uploaded only when they change, and a clip's first pass no longer pauses.
- An RPG Maker MZ fix (0.6.1): quick controller, touch gamepad, and key presses all reach the game. Each press and release reaches the game's input on its own update, and a VisuStella Core Engine input clear no longer drops the presses that follow it, so timed attacks that need several quick presses succeed.
- The whole app works with a controller alone (0.6.2): every screen, Settings page, dialog and sheet takes controller focus, text fields open an on-screen keyboard the controller types on, the mapping editor has Reset and LB and RB group jumps, and import lists archives in VNDegen's folder. The Files and Photos pickers say they need touch or a mouse. Settings rows on the Mac show controller focus.
- iPad windows (0.7.0): VNDegen fits small and odd window sizes on iPad. The library, game pages, the in-game menu, the touch gamepad, and every engine fit the window and keep clear of iPadOS's window controls. A game in a window follows the window's shape. Checked by laying the app out at sizes down to 320 points wide and in real windows on the iPad simulator. Real drag-resizing, tiling, and Slide Over couldn't be driven in the simulator.
- Game files found whatever case the game names them in, for every engine (0.7.0). iPhone and iPad disks tell `Img/Title.PNG` from `img/title.png`, and Windows games often mix the two. Every engine plugin must pass the case check in the README's "Adding an engine" section.
- Flash (0.7.0): Flash projectors, `.swf` movies, and HTML launcher pages run in Ruffle in VNDegen's web view, with ActionScript 1, 2, and 3, saves in Flash's own `.sol` files, and every input method. All 14 library Flash games reach their title and play on the Mac and the iPad simulator, and the 12 that save also save and load.
- Windows fonts (0.7.0): games that ask for MS Gothic, MS PGothic, MS Mincho, Arial, Tahoma and other Windows fonts by name get free fonts with the same metrics, as under Proton, in RPG Maker MV and MZ, TyranoScript, KiriKiri, RPG Maker XP, VX and VX Ace, and Flash. One table of font name substitutions serves every engine. Chinese and Korean substitutes are a download in Settings.

## In progress
Each item is work on its own branch, not merged into `dev`. New engine plugins are built and checked on Linux first, then on the iPad simulator.
- NScripter, on VNDegen's own clean-room runtime (item 2).
- Godot (item 8), with Godot 3.6.3 for 3.x games and 4.7.2 for 4.x games. Two library games can't be supported. One needs Steam ownership, and one has an encrypted pack and Windows-only GDExtensions.
- Artemis (item 5), on art3m1s-core (MPL-2.0) with VNDegen's patches. 7 of 8 test games reach their title screen on Linux, and a smoke test on the iPad simulator passed. Saves that load in the PC version are in progress.
- Paused, to resume later: Wolf RPG Editor (item 6), whose feasibility test is done, with all 3 test games playing past their title screen on Linux. The clean-room runtimes for RGSS and for AliceSoft System 3 and System 4 (item 4).

## Licenses
VNDegen should stay free to distribute and may be sold on the App Store later. Builds that include mkxp-z, the RGSS runtime, are GPL-3.0 as a whole, as the README's License section explains. To change that:
- VNDegen's own MIT runtimes replace the GPL ones. The clean-room RGSS runtime replaces mkxp-z once it runs the same test games. NScripter and AliceSoft System 3 and System 4 are clean-room from the start, so VNDegen never ships ONScripter, xsystem35, or xsystem4. The clean-room runtimes are written from the game files and the engines' own documentation, never from GPL code. The original engines run under Wine only to settle open questions, such as the PC save format. With no GPL runtime left, the app is MIT for its own code plus permissive notices.
- New engines use only permissive licenses: MIT, BSD, zlib, Apache-2.0, ISC, MPL-2.0, or public domain, and OFL-1.1 for fonts.
- The LGPL libraries linked statically today are item 13.

## Next
1. A frame rate check of every engine plugin, limited to the games where it matters: games that aren't strictly visual novels, such as RPG Maker games, and visual novels with video, such as movies or animated clips. Text, still images and transitions don't need it. Each of those test games should reach the frame rate it targets, such as 60 fps for RPG Maker MV and MZ, and hold it on average through normal play, including scenes with heavy effects. Where a game falls short, find out whether VNDegen or the engine plugin causes it, or something on the test machine, and fix what is VNDegen's. Every new engine plugin gets the same check, on the same kinds of games, before it ships.
2. NScripter, on VNDegen's own clean-room runtime.
3. A controller layout library, like Steam's controller layouts. Each game uses a layout from the library, and the player can pick another, edit it, or save their own. The library starts with VNDegen's built-in layouts, among them "Keyboard and mouse", copied from Steam's template of that name, which engines without gamepad support use by default unless an engine has a better standard layout. It then takes in suggested layouts: on a game's first start, VNDegen reads the game's files for the controls it uses, such as its key bindings and whether it relies on the mouse, and proposes a layout that maps those keys, the mouse pointer, and mouse buttons to the controller, which the player can accept or change. Suggestions may be hard and costly, so they come after the library itself.
4. AliceSoft System 3 and System 4, on VNDegen's own clean-room runtimes, including the oldest `SYSTEM3.EXE` games.
5. Artemis, on art3m1s-core (MPL-2.0).
6. Wolf RPG Editor, which needs a new runtime.
7. Flash, through Ruffle (MIT or Apache-2.0) in the web view the RPG Maker MV/MZ and TyranoScript plugins already use. 14 library games. Shipped in 0.7.0.
8. Godot, on Godot 3.6.3 for 3.x games and 4.7.2 for 4.x games. A game's `.pck` loads only if it uses no native GDExtension libraries. 6 library games.
9. NekoSDK, parked for later. No runtime or reimplementation exists, so this is a reimplementation from the game files. A feasibility test on branch `ws/nekosdk` (`docs/nekosdk.md` there) found that every game's archives and story scripts parse, but each game's title, save, config and gallery screens, and in 6 games the story order, are compiled into its exe. It estimated 10 to 14 weeks for the 10 games whose story order is data, with VNDegen-style menus. 16 library games, 45 GB.
10. Unity. Before any work, decide with the maintainer whether it is feasible and how to proceed, including converting games to run natively on ARM. IL2CPP games are compiled x86 code. Mono games carry their game code as portable .NET assemblies, but run on Unity's x86 Windows player. 60 library games: 44 Mono, 10 IL2CPP, and 6 not yet checked.
11. After the engine plugins: installing a game from its installer. VNDegen would extract the game files from the installer into a folder an engine plugin can run, rather than running the installer. The approach is to be agreed with the maintainer when this starts.
12. Cleaning up a game's engine files. After a game is added, VNDegen offers to remove the bundled Windows engine files the game no longer needs to play in VNDegen, such as NW.js or Electron runtimes, Ren'Py's Windows and Linux libraries, and engine `.exe` and `.dll` files. Each engine plugin lists exactly which files it may remove. VNDegen shows the files and the space saved, and removes them only after the user confirms. Saves and the game's own data are never touched.
13. Before any App Store release: the LGPL libraries. Ren'Py's FFmpeg, FriBidi and parts of pygame_sdl2, and KiriKiri's FFmpeg, are linked statically today, which LGPL allows only while users can relink them. Either ship them as separate frameworks that can be replaced, as Ren'Py's own App Store games do, or replace them with Apple's frameworks and permissive code. Decide with the maintainer when an App Store release is planned.
14. Last: save sync through every cloud type Ludusavi offers (Box, Dropbox, Google Drive, OneDrive, FTP, SMB, WebDAV, and a custom rclone remote), with the same settings as Ludusavi.

## Runtime packages
When a game needs a runtime package that VNDegen doesn't bundle, such as the RPG Maker RTP, the app offers to download it from its official source and install it, in addition to importing it from a folder. The `RuntimePackages` module does this: an engine describes each package with its official URL and pinned SHA-256 (`RuntimePackage` in VNCore), and the module downloads, checks, and unpacks it, with an Inno Setup reader for installers like the RPG Maker RTPs'. The RPG Maker 2000 and 2003 RTPs can use it the same way.

## Out of scope for now
- Apple TV.
- iCloud saves.
- GameMaker and the other general-purpose and custom engines in `docs/engine-survey.md` that are not listed under Next.
