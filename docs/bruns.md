# Bruns

VNDegen runs Bruns games on a runtime of its own, `Packages/VNDegenKit/Sources/CBRUNS`, written in C from the games'
files, GARbro's MIT-licensed notes on the file encryption and the audio, and the original engine under Wine. It is MIT
licensed like the rest of the app. `docs/bruns-cleanroom.md` lists where each part came from. Bruns is the engine
behind Morningstar Rush's games, built on Ultramarine3.

## Test games

| Game | Engine build | Notes |
|---|---|---|
| Orc Kingdom: Monmusu Breeder King (Japanese, DLsite, `オーク・キングダム.exe`, Version 2.0) | `binary/bruns.exe`, 2011 | Title, prologue, the main room, world map, dungeons with their battles and events, village hunting, the monster girl list, status and talk, Save, Load, quick save, config, backlog, choices, voices, music, the original's saves both ways |

The game is 1280x720. Its Japanese text and bitmap fonts are drawn as on the PC.

## How the games are built

A game folder holds `ams.cfg`, `binary/bruns.exe` (the engine), `scene/brsNNNNNN.bso` (scripts), `image/`,
`parts/`, `stream/`, `font/`, `music/`, `effect/`, `voice/` and `save/bruns-system0.dat` (the default system data).
The plugin takes a folder as a Bruns game when `ams.cfg`, `binary/bruns.exe` and `scene/brs009000.bso` are there, up
to one folder down. The PC version only runs after its own installer; VNDegen needs only the files.

- Every data file is `EENC` or `EENZ`: a signature, the unpacked size, and the body XORed with that size XORed with
  `0xDEADBEEF`, repeated every four bytes. `EENZ` is zlib after that.
- Scripts and `ams.cfg` are S-expressions in Shift JIS. A script has one `a3:define-set` (its entry) and numbered
  `a3:define-sub`s. Statements: `call-sub`, `call-far` (another script's sub), `jump` (start another script), `return`,
  `label`, `goto`, `eval` and `if` (jump when the last `eval` was true), `switch`, `eval-noeffect`, `send-string`
  (text, `clear`, `wait-w`) and `call-user-func` (a numbered builtin). Values are words (a flat array), locals, and
  the global `acc` and `reflag`.
- `ams.cfg` gives the resource paths, the first system script (flag constant 1, here 9010), the message windows, the
  choice windows, the colour table, the right-side menubar and the parts table: buttons with four pictures, pictures,
  text, animations and touchmaps (pictures whose colours mark clickable regions).
- Pictures are PNG. `stream/NNNNNNNN/index.txt` lists the frames of an animation. Music, effects and voices (`.um3`)
  are Ogg Vorbis with the first 0x800 bytes XORed with `0xFF`; a voice number N on channel C is
  `voice/C/C` + eight digits of N.
- The bitmap fonts `font/ff_0N.bmp` (32 dots) and `ff_0Nl.bmp` (64 dots) hold one cell per Shift JIS code from `0x40`
  to `0xFF` after each lead byte: `ff_00` covers lead bytes 0x81 to 0x8F, `ff_01` 0x90 to 0x9F, `ff_02` 0xE0 to 0xEF,
  and `ff_03` half-width ASCII.

## The runtime

- Builtins by range: 1000-1046 layers, pictures and touchmaps; 2000-2151 the system background, message windows,
  choices, parts objects and the fade; 3001-3024 sound; 4000-4430 the event wait, system flags, saves and backlog;
  5003-5201 strings and the random seed; 6000-6010 quitting, starting the game and window modes; 7001 and 7002 text
  input and the licence dialog.
- Layers are numbered, drawn back to front from the highest number, each with one main picture and the motion
  parameters 1006 sets: an anchor (bits 0-3 pick the screen side, bits 8-11 the picture's pivot), position, scale out of
  1024, source rect and colour, tweened when its fourth argument is 201. Changes show when 1007 presents them, with a
  crossfade; 1015 freezes the screen and clears every layer for the next scene.
- `4000 mask timeout flag` waits for events: bit 0 a click, bit 2 parts buttons (click and hover events from 2131),
  bit 13 the mouse (131072 plus flags), bit 14 keys (65536 plus the virtual key times four, plus one when released),
  bit 16 touchmaps (a mode's base plus the region).
- Save, Load, quick save and load, config, backlog, the right-click view and the return to the title are the game's
  own system scripts (9001 to 9020). The engine runs them when the menubar, a right click or the wheel asks, with the
  game's state set aside and put back when they end.
- Saves follow the PC: one `bruns-system.dat` (EENZ S-expression) holds the system flags, the read-text and seen-picture
  records and every slot, each with the layers, pictures, music, message window, call stack and words. The runtime
  keeps the file as a tree, so parts it doesn't use survive. It lives in
  `AppData/Roaming/<author>/<product>/save` in the game's data folder, the folder names coming from `ams.cfg`, so
  Ludusavi backups from Windows restore there.

## Testing

`Engines/Bruns/host` builds `brrun`, which runs a game headless on Linux: `brrun <game> <saves> <out> [commands]` with
`wait:ms`, `click:x,y`, `rclick:x,y`, `move:x,y`, `key:vk`, `wheel:n`, `skip:ms` (Ctrl held), `clicks:n,ms`,
`monkey:n,seed`, `jump:script`, `shot:name`, `peak` (audio level) and `state`. `BRUNS_TRACE=1` logs every builtin.

## Known limits

- Flow mode isn't there; this game leaves its menubar slot empty, so nothing reaches it.
- The licence dialog the PC shows on first start is accepted without showing it, and the save comment box (7001)
  keeps the default comment.
- The animated mouse cursors (`.ani`) aren't drawn, and the window size and full-screen settings do nothing.
- Mask pictures (1006's second group of parameters) aren't drawn; the game doesn't appear to use them.
- The backlog's line spacing is close to the PC's but not the same.
