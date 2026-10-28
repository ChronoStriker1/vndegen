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
the view, letterboxed. Sound is mixed at 44.1 kHz and pulled by AVAudioEngine on its audio thread. A decoder thread of
the runtime's own keeps each playing sound 0.75 s ahead in a ring buffer, so the audio thread only resamples and mixes,
and a slow decode can't make it miss its deadline or hold up the game. Everything runs in the app's process, with no
JIT, on Mac, iPhone and iPad.

Third-party code: stb_image (PNG, JPEG, BMP), stb_vorbis (Ogg Vorbis), dr_mp3 (MP3) and pl_mpeg (MPEG-1 movies with
MP2 sound), all MIT, MIT No Attribution or public domain, listed in `Engines/NScripter/UPSTREAM.md`. Text is drawn with the font pack's substitutes
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
- Sound: Ogg Vorbis, PCM WAV and MP3 (`mp3`, `mp3loop`, `bgm` and the `mv` voices). Movies: MPEG-1 (`mpegplay`,
  `movie`, `avi`).
- Backgrounds (`bg`) are opaque, as on the PC engine: the default transparency mode doesn't apply to them.

## English word wrap

Plain NScripter breaks English text wherever a line fills up, even inside a word. Some English translation patches
ship a patched engine that breaks between words instead, and their scripts rely on it. The runtime turns word wrap on
by itself when any `.exe` in the game's top folder has a PE section named `.patch`, which those patched engines add and
the stock engine doesn't have, and it logs that it did. Two test games have such an exe. `VNNSLaunch.englishWordWrap`
forces word wrap on for other games.

## Plugins

NScripter games call Windows DLLs. The runtime emulates the ones the test games use:

- `NSFont.dll`'s `gaiji`, which draws an image in place of a character.
- `MSpDraw.dll`, a layer plugin (`setlayer`, `layermessage`), for the drawing one test game asks of it.
- Effect 99, a transition a DLL draws (`print 99,ms,"system\breakup.dll/urb"`), for the four one test game uses,
  drawn as the original DLLs draw them under the PC engine:
  - `breakup.dll` (`urb`, `lrB` and the other corners): the area that changes breaks into round 24-pixel pieces,
    starting at one corner, which shrink and fly off, towards that corner's side with `b` and away from it with `B`.
  - `lngtwave.dll/vwi`: the screen ripples vertically in a wave that runs down it and dies away, as it crossfades.
  - `trvswave.dll/h`: the changed area ripples sideways, the wave tightening and growing until halfway and easing
    back, as it crossfades.

  Other effect DLLs, and other parameters of these, crossfade. The DLL is matched by its file name, whatever its
  case and folder.

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
it runs that statement again, as the PC engine does. A save made inside a `defsub` before its `getparam` records the
arguments still to read, as the PC engine's does, so `getparam` reads them after loading it.

The PC engine writes global variables and the read-text record only at saves and when it quits. iOS may end an app
in the background without warning, and a player may force-quit it, so the runtime also writes them when the app
resigns active, goes to the background or terminates, and writes `gloval.sav` whenever the script stops for the player
after a global variable changed. A custom save menu that keeps each slot's date in a global keeps it. The files are
the same the PC engine writes.

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

## Built-in dialogs

`yesnobox`, `okcancelbox`, `mesbox` and `input` are laid out as the PC engine's Windows dialogs are: light grey, black
text, buttons side by side, over the game without dimming it. A message box is centred, with its first button
focused, so Return answers it. `input` opens at the top left of the screen, and its last four arguments set its
window's and text field's size, in the 4/3-pixel units the PC engine's dialog uses. Buttons are labelled in English
when the message is plain ASCII, as a translated game's is, and in Japanese otherwise. The default save and load
menus and the right-click menu keep the runtime's own layout.

## Known limits

- Effects 1 to 18 are drawn as NScripter's manual describes them. Of the effect DLLs, only `breakup.dll`,
  `lngtwave.dll/vwi` and `trvswave.dll/h` are drawn; others crossfade.
- CD audio (`play "*2"`) isn't played. The PC engine plays a disc's tracks; no test game uses them.
- MIDI music (`play "song.mid"`) isn't played; no test game has any.
- `.nsa` compression types 2 (LZSS) and 4 (NBZ) aren't read; no test game uses them.
- NSLua isn't supported; the test games ship `nslua.dll`, but their scripts don't call Lua.
- Of `MSpDraw.dll`, only the drawing one test game uses is emulated. Its author's manual (version 1.10) describes much
  more: `set`, `stop`, `move`, `hide`, `disp`, `getphase`, `drawTo` and a program mode. By that manual, the field the
  runtime reads as a horizontal speed is the tilt angle, which that game leaves at 0.
