# PJADV

VNDegen runs PajamasEX / Production PENCIL's PJADV engine ("2XT - Pencil Adventure Engine") on a runtime of its own,
`Packages/VNDegenKit/Sources/CPJADV`, written in C from the game's files, GARbro's MIT-licensed archive and picture
readers, and the original engine run under Wine. It is MIT licensed like the rest of the app. `docs/pjadv-cleanroom.md`
lists where each part came from, rule by rule for the minigame.

## Test games

| Game | Engine | Notes |
|---|---|---|
| Paradise Cleaning!- Tutor X Hypnosis - (Steam build, `Cleaner.exe`) | 2XT 4.5, 1920x1080 | Logos, title, stage select over four chapters, the cleaning minigame, the reward scenes with voices, gallery, options and help, in English, Japanese or Chinese |

## How the game is built

A game folder holds the exe, `archive.dat` (the scenario, its text, the resource names and the ini files), `bgm.dat`,
`graphic.dat`, `se.dat`, `voice.dat` and `save/`. The plugin takes a folder as a PJADV game when its `archive.dat` is a
`GAMEDAT PAC` archive holding `scenario.dat` and `textdata.bin`, up to one folder down.

- Archives: `GAMEDAT PAC2`, a count, 32-byte names, then offset and size pairs from the end of the index. Nothing is
  compressed. `textdata.bin` is XORed with a key that starts at 0xC5 and grows by 0x5C a byte. `archive.ini` lists the
  archives the engine opens; VNDegen opens every `GAMEDAT` archive in the folder, those first.
- Pictures: EPA, an LZ with a 16-entry table of back offsets over planar channels, 24 or 32 bits here. `.def` text files
  place a picture or a rectangle of an atlas (`filepos`) and name its animations; each language has its own picture
  (`srce`, `srcj`, `srcc`).
- Animations: `ANM1` runs tracks of moves, alpha and scale changes, waits, rectangle changes and random or fixed jumps;
  a block can fork a parallel track. `ANM2` holds a button's six states (rectangle changes, or a sound for hover).
- Script: `scenario.dat` (`PJADV_SF0001`) is a list of dword commands. A command's first dword gives its length in
  dwords, its operation, flags and category; operands are immediates (28-bit), three variable banks, eight
  temporaries and the last engine result. Categories: arithmetic and comparisons, jumps, calls and returns, waits,
  buttons and sliders, layers and tweens, transitions, messages, sound, achievements, and the minigame (`f1`).
- Text: `textdata.bin` (`PJADV_TF0001`) holds every string; the script points at them by offset. Characters outside
  ASCII are written `\U<count><hex>h`, the UTF-8 bytes in hex. The Japanese, English and Chinese scenes are separate
  scripts; the engine reports the chosen language and the script branches on it.
- The minigame lives in the exe, not the script. The script calls it with the chapter and stage and waits on a variable;
  `docs/pjadv-cleanroom.md` has its rules.

## What VNDegen does

- Language: English, or Japanese when the device uses it. The PC's startup language and display
  dialogs are skipped; the game always draws at 1920x1080.
- Layers stack in the order they were loaded, later in front, which the original's logos show. Screen changes made with
  the `80` commands wait for the next transition command, which crossfades.
- Buttons are polled: the script turns a group's input on, keeps looping, and a click runs the button's handler at the
  next frame. Sliders write their variable while dragged.
- English text shows 「」 and 『』 as straight quotes and an ideographic space as a space, as the original does.
- Controls: the Mouse layout, a pointer on the stick with clicks on the buttons, and the shoulder buttons (the wheel)
  for tools. Touch drags directly.
- A VNDegen addition: in the minigame the mouse wheel cycles tools, forward and back, so a controller doesn't have to
  reach the tool button. The original changes tools only with that button.
- Saves are the PC's own `save/syssave.dat` (the script's global variables, which hold the options) and
  `save/syssvpo.dat` (the cleared stages and their times), written when a stage is cleared and, when the options or
  other globals changed, when the game ends, whether by its own EXIT or by Quit to library, and they load both ways.
  The game has no save slots; `savedata.dat` stays empty.

## Known limits

- Which minigame sound plays for which event is a guess, and so is the exact timing of the clear flash and sparkles.
- How hard the original is about counting a second wipe made soon after the first isn't reproduced; VNDegen counts it.
- The Window mode option changes only its label.
- A missing `syssave.dat` is replaced by one that holds only the variables; the PC's flag bytes and settings after them
  start empty. The game ships with both files, so this only happens when they are deleted.
- Achievements are logged, not shown.
- The game's Chinese scenes aren't offered. The script's Chinese branch is left over from another Pencil game: it
  shows other characters and asks for pictures and voices (`BG305_01A`, `EH_A01_01`, `NTRP_*`) that aren't in this
  game's archives. The runtime still runs it when asked for language 3, black behind the text. Not checked in the
  original under Wine.
