# ExHibit

VNDegen runs ExHibit (ExHIBIT) games on a runtime of its own, `Packages/VNDegenKit/Sources/CEXHIBIT`, written in C
from the games' files, GARbro's MIT-licensed picture reader, a prose write-up of the script container, the command
names in the engine's debug strings, three minimal readings of `resident.dll`, and the original engine under Wine. It
is MIT licensed like the rest of the app. `docs/exhibit-cleanroom.md` lists where each part came from. No other
runtime for the engine exists.

## Test games

| Game | Engine build | Notes |
|---|---|---|
| Succubus Sessions: Mami Mamiya's Sweet Slice of Hell (English, GOG, `ExHIBIT.exe`) | ExHibit with `resident.dll`, 2024 | Logos, title, the whole story to the ending, voices, music, sprites, H-scene speech bubbles, the control bar, the system menu, Save, Load, backlog, all five config pages, the original's saves both ways |

The game is 1920x1080.

## How the games are built

A game folder holds `ExHIBIT.exe`, `resident.dll` (the engine), `ExHIBIT.ini`, `sc00.dll`, `rld/` (scripts),
`res/g/` (pictures), `res/s/` (sound), `res/c/` (layout files) and, once played, `userdata/`. The plugin takes a
folder as an ExHibit game when `ExHIBIT.ini`, `resident.dll` and `rld/start.rld` are there, up to one folder down.

- Pictures (`.gyu`) are 8, 24 or 32-bit, LZSS or GYU's bit-coded LZ packed, with ten byte swaps from a Mersenne
  Twister seeded with the file's key and an optional alpha plane.
- Scripts (`.rld`) are XORed with a table from a Mersenne Twister seeded with a 32-bit key: a fixed key for `def.rld`
  and one game key for the rest. The runtime finds the game key from the scripts themselves (about 1.5 s on eight
  threads, once) and keeps it in the game's data folder as `exhibit-keys.txt`. A script is a list of commands, each a
  packed word (opcode, number count, string count), its numbers and its Shift-JIS strings. `def.rld` and
  `defChara.rld` run first, then `ExHIBIT.ini`'s entry script.
- Registers: `R` (game, 2048), `S` (system), `Q`, `L` (settings such as `L12`, the countdown, and `L18`, the system
  sound), `F`. Conditions and values are infix expressions (`R1300>=30 & R1300<=R1286`, `;` is "or"). A number of a
  million or more in a sound command names a register.
- Commands: `BLOCK` is a label and a conditional block, `JUMP`, `CALL` and `RETURN` (functions live in
  `library.rld`), `CHANGESCENARIO` moves to the next scene, `MESSAGE` shows a line (with speech-bubble and timed
  side commands in its later strings), `DRAW` sets the background with an optional camera rectangle, `ENTERCLIP`
  pans or zooms it, `BILLBOARD` and `REMOVEBILLBOARD` add and remove numbered pictures, fills and snapshots with fades
  and mask wipes, `FACEUP` wipes a picture in, `ENTERMOVE`, `ENTERFADE` and their `LEAVE` waits animate them, `C_ENTER`
  and friends place sprites, `DRAWCHARACTER` draws the portraits beside the window, `FOCUS` blurs the background,
  `FADEOUTEVENT` ends a scene, and `PREPAREVIEW` to `SELECTVIEW` build the title menu.
- Sprites: `res/g/ch/CCCCPP00/CCCCPPPPP000.gyu` is a body and `...F.gyu` a strip of faces pasted onto it at the place
  `DEFCHARACTER` gives. A sprite is drawn at 1:1 centred on 960 plus the command's offset, moved by the definition's
  offsets.
- Text is the game's own `res/NotoSans-Medium.ttf`, 32 px on a 52 px pitch in the window, with a shadow; `{I:1}`
  turns italics on. Speech bubbles in H scenes use the grey masks in `res/g/ch/fd`, placed in the portrait space the
  H pictures are drawn in.
- Sound: music is Ogg Vorbis with its first 512 bytes scrambled (see the clean-room notes); voices (`res/s/v`) are Ogg,
  effects (`res/s/e`) are WAV. A line plays the voice whose number is its text number when the speaker is voiced.
- Menus and system screens are drawn from the layout files in `res/c` (`save.layout.rnf`, `load.layout.rnf`,
  `backlog.layout2.rnf`, `settings.layout.1st.rnf`) and the control bar's definition in `def.rld`.

## Saves

The PC engine writes `userdata/u0` to `u89`, `a0` to `a8` (auto) and `q0` to `q8` (quick), with `udb`, `adb` and
`qdb` holding each slot's DOS time stamp, and `userdata/thumbnail/U00000.gyu` and so on as 320x180 thumbnails.
VNDegen uses the same folder and format:

- A slot is a run of length-prefixed records: the register file as it stood at the last `BLOCK`, with register 5
  holding the index of the command before the current line and register 10 the picture at that `BLOCK`, the script
  name, and records for the window, sound and portraits.
- Loading a slot restores the game's registers and replays the scene from its start to that line, with transitions
  finished at once, sounds and waits left out, and register commands before the `BLOCK` skipped. The original's
  saves load this way in VNDegen.
- VNDegen writes the same records, so its saves load on the PC (checked under Wine), and keeps its own exact state in
  `userdata/vndegen/slotNNN.sav`, which it uses when it matches the slot.
- Settings are kept in `userdata/vndegen/config.txt`.
- The PC keeps what has been seen in `userdata/<CLASS>.system.dat` (`CLASS` from `ExHIBIT.ini`), a register file of
  `N_SYSREG` numbers whose registers 189 to 192 hold the sizes of four tables that follow from register 193: one
  register per picture number (`N_CG`), one per scene (`N_SCENE`), then sounds and read text. VNDegen reads the
  picture and scene tables when it starts and sets the registers for what it shows, so Extra counts what was seen on
  either side. It writes only when that file is there, changing no other register.

## What runs

- The logos, the title (New Game, Resume, Load, Settings, Extra, Exit), the whole story to the ending.
- Messages with names, typing, voices, the wait mark, click to finish or advance, Ctrl or the control bar to skip,
  auto mode, the wheel for the backlog, right click for the system menu.
- Backgrounds, the camera, crossfades, mask wipes, sprites with faces, hops and shakes, portraits, the letterbox and
  blur of the heroine's-view scenes, H-scene speech bubbles, the end-of-scene fades.
- The control bar (voice replay, quick save and load, save, load, system menu, backlog, auto, skip, mute, hide, the
  window transparency slider) and the system menu (config, save, load, backlog, title, end game).
- Save and Load with ten pages, the auto and quick pages, thumbnails and the newest-save mark; the backlog with names
  and voice marks; the five config pages, of which window transparency, text and auto speed, the sound and voice
  volumes, per-character voice volumes, the countdown and the system sound take effect.

- Extra: the CG, Scene and Music pages, drawn from the game's own pictures (`res/g/sy/02`, the thumbnails in
  `res/g/sy/tn`). The CG groups come from `cgMode.rld`'s `CGMODE` list, the scenes from `res/c/replay1.rnf`, and the
  music list and its track numbers from `musicMode.rld` and the table `def.rld` builds with `CREATEDBINT`. A CG group
  opens once one of its pictures has been shown and steps through all of them; a scene opens once its `SETDONEFLAG`
  has run and plays from its script with `R1020` set, and the scene's own `exitReplay` brings Extra back; every track
  plays, with the music volume bar. VNDegen keeps what has been seen in `userdata/vndegen/extra.txt` and in the PC's
  `system.dat` (see Saves).

## Known limits

- The emotion icons that `MANPU` attaches to sprites (`.dlt` animations) are not drawn; see the clean-room notes.
- The CG viewer has no Full Screen or Original buttons.
- Returning to the title from the system menu doesn't ask first, as the PC game does.
- The backlog's Jump buttons aren't shown, and the config page's window-size and full-screen choices do nothing.

## Linux host

`Engines/ExHibit/host` builds `exrun`, which plays a game headless from a list of commands (`wait:ms`, `click:x,y`,
`rclick:x,y`, `move:x,y`, `key:vk`, `upto:textId`, `skipto:textId`, `state`, `shot:name`, `shots:every,count`) and
writes PNGs.
