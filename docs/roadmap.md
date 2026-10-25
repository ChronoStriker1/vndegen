# Roadmap

Agreed with the maintainer on 2026-10-25. Engine order comes from `docs/engine-survey.md`.

## Now
- RPG Maker MV/MZ runtime, verified with No Socks RPG on the Mac, iPhone, and iPhone Duo.
- Ludusavi backup and restore against the existing Ludusavi store on the NAS (`games/Ludusavi`).
- Integration, Codex review, and an unsigned IPA for sideloading.

## Next
1. Archive import: pick a zip, rar, or 7z in Files and unpack it into the library. 462 of the 521 folders on the NAS are archives.
2. TyranoScript, reusing the MV/MZ WebKit runtime.
3. Ren'Py, starting with Ren'Py 8.
4. KiriKiri, building on krkrsdl2.
5. RPG Maker XP, VX, and VX Ace, building on mkxp-z.
6. NScripter (ONScripter), then AliceSoft System 3 and 4 (xsystem35, xsystem4), then Artemis.
7. Wolf RPG Editor, which needs a new runtime.

## Out of scope for now
- Apple TV.
- iCloud saves.
- Unity, Godot, GameMaker, and other general-purpose engines. Flash through Ruffle is the most realistic of these if it is ever picked up.
