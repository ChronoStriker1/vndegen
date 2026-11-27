# Smile Game Builder: where the runtime's knowledge comes from

VNDegen runs Smile Game Builder games on a runtime of its own, `Packages/VNDegenKit/Sources/CSmileGame`, written in C
and MIT licensed like the rest of the app. Nothing in it comes from the original player. The games' .NET player
(`sgb_rpgplayer.exe`, `common.dll`) and its core (`SharpKmyCore.dll`, mixed-mode C++/CLI, and the native `kmyCore.dll`)
were never disassembled or decompiled, .NET IL included. Every layout below was read from the games' own data files,
and every behaviour from running the original player under Wine as a black box.

## Sources

| Piece | Source |
|---|---|
| Package (`data.sgbpack`) | The game files: a ZIP whose first local header starts with `YUKARPKG`. Deflate by stb_image's zlib decoder (public domain or MIT) |
| `.sgr` container, resources, game settings, maps, events, scripts | The game files of Princess Colette's Great Adventure and Sacred, compared field by field across both games, with values checked against what the editor names show and what the player does under Wine |
| Pictures | PNG, by stb_image |
| Text | The game's own `font.ttf`, drawn with stb_truetype (public domain or MIT) |
| 3D models | Binary FBX 7.4, read with ufbx 0.23.1 (MIT or public domain, `third_party/LICENSE-ufbx.txt`) |
| Material blending | The `.def` text file next to each model (`mtl`, `blend modulate`, `depthwrite false`) |
| Map look, camera, message window, title screen | Captures of the original player under Wine (Wine 11.21, .NET Framework 4.8 from winetricks), compared with the runtime's frames |

## Package

`data.sgbpack` is a standard ZIP archive with one change: the first eight bytes, which in a ZIP are the local header
signature, version and flags, read `YUKARPKG`. Every other local header and the central directory are standard. Entries
are stored or deflated, never encrypted. Names use backslashes; some carry the UTF-8 flag. The player opens the package
with write access and touches its modification time, but never changes its contents.

The package holds the game's `.sgr` data, `res/` (PNG, Ogg Vorbis, binary FBX), `font.ttf`, and one folder per
resource pack under `samples/` (`samples/common` is the editor's built-in set; DLC packs have a `dlcinfo_en.xml` whose
`<guid>` names them).

## `.sgr` files

A header of `YUKAR`, an `int32` format number (6, or 10 for `ResourceItem.sgr`), the bytes `01 00 07 00 06 00 00 00`,
and for format 10 one more `int32` 0. Then records to the end of the file:

| Field | Type |
|---|---|
| size | `int32`, the body's length |
| sub | `uint8` |
| type | `uint8`: 1 game settings, 2 hero, 3 map, 4 item, 5 skill, 6 hero class, 7 event (sub 0) or script (sub 16), 8 monster, 9 effect, 17 resource (sub is the resource kind), 18 a resource pack reference |
| body | size bytes |

Bodies are .NET `BinaryWriter` data: little-endian integers and floats, strings as a 7-bit length prefix and UTF-8,
GUIDs as 16 bytes in .NET order. Every body starts with the record's GUID and its name.

Princess Colette (player 1.8.1) and Sacred (player 1.0.6.26) use the same format: all their maps, events and scripts
read with one reader, and their game settings match field for field through the title screen.

## Resources (`ResourceItem.sgr`)

GUID, name, path, then fields by kind. A path is relative to the package root for the game's own file and to the pack's
folder for a file under `samples/`; a `dlc:<guid>|` prefix names the pack by its GUID, `sys:` the built-in pack.

| Kind | What | Extra fields |
|---|---|---|
| 1 | BGM, Ogg | `bool` loop |
| 2 | sound effect | |
| 3 | chip set | |
| 4 | character: a PNG sheet or an FBX model | `int32` frames, frame time in ms, a flag, `int32` 1 when the sheet has four direction rows, a count of extra motions, then the motions as nested records |
| 5 | icon sheet | |
| 6 | monster picture | |
| 7 | picture | |
| 8 | window frame | four `int32` borders (nine-slice), a tiling flag |
| 9 | effect | |
| 17 | map object, FBX | a collision list |
| 19 | face folder | |
| 20 | ambient sound | |
| 21 | battle background | |
| 22 | terrain chip, PNG | an attribute byte |
| 23 | 3D item model | |

A 2D character sheet with four rows has its frames side by side; the rows face away, toward, left and right. In the
built-in sets a frame is 48 by 64 pixels, and 48 pixels are one map cell.

## Game settings (`GameSettings.sgr`)

GUID, game title, party (`int32` n, n hero GUIDs), 8 bytes, start x, start z, start map GUID, the switch names and the
variable names (each a count, then `int32` index and string), six window GUIDs (the first is the message window), 2
bytes, 8 inn and shop strings, the element names (count and strings), a GUID, 50 menu strings, a GUID, 23 battle
strings, 10 GUIDs of sounds and windows, 16 bytes, the title overlay picture GUID, 1 byte, the text colour (`#RRGGBB`),
28 bytes, the title picture GUID, the title BGM GUID, 16 bytes, the three title menu strings, the title menu layout
(`int32`, 0 a column, 1 a row), 4 placement bytes, the title text colour, 8 bytes, the title cursor GUID, the logo GUID,
and more interface strings and layout. The last two `int32` are the screen size, 960 by 544 in both games.

## Maps (type 3)

| Field | Type |
|---|---|
| GUID, name | |
| format | `int32` 6 |
| group | string, the map's folder in the editor |
| saved | `int64` .NET ticks |
| events | `int32` n, then n × (`int32` x, `int32` z, event GUID) |
| encounters | `int32` rate, `int32` n, n × (`int32`, `int32`, GUID) |
| | 16 bytes, `int32`, GUID |
| size | `int32` width, `int32` depth |
| heights | `int16` per cell, row by row; one unit is one cell high |
| chips | `int16` per cell, an index into the chip list |
| objects | `int32` n, n × (`int32` x, `int32` z, `int32` direction, map object GUID) |
| stairs | `int32` n, n × (`int32` x, `int32` z, `int32` direction, `int32`) |
| light | two ARGB colours as four `int32` each; the first lights the map |
| environment | a map GUID, the BGM GUID, 48 bytes, `int32`, 2 bytes |
| chip list | `int32` n, n × (`int32` index, chip GUID) |
| rest | camera and display settings, not read yet |

A chip PNG is 48 pixels wide: the top 48 rows are the cell's top face, the next 48 the side of the topmost unit, and
the 48 after that, when present, the sides of the units below.

## Events (type 7, sub 0)

GUID, name, `int32` sheet count, then per sheet: `int32` condition count and per condition eight `int32` (kind,
comparison, index, value, then four more), the sheet's name, the graphic GUID (a character or a map object), `int32`
direction, 18 bytes of movement and display options, the script GUID, and a motion name string. After the sheets come
the editor template's settings as text, not read.

Conditions seen so far: kind 1 compares variable `index` with `value`; kind 0 tests switch `index`, which must be on
when `value` is 0 and off when it is 1. A switch index from 65536 up is the event's own local switch. Of the sheets
whose conditions hold, the last one is active.

## Scripts (type 7, sub 16)

GUID, a byte, `int32` trigger (1 runs by itself, 2 on talking, 3 on touching), `int32` command count, the commands, and
2 bytes. A command is `int32` type, `int32` nesting depth, `int32` argument mask, and the arguments. The mask holds
2 bits per argument from the lowest bits up: 1 an `int32`, 2 a GUID, 3 a string. When the mask's top four bits are
`0100`, a second mask word follows, and its arguments come first. All 1960 scripts of both games read to their last
byte with this rule.

| Type | Command | Arguments as seen |
|---|---|---|
| 9 | wait | ms, flag |
| 20 | move to map | map GUID, x, z, direction |
| 21 | play BGM | GUID, volume, tempo, flag |
| 22 | play sound | GUID, volume, tempo |
| 23 | erase picture | number, flag |
| 25 | show picture | number, picture name, … |
| 29 | message | text, window, flag |
| 30 | choices | count, the choices, flag |
| 31 | set variable | target, index, operand kind, value, operation |
| 32 | set switch | index, value |
| 43 | message with faces | text, position, flag, left face, expression, right face, expression, speaker |
| 44 | screen caption | text, position, scroll flag (or text, position, picture, scroll flag) |
| 51 | screen on or off | 1 black, 0 shown |
| 60 | fade | ms, 0 in or 1 out, wait |

A message page ends at a blank line or after three lines. Round 2's section below has the rest of the commands.

## Event commands (round 2, commands)

Everything here was read from the two games' scripts with separate tools and checked against the original player under
Wine, playing Colette from the title to the bridge guard and Sacred from the title to the guard at (35,30).

### Event sheets in full

Each condition is eight `int32`: kind, comparison, index, value, and a GUID in the last four.

| Kind | Tests |
|---|---|
| 0 | switch `index` (65536 up: the event's local switch); `value` 0 means on, 1 off |
| 1 | variable `index` against `value` |
| 2 | money against `value` |
| 3, 5 | the count of the item named by the GUID against `value` (both kinds appear with items) |
| 4 | the hero named by the GUID is in the party |

Comparisons: 0 equal, 1 not equal, 2 at least, 3 at most, 4 more than, 5 less than. Colette's throne door uses 2 and 3
with its key (the PC opens it once the key is held), its castle stairs use 3 and 2 with variable 0.

The 22 bytes after the graphic GUID are `int32` direction, `int32` fixed direction, a byte that is 1 on sheets with a
graphic, an `int32` from -3 to 3, a byte that is 1 on map objects such as doors and chests, `int32` movement (0 still,
1 random walk, 2 toward the player) and `int32` speed from -3 to 3. Only the direction is used so far: an event whose
direction is 1 and not fixed turns to the player when talked to and turns back afterwards (Colette's aide does, the
bridge guard with direction 3 does not).

### Triggers

The script's trigger: 1 runs once when its sheet becomes active (on entering the map, or later when a switch changes),
2 on talking from the cell in front, 3 when the player steps onto the event's cell or walks into it, 4 when the event
touches the player (Colette's roaming monsters), 5 runs again each time it ends while its sheet stays active (Colette's
dungeon keeps offering its three choices until one leaves), 6 runs alongside play (one Sacred event). Talking reaches
one cell further when the cell in front is higher than the player's, as with Sacred's queen behind the dais step.

### Blocks

Branch commands open a block: the commands after them sit one nesting level deeper. Type 71 at the header's level
starts the else part and 72 ends the block. Choices (30) put the first choice's commands right after the header and
each later choice after a 74 whose argument is its number. Branches: 46 switch (index, 0 on or 1 off), 47 variable
(index, value, comparison 0 equal), 48 item (item GUID, count, a flag, 1 for at least), 50 hero in party (hero GUID,
0 in), 65 battle (its first part runs on a win, the else part on a loss), 37 shop and 38 inn.

### Commands

| Type | Command | Arguments | What the PC does |
|---|---|---|---|
| 10 | turn event | direction code | turns the running event |
| 11 | move event | direction code, cells, 0, 1, a flag, no-wait flag | walks the running event cell by cell |
| 14 | event graphic | graphic GUID, motion name | changes the running event's graphic until its sheet changes |
| 16 | turn player | direction code | |
| 17 | move player | as 11 | Colette's stairs push the player back up two cells |
| 20 | move to map | map GUID (zero: this map), x, z, facing | facing 0 keeps, 1 up, 2 down, 3 left, 4 right |
| 23 | erase picture | number, fade flag | |
| 24 | change picture | number, opacity %, ms, … | |
| 25 | show picture | number, picture GUID, opacity %, colour (-1 plain), 1, x, y, -1, fade flag | full-screen pictures centred |
| 27 | effect | 0 the event or 1 the player, effect GUID | effect from `Effect.sgr` |
| 29 | message | text, position (0 top, 1 middle, 2 bottom), flag | dark `default` window, white text |
| 31 | variable | target 0, index, operand 0 constant or 1 variable, value, operation 0 set, 1 add, 2 subtract | |
| 32 | switch | index, 0 on or 1 off | |
| 33 | item | item GUID, 0, count, 1 add or 2 remove | |
| 34 | money | 0, amount, 1 add or 2 remove | |
| 42 | party | hero GUID, 0 join or 1 leave | the first member is the player's graphic |
| 43 | message with faces | see above | light `message` window (the settings' window), black text |
| 44 | screen caption | text, position, scroll flag | static captions are centred with an outline and wait for a key; scrolling ones rise at 30 pixels a second |
| 45 | balloon | 0 the event or 1 the player, effect GUID | about a second each, one after another |
| 51 | screen | 1 black, 0 shown | |
| 52 | hide event | 1 hide, 0 show | Colette's monsters hide on a win |
| 53 | camera | mode, then values in thousandths: mode 0 pitch, yaw, distance, field of view, ms, wait; mode 3 adds a target x, y, z first | |
| 59 | recover party | none | "全員のHPとMPが回復した" follows it |
| 60 | fade | ms, 1 out or 0 in, wait | covers the map and pictures, not windows |
| 61 | screen tint | ms, ARGB colour, wait | |
| 62 | shake | ms, strength, wait | |
| 63 | flash | ms | |
| 69 | hero graphic | hero GUID, face folder, graphic GUID (zero: the hero's own), motion | applies when the hero leads the party |
| 21, 22, 64 | BGM, sound, ambient sound | GUID, volume, tempo, … | |

Direction codes for 10, 11, 16 and 17 are 0 up, 1 down, 2 left, 3 right; Sacred's castle scene ends with the player and
the soldier where these codes put them. 10 and 16 also take 4 to 7, and 11 a one-argument form; their meaning is not
known yet.

### Messages

The window is 720 by 160 pixels at x 120; position 2 puts it at y 351, position 0 at y 33. Text is the game's font at
24 pixels, 36 pixels a line, three lines a page, from 18 pixels in and 19 down. Lines break at 684 pixels, and a closing
mark or small kana never starts a line: Colette's aide's "…くださいっ。" breaks before "っ". 43's faces are the face
folder's `_normal`, `_smile`, `_anger` and `_sorrow` pictures for expressions 0 to 3 (a folder with one picture uses it
for all), drawn at the bottom-left and bottom-right corners behind the window; when both sides have a face, the one
not speaking is darker. A choice keeps the message before it on screen; choices sit in a centred `default` window with
a `default_selected` bar under the cursor.

### Map object collision

A map object resource ends with its collision list: `int32` 32, `int16` count, and per cell `int16` x and z (32 is the
object's own cell), `int16` height and `int16` 0 or 1, then 4 bytes. The height is added to the map's height under the
object, turned with the object's direction. The player walks only between cells of equal height (or onto stairs), so
Colette's town bridges (height 1 over water at height 1) join the grass at height 2, and trees and houses block.

## Saves

`savedata\Save_NN.sgs` in the game folder: a `YUKRDATA` header, then plain `BinaryWriter` data. Layout not read yet.
