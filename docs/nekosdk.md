# NekoSDK feasibility test

Roadmap item 10. NekoSDK has no runtime or reimplementation anywhere, so supporting it means writing one from the game files. This test mapped the formats of the 16 library games (Test Games 82 to 97, 49 GB unpacked) on Linux, parsed every script, and rendered the first scene of two games. It ran no Windows executable and decompiled none. The executables were only searched for strings.

## Summary

- **Data formats are solved.** One archive format, one script format, and standard image and audio formats cover all 16 games. Every script in every game parses: 6,993 scripts and 1,252,968 commands.
- **Story playback is data-driven.** Scenes are linear lists of about 80 commands with named parameters. The first scene of Test Game 82 (oldest layout) and Test Game 95 (newest layout) render on Linux from the game files, with backgrounds, characters, the game's own message window, names, and text.
- **Menus are not data.** The title, save and load, config, CG gallery, scene replay, sound test, and dialog screens are C++ code compiled into each game's executable, with their image names and coordinates in code. A runtime must rebuild them.
- **Six games keep their story order in the executable too.** Ten games have a `flow.pak` with the route, choices and variables as data. The other six (Test Games 82, 83, 86, 87, 90, 96) have no flow scripts, so the order of their scenes and any choices live in code.
- **Some command semantics are guesses.** The meaning of most command parameters follows from their values, but the sprite anchor, the animated transform's move semantics, and transition types need reference footage of the PC game to pin down.

## Sources and licenses

No runtime, partial runtime, or engine documentation exists. The tools below read the archives or extract text. I read them for format facts and copied no code from any of them. The prototype in `scripts/nekosdk/` is VNDegen's own code.

| Project | Commit | License | What it covers | Used |
|---|---|---|---|---|
| [GARbro](https://github.com/morkt/GARbro), `ArcFormats/NekoSDK/` | `b09ee45` | MIT | NEKOPACK4 archive layout and keys, BMP plus ALP alpha | Archive layout, read and reimplemented |
| [FuckGalEngine](https://github.com/Inori/FuckGalEngine), `NekoSDK/` | `c966c2b` | None | Kaitai spec of the script node, a Ruby converter for 9 commands | Node layout (a format fact). The prototype's seven character positions come from its converter and are unverified |
| [nekosdk_tools](https://github.com/adsf0427/nekosdk_tools) | `17eb326` | None | Same Kaitai spec, text extraction | README and spec read |
| [GalTransl-GalGameReverse-Skill](https://github.com/GalTransl/GalTransl-GalGameReverse-Skill), `engines/nekosdk.md` | `442ae2f` | GPL-3.0 | Prose notes on the archive and text records | Notes read, no code |
| [SExtractor](https://github.com/satan53x/SExtractor) | `8d8d976` | GPL-3.0 | Text extraction | Cloned, not read |
| [1F1E33-float32/Tools](https://github.com/1F1E33-float32/Tools) | `fc74ca2` | None | Text extraction | Cloned, not read |
| [lossless-text-repack](https://github.com/SuQiandYing/lossless-text-repack) | `c02b86b` | MIT | Text repacking notes | Cloned, not read |

A runtime would be written from scratch. The libraries it needs have permissive options: zlib (zlib license, or Apple's libz), libwebp (BSD), libogg and libvorbis (BSD) or stb_vorbis (public domain or MIT). Apple's frameworks cover BMP and PNG decoding, WAV, fonts, and H.264 video.

## Game library

| Game | Canvas | Flow scripts | Notes |
|---|---|---|---|
| Test Game 82 | 1280x720 | No | Images are WebP inside `.bmp` names, with difference images (below) |
| Test Game 83 | 1280x720 | No | |
| Test Game 84 | 1280x720 | Yes | Loose `sc/*.pak` add-on archives |
| Test Game 85 | 1280x720 | Yes | |
| Test Game 86 | 1280x720 | No | `sys_3.pak` patch |
| Test Game 87 | 1280x720 | No | |
| Test Game 88 | 1280x720 | Yes | `gt.pak` interactive text groups, loose `sys_1/scr` patch |
| Test Game 89 | 1280x720 | Yes | `init.ini` names its flows under other keys than `FirstFlow`. 31 ASF movies |
| Test Game 90 | 1280x720 | No | Separate DLC folder, character and phase select screens |
| Test Game 91 | 1280x720 | Yes | `gt.pak` |
| Test Game 92 | 1280x720 | Yes | `gt.pak`, 72 ASF movies, `sys_5` patch folder |
| Test Game 93 | 1280x720 | Yes | `gt.pak` |
| Test Game 94 | 1280x720 | Yes | `gt.pak`, 2 ASF movies, fan translation with broken string lengths (below) |
| Test Game 95 | 1920x1080 | Yes | `sys_5.pak` patch |
| Test Game 96 | 1280x720 | No | Custom vote, comment and route-tree systems, 94 config files |
| Test Game 97 | 1280x720 | Yes | Short, 7 scripts |

## Formats

### Archives

Every `.pak` is NEKOPACK4 mode `A`. GARbro's layout holds for all 16 games:

- Header `NEKOPACK4A`, then a `u32` index size at offset 10. The index starts at offset 14.
- Each index record is a `u32` name length, a CP932 name with its NUL, then a `u32` offset and `u32` size, both XORed with the signed sum of the name bytes. A zero length ends the index.
- An entry's first 4 bytes are XORed with a rolling key that starts at `size / 8 + 0x22` and shifts left 3 bits per byte. The entry is then a zlib stream, and its last 4 bytes give the unpacked size.

A path like `bg\B300_000.bmp` names entry `B300_000.bmp` in `bg.pak`. Some archives store entries with folders, such as `cfg\gt_000_001_100.txt` in `gt.pak` or `cfg\msg_button.ini` in `sys_5.pak`. Patches come as `sys_N.pak` archives and `sys_N/` folders, which must take priority over the base archives. Scripts refer to files in a different case from the archive, so every lookup must ignore case.

### Scripts

All scripts are `NEKOSDK_ADVSCRIPT2` files with a `.txt` name, in `scr.pak`, `flow.pak`, `gt.pak`, patch archives, or loose patch folders. A file is a node count and a list of nodes. Each node has an id, an editor colour tag, an editor position, an opcode, 32 `int32` parameters, a `next` id, 64 reserved bytes, and 33 length-prefixed CP932 strings. String 0 is a readable form of the command, such as `[ＢＧＭ再生] bgm\M01-m.ogg` with `ch:0/vol:40/pos:0/tm:3500` below it. String 1 usually holds the file name and strings 2 and 3 the text and voice. Nodes run by following `next` from the start node.

The fan translation of Test Game 94 breaks string lengths both ways. Three scripts have lengths one byte short, and two have NUL bytes inside correctly counted text. The parser trusts a length unless its span doesn't end in a NUL, and then reads on to the next NUL. That reads every script in all 16 games. How the PC engine handles these strings is not known.

Scene and flow scripts reuse opcode numbers for different commands, so the interpreter needs the script kind. Flow scripts use 13 commands: script call, scene end, choice, choice branch, condition branch, variable change, flowchart jump, recollection end and choice, a debug print, and a start marker. Scene scripts use 81 commands. How many games use them:

| Games using a command | Scene commands |
|---|---|
| All 16 | 19 |
| 10 to 15 | 19 |
| 5 to 9 | 14 |
| 2 to 4 | 6 |
| 1 | 23 |

The 19 universal commands are the start marker, text display, wait, text window hide, screen update (transitions), image load, delete and delete all, image transform, frame animation start, BGM playback, sound effect playback, stop and stop all, voice playback and stop, stop all sound, and screen shake and its stop. Other common ones are flash, noise, wave and motion blur effects, frame animations, and the older background and character commands. Commands only one game uses include the comment and highlighted-text system of Test Game 96, face graphics, slide panels, and particles. One command used by 10 games appears to drive an external accessory and could be ignored. The full table is in the private evidence (`opcodes.tsv`).

The image transform carries a slot (which is also the draw order), x and y offsets, x and y scale in thousandths, alpha out of 256, and flags for which of position, scale, rotation and alpha change. The animated form adds a duration. These meanings come from value ranges and a rendered result, not from the engine.

### Configuration

`cfg/init.ini` (CP932, tab-separated, `//` comments) gives the window size, frame rate, message window image and position, name and text positions, font size and outline type, line count, save thumbnail size, the system sound effects, the user data folder, and in nine games `FirstFlow`, the flow script the game starts from. Test Game 89 names its flows under other keys. Other files in `cfg.pak` list the CG gallery (`cg_view.ini`), scene replay with its flow scripts and unlock keys (`play_view.ini`), the sound test (`sound_view.ini`), message window buttons with sprite-sheet cells (`msg_button.ini`), flag names (`macrolist.ini`), and frame animations (`anime_cfg.ini`).

### Images

Images are 24-bit BMP with a separate `.alp` file of 8-bit alpha, one byte per pixel with no row padding, or PNG for some effects and animations. Test Game 82 stores WebP inside `.bmp` names and uses magenta as a transparency key. Some of its WebP files carry an extra `OVER` chunk naming a base image. Such a file is a difference image: magenta pixels keep the base pixel, and bases chain. Standard WebP decoders reject the extra chunk, so a runtime strips it first.

### Audio

Music, sound effects and voices are Ogg Vorbis, with a few WAV sound effects.

### Movies

Three games ship movies as extensionless ASF files with WMV3 video and WMA Pro audio: 31 background animations and clips in Test Game 89, 72 scene animations in Test Game 92, and 2 staff rolls in Test Game 94. Apple's frameworks decode neither codec. The portable decoders I know of are FFmpeg's, which are LGPL. The KiriKiri plugin's WMV support uses FFmpeg, so a NekoSDK runtime can't reuse it.

The proposed handling is for the plugin to play `movie/<name>.mp4` (H.264 and AAC, through AVFoundation) when the user has added one next to the original, and otherwise to skip the movie and continue as if it ended, logging it. The game page would say which movies are missing and give the one-line conversion command for a computer. The conversion tool is not part of VNDegen. Writing VNDegen's own VC-1 and WMA Pro decoders would make the movies play with no user step, but would take weeks for three games.

### Saves

Each game saves to `%APPDATA%\<UserDataDirectory>`, a folder named in `init.ini`, in files `save\save%03d.sav`, `save\cfg.sav`, `save\read.sav`, and `play_view.sav`. Ludusavi paths can be computed from `init.ini`. No PC saves were available, so the save format is unknown. Loading saves made on a PC needs sample saves to work it out. Without them, the runtime would use its own save format in the same folder.

## What the prototype does

`scripts/nekosdk/` holds Python research tools, which are not part of the app:

- `nekopak.py` lists and extracts archives.
- `advscript.py` parses and dumps scripts.
- `survey.py` and `opcodes.py` produced the format survey and the command table.
- `render_scene.py` runs a game on Linux headless from `init.ini`'s `FirstFlow` through the flow script into the first scene, or from a scene script given by hand. It writes one PNG per text box. It handles image load, transform, delete, screen update, text, the old background and character commands, ALP alpha, WebP colour keys and difference images. Animations show only their end state, and sound is only logged.

Results:

- **Test Game 95.** It followed `flow\flow00_000.txt` into `scr\ep_000_000_000.txt` and rendered 12 text boxes. The backgrounds are correct. In the zoomed opening CG, the anchor and move guesses frame a different part of the image than the PC game probably does.
- **Test Game 82.** It rendered 40 text boxes of `scr\ep_00_00.txt`, including three characters built from chained difference images. One line of translated text contains a private-use code point, probably a custom glyph the PC game draws.

## Work for a full runtime

A runtime would be VNDegen's own Swift code, drawing through Metal, with libwebp and an Ogg Vorbis decoder. These estimates are for agent work, with Apple builds and simulator checks included:

| Part | Estimate |
|---|---|
| Archive reader with patch priority and case-insensitive lookup, image and audio decoding | 1 week |
| Scene interpreter with the 38 commands that 10 or more games use: sprites, transforms and their animations, transitions with rule images, shake, flash, sound channels | 3 to 4 weeks |
| Message window, text layout and outlines, click wait mark, backlog, auto and skip, voice | 1 to 2 weeks |
| Flow interpreter: choices, branches, variables, flags, replay | 1 week |
| Menus as VNDegen screens built from the game's lists and art: title, save and load, config, CG gallery, scene replay, sound test | 2 to 3 weeks |
| Plugin integration: detection, session, input and controller layout, Ludusavi paths, case check, docs | 1 to 2 weeks |
| Effects that 2 to 9 games use, and the per-game modes: interactive text groups, Test Game 90's select screens, Test Game 96's vote and comment systems | 2 to 4 weeks, open-ended |

That is about 10 to 14 weeks before the per-game modes. It would make the ten flow-script games playable from start to finish with VNDegen-styled menus. The six games without flow scripts each need their scene order and choices rebuilt by hand. That is 2 to 5 days per game if the scene list and order can be read from strings in the executable, and not possible from data alone where choices live in code.

## Risks

- **Menus.** The game's own title, save, config and gallery screens are compiled in, with 100 to 550 hard-coded asset paths per executable and their positions in code. Reproducing them faithfully needs disassembly, which this test didn't do, or screenshots of each screen from the PC game. VNDegen-styled menus avoid this but look different from the PC game.
- **Command semantics.** The sprite anchor, animated transforms, transition types and effect parameters are inferred. Getting them right needs side-by-side reference footage of the PC games.
- **Six games without flow scripts** need per-game story order, and Test Game 96 has several systems no other game uses.
- **Saves.** Loading PC saves needs sample saves and format work. Without that, saves made on a PC don't load in VNDegen.
- **Movies** play only after the user converts them, unless VNDegen writes its own decoders.
- **Fonts.** The games ask for Windows fonts (MS Gothic and others) and custom glyphs in private-use code points.

## Recommendation

Go ahead in stages. Build the runtime for one flow-script game first, playable from title to ending with VNDegen-styled menus (about 5 to 6 weeks of the estimate above), then decide whether to extend it to the other nine flow-script games and the six code-driven ones. Two decisions would cut the risk most: whether VNDegen-styled menus are acceptable in place of the games' own screens, and whether a source of reference footage or PC saves can be used to check behaviour.
