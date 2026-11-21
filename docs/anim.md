# ANIM

VNDegen runs the ANIM engine's games on a runtime of its own, `Packages/VNDegenKit/Sources/CANIM`, written in C from
the games' files, GARbro's MIT-licensed readers and the original engine run under Wine. It is MIT licensed like the
rest of the app. E-mote characters are drawn through eluna, an MPL-2.0 Rust reimplementation of E-mote, behind
VNDegen's own `vnemote` interface (`Engines/ANIM/UPSTREAM.md`). `docs/anim-cleanroom.md` lists where each part came
from.

## Test games

| Game | Notes |
|---|---|
| Double Date Dilemma (`DDD.exe`, 2022, English) | Boot logos, title, story, choices, voices, music, loops, background voices, movies and the scene loops, E-mote characters, the flowchart and its jumps to either point of view, Save and Load both ways with the PC, quick save, backlog, the Volume screen |
| My Unique Reincarnation Skill 'Magical Sword' Makes Every Busty Girl Yield to Orgasm! (`mgsw.exe`, 2025, fan English patch) | Runs the same script engine; its title and screens are laid out differently and aren't built yet |

The two Stepmother Loves Her Son games were left out under the content rule.

## How the games are built

A game folder holds the exe, `<name>_sce.dat` (the compiled script), `<name>_define.dat` (sprite names, the anime
and expression presets, the flowchart, the gallery lists, the character table), `<name>_system.dat` (the system
save, rewritten in place by the PC), `cg/*.gax` pictures, `cg/psbpack.dat` (the E-mote characters),
`voice/<name>_wave.pck` (voices and sound effects), `bgm/*.eog` and `movie/*.mpg`. The plugin takes a folder as an
ANIM game when `<name>_sce.dat` sits beside `<name>_define.dat`, `<name>_system.dat` and `cg/psbpack.dat`, up to one
folder down.

- Encryption: the three `.dat` files, the `.gax` pictures and the save files share one scheme: `00 00 00 01`, a
  16-byte key, then a rolling XOR over 16-byte blocks.
- Pictures: PNG in 2048 by 2048 textures, the image at the top left. The screens are atlases; their pieces are
  placed where the PC draws them.
- Script: a code block, a table of named labels and a string pool. Instructions carry typed operands (`40` and `41`
  variables, `43` constants), raw numbers, string indexes and addresses. The scripts run the boot sequence, the story
  and the system routines (waits, effects, the scene jump table); the screens are the engine's own.
- E-mote: PSB files, encrypted with the engine's key, drawn at the slot positions and the scale the script sets
  (`66`). The define's character table (name and a percentage: Risa 100, Shiho 110, Yousuke 120, Kouki 130, Sumi 250)
  sets how far below the screen's centre a character's origin sits, 100 pixels times the percentage whatever the
  scale (a model not in the table, such as an event, counts as 100), measured against the PC; it doesn't scale the
  character. Expressions are presets of E-mote variables in
  the define.
- Text: Arial at 24 pixels (Liberation Sans or another metric match where Arial is missing), each glyph's advance
  rounded up to a whole pixel as Windows lays it out, wrapped at 884 pixels from x 180 (the PC keeps an 878-pixel line
  and wraps one of 891). Kanji and kana the font lacks come from the Japanese fallback (MS Gothic's substitute).
- Backlog: entries 46 pixels a line plus one blank line, the newest ending at y 666 and older ones above it, the line
  on screen included. Magical Sword draws it at 20 pixels with unrounded advances, the speaker's line 41 pixels below
  the name and indented 12 pixels; the scroll thumb's height is the share of entries shown.
- Sound: Ogg Vorbis behind an 8-byte header, in `bgm/` and the voice archive. Movies are MPEG-1.

## Drawing E-mote characters

The runtime draws on the CPU into the frame it hands the app. Each frame, eluna advances every character on screen, in
parallel, one character per thread: its motion, its expression changes, its blinking and its hair physics.
VNDegen's `vnemote` layer turns eluna's sprites into triangles: each sprite is deformed by its own mesh in its own
coordinates, then by each mesh-deforming ancestor in that ancestor's coordinates, nearest first, as E-mote's mesh
chain does. Each mesh is divided to about 8 pixels on screen
rather than the mesh's own division, which is far finer than the characters' size on screen needs. The triangles are
drawn in horizontal bands across a small thread pool. A character whose triangles didn't change since the last frame,
drawn at full opacity in place with normal or additive blending, is kept in a layer of its own and composited, so a
scene of characters standing still costs little more than its background. eluna builds each frame's scene once, with hair and
bust physics taken from the frame before, and the drawing bands skip sprites and triangles outside their rows. Three
characters swaying in their idle motion hold 60 frames a second in the Mac app on the Mac mini (35 to 45 on its
iPad simulator, which draws a still picture at only 50; `docs/frame-rate.md`).

Characters play the PC's idle motion, measured under Wine. Each E-mote layer names an idle in the save (`sPauseWait01`
unless set otherwise); the define's type-10 records list its variables and ranges (`head_UD` -10 to 10, `body_UD` -20
to 20). Every 0.8 to 1.9 seconds each variable heads, in a straight line, for a random value in the other half of its
range from the last one, so the character sways up and down; the idle replaces those variables' scripted values, as on
the PC. After 40 seconds without a click or key the wider `sPoseWaitPause` record joins in (head and body turns, head
tilt, and its own nods; the PC leaves its `body_slant` still), and the next input holds its variables where they are
until the next 40 seconds pass.

## Building the E-mote library

`scripts/anim-emote.sh` builds `Engines/ANIM/vnemote`, eluna behind VNDegen's C interface (`include/vnemote.h` in
CANIM), as `build/anim/VNEmote.xcframework` for the Mac, iPhone and iPad and the simulator, with the licenses of eluna
and each crate it builds in. `scripts/build.sh` runs it before every app build. It installs the pinned Rust toolchain
and the Apple targets through rustup when they are missing (`brew install rustup` first), builds offline from the
crates vendored in `Engines/ANIM/vendor`, and does nothing when its inputs haven't changed. The runtime finds the
library at run time and draws no characters without it. On Linux, `Engines/ANIM/host` builds the runtime, and vnemote through cargo,
for its headless and SDL test tools.

## Saves

The PC engine's own files in the game folder: `<name>_save.dat` (180 slots of 248,216 bytes, the first 160 on the
Save and Load screens), `qSavedata.dat` (the quick save), `cg/pSaveThumbNNN.png` (256 by 256, the screen at the top)
and `<name>_system.dat` (read lines, the flowchart's passed and read nodes, the last slot). A slot keeps the date, the
comment, the text instruction it was made at, the background, each layer's picture or character, offset, scale and
E-mote variables, and the script's local variables. VNDegen writes those, and the defaults the PC keeps in every slot:
the slot positions, the layer tables of 1.0 scales and -1 handles, the characters' idle motion name (`sPauseWait01`)
and the E-mote scale. The PC needs them: without the defaults it crashes loading the slot, and without the layer
scales it loads the scene with no characters. The comment is the speaker and the line, cut to 100 characters as the PC
does; a longer comment that holds a line break loads black on the PC. Saves load both ways with the PC.
A card's DELETE and the Reset Saves button clear the slot, or all 180, to zeros after a confirmation, as the PC
does; the thumbnails stay in `cg/`, and a used slot without one shows the PC's NO IMAGE picture.

## Settings

Settings and Volume keep their values in the PC's own `<name>_system.dat`, where the PC keeps them, so they carry
between the PC and VNDegen: each option a byte (Screen Effects at 0x01, Skip Unread at 0x02 and so on, worked out by
changing one at a time under Wine), the volumes as percentages from 0x18 (master, music, sound effects, the two
ambient volumes), the window opacity as 0 to 217 at 0x298, and the text and auto speeds as delays at 0x27c and 0x280.
Each character tab's voice volume sits at 0x21 plus the tab, and its background voices and their speech volume in two
more arrays, at 0x2b and 0x2f in Double Date Dilemma (two tabs with background voices) and at 0x2b and 0x34 in Magical
Sword (eight); the last tab, Others, has only a voice volume. A voice or background voice belongs to the tab its
character letter names (the last letter of a voice's name, the tag of a background voice). Magical Sword keeps every
option at Double Date Dilemma's byte; its Voice Cut is Skip Voice. They're written when the player leaves either
screen. Screen Mode isn't stored there, and VNDegen has no use for it.

The info line at the top of both screens follows the PC, and keeps the last control's line when the pointer leaves it,
as the PC does. Magical Sword's English patch redrew the info strip with one line per option, which the PC draws a
little smaller than the strip. Settings' text box sample is typed at the chosen speed in the window colour at the chosen
opacity: Magical Sword's comes from its define file (in Japanese, as on the PC, drawn with the Japanese fallback font),
Double Date Dilemma's from its exe. The Shortcut Key List button shows the PC's list over the screen. The menu bar's
lock icon and the Voice Wait and Auto Wait gauges are drawn as on the PC, the gauges when their options are on.

## Known limits

- E-mote characters blink and their hair moves, but their timelines and the scripts' short pose animations (`b9`,
  `bc`, `be`, `c2` to `c5`) don't play yet. eluna needed fixes to draw them like the PC (`Engines/ANIM/UPSTREAM.md`);
  with the PC's idle held still, faces, eyes, mouths, head and body turns, nods and tilts match it.
- The second swimsuit event (NOM04) shows a few backdrop tiles wrong: a lighter block at the lower left and a darker
  one at the right, where the PC shows the floor. The first (NOM03) matches the PC.
- The Extra screens (CG, scene and music galleries) aren't built. Settings' font list is empty and its choice does
  nothing, and the volume screen's Play and Allow Test buttons are drawn but do nothing.
- Magical Sword: the title, message window, menu bar, Config, Volume, Save, Load and the message log are built; its
  other screens use Double Date Dilemma's layout for now.
- The Load screen's AUTO SAVE page isn't built, and the PC's comment editing on cards isn't either.
- The PC's screen effects (shake, flashes other than the colour flash) are approximated.
