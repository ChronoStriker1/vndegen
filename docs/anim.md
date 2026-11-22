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
| My Unique Reincarnation Skill 'Magical Sword' Makes Every Busty Girl Yield to Orgasm! (`mgsw.exe`, 2025, fan English patch) | The same engine with its own layouts: title, HELP page, message window and menu bar, Config, Volume, Save and Load both ways with the PC, the message log, choices, the opening's camera pans and zooms |

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
  character. A model takes the entry its name starts with (Magical Sword's `Malg202` is `Mal`, 40), as the PC
  records in the save.
- Camera and layers: `80` places the background by the top left of its scaled picture (x, y, scale, time), so
  `80 #320 #180 #1500` is a centred zoom. `59` moves a layer by an amount, relative to where its last move ended, and
  `7c` scales it, both over a time; `52` puts a new layer back at no offset and 100%. Magical Sword's opening pans from
  the goddess's legs to her face this way, and matches the PC frame for frame. Expressions are presets of E-mote variables in
  the define.
- Text: Arial at 24 pixels (Liberation Sans or another metric match where Arial is missing), each glyph's advance
  rounded up to a whole pixel as Windows lays it out, wrapped at 884 pixels from x 180 (the PC keeps an 878-pixel line
  and wraps one of 891). Kanji and kana the font lacks come from the Japanese fallback (MS Gothic's substitute).
- Backlog: entries 46 pixels a line plus one blank line, the newest ending at y 666 and older ones above it, the line
  on screen included. Magical Sword draws it at 20 pixels with unrounded advances, the speaker's line 41 pixels below
  the name and indented 12 pixels; the scroll thumb's height is the share of entries shown.
- Pose animations: `b9 <id> <n>` and `b8 <id> <n>` start a block that runs beside the story up to its `c4`, while the
  script carries on after it: preset changes (`be`), moves relative to the last one (`bc`), scale (`c0`), sounds
  (`c3`) and waits (`43`), each on its own clock. `c2` sets a flag the main script reads (`L15`, the block finished),
  and `c5 <id>` stops a block early, after which the script applies the final pose itself. Each block runs once; on the
  PC neither the floating pig (`EmSetFloating`) nor `EmSetShakeLoop` repeats, and the pig rises at once whatever the
  second number. A new scene, a load or the title stops them. Measured on Magical Sword's goddess, the expression
  changes and the stepwise moves land within 0.1 second of the PC. The models' own E-mote timelines (`sample_00` and
  the like, left by the authoring tool) are never named by either game and don't play on the PC.
- Choices: `d0` lists the options by id; `b0 n on` before it turns option n on or off (a system routine turns all six
  on), which is how Double Date Dilemma's six-way choice opens its options one by one and Magical Sword drops an
  option already taken. One shown option sits at y 213, two at 175 and 305 (130 apart), in both games; three or more,
  which haven't been measured, are centred and closer when they wouldn't fit. They're drawn over the message window's
  background and menu bar without its text.
- Sound: Ogg Vorbis behind an 8-byte header, in `bgm/` and the voice archive. Movies are MPEG-1.

## Drawing E-mote characters

The runtime draws on the CPU into the frame it hands the app. Each frame, eluna advances every character on screen, in
parallel, one character per thread: its motion, its expression changes, its blinking and its hair physics.
VNDegen's `vnemote` layer turns eluna's sprites into triangles: each sprite is deformed by its own mesh in its own
coordinates, then by each mesh-deforming ancestor in that ancestor's coordinates, nearest first, as E-mote's mesh
chain does. Each mesh is divided to about 16 pixels on screen
rather than the mesh's own division, which is far finer than the characters' size on screen needs. The triangles are
drawn in horizontal bands across a small thread pool. A character whose triangles didn't change since the last frame,
drawn at full opacity in place with normal or additive blending, is kept in a layer of its own and composited, so a
scene of characters standing still costs little more than its background. eluna builds each frame's scene once, with hair and
bust physics taken from the frame before, and the drawing bands skip sprites and triangles outside their rows. Three
characters swaying in their idle motion hold 60 frames a second in the Mac app on the Mac mini (35 to 45 on its
iPad simulator, which draws a still picture at only 50; `docs/frame-rate.md`). Magical Sword's goddess has 1,051 sprites, three times
a Double Date Dilemma character: alone she holds 46 frames a second in the Debug Mac app, and 26 at 300% in the opening's
close-up, where most of the time goes to drawing her pixels. 16-pixel cells, rather than 8, scored the same on the face
checks and cut her vertices to under a third.

Masks are drawn as stencils: the sprites a layer's stencil list names (an inner mask passes inside them, an outer one
outside), and for a composite owner (`stencilType` bit 2) every source in its composite list, drawn at full opacity
even when the source itself is invisible. A sprite under a composite owner that isn't on its stencil chain (eluna's
`Filtered` pass, such as the goddess's irises under `ステンシル(shirome)`) takes that owner as an inner mask. Textures
come as 32-bit RGBA, or 8-bit `A8` (white with that alpha) and `L8` (grey), which NOM04 uses for most of its
trimming-frame tiles.

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
E-mote variables, and the script's local variables. A layer's y is kept as its offset plus its character-table drop (Malg202 at -1500
saves -1460), its scale from `7c` beside it (0 for 100%), and the camera from `80` as an on flag, x and y, scale and
time; a new background turns the flag off and leaves the old values, which the PC then ignores. Saves made in Magical
Sword's 300% close-up load zoomed both ways. VNDegen writes those, and the defaults the PC keeps in every slot:
the slot positions, the layer tables of 1.0 scales and -1 handles, the characters' idle motion name (`sPauseWait01`)
and the E-mote scale. The PC needs them: without the defaults it crashes loading the slot, and without the layer
scales it loads the scene with no characters. The comment is the speaker and the line, cut to 100 characters as the PC
does; a longer comment that holds a line break loads black on the PC. Saves load both ways with the PC.
The comment is kept in Shift-JIS, as the PC reads it. Magical Sword's PC writes the speaker and the line without a
break, and its cards show the first 40 bytes broken at 206 pixels wherever that falls, mid-word included; Double
Date Dilemma's show two lines of up to 20 characters broken at spaces. Magical Sword autosaves before each choice (op `15`, which Double Date Dilemma's PC doesn't act on): slots 160 to 167
in turn, the last one used kept at 0x0c in the system save (-1 before the first), the slot resuming at the choice
itself. Its Load screen's AUTO SAVE button shows those eight as cards 161 to 168. The PC's own autosave thumbnail
comes out black under Wine; VNDegen's shows the scene. A card's DELETE and the Reset Saves button clear the slot, or all 180, to zeros after a confirmation, as the PC
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

## Extra

The Extra screen's CG, Animation, Scenes and Music pages follow the PC in both games, which lay them out alike. The define lists
every CG as a numbered type-2 record (`HEL01aa` is 0), grouped by the first five letters of its name, and every
memory as a type-8 record with its title and blurb. The thumbnails are ready-made pages (`p_cgtnNN`, `p_metnNN`),
24 cells of 256 by 144: rows alternate between the sepia pictures and the coloured ones the pointer brings up. A
CG counts as seen once the script shows it as a background (`4b`), a layer (`52`) or names it (`1f`, Double Date
Dilemma); a memory once the script passes its marker (`02 n`, just before the `MemNN` label). Both are kept in the PC's
`<name>_system.dat`, four bytes each, CGs from 0x25c8 and memories from 0x3888 + 4 × n, so unlocks carry both ways
with the PC. The viewer steps through every unlocked CG from the clicked group onwards; a right click returns. A
memory replays through the script's own `CheckMemory` routine with `L4` set to its number and returns to the Scenes
page at its next marker, or when the script would end. Animation lists the define's movies (type-3 records, `HEL03aa.mpg` is 0), each unlocked once the
script plays it as a scene loop (`95`, `96`), kept at 0x3ba8 + 4 × n; its thumbnails are `p_mvtnNN`. A movie loops full
screen; a click moves to the next unlocked one and a right click returns. The PC's own player crashes under Wine on
any click, so what clicks do there wasn't seen. Music plays the bgm files in the PC's order (Double Date
Dilemma's are numbered 01-07, 20, 21, 30, 31), on to the next unless Repeat is on, and the title music comes back
on leaving.

Sprites is the PC's character viewer. The define holds its lists after the movies: the characters, the backgrounds
(from `spRoombg01`), each character's expressions, each character's outfits (an E-mote model and an icon number) and
the arm presets that go with them. Double Date Dilemma's records use other type numbers, so they're found by their
shape. A choice loads the outfit's model, applies `AllReset`, `emViewDefault1` (hair on) or `emViewDefault2`, the
arm preset and the expression, then the sliders. The character stands at the screen's centre at half the story's
scale. Each slider is a knob offset of 0 to 180 pixels; the variables and the numbers beside them were read from the
PC under Wine, knob by knob:

| Slider | Variable | Number shown |
| --- | --- | --- |
| Zoom | scale 500 + 1500 × px / 180 | 50 + 150 × px / 180 |
| Head and body tilt, eyes up/down, left/right | px / 3 − 30 | px / 3 |
| Eyes | px / 3 − 10 | px / 3 |
| Eyebrows | ⌊px × 53 / 180⌋ | the same |
| Mouth | ⌊px × 46 / 180⌋ in steps of 10, 46 at the end | the same |
| Mouth open | px / 36 | its whole part |
| Blush | px / 90 | its whole part |

Tears set `face_tears` to 30, the crest (Magical Sword) `fade_x` to 1, and Idle Motion picks `sPauseWait01` or
`sNoIdle`; Lip Sync moves `face_talk`. Save and Load keep one setting per character in the PC's system save: the ten
knob offsets at 0x7e9e4 + 40 × c, the outfit at 0x7ee94 + 4 × c, the expression's name at 0x7ef0c, the expression and
background at 0x7efd4 and 0x7f04c + 4 × c, then tears, hair, crest and lip sync from 0x7f1b4, 0x7f22c, 0x7f2a4 and
0x7f31c + 4 × c. Saves load both ways with the PC. Idle Motion isn't part of a saved pose: on the PC, Load leaves it
as it was (Magical Sword under Wine, saved with it on, loaded with it off), and VNDegen does the same. The camera button writes the scene without the controls to
`SaveGraphic/emotesave_NNN.png` in the save folder, numbered from 000 as on the PC (whose own file comes out black
under Wine). The folder button beside it opens that folder, in Finder on the Mac and in Files on iPhone and iPad, as the
PC opens it in Explorer ("Save image and open folder." on its info line); it writes no picture itself. The info line shows the help for the row under the pointer, and Double Date Dilemma's Lips and Move
buttons have their own on and off lines.

## Known limits

- `bd n` and `bb n` are skipped. `bd` appears 22 times across both games, each time inside a pose block just before an
  expression preset with the same duration (`bd 400`, then `be` with 400 ms); `bb -1` runs once at the start of each
  script. Two PC recordings of the goddess's `bd 400` (the line after she grants the skill) show nothing beyond her
  usual blinks, which the PC times the same way on every load, so no visible effect could be tied to either. A character's first frame takes its preset
  at once, where the PC eases into it during the scene's fade-in. eluna needed fixes to draw them like the PC (`Engines/ANIM/UPSTREAM.md`);
  with the PC's idle held still, faces, eyes, mouths, head and body turns, nods and tilts match it.
- The second swimsuit event (NOM04) shows faint seams between its backdrop tiles that the PC doesn't.
- When a line hides one layer and shows another, the PC holds the old picture for about half a second and then
  crossfades over about 0.3 s; VNDegen swaps them at once (seen when the goddess returns after the skill list).
- Settings' font list is empty and its choice does nothing, and the volume screen's Play and Allow Test
  buttons are drawn but do nothing.
- Double Date Dilemma's memory blurbs are drawn about 1.5% narrower than the PC's, so a line can break one word
  earlier.
- The goddess (Malg202) draws her lids too low: her eyes look half closed where the PC's are open, in the story and in
  the Sprites viewer. Her irises now stay inside the eye whites. She is on screen in the opening (`000-000`, `000-010`)
  and in `060-040`/`060-050`, plus the `Mem30` replay (Malg200, Malg203). Her eye motion's time follows
  `face_eye_open` (rest value 2, time 12); our render matches the PC's open eyes only near time 0. The parameter
  mapping, three readings of the null mesh keys in her lid and iris meshes, and leaving lid sprites unwarped were
  tried and none matched; the cause is somewhere in how the mesh chain deforms her nested eye layers. Other
  characters' eyes match the PC.
- Magical Sword: every screen is built to its own layout and checked against the PC. Its goddess, with three
  times the sprites of a Double Date Dilemma character, runs at 46 frames a second alone in the Debug Mac app and 26
  in the opening's 300% close-up (34 and 20 on the iPad simulator). A Release build on the iPad simulator updates the
  screen about as often (32 and 20, counted from screen recordings), so the close-up isn't smooth there; a Release Mac
  app hasn't been watched, since its library and input can't be set from a test run.
- The PC's comment editing on save cards isn't built.
- The PC's screen effects (shake, flashes other than the colour flash) are approximated.
