# SystemAoi runtime: where the knowledge came from

The SystemAoi runtime (`Packages/VNDegenKit/Sources/CSystemAoi`, hosts in `Engines/SystemAoi/host`) is being written
for VNDegen. It derives from no GPL or LGPL code. The game's exe and DLLs are run under Wine as a black box and not
disassembled; their strings were read for file names only.

| Part | Sources |
|---|---|
| Archives (`VF`, `VL`) | GARbro `ArcFormats/Aoi/ArcVFS.cs` (MIT): the header and the version 2 index. The `VL` part letter from the game's files |
| Script archives (`AOIMY01`) | GARbro `ArcFormats/Aoi/ArcBOX.cs` (MIT): the Unicode index and the offset-keyed XOR |
| Pictures (AGF) | GARbro `ArcFormats/Aoi/ImageAGF.cs` (MIT): the header and the five pixel operations |
| Voices (`AoiOgg`) | GARbro `ArcFormats/Aoi/AudioAOG.cs` (MIT): the wrapper's header |
| Bytecode | The game's 378 scripts: operand tags and their sizes worked out by tokenizing every script and checking that the jump targets fall on instruction starts (3,109 of 3,111 do). Opcode meanings from how the scripts use them |
| Tables (`obj.vfs`) | The game's files: the layout from the trailing counts, which multiply out for all 24 tables. Column meanings from the HTML manual's numbers (a 魔王亭 makes 30 money a turn) and from play under Wine |
| 3D models and maps | The game's files: Pegasus3D models are text. The maps' open-side mask from the symmetry between neighbouring cells |
| The game's rules | The game's HTML manual (`html/page/*.htm`) and printed manual, then play of the original under Wine |
| Saves | Diffs of the files the original writes under Wine |

## The town, rule by rule

Measured means read from captures of the original under Wine at 1280x720; inferred means chosen to fit what play showed
where play couldn't pin it down.

- **Facilities.** `TownBuil.obj` columns: 9 housing, 18 build cost in resources, 19 workers needed, 23 and 24 residents a
  turn, 25 and 26 food, 27 and 28 money, 29 and 30 resources, 34 resources used a turn, 35 to 37 pictures, 66 and 67 the
  card's texts. Measured: costs and workers match the manual; a 魔王亭 makes 30 money a turn and a field costs 150
  resources and needs 30 workers.
- **Grid.** `TownMass.obj`: 90 cells, 10 by 9, each with the stage that opens it (0 the 魔王亭's cell, 99 never).
  Measured: the original's first board shows the 魔王亭's cell and the twelve cells of stage 1.
- **Workers.** Inferred: residents go to facilities in grid order, and a facility works only when all its workers are
  free. Measured: with 5 residents a field shows 0/30 and makes nothing, and the town window shows 5 - 30 = -25.
- **Residents a turn.** Inferred: a random number between columns 23 and 24 for each working facility. Measured: from
  the same save, +2 and then +1 on the next turn, and +2 again when the first turn was replayed.
- **Food.** Inferred: one food a turn for every ten residents. Measured: 5 and 7 residents used none.
- **Placing.** Measured: a facility is chosen from the list and placed on an open cell; 決定 builds it and takes its cost
  in resources; the town window shows the result before 決定. VNDegen places with one click where the original holds the
  piece until a second.

## Battles, rule by rule

Measured from the original's battle log under Wine, read line by line from captures (the log is drawn in a 12-pixel
font that Ume Gothic at 12 pixels matches exactly), over six battles in 千樹庭's first area with Darkus at level 1.

- **Stats.** `unit.obj` columns 23 and 24 are HP and MP, and 25 to 32 physical attack, magic attack, physical
  defence, magic defence, speed, evasion, government and command, the order of the training screen's card
  (`grw_f00`). Column 15 is the unit's 格 (Darkus 0), column 17 its level, column 19 its growth type. Measured: Darkus
  shows 170 HP and 35 MP.
- **Growth.** `lvup.obj` gives each growth type's gains as text, such as `攻撃+1/4|防御+1/6|速度+1/12|回避+1/15`, one
  point for every so many levels. Inferred: counted from level 1, so a level 5 enemy of the common type has one more
  attack and no more defence. Every sample below fits that.
- **Damage.** A random number in the skill's power range (`skill.obj` columns 17 and 18), plus the attacker's
  physical attack, minus the target's physical defence. Measured: 31 hits by ten skills, each inside its predicted
  range. Darkus's 剣撃 (27 to 30, attack 18) did 44, 44, 36, 33, 45, 44, 43, 41, 42, 48 and 46 to targets of defence
  3, 12, 0, 2, 5 and 0; enemies' hits on Darkus (defence 17) matched the same way, such as ホネ's 突き刺し (25 to 28,
  attack 9) for 18 each time and フニ's ひっかき (25 to 28, attack 10) for 20 and 21. 斬撃, 打撃 and 射撃 skills all
  follow it.
- **Several targets.** 乱舞斬 (target 敵３) hit its first target for full damage and the second for half. Measured
  twice; how the third target and rows count is still open.
- **Healing.** 小回復 (38 to 42) healed 39 and 38: inferred as the power alone.
- **Hit and miss.** Not yet known: ツタ (accuracy 85) missed Darkus three times in four, more than its accuracy
  suggests, so evasion or speed takes part.
- **Rewards.** Measured: a win gives money, resources and experience in a box over the board; a loss ends the
  expedition and loses what it found.
