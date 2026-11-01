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

## Movies

Movies play through vnvideo, the movie decoder native engines share (`Engines/Shared/vnvideo.[ch]`, described in [video.md](video.md)), which reads them through the core's file lookup, from the archives or beside them, in any case. MPEG-1 movies with MPEG-1 Layer II sound, such as the studio logos of Test Game 06, Test Game 76, and Test Game 77 and the openings and endings of Test Game 79, Test Game 80, and Test Game 81, stream from disk. Ogg Theora movies play silent or with Opus sound, as Test Game 79's title animation and scene movies and Test Game 78's prologue clips do.

A fullscreen movie, which the script waits on, shows over the game. It skips on the keys the PC engine skips it with: the game's advance keys, a click or Return, for a movie played with `skip=1`, and the keys the game gives the movie-skip role with `[keyconfig role=1]` for `skip=2`. Test Game 06 gives that role every key, and Test Game 78 its Cancel keys. Touches act as clicks and VNDegen's buttons send their keys, so Confirm presses Return and Cancel Escape. A press counts only if it starts during the movie, so the click that starts one doesn't also skip it. A movie on a layer goes to the core as that layer's picture, frame by frame, so the game can move, fade, or filter it, and a looping one starts over at its end. A movie's clock is its sound when it has sound, so picture and sound stay together. Movie sound follows the game's music volume. The game's log, `logs/artemis.txt` in its data folder, records each movie with the frames it showed and its average and 1% low frame rate.

## Screen effects

Effects a game writes as HLSL, such as Test Game 78's blur, mosaic, sepia, and negative, compile at load through the core's own shader compiler to Metal, from the Direct3D 9 effect files the game ships for Windows.

## Controls

Artemis games read a mouse and keyboard and no gamepad, so they start with the "Keyboard and mouse" layout. Through VNDegen's buttons, Confirm presses Return, Cancel presses Escape, Menu right clicks, which opens the game's menu or hides its message window, as the game chooses, Dash holds Control, which skips, and the page buttons turn the mouse wheel, which opens the message history and reads on. Touches act as the mouse.

## Known limits

VNDegen builds only what its test games need to reach their title, play, save, and load. These are left out, with the test games that hit them:

- Movies in other formats don't play and count as finished at once: Ogg Theora with Vorbis sound plays silent, and WMV, such as the logo of AMANATSU+, which isn't a test game, doesn't play. No test game has either.
- The Steam functions a Steam build calls don't exist. Test Game 77 calls one at startup and carries on without it, so its Steam achievements aren't checked.
- Only the ASCII glyphs of `.rft` bitmap fonts are drawn. Japanese text and symbols such as full-width commas and curly quotes in those games use MS Gothic, so a line with them can wrap a word earlier or later than on the PC. Test Game 79, Test Game 80, and Test Game 81 ship `.rft` fonts.
- A game's own native dialog, such as the name entry in Test Game 06, Test Game 76, and Test Game 77, shows as a system alert. Confirm, Return, or Space answers OK, Cancel, Escape, or E answers Cancel, and VNDegen's on-screen keyboard types into its text field.
- After a quick load in Test Game 78 the screen stays black until the next click, which shows the saved page.
- The message font a save records, in field 25 of the save, isn't restored on load. Every test game sets its message window up again when a save loads, so none shows a wrong font.
- The script stack a game's Lua reads through `e:getScriptStack` can differ from the one the engine runs in two cases. A `call` or `jump` tag from Lua that does nothing, such as a call to file `"0"` or a jump whose condition is false, still changes the stack Lua sees. After a `call` or `jump` from Lua, the frame Lua sees points at the start of the target file rather than at the label. No test game was seen to depend on either.
- Read history can mark the wrong line in two cases. If a save is made inside a macro and the macro finishes after a load, the line that called the macro isn't marked read. If the game's menu opens while a story line waits on a queued `wait`, the mark can land on the next instruction. Either way, that line counts as unread on the PC, so the PC's skip stops there once. No test game was seen to hit either.
