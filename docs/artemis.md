# Artemis engine

VNDegen runs Artemis engine games on art3m1s-core, an open-source Rust reimplementation of the Artemis runtime, built into the app as a static library. The game draws inside VNDegen's window through the core's Metal renderer, so the VNDegen menu and touch controls work as for the other engines. Lua runs on Lua 5.1, with no JIT, on Mac, iPhone, and iPad. `Engines/Artemis/UPSTREAM.md` lists the source, its licenses, how it is built, and every change VNDegen made to it.

## Games

A game is found by its base archive, a `.pfs` file such as `root.pfs` whose first bytes are `pf2`, `pf6`, or `pf8`, up to two folders down. The patch volumes beside it (`root.pfs.000`, `root.pfs.001`, ...) are mounted over it in order. A game unpacked into a folder is found by its `system.ini`, which names the script the engine boots. File names inside the archives are Shift_JIS, unless an archive's names read as UTF-8, and lookups ignore case, as on Windows.

## Saves

Saves are the PC engine's own files, so Ludusavi moves them between VNDegen and a PC in both directions. `docs/artemis-save-format.md` describes the format. The engine writes two versions of it: games whose archives are `pf2` use version 1000, and games with `pf8` archives, which run a newer engine, use version 1002. VNDegen writes the version of the PC save files it finds, or the one the archives suggest when there are none.

A game keeps its saves in the folder `system.ini`'s `SAVEPATH` names. On a PC that folder is in AppData when `SAVEPATH_CSIDL` is set, and beside the game otherwise. VNDegen keeps it in `saves/` in the game's data folder. The PC engine names that folder in Shift_JIS, which a Windows set to another language reads as cp1252, so a backup from such a PC holds the folder under a garbled name. VNDegen restores either spelling and uses the one that exists, the real name for a new game.

A save made in VNDegen loads in the PC version at the same page, waiting for the same click, and the other way round. This was checked with the PC executables under Wine for quick saves, numbered saves, and a save at a choice, on a game of each version.

## Text

Text uses the fonts a game ships. Some games ship their message font as bitmap `.rft` files, one per size, with no character map. VNDegen draws ASCII from them, spaced as the PC engine spaces it, so English text wraps where it does on the PC. Other characters, and faces the game ships no file for, use the font pack's MS Gothic.

Lines are spaced as the PC engine spaces them, with a body 1.2 times the font size plus the spacing the game's font settings add, so a page that runs below its message window breaks at the same line as on the PC and waits for a click.

## Controls

Artemis games read a mouse and keyboard and no gamepad, so they start with the "Keyboard and mouse" layout. Through VNDegen's buttons, Confirm presses Return, Cancel presses Escape, Menu right clicks, which opens the game's menu, Dash holds Control, which skips, and the page buttons turn the mouse wheel, which opens the message history and reads on. Touches act as the mouse.

## Known limits

- Movies don't play yet. A movie the story waits on is skipped.
- Only the ASCII glyphs of `.rft` bitmap fonts are drawn. Japanese text and symbols such as full-width commas and curly quotes in those games use MS Gothic, so a line with them can wrap a word earlier or later than on the PC.
- A game's own native dialog, such as a name entry, shows as a system alert.
