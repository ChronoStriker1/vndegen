# NScripter runtime: clean-room provenance

VNDegen's NScripter runtime was written without ONScripter or any of its forks (Ogapee's ONScripter, ONScripter-jh,
ONScripter-EN, OnscripterYuri, PONScripter), which are GPL. Nobody working on it opened, searched or asked about their
source, or the VNDegen branch that once ported one. This file lists, for each part, where its knowledge came from.

## Sources

- **NScripter's own documentation**, from its author's site, nscripter.com: `nscrdoc.zip` (the manual's command
  pages, `新マニュアル/追加命令一覧.txt` with the commands added after the manual, the plugin interface notes and
  samples, the sample scripts) and `nscr_2018.zip` (the final release's manuals: update history 2009 and 2010 to
  2018, FAQ, NSLua notes). Also github.com/NaokiTakahashi/NScripter at 8f252e2, the author's repository of the final
  release, which holds manuals and binaries and no engine source.
- **The test games' own files**: scripts, archives, images, sounds, save files.
- **The original engine as a black box**: each test game's NScripter exe, and the author's archive tool
  `nsaarc.exe`, run under Wine on Linux. Small scripts of our own ran on them to settle open questions, and their
  output (screens, save files, archives) was compared. Nothing was disassembled.
- **Permissive libraries**, used as libraries: stb_image, stb_vorbis, pl_mpeg and dr_mp3 (see
  `Engines/NScripter/UPSTREAM.md`).
- **The effect DLLs one test game ships** (`breakup.dll`, `lngtwave.dll`, `trvswave.dll`), run as black boxes by that
  game's own NScripter exe under Wine. No documentation of them was found. Lab scripts of our own ran each call the
  game makes on synthetic images (grids, and images whose colours encode each pixel's position), and the screen was
  recorded at 30 frames a second. The effects were worked out from the frames. Nothing was disassembled.
- **`MSpDraw.dll`'s own distribution**, `MSpDraw.zip` by its author (manual `MSpDraw.txt` version 1.10, a sample
  script, the DLL), from the author's site as the Internet Archive keeps it, found through a public list of
  NScripter plugins. Its manual gives the messages and their fields, which the emulation follows.
- **NSLua's notes** in `nscr_2018.zip` (`NSLua解説１～２.txt`, `NSLua解説３.txt`, `NSLua内部情報.txt`,
  `NSLua直接描画命令.txt` and the four samples), and **Lua 5.1.5** itself (MIT), used as a library. The games'
  `system.lua` files showed which functions matter.
- **The stock cursor bitmaps** in `nscrdoc.zip` (`デフォルトのカーソルファイル`), as plain files to compare LZSS
  archive entries against.
- **bzip2**, the system library, for NBZ sounds.

Not used: the English "NScripter API reference" hosted inside an ONScripter project's website, though it is prose,
because of where it lives. GARbro (MIT) was opened to check its NScripter archive support, but its decompression code
says it comes from ONScripter-EN, so only its license header and a list of names were seen, and it was deleted.

## By part

| Part | Source |
|---|---|
| Script loading and order (`0.txt` before `nscript.dat`, `00.txt` .. `99.txt`) | Manual, `d/scload.htm` and update notes; checked on the test games |
| `nscript.dat` encoding (XOR 0x84) | The test games' files: decoding with that key gives the script |
| First-line settings (`;mode800`, `;value500`, `;$V..G..S..L..`) | Update history (2015) and `追加命令一覧.txt` |
| Archive formats (`.nsa`, `.sar`, `.ns2` indexes) | The test games' archives, read byte by byte |
| SPB bitmap compression | Worked out from `nsaarc.exe`'s output for synthetic bitmaps (gradients, single deltas, noise). Confirmed by exact pixel matches of 25 probe images, and by every one of 5,846 SPB entries in a test game decoding to exactly its stored length |
| File lookup order (loose, then `.ns2` highest first, then `.nsa` after `nsa`) | Black box: colored images of one name in a loose file and in archives |
| Commands and their arguments | Manual command pages, `追加命令一覧.txt`, the samples, the update history |
| Images with an alpha channel keep it whatever the tag says | Black box: a test game's title logo, an RGBA PNG loaded with `:a;` |
| When the screen updates (`print`, effects, text output; not `wait` or `click`) | Black box: lab scripts |
| English word wrap trigger (`.patch` PE section) | The test games' patched and stock exes, compared by their section tables and strings; one patch's readme credits the patched exe with word wrap |
| Save files (`saveN.dat`, format 2) | Black box: saves written by four engine versions (2.95, 2.96, 3.02, 3.04) for lab scripts that change one thing at a time, then a test game's real gameplay save. 123 lab saves from the runtime match the engine's byte for byte |
| Where a save resumes | Black box: lab scripts. Saves record the saving statement until a text page starts, then the page head until the next page, `saveon` or `saveoff`. Loading a save made with `saveon` runs the saving statement again (a second run wrote a save numbered by a global variable it had changed) |
| Script offsets in saves | Black box: the same lab script with and without carriage returns. Offsets count the script without them |
| `kidoku.dat`, `NScrflog.dat`, `NScrllog.dat` | Black box: their sizes and contents after lab scripts; the logs are a count line and quoted names XOR 0x84 |
| Global save (`gloval.sav`) | Black box: its size is 5 bytes per global variable when all strings are empty; one int32 and one NUL-terminated string each |
| Font metrics and baseline | `docs/fonts.md` (VNDegen's font pack); MS Gothic's metrics |
| `breakup.dll` | Black box (engine 2.96 with the game's DLL): sprites made of colour-coded pixels, removed with each corner's parameter, at 1000, 4000 and 4500 ms. Tracking each pixel gave the 24-pixel grid laid from the changed rectangle's corner, the start order (one cell a step from the named corner), each piece's 44 steps (4 shrinking in place, then 7 pixels a step in one of 8 directions 19.3 degrees apart), its radius each step (from its pixel counts), the step length (the duration over the number of steps), and that only changed pixels fly |
| `lngtwave.dll/vwi` | Black box: rows coded by colour ratio, so each screen row's source row survives the crossfade. A sine with a period of half the screen, pinned at the top, running down about 3.8 cycles while its amplitude falls from about 38 pixels to 0; old and new frames move alike and crossfade linearly. Measured at 1000 and 5000 ms: it depends only on the progress |
| `trvswave.dll/h` | Black box: columns coded the same way. Each row shifts by -16q sin(2 pi q y / 16), q being the progress or its complement, whichever is smaller; a linear crossfade; only the changed rectangle moves. Measured at 1500 and 3000 ms |
| MP3 | dr_mp3 decodes it; the commands that play it are in the manual (`mp3`, `mp3loop`, `bgm`) and `追加命令一覧.txt` (`mv` voices) |
| Opaque backgrounds | Black box: a `bg` image whose top-left pixel is white shows that white, with the default transparency mode |
| `input`'s last four arguments | Black box: the same call with each argument changed. They are the dialog's width and height and its text field's width and height, in 4/3-pixel units (300 makes a window 394 pixels wide inside its frame, 500 makes it 661). The dialog opens at the top left of the game's window, light grey, with an OK button beside the field; with no sizes it is 397 by 92 pixels |
| `yesnobox` layout | Black box: a Windows message box, 167 by 63 pixels inside its frame for a short message, centred, buttons side by side with the first focused; the game isn't dimmed |
| Saves inside a `defsub` | Black box (engines 3.04 and 2.96): a defsub frame's saved offset is that of the arguments `getparam` hasn't read (10 lab saves, byte for byte), and loading such a save lets `getparam` read them. The PC engine treats arguments left unread at `return` as text to display, which no working game relies on; the runtime skips them |
| Unused extended sprites after a load | Black box: a save made after loading writes alpha 255 for unused `lsp2` slots, where a fresh game writes -1 |
| `gloval.sav` written early | Black box: the file the PC engine writes at exit is the one the runtime writes at the first click wait after the same globals change, byte for byte (engines 3.04 and 2.96) |
| `.nsa` compression type 2 (LZSS) | Two games' 14 entries, read as a bit stream. A flag bit, then a literal byte or an 8-bit position in a 256-byte ring and a 4-bit length + 2; the ring starts zero-filled with its write pointer at 239. 10 of the entries are NScripter's stock cursor bitmaps and decode byte-identical to the files in `nscrdoc.zip`; the other two are the same cursors with two more bytes; all 14 decode to exactly their stored sizes |
| NBZ sound files | One game's archive entries named `.nbz`, each a 4-byte big-endian size followed by a bzip2 stream of a WAV file; `soundpressplgin "nbzplgin.dll\|nbz"` in the manual names the plugin. Decoded by the system's bzip2 |
| CD tracks | The manual's `play`, `playonce`, `playstop` and `cdfadeout` pages (a disc's track n; no folder is described); the game's `CD` folder of `track01.ogg` .. `track10.ogg` and its readme, which tells the player to put ripped tracks there under those names |
| Backquoted English text and `\|` pauses | One game's script alone: text lines open with a backquote and hold half-width English; `\|` appears singly, doubled, and in runs of up to 128 at dramatic moments, so it is read as a pause unit. Backquotes also quote strings in its `rmenu`, `clickstr` and `savename` lines |
| NSLua | The notes: `system.lua` loads at start; `luasub` binds `NSCOM_name`; `luacall` binds `NSCALL_name`; the `NSPop*`, `NSGet*`/`NSSet*`, `NSExec`, `NSGoto`/`NSGosub`/`NSReturn`, sprite, sound, mouse, key and timer functions, `NSLuaAnimationInterval`/`Mode` and `NSTimer`. Checked by running the two games' `system.lua` files: battle data tables, a target marker that `NSCALL_animation` flashes, and a mod's label hooks with `NSCALL_reset`, `NSCALL_save` and `NSCALL_load` |
| `MSpDraw.dll` messages | Its manual: `draw/n,tag,cx,cy,alpha,rot,zoomx,zoomy,slope,steps,dcx,dcy,dalpha,drot,dzoomx,dzoomy,dslope,acx,acy,delay`, `set/n,name=value,...,time=count`, `clear/n,...` or `clear/all`, and the `+`/`-` blending suffix of the tag. One game's opening sequence (a scrolling map, a logo tilting in, a spear rising with an additive glow) runs on it |
| `getini` values | Windows' `GetPrivateProfileString`, as a mod's `list.ini` relies on: spaces around the key and value are dropped, and a matching pair of quotes around the value |

## Open questions and how they were settled

- **Background images smaller than the screen**: drawn centred. To be confirmed on the original engine.
- **`:a` images with several cells**: each cell is its picture followed by its mask, as a test game's two-cell button
  image shows (black and white halves alternate).
- **Width-1 SPB images**: the packer writes an extra junk column for them. No game image is one pixel wide, so the
  runtime doesn't special-case them.
- **Movies with a 59.94 fps header**: one test game's MPEG-1 clips hold 29.97 frames a second (ffprobe counts 88
  frames in 2.92 s), so drawing 30 a second is their full rate.
- **Compression type 4 in `.nsa`**: no test game uses it and no sample exists to check a decoder against, so it
  isn't implemented. The runtime logs the file instead. (Type 2 is implemented; see the table.)
- **`MSpDraw.dll`'s tilt**: the manual calls `slope` a tilt angle, 0 upright and 90 flat, and lists projection as its
  slowest drawing mode. Checked as a black box: a lab script ran the DLL in one game's own engine under Wine, drawing
  a picture at slope 90 and tilting it to 0 over 30 steps, with the screen recorded at 30 frames a second. The
  picture's width stayed about 230 pixels throughout while its height grew from a sliver to its full 136, within a
  step of the cosine of the angle, and its top and bottom edges stayed the same width (no perspective). The runtime
  draws it the same way.
- **The `\|` pause length** in the backquoted script: 100 ms each, a guess; no engine was available to measure it.
- **`heartmode`**, a define command one game uses that no manual describes: read and ignored.
- **Lua's standard libraries**: the notes say only that nslua.dll embeds Lua. The runtime opens the base, table,
  string and math libraries and withholds file, shell and module loading, since a game's script must not reach
  outside its folder on a phone; the two games' scripts use nothing else (one opens a debug log only behind a flag
  that is off).
- **`NSTimer` during a Lua call**: the notes describe it as the system's millisecond tick. The runtime's clock
  advances between frames, so `NSTimer` adds the time the current frame has taken, which lets a timed Lua loop end.
- **The drawing order of `MSpDraw.dll` pictures**: the order they were drawn in, which puts one game's map under its
  logo as the script needs. The manual doesn't say.
- **The effect DLLs' rectangle**: the PC engine passes a DLL the rectangle it redrew, so `breakup.dll`'s grid starts at
  the removed sprite's corner. The runtime uses the smallest rectangle holding every changed pixel, which can start a
  few pixels further in when the sprite has a transparent margin. The pieces look the same.
- **`breakup.dll`'s directions**: each piece's direction comes from a fixed sequence, the same on every run, which no
  formula tried reproduced. The runtime uses a fixed sequence of its own, so a transition also looks the same each
  time.
- **`breakup.dll`'s radius**: the pieces' pixel counts fit a disc of radius 11.85 - 0.995 n after n pairs of steps in
  flight; the true rule may be a Windows ellipse, but the counts match.
- **CD audio**: NScripter's manual says `play "*n"` plays a disc's track n and describes no folder of track files.
  The game that plays them ships them as `CD\trackNN.ogg`, the name its readme gives, so that's what the runtime
  plays, accepting `.mp3` and `.wav` too.
