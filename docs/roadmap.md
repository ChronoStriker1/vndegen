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

## In progress
Each item is planned work on its own branch, not merged into `dev`.
- KiriKiri (`ws/kirikiri`) on Mac, iPhone, and iPad, built on krkrsdl3.
- RPG Maker XP, VX, and VX Ace (`ws/rgss`), built on mkxp-z's libretro core in the app on all three platforms, Apple silicon only on Mac. The feasibility test is written up in `docs/rgss-spike.md` on that branch.
- Fixes for Touch mouse mode on iPhone (`ws/touch-mode-fix`).
- Xbox gamepad controls by default for the touch gamepad and real controllers, with every Xbox button in the mapping editor and an RPG Maker preset (`ws/xbox-controls`).
- A blur over the lower game area on iPad after the game has been in portrait (`ws/ipad-blur`).
- Ludusavi save sync over SMB instead of SSH and rsync (`ws/ludusavi-smb`), reaching the share the PCs already use, as guest or with a password.

Merged into `dev` for the next release: Ren'Py games two folders deep are detected, Ren'Py on iPhone and iPad no longer offers touch control methods that do nothing there, and a game started after tapping Quit to library on a landscape iPhone no longer stays black.

## Next
1. Together, as the next work:
   - NScripter (ONScripter).
   - iPad multitasking: Split View, Slide Over, and an iPad window of its own. A windowed game follows the window's shape, as Follow device does, whatever in-game orientation Settings sets. The hub, the in-game menu, the touch gamepad, and every engine lay out correctly at small and odd window sizes. The iPhone has no split screen, so it needs no change.
2. AliceSoft System 3 and 4 (xsystem35, xsystem4).
3. Artemis.
4. Wolf RPG Editor, which needs a new runtime.
5. Suggested controller layouts for games and engines without gamepad support. On a game's first start, VNDegen reads the game's files for the controls it uses, such as its key bindings and whether it relies on the mouse. It then proposes a controller layout that maps those keys, the mouse pointer, and mouse buttons to the controller, which the player can accept or change. This may be hard and costly, so it comes second to last.
6. Last: save sync through every cloud type Ludusavi offers (Box, Dropbox, Google Drive, OneDrive, FTP, SMB, WebDAV, and a custom rclone remote), with the same settings as Ludusavi.

## Runtime packages
When a game needs a runtime package that VNDegen doesn't bundle, such as the RPG Maker RTP, the app offers to download it from its official source and install it, in addition to importing it from a folder.

## Out of scope for now
- Apple TV.
- iCloud saves.
- Unity, Godot, GameMaker, and other general-purpose engines. Flash through Ruffle is the most realistic of these if it is ever picked up.
