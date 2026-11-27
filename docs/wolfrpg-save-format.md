# Wolf RPG Editor save files

Wolf RPG Editor keeps each save slot in `Save/SaveDataNN.sav` under the game folder, where NN is the slot number in two digits (slot 1 is `SaveData01.sav`). Games can also name a file of their own in the save variable commands, such as `Save/System.sav`, which holds the same format. VNDegen's runtime reads and writes these files in the PC engine's own format, so a save moves between the PC and VNDegen both ways and Ludusavi can sync the `Save` folder.

The byte layout comes from [WolfSave](https://github.com/Sinflower/WolfSave) (MIT, commit `537ceba4262368c61bfb9c4fa5333341a115c662`), which parses every field but names none. `core/savefile.h` and `core/savefile.cpp` describe the layout once for both reading and writing, and `wolfsave check` confirms that parsing and writing again reproduces every test save byte for byte. What each field means was established by observing the original engine under Wine, as this document records. Saves made there are in the private evidence folder.

## Method

Each meaning below comes from one of these observations, named in the tables:

- **Controlled saves.** Save from the game's menu, change one thing in the game, save again, and diff the two files with `wolfsave dump`.
- **Game data.** Compare a field with the game's own data: the system database, the map file, the tileset, the commands that set a value.
- **Load test.** Edit a save (or write one from VNDegen's runtime) and load it in the PC engine: what the load screen shows, where the hero stands, what the menu shows, and what the engine writes when it saves again.
- **Runtime state.** Run the same game in VNDegen's runtime and compare the values its events set with the save.

Test Game 53 runs on a Wolf RPG Editor 2.2x engine that writes save version 125 in Shift-JIS. Test Game 02 has the same Game.exe. Test Game 52 runs on Wolf RPG Editor 3.282, which writes version 142 in UTF-8.

## Encryption and header

The first 20 bytes are stored in clear. The rest is XORed three times with the C runtime's `rand()` stream, seeded by header bytes 0, 3 and 9, over every byte, every second byte, and every fifth byte. Header byte 2 is the sum of all bytes after the header, modulo 256. Byte 6 is `U` (0x55) in a UTF-8 save and 0 otherwise, and byte 8 is 0. The other header bytes differ from save to save.

The engine loaded saves whose seeds were changed and saves whose whole header was random, so VNDegen writes fresh random seeds. In one run the engine failed to start while a VNDegen-written `System.sav` was present, but the same file started it in later runs, and the engine also fails as described under [Known limits](#known-limits) with a `System.sav` of its own, so that failure is not attributed to the header.

After the header: a marker byte 0x19, the game title (u16 length), the save version (u16), seven sections, and a closing 0x19. Strings carry a terminating NUL that their length counts, in the game's encoding.

## Section 1: the map

| Field | Meaning | Established by |
|---|---|---|
| `mapId` | Row of the system database's map list. -1 in a save that has no map yet | Controlled saves: 2 on the second map, 3 after walking to the third |
| `unknown1` | The camera's top left in quarter pixels, which the engine shows right after a load: 240 1040 is 60, 260 | Load test: President Yukino's own save at the school front holds 240 1040, the hero-centred camera for 40-pixel tiles, and one with 0 0 opens scrolled to the map's top; Journey of Depravity's holds 0 64, its 16-pixel limit. VNDegen writes its camera there in version 128 saves and carries the value over in others |
| `width`, `height`, `tiles` | The current map's size and its three tile layers, laid out like `Map::tiles`, so tiles changed by events persist. A -1 in place of the first tile means none are stored, which system variable 127 asks for | Game data: they equal the map file's |
| `tilesetBase`, `autotiles`, `tileFlags` | The tileset in use: base image, autotile images, and every tile's passability | Game data: they equal the tileset's in `TileSetData.dat` |
| `events` | Every event of the current map, in map order, as characters (below) | Game data, controlled saves |
| `v72`, `values2` | Not known | Kept |

## Characters

Map events, the hero and the five party members share one layout.

| Field | Meaning | Established by |
|---|---|---|
| `id` | Map event id; -2 for the hero, -3 to -7 for party members | Game data |
| `x`, `y` | Position in half tiles | Controlled saves: one step right adds 2 to the hero's `x`. Load test: a hero written at tile (6,9) stands there in the PC engine |
| `graphic` | Character image file; empty for none | Game data, load test |
| `appearance` | Opacity, blend mode, direction (numeric keypad: 2 down, 4 left, 6 right, 8 up), animation frame | Controlled saves: a step right turns the hero from 2 to 6. Load test: a hero written facing 4 faces left |
| `hidden` | 1 when the character shows nothing: an event with no active page, an empty party slot | Game data: the events whose page conditions fail have it set. The hero's carries the party command's transparency in VNDegen: inferred, as False Myth 2's PC saves where the hero shows have it 0 and none was made while the party was transparent |
| `route.flags` | For the event's page: animation speed, move speed, frequency, move type, the page's option bits, and the route's flags | Game data, compared for one map's events; not changed and reloaded |
| `selfVariables` | Always empty in the test saves; map event self variables are in section 3 | |
| the rest | Movement state and counters | Not known; VNDegen writes the values the engine wrote for the same character, else its usual ones |

## Section 2: the system state

Most of this section holds engine settings without names in the format. The fields known so far:

| Field | Meaning | Established by |
|---|---|---|
| `b[0]` | Play time in seconds, system variable 29 | Controlled saves: grows with play time; the load screens read 9000029 from a save and show it. Load test: a VNDegen save with 213 seconds shows 0:03:33, and the engine counts on from it |
| `b[6]` to `b[13]` | System variables 0 to 7 (face, message window position, choice window position, pause cursor offset, the choice's initial position) | Runtime state: the values Test Game 52's and 53's events give them; -1 in an empty save, as the engine's defaults |
| `frameCount` | System variable 28, frames since the game started | Controlled saves: grows about 60 per second |
| `volumes` | System variables 100 to 102: BGM, BGS and SE volume in percent | Runtime state: Test Game 52's events set them to 45, 69, 45, which the save holds; the game's settings screen shows BGM 45% and SE 45% after a load |

String variables beyond those in section 3, system strings other than the party graphics, the screen tint and the playing BGM and BGS were not found in the saves. See [Known limits](#known-limits).

## Section 3: the variables

| Field | Meaning | Established by |
|---|---|---|
| `tickCount` | Differs from session to session; it tracked the machine's uptime in milliseconds | Controlled saves; the engine loads any value |
| `eventSelf` | Map event self variables, as `eventSelf[map][event]`, each list cut after its last nonzero value | Game data: an event whose second page needs self variable 1 = 1 has `eventSelf[2][0] = [0, 1]` |
| `numbers` | Normal variables, then spare variables 1 to 9, each list as long as the system database has names for that kind | Game data: the list lengths equal the name counts in both test games. Load test: the PC engine's load screen shows a location chosen by normal variable 13 from a VNDegen save |
| `strings` | String variables, as far as the engine writes them (one empty string in Test Game 53's saves, none in Test Game 52's) | Not tested by changing them |
| `commonSelf`, `commonSelfStrings` | Common event self variables 0 to 99 for every common event, and the string ones, 5 to 9 | Game data: one list per common event; the save screen's own working values appear in its common event's list |

## Section 4: the hero and the party

The hero, then five party members, as characters. Then two unknown bytes, `trail`, and from version 0x8A one more list. `trail` holds the hero's last steps as directions, which the members follow: a step right appends a 6. An empty save has no trail.

## Section 5: pictures

Each picture has its number, a kind (`0 0` an image, `1 0` text, `0 1` a window, `<SQUARE>` or `<GRADX-a-b>`), its file or text, and `a`. In version 128 saves, checked against the PC engines under Wine by changing one field at a time in their own saves:

| Field | Meaning |
|---|---|
| `unknown3` | The anchor, as the picture command gives it: 0 top left, 1 centre, 3 top right and so on |
| `a[0]`, `a[1]` | x and y |
| `a[2]`, `a[3]` | Divisions across and down, or a window's width and height |
| `a[4]`, `a[5]` | Pattern and opacity |
| `a[6]` | Blend mode: subtract (2) hides white text on black, as the picture command's blend does |
| `a[7]` | -1 in every save seen |
| `a[8]` | The frames of the last command on the picture; changing it shows nothing |
| `a[9]`, `a[10]` | Zoom across and down |
| `a[11]` | The angle as the picture command gives it, in tenths of a degree: 45 turns an image by 4.5 degrees on the PC and 300 by 30. Text pictures loaded from a save didn't turn |
| `b[0..5]` | Colour now and its target, red, green and blue, as percent × 254 / 100 − 254: 100% is 0, 80% −51, 0% −254 |
| `v69` | 0x646464 twice, then zeros, then 100 twice, in every save seen. Setting `v69[2]` crashes the engine; the other zeros, `v72b`, `v73a`, `v74`, `v75`, `v81` and `v87` change nothing seen when set to 45 |
| `v73b` | The event that made the picture: a type or −1, the common event, the command line |
| `v76` | −1 −1 1 for images and text, 0 −1 0 for `<SQUARE>`, and 4, a × 1000 + b, 1 for `<GRADX-a-b>` |

Text keeps the engine's own codes, each a 01 byte, a length that counts itself, the code and its value. A value is two digits, each plus 14, the first counting 64s: `\f[n]` is 01 05 03 0E (n + 14) 01 for sizes under 64. The codes read so far, from the bytes the PC engines wrote for text whose source the events give:

| Code | Bytes |
|---|---|
| `\f[n]` | 01 05 03, n in two digits, 01 |
| `\E` | 01 02 05 |
| `<R>` | 01 02 15 |
| `\ay[n]` | 01 05 1E, n + 5000 in two digits, 01: `\ay[32]` is 5C 36 and `\ay[62]` 5D 14 |

Other codes (colours, `\ax`, `<C>`) aren't mapped yet, and a text picture holding one is left out of the save.

Engines that write version 128 (President Yukino's 2.21, Drain Dungeon's and Journey of Depravity's 2.23) save the pictures on screen and show them again after a load: a picture added to a Drain Dungeon save at a number the game never uses shows after loading on the PC. VNDegen writes and restores pictures for version 128 saves, leaving out pictures numbered below 0 (Drain Dungeon's dungeon background is -5 and the PC's save has none of them), and writes `a[8]` as 1, `v73b` as -1 and `v69` as the PC writes them for a plain picture. ANGEL QUEST's engine (version 123) closes when it loads a save holding a picture record in version 128's shape, and the engine that writes 125 (Test Game 53) showed none of a save's pictures, so VNDegen writes an empty list for those and clears pictures on load, as before. Wolf RPG Editor 3.714 (version 159) also restores pictures, with each value now and its target where a picture command moves it. Checked against The Clumsy Witch Seris's engine under Wine by adding pictures to its own save, one changed field each; it draws the values now and leaves the targets alone after a load:

| Field | Meaning |
|---|---|
| `a[0]`, `a[1]`; `a[2]`, `a[3]` | x and y now; their targets |
| `a[4]`, `a[5]`, `a[6]` | Divisions across and down, and pattern |
| `a[7]`, `a[8]` | Opacity now and its target |
| `a[9]` | Blend mode |
| `b[0]`, `b[1]`; `b[2]`, `b[3]` | Zoom across and down now; their targets |
| `b[4]`, `b[5]` | Angle now and its target |
| `v9fA` | Colour now, red, green and blue, then its target, as version 128's `b` |
| `v81[14]`, `v81[16]` | The effect command's zoom across and down: 0 hides the picture, 50 halves it |

VNDegen writes and restores images and text there as for version 128, with each target equal to its value, `v81` as -1 seven times, 1, zeros and the four 100s, and `v9fB` to `v9fD` as the engine writes them for an image or text. The engine loads those saves with their pictures. Window pictures aren't written for version 159: no Seris save shows where their size goes.

## Section 6: the variable database (CDB)

Every type of the CDB, with each type's ID method, field kinds as in `CDataBase.project` (below 2000 a number, 2000 and up a string), and its rows. Rows hold values the way the runtime's `DatabaseType::Row` does. Established by game data and the load test: Test Game 52's load screen and menu show the character name, face and stats from a VNDegen-written CDB, and the runtime reads the same UTF-8 name back from the engine's own save with command 221. The user and system databases aren't saved.

## Section 7

A presence byte, 0 in every test save.

## How the runtime uses this

**Saving** (command 220 with exec code 0, or `Game::saveSlot`) writes `Save/SaveDataNN.sav`, creating `Save/` if needed, through a temporary file renamed over the slot. The fields above come from the game's state. Every other field comes from the save the session last read or wrote, else from the newest save already in the game's `Save` folder, which the same engine wrote, else from the values the engine writes into an empty save. The version is that save's, else 142 for games with Wolf RPG Editor 3 data (UTF-8, or a Game.dat settings block longer than 22 bytes), 128 for a block of 22 bytes, else 125. A version 125 save of a game with a 22-byte block, which only VNDegen 0.58.1 writes, becomes version 128 on the next save, with `v126` 10 and `v128` 0 as those engines write them; those two values are all that 128 adds to 125's layout. The block's size follows the engine: ANGEL QUEST's has 20 bytes and its engine writes 123; Milky Quest's (Test Game 53), Parade Buster's and The Succubus of the Forest's have 21 and their engine writes 125; President Yukino's (2.21), Drain Dungeon's and Journey of Depravity's (2.23) have 22 and theirs write 128; 3.x games have a longer one (Two Beasts has 35). President Yukino's engine stops with "Buffer overrun detected" on a version 142 System.sav or save.

**Loading** (exec code 1, or `Game::loadSlot`) reads the file at once and applies it at the start of the next frame, when every running event ends, as on the PC. It restores the variables of every kind above, event and common event self variables, the CDB, system variables 0 to 7, 28, 29 and 100 to 102, the party's graphics (system strings 10 to 15), the map with its saved tiles and tileset, the hero's and members' positions and directions, and each event's position, direction and frame. Event pages follow from the restored variables. An event the save shows as hidden while its page would show it was erased, and stays so. The map's own music plays. System variable 24 is 1 after a load and 0 after a failed one, and 25 holds the slot.

**Save variables** (221 and 222): the slot argument is a slot number, or a string variable that names a file under the game folder. The address is the third argument, or the value of the variable it names when bit 0 of the fourth is set. Reading 9000024 gives 1 when the save could be read and 0 otherwise, which is how load screens tell empty slots. Reads and writes cover normal and spare variables, string variables, common event self variables, the CDB, and system variables 0 to 7, 28, 29 and 100 to 102. Writing to a file that doesn't exist first creates it as the engine does: no map, a 20 by 15 map of tile 100000, every variable 0, an empty CDB.

## Version 145 (Wolf RPG Editor 3.3x)

False Myth 2's engine, Wolf RPG Editor 3.343, writes save version 145 (0x91) in a container of its own, marked by 2 in header byte 8. It is the 3.5 container (`docs/wolfrpg-cleanroom.md`) without the swapped blocks: the unpacked and packed sizes, then the body as one LZ4 block, XORed from byte 20 with the same 128-byte stream seeded by header bytes 0, 1 and 5, with header byte 3 the byte sum and bytes 4 and 7 the check value. The body is version 142's, with no arrays and no map layer count, plus two u32 at the end of each picture, after `v89`: 0 and 1 in all 135 pictures of two menu saves. Version 159 writes the same pair there, before its flag.

Established from the game's own files: the save it ships, the save the engine rewrites at every start (`SaveData101.sav`, its title map, 110), and saves made from its menu under Wine (`SaveData02.sav` with the menu's 109 pictures on screen, and `SaveData102.sav`, which it writes when the menu opens). Each parses to the end marker and writes back to the same body bytes. Load test: the PC engine lists saves VNDegen wrote, with the play time they hold (12780 seconds shows as 003:33), and loads them, both a copy of its own menu save with all 109 pictures and one with no pictures, as the runtime writes; after a load its own events draw the menu and HUD again. A save the PC engine then made from that state reads in VNDegen, with the play time carried on from VNDegen's. The game puts the hero back where its own variables say after a load, so the hero's saved position wasn't seen to matter.

The runtime treats 145 as 142: pictures aren't written and a load clears them. Versions 143, 144 and 146 to 158 are still refused, since no save of those versions has been seen.

## Known limits

- The screen tint and the playing BGM and BGS weren't located in the saves. After a load the map's music plays and the tint stays as it was. Music an event started before the save is lost.
- String variables were never changed in a controlled save, so their place rests on the layout alone.
- Under Wine, Test Game 52 crashes when Continue is chosen while `Save/System.sav` exists, with the engine's own `System.sav` and saves as much as with VNDegen's, so reading a VNDegen-written `System.sav` was checked in the runtime only.
- A Wolf RPG Editor engine older than 2.2x that has no saves in its folder gets a version 125 save, which it may not read.
