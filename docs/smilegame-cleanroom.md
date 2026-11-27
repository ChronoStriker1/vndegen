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
| Ogg Vorbis | stb_vorbis (public domain or MIT, `third_party/LICENSE-stb.txt`) |
| Text | The game's own `font.ttf`, drawn with stb_truetype (public domain or MIT) |
| 3D models | Binary FBX 7.4, read with ufbx 0.23.1 (MIT or public domain, `third_party/LICENSE-ufbx.txt`) |
| Material blending | The `.def` text file next to each model (`mtl`, `blend modulate`, `depthwrite false`) |
| Saves, config, music levels and loops | Saves the original wrote under Wine, edited saves it loaded, and its audio recorded from a PulseAudio null sink |
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
graphic, `int32` step speed from -3 to 3, a byte that is 1 on map objects such as doors and chests, `int32` movement
(0 still, 1 random walk, 2 toward the player) and `int32` step frequency from -3 to 3 (round 3 found which is which;
see "The field" below). The direction is the event's facing when its sheet becomes active, in the save's codes (0 up,
1 down, 2 left, 3 right): Sacred's training soldiers (3) face right in the PC's saves. An event whose direction is 1
and not fixed turns to the player when talked to and turns back afterwards (Colette's aide does, the bridge guard
with direction 3 does not).

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
| 24 | change picture | number, zoom %, ms, 1, colour, 1, x, y | moves zoom, colour and position over the time |
| 25 | show picture | number, picture GUID, zoom %, colour (-1 plain), 1, x, y, -1, fade flag | pictures centred, then offset by x, y |
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
| 37 | shop | n, n item GUIDs, n prices when present, flag | a branch: runs when something was bought ("まいどあり" follows it) |
| 38 | inn | price, 1, 1 | a branch: runs when the party stayed |
| 53 | camera | mode, then values in thousandths: mode 0 pitch, yaw, distance, field of view, ms, wait; mode 3 adds a target x, y, z first | the last argument 0 makes the event wait: Sacred's 5000 ms move into the king's room ends before "ごほっ" appears |
| 59 | recover party | none | "全員のHPとMPが回復した" follows it |
| 60 | fade | ms, 1 out or 0 in, wait | covers the map and pictures, not windows |
| 61 | screen tint | ms, ARGB colour, wait | |
| 62 | shake | ms, strength, wait | |
| 63 | flash | ms | |
| 69 | hero graphic | hero GUID, face folder, graphic GUID (zero: the hero's own), motion | applies when the hero leads the party |
| 21, 22, 64 | BGM, sound, music effect | GUID, volume, tempo, … | 21 with a zero GUID stops the BGM; 64's resources are ME_item_get, ME_lodging and fanfares |

Direction codes for 10, 11, 16 and 17 are 0 up, 1 down, 2 left, 3 right; Sacred's castle scene ends with the player and
the soldier where these codes put them. 10 and 16 also take 4 to 7, and 11 a one-argument form (round 3, below).

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

## 3D rendering (round 2, delegate D)

Everything here was read from the two games' map records and resources, then checked by rendering the same cell as
a PC capture under Wine and comparing them side by side (`sgview` in the Linux host draws a map at a cell).

### Map header and tail

The 36 bytes after the encounter list start with an `int32` background: 0 draws the built-in noon sky
(`samples/common/res/system/Sky noon - 01`, `Cloud noon - 01` and `Sun noon - 01`, models centred on the camera), 2
leaves the background black. Sacred's field uses 1, which the PC camera can't be tilted far enough to show.

After the chip list:

| Field | Type |
|---|---|
| slope chips | `int32` n, n × (`int32` index, chip GUID): the stair and slope textures this map uses |
| | `uint8` |
| outside chip | GUID: the chip drawn around the map |
| outside height | `int32` |
| sun | `int32` 1 on |
| sun pitch, yaw | `float` each, radians; both games use -1.2 and -0.7 |
| sun colour | three `int32` |
| | 4 bytes, `uint8` |
| camera kind | `int32` |
| has camera | `uint8`; when 1, an `int32` byte count and that many bytes of `float` |

With camera kind 0 the eight floats are pitch, yaw, an unused 0, field of view, distance, -1, 0, 0 (Sacred's castle:
-3, 5, 0, 70, 3.5, -1, 0, 0). Kind 1 holds two floats (Colette's event rooms: 27.5 and 1.7). `GameSettings.sgr` holds
the default map camera as the same eight floats after an `int32` 32: Colette -49, 0, 0, 70, 3.0, -1, 0, 0 and Sacred
-48, 0, 0, 70, 3.5, -1, 0, 0.

Fitting renders to PC captures pinned how those numbers make a view: the eye looks at a point one unit above the
player's feet (the -1), from twenty times the stored distance, with a vertical field of view of a tenth of the stored
value (7°). Carpet edges in Colette's throne room then land within a pixel of the PC's. Yaw turns the other way from a
right-handed reading, for the camera and for the sun.

### Stairs and slopes

Each record in the map's stair list is `x`, `z`, an index into the slope chip list, and a shape:

| Shape | High corners |
|---|---|
| 0, 1, 2, 3 | south, north, east, west edge: a ramp, or three steps when the chip is a stair chip |
| 4, 5, 6, 7 | one corner: south-east, north-west, north-east, south-west |
| 8, 9, 10, 11 | three corners, low at north-west, south-east, south-west, north-east |

The shapes were read from which neighbours stand one unit higher, over all 2,356 records in both games. A slope rises
one unit from the cell's height.

### Chips

A chip's attribute byte (after its path in `ResourceItem.sgr`): low nibble 1 natural ground, 3 built ground, 4 a liquid
(sea, lava, ice), 5 field, 7 poison swamp; 0x11 a stair chip, 0x21 a slope chip. Liquids are 384 pixels wide, eight
48-pixel frames side by side, and sit a quarter unit below their cell's height. A stair chip's top 48 rows are the
treads seen from above, the next 48 the risers, and the last 48 the side of the flight.

### Light

The two map colours are a key and a fill. Measured on lit and shadowed floor, wall fronts and grass in both games,
a texel shows as texture × (0.77 × fill + 0.74 × key × sun colour × N·L), with N·L zero in the sun's shadow. The PC
darkens the foot of walls a little more than this (it looks like ambient occlusion); the runtime doesn't.

### Models and motions

A model's `.def` gives each material's blending (`blend modulate` for shadow quads, `blend add` for glows,
`depthwrite false`) and, after `anim`, named frame ranges at the scene's frame rate (`walk 1 90 loop`) for models that
carry their motions. Characters without ranges keep their motions as separate files in a `motion` folder beside the
model, `motNNN_name.fbx` (an `o_` before the name is dropped), each sharing the model's bone names. Map objects rest in
their `wait`, `keepClose`, `close` or `lightOff` motion. Texture names that end in `.psd` load the `.png` of the same
name.

### Characters on the map

- 2D sheets step through frames 1, 0, 1, 2 at the sheet's frame time, walking or standing (a 30 fps recording of the
  PC), and stand a quarter cell toward the camera from the cell's centre.
- 3D characters face south at rest and turn by quarter turns; they play `wait` standing and `walk` moving.
- Where map geometry hides the player, the PC draws its silhouette in white at about 40%, one layer deep.
- 3D characters cast shadows from the sun; 2D sheets don't. The round blob under characters appears only on maps
  without a sun.

## Saves (round 2, E)

Learned from saves the original player wrote under Wine at known states (a fresh start, one cell moved, each of the
four facings, after opening a chest and an urn, after rotating and zooming Sacred's camera, after changing the
volume), and checked by editing a save, loading it in the original and reading what its menu shows. The original
loaded every edited and every VNDegen-written save, and a save it wrote back after loading one differed only in play
time (and, in Sacred, in the positions of townspeople who wander).

`savedata\Save_NN.sgs` in the game folder, NN from 00, 8 slots a page over 5 pages. The slot list shows the file's
modification time as the save date; the date is not stored in the file. `settings.dat` is named in the player but was
never written in either game: the config screen's values (BGM and sound volume, message speed, cursor memory) are kept
in each save and reset to the defaults when the player starts again.

The file is `YUKRDATA`, an `int32` 0, then three sections, each an `int32` byte length and its body.

**Party.** Heroes in the party: `int32` n, n × (`int32` size, hero). Heroes out of the party: the same. Items: `int32`
n, n × (item GUID, `int32` count). `int32` money. Items ever obtained: the same as items. Changed hero looks:
`int32` n, n × (hero GUID, face GUID, graphic GUID).

A hero is 188 bytes in player 1.8.1 and 160 in 1.0.6: hero GUID, then `int32` level, max HP, max MP, HP, MP, power,
vitality, an `int32` the PC writes as 0 for every hero seen (read as magic, though the hero tables give 10 and 12),
speed, experience (Sacred's level 2 hero has 26 here, as its status page shows), one more, six equipment GUIDs
(weapon, shield, head, body, two accessories), an `int32` not yet read (0 in every save seen), and in 1.8.1 28 bytes
more. Heroes that leave the party stay in the second list with their stats; a new game's party holds only the settings' non-empty slots. The original shows these values as stored and does not recompute them: a
save with power 77 showed 82 with a +5 knife, and HP 99 over a maximum of 20 showed as 99/20.

**Flags.** `int32` 1000, 1000 switch bytes; `int32` 1000, 1000 `int32` variables; 22 config bytes (two flags, `int32`
BGM volume, `int32` sound volume, `int32` message speed, two `int32`); local switches: `int32` n, n × (event GUID,
`int32` index from 65536 for A, `bool` on); `float` play time in seconds; then 1 byte in 1.0.6, 6 bytes in 1.8.1.

**Map.** Map GUID, `int32` player x, `int32` player z, `int32` n, n × (`int32` size, event state) in the map's own
event order. An event state is the event GUID, its current graphic GUID, its motion name, `int32` direction, `float`
x, y, z (cell centres, so x + 0.5) and a byte. Then the player's `float` height, an `int32` (37 in every save seen),
the camera as six `float`s (0, pitch, yaw, distance, field of view 70, 0.9; Sacred's E/Q, R/F and C/V keys change
pitch, yaw and distance), three `float`s (a point Sacred's opening camera looks at, zero in Colette), a byte, the
pictures on screen (`int32` n, n × (`int32` size, body: number, picture GUID, scale, colour, position and flags)),
`int32` player direction, and 4 bytes.

Directions are 0 up, 1 down, 2 left, 3 right, in the player and in event states.

The default camera of a new game is in GameSettings: pitch, two zeros, 70, distance (Colette -49 and 3, Sacred -48
and 3.5).

## Data tables (round 2, delegate B)

Every table below was read from both games' files and the values checked against the PC player's menus under Wine
(status, equipment and item pages). Both player versions use the same layouts. Fields marked "inferred" fit every
record of both games but were not yet seen to change anything on the PC.

### Game settings beyond the title

After the six window GUIDs: 2 bytes, 8 inn and shop strings (stay, leave, not enough money, buy, sell, quit, bought
`{0}`, sold `{0}`), the elements (`int32` n and n strings: 無 火 氷 風 雷 聖 冥, then the states 毒 眠り 麻痺 即死 混乱
魅了), a GUID, the 50 menu strings in a fixed order (result, saved, overwrite question, quit question, the button guide
words, item and equipment labels, the menu items, LV HP MP, state names, the who-uses questions, なし はずす, the four
base stat names, exp, next level, game over), the battle BGM GUID, 23 battle strings, 10 sound GUIDs (victory ME, game
over ME, start, cancel, decide, cursor, item use, status up, two not identified), 33 bytes, the text colour, the title
block, はい and いいえ, the empty-world text, 13 config strings (the four message speeds, remember and forget, BGM,
sound effects, message speed, cursor position, reset, the config title, 属性攻撃力), `int32` n and n GUIDs (2 in
Colette, 0 in Sacred), the money and play time labels, 1 byte, 14 battle state messages, 3 GUIDs, 1 byte, and the save
slot name (セーブデータ). Camera settings follow.

### Heroes (`Hero.sgr`, type 2)

GUID, name, graphic GUID, `int32` start level, four stats as (`int32` base, `float` growth, `float` curve) in the
order 腕力 体力 魔力 素早さ, skills (`int32` n, n × (skill GUID, `int32` level learned)), equipment exceptions
(`int32` n, n × (item GUID, `uint8`)), five equipment GUIDs (weapon, shield, head, body, accessory), 2 bytes, battle
commands (`int32` n, n GUIDs), 16 bytes, description, face GUID, HP and MP as base, growth and curve, then 6 bytes.

A stat at level L is base + growth × (L − 1)^curve, truncated: Sacred's アリア at level 2 has HP 40 + 15 = 55 and
腕力 32, as the PC shows. 攻撃力 on the status page is 腕力 plus the weapon's attack, 防御力 is 体力 plus the armour's
defence, 素早さ is the stat plus equipment. An item in the exception list can't be equipped by that hero (Colette's
ダーク and アイスロッド, inferred from the editor's default of "everything").

Experience: the tables hold no curve. The totals seen on the PC are listed under Battles below.

### Battle commands (`Hero.sgr`, type 6)

GUID, name, description, icon sheet GUID, `uint8` icon column and row, `int32` kind (0 attack, 1 or 4 skill, 2 defend,
5 item), `int32` 100, `int32` 1, `int32` 50 or 70 (inferred: rate and priority), then zeros.

### Items (`Item.sgr`, type 4)

GUID, name, description, `int32` price, icon sheet GUID, `uint8` icon column and row (icons are 32 × 32 cells of a
512 × 512 sheet), `int32` kind, then an `int32` length and a block of that length, then for a weapon a second
length-prefixed block, then a model GUID (zero for most items).

| Kind | What | Block |
|---|---|---|
| 1 | expendable (60 bytes) | effect GUID, `int32` HP, MP, HP %, MP %, 20 bytes, five state cures (bytes), two usable flags (inferred: battle, field), revive |
| 2 | skill book (18 bytes) | 2 bytes, the skill GUID |
| 3 to 7 | weapon, shield, head, body, accessory (101 bytes) | 24 `int32` (0 defence, 2 max HP, 3 max MP, 4 attack bonus, 7 speed, 8 evasion, 9 hit %, 11–16 element resistances 火 to 冥, 17–22 state resistances: 17 poison and 22 instant death by the items' names), 1 byte, `int32` (inferred: critical %) |
| 3 | weapon, second block (29 bytes) | effect GUID, `int32` attack, 1 byte, `int32` element, `int32` element attack |

The PC's equipment page shows ナイフ as 攻撃力 5 and 古びた剣 as 攻撃力 10, the second block's attack.

### Skills (`Skill.sgr`, type 5)

GUID, name, description, icon sheet GUID, `uint8` icon column and row, then two length-prefixed blocks. The first
(21 bytes, 27 in one Colette skill) holds `uint8` usable in the field, `uint8` usable in battle, `int32` 0, `int32` MP
cost, `int32` target (0 one ally, 1 all allies, 2 one enemy, 3 all enemies, 4 the user; 5 and 10 seen once each). The
second (94 bytes) starts with the effect GUID and the `int32` power; the rest is left for the battle round.

### Monsters (`Monster.sgr`, type 8)

GUID, name, graphic GUID, attack effect GUID, description, 10 `int32` (the first is HP; the others, inferred: MP,
attack, defence, magic, hit %, two flags, evasion, speed), 13 `int32`: resistances to the six elements 火 to 冥, then
to the six states in the skills' order (毒 眠り 麻痺 混乱 魅了 即死), then a value of 0, 1 or 2 not yet read (negative
is a weakness), actions (`int32` n, n × (`int32` condition, two arguments, rate, kind (see Battles below),
skill GUID, `int32` priority)), then the money and the experience the monster gives, in that order (the PC's result
for ゾンビーナＬＶ01 showed 経験値 32 and 13 G, the table holds 13 then 32), two drops (item GUID, `int32` percent),
`int32`. All 91 monsters of both games read to their last byte.

### Effects (`Effect.sgr`, type 9)

GUID, name, then the 2D and 3D effect resources; only the name is read so far.

### Event commands seen by the menu

37 opens a shop: `int32` n, n item GUIDs (zero GUIDs pad Sacred's to 6), then in most uses n prices and a trailing
`int32`. 38 opens an inn: `int32` price, then flags. 42 and 50 add and remove a party member by hero GUID.

## Menus (round 2, delegate B)

Measured from captures of the PC player under Wine at 960 × 544:

- Main menu: a 208 × 440 panel at (60, 40) with eight 192 × 53 buttons (アイテム, スキル, 装備, ステータス, セーブ, ゲーム終了,
  コンフィグ, 閉じる), a 624 × 422 party panel at (328, 16) holding up to four 298 × 196 member boxes, and a 624 × 66
  panel at (328, 436) with the map name, money and play time. Panels use the built-in `default` window, buttons
  `default_selectable`, the cursor `default_selected`. Menu text is 21 px, member boxes 17 to 18 px.
- A face is drawn at 80 % in the member box, its source window starting at (60, 106) of the 320 × 544 bust, and at full
  size at (14, 10) on the status page; both found by matching the PC's pixels.
- Items, skills and shop lists are two columns of 298 × 44 entries with a やめる button at (490, 380); the bottom panel
  shows the description at 24 px, shrunk to fit. Pages that pick a member first show the member boxes with the cursor
  on a box.
- Equipment: the member on a 312 × 486 panel at the left with the six stats, six slots (weapon, shield, head, body, two
  accessories) with the `equip_icons.png` cells, and the comparison `25 → 20` when a candidate is chosen.
- Status: a 624 × 486 panel with the stats, equipment, exp and next level, the hero's description, and the face at the
  left.
- Save: 8 slots a page, 5 pages; a slot's date is the save file's modification time, which the PC shows for a file
  copied in by hand.
- The quit dialog (640 × 160 at (160, 192)) starts on いいえ; はい ends the PC player at once. The overwrite question starts
  on はい.
- Config: BGM and sound volume, message speed, cursor memory (on by default: the menu returns to the last item), reset.
- The bottom row shows the button guide from `button_guide.png` at 90 %.
- Opening the menu scales the panels in from about 85 % over roughly 0.2 s; the PC ran near 9 fps under Wine, so the
  time is approximate. The cursor jumps without animation. Esc or X opens the menu when no event runs.

## Audio (round 2, E)

BGM, ambient sound and sound effects are Ogg Vorbis; neither game ships WAV. A resource of kind 1 carries a loop flag.
Fifteen of Colette's and nine of Sacred's Ogg files carry `LOOPSTART` and `LOOPLENGTH` comments in samples; the
original honours them: recorded under Wine, Colette's title music runs to 102.25 s and goes on from 6.25 s, as the
two comments say. A map's environment holds, after its BGM GUID, an ambient sound GUID and a battle BGM GUID.

The title plays GameSettings' title BGM; entering a map plays the map's BGM. Recorded through the same capture path as
VNDegen's SDL runner, the original plays a BGM at volume 100 at half the amplitude of the decoded file (-6 dB), so the
runtime's output carries that level. Changing BGM cuts over with no gap.

Commands seen: 21 play BGM (GUID, volume, tempo, flag), 22 play sound (GUID, volume, tempo; tempo 75, 100, 125, 150
and 175 occur), 64 play a music effect (GUID, volume, tempo, flag; Sacred's item jingles). Colette's opening calls 21
with a GUID that two resource packs also use for a folder record (type 18); it is also the GUID of the BGM
`BGM_town_countryside` in `samples/common`, and that is what the PC plays (round 3, below).

## Battles (round 2, delegate C)

Measured from battles on the PC player under Wine: 69 of Sacred's practice battles (the guard at (35,30) on the castle
map, one スライミー, アリア at levels 2 to 5) and one Colette battle on the world map (ダーク and コレット at level 1
against a wandering ゴーレムＬＶ01, lost, with six tries at escaping). Everything not marked otherwise was seen on the PC.

### Command 65

`int32` n, n monster GUIDs, then three `int32`: continue after a loss (1 in the practice battle and some boss fights;
inferred: the event goes on to its `else` branch instead of GAME OVER), escape forbidden (1 in the practice battle:
逃げる is listed but choosing it does nothing), and one more (always 0). Then the battle BGM GUID (zero or an empty
resource reference for the default) and the background map GUID (zero for the default background, a flat grass
plane). Optional tagged groups follow: `1000, x, z` is the cell of the background map the battle is centred on (the
map's centre when absent), `1001` and one `x, z` pair per monster is each monster's cell relative to that centre
((0, −3) for one monster, (−2, −3) and (2, −3) for two). The commands nested one level deeper run after a win; the
`71` at the same level opens the branch for a loss or escape and `72` closes it.

### Screen

- The battle is drawn on the background map in 3D: a front view from behind the party at about 28° down, a long lens
  (about 19° of vertical view from 33 cells away), centred a little right of the monsters. Monsters stand three cells
  beyond the centre; the party stands 2.6 cells in front of it and 2.8 cells to the right, members 3.3 cells apart.
  These are a fit to the positions of ダーク, コレット and the golem in the PC's frame, within about 6 pixels.
- Monsters are their picture as a billboard at 0.63 of the map's 48 pixels per cell. 2D heroes use their character's
  motions (`wait`, `attack`, `damage`, `guard`, `KO`, `skill`, nested in the character resource: `int32` count, then
  per motion an `int32` size, GUID, name, file, frames, frame time, flag and divisions) at 1.29 × 48 pixels per cell;
  コレット faces the camera in her `wait` motion. A hero without motions (ダーク) shows the walk sheet's back row. 3D
  heroes (アリア) are their model from behind.
- The member choosing a command stands on a light green square and has a light row in the status list.
- Bottom left: `Command` over a rule, then the hero's commands from the hero table with their icons, then 逃げる,
  34 pixels apart; the cursor is a light blue bar. Bottom right: `Name`, `HP`, `MP` headers with a row of name, value
  and gauge per member.
- A dark bar across the top shows what is happening: モンスターが現れた！, the target's name while choosing one,
  `{0}の攻撃！`, the skill's name, `{0}は様子を見ている`, `{0}の会心の一撃！`, 逃げることができなかった！. Damage
  rises over the target in white digits; a miss shows ミス.
- Victory shows a result panel across the top (リザルト, 経験値, G, アイテム), then returns to the map. A wiped party
  fades back to the map darkened with GAME OVER, then the title screen.

### Rules

- Turn order: every living fighter acts once a turn in order of speed, highest first. The golem (speed 40) acted
  before コレット (16) and ダーク (15) in each round; アリア (33 and up) always acted before the slime (0).
- Attack damage is (1.5 × attack − defence) / 4 × a random factor, rounded, never below 0. The factor is uniform
  between 0.94 and 1.04: of 105 hits on the slime (defence 5), attack 42 gave 14 (16) or 15 (3), attack 54 gave 18
  (11), 19 (15) or 20 (6), attack 66 gave 22 (10), 23 (22) or 24 (16), attack 78 gave 27 to 29 (6). The same formula
  gives the slime's 2 or 3 against defence 35 and 0 against 45 and 55, ダーク's and コレット's 3 against the golem
  (defence 25), and the golem's 26 and 27 against ダーク (defence 15) and コレット.
- Hit chance is the attacker's hit % minus the target's evasion: monsters 90 to 95, heroes their weapon's hit (95).
  アリア (hit 95) missed the slime 3 times in about 155 attacks.
- Critical hits (`{0}の会心の一撃！`): 8 in about 155 of アリア's attacks with a weapon whose critical field is 10, so the
  runtime uses half that field as the percentage (inferred). The damage number was hidden behind the slash effect;
  the runtime ignores defence on a critical hit (inferred).
- Skill damage: the golem's 落石ＬＶ01 (power 20) took 21 from コレット and missed ダーク twice. The runtime uses
  (power + magic) × the factor − defence / 4 (one sample, inferred). Healing skills restore power + magic (inferred).
- Escape: chosen by one member, it is tried at once and ends the party's turn whether or not it works; on a failure
  the monsters still act. Against the golem (speed 40, party 15 and 16) all six tries failed. The runtime uses 50 % ×
  party speed / monster speed, between 5 % and 100 % (inferred).
- Defending (身を守っている): アリア at 1 HP defended through three of the practice slime's turns and kept her 1 HP,
  where its normal hits do 2 or 3, so defending more than halves damage; the runtime doubles the defender's defence
  (inferred). A charge (力をためている) doubles the next attack (inferred).
- Monster actions: the table's entries are (`int32` 36, `int32` turn (0 every turn, 1 odd turns, 2 even turns,
  inferred from Sacred's slimes), `int32` 0, `int32` weight, `int32` kind, skill GUID, `int32` 50). Each turn one
  entry is drawn by weight. Kinds: 0 attack, 2 watch (`{0}は様子を見ている`: the slime has four attacks and one
  watch at equal weight and watched 6 times in 33 turns), 3 skill, 4 charge, 6 run away (`{0}は逃げ出した！`). A skill
  the monster has no MP for is not drawn. Targets are random among living members (inferred).
- Rewards: the monsters' experience and money (the slime: 経験値 5, G 5, as the table says), each drop rolled at its
  percentage. Each living member gets the full experience: ダーク and コレット both reached level 2 on the 32 points
  of one ゾンビーナ.
- Experience totals for each level, from アリア's status page and the battle in which her HP rose: level 2 26, level 3
  84, level 4 between 182 and 186, level 5 350, level 6 600. Colette's status pages (経験値 plus つぎのレベルまで) with
  the runner's saves loaded in the PC add level 7 966, level 8 1484, level 9 2196 and level 10 3150. The runtime uses
  these (184 for level 4) and 3.7984 × L^2.5 × 1.101^L + 3.05 above level 10, a fit that matches all nine within 4.
  The PC shows the level these totals give, not the level a save holds: a runner save that called メイリ level 9 at
  1984 points showed level 8 there before this table.
- A level-up raises the maximum HP and MP and leaves the current values as they were: after that ゾンビーナ battle
  the PC's menu showed ダーク at 24/45 and コレット at 12/41, the HP each had at the end of the battle.
- Elements and states: see Battles from the story below.

## Joining the pieces (round 2, G)

Checked under Wine against the PC player, both games.

- One party: a save made by VNDegen at Colette's bridge guard and at Sacred's guard at (35,30) loaded in the PC at
  the same cell, with the same party, items, money and hero looks in its menu, and the PC's own save made there
  afterwards has the same party, reserve, items, obtained list and look list as VNDegen's. VNDegen loads that save
  back at the same cell.
- A new game's hero stats come from the hero tables at the hero's start level; the PC's own new-game saves give the
  same values (Colette's Dark 32 HP, 10 MP, power 20, vitality 10, speed 15; Sacred's starting hero 50, 20, 25, 20, 10).
- Command 69 adds or replaces the hero's entry in the save's look list (hero, face, graphic), whichever hero it names.
- Events stand on top of map objects, as the player does: Sacred's queen sits on her throne two units above the floor,
  and the king lies on his bed, both in their event sheet's motion (`chair`, `bed`). After loading the castle save
  the PC shows both so, and runs no event.
- Camera moves glide the whole view, the target too: Sacred's opening goes from the courtyard over the wall into the
  king's room.
- The save page of the menu shows the slot's date, map name and each hero's name, face, level, HP, MP and state in the
  same box the main menu uses, at the place the PC draws it.
- A saved picture's body is `int32` number, `int32` 0, picture GUID, `int32` zoom, the colour, a byte, `int32` x,
  `int32` y, a shown flag and `int32` -1 (round 3 decoded it; see "The field" below).

## Battles from the story (round 3, H)

Seen on the PC player under Wine in Colette's first world-map battle (ダーク and コレット against a wandering
ゾンビーナＬＶ01, from a save made at the bridge guard after the dungeon scene in which コレット joins), Sacred's
practice battle, and 18 random encounters in Sacred's first dungeon, 王家の洞窟. Audio was recorded from the PC
through a PulseAudio null sink and matched against the game's Ogg files by cross-correlation.

### Starting and ending

- Command 65 runs inside the event: the commands before it (Colette's monsters say くっ、モンスターめっ！ with a face)
  show first, the battle runs, and a win goes on with the commands nested under 65 (Colette's monsters hide with 52
  and set their switch). A loss with the continue flag, and an escape, take the 71 branch: on the PC, アリア at 1 HP
  fell to the practice slime's 3, the battle ended at once with no 全滅した message, and the event went on to recover
  the party (59) and say お疲れ様でした. A loss without the flag ends at GAME OVER: the PC fades the
  `res/system/gameover.png` picture in over the map. What happens to a wandering monster after an escape was not
  seen.
- Colette's world-map monsters are events with trigger 4 that walk towards the player; which set wanders is chosen by
  variable 9, which command 70 (in the world map's entrance scripts) sets at random. On the PC a ゾンビーナ (variable
  9 = 2) reached the player on the bridge below the castle.
- Background: the command's map, else the current map's battle background, which is the GUID read as the map's
  environment (`env` in `sg.h`): `Default` (a flat grass map) for Sacred's castle, `Cave` for 王家の洞窟, the map
  itself for some. Battle maps use look value 2 for their sky; the PC draws a blue noon sky over them.
- Music: the command's BGM, else the map's battle BGM (the third GUID after the map's BGM: BGM, ambient sound, battle
  BGM), else GameSettings' battle BGM (BGM_battle1 in both games). The PC starts it 0.5 s after the key that closes the
  last message, as the screen fades. At a victory the BGM stops and the victory music effect (ME_fanfare1) plays once;
  the result screen is then silent. After the battle the map's BGM goes on from where it was: Colette's world-map
  track resumed at 41.4 s, about the time it had played before the battle.
- Entry: the field flashes white and fades to black, the battle fades in while the camera pushes in from further out,
  and モンスターが現れた！ shows in a centred rounded box (dark, white border) that folds away before the first command.
- Victory: the camera turns to face the party from the monsters' side, a little above them and about 1.9 times
  closer than the battle view; 3D heroes play their `win` motion; the result panel is two bars across the top
  (リザルト, then 経験値 and G) with アイテム and each dropped item (icon and name) below on the open sky. No
  level-up text shows there or after it.
- During a turn the acting monster flashes white, and the camera pushes towards the actor (not done yet).

### Encounters

A map's encounter table (after its events: `int32` rate, `int32` n, n × (`int32` 1, `int32` 1, monster GUID)) starts
battles from steps. The rate is a step count, not a chance per step: 18 encounters in 王家の洞窟 (rate 40) came after
44, 49, 35, 39, 37, 32, 36, 40, 52, 39, 51, 47, 34, 49, 49, 34, 48 and 42 key presses walking a corridor back and
forth, up to three of each against its end wall. The runtime draws the next encounter between three quarters and five
quarters of the rate (inferred from that spread). Of 21 encounters there, the 10 with ミニゴーレム had one each and
the 8 with スライミー had one (3) or two (5); how the count is chosen, and what the two 1s in each entry mean, is not
known (every entry in both games is 1, 1), so the runtime brings one. All showed the `Cave` background. Colette's maps
have no tables; its monsters are events.

### Skills, states and drain

A skill's second block: effect GUID, `int32` power, `int32` 0, `int32` (values 1 to 13, not read), seven `int32` 0,
then at byte 56 six state bytes in the order 毒 眠り 麻痺 混乱 魅了 即死 (毒牙 sets the first, スリープ the second,
麻痺にらみ the third, パニックボイス the fourth, 魅惑のキッス the fifth, デス・ストライク and レイズ the sixth;
キュアー sets the first five), bytes 62 to 85 (呪い 25 and ファイアガード 35 at byte 62, not read), `int32` element
at 86 (氷 for ブリザード) and `int32` hit % at 90 (毒牙 90, スリープ 68). Byte 14 of the first block marks a drain
skill (the 吸血 skills). On a friendly skill the state bytes cure, and the sixth revives.

- 毒牙ＬＶ01 (power 0) took 1 HP and poisoned コレット and later ダーク; 吸血ＬＶ01 (power 0) took 1 HP from ダーク and
  the ゾンビーナ gained 1. The runtime gives a damaging skill that hits at least 1 (two samples).
- Poison takes a tenth of maximum HP, rounded down, after the poisoned fighter's own action, with
  `{0}は毒を受けている！`: コレット (29) lost 2 and ダーク (32) lost 3 each turn. Poison stays after the battle: the
  menu shows 状態: 毒, and the PC's save holds 1 in the hero's `int32` after experience. The field shows the state's
  icon over the leading hero.
- A state shows as a cell of `icon_system.png` over the fighter (poison: column 7, row 1; sleep, paralysis,
  confusion and charm at columns 2 to 5 of the same row). The game settings' three icon references after the
  battle state messages are 逃げる's icon and two menu icons, not states.
- Not seen yet, so inferred in the runtime: whether a state lands (100 − the target's resistance) %; sleep and
  paralysis skip the turn with the game's messages and end with a one-in-three chance at each turn; confused
  fighters attack anyone, charmed ones their own side; only poison outlasts the battle.

### Effects

An effect (`Effect.sgr`) names its sheet, its sound (Weak_Hit_3D for 01打撃小) and a frame count; the sheet's
resource record ends with its grid after the path (01_Weak_Hit 5 × 1, 02_Medium_Hit 4 × 2, 03_Strong_Hit 4 × 3).
Attacks use the weapon's effect (a monster's: its attack effect), skills and items their own. The runtime plays the
cells in order over the frame count at 30 frames a second, centred on the target at 0.6 of the cell size, with the
sound at the start; sizes and timing were matched by eye. Each effect also holds a keyframe timeline (positions,
scales, a flag byte with optional colour, which looks like the screen flash the PC shows for 吸血) that is not read.

### 3D heroes

A 3D hero's model folder has `motion/motNNN_<name>.fbx` clips: wait, attack, skill, item, damage, guard, charge,
ko, win, escape (アリア's c_023a). The PC plays attack while the hero strikes and win at the result; the runtime
plays them for the clip's length.

### Still inferred

Critical hits (half the weapon's critical field, defence ignored), the skill damage formula, healing (power +
magic), the escape chance, defend and charge, the state chances above, and the recovery of sleep, paralysis,
confusion and charm, and the monster count of a random encounter.

## The field (round 3, delegate I)

Learned by black-box experiments on the PC player under Wine: a copy of a game's package with one event's scripts or
sheet bytes replaced (re-zipped with the `YUKARPKG` magic, which the player accepts), loaded from a PC save, then the
PC's own save read back or the window filmed at 30 frames a second. No player code was read.

### Event movement

- Sheet movement 1 walks one cell in a random direction, 2 one cell toward the player along the longer axis (the other
  axis when blocked). Blocked steps are skipped and tried again after the next pause. Events walk while other events
  run, except the running one.
- Step speed (the `int32` before the map-object byte): 0 crosses a cell in about 0.25 s (4 cells a second, the
  player's walking pace), -3 in 0.77 s, 3 in under 0.1 s. In between VNDegen uses 1.33, 2, 2.8, 4, 5.5, 7.5 and 10
  cells a second for -3 to 3.
- Step frequency (the last `int32`): the pause after a step is about 1.05 s at 0, 1.85 s at -3 and none at 3; VNDegen
  interpolates linearly.
- A chaser (movement 2) whose trigger is 4 starts its event when it walks into the player: Colette's world-map
  monsters, which then say "くっ、モンスターめっ！" and start their battle.
- In a save, an event that has started walking has motion `wait`, as E and G saw for Sacred's townspeople.

### Direction and position commands

From saves written after patched scripts (aide at (15,6), player at (14,6), Colette's throne room):

| Command | Code | Effect |
|---|---|---|
| 10, 16 | 4 | a random direction |
| 10, 16 | 5 | toward the player (10) or the running event (16) |
| 10, 16 | 6 | away from it |
| 10, 16 | 7 | the direction does not change; Sacred spins characters with four in a row, drawn as a quick turn |
| 11 (one argument) | 4 to 6 | one step in the direction 10 would turn to (Sacred's pushable boxes use 6: away from the player); blocked steps only turn |
| 11 (one argument) | 7 | no step |
| 15 | x, z, event GUID (zero: this event), code | places the event at x, z; code 0 keeps its facing, 1 up, 2 down, 3 left, 4 right |

### Other commands

- 66 `int32`: 1 clears and 0 sets the second config byte of the save, the one that allows random encounters
  (Sacred's royal cave turns them off while the hero is alone and on again after the fairy). The menu and dash still
  work with it cleared.
- 70 `int32` 0, variable, 1, low, high, 0: sets the variable to a random value from low to high inclusive (twenty
  draws of 0 to 4 on the PC gave every value and nothing else). Colette picks its world-map monster set with it.
- 56 hero GUID, skill GUID, 0 learn or 1 forget. A hero's save entry holds, at byte 156, `int32` n and n skill GUIDs
  it has learned outside its table, in the order learned (level-ups append there too); the entry grows by 16 bytes a
  skill (1.8.1's 28 extra bytes come after the list).
- After 56 teaches a skill the PC shows "ロイは「ヒール」を覚えた" (hero, 「skill」, を覚えた) in the dark message
  window and waits for the key (filmed in Sacred's royal cave).
- 21 with a zero GUID stops the BGM at once (recorded on the PC: silence within 0.1 s).
- 53 mode 2 (view angle, eye height, ms, wait), seen in Sacred's forest as 30000, 898 and then 23000, 999: the PC
  shows the cave entrance from the hero's eyes along its facing with nobody in view, and the smaller angle zooms in.
  VNDegen takes the first value in thousandths as a view angle, the second in thousandths as the eye height, and hides
  the hero; the door then comes out the same width as on the PC.
- 44's second argument is 1 for a picture caption (the GUID follows); 0 and 2 are text.

### Pictures

25's third argument is the zoom in percent: Colette's `rogo` at 50 showed at half size, centred on the screen plus
(x, y). The colour is ABGR: `0xff0000ff` turned white text red. Each channel scales by its byte over 127.5, so
`0x7f7f7f7f` (Sacred's help bar) looks plain and -1 means no colour at all; the alpha byte over 127.5, capped at 1,
is the opacity (`0xff0000ff` covered the map fully). 24 moves zoom, colour and position.

In a save each picture is 46 bytes: `int32` number, `int32` 0, GUID, `int32` zoom, the colour with its bytes reversed
(the PC wrote `40 ff 00 00` for `0x40ff0000`), a 0 byte, `int32` x, `int32` y, a shown byte and `int32` -1. Erased
pictures stay in the list with the shown byte 0; one erased with a fade keeps colour 0. On loading, the PC shows every
picture whose shown byte is 1 (Colette's half-size `rogo` came back), which is why Sacred's help bar returns and its
faded tutorial picture does not.

### Captions, balloons and effects

- Static captions (44 with scroll flag 0): Sacred's player (1.0.6) fades one in over about 0.4 s and, after the key,
  out over 0.4 s before the event goes on; Colette's player (1.8.1) shows and removes it within a frame.
- An effect record (`Effect.sgr`, type 9) is GUID, name, the first picture-sheet resource GUID, then `int32` layers,
  `int32` 0, `int32` keys, and an `int32` that VNDegen reads as the length in 60ths of a second (66 for the balloons,
  about the second A measured). Keys follow: per key a frame and an entry count, and per entry a layer byte, a mask
  byte, and fields by mask (colour and a byte for bit 0, then position, scale, an `int32`, the sheet cell byte and one
  more byte). The keys are not fully read yet.
- Effect picture sheets (resource kind 9) carry two bytes after the path: columns and rows (`emotion.png` 10 by 11,
  48-pixel cells). Balloons draw cell 0 (the bubble) and the icon cell inside it: ! 8, ? 7, the dots 9 (twice), the
  skull 3, the bulb 10, the heart 6.
- Colette's opening music is `samples/common/res/bgm/BGM_town_countryside.ogg`: recorded from the PC, it matches that
  file at half amplitude with no residual, from 4.2 s after the new game starts.

### Shop screen

Filmed in Sacred's weapon shop (25,19) after loading a VNDegen save there. The shopkeeper's message (from the event,
before 37) stays in its window while a 103 by 158 column at (428,193) offers buy, sell and quit, and a money box at
(728,3), 218 by 48, shows "100 G". Buying opens a list panel at (13,73), 397 by 397, with one item a row (48 pixels,
icon, name, price without "G") and the quit button at the bottom, a detail panel at (414,73), 532 by 397, with the
item's kind, the count held, its attack, and for each hero who can equip it the face (a 125 by 135 crop), the name
and the attack and elemental attack before and after (the new value in light blue), and the description along the
bottom at (13,473). Choosing an item opens a 239 by 65 box at (606,381) with the item, "x1" and the total price
before it is bought.

## First release (round 4, J)

Seen on the PC player under Wine (Colette, player 1.8.1), playing Colette from the title to its first world-map battle
and back, beside the SDL runner playing the same path with real key events.

### Saves after the first battle

- The hero's `int32` after experience is its state: the PC wrote 1 for コレット, poisoned, and 0 for ダーク after a
  battle in which only she was poisoned. The runtime reads it into the member and writes it back; recovery and the inn
  clear it.
- The learned-skill list after the equipment holds the skills a hero reached by levelling up: ダーク's record grew to
  204 bytes at level 2 with キュアー (his level 2 skill) in it, and round 3's patched save, levelled to 6 by command
  55 after command 56 taught ファイア and アイス, held those two and then his level 2, 5 and 6 skills: ファイア, his level
  3 skill, was not added twice. Level 1 skills are not in it. The runtime appends the
  skills of each level passed at a level-up and lists the learned skills in battle after the hero table's.
- The monster table's two `int32` before the drops are the money and then the experience (ゾンビーナＬＶ01's result:
  経験値 32 and 13 G, the table holds 13 then 32).

### Title

- With a save in `savedata`, つづきから (Continue) is lit and the cursor starts on it; without one the PC draws it
  grey. It opens the save page's layout (eight slots a page, five pages, the selected slot's date, map and party on
  the right) over the title darkened by about half, and loading a slot starts the game there. The third item,
  コンフィグ, opens the config page over the title.
- The save page's overwrite question defaults to はい.

### Mouse

- A left click acts as the confirm key, when the button comes up: on the map it talked to the aide the player faced,
  in a message it went to the next page, in the menu it chose the highlighted item.
- Holding the left button on the map opens the menu: at 0.3 s it was a click, at 0.7 s and 1.0 s the menu opened
  while the button was down, and the release then chose the highlighted item (アイテム). Holding in a menu page did
  nothing.
- The right button did nothing on the map, in messages or in the menu, and moving the pointer over a menu item did
  not move the cursor. The player does not walk to a clicked cell.

### After a battle

- With a member poisoned, the PC keeps the `!Poison` balloon over the leading hero on the map: the balloon is cell 0
  of `emotion.png`, about 28 pixels lower than the event balloons of command 45, with purple bubbles (cell 19) moving
  inside it. It showed while コレット, not the leader, was the poisoned one.
- A wandering monster that loses the battle without the continue flag ends the game: the party lost to a
  ゴーレムＬＶ01 on the world map (variable 9 had picked its set), GAME OVER showed and the title followed.

### The path

The PC and the runner show the same scenes in the same order: the opening scroll, the throne room and the aide's
pages, the corridor, the dungeon's three visits with their choices (the PC leaves an empty message window up while
the choice returns), the town and the king's hideout, コレット joining, the bridge guard, the world map and a
monster reaching the player on the bridge. Townspeople who wander can step onto an invisible exit or door cell with a
pass-through sheet, such as the castle door and the town's pier, and stand there while the player blocks the way
out; whether the PC lets them was not checked.

## Colette past the first area (round 5, L)

Played in the runner from round 4's save on the world map, and compared with the PC under Wine by loading the
runner's saves there (each loaded at the same cell with the same party, items, money and levels).

### Inn (command 38)

- The offer is a choice window like an event's, `泊まる：<price>G` (the game text's stay string, a full-width colon,
  the price) and `泊まらない`, over the innkeeper's message, which stays up. The party's money shows top right in a
  `default` window at (729, 3), 219 × 49, right-aligned at x 937 as `27 G`. There is no button guide.
- Staying closes the windows and plays `samples/common/res/bgm/me_lodging.ogg` (8.93 s; a recording of the PC matched
  it by its envelope, correlation 0.92, starting with the key press). The screen fades to black over about 1 s, stays
  black until the music effect ends and fades back in over about 1.2 s; the map's BGM resumes when the effect ends.
  The party is healed and the player faces down when the picture returns, then the script's "stayed" branch runs.
- Without enough money the player shows the game text's `お金が足りない...` in a plain message window in the middle of
  the screen (position 1), waits for a press, and the script takes its other branch.

### Map objects

- Directions 1 and 3 turn a map object the other way from what the runtime first assumed: the inn's SingleBed at
  (11,12), direction 1, has its head on the west side of its cell and its foot one cell east in the PC. The model and
  its collision cells turn the same way.
- Collision heights count from the ground under the object's own cell. Colette's suspension bridge in コムコムの穴
  B2F is anchored at (12,15) on height 9 with deck cells of height 1, so its deck is at 10 all the way over the chasm,
  level with the stairs at each end; the PC walks across it.
- A collision cell's second `int16` (1 on bridge decks and rails) marks a cell that can also be walked under at
  ground height. B3F's stone bridge arches over the only way into the pit with the stairs down: the runtime lets a
  walker at ground level through when the deck is at least two units up. The PC wasn't walked through that arch; the
  pit has no other way in.

### Stairs

- A ramp-shaped stair (shapes 0 to 3) can be entered and left along its slope only: in B3F the PC player at (6,15)
  could not step north onto the west-rising stair at (6,14), at the same height. Stepping sideways between cells of
  the same ramp at the same height (a staircase several cells wide) stays allowed.
- Characters on a stair or slope stand on its surface; drawn at the cell's base height they sank into the steps and
  showed as the see-through outline, while the PC draws the player on the steps.

### Camera mode 1

`53 1 pitch yaw angle height ms wait`, all in thousandths, is a view from the player's cell like mode 2: the eye sits
`height` above the player, looking along `yaw` and down, and the player isn't drawn. Fitted to the public square scene
(`53 1 -13000 0 20000 2595 1000 0`) against a PC capture: the view angle is twice the value as in mode 2 (40° for
20000), the eye height is as given, and the PC looks down about 0.58 times the given pitch (7.5° for 13). Colette's
other mode 1 pitches (−22, −30, −55, in scenes after a defeat and their replays) weren't compared.

### Other commands

- 76 names a sound effect and stops it: Colette's three uses come at the end of a scene that started the same
  20-second effect with 22.
- Colette uses none of 55, 56, 58, 68 and 77.

### Battles

メイリ's character resource has no `wait` motion; her character picture is an eight-frame standing animation in one
row (152 × 152 frames). The PC shows her standing, facing the camera, beside コレット.

### Saves after the first area

Saves move both ways at two more places. A runner save in the castle town (party of three at level 9, メイリ
poisoned, 636 G, nine kinds of items) loaded in the PC at (12,12) facing down with the same levels, HP, MP, state,
money, play time and item list, and the PC's save made there loaded in the runner the same. A runner save on the
サキュバスタワー's sixth floor loaded in the PC with its floor caption showing again, as the runner shows it, and the
PC's save made there loaded in the runner at (4,5).

## Sacred's first dungeon (round 5, K)

Sacred was played from the title to the world map in the runner and in the PC under Wine side by side: the opening,
the castle and the queen, the practice battle, the town and its weapon shop, the escort to 王家の洞窟, the fairy's
scene, the golems where アリア rejoins, the crown behind the stone bridge, the second cave and the Vord forest. Heights,
positions and graphics below were read from saves the PC wrote at each spot; the rest from captures of the same spot.

### Heights in saves

- A character on a stair or ramp cell is saved 0.6 above the cell's height: the soldiers on the throne-room stairs at
  7.6 (floor 7), the castle's exit-stair events at 3.6 (floor 3), and the player going up the royal cave's first ramp
  at 6.6, 5.6 and 4.6. Elsewhere the saved height is the floor's. The runtime keeps whole levels and writes and reads
  the 0.6 at the save.

### Stairs and bridges

- On Sacred's castle stairs (a flight along x) the PC player can't step north onto the ground at the flight's foot
  height, but steps along the flight's cells and down it; this is the sideways rule of round 5 L.
- A map object cell whose fourth value is 1 is a bridge deck. The royal cave's `Stone Bridge W4` at (24,4) facing 3
  covers (24,4) to (24,7), the deck standing at the bridge's own cell height plus its value; the PC's saves put the
  player at 8 on (24,7), (24,5) and (24,3) and at 4, the ground's height, under it at (24,6). An object turned to direction 1 or 3 covers the
  cells its model is drawn on; the earlier footprint was mirrored.

### Events

- The byte after a sheet's fixed direction is the solid flag (`solid` in `SgaSheet`): the second cave's iron door
  opens into a sheet with 0, and the PC player walks through the open door cell; the floor switch also has 0.
- A transfer (20) that names no map moves only the player: after the queen's talk the PC's save keeps the opening
  scene's soldier at (27,12), where he walked, not at his editor cell (35,8).
- A save writes no graphic for an event whose active sheet has none: the PC writes zero for the throne-room soldiers
  who leave after the queen's talk, and an event so saved stays hidden when loaded.
- Command 48 with its third value 0 counts items worn by the party with those in the bag: the escort soldier's check
  for one ロングソード passes on the PC with the sword equipped and none in the bag.
- Pictures go when the player moves to another map: Sacred's help bar (picture 5) is gone in the town on the PC and
  its save there lists no pictures.

### Drawing

- Map objects and 3D characters are lit by the map's fill colour plus its key colour times N·L, without the 0.77 and
  0.74 that terrain takes: Sacred's town pavement measured 222,181,116 on the PC, 174,140,88 before and 221,180,115
  after; its castle statues, beds and chests, and Colette's castle and trees, match the PC the same way.
- Maps with the sun off show no sky and nothing past the map's edge: the PC draws black around the royal cave and
  Colette's dungeon cell, where the runtime drew the noon sky and the outside sea chip.

### Menus

- The item page shows a weapon in white with only its description; the attack value is on the equipment page.
- The shop's quantity box is 480 by 132 in the middle of the screen: icon and name at 24 pixels, the count in a
  highlight at its right and the total below it. Items the party can't afford are listed in grey.

### Saves after the first dungeon

A runner save in the royal cave (ロイ at level 2, 75 G, a herb) loaded in the PC at (15,21) with the same level, HP,
MP, money and items, and one at level 4 loaded there at the same spot with its level, HP and MP on the save page. The
PC's save beside アリア's fight at (32,13) (ロイ at level 1, 40 HP, 15 G, ヒール learned in the fairy's scene) loaded in
the runner at the same cell with the same menu. The cave's bed heals the party (59) as on the PC; Sacred's first inn is in a later town.
