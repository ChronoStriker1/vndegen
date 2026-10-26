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
- RPG Maker XP, VX, and VX Ace (`ws/rgss`), starting with a feasibility test of mkxp-z on iOS.
- Ren'Py follow-ups (`ws/renpy-followups`): detect games two folders deep, and hide touch control methods that do nothing for Ren'Py on iPhone and iPad.

## Next
1. NScripter (ONScripter).
2. AliceSoft System 3 and 4 (xsystem35, xsystem4).
3. Artemis.
4. Wolf RPG Editor, which needs a new runtime.

## Out of scope for now
- Apple TV.
- iCloud saves.
- Unity, Godot, GameMaker, and other general-purpose engines. Flash through Ruffle is the most realistic of these if it is ever picked up.
