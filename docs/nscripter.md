# NScripter

VNDegen runs NScripter games on its own NScripter runtime, written for VNDegen and MIT licensed like the rest of the
app. It replaces ONScripter, which is GPL. The same runtime runs Ponscripter games, NScripter's dialect with UTF-8
scripts, in a mode of its own (see Ponscripter below). How it was written, and from which sources, is in
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

Third-party code: stb_image (PNG, JPEG, BMP), stb_vorbis (Ogg Vorbis), stb_truetype (Ponscripter's fonts), dr_mp3 (MP3), pl_mpeg (MPEG-1 movies with
MP2 sound) and Lua 5.1 (NSLua scripts), all MIT, MIT No Attribution or public domain, listed in
`Engines/NScripter/UPSTREAM.md`, plus the system's bzip2 library for NBZ sounds. Text is drawn with the font pack's substitutes
(`docs/fonts.md`): MS Gothic, NScripter's default, is Ume Gothic, a script's `font` or `defaultfont` name goes through
the same table, and Hiragino covers characters the substitute lacks.

## Detection

A game is NScripter when a folder, at the top of the game folder or up to two folders down, holds `nscript.dat`, or
`0.txt` or `00.txt` beside an archive (`.nsa`, `.ns2`, `.sar`) or one of NScripter's own files (`envdata`,
`nslua.dll`, `nsogg2.dll`, `nspng.dll`). A numbered text script alone isn't enough, since `0.txt` could be anything.

A Ponscripter game has `0.utf` (or `00.utf`) beside its archives, and is detected the same way; the library shows it as
NScripter, version Ponscripter. The Umineko When They Cry Question and Answer Arcs are the library's two. The original
Japanese releases, Umineko no Naku Koro ni Hane and Tsubasa, ship `nscript.dat`, and are ordinary NScripter games.
Umineko: Golden Fantasia is neither: it is a fighting game on an engine of its own (Direct3D 9), and isn't supported.

## What the runtime reads

- Scripts: `0.txt` .. `99.txt` or `00.txt` .. `99.txt` joined in order, else `nscript.dat` (each byte XOR 0x84).
  `0.txt` wins over `nscript.dat`, as on Windows. Ponscripter's `0.utf` .. `99.utf` (or `00.utf` ..) come before all
  of them. The first line may set the screen size and variable counts
  (`;mode800`, `;value500`, `;$V2000G1000S800,600L10000`).
- Files, in the order NScripter searches: loose files in the game folder; then `.ns2` archives, highest number first;
  then `arc.nsa` and `arc1.nsa` .. `arc9.nsa`, once the script runs `nsa`. Archive entries stored plain, with the
  packer's SPB bitmap compression, with its LZSS compression (type 2), or as NBZ (type 4, a bzip2 stream, which the
  Ponscripter games use for their bitmaps) are read. Names match without regard to case, as on
  Windows: loose files through an index of the folder made at start, archive members by their lowercased names, and
  files the game writes later (saves, screenshots) through `casepath_resolve` from `Engines/Shared/casepath.c`. All
  six test games pass the case check (title, play, save and load with every name's case swapped) on the Mac app and
  the iPad simulator, and one on the iPhone simulator.
- Bitmaps NScripter's packer stored with its SPB compression, as well as PNG, JPEG and BMP.
- Sound: Ogg Vorbis, PCM WAV and MP3 (`mp3`, `mp3loop`, `bgm` and the `mv` voices), and NBZ files, the sound
  plugin `nbzplgin.dll`'s format (a bzip2-packed WAV), wherever a sound command names one. Movies: MPEG-1
  (`mpegplay`, `movie`, `avi`).
- CD tracks: `play "*n"` and `playonce "*n"` play `CD\trackNN.ogg` (or `.mp3` or `.wav`) from the game's `CD`
  folder, looped or once; `playstop` and `stop` stop them and `cdfadeout` sets their fade. The PC engine plays the
  disc itself; the game that uses these ships its tracks as those files and tells the player to name them so.
- Backgrounds (`bg`) are opaque, as on the PC engine: the default transparency mode doesn't apply to them.
- `blt` copies part of the `btndef` image straight onto the screen, scaled to the rectangle it names, past the scene;
  a game's ending credits roll scrolls that way. The drawing stays until the scene is next committed, or the commit
  after that once `ofscpy` has run, as the manual describes.

## Ponscripter

Ponscripter is NScripter with UTF-8 scripts and proportional English text. The runtime runs a game in its Ponscripter
mode when the game's folder has `0.utf`; script text, file names and string variables are then UTF-8 bytes where they
would be Shift-JIS, so byte-counted commands (`len`, `mid`) count UTF-8 bytes. The mode's commands are known only to
Ponscripter games, so an NScripter game reads exactly as before. Everything below was settled as a black box on the
Umineko games' own engine, run under Wine (see `docs/nscripter-cleanroom.md`).

- Languages. The script holds each line in Japanese and English: `langjp` and `langen` set the language of the text
  that follows, until the next of them, and `langall` makes it every language's. `showlangen` and `showlangjp` choose
  the language shown, English until either runs. Text of the other language isn't drawn, makes no text buttons or
  ruby, applies no `~tags~` and leaves no line break, but its click, page and `clickstr` waits still run; the games' textgosub routine skips them by reading `getreadlang` (-1 for every
  language, 0 English, 1 Japanese). `language english` is read and ignored: it changed nothing in any lab.
- English text sits between carets, `^...^`. Inside them `@`, `\`, `#rrggbb`, `!sd`, `!sN`, `!dN`, `!wN` and `_` work
  as in any text line, while `$`, `%`, `{`, `<`, `(` and `[` are plain characters. A text line whose last character is
  the closing caret doesn't end the line on screen, so a sentence can run on over several script lines.
- Text is drawn in the game's own fonts: `h_mapfont n,"file"` names face n's file, and a face it doesn't name is
  `fonts/face<n>.ttf`. `~cN~` picks face N, plain; `~i~` and `~b~` toggle italic and bold, which add 1 and 2 to the
  face number, so `~ib~` in a game whose faces 1 to 3 are its bold font draws bold. A style lasts from line to line
  until a tag changes it. There's no fallback font: a character the face lacks draws as its missing-glyph shape.
- Layout follows the PC engine, which draws on a screen twice the game's size and halves it. Each glyph is rasterized
  (stb_truetype, no hinting) at twice the window's font size and placed at whole pixels of that doubled screen, the
  pen moving by the glyph's advance there, rounded down; there is no kerning, though the fonts carry some. The baseline
  is the font's ascender below the line's top (at the game's size, rounded up), lines are the font's height plus the
  window's line spacing apart (at the doubled size, rounded up), and `br` moves down half a line plus the spacing to
  start a paragraph. A word that won't fit starts a line, and the space before it isn't drawn; Japanese text breaks
  at any character, with NScripter's line-start kinsoku. The shadow is `shadedistance` pixels of the doubled screen
  (3 is a pixel and a half), and setwindow's bold flag changes nothing, as on the PC.
- `'` and `` ` `` are drawn as curly single quotes (U+2019, U+2018), `''` and ``` `` ``` as double ones. `"`, `--`
  and `...` stay as written, and there are no ligatures.
- String sprites (`:s`) and `select` items are laid out the same way. A size in an `:s` tag (`:s/16,16,0;`) is the em
  and the character pitch on the doubled screen, so `/30,30,0` is 15 pixels; only the height and the pitch count.
- A Ponscripter text line may open with a full-width space, which is drawn, where NScripter would skip it. Outside
  carets, a half-width space isn't drawn.
- `gettext` keeps a page's `~tags~`, and `logsp n,text,x,y,colours` draws such a page as a sprite for the games' own
  backlog: its position is on the doubled screen, it breaks at any character to fit the window's width, the page's
  line breaks are dropped, and the tags style it. Like `strsp`'s, the sprite isn't brought back by loading a save.
- `endroll` scrolls the `btndef` image for the end-of-episode credits: its eleven arguments are the screen rectangle,
  the image rectangle, a mode (the games use 2), the time per pixel in tenths of a millisecond, and the distance in
  pixels. The script waits until it has scrolled, and a click doesn't cut it short; the image stays on screen, as
  `blt` drawing does, until the scene is next brought up to date.
- `drawclear`, `drawsp` and `draw`, NScripter's direct drawing, build a picture of sprites that `draw` puts on the
  screen in the same way; one tea party's credits animate with them.
- `tachistate` reads whether each standing image (left, centre, right) is on screen.
- An effect's mask may be an image tag (`":c;Bmp\Efe\left.bmp"`), as the games write them; the file after the tag
  is the mask.
- `h_rendering` (no hinting, whole pixels) is read; it is what the runtime does.
- Images tagged `:b` (`:b;`, `:bc;`, `:ba/2,0,3;`, ...) are drawn at half their size, rounded up, each pixel the
  average of a 2x2 block with a pixel past an odd edge counting as transparent. The games' menus and buttons are drawn
  at twice the screen's resolution this way. `:c` draws a PNG with an alpha channel opaque, as if over black, where
  NScripter would keep its alpha.
- `steamsetachieve` logs the achievement and does nothing more.
- Saves are the PC Ponscripter engine's own files, `saveN.dat`, `global.sav` and the thumbnails the games write in
  `screenshot\`. The runtime keeps them in `vndegen-saves`, whatever `savedir` says, never in the game's `saves` folder,
  which Steam may sync; a PC backup's `saves` files restore there, and saves move both ways. The first time a game
  runs with no save slot of VNDegen's own, the PC's `saveN.dat`, `global.sav`, `envdata` and `screenshot\*.bmp` are
  copied from its `saves` folder into `vndegen-saves`, never replacing a file there; `saves` is only read, and a note
  left in `vndegen-saves` keeps the copy to once. The format is NScripter's
  after a `PNS` header, with positions on the doubled screen, the window as a text area in doubled pixels, sprites'
  shown flags with bit 1 set, call stack records of its own, no ruby settings and no message history. The PC engine
  keeps neither the text line's language nor the style, so a page replays from its head, whose lines name their
  language. A call frame in the PC format holds only its return offset; which user command it belongs to is worked
  out from the script on load (the name whose label holds where the call was, else the statement's head). VNDegen's
  own saves keep that beside them, in `saveN.dat.calls`, so they load as they ran. A sprite `csp` clears also moves
  to (-500, -500), as on the PC. The PC loads the runtime's save of the Question Arcs' first page to a screen
  identical to its own load, and the runtime loads the PC's.
- A textgosub or pretextgosub routine may draw text of its own; the line it interrupted goes on with its carets,
  language and style as they were. (Inferred: the games' routine draws none.)
- Malformed UTF-8 (overlong forms, surrogates, values past U+10FFFF) reads a byte at a time, as U+FFFD.

## English word wrap

Plain NScripter breaks English text wherever a line fills up, even inside a word. Some English translation patches
ship a patched engine that breaks between words instead, and their scripts rely on it. The runtime turns word wrap on
by itself when any `.exe` in the game's top folder has a PE section named `.patch`, which those patched engines add and
the stock engine doesn't have, and it logs that it did. Two test games have such an exe. `VNNSLaunch.englishWordWrap`
forces word wrap on for other games.

One English release ships no NScripter exe and writes its text lines for another engine: each line starts with a
backquote, and the text is half-width English. The runtime draws a backquoted stretch at half width, wrapped between
words, and treats each `|` inside it as a short pause (100 ms) the player can click past; the script has runs of up to
128 of them at dramatic moments. Backquotes also quote strings in that script's commands (`rmenu`, `savename`), and
are never drawn. All of this was settled from the script alone.

## Plugins

NScripter games call Windows DLLs. The runtime emulates the ones the test games use:

- `NSFont.dll`'s `gaiji`, which draws an image in place of a character.
- `MSpDraw.dll`, a layer plugin (`setlayer`, `layermessage`) that moves, turns, scales and fades pictures of its own,
  as its author's manual describes: `draw` with the picture's centre, alpha, rotation, zoom, tilt, step count,
  per-step increments, accelerations and start delay, `set` to change those values and run on, `clear` for one, several
  or all pictures, and the `+` and `-` blending tags. Two test games use it, for a panning picture and an opening
  sequence. The manual's tilt is drawn as the picture's height scaled by the cosine of the angle, which is what the
  DLL does in the PC engine (checked under Wine).
- Effect 99, a transition a DLL draws (`print 99,ms,"system\breakup.dll/urb"`), for the four one test game uses,
  drawn as the original DLLs draw them under the PC engine:
  - `breakup.dll` (`urb`, `lrB` and the other corners): the area that changes breaks into round 24-pixel pieces,
    starting at one corner, which shrink and fly off, towards that corner's side with `b` and away from it with `B`.
  - `lngtwave.dll/vwi`: the screen ripples vertically in a wave that runs down it and dies away, as it crossfades.
  - `trvswave.dll/h`: the changed area ripples sideways, the wave tightening and growing until halfway and easing
    back, as it crossfades.

  Other effect DLLs, and other parameters of these, crossfade. The DLL is matched by its file name, whatever its
  case and folder.

Other DLL calls are logged and ignored.

## NSLua

Games that ship `system.lua` beside `nslua.dll` script part of their logic in Lua. The runtime embeds Lua 5.1 and
does what the DLL's notes describe: `system.lua` runs when the game starts, before the define block; `luasub name`
makes `name` a command whose arguments the Lua function `NSCOM_name` reads with `NSPopInt`, `NSPopIntRef`, `NSPopStr`,
`NSPopLabel`, `NSPopComma` and the like; `luacall` asks for the callbacks `NSCALL_animation` (run at its own interval
while the game waits), `NSCALL_save`, `NSCALL_load` and `NSCALL_reset`. Lua reads and sets the script's variables,
runs single commands with `NSExec`, jumps with `NSGoto` and `NSGosub`, and reads sprites, the mouse, keys and the
timer through the documented `NS*` functions. Two test games use it: one for its battle data, menus and a target
marker animation, the other, a mod, for hooking extra scenes onto the story's labels. The direct drawing functions
(`NSD*`), `NSDefSpline`, `NSSpline` and `NL_bind` aren't emulated and log when called; `NSOkBox` and `NSYesNoBox`
log their text and answer yes, since the runtime has no blocking dialog.

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

Measured again on 2026-10-28, after sound decoding moved to its own thread, on the Mac mini with nothing else running
(load average about 3, no other simulators):

| Scene | Where | Target | Average | 1% low | Measured |
|---|---|---|---|---|---|
| Dungeon walking (Test Game 26) | Mac app | 60 | 60.0 | 60.0 | 50 s |
| Dungeon walking (Test Game 26) | iPad simulator | 60 | 60.0 | 60.0 | 40 s |
| Movies (Test Game 98's three clips, looped) | Mac app | 60 display, 29.97 movie | 60.0, 30.6 drawn | 60.0 | 40 s |
| Movies (Test Game 98's three clips, looped) | iPad simulator | 60 display, 29.97 movie | 60.0, 30.6 drawn | 60.0 | 50 s |

Simulator timing isn't iPad timing. The movies' headers say 59.94 fps, but they hold 29.97 frames a second (88 frames in 2.92 s), which the runtime draws
in full. Loading a save or opening a menu can take one step of 40 to 100 ms while it decodes images.

## Built-in dialogs

`yesnobox`, `okcancelbox`, `mesbox` and `input` are laid out as the PC engine's Windows dialogs are: light grey, black
text, buttons side by side, over the game without dimming it. A message box is centred, with its first button
focused, so Return answers it. `input` opens at the top left of the screen, and its last four arguments set its
window's and text field's size, in the 4/3-pixel units the PC engine's dialog uses. Buttons are labelled in English
when the message is plain ASCII, as a translated game's is, and in Japanese otherwise. The default save and load
menus and the right-click menu keep the runtime's own layout.

## Game commands

A game's Windows menu bar, which it builds with `resetmenu` and `insertmenu` and the runtime never draws, appears in
VNDegen's Game commands, through `vnns_menu` and `vnns_menu_click`. The functions VNDegen runs are listed: `SKIP`, the
read-text skip setting (`KIDOKUOFF`, `KIDOKUON`), and the three text speeds (`TEXTSLOW`, `TEXTMIDDLE`, `TEXTFAST`, from
`defaultspeed`), with check marks on the current choices. Volume (`DWAVEVOLUME`) and screen mode (`FULL`, `WINDOW`)
are left out. Lilipalace (Test Game 26) has its message skip and text speed only there. `deletemenu` leaves a game with
no menu bar, as on the PC.

The chosen text speed survives quitting and relaunching. The runtime writes the choice immediately to its own
`vndegen-text-speed.dat` in the save folder, a single byte containing 0, 1 or 2 for Slow, Middle or Fast. At the end of
the define block it restores that choice using the game's `defaultspeed` values. Missing or invalid data leaves the
default choice unchanged. The runtime does not read or write the PC engine's `envdata`.

## Known limits

- Effects 1 to 18 are drawn as NScripter's manual describes them. Of the effect DLLs, only `breakup.dll`,
  `lngtwave.dll/vwi` and `trvswave.dll/h` are drawn; others crossfade.
- CD audio plays only from files in the game's `CD` folder; a disc isn't read.
- MIDI music (`play "song.mid"`) isn't played; no test game has any.
- Ponscripter: glyph edges differ from the PC's by a few levels of grey, stb_truetype's antialiasing against
  FreeType's; layout is the same pixel for pixel in every lab. Only the `~i~`, `~b~` and `~cN~` tags are read, the
  ones the games use. The window's character pitch isn't added to proportional text (the games' is 0). `logsp` reads
  tags in any string, where the PC engine reads them only in a page that `gettext` gave; one backlog line in a lab
  broke a character later than the PC's. `endroll`'s other modes aren't measured. `len` and `mid` count bytes, as in
  NScripter; the games don't use them, and the PC engine wasn't checked. A save made before the first `setwindow`
  writes the default window in the doubled form, where the PC engine writes its own default values (the games set a
  window first). A save without its `saveN.dat.calls` record has its calls worked out from the script: a PC save, and a
  VNDegen save synced to another device, since the record stays on the device that wrote it. There, a save made
  inside a user command that jumped to another label before saving, whose arguments include another user command's
  name, that is the second call to the same command in one statement, or whose line uses a built-in's name (`gosub`,
  `csel`) as an alias inside an expression, may load as if it were a different call. The one-time copy of a PC copy's
  saves keeps to its folders and never replaces a file, but it trusts what it finds there: a hand-made list of an
  interrupted copy, or a slot of another case (`SAVE1.DAT` beside `save1.dat`) made while the copy runs, isn't
  told from VNDegen's own. An unclosed `~` tag is drawn as it stands, where the PC
  engine drops the line.
  Without a textgosub, the PC engine's `\` waits without clearing the page, and its saves resume at the statement
  that saved; the runtime clears the page and keeps NScripter's savepoints there. The games always set one. With a
  textgosub, a save made before any text follows the statement that saves, where the PC engine keeps the script's start.
- The backquoted English text of one release, and its `|` pauses, follow a reading of its script that no engine was
  available to check: the pause's length is a guess.
- NSLua: the direct drawing functions (`NSD*`), splines and `NL_bind` aren't emulated; `NSOkBox` and `NSYesNoBox`
  don't show a dialog. A game's Lua gets the base, table, string and math libraries only: no `io`, `os`, `package`
  or `debug`, and no `dofile`, `loadfile` or `require`, so a script reads files only through `NL_dofile` and the
  engine's commands. Lua runs on the game thread, so `NSSleep` sleeps for at most 10 ms, and a function that polls
  `NSDoEvents` for more than 3 seconds is told to return. `NSGoto`, `NSGosub` and `NSReturn` are refused inside a
  statement `NSExec` runs, as the DLL's notes forbid.
- Of `MSpDraw.dll`, the messages the test games send are emulated: `draw`, `set` and `clear`. `stop`, `move`,
  `hide`, `disp`, `getphase`, `drawTo` and the program mode its manual describes aren't.
