# Nitroplus N2System

VNDegen's own clean-room runtime for Nitroplus's N2System ("NitroplusSystem2.0"), written in C and MIT licensed. `docs/n2system-cleanroom.md` lists the sources for each part.

Status: in progress on `ws/n2system`. This file starts as the design checkpoint and becomes the engine notes as the work lands.

## Games

| Game | Library | Engine | Notes |
|---|---|---|---|
| The Song of Saya (JAST USA, version 2.00) | yes, 1.1 GB | N2System, `saya.exe` | First test game. `.npa` archives without encryption or compression. English text, Tahoma font. |
| Saya no Uta Collection (original 2003 release) | yes, same archive | older Nitroplus engine, `.txt` scripts on a CD image | Not N2System. Out of scope. |
| Sonicomi | yes, `.npk` archives | a later Nitroplus engine | To check against N2System before any work. |
| Hanachirasu, Zanmataisei Demonbane, STEINS;GATE (JAST) | not yet | N2System per GARbro's title list; STEINS;GATE to check | The user can buy these. Demonbane and Hanachirasu encrypt their `.npa` files. |

## What the PC engine does

Observed from the game files and from `saya.exe` running under Wine.

- `system.npa` holds `system.ini` (Shift-JIS): product name and version, window title, default font (Tahoma 18), start script `nss/boot.nss`, the menu, save, load, config and backlog scripts, and text icons.
- `nss.npa` holds the scripts as plain-text NSS source in Shift-JIS, not compiled bytecode. The engine parses them at run time.
- `cg.npa` holds JPEG, PNG and BMP images. `sound.npa` and `voice.npa` hold Ogg Vorbis. Saya plays no movies.
- The screen is 800x600.
- NSS is a C-like language: `chapter`, `scene` and `function` declarations, `#include`, `if`/`else`, `while`, `switch`, `select` with `case` labels that name buttons, `call_chapter`, `call_scene`, `return`, `break`, `continue`. `$name` variables are global to the game and saved with each slot. `#name` variables persist across all saves. Function parameters are the only locals. `@` before a number makes a coordinate relative. A string literal that names a variable or parameter reads that variable.
- `<PRE @box>[name] ... </PRE>` blocks hold the text. Inside them, `<voice>`, `<K>` and `<k>` key waits, `<I>`, `<FONT>`, `{code}` runs at that point of the typewriter, and `&` escapes the punctuation after it.
- About 70 built-in functions drive a tree of named objects (textures, colors, windows, text, choices, sounds, processes) with priorities, aliases and wildcard paths such as `@box*img` and `@box09/*`. `Fade`, `Move`, `Zoom`, `Rotate`, `Shake` and `DrawTransition` animate them over time with ease curves.

### Saves

The PC engine writes to `%APPDATA%\Nitroplus\<system.ini name>\<system.ini version>\`, which for Saya is `Nitroplus\Saya no Uta - The Song of Saya\version2.00\`.

- `common\val.npf`: the `#` variables. `common\cqst.npf`: which text each script has shown, for skip.
- `NNNN\` per slot (`0000` to `0099`, plus `9999` for "return to previous choice"): `val.npf` (the `$` variables), `script.npf` (each process: the full source of every loaded script plus its execution state), `frames.npf` (each object: its creation call and its state), `bklg.npf` (backlog), `thum.npf` (thumbnail), and `date.npf`, `cmt.npf`, `tm.npf`, which the save screen script writes itself.
- Every `.npf` is an `NPSF` container: a 16-byte header (magic, unpacked size, two flags), a zlib stream, and the first 4096 bytes mapped through a 256-byte table after adding a key byte derived from the file name. Variables are fixed 1024-byte `NPSG` records.
- `script.npf` copies the engine's in-memory structures, including heap pointers. Reading positions out of a PC save is likely possible. Writing a `script.npf` the PC engine accepts means reproducing those structures. That is the main risk to byte-compatible slot saves. The plan is: global data (`common\`) and `val.npf` byte-compatible both ways first, then `frames.npf`, then a time-boxed study of `script.npf`. Anything left is reported, with options.

## Design

The runtime follows the System 3 runtime's shape: C with no threads and no platform code, CPU compositing, presented through FrameHost.

- `Packages/VNDegenKit/Sources/CN2System`, with the C interface `include/vnn2.h`: open, close, frame, pixels, frame serial, mouse, keys, wheel, audio pull, pause, quit, title, problem. The same calls as `vns3.h`.
- `npa.c`: archive index and reads. Name decoding from GARbro's reader (MIT). Encryption and compression only when a test game needs them.
- `fs.c`: resolves `cg/bg/x.jpg` to loose files first, through the shared `casepath_resolve`, then to the matching archive entry, ignoring case.
- `lex.c`, `parse.c`, `compile.c`: NSS source to bytecode per file. Each instruction keeps its source offset, so a save can name a position in the source, as the PC engine's saves do.
- `vm.c`: processes (the main script, `CreateProcess` threads, text blocks) as explicit stacks, so any of them can wait on time, input, an animation or text without C recursion.
- `obj.c`, `anim.c`, `draw.c`: the object tree, animations and the compositor (alpha, add, multiply blend, masks for `DrawTransition`, zoom and rotation).
- `text.c`: markup parsing, line breaking with `system.ini`'s kinsoku lists, glyphs from stb_truetype with the font from the shared font pack (`vnfont_resolve("Tahoma")`), outlines, typewriter speed and key-wait icons.
- `snd.c`: Ogg Vorbis through stb_vorbis, BGM, SE and voice channels with volumes, loops and fades, mixed into the pull callback.
- `save.c`: `NPSF` containers, `NPSG` variables, and the slot files above.
- `Engines/N2System/host`: Linux test tools on CMake, a headless runner that plays scripted input and writes PNG frames, and an SDL2 player.
- `EngineN2System` in Swift: detection (`system.npa` or `nss.npa` next to an exe that holds `NitroplusSystem2.0`), the session on FrameHost, input with the "Keyboard and mouse" layout, and save locations for Ludusavi.

Libraries: stb_image, stb_vorbis and stb_truetype (public domain or MIT), the system zlib, and the shared casepath, vnfont code.

## Estimate

About 9,000 lines of C and 500 of Swift. NPA and file layer 1 day. Parser, compiler and VM 3 days. Objects, animation, compositor 3 days. Text 2 days. Sound 1 day. System scripts (menu, save and load screens, config, backlog) running through 2 days. Saves 3 to 5 days, depending on `script.npf`. Swift plugin, Apple builds, simulator and case checks 2 to 3 days. Total 17 to 20 working days.

## Known limits

Filled in as the work lands.
