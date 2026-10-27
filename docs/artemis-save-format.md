# The PC Artemis engine's save files

VNDegen reads and writes the save files of the PC Artemis engine, so Ludusavi can move saves between the PC version of a game and VNDegen. This is the format as worked out from saves the engine wrote, under Wine, with the game's own executable. It describes the 32-bit Windows engine (it reports itself as Artemis Engine Rev.1825). The tools in `Engines/Artemis/tools/save-format/` dump these files.

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

Two shapes come up everywhere:

- A **tag** is an object with field 2, an int (the tag's source line, or -1), field 3, the tag name, and field 4, its parameters: a count and that many name and value strings. An empty tag has all three fields zero.
- A **tag list** is a count and that many entries, each a key string, a count, and that many tags with their lengths.

## The files

Each file is a magic, the version 1000, then a root object whose offsets count from byte 8.

### system.dat, `BOWX`

Field 1: a count and that many name and value strings, the `s.` variables.

### saveg.dat, `BOWG`

- Field 1: a count of scripts, and for each a script name, a count, and that many instruction indexes already run.
- Field 2: the `g.` variables, as in system.dat.

The engine marks every instruction it runs, not only text. An instruction index counts the instructions of a script after preprocessing, `[lua]` blocks included, from 0.

### Numbered and quick saves, `BOWS`

The root object has 68 fields, numbered up to 76. The ones identified so far:

| Field | Contents |
|---|---|
| 3 | The current position: an object with the script name (field 3), the instruction index to continue at (field 4), the tags queued at that point (field 6, a count and tags), and the tag at that index (field 8) |
| 4 | The call stack: a count and position objects as in field 3, outermost first |
| 11 | The current message layer id |
| 13, 14, 59, 60 | Message window state, a tag list keyed by window id whose entries are objects of nine fields |
| 18 | Message window settings, keyed by window id |
| 24 | For each message layer, the font tags that set it up (`fontinit`, `font`) |
| 25 | For each message layer, its resulting font state: face, ruby face, size, colors, and so on, in 33 fields |
| 26 | The layers, a tag list keyed by layer id: for each layer, the tags that recreate it, such as `lyc` and a `lyprop` with its current properties |
| 29 | The background music, as the `splay` tag that plays it |
| 30 | Variables: field 2 holds the local variables as a count and name and value strings, field 3 the macro argument scopes |
| 33 | The `setondirchg` handler, as its tag |
| 36 | The macro files loaded, as a count and strings |
| 38, 50 | The Lua functions for `keyClickStart` and `keyClickEnd` |
| 40 to 49 | Ten more handlers, empty tags when unset |
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

The other fields hold single flags and counters.

Variable values are text, except data from Pluto, the Lua persistence library games use to keep Lua tables in variables. Pluto data always holds zero bytes, which text never does.

### Pluto data

Every object is an int `firstTime` and an int `ref`. A nil is `0 0`, an object written before is `0 ref`, and a new object is `1 ref type payload`, where `ref` counts the new non-nil objects written so far. Types and payloads: 1, a boolean as an int; 3, a number as an 8-byte double; 4, a string; 5, a table, written as an int 0, its metatable object, key and value objects, and a nil to end them. See `crates/asb-interpreter/src/pluto.rs`.
