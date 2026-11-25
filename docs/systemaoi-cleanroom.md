# SystemAoi runtime: where the knowledge came from

The SystemAoi runtime (`Packages/VNDegenKit/Sources/CSystemAoi`, hosts in `Engines/SystemAoi/host`) is being written
for VNDegen. It derives from no GPL or LGPL code. The game's exe and DLLs are run under Wine as a black box and not
disassembled; their strings were read for file names only. The one exception is the save codec below, read with the
maintainer's approval.

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
| Saves | Diffs of the files the original writes under Wine; the codec from a reading of its two routines (below) |

## The town, rule by rule

Measured means read from captures of the original under Wine at 1280x720; inferred means chosen to fit what play showed
where play couldn't pin it down.

- **Facility levels.** Measured over 14 turns of the original: EXP is cumulative and a facility reaches level L
  when its EXP reaches `TownLvup.obj` column 2 of its group's row L (the 魔王亭 reached level 2 at 150 and kept
  counting). The check runs at the turn's end after the engine's +1 per staffed facility; the turn script's
  3 to 5 comes after it, so a level shows a turn later. A level adds every `TownLvup` row up to it to the
  facility's `TownBuil` values (column c adds to column c + 6), except housing: three level-10 住居 still housed
  200 each.
- **Posts and production.** Measured over 4 turns each (fields, shops and mines, holders' 統治 10 to 200): the town
  window's 収支 ignores posts, but at the turn's end each staffed facility in a post's area gains about 統治 × 0.3 to
  0.7 (one roll a turn) on its main product: 農業担当 on fields' food (+51%, +265%, +481% and +1040% at 10, 50,
  100 and 200), 商業担当 on shops' money, 工業担当 on mines' resources (not their money); 街長 the same on every
  area but the 魔王亭's money; 街長補佐 about half that, the 魔王亭 included. Inferred: 警備担当 covers 行政, 教育担当
  学園, 衛生担当 環境, and 娯楽担当 the 娯楽店, Ｈな娯楽店, スタジアム, プール and 酒造店 (which 商業担当 then
  leaves); the main product is the largest of the facility's food, money and resources columns.
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
- **Party diorama.** Measured against a turn 2 capture: the platform is `m996bt1a` from `bmap.vfs` at (-62, 504),
  drawn 1:1; the six slot tiles are the first 152x150 frame of `mfloor01`, front row at (108, 451) and back row at
  (27, 492), each 57 right and 29 down from the last; the five tiles on the step are that frame at half size, half a
  step apart from (-16, 588). A unit's sprite sheet is `u` and `unit.obj` column 11 (Darkus 9990), centred on its
  tile with its bottom 17 below the tile's centre; its HP and MP bars are two `bat_o00` frames 12 and 8 above the
  sprite, with `bat_o01`'s first three rows and `bat_o03` inside, 50 wide when full. The idle animation plays the
  sheet's filled cells in order, 100 ms each (measured at 30 fps under Wine). The 格 label is `mai_f00` at (1, 695).
  The platform, tiles, bars and map match the capture pixel for pixel. Inferred: which tile each slot uses, the half
  size tiles' filter (6 levels off), the label's digits (the original's are bolder than `dgn_f00`), and the diorama
  hiding with the windows.

## Script commands, from play

- **`f5 "<facility>" 1`.** Measured from the opening under Wine: the town map without its windows becomes the
  background, fading in, and the scene's background then fades in over it. Inferred: the fade's length, 300 ms.
- **`50 <x> <y> <mode>`.** Used once, in the prologue, on `bg999a0`, a 1280x1220 picture: `50 0 -500 0` then
  `50 0 0 1`. Measured at 30 fps under Wine: the picture, first shown at its top, scrolls up at a steady 40 pixels a
  second (25 ms a pixel) until its bottom shows; the script waits there, a click ends the scroll at once, and with
  skip held it runs about 16 times as fast. Inferred: mode 0 starts the move to (x, y) and mode 1 waits for it.
- **`f8` and `f9`.** From the script around them: they set a flag when BUNNY BLACK 1 or 2 is installed, and the
  opening then offers special units; VNDegen reports neither.
- **`ef 0 <n> <mode>`.** Facility EXP. Measured: the turn script opens with `2e CL8 3 5` and `ef 0 CL8 1`, and
  every staffed facility gains that 3 to 5 (with the engine's own +1, the 4 to 6 a turn the saves show; with no
  hooks registered only the +1 remains). Inferred: mode 0, which the damage events use with -40 to -30 before
  "経験値ダウン", scales the EXP by that percentage.

- **`8a`, the message box.** Measured from the original: the box from `chipB`'s `ret_a02` header at (430, 130), the
  `ret_a04` text area (text at (462, 178), 20 pixels a line) and the `ret_a00` foot with OK (`ret_a01`, normal and
  hover) at (565, 332). The turn's income box adds `ret_a03` (住民, 食料, 資金 and 資源 before, change and after)
  between the header and the text, says このターンの収支です, and shows rises in blue. The after column counts up from the
  before value (measured: 1066 on the way from 1060 to 1090); over a second (inferred). The script's last operand is
  the text; what the four numbers before it do is still open (the defeat hook passes four variables, the unit-join
  messages pass a number first, perhaps a picture for `ret_a05`).
- **`f1 N`** widens the town's build area to stage N (the turn script calls it with 建設エリアが広がりました once
  enough is built: stage 2 at 30 facility points, 3 at 70, from `f3`'s total). The engine keeps the stage in C490,
  which the turn script reads (inferred from those reads; no script writes it).
- **`c3 "role" var var`** asks which unit holds a post (街長 to 第七階層長), answering whether it's held and the unit;
  `bo3105` uses it for the appointment scenes.
- **Missions (目標).** The turn script runs the target scene (`fa0000.box`) once F143 is set, after `9e var` gives
  1 or more, then `9f var`: inferred as "a mission is due" and "start it". `mission.obj` lists マクドゥの試験 1 to 15
  with numbers that look like turns (8 and 9, 17, 26 and 27, …) and three amounts (200, 1000, 200 for the first),
  shown in the 目標 box and settled with 提出. F143 isn't set in the opening turns, so none was seen yet; VNDegen
  answers no mission due until a capture shows the rules.
- **Other commands seen in the opening turns.** `f7 N` takes N EXP from every facility (抵抗勢力の攻撃…経験値ダウン),
  `ee "facility" cell` places a facility (the 学園 at cell 21), `c9 var "unit"` gives a unit's level (inferred from
  its use against C590).
- **`bc 0 "unit" -1`** makes a unit join (74 uses: 味方ミアルテ, 味方ルーアルテ and the others), with an `8a`
  message naming them. VNDegen adds the unit to the roster and to the first free party slot (front left, front
  right, back centre, back left, back right, front centre; inferred until 配置 is built). Ally sprite sheets have
  their own grids (6 by 6, 4 by 3 as well as Darkus's 11 by 3): the frame size is read from the sheet's first row of
  frames.
- **Script hooks.** `init.box` registers a script per hook number with `72`: 0 `bo0000` (each turn), 27 `bo2700`
  (a battle's end, the result in C155: 2 is a loss, which says Darkus's line and shows the loss message), 30
  `bo3000` (town commands, C095), and others for the expedition still to map.

## Battles, rule by rule

Measured from the original's battle log under Wine, read line by line from captures (the log is drawn in a 12-pixel
font that Ume Gothic at 12 pixels matches exactly), over nine battles in 千樹庭's first area with Darkus at level 1.

- **Stats.** `unit.obj` columns 23 and 24 are HP and MP, and 25 to 32 physical attack, magic attack, physical
  defence, magic defence, speed, evasion, government and command, the order of the training screen's card
  (`grw_f00`). Column 15 is the unit's 格 (Darkus 0), column 17 its level, column 19 its growth type. Measured: Darkus
  shows 170 HP and 35 MP.
- **Growth.** `lvup.obj` column 24 gives each growth type's gains as text, such as
  `攻撃+1/4|防御+1/6|速度+1/12|回避+1/15`, one point for every so many levels. Inferred: counted from level 1, so a
  level 5 enemy of the common type has one more attack and no more defence. Every sample below fits that, and one
  needs it: a level 5 ベト's ベトハンマー (27 to 30, attack 7) hit Darkus for 21, one more than its base attack allows.
- **Experience.** Measured: the panel's EXP bar is 100 pixels wide and fills by EXP × 100 / 120, where 120 is the
  first number in the unit's growth row (`lvup.obj` column 2): 9, 21 and 43 EXP showed 7, 17 and 35 pixels, and 11
  and 21 showed 9 and 17. So level 2 takes 120. Measured on a 迷宮長 earning 16 EXP a turn (万能タイプ, 120 and
  10): levels 2, 3 and 4 took 120, 140 and 180, so level L to L + 1 takes column 2 + column 3 × L × (L − 1).
  Inferred: levels don't raise HP or MP until that is measured.
- **Damage.** A random number in the skill's power range (`skill.obj` columns 17 and 18), plus the attacker's
  physical attack, minus the target's physical defence. Measured: 44 hits by eleven skills, each inside its predicted
  range. Darkus's 剣撃 (27 to 30, attack 18) did 44, 44, 36, 33, 45, 44, 43, 41, 42, 48 and 46 to targets of defence
  3, 12, 0, 2, 5 and 0; enemies' hits on Darkus (defence 17) matched the same way, such as ホネ's 突き刺し (25 to 28,
  attack 9) for 18 each time and フニ's ひっかき (25 to 28, attack 10) for 20 and 21. 斬撃, 打撃 and 射撃 skills all
  follow it.
- **Several targets.** 乱舞斬 and 火矢三連 (target 敵３) hit their first target for full damage, the second for half
  and, once seen, the third for a quarter (乱舞斬's 12 where a first hit would have done 50 to 53). Against two enemies
  乱舞斬 hit each once, so VNDegen picks different targets; 火矢三連 once hit the same slime twice (a miss, then 20),
  which that doesn't explain yet.
- **Choosing a skill.** `unit.obj` lists up to eight skills from column 41 in threes: name, weight and a third number.
  Enemies' weights (ベト: ベトアタック 30, ベトシュート 10, ベトハンマー 10) fit how often each showed in the logs;
  VNDegen picks by weight, and heals only when an ally is at half health or below (inferred). Order in a round is by
  speed, ties at random (inferred).
- **Encounters.** `party.obj` lists each enemy group in three rows of eight columns from column 18: five cells, a
  list of candidates, then a minimum and a maximum. The first row is the front (前衛). A cell names a unit for that
  column; the game then draws between the two counts from the list and puts them in free columns. Measured on the
  fixed group 千固定0100: ベト, 二角ベト and ベト stood in the front row's columns 0, 2 and 4, two 一角ベト in the
  middle row's 1 and 3, and one 二角ベト from the back row's list (counts 0 to 1). Enemy keys such as ベトＡ are
  `unit.obj` column 1, with the level in column 17 and the sprite in column 11.
- **Board.** Measured: the party's platform (`m000bt1a` in 千樹庭) at (193, 299) with the town diorama's tiles, and
  the enemies' on a grid of three rows by five columns whose tile centres are (627 + 57k − 81r, 202 + 29k + 41r), row
  r from the back. `party.obj` column 17 picks the platform: 1 is `m000bt2a` at (406, 116) with columns 1 to 3, 2 is
  `m000bt2b` at (326, 130) with all five. Inferred: 10 is the larger `m000bt2c`, placed at (250, 80) until a capture
  shows it. Enemy sprite sheets (`u????a2`) pack two rows of frames at uneven widths; the runtime splits them at the
  gaps between the opaque parts. Floor tile frames: grey, red, blue (the party while orders are given) and yellow
  (the unit being given orders, or the targets a choice covers). The Backs and Forward panels (`bat_j01`, `bat_j00`) at (103, 541) and (603, 541) with a `bat_j02` box per slot, the
  preset buttons `bat_d00` to `bat_d11` down the left, 戦闘開始 (`bat_l00`) at (500, 497) and the log (`bat_m00`) at
  (1105, 270) with 12-pixel text on 18-pixel lines. The backdrop is the 3D dungeon, still to come.
- **Magic.** Inferred: the same rule with magic attack and magic defence. Darkus's 火矢三連 (fire, 28 to 30, magic
  attack 15) did 39 to a 二角ベト of magic defence 4 (predicted 39 to 41), and 20 to the second target; it did 36 to a
  ベト once, three short, which stays open.
- **身代わり.** Measured: a クマン (battle ability 身代わり, `unit.obj` column 34) took a hit of 乱舞斬 aimed at a ウルフ;
  the log said クマンが身代わりになった, ウルフに0ダメージ, クマンに58ダメージ. 58 fits the damage against the ウルフ's
  defence (55 to 58 with the sword), not the クマン's (50 to 53). `ability.obj` says it covers allies with less HP
  "時々" and gives 50 in column 4: inferred as a 50% chance. That 乱舞斬 logged only this one hit against two
  enemies; how many hits a several-target skill makes when fewer enemies stand is still open.
- **Healing.** 小回復 (38 to 42) healed 39 and 38: inferred as the power alone.
- **Hit and miss.** Inferred: a hit when a roll under 100 is below the skill's accuracy (`skill.obj` column 16) minus
  the target's evasion. Too few misses were seen to pin it down: ツタ (accuracy 85) missed Darkus three times in seven,
  and Darkus's 剣撃 (accuracy 100) missed a ドリツル (evasion 2) once in 16.
- **Orders.** Measured from the original's screens: clicking a unit's action button opens スキル選択 to the right of
  its box, anchored to the screen's bottom (`bat_e01` header, `bat_e03` rows 29 pixels apart, `bat_e02` foot at
  y 716): the unit's skills, each with its attribute icon from `pun_d00` (斬 打 射 無 光 闇 火 水 土 風 雷 回 異), name
  and MP, then 自由戦闘, 攻撃戦闘, 防御, 支援戦闘, 交代 and 逃走. The target button opens スキル対象選択 (`bat_g01` to
  `g03`): 敵 for a skill aimed at enemies, 味方 for one aimed at allies, 自由 otherwise, then 前衛, 中衛, 後衛, 右側, 真中
  and 左側. Choosing an enemy skill while the target is 自由 makes it 敵. Hovering a target lights its tiles: the back,
  middle or front row; on the right, the two highest columns; in the middle, column 2; on the left, columns 0 and 1.
  行動全自由 lights while every order is 自由戦闘. The other buttons set every unit's order or target, or save and
  load three sets of orders. Inferred, until a capture shows it: 攻撃戦闘 and 支援戦闘 pick by weight among attacking or
  supporting skills, 防御 halves damage taken that round, 逃走 ends the battle when that unit acts, an empty target
  area falls back to any target, the party's 右側 is the third column of a row, and 自動戦闘 starts each round by
  itself. ゲージON steps through `bat_p00`, `p01` and `p02` (ゲージOFF, 戦闘時可変), the last showing the bars only
  while a round plays (inferred).
- **Rewards.** Measured over five wins: the screen dims to 3/8 brightness and a box in the general popup frame
  (`pop_a00` to `a03`, edges only) centred on (641, 360) says 資源を*N*材手に入れた, *N*Pの経験値を獲得した and, once,
  使い込まれた杖を手に入れた, in white with the numbers and names in green or yellow, 24 pixels a line. Resources were
  1 or 2, the range in `party.obj` columns 63 and 64. EXP was 9 and 10 for a ドリツル, 12 and 11 for a ナメッド and 22
  for a クラム and a フニ, whose `unit.obj` column 72 values are 8, 10, 10 and 10: inferred as their sum times 110 to
  125 per cent. Inferred: an item drops with the chance in `party.obj` column 65 (10), from the items whose `item.obj`
  column 30 names the group's table (column 67, 落物00), weighted by column 31. No money showed in these battles.
- **Defeat.** Measured: a fallen ally is logged as ダークスが倒された. The screen after a loss wasn't captured; a
  loss ends the expedition and loses what it found.

## The dungeon view, from captures

Measured under Wine in 千樹庭's first area (`mp0101.amd`), from the start cell facing each way and two steps on.

- **Part set.** `floor.obj`'s column after the map name names the part set: 10 for 千樹庭 is `m010set.pvm` (forest);
  `m000set` is 炎山洞's lava. Floor, ceiling and wall parts are drawn per cell; walls are modelled on the cell's +x
  side and turned onto each side.
- **Map layout.** `mpDDFF.amd` holds a 48-byte header (word 1 the width, word 2 the height: 15 by 15 in 千樹庭, 16 by
  16 and 20 by 20 in 炎山洞, 5 to 20 in all) and then the cells row by row: cell (x, y), with y growing south, is
  record x + width × y. The wall fields are 9, 12, 15 and 18 for north, east, south and west (0 a wall, 10 a doorway
  the party walks through, 20 an edge passage to the next area, 100 likely a hidden door), each
  followed by its 移動 field; field 21 is the open-side mask (bits 1, 2, 4 and 8 for north, east, south and west).
  Measured: the original's C073 to C075, its saves' positions and the scripts' `d7`/`d8` coordinates all use this
  frame. Wall 100 stays a wall: measured, forty bumps into 中腹エリア's stair room from outside never opened it.
  `dunChang.obj` (100: 壁 0 to 100, 移動 0 to 1, 5) and 感知 ("迷宮で隠された扉の存在に気付く") suggest a hidden door
  that a 感知 unit reveals; a hundred and forty bumps without one never opened it, so not a chance per bump.
  VNDegen lets the party through wall 100, from either side, while a main member's field ability or a reserve
  member's reserve ability is 感知 (サキュー, スカイアン); inferred.
- **Camera.** `Cam.pvc`'s position and target (3.2 high, 4.6 behind the cell's centre, tilted down slightly), with
  a 47.2° vertical field of view: the floor lines and wall edges fit that best (correlation 0.58 against 0.30 to 0.35
  at other angles); the file's Zoom(40) isn't used as given.
- **Light and fog.** Inferred from colour fits: textures times 1.6 × (ambient 0.75 plus the two lights' diffuse), and
  linear fog to (40, 45, 30) from −4 to 28 units. Back faces are culled. A texture whose top-left colour covers a
  tenth of it treats that colour as transparent (the grass `010w0x0`).
- **Objects.** `mallset.pvm` holds the objects every dungeon shares (`action_box`): `a100` the gold-framed door,
  `a101` to `a108` the floor markers of pickups (101 to 104 glowing, 105 spent) and other marks. Measured: floor
  object 100 is the door home; it stands at its cell's centre and turns slowly, edge-on to face-on in about 4.5
  seconds (a turn every 18 seconds, filmed at 10 fps). A floor object N draws `aN` at its cell's centre (inferred
  for all but the door). Wall-20 sides draw nothing; the original floats a purple marker over such cells.
- **Action buttons.** Measured: standing on the door's cell shows 帰る (`dgn_g02`) at (1101, 498), above the pad.
  Inferred: floor part 100 (`f100`, stairs down) shows 下りる (`g01`) and 101 (`f101`, up) shows 上る (`g00`).
- **Areas.** `floor.obj`'s rows of one dungeon are areas side by side, named from the first: 中央森, 中央森の東,
  中央森の東南 and so on (`奥` one further the same way). Measured: stepping onto a cell with a wall-20 side moves at
  once to the neighbouring area (a LOADING badge, then an autosave), arriving one cell in from that area's passage
  on the facing edge at the same row or column, still facing the same way; 中央森's east passage at (14, 2) leads to
  中央森の東's west passage at (0, 2), and back. Every passage of 千樹庭's ten areas pairs up that way. VNDegen places
  areas on a grid from their names and checks the neighbour has the matching passage; failing that, it takes the
  one area that has.
- **Floor objects seen on the first floor.** 100 the door, 60 a yellow crystal (the original shows 開ける at one);
  neither is handled by the floor hook, so both are the engine's.
  `floor.obj` columns 36 onwards hold eight (table, minimum, maximum, count) entries such as 武器Ａ 1 1 1 and 宝箱Ａ 1 3
  1, whose names are `item.obj` column 30 tags. Inferred: standing on a crystal shows 開ける (`dgn_g07`); opening the
  floor's k-th crystal (in map order) gives between the minimum and maximum of items from the k-th entry, each drawn
  by `item.obj` column 31's weight, says 『name』を手に入れた。 and removes the crystal.
- **Minimap.** Measured: cells visited on earlier expeditions stay shaded on the next one. Its icons are 16-pixel
  tiles drawn twice the size: `mmf0003` the door, `mmf0004` a crystal, `mma0000` the party's arrow and `mma0001` an
  enemy group's red cross (drawn for groups up to a cell outside the 5 by 5 window, overlapping its edge). A passage
  side is a magenta bar. `mmf`/`mmw` hold more floor and wall tiles VNDegen doesn't use yet.
- **Start cell.** Measured: one cell from the door, through its open side, facing away from it ((6, 2) facing south
  in 中央森, every time).
- **Step counter.** Measured: the number under the compass is the expedition's steps, drawn with `dgn_f00` (digits
  10 pixels apart) right-aligned with its last digit at (994, 72). Turning doesn't count; crossing to another area
  doesn't either. The 目標/時間 panel shows at the top right as in the town.
- **Enemy groups.** Measured: each area's fixed groups (`floor.obj` column 30, 千固定0100 and 千固定0101 in 中央森)
  stand on the floor from the moment the party arrives, always on the same cells ((4, 12) and (14, 7) in 中央森,
  (12, 7) and (10, 0) in 中央森の東), with their members rolled afresh each time (`party.obj`'s random cells). They
  show only on the minimap, not in the 3D view, and move a cell at a time as the party walks, toward a target cell
  ((4, 1) for the first), then on to another. Walking onto a group's cell starts its battle; after fleeing, the group
  was elsewhere and every group's counter read 40. Leaving an area and coming back resets its groups. Inferred:
  groups move one cell per party step along the shortest path; a group that has fought waits out its 40-step count
  before it can fight again; a beaten group is gone for the visit. VNDegen's spawn cells are its own (eight or more
  steps from the arrival, chosen per area and group), as the original's rule isn't known. `a8 "name"` from the
  scripts adds a group the same way.
- **`bf var, N`** answers how many of dungeon N + 1's areas are offered as entries (below). Measured: the PC's
  turn-10 save has F751 (bf = 1) set and F752 (bf ≥ 2) clear although its party had walked into 中央森の東, which a
  count of visited areas would have made 2.
- **Entries.** `dungeon.obj` column 3 is the flag that opens a dungeon in the select screen (F300 千樹庭, F301
  炎山洞); `floor.obj` column 14 is the flag that offers an area as an entry (0 always, -1 never; 中央森 0, 中央森の東南
  F256, 炎山洞 中腹エリア F279, the 抵抗勢力拠点 F1902). Measured: after the 炎山洞 event (bo3001, on 遠征 at turn 10)
  the select screen lists 炎山洞 and its 中腹エリア, VNDegen's 96% pixel-exact. Floor object 99 is a door spot:
  stepping on it, the floor hook (bo1200 entry 4) turns it into the door 100 and sets the area's flag; `de` tells
  that hook whether the current area's flag is already set (kept in F450), and a registered area shows its door
  from the start. Inferred from the scripts and maps.
- **Stairs.** Floor parts 100 and 101 show 下りる and 上る. Inferred from the maps: they lead to the area with the
  opposite stairs, the one whose name gives the stairs' quarter of the map when an area has several (炎山洞
  中腹エリア's four stair rooms lead to 裾野エリア南東, 南西, 北西 and 北東), else the one area left (中腹エリア北 to
  山頂エリア); the party arrives on the target's stairs.
- **Per-area looks.** `floor.obj` column 10 is the fog colour (0x2b3021 in 千樹庭, 0x38221c in 炎山洞, fitted on the
  far view). Column 9 is 1 for 炎山洞 and the dungeons after it, whose floors show about 1.55 times brighter than
  the walls (inferred from one capture; VNDegen lights their floor parts that much more).
- **Counters and the floor hook.** Measured from autosaves: C061 to C063 count pad actions (moves and turns: all
  expeditions, this one, this area), C066 all steps and C067 this expedition's steps; the floor hook (12) runs
  after every step and wall bump (炎山洞's first-step explanation plays on the first step, as on the PC). Hook 10
  runs once per expedition (C600 counts expeditions: 5 at turn 10 in every save after five trips with many areas).
  On loading, the hooks `init.box` registers are registered again.

## Expeditions, from the scripts and play

`init.box` registers a script per hook number (`72`). What each hook does is read from its script; when the engine
calls it is inferred from that and from play, and marked so.

- **Variables the engine keeps.** C070 the dungeon and C071 the floor (`floor.obj` columns 0 and 1), C073, C074 and
  C075 the party's cell and facing (passed to `d7`/`d8`), C069 the expedition's steps plus one and C068 the steps in
  the current area (both kept after coming home), C076 and C077 set to 2 and 3 from the first expedition on (the
  original draws no walls and only the party's cell with them at 0; inferred as the view and minimap ranges that
  遠視 and 地図 widen), C150 the enemy group's id, C151 the dungeon again (for its
  music), C155 a battle's result (1 won, 2 lost; 3 fled is inferred). C210 to C213 hold what the expedition has found
  (residents, food, money, resources). Facing is inferred as 0 north, 1 east, 2 south, 3 west.
- **Hooks.** 10 `bo1000` sets the pickup ranges per dungeon (C520 to C527); 25 `bo2500` plays the dungeon's music by
  C151 (31 for 千樹庭); 11 `bo1100` sets up a floor (enemy groups by name with `a8`, floor objects with `d7`); 12
  `bo1200` handles the cell the party stands on or faces (pickups, keyed doors `壁` 101 to 110, opened as 111); 17
  `bo1700` runs on the way home; 26 `bo2600` before a battle (battle music 35); 27 `bo2700` after one (on a loss
  Darkus's line, then the loss message). Inferred order on entering: 10, 25, 11; hook 12 after each step onto a cell
  with a floor object and on walking into a wall; 17 on leaving.
- **`d7` and `d8`.** Write and read the map: `d7 "name" value x y mode [facing]`, `d8 "name" var x y facing`. Names:
  床物 (the cell's object, `mpDDFF.amd` field 6), 壁 (the wall on a side) and 移動 (the field after it; 1 lets the
  party through). Mode 2 also writes the neighbour's facing side (inferred from doors being opened that way).
- **Pickups.** Floor objects 101 to 104 give residents, money, resources or food (a random amount in the hook's
  ranges, 20 to 40 residents, 20 to 100 money, 10 to 30 food, 20 to 80 resources in 千樹庭) and become 105.
- **`8a`'s numbers** add to what the expedition has found, and when any is non-zero the box shows the finds before,
  the change and after in its table (measured on the loss message: 資金 1 → 0, 資源 1 → 0); the loss hook passes their negatives, so a loss leaves
  nothing. Measured: the loss message reads 戦闘敗北した事により、遠征で得た物を全て失ってしまった。, then the turn
  ends with the income box.
- **`89`** after an event's scenes fades back to the dungeon view (C070, C071, C072, fade 10).
- **Coming home.** 帰る (or 休憩, inferred) runs hook 17, adds the finds to the town and ends the turn (inferred).
- **Encounters.** Measured: battles came on steps 48, 56, 69 and 81 of one expedition (7 to 12 steps after the
  previous one), and none in the first 34 steps of another. Inferred: random encounters start once the expedition
  has walked 40 steps, then come on a countdown drawn from `floor.obj` column 13 ± column 12 (10 ± 2). The group comes from one of
  three lists, columns 30, 31 and 35 (on the first floor 千固定0100|千固定0101, 千05|千06 and 千00|千01), chosen by
  the weights in columns 32 to 34 (10, 60, 10); measured in support: the fixed seven-slime group 千固定0100 came up
  in the original's random battles.
- **Dungeon select.** Measured (99% of pixels exact): the hall `bg005a0` with the party on the `m998bt1a` platform,
  a plate per open dungeon (`gat_bNN`, normal, hover, chosen) from (340, 40), its badge at (354, 52) (`gat_d01` NEW
  until one of its maps is visited, then `gat_d02`), and 戻る (`gat_c00`) at (444, 640). Choosing a dungeon shows
  its entries' plates (`gat_aNN` for dungeon NN, with the badge and the area's name in white, 22 pixels, centred 16
  pixels down) from (765, the chosen dungeon's row).
- **Battles from the dungeon** use the dungeon's platforms (`mNNNbt1a` and `bt2a` to `bt2c`, NNN the dungeon less
  one) and the top bar names the dungeon and floor. The backdrop is the dungeon view; the original's seems to look
  further along, from a different camera.

## Missions, from the scripts and play

Measured under Wine from a turn-16 save, through to the deadline and a submission.

- **Start.** The turn script (bo0000) asks `9e` whether a mission is open this turn and `9f` to pick one, then
  plays its scene (fa0000 #1) and `a4` shows the 提出 panel with 確認 over the scene. `mission.obj`: column 0 the id,
  6 and 7 the turns it can start on, 11 the turns allowed (C057 = start + 11 - 1: 17 + 8 - 1 = 24), 12 to 14 the
  food, money and resources wanted. C055 is the mission, C056 its first turn, C057 its last. The original picked
  試験04 where 試験02 to 04 all start on turn 17; VNDegen picks among them at random (inferred).
- **Town.** While a mission runs, 提出 (`mai_b08`) shows at (16, 420), the top panel's 目標 gives C057, and the town
  window's 目標 rows give the amounts, blue when met and red when not.
- **提出.** Opens over `bg009a0`: `pay_a00` at (340, 130), the turns in `pay_c00` digits, the amounts in `pay_c01`
  (black, red, blue rows), and 提出 (`pay_b00`, only when every amount is met) and キャンセル (`pay_b01`) at (570, 350) and
  (780, 350). 提出 takes the amounts and runs the command hook (bo3100 #0 with C095 29), which adds the turns to
  spare to C581, ends the mission (`a1 1`) and plays 条件達成 (fa0000 #2); C580 counts missions passed. 99% of
  the screen's pixels match.
- **Deadline.** The town hook (bo0100, run after every turn script) calls `a0` once the turn reaches C057; past
  it, the mission passes if the town has the amounts, and they are taken (the original went from 205 food, 1,720
  money and 850 resources to 5, 720 and 350 at turn 25), else it fails; C058 tells the hook which (1, 2), and the
  hook ends the mission with `a1` and plays fa0000 #2 or #3. A failed test ends in マクドゥ's scene and `7a`, back
  to the title (inferred).
- **Save.** A list from 1,008,328 of (mission, 1, result) per mission started: (4, 1, 0) while it ran, (4, 1, 1)
  once passed.

## Town posts, from the screen

- **組織図.** Measured from turn 20 (F044 adds 一覧, 組織図 and 編成 to 配置's top bar, `uts_b00` to `b02` at (321, 26),
  79 apart): `uts_a01` with nine post tiles (`uts_w00`) at (299, 112) 街長, (412, 168) 街長補佐, (165, 187) 農業担当,
  (249, 229) 商業担当, (333, 271) 工業担当, (417, 313) 警備担当, (5, 214) 教育担当, (89, 255) 娯楽担当, (173, 298)
  衛生担当 (`Position.obj`'s rows 1 to 9), 解任 (`uts_x01`) at (372, 675), and a corner panel with the post, the
  holder and their 統治/統率. Clicking a member then a tile posts them (they leave the field); the list badge
  shows the post (`uts_h00` onwards). The town window gains a 街役職情報 tab listing the holders.
- **Save.** Post k holds the member's index at 1,008,208 + 4k (-1 when empty); the member's +25 byte is 3 and +26
  the post. The town's posts keep their `Position.obj` row; the 迷宮's are the row + 1 (迷宮長, row 10, is 11 in a
  turn 42 save the original wrote). The holder's second stat block (+668) carries the raised 統治 and 統率 at its
  ints 8 and 9 (ミアルテ as 街長 8/10 over 3/6), and a 天才 holder has 2 in its thirteen ints from 65 (VNDegen writes
  the ability's value there for abilities that raise "その効果", inferred). VNDegen round-trips all of it.
- **Ability bonus.** Measured on turn 42 by moving two members between posts: a holder whose ガヴァメント能力
  (`unit.obj` column 36) works at the post shows 統治 and 統率 raised, in blue (0x0068B7) in 配置's list and in the
  役職 windows, and with `uts_i02`'s first icon beside their tile and their window row. ミアルテ (政治家, 3/6)
  reads 8/10 as 街長, 迷宮長 or 警備担当 and 3/6 as 第一階層長 (統率 only); ルーアルテ (天才, 2/5) reads 4/7 at every
  post tried. Inferred: the raise is `ability.obj` column 4 to 統治 and that less a fifth to 統率 (5 → +5/+4, 2 →
  +2/+2), where an ability works follows its description (政治家 at posts whose `Position.obj` column 2 has 統治,
  大将軍 with 統率, 指揮官 at 迷宮長 and 補佐, 小隊長 at the floor heads, the 才 at their own post, and so on), 無能
  lowers by the same amount with the red icon, and 人気者, 資金管理, 食料管理 and 資源管理 show their own icons
  (people, money, food, resources) without a raise. 配置's list also shows the first icon beside some rows in a
  way not pinned down; VNDegen doesn't draw it there.
- **Effects.** Not measured beyond one turn: with ミアルテ as 街長 and ルーアルテ as 農業担当 the residents grew by
  5 where the same turn without them grew by 2 (residents' growth is random, so this proves nothing yet); food,
  money and resources were the same. VNDegen's posts change nothing in the town so far. `c3 "post"` answers
  whether a post is held and by whom (bo3105's scenes).

- **解任 (配置).** The tile at (445, 536) takes a field or reserve member off the board; inferred (dropping an
  unplaced member there did nothing on the PC).

## ダークス宮, from the screens

- **Opening.** Measured: turn 42's script (`bo0000.box`, F506 and F038 off) plays ソフィア's scene, shows
  迷宮配置が出来るようになりました。 and sets F038, F349 (迷宮視察 in 内政) and F506. F038 opens everything below.
- **Town.** Measured pixel for pixel against turn 42 (the same capture showed the whole town 1 px right of what
  VNDegen drew before, and 遠征, 内政 and 休憩 12 px right; all now match): the tab bar `mai_d00` at (54, 3) with
  施設1, 施設2, 街, 迷宮, 街役 and 迷役 (`mai_d01` to `d06`, 40 by 21, lit while the window shows) from (68, 7), 42
  apart; the ダークス宮 button `mai_e00` at (832, 4); 迷宮情報 (`flw_d01`) at (868, 70) with 迷宮サイズ, 守備要員,
  通路, 罠 and 宝箱 each with a `flw_d11` mark, 迷宮収支 and 迷宮評判 (`flw_d10`, 皆無 to 常識; 幻 on turn 42) and
  侵入者; 街役職情報 (`flw_e01`) at (400, 493) and 迷宮職情報 (`flw_f01` with the `flw_f02` title and a `flw_f0NN`
  label for each open post) at (750, 453), each row with the holder's icon, name and 統治/統率 (`flw_e08`'s blue row
  when raised, `--` when empty). A window's buttons, from the right: × hides it (its tab goes dark), − folds it to its
  180-wide title (`flw_b00`, `b50`, `c00`, `d00`, `e00`, `f00`) right-aligned on the same row, and the round arrow
  narrows a 役職 window to its 272-wide form (`flw_e03`, `f03`) or folds a 施設 or 街 window's sections to their
  headers (`flw_b11` to `b17`, `c08` to `c11`); a section's own header folds just that section. The marks, depth
  and the posts that open with it are below. Inferred: 街役 opens with F044.
- **The screen.** Measured pixel for pixel (both tabs, and with 最近の収支 hidden and the 役職 window narrowed): the
  command hook (`C095` 28) shows its help over the cross-section the first time (F108); the backdrop `dmap00`; each
  floor K at size S is `dK_Sf` (positions below), with its stairs `dK_Ss`; the deepest floor's shaft `dK_Sy` at (225, 121) and tunnel
  `dK_Sx` at (244, the floor's row + 29); the tab bar `dmz_e00` at (54, 4) with 迷宮役職 and 最近収支 (`dmz_e01`,
  `e02`) from (68, 8), 63 apart; 最近の収支 (`dmz_b00`) at (30, 374) with `dmz_d00` digits; the 迷宮職情報 window
  shared with the town; 戻る (`dmz_c02`) at (1117, 675). `dK_S` exists for each floor's sizes up to
  `maze.obj`'s 広さ最大 (3, 2, 4, 5, 3, 5, 5). `dmap01` to `dmap03` are full mazes drawn with another layout and
  don't match.
- **組織図.** Measured: the 迷宮 platform `uts_a03` at (9, 421) and the list badges `uts_h09` to `h19`; the tiles and
  rails are under Posts open with depth.
- **迷宮情報's numbers.** Measured by editing post holders, their stored 統治/統率, facilities and depth in a turn-42
  save and reading the window on the original (each value is the sum of all sources, floored once). Points: 守備 is
  each staffed facility's `TownBuil.obj` column 11 plus the 統治 of 警備担当, 教育担当 and 衛生担当 plus 1.5 × the
  統率 of each open floor head; 罠 is column 13 plus the 統治 of 工業担当 and 警備担当 plus the 統率 of 迷宮長 and
  迷宮長補佐; 宝箱 is column 12 plus 商業担当's 統治 plus 3 × the 統治 of 迷宮長 and 補佐. With F floors, 守備要員 is
  1.2·F·points, 罠 3·F·points, 宝箱 3.75·F·points and 通路 F × (10 × the 統率 of 迷宮長 and 補佐 + 15 × each floor
  head's 統率). Stats are the raised ones the post shows. 遊撃隊隊長 and 支援隊隊長 add nothing, and an unopened floor
  head counts for nothing. A facility counts only when staffed; its value at level L is the column plus every
  `TownLvup.obj` row of its growth group up to L, where `TownLvup` column c adds to `TownBuil` column c + 6.
- **Marks.** Measured by bisection: × below 300, △ from 300, ○ from 600, ◎ from 1200, ☆ from 2400, the same steps
  for all four numbers and at every depth.
- **評判.** Measured live on the original: the 迷宮評判 plate follows the staffed facilities' column 14 total (with
  levels) plus 11: 噂 from 50, 普通 from 100, 話題 from 150, then 有名, 超有名 and 常識 at about 270, 370 and 470
  (bisected to within 9). It never fell below 幻, even at −83; 皆無 wasn't seen.
- **Depth.** C240 holds the floor count (2 from the start) and C248 the turn the 迷宮 first ran (42). Measured over
  single turns: floor k (3 to 7) opens when the turns since C248 reach `maze.obj`'s 制限ターン (10 to 50) and the
  staffed facilities' column 10 total plus residents / 2, less about 42, reaches its 階数 (1000 to 3000); several
  floors can open in one turn. The 42 is inferred from brackets of 40 to 45.
- **Width.** C251 to C257 hold each floor's width points: every turn the floor head's 統率 is added (removing the
  head keeps the points), and C261 to C267 the sizes: 2 from 30 points, 3 from 50, 4 from 100 and 5 from 150
  (`maze.obj` 広さ), capped by 広さ最大.
- **Posts open with depth.** Measured: 支援隊隊長 at 4 floors, 迷宮長補佐 at 5, 遊撃隊隊長 at 6. The 組織図 tiles
  (`uts_v00`): 迷宮長 (298, 382), 補佐 (418, 442), floors 1 to 4 from (15, 459) in steps of (84, 42), floors 5 to 7
  from (12, 556), 遊撃 (320, 498), 支援 (442, 557); the rails `uts_y00` (358, 427) with 支援, `uts_y01` (357, 422) with
  補佐 and `uts_y02` (348, 427) with 遊撃.
- **Floors 3 to 7.** Measured pixel for pixel with all seven at every size: rows 162, 242, 312, 382, 462, 542 and
  622; each floor's x per size is in `maze.c`; the stairs are fixed per floor. `dmappos.obj` lists each floor's
  areas, one per size step: an event icon `d_evNNN` sits on the area's point, and the breakthrough icon `d_ev011` 60
  left of the deepest floor at its row.
- **The turn's record.** The save keeps the last turn at 1,009,652: the intruder count, then for each floor seven
  areas of six numbers (the `maze.obj` event id or -1 for a quiet area, the people it sent back, 0, then food, money
  and resources), and the net at 1,010,836 (also C271 to C273); C249 intruders, C278 sent back, C279 broke
  through. 最近の収支 is drawn from these: an event counts as won when its result row is type 10 (500 when it sent
  people back), a lost chest adds to 宝箱開封数, and the breakthrough column is what's left of the net.
  完全突破 (`dmz_f00`) at (362, 564) shows when anyone broke through. Measured against six original turns.
- **Intruders.** C249. Measured: 8 a turn whenever the 評判 value (above) is 0 or less, whatever the residents (1000
  to 9000) or depth; with a positive value, some turns bring 9 or 10, more often as the value rises (16 to 70: 2 turns
  in 12; 133 and 223: every turn seen), never above `maze.obj`'s 制限敵数 for the depth (10 at 2 floors). The
  original's random numbers come from the save, so identical saves give identical turns. Inferred: a chance of
  value / 2 + 5 percent (at most 90) of 1 to 1 + value / 50 extra.
- **The turn's events.** Inferred from 330 turns of the original over 15 set-ups (each number on its own at three
  strengths, then mixes), fitted to the counts within sampling noise. The intruders walk the floors in order and
  every area of a floor (one per size step) gets at most one event while any remain. Weights, in points before
  迷宮情報's multipliers: battle = the 守備 points from facilities and town posts + 0.9 × the floor head's 統率;
  trap = the town 罠 points + 15 with a 迷宮長 (and 15 with a 補佐); chest = the town 宝箱 points + 10 with a
  迷宮長 (and 10 with a 補佐); lost = 9 + 統率 / 5 with a 迷宮長 + 0.75 × the floor head's 統率. An area picks one
  kind in proportion, out of 100 when the weights add to less (the rest is quiet, recorded -1). Measured along
  the way: with only one kind, an area's chance is its points / 100, capped at 1 (宝箱 97, 300, 1200 gave 17%, 38%
  and 100%; 罠 and 守備 the same shape).
- **Each event's outcome.** Inferred weights, from `maze.obj`'s rows: a battle is won by 守備隊 (200, 0.4 × town
  points) or by the floor head (201, 0.3 × 統率, only when there is one), lost to 205 (15) or, without a floor head,
  206 (10); a trap fires (400, 0.6 × town points; 402 with a floor head, 401 with 支援隊) or is avoided (403, 10),
  so traps from the 迷宮長 alone never fire (measured: 0 of 30); a chest is held by 宝箱守備隊 (300, 0.3 × town
  points) or by the 迷宮長 (301, 3 + 統治 / 10), or robbed (303, 305, 307 at 1 each, and 304, 306, 308 at 3 each
  when there is no 迷宮長); lost is 500, which sends people back 105 times in 108 (迷路を突破, sending none,
  wasn't seen). Measured: a won event or a robbery sends back one party of 3 (or what's left), but a post holder's
  win and, with a 迷宮長, a robbery send back everyone left; yields are the result row's ranges (battle 3 to 5
  money, trap 1 resource, lost 1 food, robbery 20 to 100 of one stock) plus, for battles, ⌈town 守備 points / 20⌉
  money and, for traps, ⌈town 罠 points / 10⌉ resources.
- **EXP.** Measured: every open 迷宮 post's holder gains 2 EXP per intruder each turn (16 for 8), whatever
  happened; town posts gain none.
- **Post holders on the floors.** Measured from captures: each 迷宮 post's holder stands on a floor of the
  cross-section (a floor head on its own, the 迷宮長 on the deepest) playing its attack animation, a different spot
  each visit. Inferred: VNDegen draws the unit's `uNNNNe0` sheet at a quarter size, 100 ms a frame, its frame's
  foot 38 pixels below the floor's row (captures fit it to about 5 levels in 255, not exactly). The sheets' grids
  differ by unit (ルーアルテ 5 by 4, ミアルテ 7 by 4), so the grid is found from the empty lines between frames.
- **Breakthrough.** Measured over 20 turns: each adventurer who passes the deepest floor takes 0.25% of food, money
  and resources, rounded up on the total (8 of them took 1000 from 50000, at any depth). The turn's income box then
  adds ダークス宮の防衛に失敗しました; a turn where all were sent back adds nothing.

## Recruits and the turn order, from the scripts and play

- **Turn order.** Measured under Wine: after 休憩's income box the town hook (bo0100, hook 1) runs first, over the
  town map at half brightness and still on the old turn; then the turn advances (C060) and the turn script
  (bo0000) plays. Evidence: the hook's F1901 event (C060 >= 20) is clear in the original's turn-20 autosave and set
  in its turn-21 one. The mission deadline check in the hook therefore fires when the old turn reaches C057.
- **Recruits.** The hook calls ha0000 from turn 3 on. For each housing type it asks `f3` for the type's highest
  level (operand 4) and, at set levels, joins units with `bc` (住居 1: ベト and クマン; 3: モエルモン; 5: 天界兵Ａ;
  …; 豪邸住居 3: サキュー, who has 感知), then shows 部下が増えました。 with their cards. Measured: three 住居 built
  at turn 19 gave ベト and クマン at the next turn; VNDegen's box matches the original's to 95% of pixels. Cards show
  the sheet's first frame (クマン's sheet holds a group of eight).
- **Command hook.** Measured: 建設's 決定 runs the command hook (bo3100 #0 with C095 23), whose first run plays the
  carpenters' scene (bo3104).
- **Area bosses.** `floor.obj` columns 23, 25, 27 and 29 name up to four groups; the area's map marks one cell for
  each in field 7 (1 to 4). `party.obj` column 7 is the group's movement, 停止 or ランダム. Beating one sets C150 to
  its id, and the battle hook (bo2701) recruits its leader (ヤーヌ隊 9801: アンゴラメイ; イシュ隊 9804:
  サマキュー; …). Inferred from the tables, maps and scripts; the 抵抗勢力拠点 areas open after the turn-20 event
  and three won battles (F2001 to F2003, then F1902), which weren't played through under Wine.

## 内政, from the screens and the tables

- **Screen.** Measured (the windows 99 to 100% pixel-exact): the throne room `bg010a0`, the 内政 window `dom_a00` at
  (176, 84) with a button per category (`dom_a01` 視察, `a02` イベント, `a03` 交換; normal, hover, chosen) from
  (176, 123) 40 pixels apart and 戻る (`dom_a04`) at (366, 254); the category's window (`dom_b00`, `c00`, `d00`) at
  (531, 84) with the actions' pictures (`DomeEvnt.obj` column 3; normal, hover) from (531, 123); the description
  panel `dom_a05` at (531, 316), which shows the hovered action's two texts (columns 21 and 22) in 16-pixel text from
  (547, 347), 18 pixels a line, wrapped at 368 pixels; the town window and the turn at the right. Choosing an action
  halves the screen's brightness and shows 内政結果 (`dom_a06`) at (362, 185) with the action, its texts, 実行
  (`a07`) and キャンセル (`a08`) at (397, 435) and (577, 435).
- **Categories.** 視察 is always there; イベント once F041 is set (the turn script sets it when the residents reach
  300) and 交換 once F042 is (food, money and resources together reaching 5000). `DomeEvnt.obj` lists 視察 and
  イベント actions. 交換 is the engine's: measured, six fixed trades of 1000 (資金 → 資源 or 食料 300, 資源 → 資金
  or 食料 350, 食料 → 資金 or 資源 200) on `dom_dz00` to `dz05`, the 1000 red when short; the description panel
  sits at y 437 and reads 物々交換です。; 実行 trades, shows 交換しました。 with the table and ends the turn.
- **An action's row.** Column 2 its category, 4 a flag it needs, 6 a facility it needs, 8 the residents it needs,
  14 its event number, 15 residents gained, 16 to 18 food, money and resources (costs are negative), 20 the EXP every
  working facility gains, 21 and 22 its texts, 23 the expedition effect it gives.
- **実行.** The engine sets C492 to the event number and runs `bo3100.box` entry 2, which plays the event's scene
  (`ca0000`) and counts it in C601. Inferred: the resources change and the facilities gain the EXP first, the turn
  ends after the scene (the income box follows), and 実行 is disabled when a cost can't be paid. Facilities level up
  by `TownLvup.obj`: `TownBuil.obj` column 5 picks the growth row, whose third column is the EXP each level needs.
- **`f3 "name" …`** queries a facility type: operand 3 receives how many are built, operand 4 their highest level
  (the 街視察 scene stores each type's level in C650 onwards, for the after-scene report); with no name, operand 1
  receives the number of facilities built (inferred).

## 装備, from the screen

- **Screen.** Measured (98% of pixels exact): the library `bg009a0`; the members window `ite_c00` at (2, 20) with
  the sort tabs ポジション, 名前, ＬＶ, 格, 統治, 統率 and アイテム (`ite_d01` to `d07`) from (24, 27), 62 pixels
  apart, and a row per member (`ite_c01`, 26 pixels high, from (2, 76)): the position badge (`uts_h98` フィールド),
  the name, LV, 格, 統治 and 統率 in four cells, and the equipped item's name; the items window `ite_g00` at (625, 20)
  with the tabs 種類, 名前, 個数 (`ite_l01` to `l03`) from (648, 27) and a row per item (`ite_g01`): the item's icon
  (`itemNN.agf` scaled to 22 pixels, NN `item.obj` column 3), its name, and the free count over the total, the free
  count in red and the row grey when none is free; the scroll bars (`ite_c03` to `c05`) at x 586 and 900; 装備解除
  (`ite_j00`) at (934, 675) and 戻る (`ite_j01`) at (1117, 675).
- **Equipping.** Measured: choosing an item, then a member, gives the member that item (the row shows it and the
  item's free count drops); choosing another item for the same member swaps it. Each member holds one item. Darkus
  starts with ダークスの剣, 名剣ルージュ and 魔力の杖 (`init.box`'s `94`), none equipped, so the damage samples above
  were taken bare. Gems (宝石) aren't listed.
- **Effects.** Measured with ダークスの剣 (斬補正+20, 斬撃系スキルの威力を２０％ＵＰ): 剣撃 did 49 and 50 to a ナメッド
  (bare, 40 to 43). That fits (power + attack) × 120% − defence (49 to 52), and not the power alone × 120% (at most
  49). An item's `斬補正+N`-style effects (`item.obj` columns 15 to 22) apply this way to skills of that kind (斬, 打
  and 射 for physical skills, 魔 for the rest; the others inferred). (damage) × 120% rounded up also fits both
  samples; a target with more defence would tell them apart. Other effects (defence, speed, resistances) aren't
  applied yet. Hovering a member or an item shows a detail card that isn't built yet.

## 商会, from the screen

- **Screen.** Measured (93% of pixels exact; the rest is the figure's expression and the card's text): the library
  `bg009a0` with ソフィア (`ta01a000`) at the left; the list `shp_a00` at (241, 21) with the tabs `shp_b01` to `b06`
  and a 35-pixel row per item (`shp_a01`) from (241, 74): icon, name, 販売価格, 売却価格, 所持数, 取引数, and the
  buttons 購入 (`shp_g00`), 売却 (`g01`) and キャンセル (`g02`) at x 693, 765 and 837 (normal, hover, disabled);
  the item card `pit_a00` at (968, 86); the 所持金 box `flw_a02` at (1077, 31) under the goal box; the totals
  `shp_d00` at (968, 530) with 購入合計, 売却合計 and 計; 決定 (`shp_d01`) at (1048, 620) and 戻る (`shp_e00`) at
  (1120, 672).
- **Stock and prices.** Measured: the list holds, in `item.obj` order, the items on sale and the ones the party owns.
  An item is on sale when column 7 is 0 (inferred: or when the flag it names, 170 to 175, is set); otherwise its
  販売価格 shows dashes. 販売価格 is column 9, 売却価格 a quarter of it (150 → 37, 500 → 125, 1000 → 250). 所持金 is
  the town's 資金.
- **Trading.** Inferred: 購入 and 売却 add one to the row's 取引数 (a row trades one way), キャンセル clears it, and
  決定 applies all of them at once; equipped items can't be sold, and 購入 stops when the money would run out.

## 育成, from the screen

- **Screen.** Measured (94% of pixels exact; the rest is the card's text): the library `bg009a0`; the members list
  `grw_a00` at (10, 16) with the tabs ＬＶ, 名前, スキルＰ (`grw_b01` to `b03`) and a 30-pixel row per member
  (`grw_a01`: LV, name, and the skill points in blue) from (10, 45); the trainings `grw_d00` at (304, 16) with an
  83-pixel row each (`grw_d01`: normal, red when the points don't reach, yellow on hover) from (304, 45): the
  training's name, ステータスの…を N 上昇させます。, 消費気力 --- and 必要Ｐ; the member's status card `grw_f00` at
  (997, 21) and 能力説明 (`pun_b00`) at (997, 521); 戻る (`grw_h00`) at (1120, 670). Choosing a training halves the
  screen and shows スキル上昇 (`grw_j00`) at (290, 260), the training's row at (290, 308), and 決定 and キャンセル at
  (632, 447) and (792, 447) (99% of its pixels exact).
- **Trainings.** Measured: `StUp.obj`'s one row holds (points, gain) pairs for 体力, 気力, 物理攻撃力, 魔法攻撃力,
  物理防御力, 魔法防御力, 速度, 回避, 統治 and 統率: (10, 15), (10, 10), (10, 1) four times, (15, 1) twice and (50, 1)
  twice; the screen shows the first eight. 体力ＵＰ took Darkus from 170/170 to 185/185 and his points from 10 to 0.
  A unit starts with `unit.obj` column 37's points (Darkus 10). Inferred: `lvup.obj` column 3 (8 for Darkus's type)
  is the points each level gives; not applied yet.
- **Status card.** Measured layout: name and LV, race and 必要EXP (the EXP still needed), the role badge, 必要格,
  体力, 気力, スキルP, 速度, 回避, 統治 and 統率, 属性, the four attack and defence stats (numbers from the
  `grw_i00` digits), the skills with their MP, the count of skills over the unit's slots (`unit.obj` column 38),
  the field, reserve and government abilities (columns 34 to 36), the equipped item, the unit's sprite, and its
  description (column 76) below. Used by 育成; 装備 and 配置 show the same card on hover (not yet).

## 配置, from the screen

- **Screen.** Measured (97% of pixels exact with Darkus alone): the library `bg009a0`; the members window from
  `ums_a00` at (2, 21), `ums_a01` at (2, 338) and 解任 `ums_a02` at (438, 503), with 格 used over the limit
  (`uts_b03` at (230, 26)) and 編成 (`uts_b02`) at (403, 26); six メインメンバー tiles (`ums_z00`, 108 by 118) at
  x 112, 229 and 346 and y 86 (the front row) and 211 (the back), five リザーブメンバー tiles from (13, 366) 108
  apart, and the 解任 tile at (445, 536), each with the unit's sprite; the members list `uts_c00` at (567, 21) with
  the tabs ポジション to 統率 (`uts_d01` to `d06`) and a row per member; the status card; the party's diorama on the
  library platform `m997bt1a`; 戻る (`uts_g00`) at (1117, 675).
- **Placing.** Choose a member (tile or row), then a tile: two main tiles swap, a reserve tile benches the member
  (one must stay), and a reserve member moves to a main tile. Measured: 格 counts the reserve tiles as well as the
  main ones and may go over the limit (its digits then turn red, in 配置 and on the diorama), but 戻る (or a right
  click) then shows 「格が足りません / 配置を変更してください。」 over the screen dimmed to 3/8, and a click closes it.
  A post holder placed on a tile leaves the post; the members list keeps the recruit order (the roster), whatever
  the members' places; 格 is drawn with `flw_c15` digits, and the list has the `uts_c03` scrollbar at (967, 76) with its
  `c04`/`c05` ends (20 by 20) at y 56 and 686, as 装備 and 育成 have theirs.
- **一覧.** Measured (100% of the post rows): `uts_a00` at (2, 21) and one row per open post, `uts_z00` to `z19`
  (560 by 30: normal, hover, chosen, a fourth unused) at (2, 76 + 30 × (post − 1)), closed posts left empty; each
  with the holder's name (16 px at x 147), the ability icon (`uts_i02`) at x 337, and LV, 格, 統治 and 統率 in
  `uts_g02` digits (14 by 20, the raised values blue) right-aligned at x 403, 451, 499 and 547, `--` when vacant;
  解任 (`uts_x01`) at (372, 675). Rows take a member like the 組織図's tiles.
- **Post popup.** Measured: in 編成, hovering a post holder's row shows `pps_a00` at (10, 10) with the post's name,
  its `Position.obj` description and what it needs (統治, 統率) in widely spaced small text.
- **Status card.** Measured: the ステータス表示 / 能力詳細 button (`pun_e00`) at (1191, 2) switches the card between the
  status card and `pun_a02`, which shows the three abilities with their `ability.obj` texts and the equipped item
  (なし when none); 配置 and 育成 open in the status card. The card shows any member, posted or benched: the 役職
  badge is the post's (`uts_h00` onwards), フィールド (`h98`) or リザーブ (`h99`); the attribute (`unit.obj` column
  14) has its 20-pixel `pun_d00` icon at (242, 208); a post's raises follow 統治 and 統率 in blue 30 pixels right;
  skill icons are 20 pixels; a fixed-skill unit lists only its first skill, marked ★ without its 固定／ prefix, over
  column 38 + 1 slots (ミアルテ 1/3, Darkus 5/6). Not matched: the card's portrait frame (another sheet of the unit).

## Saves

- **DATA LOG screen.** Measured against the original's load screen: the background `sav_a00`/`sav_a01`, the autosave
  panel `sav_a06` at (35, 10) and the summary panel `sav_a07` at (46, 376); the page tabs `sav_b00` to `b09` (load)
  and `sav_d00` to `d09` (save) from (288, 376), 80 apart; 戻る `sav_a05` at (1094, 376); the slot rows `sav_c00` to
  `c09` and `sav_e00` to `e09` (600 by 50) at x 35 and 645 from y 437, 55 apart. Choosing a slot halves the screen and
  asks with YES `pop_b00` at (440, 382) and NO `pop_b01` at (640, 382).
- **Choosing a slot.** Measured: a click picks a row (lit) and shows its details: No.NN at (310, 50), the date, the
  time, プレイ時間 and the play time across the top, the count at the right ending at 1,196, then the 現在 lines, the
  weapon at x 860. DEL (`sav_a04`, 120 by 40) at (298, 318) and DATA LOAD or DATA SAVE (`sav_a02`, `sav_a03`, 200 by
  40) at (1037, 318). DATA LOAD and DEL ask ロードしますか？ or 削除しますか？ over the screen dimmed to 3/8; DATA SAVE asks
  セーブしますか？ in a box at (412, 294), 455 by 130, with two lines about the comment and a black field at (446, 378)
  with its caret, over the screen dimmed to 15/32, and OK and CANCEL (`pop_b02`, `pop_b03`) at (440, 425) and
  (640, 425). The save screen has no autosave panel or DATA LOG plate. The load screen opens on the last page used.
- **Panels.** Measured: the autosave panel shows NoA and the slot, the autosave's running count at the right, the date,
  the turn, the place (街, or the dungeon and floor names on two lines), the four town numbers and Darkus's level, in
  a bold face 14 pixels high, 20 apart from (55, 48). The panel beside it shows 現在, the game in progress (the turn
  and place, the town numbers, and Darkus's level and weapon), in a bold face 24 apart from (310, 50); at the title
  it shows NoData. Hovering an empty slot shows its number and NoData.
- **Autosaves.** Measured: `Bunny3A0.dat` to `A9.dat` hold a running count, and the count modulo ten picks the file
  (count 36 in `A6`, 41 in `A1`); the panel shows the highest count. The original autosaves on entering each floor
  (the place reads 千樹庭 / 中央森, then 中央森の東) and in the town (9ターン 街 after an expedition). Inferred: the town
  autosave comes at the start of each turn. Loading a floor's autosave puts the party back on that floor with its
  explored map; loading a town autosave puts the game in the town.
- **The original's files.** `Bunny30N.dat`, `Bunny3AN.dat` and `Bunny3.opt` unpack to 1,128,076 and 30,784 bytes
  with the codec below. `Bunny3.red` (text already read) is stored plain. `Bunny3.dsk` (32 bytes) hasn't been looked at.

## The save codec: what was read, and why

Black-box work fixed the files' header and showed an entropy-coded body over a second layer, but no decoder built
from guesses (canonical Huffman, arithmetic and range coders at several precisions and bit orders) reproduced a
save's known town numbers. With the maintainer's approval (2026-11-23), the reading is limited to:

- the routine that packs a save or option file and the one that unpacks it, with the small helpers they call for the
  coder (bit input and output, the model's tables);
- expected in `AoiCmp5.dll`, the DLL named for compression, or in `Bunny3.exe` where it calls the packer.

Nothing else is read: no game logic, no rule, no screen. What was read is recorded below as a description of the
format, and VNDegen's codec is written from that description, not from the code.

Read on 2026-11-23 in `AoiCmp5.dll`: its exported `AoiCompressPack` and `AoiCompressUnpack`, the three decoders and
three encoders they dispatch to, and the LZ window helper. All are self-contained byte transforms; none touches game
state. What they do:

- **Container.** Sixteen bytes: the unpacked size, the file size, the block size, and flags (bit 0: the data follows
  stored). Then blocks to the end of the file, each a 32-bit length, a method byte and that many bytes. Saves and
  options are one block.
- **Method byte.** Bit 7: a range coder over the block. Bits 0 to 3: 1 a run-length layer, 2 an LZ layer, applied
  under the range coder. Bits 4 to 6 name five other filters the packer can try; none is used by these files, and
  VNDegen doesn't build them. Saves are 0x82, options 0x81.
- **Choosing.** The packer tries each low layer (none, runs, LZ) with and without the range coder and keeps the
  smallest, taking a later one only when strictly smaller. An encoder gives up when its output would outgrow its
  input.
- **Range coder.** Byte counts over the block; when the largest passes 255 every count becomes
  ceil(count x 255 / largest). The 256 counts are stored first, symbol 255 first. A 257th end symbol has count 1.
  Low starts at 0 and range at 2^32 - 1. For each symbol, r = range / total, low += start x r, range = count x r;
  then while low and low + range agree in their top byte, the top byte of low goes out and both shift left by 8;
  then while range is under 2^16, the top byte goes out, range becomes (2^16 - (low mod 2^16)) x 256 and low shifts
  left by 8. After the end symbol the four bytes of low go out. Inputs under 512 bytes aren't coded. The decoder mirrors
  this, reading four bytes first and finding each symbol by binary search over the running totals.
- **LZ.** A byte n from 1 to 255 is followed by n literal bytes; a zero byte is followed by a 16-bit distance and an
  8-bit length, copied byte by byte from the output so far. The packer looks back over the last 4,096 positions that
  start with the same byte, newest first, for the longest match up to 255 bytes (a later candidate must be strictly
  longer; a match as long as the limit ends the search), and uses it when it is 5 bytes or more. Literals collect into
  runs of up to 255. Inputs under 6 bytes aren't coded.
- **Runs.** Pairs of a type and a count: type 0 is followed by count literal bytes; types 1 to 4 by one value of that
  many bytes, repeated count times. The packer tries widths 1 to 4 in turn when at least 4 bytes remain, needing 4, 3,
  2 and 2 repeats; a run never includes the last byte of what remains. Otherwise the byte joins a literal run of up to
  255. Inputs under 4 bytes aren't coded.

VNDegen's codec (`aoicmp.c`) was written from this description. It repacks all fourteen saves and option files written
under Wine byte for byte (`satool codec`).

## The original's save image, from diffs

Saves unpack to 1,128,076 bytes. Found by comparing the fourteen saves written under Wine with the runtime's state at
the same points (offsets in bytes, numbers little-endian):

- **Script variables** at 1,119,076: C000 to C999 as 32-bit numbers, then F000 to F4999 as bytes, to the end. C060 is
  the turn as shown (1 on the first turn); the scripts only compare it. C200 to C203 are residents, food, money and
  resources; C070 and C071 the dungeon and floor; C073 to C075 the position. At turn 2 the flags match VNDegen's except
  F000, F001, F021 and F924, which the original sets itself. The original also keeps numbers VNDegen doesn't yet:
  C000 (2^31 - 1), C002 to C013, C190 to C194 (-1), C209, C220 to C262, C393 and C400 to C409.
- **Town grid** at 1,010,848: 90 cells of five 32-bit numbers: the cell's state (0 when `TownMass.obj` opens it at
  the town's stage or lower, else -1), the facility type (100 the 魔王亭, 108 a field; 0 empty), a flag (1 the turn a
  facility was built, -1 when empty), the level, and a counter that grows each turn (4, 9 and 44 at turns 1, 2 and 9).
  The number after the grid is 1. From 1,012,652 come 56-byte records naming script files in UTF-16 (`ac0000.box`,
  then the hook boxes the game has run, `bo0000` to `bo3100`) with two numbers each; a script cache rather than story
  progress. The original loads images without it, so VNDegen leaves it empty.
- **Hook slots** at 1,035,060: 50 slots of 48 bytes, each the UTF-16 name of the script registered there (`bo0000.box`
  for the turn script in slot 0, `bo0100.box` the town hook in 1, …, `bo3100.box` in 31) or `EVnn.box` when empty.
  Measured: a save without them loads, but the original then runs no turn script or hooks (no story events, and
  facilities gain only the engine's +1 EXP a turn); VNDegen's export writes them.
- **Equipment** at 1,004,728: (item, free, in use) triples, as the 装備 screen shows them (ダークスの剣 0/1 while
  Darkus wields it). Equipping 名剣ルージュ instead moved the counts, changed Darkus's item at 87,220 from 100 to 104,
  and his bonus at 86,716 (the second stat block's eleventh number) from 20 to 7, the items' 斬補正.
- **Party** from 86,008: Darkus's unit (1), then his level at 86,020, EXP at 86,024, HP and MP at 86,036, skill
  points at 86,088 and the gains from training from 86,092 (体力ＵＰ wrote 15 there, and 185 into HP and both stat
  blocks). Two 544-byte stat blocks at 86,132 and 86,676: `unit.obj` columns 23 to 32 (HP, MP and the eight stats)
  plus training, the second also holding the equipment bonus. The skill slots (column 38) at 86,588 and 87,132; the
  equipped item at 87,220; the skills as (skill, 1) pairs from 87,228 (剣撃, 乱舞斬, 秘剣・鶯, 小回復, 火矢三連).
- **What the original needs.** It loads an image holding only the party region, the scene state at 993,200 to
  998,900, the town region at 1,004,728 to 1,013,300 and the variables, with everything else zero: no backlog, no
  map memory. Its LOG screen then opens empty.
- **Save details** in the variables: C002 and C013 the save time in Unix seconds, C003 the save count (shown at the
  top right of a slot's details, and the autosave panel's count), C011 the play time in seconds. Loading and saving
  again changes only these.
- **Map memory** from 6,188: up to 150 records of 532 bytes, one per area visited, in turn: a word naming the area
  (the dungeon in its third byte, the floor in its second), four layers of 32 words, and four numbers (a 1 or 3 on
  some areas; not understood, kept as read). Measured on a 炎山洞 autosave: 中央森, 中央森の東, then 炎山洞
  中腹エリア, with no slot for the areas between. Word y holds row y with bit x for column x. Layer 0 is the visited
  cells and layer 3 a copy of it; layer 1 marks a step walked between a cell and the one east of it, layer 2 between
  a cell and the one south of it (cells next to each other but never walked between stay clear). VNDegen writes the
  areas in dungeon and floor order. Fitted on every save from Wine.
- **Last position** at 998,016 (the formation list's cells 4 to 7): the area (dungeon << 16 | floor << 8) while in
  a dungeon and 0 in the town, then the cell and facing, kept after coming home (-1, -1, 0 before any expedition).
  The 36 bytes at 85,972 end with the area and cell of the last door the party came in by ((1, 1) at (6, 2) for
  every 千樹庭 save); zero before any expedition. The 52 bytes at 1,009,560 are thirteen -1s and a 0 in every save.
- **Enemy groups** of the current area at 998,288: up to two records of 280 bytes (70 numbers): the group's
  `party.obj` id, its number (1, 2), the area word, its cell and facing, its 15 board cells (row by row, three rows
  of five, `party.obj`'s row 0 first) holding the record numbers of its units or -1, ten -1s, zeros, five target
  cells as (x, y) pairs padded with -1, a flag word (1 for the first group, 0x101 for the second; 2 added after a
  battle), -1, the 40-step counter, the times fought (1 after fleeing), two zeros and -1. Its units are member
  records numbered from 100 (215,608 = 86,008 + 100 × 1,296): the unit at +0, -1 at +4, the record number at +8,
  the level, 0, the group number at +20, the group, 1 and the board cell as bytes at +24, HP and MP, then the battle
  blocks as for members. Measured on every Wine autosave: VNDegen's import and export of a floor autosave come out
  equal in these regions, except the order of the records (the original numbers them as it rolls them) and the
  ability numbers below.
- **Battle blocks**, from the enemies and members: `unit.obj` columns 23 to 32 grown to the unit's level (`lvup.obj`
  by the unit's growth type, column 19: HP + (level - 1) × column 4, MP + (level - 1) × column 5, 攻撃 on both attack
  stats and 防御 on both defences by the type's `+1/N` steps; ベトＡ at level 5 is 42/12 and 8/8 from 30/8 and 7/7);
  equipment bonuses 斬, 打, 射, 魔 at 10 to 13 and the element's attack 30 at 14 on (光 at 14, 闇 at 15, as the sisters
  and ダークマン); resistances from 33 (斬 33, 打 34, 射 35 measured; 魔 36, 光 37, 闇 38, 火 39, 水 40, 風 41, 土 42 and
  雷 43 inferred from the sisters' 光 and ルーアルテ's 雷), the unit's own element adding 20 and its opposite -50;
  必殺耐性 at 44 and 異常耐性 at 46; the abilities' ids at 47 to 49; the skill slots at 114. The second block adds the
  position bonuses of the field abilities (`ability.obj` column 4 as the amount): 攻撃強化 on both attacks, 防御強化
  on both defences, 速度強化 and 回避強化, to all (全), the front row (前, the party's front row and an enemy board's
  row 0), the back row (後), the unit's column (縦) or row (横), each counted once per unit that has it (two
  ダークマンＡ give the back row 攻撃 +10). Measured on fourteen enemy records across three groups. 生命力's 20 sits at
  106 of the second block; 凶暴者 and others use slots after it that aren't mapped. Enemies list all their skills,
  with 3 after a 固定 one and 1 after the rest, except that 一角ベトＡ lists two of its three (not explained).
- **Other regions** not yet placed: 993,208 to 997,999 (four 1,200-byte records starting -1, 1, -1), 1,035,056 to
  1,066,684 (the backlog's text) and an ascending list of offsets at 1,115,868 (the backlog's index).
- **Engine numbers.** C000 is always 2^31 - 1, C007 and C008 100, C036 and C037 1, C190 to C194 -1. C004 and C005
  are 6 and 1 in the town and 41 plus the area's backdrop number (62 in 千樹庭's bg021, 61 in 炎山洞's bg020) and 2 in
  a dungeon; the slot details read the place from them and an image without
  them opens a story scene instead of the town. C006 changes each turn, likely the random seed. The original
  recomputes C209, C220 to C262 and C393 to C409 on loading (the town window shows the right numbers without them).
  C003 counts manual saves and autosaves separately across the game.
- **Writing.** VNDegen writes the image from its state: the scene-state pattern, the -1 run at 1,008,208, equipment,
  the grid, Darkus's records (stat blocks from `unit.obj` columns 23 to 32 plus training, 必殺耐性 and 異常耐性 from
  column 33, the skills from columns 41 on), the variables and the engine numbers. At turn 2 the party and scene
  regions come out byte for byte as the original's; the original loads the file into the same town. Inferred: the
  three 2s after the resistances, and which bonus slots follow 斬補正 (打, 射, 魔).
- **Members.** Measured from a save where ミアルテ (unit 157) and ルーアルテ (158) had just joined: one record of
  1,296 bytes per member from 86,008, Darkus first. In each: the unit at +0, its place in the list at +4 and +8, the
  level at +12, EXP at +16, four bytes at +24 (00 01 02 00 for Darkus, 00 04 01 00 and 00 04 02 00 for the sisters),
  HP and MP at +28, skill points at +80, the trained gains from +84, two 544-byte battle blocks at +124 and +668, the
  equipped item at +1212 (-1 after it once one is set), the skills from +1220 and a number at +1284. In a battle
  block: `unit.obj` columns 23 to 32 at +0; 必殺耐性 and 異常耐性 (column 33) at +176 and +184; the field, reserve and
  government abilities (columns 34 to 36, as `ability.obj` ids) at +188 to +196; the skill slots (column 38) at
  +456; the equipment's 斬補正 in the second block at +40. Not yet explained: ミアルテ's 30 at +56, 60 and -50 at
  +148 (likely her 光 and 闇 affinities), a 5 at +252 of the second block, and her skills written as (531, 3). The
  original doesn't rebuild these blocks on loading: with them zeroed, her card shows 300/0 and no abilities.
- **Joining.** The turn script's event (`bo0002`, run while F2321 and F211 are on, F2322 off and C060 >= C704)
  joins ミアルテ and ルーアルテ and shows ミアルテ、ルーアルテが仲間になった。 in the message box with both units'
  pictures under the text.
- **Formation.** Measured by placing ミアルテ in 配置: a list of 32-bit numbers at 998,000 where cells 9 to 11 are the
  main members' top row, 14 to 16 the bottom row and 23 to 27 the five reserve tiles; -1 is an empty tile, -2 a
  cell that isn't a tile, and a member's index (its record's place) a placed member. Cells 4 to 7 are the last
  position (above), not a member count as first thought. Placing her set her record's +25
  byte from 4 to 1, and added her 魔法耐性's 50 to a row of resistances in her second battle block only. 配置 works by
  clicking a member, whose sprite then follows the pointer, and clicking a tile.
- **装備 and 育成** list every member, posted and benched included, in the roster's order (measured: the original's
  装備 shows ミアルテ and ルーアルテ while they hold posts).
- **Member places in saves.** Measured: the +24 word's two upper bytes are (tag, place): place 1 on a main tile with
  tag 1 (Darkus 2), place 2 on a reserve tile with tag 0, 3 in a post with the post's id, and 4 unplaced with the
  member's index.
- **Joined members.** Measured: a new member joins unplaced (not on a tile). Units whose skills are 固定 keep only
  their first skill, with 3 after it (the mark of a fixed skill, as on enemies). Not derived yet, so VNDegen writes
  zeros there: ルーアルテ's thirteen 2s at +260 of the second block, a 5 at +252, and Darkus's 3 at +1284 (turn 9
  on). The original loads such a file with the right members, places, stats, abilities and skills.
- **Reserve.** The five lower tiles of 配置 are the reserve (リザーブメンバー); a member there is written in cells 23
  to 27 with its +25 byte at 1 (inferred, as for a main tile).
- **Joining box.** Measured: `8a 2 0 0 0` in the town adds its numbers to the town (住民 15 → 17) and shows the
  table; the units that joined just before get a card each under the text (`ret_a05` on the `ret_a06` panel, the
  front sprite `uNNNNa2` and the name), newest first, then OK.
- **Dungeon saves.** Measured: an image with C004/C005 at 62/2 (in a dungeon) but nothing in the dungeon regions
  crashes the original as soon as its load screen lists it. With the groups, their enemies, the last position, the
  entry and C076/C077 written, the original lists VNDegen's dungeon saves (8ターン 千樹庭 中央森の東 0歩目) and loads
  them onto the saved cell, facing, map memory and groups (checked on a save from a VNDegen game four steps in, and
  on VNDegen's re-export of a PC autosave). The slot's N歩目 read 0 on every save so far; VNDegen shows C072.
