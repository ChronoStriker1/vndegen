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
- **Permissive libraries**, used as libraries: stb_image, stb_vorbis, pl_mpeg (see `Engines/NScripter/UPSTREAM.md`).

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

## Open questions and how they were settled

- **Background images smaller than the screen**: drawn centred. To be confirmed on the original engine.
- **`:a` images with several cells**: each cell is its picture followed by its mask, as a test game's two-cell button
  image shows (black and white halves alternate).
- **Width-1 SPB images**: the packer writes an extra junk column for them. No game image is one pixel wide, so the
  runtime doesn't special-case them.
- **Movies with a 59.94 fps header**: one test game's MPEG-1 clips hold 29.97 frames a second (ffprobe counts 88
  frames in 2.92 s), so drawing 30 a second is their full rate.
- **Compression types 2 (LZSS) and 4 (NBZ) in `.nsa`**: no test game uses them and no sample exists to check a
  decoder against, so they aren't implemented. The runtime logs the file instead.
