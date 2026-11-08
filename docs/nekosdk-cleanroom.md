# NekoSDK runtime: clean-room provenance

VNDegen runs NekoSDK games on its own runtime, `Packages/VNDegenKit/Sources/CNekoSDK`, under VNDegen's MIT license. No
runtime, partial runtime or engine documentation exists for NekoSDK. This file lists where the runtime's knowledge came
from and which meanings are settled. The rule is in `docs/roadmap.md`'s Licenses section: only permissively licensed code,
and nothing copied or ported from GPL, LGPL or unlicensed code.

## Sources

### What the runtime's author used

- **The games' own files.** Test Games 95 and 97, and Test Game 82 during planning: archives, scripts, `init.ini` and the
  other configuration files, pictures and sounds.
- **The PC games under Wine, as black boxes.** Headless gamescope for stills and input, Xvfb with `x11grab` for video, on
  CachyOS. Real saves made there, and recordings of the opening of Test Game 97.
- **The executables, by strings and imports only.** `strings` for the asset names and scene lists, and the import table.
  No executable has been disassembled or decompiled. Any disassembly needs the coordinator's approval first.
- **VNDegen's own earlier code.** `audio`, `font` and `sjis` are adapted from the System 4 runtime. `stb_image` and
  `stb_vorbis` (public domain or MIT) are the same copies System 4 uses, and zlib (zlib license) unpacks archives.
- **VNDegen's own feasibility test,** `docs/nekosdk.md` and its Python tools on branch `ws/nekosdk`.

The runtime's author opened none of the outside projects below.

### What the earlier feasibility thread read

The feasibility thread read or fetched these before the runtime was started. It reports copying no code from any of them.
This list is from its notes in `docs/nekosdk.md`.

| Project | Commit | License | What the thread did with it |
|---|---|---|---|
| [GARbro](https://github.com/morkt/GARbro), `ArcFormats/NekoSDK/` | `b09ee45` | MIT | Read for the NEKOPACK4 archive layout and keys, and BMP plus ALP alpha. Reimplemented in its Python tool |
| [FuckGalEngine](https://github.com/Inori/FuckGalEngine), `NekoSDK/` | `c966c2b` | None | Kaitai spec read for the script node layout. Its Ruby converter's seven character positions went into the prototype, unverified |
| [nekosdk_tools](https://github.com/adsf0427/nekosdk_tools) | `17eb326` | None | README and the same Kaitai spec read |
| [GalTransl-GalGameReverse-Skill](https://github.com/GalTransl/GalTransl-GalGameReverse-Skill), `engines/nekosdk.md` | `442ae2f` | GPL-3.0 | Prose notes on the archive and text records read, no code |
| [SExtractor](https://github.com/satan53x/SExtractor) | `8d8d976` | GPL-3.0 | Cloned, not read |
| [1F1E33-float32/Tools](https://github.com/1F1E33-float32/Tools) | `fc74ca2` | None | Cloned, not read |
| [lossless-text-repack](https://github.com/SuQiandYing/lossless-text-repack) | `c02b86b` | MIT | Cloned, not read |

What the runtime keeps from that, and how each was settled:

- **The seven character positions** from the unlicensed converter aren't in the runtime. When a game that uses the old
  character commands comes up (stage 4), they get measured from the PC game under Wine.
- **The script node layout** was re-checked against the bytes rather than taken on trust, as the next section says. It is
  a format fact the files themselves show.
- **The archive layout** is GARbro's (MIT), a format fact checked by unpacking every entry.

## The script node layout, re-checked

`Engines/NekoSDK/host/tools/nekocheck.cpp` reads every script of a game and requires three things: each file parses to its
exact last byte, each `next` names a node, and the numbers each node shows in its readable first string (`time:`, `type:`,
`ch:`, `vol:`, `pos:`, `tm:`, `loop:`) equal its parameters. That last check is independent of any outside description:
the editor wrote the same value twice, once as text and once as a number, so the layout is right if they agree.

| Game | Scripts | Nodes | Unreadable | Bytes after the last node | `next` naming nothing | Fields compared | Differing |
|---|---|---|---|---|---|---|---|
| Test Game 97 | 7 | 2,446 | 0 | 0 | 0 | 3,056 | 0 |
| Test Game 95 | 93 | 31,494 | 0 | 0 | 0 | 27,537 | 0 |

## Saves

A `.sav` uses the archive entry container: the first 4 bytes XORed with the rolling key the archive uses, a zlib stream, and the unpacked size in the last 4 bytes. This was found by testing the archive's scheme on saves the PC game wrote.

Inside is a stream of fields, mostly zero. `Engines/NekoSDK/host/tools/nekosave.cpp` reads a save and writes it back. Test Game 97's saves come back with at most 4 bytes different in about 140,000, and Test Game 95's with 9 or more. Those bytes are state the runtime doesn't use, on the last sprite record.

| Field | Where | How it was settled |
|---|---|---|
| A first byte of 1, then 204 zeros | Start | All saves |
| Flow script name, u32 length and bytes | 0xCD | Read from the file. An older game with no flow scripts has length 0 |
| 4,196 bytes, a choice picture's name, then 39,596 bytes | | Test Game 95's saves after a choice hold the picture named by its choice command |
| Date and time as six ints, in 400 bytes | | The save screen shows them |
| 1, then the flow node of the scene call | | Test Game 95's node 6 and 12 match the scenes each save resumes |
| 10,000 ints: the scenario's variables | The last 40,000 bytes before the sprite count | `macrolist.ini` names 110 and 4000. A save after a choice that raises the counter holds 1 at 110 |
| Sprite count, then 40 bytes, then records | | The root record has slot -999999 and the window's size |

A sprite record is a slot, two picture paths, then fixed fields: a matrix, x and y, scale, the anchor (the point a scale keeps still, which the section on placing pictures covers), alpha out of 256, whether blur is on and its amount, whether an animation runs, how many frames in, its text, then the sprite's end state. Test Game 95's record has 69 more bytes than Test Game 97's and Test Game 82's first fields agree with the older layout.

After the records come the text on screen and the last 60 lines of the backlog, newest first, then 243 sound channel records (60 of music, then sound effects), each a playing flag, path, volume, pan and position in seconds as a double. Then the scene script's name and the node to run next.

**Checked both ways on Test Game 97 under Wine:** a save the PC wrote loads in the runtime, shows the same text, and moves on to the next line. A save the runtime wrote appears on the PC's load screen with its date and thumbnail, loads, shows the same text, and moves on. Test Game 95's saves load in the runtime. A save the runtime writes for Test Game 95 hasn't been loaded on its PC copy yet.

`cfg.sav` holds the PC's settings, with the 10,000 global variables at byte 4000 (a macro of 1000 sits at byte 8000 after the prologue). The runtime keeps the rest of the file as it found it. `read.sav` isn't read or written, so PC read marks don't carry over. Config's settings are kept in a small file of VNDegen's, so they don't move to or from a PC.

The same file keeps what's been unlocked, read by comparing PC files with what the PC's galleries show: an int at byte 48000 plus 4 times each picture's number in `cg_view.ini`, and an int for each track in `sound_view.ini` from byte 100000. Both move by the length of a string at byte 44020 that names a scene: a file the PC has just made has none and is 104,000 bytes, and one with a 22-character name has its tables 22 bytes later. Test Game 95's file with 5, 4 and 7 pictures seen in its first three entries, and its 10 tracks heard of 15, gives those numbers, and Test Game 97's file agrees on the tracks. A CG row in Test Game 97 is assumed to match.

When no `cfg.sav` exists the runtime writes the values the PC game wrote on a first run under Wine, listed in `tables.cpp`, because a file of zeros sets the PC game's volumes to 0 and turns its confirmations off.

Quick saves are ordinary saves in slots 81 to 88, which the Q-Save page after the ten pages of eight shows. Under Wine a session's first quick save overwrote slot 81 and its second went to slot 82. That a page's eight fill and then wrap is assumed, and so is quick load taking the newest. A card's lock is a global variable, 200 for the first card and on from there: locking a card changed nothing while the game ran and, in the `cfg.sav` the PC wrote as it quit, only global 200 for the first card in The Alchemist, Yarinaoshi no Christmas and Test Game 92, and 204 for Test Game 94's fifth. The next session opened with the card locked: its lock closed and its Delete disabled. The runtime keeps the lock there, refuses to save over or delete a locked card, and passes it over for a quick save. NEW marks the card saved to last, which the PC keeps in global 0 and writes as it quits: after saving to Christmas's third card and quitting, its file held 3, and The Alchemist's held 1 with NEW on its first card, though another save was newer. The question's "don't ask again" box, in both games, turns that question off whichever answer follows: under Wine in Test Game 94, a ticked box and No, and the next save went through unasked. The runtime turns off Config's setting for that question.

A card's comment, typed on the PC's save screen, is the string after the save's first byte: with an input method under Wine, typing あいう on a card of The Alchemist and of Test Game 94 rewrote that save with 6 and the three characters in CP932 at byte 1, and everything after, the flow script's name first, 6 bytes later. With no comment the length is 0 and the flow's name is at 0xCD, the fixed place the runtime had read, so a save with a comment didn't read and showed as an empty card. The runtime reads the comment now and shows it on the card in white (`slotcomment`, placed and sized against the PC's: 21 pixels in The Alchemist, 13 in Christmas), and writes an empty one. The PC writes クリックでコメントを入力 or Click to set a comment on a card with none, a prompt to type one, which the runtime leaves out while it can't take one.

## Meanings settled by comparison with the PC game

Each was measured from recordings of Test Game 97 under Wine: its opening, and ten minutes clicking through the first
scene.

- Screen updates (opcode 9) don't stop the script. A 4000 ms wait right after a 2500 ms update ends the wait 4 seconds
  after the update started, not 6.5.
- Transition types 40 and 53 to 55 blur both pictures during the crossfade. Type 1 with no rule file is a plain crossfade.
- The timed change (opcode 25): parameter 24 is its delay and 25 its length, both in frames at the game's `FPS`, not
  milliseconds. A change of 3600 in a 60 fps game runs for a minute. A save the PC wrote about 36 seconds into a change of
  2400 holds 2140. Parameter 0 makes a move relative.
- Timed changes on one sprite run side by side. A character sprite given a relative -400 over 20 frames and then -100 from
  frame 20 over 180 frames sat at +80 and +48 on the way down and at 0 afterwards, by a masked fit of the recording.
- A negative length is `quadratic`: a save holds a change of -120 written as `relative_pos_y,120,0,-120,quadratic`, and the
  recording's steps along such a move fall in even amounts, which is a quadratic easing out.
- Parameters 16 to 19 of opcodes 18 and 25 say which of position, scale, rotation and alpha change: a command with a scale
  in 4 and 5 but no 17 left the background at scale 2.00 in a fit of the recording.
- On full-screen art, a sprite's position is the screen's top left plus its x and y, and scale is about the centre: fits of the recording give
  an offset of 0 at the expected scales.
- A timed change runs at once on what's on screen, as well as on the sprite waiting for the next update.
- A name on the same line as the text ends at `MsgNamePosX`. Test Game 97's name is right-aligned there.
- Text characters fade in one after another, about 20 ms apart and each over about 160 ms.
- Test Game 97's exe draws two bars no data file names: `obj\l000_000.bmp` at the top and `obj\l000_001.bmp` at y 618.
  Its message window image darkens nothing in the PC game, at the Config default of 100 or at 90.
- A shake is a burst of about 300 ms that dies away, by frame-to-frame motion in the recording. Type 4 moves sideways and
  type 8 moves both ways, inferred from the order of the bursts against the order of the commands.

## Individual Voices

`cfg\voice_type.ini` gives each speaker a type, `#type N`, and lists how the names of their voice files start. Types from 50 hold the background voices of the speaker 50 below. Recordings of a NekoSDK game's sound under Wine show that an unchecked speaker's lines and background voice don't play. The Alchemist's Audio tab has boxes for Mel and the two Misc types, drawn from a sheet of four columns of which the second is unused, and Yarinaoshi no Christmas's has one for Sonya. The boxes' places and their rows for on, off and hovered were matched against the PC game.

## Replays

A replay from the Scene gallery uses the bar `MsgButtonPlayViewDeclFilename` names in `init.ini`, `cfg\msg_button_playview.ini`, in place of the usual one. In every game seen it differs only in Exit's help picture, which says the gallery ends rather than the game. Under Wine, Test Game 84's Exit in a replay goes straight back to the Scene Library, with no question.

## Two nodes with one number

One script of Test Game 95 has two nodes numbered 1125. The first ends the scene and the second points at itself. A save resuming there on the PC game shows the first's line and goes on to the next scene, so the runtime keeps the first of any number and logs the fault. `nekocheck` fails on any such number that it doesn't list as already checked against the PC game, so run it on any game being added.

## Placing and scaling pictures

A sprite's x and y are the top left of the unscaled picture. Parameter 0 of the image load command places a picture: 1 centres it, 2 and 3 put its middle 256 to the left and right of centre, 4 and 5 put it 512 away, and parameters 1 and 2 add to that. It always sits halfway down. Every load with command 15 starts the slot afresh, at 0, 0 and scale 1 with parameter 0 at 0, and with full alpha, which Test Game 93's saves show. The rotation going back too is assumed. Command 23, a difference image, keeps the slot's place, scale and alpha. Parameter 0 of the set-position command, 18, makes its position a move from where the sprite is, as it does for command 25. These were checked by replaying scenes to the node each PC save names and comparing the sprite places the PC saved, in Test Games 84, 93, 95 and 97, at 1280 and 1920 wide, with C's division toward zero. Three sprites in a save of Test Game 88 inside a GOOD Timing script don't fit, and those are unexplained. Sprites kept from an earlier scene can't be checked that way. Scaling keeps a point of the picture still: parameters 6 and 7 of the scaling commands name it, and 8 and 9 say the middle instead, on each axis. The saved anchors match.

A picture's unlock flag in `cfg.sav` goes by its number in `cg_view.ini`, the middle field of its line, so a flag is 4 bytes at 48000 plus that number times 4. Every flag set in the PC files of Test Games 84, 88 and 93 is such a number, and those numbers don't track the entries.

The block after the sprite records holds a count at byte 22 and that many sprite numbers after the block's fixed part. The PC's save made at a choice names the choice as its flow node. Its later blocks also hold state the runtime doesn't write, and a save of ours naming the choice makes the PC run past it, so ours names the scene before, and the PC then asks the choice itself.

## Flow variables

A flow's condition (command 5) and change (command 6) name their variable by scope, parameter 0, and by its place in that scope's list in `macrolist.ini`. The places count in the names' order, not the file's: The Alchemist's file lists `GLB_PROLOGUE_OVER` (1000) before `GLB_ENDING` (1050), and its flow's `プロローグ完了` names place 1, which by name is 1000, the number a save the PC made after the prologue holds. Other games' flows show the same, such as Test Game 84's, whose chapter 1A is place 2, between `GLB_01_OVER` and `GLB_02_OVER`. VNDegen 0.26.1 counted in the file's order, so it set 1050 at The Alchemist's prologue's end (#59). The table's `implies 1050 1000` puts such a `cfg.sav` right as it's read: an ending means the prologue was seen. Checked under Wine: with the file repaired, the PC's Start offered Prologue or Main Story.

A condition that holds goes to the node's other target, the one a choice's branch takes for its option, and to `next` when it doesn't. The Alchemist's last flow settles it: `HAPPYEND_COUNTER <= 3 の時` has 正規ルートEND as its other target and 幸福ルートEND as `next`, and the counter goes up at the choices toward the happy ending. Test Game 84's `1:デス＆エス` likewise has デストラ＆エスケルダを選んでた as its other target. VNDegen 0.26.1 had it the other way round, so The Alchemist's endings were swapped (#58).

## Menus

The PC game's title, save, config, gallery and dialog screens are code in its exe. `CNekoSDK/core/tables.cpp` holds a table for each game: picture names and numbers only. The positions come from screenshots of the PC game under Wine, found by matching each picture against them, and `strings` on the exe gave which pictures each screen uses. Test Game 97's title and save screens match the PC's to within a few grey levels away from the disabled buttons and hover states. No exe was disassembled.

## Text placement

The PC engine draws its text from a glyph atlas: Wine's font log (`WINEDEBUG=+font`) shows each game creating its
text font at a cell height and at four times it, with the face init.ini's MsgFont names, and a second font for save
card comments. The Alchemist's official English release asks for Lucida Sans Unicode for both instead, which VNDegen
maps to Open Sans ([fonts.md](fonts.md)). The PC sets each glyph a fixed gap past its own advance and lines a fixed
pitch apart, which init.ini's MsgCharInterval and MsgLineInterval don't predict, so a game's table gives them
(`textgap`, `textline`, `textshift`, `textfont`, `textsize`, `commentfont`). Yarinaoshi no Christmas, measured by
loading the same PC save on the PC under Wine, with VNDegen's stand-in fonts mapped in place of the Windows ones, and
here: comments in MS Gothic at 18, and its log placed under its rules as the PC's is. Christmas asks for Meiryo at a
32-pixel cell, which Windows draws at about 21 pixels to the em, two thirds of the cell; Ume P Gothic, its stand-in,
fills the cell, so under Wine the PC's lines run off the screen. Christmas was released only in Japanese, so its text
was fitted to a Windows screenshot of its English patch, two lines whose glyphs have Meiryo's Verdana-like shapes and
an 11-pixel x-height: Ume P Gothic 22 with a gap of 4.66, lines 35 apart, the first glyph 2 pixels in and the baseline
4 lower than Wine's, which puts glyphs within 2 pixels, the lines' ends within one and the baselines on the
screenshot's rows. Its log's text drops by the same 4, inferred from the two faces' ascents. `nekocheck` lays out
every message and fails on one that runs off the screen. The Alchemist, from the publisher's Windows screenshots,
since Wine has no Lucida Sans Unicode and font-links it to Noto Sans CJK: Open Sans 22 with a gap of 5.93, lines 42 apart and the first glyph one gap in, which
puts capitals within a pixel of the screenshots; its comments at 20, sized from Wine's capture.

## The CG viewer

The panel's art, its buttons' places and their help lines were matched against screenshots of The Alchemist and Yarinaoshi no Christmas under Wine, and the panel, buttons and picture match the PC's to within blending noise. Both PC games behave alike:
- The zoom runs from 1.00 to 3.00 in steps of 0.05, from the buttons or a wheel notch, and keeps the point under the pointer still. Zoom out and reset are disabled at 1.00, and zoom in at 3.00. Reset is immediate.
- The slideshow moves on every 8 seconds.
- The entry's music, the fourth field of its line in `cg_view.ini`, starts with the viewer at the music screen's volume. Its button mutes and unmutes it while it goes on playing.
- A right click hides or shows the panel, and a click on the picture does nothing.
- Previous lands on an entry's last picture when it crosses into another gallery tab or wraps round, and otherwise on its first. Next always lands on the first.
- A disabled button shows no help.
- The Alchemist's art button switches the same setting as Config's Change Illustration and the message window's art button.

Slider fills round to the nearest pixel: at 60, The Alchemist's 173-pixel music bar fills 104 and Yarinaoshi no Christmas's 161-pixel one 97.

## Readings not yet checked

- Parameter 20 of opcode 18 is a blur that parameter 21 turns on: the PC game draws sharp with 20 alone. The radius is a fit to one frame's sharpness.
- The flash's parameters 4 to 10, and motion blur's strength.
- Named animations (opcode 19): their lengths are taken as frames, because a save holds a timed change written in the same
  text form. A recording that reached them was too coarse to confirm it.
- `loop:0` repeating for ever is read from a street ambience and from `loop:1` on one-shot effects.
- `pos:` on sound commands isn't applied. Its values look like pan.
- What a load's parameter 0 means. A load keeps the slot's position and scale.
