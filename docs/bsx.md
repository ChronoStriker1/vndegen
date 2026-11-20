# BISHOP BSX

VNDegen runs BISHOP's BSXX engine ("BSXScript") games on a runtime of its own, `Packages/VNDegenKit/Sources/CBSX`,
written in C from the games' files, GARbro's MIT-licensed archive and picture readers, and the original engine run
under Wine. It is MIT licensed like the rest of the app. `docs/bsx-cleanroom.md` lists where each part came from. No
other runtime, decompiler or format write-up for the engine exists.

## Test games

| Game | Script | Notes |
|---|---|---|
| Mesu Kyoushi ~Injoku no Kyoushitsu~ Immoral Edition (English machine translation, `mkiedl.exe`) | BSXScript 3.0, 800x600 | Boot logos, title, opening and new game, the week's mornings, special class, the map, the target cards, events, choices, eyecatches, voices, music, ambience and effects, the message window and its menu bar, STRIP and VIEW, Save and Load, quick save and load, auto saves, backlog, settings, the item screen |
| Mesu Kyoushi 2 ~Injuu no Rasen~ Immoral Edition (English machine translation, `mk2iedl.exe`) | BSXScript 3.0, 800x600 | Title, story to an ending in the headless runner, events with faces and overlays, Save and Load both ways with the PC, the Mac app, iPad touch and gamepad, a case-sensitive library |
| Mesu Kyoushi 4 ~Kegasareta Kyoudan~ (English machine translation, `mk4.exe`) | BSXScript 3.1, 1024x768 | Title, the opening and staff roll movies, story to an ending in the headless runner, the map, events, Save and Load both ways with the PC (scenes, event pictures, close-ups and the map), the Mac app, iPad touch and gamepad, a case-sensitive library |
| Mesu Kyoushi 3 ~Inetsu no Gakusha~ Immoral Edition (English machine translation, `mk3ie.exe`) | BSXScript 3.3, 1366x768 | The opening movie, title, story to an ending in the headless runner, the map and its search map, achievements, Save and Load both ways with the PC (scenes, bust-ups and the map), the Mac app, iPad touch and gamepad, a case-sensitive library. The test copy's `GraphicsEV03.bsa` doesn't match the release's checksum in `setup.xml`: 16 of Shino's event pictures in it are zeroed and draw nothing |

## How the games are built

A game folder holds the exe (`mkiedl.exe`), `boot.exe` (which the exe needs to be started from on the PC),
`bsxx.dat` (the compiled script), `bsxx.cfg` (the engine's screen and layout settings, Shift-JIS text), `bsxx.ini`
(UTF-16: the product id, the title and the hero's name), archives (`Graphics*.bsa`, `Voice*.bsa`, `bgm.bsa`,
`bgs.bsa`, `se.bsa`, `sesys.bsa`, `define.bsa`), `map.dat`, `cg.dat`, `scene.dat` and `save/`. The plugin takes a
folder as a BSX game when `bsxx.dat` sits beside `bsxx.cfg` and a `Graphics*.bsa`, up to one folder down.

- Archives (`BSArc`): a version, a count and an index offset. Versions 1 and 2 use 40-byte records (a 32-byte name, the
  offset, the size), version 3 12-byte records (a name offset, the offset, the size) with a name pool. `>name` and `<`
  records open and close folders. A `.link` entry holds another entry's name. The runtime looks names up without their
  folder or extension.
- Pictures: `BSS-Graphics` (BGRA, BGR or 8-bit, stored bottom-up, raw, per-channel RLE, or per-channel LZ with a
  running delta) and `BSS-Composition` (several such parts on a canvas, each with its offset). Bust-ups are a body
  composite and a face composite on an 800x600 canvas, or a taller canvas for close-ups. The message window's
  composite holds its parts: the shape, which is tinted with the settings' window colour, the border and the name
  plate. Hit tables are 8-bit pictures without a palette whose values are region numbers.
- `bsxx.dat`: `BSXScript 3.x`, six header words (the stack size, the local count, the choice count, the global
  count), the system variable count, the code offset and size, the message count, fourteen offset and size pairs (the
  label table, label names, the string table, variable names, character names, the message table and messages, all
  UTF-16), and the system variable bindings. Nothing is encrypted. Translations rewrite the message pool.
- Bytecode: one-byte opcodes. Operands are six bytes, a type (number, string index, local, global, argument,
  temporary) and a 32-bit value. `12` assigns; `0b` to `11` compare and `13` to `1a` compute into a temporary, `T0`
  being the flag; `04` jumps, `05` and `06` jump on the flag; `03`, `09` and `0a` go to, call and return from a label;
  `08` sets where a click skips to; `1b` and `1c` add and subtract one; `1d` shows a message, with a speaker and a
  voice; `33` and `34` add and show choices; `36` marks scene replay ends; `38` marks a label. `1e` to `37` are
  commands that take their arguments from the argument registers set just before.
- Commands: `1e`/`1f` put a picture (a background, an overlay, a bust-up or a clickable object) on the next screen,
  `21` puts an event picture's face, `22` removes a picture, `23` moves one or plays a `define.bsa` animation, `24`
  multiplies the next picture, `26` shows or hides the window, `27` and `28` change to the next screen with a fade or a
  mask, `2b` flashes, `2c`/`2d` play and stop music, `2e`/`2f` ambience, `31`/`32` effects, `35` waits for time, a
  click or animations, and `37` runs the engine's own screens: `title`, `map`, `custom map` and the STRIP and VIEW
  switches.
- `map.dat` (`BSX MAP INFO r3`): the time variable and the map's time slots, icon names, the places with their icon
  layouts, and for each heroine and item the place and icon per progress step and slot. The map returns the pick in
  `#character`.
- `bsxx.cfg` lays out every engine screen: the message window, the menu bar, choices, the title, the map, the target
  cards, Save and Load, the slot viewer, settings, backlog, the item screen and the museum.

## BSXScript 3.1

Mesu Kyoushi 4 changes these from 3.0:

- A message's voice is a count and that many voice names, so a line can be voiced by two characters. `3e` plays
  voices the same way without a message. `41` calls a named function of the exe (`InitBeginTimeParameters()`); the
  runtime logs it and goes on.
- `30` and `31` start and stop a character's looping voice, and `2e` and `2f` start and stop looping effects by name,
  several at once. `2a` shakes the screen: a new random offset of up to one and a half times its arguments every frame
  for half a second. `26` also switches the menu bar (1) and the information window (2), which hides during event
  pictures. The `op` and `staffroll` system commands play `movie/op.mpg` and `movie/staffroll.mpg` (MPEG-1 with MP2
  sound, through the shared `vnvideo`); a click skips them.
- `bsxx.cfg` is UTF-8 (the translators' copy) and the runtime reads either encoding. Faces are a face part and a strip
  of eye frames (`eye pattern`), drawn one frame at a time with a blink every `eye anime interval`. `[bu color
  adjustment]` tints bust-ups over a named background: `*` multiply, `+` add, `-` subtract, `^` colour dodge, `/`
  colour burn and `&` linear burn, with an alpha for the strength. The message window is one tinted picture, the date
  sits in its own information window, the window counts down to ejaculation from `#x_count`, and text is placed one
  font height apart, lowered by the height less the size.
- The map is the custom map, drawn by the script: kind-160 icons whose `[button object]` values go to `#select_value`.
  The menu bar shows on it with SKIP and VOICE disabled, and it auto saves as it opens. FACE, BUST and HIP pick VIEW's
  three positions, FACE by default.

## BSXScript 3.3

Mesu Kyoushi 3 adds these to 3.1:

- `3d` writes the bitwise NOT of its operand to a temporary, which the script ANDs with event flags to clear a bit.
- The exe embeds Lua 5.2 for a simulation screen set that Mesu Kyoushi 3 doesn't use. `embed.bsa` holds `boot.lua`
  and `day_result.lua`, written for another BISHOP title's characters; `boot.lua` loads five more modules that aren't
  in the release (its `embed.bsa` matches `setup.xml`'s checksum), so the PC skips them, and its story, map and
  achievements run natively. The script's two `41` calls fill Lua tables that nothing reads; VNDegen logs and skips
  them. A save stores those tables as a length-prefixed line of Lua table text, which VNDegen writes as the PC does
  and skips on load. No Lua interpreter is needed.
- Achievements: `実績解除 IDnn` sets the entry's flag in `common.dat`'s achievement table (a count, then each entry's
  name and a 32-bit flag, before the choice table) to 1. The status screen (DATA) lists `define.bsa`'s
  `achievement.db` with `status_layout`'s font (in points), columns, colours and scroll bar, and hides locked secret
  entries. `達成率` sets the museum's completion rates, which count once the museum exists.
- The slot viewer draws the week number inside its unit picture and the weekday, or the special-day picture before
  the first day. Slots are numbered from 0. Slots 0 to 24 are auto saves.
- From 3.1 on, the glyph size follows the font size (14 gives 20 pixels) and the font height is the line pitch. The
  menu bar's lamps are drawn under its buttons, which are text over the lamp diamonds in Mesu Kyoushi 3. Where
  `bsxx.cfg` repeats a key, its last value counts.

## Saves

The PC engine writes `save/noNNN` (slots; 0 to 14 are its auto saves, made when the map opens and at choices),
`save/no-01` (quick save) and `save/common.dat` in the game folder. VNDegen uses the same files:

- A slot is a 32-character id, a thumbnail (the screen times the cfg's `thumbnail ratio`: 120x90, 128x96 for
  Mesu Kyoushi 4, 204x115 for 3), the slot header (date, time, day, time slot, the current text with
  its speaker), the play head (the mode, 7 on the map, then each character's wear record: a count, and per character
  its max wear plus one and that many bytes, from `bsxx.cfg`), the program counter after the current message, the call
  frames (return address and the caller's label), locals, globals, the last 50 message positions, the music, and the
  pictures on screen (kind, name, position, the picture's own size from its file, its frames, parts, opacity; an
  overlay's record ends with 29 more bytes). A background has two parts only with a face on it. After the list come
  three flags for a hidden window, menu bar and information window. The mode is 1, 7 on the map phase, or 9 on a
  custom map that is the game's map.
- The id is the same in every save of a game and is checked on load. It is read from the game's `save/` folder; the
  four Mesu Kyoushi games' ids are also built in.
- `common.dat` holds the PC's settings (with the same wear records), a byte per choice taken, a byte per message read,
  and the museum's records. VNDegen finds the choice and read tables by their counts, updates them, and leaves the rest
  as the PC wrote it.
- 3.3 adds the messages shown and the Lua table line after the message history.
- A save the original made under Wine loads in VNDegen, and a save VNDegen made loads in the original with its
  pictures.

## Known limits

- The museum (BGM, CG, scene replay and bust-up viewer) isn't there yet. Its title button does nothing.
- Mesu Kyoushi 3's and 4's dress screen (STRIP) isn't there; the button does nothing. Their settings screens lack
  BGM SELECT, the per-character voice details and the BGV volume.
- Mesu Kyoushi 3's `sync` system command is logged and skipped; it comes right before a startled line and its effect
  isn't known. Its two completion-rate achievements wait for the museum.
- The space a translation puts after `「` is drawn; Mesu Kyoushi 4's PC drops it.
- Settings are VNDegen's own `save/vndegen.cfg`, not the PC's settings in `common.dat`. The full-screen, effect detail
  and auto-select options are shown but do nothing.
- The cut-in and voice replay modes aren't there; a voice plays once.
- The free-sight meter (dragging a close-up bust-up) isn't there; VIEW moves between the three fixed positions.
- `ActionType2` (before the staff roll) is ignored.
- A game without any save to take its id from, other than the four Mesu Kyoushi games, writes saves the PC engine
  refuses.
