# Nitroplus N2System

VNDegen's own clean-room runtime for Nitroplus's N2System ("NitroplusSystem2.0"), written in C and MIT licensed. `docs/n2system-cleanroom.md` lists the sources for each part.

Status: in progress on `feat/n2system`. This file is the engine's notes: what runs, how, and the known limits.

## Games

| Game | Engine | State |
|---|---|---|
| Hanachirasu (English release, `hanachirasu.exe`, `version1.00`) | N2System, the build Nitroplus made for the English Steins;Gate release, with commands the port added | Title, story with text, voices, music and sound, choices, the opening movie, the right-click menu, backlog, Config, Hide Text, Auto, Skip and Return to Title, by mouse and keyboard, in the Linux runner. save and load in the PC engine's format, both ways with the PC engine under Wine |
| Sonicomi | Nitroplus's later "Diesel" engine on the Mware middleware: Squirrel scripts, Direct3D 9 3D models, `.npk` archives with AES | Not N2System. Out of scope |
| The Song of Saya | N2System | Not a test game. The save container format was first worked out from it in 2026-10 |

## What the PC engine does

Observed from the game files and from `hanachirasu.exe` running under Wine.

- `system.npa` holds `system.ini` (Shift-JIS): product name and version, window title, default font (Tahoma 18), start script `nss/boot.nss`, the menu, save, load, config and backlog scripts, and text icons.
- `nss.npa` holds the scripts as plain-text NSS source in Shift-JIS, not compiled bytecode. The engine parses them at run time.
- `cg.npa` and `data.npa` hold JPEG and PNG images. `sound.npa` and `voice.npa` hold Ogg Vorbis. Hanachirasu's movies are loose MPEG-1 files (video 800x608 at 29.97 fps, MPEG-1 Layer II sound at 48 kHz) in `movie/`.
- The screen is 800x600.
- NSS is a C-like language: `chapter`, `scene` and `function` declarations, `#include`, `if`/`else`, `while`, `switch`, `select` with `case` labels that name buttons, `call_chapter`, `call_scene`, `return`, `break`, `continue`. `$name` variables are global to the game and saved with each slot. `#name` variables persist across all saves. Function parameters are the only locals. `@` before a number makes a coordinate relative. A string literal that names a variable or parameter reads that variable.
- `<PRE @box>[name] ... </PRE>` blocks hold the text. Inside them, `<voice>`, `<K>` and `<k>` key waits, `<I>`, `<FONT>`, `{code}` runs at that point of the typewriter, and `&` escapes the punctuation after it.
- About 80 built-in functions drive a tree of named objects (textures, colors, windows, text, choices, sounds, processes) with priorities, aliases and wildcard paths such as `@box*img` and `@box09/*`. `Fade`, `Move`, `Zoom`, `Rotate`, `Shake` and `DrawTransition` animate them over time with ease curves.
- A button (`CreateChoice`) has `MouseUsual`, `MouseOver` and `MouseClick` children. Entering a state shows that state's children at full opacity, even when the script left them at 0, as the title and the choices do. `SetNextFocus` links buttons for the arrow keys.
- System scripts (the right-click menu, save, load, config, backlog) draw in a layer above the game's objects, whatever their priorities.

### Archives

`.npa` archives start `NPA\x01`, then two keys, a compressed flag, an encrypted flag, counts and the index. Entry names are stored with a per-entry offset added to each byte. Hanachirasu encrypts every archive: the first 0x1000 bytes plus the name length of each stored entry go through a 256-byte table, after subtracting a key byte made from a per-game name key, the name's bytes, the archive key and the entry's unpacked size, and the byte's position. The runtime keeps each known game's table and name key and picks the one whose decryption of an archive's first entries gives a recognizable file header. Scripts are zlib-compressed; images aren't.

### Commands the English release added

- `FileExists(path)`: whether a game file exists. The port's `ANIM` function loops on it.
- `MoviePlay(path, skippable)`: plays a movie full screen and waits for it.
- `AssocArray` and `SetEnableBacklog`: VNDegen runs them as no-ops.

### Saves

The PC engine writes to `%APPDATA%\Nitroplus\<system.ini name>\<system.ini version>\`, which for Hanachirasu is `Nitroplus\Hanachirasu\version1.00\`.

- `common\val.npf`: the `#` variables. `common\cqst.npf`: which text each script has shown, for skip.
- `NNNN\` per slot (`0000`, which the scripts save on their own, one per save screen slot from `0001`, and `9999` for "return to previous choice"): `val.npf` (the `$` variables), `script.npf` (each process: the full source of every loaded script plus its execution state), `frames.npf` (each object: its creation call and its state), `bklg.npf` (backlog), `thum.npf` (thumbnail), and `date.npf`, `cmt.npf`, `tm.npf`, which the save screen script writes itself.
- Every `.npf` is an `NPSF` container: a 16-byte header (magic, unpacked size, two flags), a zlib stream, and the first 4096 bytes mapped through a 256-byte table after adding a key byte derived from the file name. Variables are fixed 1024-byte `NPSG` records.
- `script.npf` (`NPSS`) holds, per process (one per `call_chapter` level), each loaded script's name and full source, a 7-number process header, then one record per `chapter`, `scene`, `cut` and `function` declaration and per inline `{code}` in a text block, in the order the PC engine's scanner finds them. The file has no record count: the loader scans the saved text again and pairs records by position, so the order must be exact. A function's record is also its only activation. The live call chain is in the records: state (1 running, 2 waiting on a callee), the return record, and byte offsets from the body's opening brace: where to continue, the innermost `while` (the byte after the keyword and the body's closing brace) and the active `select`. The running record continues at the start of its statement, which runs again; a caller continues just after its call's `;`, or after the line for `call_scene` and `call_chapter`. Pointers in the file are only identifiers for relinking.
- `frames.npf` holds one 0x7800-byte record per object in creation order: the owner process, the creation call as source text (variables replaced by their values, literal words such as `BGM` left bare), the scene path it was created under, its own name, path and alias, 17 core values (lock, disused, hideable, clickable, priority, x, y and others), then optional blocks: the window, the texture (opacity as 0-255, zoom, rotation, render mode and derived geometry), images, text glyph tables, running animations, and the sound (state, loop, volume, frequency, pan, the play position in bytes of decoded audio, fades, loop points). The PC engine re-runs the creation call in the owner process, then applies the rest; a playing sound resumes at its position.
- `bklg.npf` (`NPSB`) is the backlog as one Shift-JIS byte per 16-bit unit, with voice markers, `\n` ending each paragraph and `\r` at soft wraps. `common\cqst.npf` holds a read flag per source byte of each script.

VNDegen reads and writes these files as the PC engine does, so slots move both ways:
- A slot saved while text shows continues at that text block's `<PRE`, which runs the block again from its start, and no text object is written; the PC engine loads such a slot the same way. Loading a PC slot that holds a text object does the same.
- Objects are restored by running their creation call again, then applying the saved state. A running animation is given its end value.
- `$SYSTEM_present_process` holds the name of the running function or scene, as on the PC, because each scene copies it into the variable `LoadText` reads its script from. Function parameters are also global variables, as on the PC.
- A sound faded or set to volume 0 stops, as on the PC.
- `common\cqst.npf` isn't read or written; VNDegen keeps its own record of read text for skip.

In the app the save folder is `AppData/Roaming/Nitroplus/<name>/<version>` in the game's data directory, so a backup's Windows paths map onto it.

## Design

The runtime follows the System 3 runtime's shape: C with no threads of its own and no platform code, CPU compositing, presented through FrameHost.

- `Packages/VNDegenKit/Sources/CN2System`, with the C interface `include/vnn2.h`: open, close, frame, pixels, frame serial, mouse, keys, wheel, audio pull, pause, quit, title, problem, and `vnn2_product` for the save folder name.
- `npa.c`: archive index, decryption and reads. `fs.c`: resolves `cg/bg/x.jpg` to loose files first, through the shared `casepath_resolve`, then to the matching archive entry, ignoring case.
- `script.c`: NSS source to bytecode per file. `vm.c`: processes (the main script, `CreateProcess` threads, text blocks) as explicit stacks, so any of them can wait on time, input, an animation, text or a movie without C recursion.
- `obj.c`, `draw.c`: the object tree, buttons and keyboard focus, animations and the compositor (alpha, add, multiply blend, masks for `DrawTransition`, zoom and rotation).
- `text.c`: markup, line breaking, glyphs from stb_truetype with the font from the shared font pack (`vnfont_resolve("Tahoma")`), outlines, typewriter speed and key-wait icons.
- `snd.c`: Ogg Vorbis through stb_vorbis, BGM, SE and voice channels with volumes, loops and fades, and a stream channel for movie sound, mixed into the pull callback.
- `movie.c`: MPEG-1 movies through the shared `vnvideo` decoder (pl_mpeg). The picture follows the sound actually played.
- `save.c`: `NPSF` containers, `NPSG` and `NPSL` variables, thumbnails and the slot files. `scan.c`: the PC engine's script scanner, which gives the record order. `slot.c`: `script.npf` to and from the main thread's frames, through each chunk's body offset and statement table. `frames.c`: `frames.npf` to and from the object tree. `backlog.c`: `bklg.npf`.
- `Engines/N2System/host`: Linux test tools on CMake. `n2run` runs a game headless with scripted input (`wait`, `click`, `rclick`, `move`, `clicks`, `key`, `wheel`, `set:NAME=NUMBER`, `print:NAME`, `tree:NAME`, `shot:NAME`) and writes PNG frames and the sound it played as `audio.wav`. `n2play` is an SDL2 player.
- `EngineN2System` in Swift: detection (`system.npa` beside `nss.npa`, up to two folders down), the session on FrameHost and the shared frame driver, input with the "Visual novel" controller layout, and the save location above.

Libraries: stb_image, stb_vorbis and stb_truetype (public domain or MIT), pl_mpeg through the shared `vnvideo` (MIT), the system zlib, and the shared casepath and vnfont code.

## Known limits

Filled in as the work lands.

- Only Hanachirasu's archive key is known. Another encrypted N2System game needs its own table, recovered the same way from its files.
- `AssocArray` and `SetEnableBacklog` do nothing.
- A loaded slot starts its text block again from the beginning, on the PC engine too when VNDegen saved it. After loading a slot the PC engine saved, the lines of that block it had already shown appear twice in the backlog.
- `common\cqst.npf` (read text for skip) isn't shared with the PC engine.
- Pixel dumps of images changed at run time (`images\NNNN_MMMM.npf`) aren't read or written; no Hanachirasu scene was seen to make one.
