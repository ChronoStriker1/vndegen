# The PC Artemis engine's save files

VNDegen reads and writes the save files of the PC Artemis engine, so Ludusavi can move saves between the PC version of a game and VNDegen. This is the format as worked out from saves the engine wrote, under Wine, with each game's own executable. The engine writes two versions. Rev.1825, in a game whose archives are `pf2`, writes version 1000. Rev.2544, in a game whose archives are `pf8`, writes version 1002. VNDegen reads both and writes the version of the PC save files it finds, or, when there are none, the version the game's archives suggest. The tools in `Engines/Artemis/tools/save-format/` dump these files.

## Encoding

Integers are little-endian 32-bit. A string is its byte length and its bytes, in the game's charset (often Shift_JIS), with no terminator. A byte is one byte.

## Objects

Everything is built from one structure, an object with numbered fields:

| Part | Type |
|---|---|
| field count | int |
| table | count pairs of (field number, offset) |
| field data | the fields, one after another |

An offset counts from the field count. Fields whose number is missing from the table are absent. A field's data runs to the next field's offset, or to the end of the object. An object nested in a field starts with its byte length, which doesn't count the length itself.

Version 1002 lays the same objects out differently. After the version comes the uncompressed length, and the rest of the file is zlib data. In it, an object is only its fields' data, with no count, table, or length. The tables follow all the data, in the order the objects end, so the root's comes last. The table area is a count of tables, then for each the offset of its object, its field count, and `(field number, offset)` pairs, all offsets counting from the start of the uncompressed data. The data ends with the offset of the table area. The root object is at offset 0. Since a nested object carries no length, a reader finds where one ends from its last field: a tag ends after its parameter list, and a frame after its tag.

Two shapes come up everywhere:

- A **tag** is an object with field 2, an int (the tag's source line, or -1), field 3, the tag name, and field 4, its parameters: a count and that many name and value strings. An empty tag has all three fields zero.
- A **tag list** is a count and that many entries, each a key string, a count, and that many tags with their lengths.

## The files

Each file is a magic, the version, then a root object. In version 1000 its offsets count from byte 8.

### system.dat, `BOWX`

Field 1: a count and that many name and value strings, the `s.` variables.

### saveg.dat, `BOWG`

- Field 1: a count of scripts, and for each a script name, a count, and that many instruction indexes already run.
- Field 2: the `g.` variables, as in system.dat.

The engine marks every instruction it runs, not only text. An instruction index counts the instructions of a script after preprocessing from 0, with each `[lua]` block as one instruction and each run of text between tags as one. `[/if]` gets no index. Leading and trailing ASCII spaces and tabs of a line don't count, and neither does text of ASCII spaces alone between two tags, but a full-width space opening a line is text with an index of its own.

### Numbered and quick saves, `BOWS`

The root object has 68 fields in version 1000, numbered up to 76, and 74 in version 1002, which adds 77 to 82. The ones identified so far:

| Field | Contents |
|---|---|
| 3 | The position to continue at, a frame object |
| 4 | The call stack: a count and frame objects, outermost first |
| 11 | The current message layer id |
| 13, 14, 59, 60 | Message window state, a tag list keyed by window id whose entries are objects of nine fields |
| 18 | Message window settings, keyed by window id |
| 24 | For each message layer, its tags since it was last cleared: the font tags that set it up (`fontinit`, `font`) and its visible text (`print`, `rt`) |
| 25 | For each message layer, its resulting font state: face, ruby face, size, colors, and so on, in 33 fields. Version 1002 adds field 35 and keeps positions, spacing, and scales as floats where 1000 keeps ints, with a scale of 1 as 65536 or 255 |
| 26 | The layers, a tag list keyed by layer id: for each layer, the tags that recreate it, such as `lyc` and a `lyprop` with its current properties |
| 29 | The background music, as the `splay` tag that plays it |
| 30 | Variables: field 2 holds the local variables as a count and name and value strings, field 3 the macro argument scopes |
| 33 | The `setondirchg` handler, as its tag |
| 34 | The Lua function for `onEnterFrame` |
| 36 | The macro files loaded, as a count and strings |
| 38, 50 | The Lua functions for `keyClickStart` and `keyClickEnd` |
| 40 to 49, 77 | More handlers set by tag, such as `setoncontrolskipin` (44) and `setoncontrolskipout` (45), empty tags when unset |
| 51 | The script of the menu |
| 53 | Sound effect slots, keyed by id, each a tag |
| 56 | The `setonpush` handlers, each an int key and a tag |
| 62 | The backlog message layer id |
| 64 | Message layer ids, a count and strings |
| 66 | `prohibit` settings by message window, as tags |
| 67 | `prohibit` settings by message layer, as tags |
| 69, 70 | The Lua functions for onSave and onLoad |
| 75 | `indent` settings by message window, as tags |
| 76 | `indent` settings by message layer, as tags |
| 79 | An int, 3000 |
| 80 | The Lua function for `onDebugSkipOut` |

The other fields hold single flags and counters.

A frame object has these fields:

| Field | Contents |
|---|---|
| 1 | The wait the frame was in when it called on: 0 none, 1 a click wait, 3 a `[stop]` |
| 2 | 1 or 4 in some saves, meaning unknown; 0 works |
| 3 | The script name |
| 4 | The instruction index: in field 3, where to continue; in the call stack, where to return to |
| 6 | The tags queued at that point, a count and tags |
| 7 | A byte, 0, in version 1000 only |
| 8 | In the call stack, the tag at the return index, with its line; in field 3, an empty tag. Text is a `print` tag with the text as `data` |

A game saves from its own save routine, which a button or the script calls. The position in field 3 is the instruction right after the call that saved, in that routine. On a load, the PC engine restores the local variables and the call stack, recreates the scene, and continues at that position. The routine sees it was loaded and shows the restored screen, often with a `[trans]`, then returns. When it returns to a frame that was in a wait, that wait starts again: at a click wait, the engine waits for the click, then runs the frame's queued tags; at a stop, it stays stopped. A frame whose wait field is 0 carries on at once, so a save written with 0 there loses its page. A frame returning into a `[stop]`, such as a menu's, points past the stop with the wait 3.

An input handler that interrupts a wait and calls a routine adds a frame for that wait. In the saves seen, a menu's button adds one when the game's menu calls its routines, as in a game with `pf8` archives, and none when it jumps to them, as in one with `pf2` archives.

VNDegen loads a save the same way. It replays the tags of fields 26 (layers), 24 (message layers, inside a `chgmsg` for each), 56 and 33 (handlers), and 29 (music), continues at field 3, and holds each waiting frame's wait and tags until the routine returns to it. It writes the frame of the button handler that called the save routine with the wait it interrupted.

Variable values are text, except data from Pluto, the Lua persistence library games use to keep Lua tables in variables. Pluto data always holds zero bytes, which text never does.

### Pluto data

Every object is an int `firstTime` and an int `ref`. A nil is `0 0`, an object written before is `0 ref`, and a new object is `1 ref type payload`, where `ref` counts the new non-nil objects written so far. Types and payloads: 1, a boolean as an int; 3, a number as an 8-byte double; 4, a string; 5, a table, written as an int 0, its metatable object, key and value objects, and a nil to end them. See `crates/asb-interpreter/src/pluto.rs`.
