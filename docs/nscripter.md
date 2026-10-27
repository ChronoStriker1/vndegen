# NScripter

VNDegen runs NScripter games on its own NScripter runtime, written for VNDegen and MIT licensed like the rest of the
app. It replaces ONScripter, which is GPL. How it was written, and from which sources, is in
`docs/nscripter-cleanroom.md`.

## Layout

| Part | Where | What it does |
|---|---|---|
| Runtime | `Packages/VNDegenKit/Sources/CNScripter/core/` | C++17 interpreter: archives, script, sprites, effects, text, buttons, sound, movies, built-in menus, saves |
| C interface | `Packages/VNDegenKit/Sources/CNScripter/include/vnns.h` | What the app calls: start, step, frame, audio, input, quit |
| Plugin | `Packages/VNDegenKit/Sources/EngineNScripter/` | Detection, the session, input mapping |
| Presenter | `Packages/VNDegenKit/Sources/FrameHost/` | Shows the runtime's CPU frame through Metal and plays its mixed sound through AVAudioEngine. Shared with the other software-composited runtimes |
| Linux test host | `Engines/NScripter/host/` | Runs a game headless, driven by commands on stdin, for development on Linux |

The runtime draws each frame on the CPU at the game's resolution (640 x 480 or 800 x 600 for the test games). The
session steps it once per display refresh on the main thread and hands the frame to `FrameView`, which scales it to
the view, letterboxed. Sound is mixed at 44.1 kHz and pulled by AVAudioEngine on its audio thread. Everything runs in
the app's process, with no JIT, on Mac, iPhone and iPad.

Third-party code: stb_image (PNG, JPEG, BMP), stb_vorbis (Ogg Vorbis) and pl_mpeg (MPEG-1 movies with MP2 sound),
all MIT or public domain, listed in `Engines/NScripter/UPSTREAM.md`. Text is drawn with the font pack's substitutes
(`docs/fonts.md`): MS Gothic, NScripter's default, is Ume Gothic, a script's `font` or `defaultfont` name goes through
the same table, and Hiragino covers characters the substitute lacks.

## Detection

A game is NScripter when a folder, at the top of the game folder or up to two folders down, holds `nscript.dat`, or
`0.txt` or `00.txt` beside an archive (`.nsa`, `.ns2`, `.sar`) or one of NScripter's own files (`envdata`,
`nslua.dll`, `nsogg2.dll`, `nspng.dll`). A numbered text script alone isn't enough, since `0.txt` could be anything.

## What the runtime reads

- Scripts: `0.txt` .. `99.txt` or `00.txt` .. `99.txt` joined in order, else `nscript.dat` (each byte XOR 0x84).
  `0.txt` wins over `nscript.dat`, as on Windows. The first line may set the screen size and variable counts
  (`;mode800`, `;value500`, `;$V2000G1000S800,600L10000`).
- Files, in the order NScripter searches: loose files in the game folder, matched without regard to case; then `.ns2`
  archives, highest number first; then `arc.nsa` and `arc1.nsa` .. `arc9.nsa`, once the script runs `nsa`.
- Bitmaps NScripter's packer stored with its SPB compression, as well as PNG, JPEG and BMP.
- Sound: Ogg Vorbis and PCM WAV. Movies: MPEG-1 (`mpegplay`, `movie`, `avi`).

## English word wrap

Plain NScripter breaks English text wherever a line fills up, even inside a word. Some English translation patches
ship a patched engine that breaks between words instead, and their scripts rely on it. The runtime turns word wrap on
by itself when any `.exe` in the game's top folder has a PE section named `.patch`, which those patched engines add and
the stock engine doesn't have, and it logs that it did. Two test games have such an exe. `VNNSLaunch.englishWordWrap`
forces word wrap on for other games.

## Plugins

NScripter games call Windows DLLs. The runtime emulates the ones the test games use:

- `NSFont.dll`'s `gaiji`, which draws an image in place of a character.
- `MSpDraw.dll`, a layer plugin (`setlayer`, `layermessage`), for the drawing one test game asks of it. Not yet
  complete.

Other DLL calls are logged and ignored. NSLua (`nslua.dll`, Lua scripts) isn't supported; no test game uses it.

## Known limits

Work in progress. Saves are being matched to the PC engine's format.
