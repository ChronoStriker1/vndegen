# EAGLS

VNDegen runs Tech-Arts' EAGLS ("Enhanced Adventure Game Language System") games on a runtime of its own,
`Packages/VNDegenKit/Sources/CEAGLS`, written in C from the games' files, GARbro's MIT-licensed archive reader, the
engine maker's game files and the original engine under Wine. It is MIT licensed like the rest of the app.
`docs/eagls-cleanroom.md` lists where each part came from. No other runtime for the engine exists.

## Test games

| Game | Engine build | Notes |
|---|---|---|
| Oppai no Ouja 48 ~Nani mo Kangaezu Me no Mae no Oppai Zenbu Shabure!~ (English fan release, `MILK48.exe`) | EAGLS 1.5, 2008 | Title, prologue movie, story with text, voices, music and sound, characters, choices, name entry, the message window's wheel and system menu, Save and Load, backlog, settings, the animation settings page |

The game is 800x600.

## How the games are built

A game folder holds the exe (`MILK48.exe` here), `EAGLS.dll`, `EAGLS.ini`, `CG/CGPACK.pak` and `.idx`,
`Script/SCPACK.pak` and `.idx`, `Wave/` (music as loose `01.wav` to `10.wav`, and `WAVEPACK.pak` and `.idx` for
voices and effects), `MOVIE/` and `Save/`. The plugin takes a folder as an EAGLS game when `Script/SCPACK.idx`,
`Script/SCPACK.pak` and `CG/CGPACK.idx` are there, up to one folder down.

- An index (`.idx`) is XORed with a fixed key string picked by the C runtime's `rand`, seeded from its last four
  bytes. Its entries are a name, an offset and a size; offsets count from the first entry's.
- Pictures in this game are plain BMPs: 24-bit, 32-bit with alpha, or 8-bit. Click maps are BMPs whose grey level is a
  button's id.
- Scripts (`.dat`) have a 3600-byte label table, then Shift-JIS source text with every second byte XORed with
  `EAGLS_SYSTEM` picked by `rand`, seeded from the file's last byte. The text is compiled when a file is first used:
  - `$label`, `&N"text"` (a message, which waits for a click and replaces the last one), `#Name` or `#Name=voice` (the
    speaker and the voice for the next message), `(e)` is a line break.
  - `N(args)`: one of about 70 numbered commands. Arguments are numbers, quoted strings, `TRUE`/`FALSE`, and variables;
    a quoted string that starts with `_`, `@` or `:` names a string variable.
  - `_X=a,b,+,` assigns a reverse Polish expression (`+ - * / %`, `=` equal, `!` not equal, `> <`, `&`, `|`).
  - `120,n(cond){...}n;` if, `122` else if, `121` else, `123` loops until its condition holds, `124()` breaks,
    `125()` continues. A loop that goes round without waiting waits a frame at its end, as the PC's polling loops need.
  - Variables: `_` the game's own (arrays of 100), `@` global across games (seen CGs and scenes, `@Reco_...`), `:`
    system (`:SkipFlg`, `:AutoFlg`, `:RightClickFlg`, `:NameSuffix`, options). Names ignore case.
  - `41`/`42`/`43` goto, call, return; `80`/`81`/`82` start, stop and ask about a script thread (the shaking effects
    and the blinking cursor run beside the story).
- Drawing: slots hold pictures (`0` loads or frees), layers show a slot's rectangle scaled to a screen rectangle with an
  alpha (`19`, `20` for the background), `2` and `8` copy between slots (`8` only where a click map has one colour,
  which is how hover highlights work), and the screen changes at `3()`, at a layer drawn with its last flag set, or
  through a transition (`4`).
- Text: `150` defines fonts (the em size, character spacing and line gap); messages use font 0, drawn in grey with a
  one-pixel black outline, as the PC draws MS Gothic without antialiasing.
- Sound: `100(name, mode, channel, fade)`. Music crossfades between channels 0 and 1, effects use 2 to 4, voices 6.
  Names with a path (`WAVE\03`) are loose files; others come from `WAVEPACK`, `.ogg` then `.wav`.
- Movies: MPEG-1 (800x600, some with MP2 sound) played into a slot by `61`/`62`, polled with `68`.

## Saves

The PC engine writes `Save/SaveN.sav` (slot N+1, 80 slots) and `Save/GlobalSave.sav` in the game folder, and VNDegen
uses the same folder:

- A slot is a 128-byte banner, two LZSS-packed copies of the engine's 1.38 MB state block (now and at the previous
  message), XORed with the index key, and a seed byte. The block holds the variables in the engine's own hash tables,
  the script position (the byte offset of the current message's name line), the slot pictures, the layers, the window
  parts, the music, the text and a 127x95 thumbnail.
- VNDegen reads the original's slots: they list in VNDegen's Load screen with their thumbnails and load on the same
  message with the variables, background, characters, window and music.
- VNDegen writes the same container with the fields above, followed by its own exact state, which the PC ignores.
  VNDegen's slots load in the PC game on the same line, with the background, characters, window, name and text.
  Each layer's record goes in the PC's per-layer block (0x5E00 bytes from 0x9B580; the background's record twice,
  with its picture size, and a character's in a second table as well), the fonts from the script's `150` definitions,
  the backlog ring's header and counters, and the name plate's fixed geometry. The PC fills in its own handles.
- `@` globals (seen CGs and scenes) are read from the PC's `GlobalSave.sav`. VNDegen keeps its settings and the `@`
  and `:` variables in `Save/VNDegen.cfg`, and its quick save in `Save/VNDegenQuick.sav`.

## What runs

- The logos, the title with its hover buttons, New Game, Load Game, CG View, Scene View, Soundtrack, Settings and Exit Game.
- The prologue movie, backgrounds, characters, emotes, transitions with mask pictures, shaking.
- Messages with the PC's layout, speaker plates, voices, the wait cursor, click to finish or advance, right click to
  hide the window, the wheel to open the backlog, Ctrl to skip.
- The window's wheel: settings, log, hide, skip, auto, quick save, quick load, and the system menu (save, load,
  animation settings, champion brassiere page, return to title, exit).
- Save and Load pages with ten pages of eight slots, thumbnails, the NEW mark, Save Confirm and Load Delete, and the
  confirm dialogs; the backlog with speaker names, its scroll bar and voice buttons; name entry; choices.
- The map that picks the next heroine, from the game's own scripts and click maps.
- CG View: five pages of the game's own sheets (`CGMode1-N`, its `C` thumbnails, `S` highlights and `CGModeMask`). A
  slot shows its thumbnail once any of its pictures has been seen (`@xa01`, `@xa01r1` and so on). Clicking it shows
  each seen version full screen in turn; the tall group pictures show their top, then their bottom.
- Scene View: four pages (`SCMode1-N`), one slot per scene, shown once `@Reco_<scene>` is set. Clicking a slot plays
  the scene from its `$Reco_<scene>` label with `:SCENEFLG` set; the scene's closing `140()` returns to `TitleSC.dat`'s
  `$SceneStart`, which opens the page again. The slot order isn't in the scripts: it was read off the thumbnail sheets
  by matching each thumbnail against the game's pictures.
- Soundtrack: the ten tracks, play, stop, previous, next and the volume bar, at the PC's places, drawn opaque as the PC draws
  them.
- Settings: VNDegen draws a dialog with the PC dialog's Text and Sound options (text speed, auto speed, white text,
  skip unread, wait for voices in auto, volumes and mute), standing in for the PC's Windows property sheet.

Games start on the "Visual novel" controller layout.

## Known limits

- The title's locked "?????" bonus and the window's Oppai Gallery and Oppai View buttons, which the story turns on
  once the scouter is found, do nothing: they are screens built into the game's exe.
- In Scene View a scene's choices are the player's to make, as in the story.
- The champion brassiere page shows every brassiere as not found.
- Transitions other than crossfades and mask wipes (`4` with effects 3, 5, 8 and 10) are drawn as crossfades.
- The settings dialog's Screen, System, Info and Machine tabs aren't there; full screen is the app's.
- Name entry takes letters, digits and spaces from a hardware keyboard; without one, the default name stays.
- VNDegen doesn't write `GlobalSave.sav`, so CGs and scenes seen in VNDegen don't show as seen on the PC.
