# Roadmap

Agreed with the maintainer on 2026-10-25. Engine order comes from `docs/engine-survey.md`. Release notes are in `docs/releases/`.

## Shipped
- RPG Maker MV/MZ on Mac, iPhone, and iPad (0.1.0).
- Archive import from zip, RAR, and 7z (0.1.0, fixes through 0.1.4).
- Ludusavi backup and restore through a folder picked in the app (0.1.0).
- TyranoScript (0.2.0).
- Control methods and button mapping (0.2.1).
- Ren'Py on macOS (0.3.0, fixes in 0.3.1).

## In progress
Each item is planned work on its own branch, not merged into `dev`.
- Ren'Py on iPhone and iPad (`ws/renpy-ios`). The plan is in `docs/renpy-spike.md`.
- KiriKiri (`ws/kirikiri`), starting with a feasibility test of encrypted `data.xp3` archives and Windows plugin DLLs.
- Ludusavi rebuilt into the app (`ws/ludusavi-native`). VNDegen will connect to the backup server itself over SSH with rsync instead of using a picked folder, match games automatically through Ludusavi's manifest, and build in Ludusavi's backup features.
- Fixes to the TyranoScript in-page Ogg decoder.
- A fix for the macOS Ren'Py window handoff.

## Next
1. RPG Maker XP, VX, and VX Ace, building on mkxp-z.
2. NScripter (ONScripter).
3. AliceSoft System 3 and 4 (xsystem35, xsystem4).
4. Artemis.
5. Wolf RPG Editor, which needs a new runtime.

## Out of scope for now
- Apple TV.
- iCloud saves.
- Unity, Godot, GameMaker, and other general-purpose engines. Flash through Ruffle is the most realistic of these if it is ever picked up.
