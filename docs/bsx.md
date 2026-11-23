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
  voice; `33` and `34` add and show choices (below); `36` marks scene replay ends; `38` marks a label. `1e` to `37` are
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
- Choices: `33` takes an id, a label and a message, with the design in the first argument register and a value in
  the second. A pick writes the value to the variable `bsxx.dat`'s system-variable table binds to 0x10303e8
  (`#select_type`) and goes to the label when the design isn't negative. Mesu Kyoushi 4's choices all have negative
  designs and read `#select_type`; its second field is then the value again, or -1. The system-variable table pairs
  engine ids with script variables (0x1030000 `#day`, 0x1030001 `#time`, 0x103000a `#select_value`).
- `bsxx.cfg` lays out every engine screen: the message window, the menu bar, choices, the title, the map, the target
  cards, Save and Load, the slot viewer, settings, backlog, the item screen and the museum.

## BSXScript 3.1

Mesu Kyoushi 4 changes these from 3.0:

- A message's voice is a count and that many voice names, so a line can be voiced by two characters. `3e` plays
  voices the same way without a message. `41` calls a named function of the exe (`InitBeginTimeParameters()`); the
  runtime logs it and goes on.
- `30` and `31` start and stop a character's looping voice (BGV, at `[default volume]`'s `bgv` level), and `2e` and
  `2f` start and stop looping effects by name, several at once. `2a` shakes the screen: a new random offset of up to one and a half times its arguments every frame
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
  entries. `達成率` sets a variable to the museum's CG or strip completion rate; at the title the script unlocks ID50
  and ID46 when they reach 100.
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
- A game folder without `save/common.dat` gets the one the PC makes when it first quits: the id; the settings block
  (50 bytes in 3.0, 51 in 3.1 and 3.3) with the text and auto speeds at 0.5 and `[message window]`'s `bg color`; the
  wear records, each `chN displayed wear` byte set; 3.3's achievement table (a count, then each `IDnn` of the
  achievement db with its flag clear); the choice and message tables cleared; the persistent variables, the first
  0xffffffff and the rest 0; the CG and scene records cleared; 3.3's Lua table `{}`; the volumes from `[default
  volume]` (master, BGM, BGM under a voice, SE, BGS, voice, BGV, as floats); the voice mask; each `voiceNN` id with its
  voice and BGV volume; the voice and BGV volume of every other speaker, three bytes of 1 and ten empty playlists.
- Settings are the PC's, in `common.dat`, so they move with the saves. In the settings block, the text and auto speeds
  are floats at 0x35 and 0x39 (1 minus the slider's place) and the window colour B, G, R, A at 0x3d. In 3.0 the effects,
  cease skip, unread skip and voice mode bytes are at 0x4c to 0x4f and auto select at 0x51; in 3.1 and 3.3 effects
  are bit 0x80 of 0x48, cease skip, unread skip and voice mode at 0x4a to 0x4c, Mesu Kyoushi 4's BGM select at 0x4d
  and auto select at 0x52. The volumes are the final gains (a BGM slider at 35% plays at 0.35 times the master). The
  voice mask has a bit per settings character, the speakers' volumes scale their voices, and the BGV room's
  per-character volumes are the same BGV levels. VNDegen's own `save/vndegen.cfg` keeps only the x-count switch, which
  has no place there yet; its older settings are taken over once, when VNDegen first makes `common.dat`.
- The voice modes, as Mesu Kyoushi 4's PC plays them under Wine with music muted: 0 stops a voice when the line is
  clicked on; 1 repeats it until then (once in auto or skip); 2 and 3 let it play on through the following lines
  until another voiced line starts.
- The settings screen has the BGV slider (`volume5`), Mesu Kyoushi 4's BGM select (`bgm selector pos0` and `pos1`) and
  each character's 詳細 button, which opens `[detailed sound setting]`: the character's voice on or off and its voice
  and BGV volume (the other speakers' for 他). With BGM select at マイルド (1), a track `define.bsa`'s `bgm_select`
  table lists in its first column plays as the second (`bgm06` as `bgm11`); which column is which was read from the
  table's order and the radio's stored value, not heard on the PC.
- 3.3 adds the messages shown and the Lua table line after the message history.

## Museum

The title's museum button opens `[museum phase]`. Its mode buttons are on when `@museum_state` has their `modeN
state` bit. `@museum_state` is the first of `bsxx.dat`'s persistent variables (sections 4 and 5), which
`common.dat` keeps after the read table as a count and 32-bit values; the PC writes 0xffffffff.

- `cg.dat` lists the CG pages: a page count, then per page a thumbnail count, per thumbnail a variant count, and per
  variant a picture count, that many 32-bit picture ids and a tail byte (two between variants). A name table follows:
  a count and NUL-terminated UTF-16 picture names. A name may end in `:` and tags: `c` marks a cut-in, `s` a breath
  or heat-haze layer (Mesu Kyoushi 4's `t` and `n` pictures). `scene.dat` is CSV: page, index, replay label, an empty route and
  the pictures that make its thumbnail.
- `common.dat` keeps, after the persistent variables, a count and a byte per `cg.dat` name (seen) and a count and a
  byte per scene (open). A picture is seen when it is put on screen; a scene opens once all its pictures are seen.
  VNDegen reads and writes these bytes in place, so the museum shares them with the PC. The strip rate counts the
  wear records' bytes. The rates are the seen variants, the open scenes and the set wear bytes out of all, in percent,
  which the PC confirms: all set unlocks Mesu Kyoushi 3's ID46 and ID50, one wear byte clear keeps ID46 locked.
- Thumbnails are the variant's pictures composed and scaled to the slot size, blurred where a `<picture>m` mask is
  white, and drawn at half brightness unless the pointer is on them. A page's slot frames come from the mode's `base`
  composition, one part per thumbnail that exists.
- The CG viewer steps through the seen variants; a click past the last one closes it, a right click hides its panel.
  Its panel (`[cg viewer]`) has the arrows, the number, cut-in (hides `c`-tagged pictures and pictures whose name is
  another's plus `c`), hair (uses `<picture><hair suffix>` when the game has one), Mesu Kyoushi 4's steam (hides
  `s`-tagged pictures), BGV volume and the BGV panel. A button picture whose first frame is one flat colour has that
  frame as its hit mask, and its states start at the second.
- Scenes play now or go into one of ten playlists (`[scene playlist]`). The playlists sit at the end of `common.dat`:
  a count of 10, per list a count and 12-byte entries (page, index and route), then the selected list. A replay runs
  the scene's label until `36 -1`, without auto saves, and comes back to the museum's scene page, or plays the
  playlist's next scene.
- BGM plays the cfg's tracks with the music volume. BGV (Mesu Kyoushi 3 and 4) lays out `bgv_layout` from
  `define.bsa`: per character a scrolled list of loop voices, which play as the story's `30` loops do, with their own
  volume and stop, and the overall BGV volume.
- The strip mode (立ち絵鑑賞) is the bust-up editor `[museum phase - bu]` names, `museum_buex_layout` in `define.bsa`.
  Its tabs pick a background (the pictures matching `[main window]`'s `bg signature`, three pages, or any seen CG),
  play BGV (Mesu Kyoushi 3 and 4), set the characters up and store states. Each character can be shown, reordered
  (earlier in the list is in front), moved along the position bar and switched through its rows: pose, view, undress,
  outfit, underwear and face, always in that order whatever the layout's comments say. A bust-up is named
  `bu<no><f|u><undress level><outfit><underwear+1><a|b><face>` from the layout's lists; an option whose body picture
  doesn't exist is drawn off, and an outfit change picks the first underwear that exists. A close-up (`u`) is centred
  by its composition's canvas width and scrolled by the free-sight meter: in 3.1 and later the picture's top at sight
  0 and `H + top + sight * (height - H)` lined up with the face position, in 3.0 the face position at the top. The
  face, bust and hip buttons set the sight from `[free-sight screen]`'s fixed positions. With a character on screen
  the background is its `sfx bg` version and `[bu color adjustment]` tints the bust-ups. Mesu Kyoushi 3's hair-off
  and cut-in, and Mesu Kyoushi 4's steam, apply to bust-ups and CG backgrounds.
- Editor states are the PC's own `save/buex_dsNN.dat`, shared with the PC: the thumbnail (bottom-up BGRA at the
  thumbnail size), then the background (group, page, index), `--order--` with a count and per character its number,
  shown (0) or hidden (2), position (a float, -1 to 1), 6 and the six row values, `--mesgwnd--` and `--freemsg--` kept
  as they are, and `--buinfo--` with the flags (hair-off 0x20, cut-in 0x08), the sight, the level and the wear table.
  Clicking an empty slot saves; a saved slot offers replace, load and output slot. VNDegen reads and rewrites every
  state the PC wrote under Wine byte for byte.
- A save the original made under Wine loads in VNDegen, and a save VNDegen made loads in the original with its
  pictures.

## Known limits

- The bust-up editor's message tab (文章設定: names, free text, balloons and the colour picker) is off, and its image
  output (画像として保存する) does nothing; states keep their messages as they are. Its output slots last until the
  editor closes. Mesu Kyoushi 4's ahegao-off buttons (viewer and editor) are drawn but do nothing, and its steam
  setting isn't stored in a state. In 3.0 the free-sight scale past the face position is VNDegen's own.
- Mesu Kyoushi 3's and 4's dress screen (STRIP) isn't there; the button does nothing.
- Mesu Kyoushi 3's `sync` system command is logged and skipped; it comes right before a startled line and its effect
  isn't known.
- The space a translation puts after `「` is drawn; Mesu Kyoushi 4's PC drops it.
- The full-screen, effect detail and auto-select options are stored as the PC stores them but do nothing; the BGM
  volume under a voice is stored and not applied.
- Cut-in in the story hides the pictures the museum viewer's cut-in hides (`c`-tagged in `cg.dat`, or another
  picture's name plus `c` while that picture is shown); it wasn't watched in a story scene on the PC.
- Voice mode 2 (until the same speaker's next line) is played as mode 3; on the PC both carried a voice through an
  unvoiced line, and how mode 2 overlaps two speakers' voices wasn't measured.
- The free-sight meter (dragging a close-up bust-up) isn't there; VIEW moves between the three fixed positions.
- `ActionType2` (before the staff roll) is ignored.
- A game without any save to take its id from, other than the four Mesu Kyoushi games, writes saves the PC engine
  refuses.
