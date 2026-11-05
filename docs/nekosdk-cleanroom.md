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

A `.sav` uses the archive entry container: the first 4 bytes XORed with the rolling key the archive uses, a zlib stream,
and the unpacked size in the last 4 bytes. This was found by testing the archive's scheme on saves the PC game wrote.

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
- A sprite's centre sits at the screen's centre plus its x and y, and scale is about the centre: fits of the recording give
  an offset of 0 at the expected scales.
- A timed change runs at once on what's on screen, as well as on the sprite waiting for the next update.
- A name on the same line as the text ends at `MsgNamePosX`. Test Game 97's name is right-aligned there.
- Text characters fade in one after another, about 20 ms apart and each over about 160 ms.
- Test Game 97's exe draws two bars no data file names: `obj\l000_000.bmp` at the top and `obj\l000_001.bmp` at y 618.
  Its message window image darkens nothing in the PC game, at the Config default of 100 or at 90.
- A shake is a burst of about 300 ms that dies away, by frame-to-frame motion in the recording. Type 4 moves sideways and
  type 8 moves both ways, inferred from the order of the bursts against the order of the commands.

## Readings not yet checked

- Parameter 20 of opcode 18 blurs a sprite. Its radius is a fit to one frame's sharpness, and parameter 21 isn't read.
- The flash's parameters 4 to 10, and motion blur's strength.
- Named animations (opcode 19): their lengths are taken as frames, because a save holds a timed change written in the same
  text form. A recording that reached them was too coarse to confirm it.
- `loop:0` repeating for ever is read from a street ambience and from `loop:1` on one-shot effects.
- `pos:` on sound commands isn't applied. Its values look like pan.
- What a load's parameter 0 means. A load keeps the slot's position and scale.
