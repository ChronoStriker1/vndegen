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
| 31 | set variable | index, operation, kind, value, flag |
| 32 | set switch | index, value |
| 43 | message with face | text, window, flag, face |
| 44 | scrolling text | text, speed, flag |
| 51 | screen on or off | 1 black, 0 shown |
| 60 | fade | ms, 0 in or 1 out, wait |

A message page ends at a blank line.

## Saves

`savedata\Save_NN.sgs` in the game folder: a `YUKRDATA` header, then plain `BinaryWriter` data. Layout not read yet.

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
attack, defence, magic, hit %, two flags, evasion, speed), 13 `int32` resistances in the element list's order (negative
is a weakness), actions (`int32` n, n × (`int32` condition, two arguments, rate, kind (see Battles below),
skill GUID, `int32` priority)), `int32` exp, `int32` money, two drops (item GUID, `int32` percent), `int32`. All 91
monsters of both games read to their last byte.

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
- Defending (身を守っている) halves damage and a charge (力をためている) doubles the next attack (both inferred).
- Monster actions: the table's entries are (`int32` 36, `int32` turn (0 every turn, 1 odd turns, 2 even turns,
  inferred from Sacred's slimes), `int32` 0, `int32` weight, `int32` kind, skill GUID, `int32` 50). Each turn one
  entry is drawn by weight. Kinds: 0 attack, 2 watch (`{0}は様子を見ている`: the slime has four attacks and one
  watch at equal weight and watched 6 times in 33 turns), 3 skill, 4 charge, 6 run away (`{0}は逃げ出した！`). A skill
  the monster has no MP for is not drawn. Targets are random among living members (inferred).
- Rewards: the monsters' experience and money (the slime: 経験値 5, G 5, as the table says), each drop rolled at its
  percentage. With one member the whole experience went to アリア; with more, the runtime gives each living member
  the full amount (inferred, not seen).
- Experience totals for each level, from アリア's status page and the battle in which her HP rose: level 2 26, level 3
  84, level 4 between 182 and 186, level 5 350, level 6 600. The runtime uses these (184 for level 4) and
  3.137 × L^2.928 + 3.29 above level 6, a fit that matches all five within 3.
- A level-up raises HP and MP by the growth of their maximum (inferred); the guard's script heals after the practice
  battle, so the PC's own rule was not seen.
- Elements and states: a weapon's element scales damage by (100 − the target's resistance) %, negative resistance being
  a weakness (inferred, not seen in these battles). States (poison, sleep, paralysis, confusion, charm) are not
  applied yet.

