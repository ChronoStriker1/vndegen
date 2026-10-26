# Roadmap

Agreed with the maintainer on 2026-10-25. Engine order comes from `docs/engine-survey.md`.

## Now
- RPG Maker MV/MZ runtime, verified with No Socks RPG on the Mac, iPhone, and iPhone Duo.
- Ludusavi backup and restore against the existing Ludusavi store on the NAS (`games/Ludusavi`).
- Integration, Codex review, and an unsigned IPA for sideloading.

## Next
1. Archive import: pick a zip, rar, or 7z in Files and unpack it into the library. 462 of the 521 folders on the NAS are archives.
2. TyranoScript, reusing the MV/MZ WebKit runtime.
3. Ren'Py, as one engine entry that picks its runtime from the game's version (`renpy/__init__.py` or `renpy/vc_version.py`, falling back to `lib/py2-*`/`py3-*` and the engine's compiled files). macOS runs Ren'Py 8 and Ren'Py 7 games on matching bundled runtimes in a helper process. iOS embeds Ren'Py 8 and runs Ren'Py 7 games on it automatically through an in-memory compatibility layer, remembers which games boot, and ships the known-compatibility list from the library test. Game files are never rewritten. A runtime override on the game page is an advanced option only. Test whether saves made by Ren'Py 7 games on the Ren'Py 8 runtime still load in the PC version, and warn before a Ludusavi backup if not. Plan and estimates in `docs/renpy-spike.md`; the prototype is on branch `ws/renpy`. Built: macOS in 0.3, iOS in phase 2 (see "Phase 2 on iOS" in `docs/renpy-spike.md`).
4. KiriKiri, building on krkrsdl2.
5. RPG Maker XP, VX, and VX Ace, building on mkxp-z.
6. NScripter (ONScripter), then AliceSoft System 3 and 4 (xsystem35, xsystem4), then Artemis.
7. Wolf RPG Editor, which needs a new runtime.

## Out of scope for now
- Apple TV.
- iCloud saves.
- Unity, Godot, GameMaker, and other general-purpose engines. Flash through Ruffle is the most realistic of these if it is ever picked up.
