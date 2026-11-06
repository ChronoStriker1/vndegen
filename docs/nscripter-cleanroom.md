# NScripter runtime: clean-room provenance

VNDegen's NScripter runtime was written without ONScripter or any of its forks (Ogapee's ONScripter, ONScripter-jh,
ONScripter-EN, OnscripterYuri, PONScripter and its forks), which are GPL. Nobody working on it opened, searched or asked about their
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
- **The Umineko When They Cry games' own Ponscripter engine** (`Umineko1to4.exe`, "Ponscripter 20160615 (NScr 2.81)"),
  run as a black box under Wine with networking off, on lab scripts of our own written as `0.utf` in a scratch folder
  beside the game's fonts. The screen was captured 1:1 (a 712 x 534 virtual screen puts the 640 x 480 game at 36,27).
  The games' scripts and archives showed which commands and formats matter. The engine's list of command names was
  read from the binary's strings, to know which names it defines; nothing else in the binary was looked at.

Not used: the English "NScripter API reference" hosted inside an ONScripter project's website, though it is prose,
because of where it lives. For the same reason, Ponscripter's own documentation, which lives in the GPL project, wasn't
read, nor was the repository a comment in one game's script names. GARbro (MIT) was opened to check its NScripter archive support, but its decompression code
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
| `blt` and `ofscpy` | `追加命令一覧.txt`: blt's eight arguments (screen rectangle, then the btndef image's rectangle, scaled to fit), drawn straight to the screen and not saved; ofscpy moves that drawing to the offscreen image so an effect or print can follow. One game's ending credits roll scrolls a 350 x 3444 image this way. Drawn opaque, since it's a straight copy, into a layer over the committed scene and under the built-in menus, so hiding the text window or opening a menu redraws around it; a load or reset drops it. Checked on that roll and lab scripts |
| Ponscripter scripts | The Umineko games: `0.utf`, UTF-8 with CRLF line ends, its first line `;value2500`; the archives and fonts beside it |
| `.nsa` compression type 4 | The Umineko games' archives: every one of 2,913 type 4 entries is a 4-byte big-endian size and a bzip2 stream of a BMP of exactly that size, the layout of NBZ sounds |
| Ponscripter's carets | Black box: lab lines with each control inside `^...^`. `@`, `\`, `#rrggbb`, `!sd`, `!sN`, `!dN`, `!wN` and `_` act; `$5`, `%5`, `{}`, `<>`, `[]` and a mid-line `/` are drawn as they stand; an unclosed `~` blanks the whole line |
| Ponscripter's line ends | Black box: a line ending in `^` runs on into the next line's text; one ending in `@`, `!sd` or Japanese text breaks the line; `/` and `\` act as in NScripter. A line that draws nothing, such as `langen!sd` or `langen#ffffff` between two lines of text, leaves no line break |
| Page waits without textgosub | Black box: `^aaa^\`, `^bbbbbb^\`, `^ccccccccc^@` stood on three lines after two clicks, plain Japanese lines too, so the PC engine's bare `\` doesn't clear the page. The games' textgosub routine ends in `texec`, which clears it after a page wait in the language shown; after one in the hidden language it leaves the page (see where saves resume) |
| Ponscripter save files | Black box: the PC engine's saves for lab scripts that change one thing at a time (window, background, standing images, sprites, `lsp2` sprites, variables, bars and prnum, gosub and for frames, carriage returns, language commands) and the Question Arcs' own save. A `PNS` header (bytes 2 and 6), three ints (1, 1, 0), then NScripter's save format 2 field for field, except: the window as origin, text area width (columns × 2 × (font width + 2 × pitch)), height (rows × 2 × (height + 2 × spacing)), font height, an int, pitch and spacing, all doubled, and its rectangle doubled; standing image, sprite and `lsp2` positions doubled; shown flags with bit 1 set; the call stack as minus its frame count, then each frame outermost first (0 and the return offset for a gosub; 1, the body's offset, variable, limit and step for a for loop), offsets counting the script without carriage returns; no ruby fields; ten ints of 0 in place of NScripter's tail; no message history; the line and statement, then the note in quotes and `*`. White stays in the text color field whatever `textcolor` sets; a fresh game keeps monocro color (136, 246, 122) and 1 in the field after the prnums. `global.sav` holds the global variables as NScripter's `gloval.sav` does. Language commands and `allsphide` leave no mark. The runtime's saves of the same labs match byte for byte, except before the first `setwindow` |
| `csp` and sprite positions | Black box: the Question Arcs' save. Sprites cleared after holding an image are at (-1000, -1000) on the doubled screen with cell 0; sprites never loaded stay at (0, 0) |
| Save thumbnails | Black box: the PC engine writes `savescreenshot2 "screenshot\ss1.bmp"` to `saves\screenshot\ss1.bmp`, and the games show it with `lsp` from there |
| Where a Ponscripter save resumes | Black box: the PC's saves end with the 0-based script line they resume at, a 32-bit little-endian number 11 bytes from the end. In lab scripts shaped like the games' (`saveoff` at the start, a pretextgosub running `saveon`, a textgosub running `saveoff` before it waits, except on lines in the hidden language), a save from the textgosub resumed at the head of its page, the first text line since the page was cleared, but only when `saveon` was in effect at a later text line's head before the pretextgosub ran; a hidden `\` and `texec` didn't start a new page. With a textgosub and no pretextgosub, every save resumed at the start; with neither, at the statement that saved. The Question Arcs' save on its first page resumed at `langjp!sd` at the head of `*umi1_opning` (line 3189), the episode's TIPS introduction |
| A word after a command's arguments | Black box: `for %1 = 1 to 5 : add %3,%1 next` and `mov %4,7 mov %5,8` gave 15, 7 and 8. The games redefine `csp` with `defsub` and write `for %tmp = 100 to r_used_lsp : csp %tmp next`; on the PC that loop clears the System panel before the Save screen opens, so a word after the arguments a user command's `getparam` read starts the next statement too |
| `langen`, `langjp`, `langall`, `showlangen`, `showlangjp` | Black box: a bare `langjp` hides the lines after it under `showlangen` until `langen` or `langall`; with neither `showlang` command, English shows; a hidden line's `@` took a real click (xdotool) before the shown line appeared |
| `getreadlang` | Black box: -1 at start and after `langall`, 0 after `langen`, 1 after `langjp`. The games' textgosub routine compares it with the language the player chose |
| `language` | Black box: `language english` and `language japanese` showed the same lines |
| `:b` images | Black box: an image of known pixels with each tag. `:b` halves it, rounding up (101 x 51 became 51 x 26); a 1-pixel checker averaged to its mean (box filter, not nearest); one opaque red pixel among three transparent ones gave (63,0,0) over black, so the average is of premultiplied pixels, and the odd edge divides by four too. `:b` keeps a PNG's alpha; `:c` drew it opaque, its colour as if over black |
| Ponscripter text size and advances | Black box: runs of H at window font sizes 20, 23, 30 and 40 advanced by 14.5, 16.5 (16 and 17 in turn), 21.5 and 29.0 pixels, and `i` by 6.0 at 23: each is the glyph's advance at twice the size, rounded down, halved. A reference model (FreeType without hinting at twice the size, those advances, a 2x2 box filter) matches a captured line to a mean error of 0.085 of 255 |
| No kerning | Black box: `AVAVAV...` and `AAA...VVV`, `ToTo...` and `TTT...ooo` are the same width in Noto Sans CJK and Nimbus Sans, though both fonts carry GPOS pair kerning (stb_truetype reads it; the runtime doesn't apply it) |
| Ponscripter baseline, line height and `br` | Black box: lines at window sizes 20, 23, 30 and 40 with line spacing 2, 2, 4 and 0 were 64, 73, 97 and 119 pixels apart on the doubled screen, the font's ascender-to-descender height there rounded up plus twice the spacing; each `br` moved 34, 39, 53 and 60, half that height rounded up plus twice the spacing; the baseline sat the font's ascender, at the game's size rounded up, below the line's top |
| Ponscripter word wrap | Black box: wrapped paragraphs at 25 and 26 columns of 23; a word that won't fit within columns times the font width starts a line, without its space; a word longer than the line breaks where it fills it |
| Shadow and bold | Black box: on grey, a shadow offset of 3 on the doubled screen matches `shadedistance 3,3` (1.5 pixels on screen), 1 and 2 or 4 and 6 don't; the bold flag left every pixel the same |
| Quotes and ligatures | Black box: ``a'b `c' ``d'' "e" f--g h---i j...k fi fl ffi ffl ff`` matched the model only with U+2019, U+2018, U+201C and U+201D for the quotes, and plain letters, dashes and dots |
| Faces and `~tags~` | Black box: `~i~`, `~b~` and `~ib~` drew in faces 1, 2 and 3 (Noto Bold in the games), `~c5~`, `~c6~`, `~c7~` in faces 5, 6, 7; an unclosed `~b~` lasted into the next line; `~c4~` then `~b~` drew face 6, so `~cN~` clears italic and bold. With no `h_mapfont`, faces were read from `fonts/face<n>.ttf` (a copy of face 0 under that name made text appear) |
| `:s` sprite sizes | Black box: `/30,30,0`, `/16,16,0`, `/8,15,1`, `/46,23,0` and `/23,46,0` drew H at ems of 15, 8, 7.5, 11.5 and 23, so the second number is the em on the doubled screen, and `/8,15,1`'s pitch of 1 added half a pixel per character. Each glyph hangs from its own face's ascender: a `~c4~` sprite sat 4 pixels higher than face 0 text |
| Leading full-width space | Black box: the PC engine draws `langjp　聴診器…` a full-width space in from the line's start |
| `tachistate` | Black box: 0,0,0 with no standing images, 1,0,0 after `ld l`, 0,0,1 after `ld r` and `cl l` |
| `endroll` | The games' calls (`endroll 0,0,640,480,0, 0,640,480,2,91,9350` after `btndef ":b;…end_1c.jpg"`, an image of 640 x 9830 once halved: 9350 is its height less the screen's) and a black box: a numbered 2000-line image scrolled up a pixel at a time; (speed, distance) of (10, 500), (20, 500), (10, 1000) and (40, 1000) took 502, 1004, 1002 and 4008 ms, distance times speed over ten; a click at a second in left 6012 ms of 6000. Very small speeds took longer (the PC's own frame time) |
| `drawclear`, `drawsp`, `draw` | NScripter's manual (`追加命令一覧.txt`), and a black box: `drawsp` places a cell's top left at its coordinates on the game's screen, a cell number past the last draws the whole image, and alpha 128 draws it half through |
| `gettext` and `logsp` | Black box: `gettext` after a page `ab'c ~ib~d~ib~`, a line break, `br` and `e` gave 17 characters, so it keeps tags and line breaks; `logsp 5,$1,20,200,#FFFFAA` drew at (10, 100) on screen, bold where the page was, breaking at any character within columns times font width, its line breaks dropped. A literal string with tags drew them as text |
| Spaces outside carets | Black box: `%1 %2 %3` outside carets drew `000`, and a `:s` sprite without carets dropped every space |
| Masks given as image tags | The games' `effect 3,18,1300,":c;Bmp\Efe\left.bmp"`, whose files are `bmp\efe\left.bmp` in arc.nsa |
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
- **`NSTimer`**: the notes describe it as the system's millisecond tick. The runtime returns the wall time since the
  engine started, which never goes back and advances during a Lua call, so a timed Lua loop ends. The engine's own
  clock, which `gettimer` reads, moves only between frames, so it can't serve.
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
