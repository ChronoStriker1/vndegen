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
- Files, in the order NScripter searches: loose files in the game folder; then `.ns2` archives, highest number first;
  then `arc.nsa` and `arc1.nsa` .. `arc9.nsa`, once the script runs `nsa`. Names match without regard to case, as on
  Windows: loose files through an index of the folder made at start, archive members by their lowercased names, and
  files the game writes later (saves, screenshots) through `casepath_resolve` from `Engines/Shared/casepath.c`. All
  six test games pass the case check (title, play, save and load with every name's case swapped) on the Mac app and
  the iPad simulator, and one on the iPhone simulator.
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

## Saves

Saves are the PC engine's files, byte for byte, so saves move between VNDegen and a PC copy of the game in both
directions:

- `saveN.dat` in save format 2, which engine versions 2.95 to 3.04 all write. 123 saves the runtime wrote for lab
  scripts match the PC engine's own exactly.
- `gloval.sav` (global variables), `kidoku.dat` (text already read), `NScrflog.dat` and `NScrllog.dat` (files and
  labels seen), when the script turns each on.
- Screenshots that `savescreenshot` writes, as 24-bit BMP.

They go in the folder the script's `savedir` names, or the game's script folder, as on Windows. Ludusavi sync finds
them there (`NScripterPlugin+Saves.swift`). A save made in save-on mode records the statement that saved, and loading
it runs that statement again, as the PC engine does. Global variables and the read-text record are written at each
save, when the game ends, and whenever the session pauses, since iOS may stop a paused app without warning.

## Frame rate

The runtime steps on the display link and draws a new frame only when the scene changes. With `-frameMonitor YES`, a
DEBUG build logs the session's pacing every 10 seconds ("NSFRAMES": display link rate, its 1% low, frames drawn, and
the slowest step). The runtime is built with `-O2` in Debug builds too, since an unoptimized software compositor
misses frames that a Release build makes.

Measured on 2026-10-27 on the Mac mini, with other threads' apps and a simulator running:

| Scene | Where | Target | Average | 1% low | Measured |
|---|---|---|---|---|---|
| Dungeon walking (Test Game 26) | Mac app | 60 | 60.0 | 60.0 | 30 s |
| Dungeon walking (Test Game 26) | iPad simulator | 60 | 60.0 | 60.0 | 30 s |
| Movies (Test Game 98's three clips, looped) | Mac app | 60 display, 29.97 movie | 60.0, 30.6 drawn | 60.0 | 40 s |
| Movies (Test Game 98's three clips, looped) | iPad simulator | 60 display, 29.97 movie | 60.0, 30.6 drawn | 60.0 | 40 s |

The movies' headers say 59.94 fps, but they hold 29.97 frames a second (88 frames in 2.92 s), which the runtime draws
in full. Loading a save or opening a menu can take one step of 40 to 100 ms while it decodes images.

## Known limits

- Built-in dialogs (`yesnobox`, `mesbox`, `input`, the default save and load menus) are drawn by the runtime rather
  than Windows, in its own layout. Their buttons are labelled in English when the message is plain ASCII, as a
  translated game's is, and in Japanese otherwise. `input`'s window position and size arguments are ignored.
- Effects 1 to 18 are drawn as NScripter's manual describes them. Effect DLLs (effect 99, such as `breakup.dll`)
  fall back to a crossfade.
- MP3 and CD audio aren't played. The test games use Ogg Vorbis and WAV.
- `.nsa` compression types 2 (LZSS) and 4 (NBZ) aren't read; no test game uses them.
- NSLua isn't supported, and of `MSpDraw.dll` only the drawing one test game uses is emulated.
