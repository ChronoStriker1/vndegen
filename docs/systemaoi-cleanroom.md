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
- **`ef 0 <n> <mode>`.** Inferred from the scripts: a percentage that falls with the message "the town's facilities
  were damaged" (-60 to -40) and rises by 3 to 5 each turn; VNDegen keeps it, from 100, but nothing shows it yet.

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
- **`c3 "role" var var`** asks which unit holds a town role (街長, 農業担当 and the others 配置 assigns); not built
  yet, so it answers none.
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
  and 21 showed 9 and 17. So level 2 takes 120. Inferred: each later level takes that number times the level, and
  levels don't raise HP or MP until that is measured.
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
- **Map layout.** `mpDDFF.amd` stores its 15 by 15 cells column by column, mirrored: cell (x, y), with y growing
  south, is record (14 − x) × 15 + y. The wall fields are 18 north, 9 east, 12 south and 15 west (0 a wall, 10 a doorway
  the party walks through, 20 the exit), and the open-side mask's bits 1, 2, 4 and 8 are east, south, west and north. Measured: the start
  (0, 0) has the exit to the north, and walking south gives a wall two cells on with an opening to the east, as the
  original shows.
- **Camera.** `Cam.pvc`'s position and target (3.2 high, 4.6 behind the cell's centre, tilted down slightly), with
  a 47.2° vertical field of view: the floor lines and wall edges fit that best (correlation 0.58 against 0.30 to 0.35
  at other angles); the file's Zoom(40) isn't used as given.
- **Light and fog.** Inferred from colour fits: textures times 1.6 × (ambient 0.75 plus the two lights' diffuse), and
  linear fog to (40, 45, 30) from −4 to 28 units. Back faces are culled. A texture whose top-left colour covers a
  tenth of it treats that colour as transparent (the grass `010w0x0`).
- **Objects.** `mallset.pvm` holds the objects every dungeon shares (`action_box`): `a100` the exit's gold-framed
  door, `a101` to `a108` the floor markers of pickups (101 to 104 glowing, 105 spent), items, event marks and enemy
  markers. Measured: the door stands at the centre of the exit cell (the cell with wall 20) and turns slowly, edge-on
  to face-on in about 4.5 seconds (a turn every 18 seconds, filmed at 10 fps). A floor object N draws `aN` at its
  cell's centre (inferred for all but the door).
- **Action buttons.** Measured: standing on the exit cell facing the door shows 帰る (`dgn_g02`) at (1101, 498),
  above the pad; walking into the door does nothing. Inferred: floor part 100 (`f100`, stairs down) shows 下りる
  (`g01`) and 101 (`f101`, up) shows 上る (`g00`), going to the next or previous floor, arriving at its entrance.
- **Floor objects seen on the first floor.** 100 is a door model standing in a dead end, 60 a yellow crystal (the
  original shows 開ける at one); neither is handled by the floor hook, so both are the engine's. Open: what they do.
  `floor.obj` columns 36 onwards hold eight (table, minimum, maximum, count) entries such as 武器Ａ 1 1 1 and 宝箱Ａ 1 3
  1, whose names are `item.obj` column 30 tags. Inferred: standing on a crystal shows 開ける (`dgn_g07`); opening the
  floor's k-th crystal (in map order) gives between the minimum and maximum of items from the k-th entry, each drawn
  by `item.obj` column 31's weight, says 『name』を手に入れた。 and removes the crystal.
- **Minimap.** Measured: cells visited on earlier expeditions stay shaded on the next one.
- **Start cell.** Measured: twice one cell in from the exit cell, facing away from the door (the start view
  correlates 0.53 with VNDegen's, and 0.63 after turning to the door); once a cell further. VNDegen starts one cell in.
- **`bf var, N`** feeds the turn script's thresholds (1, 2, 5, 8): inferred as the number of floors reached in
  dungeon N + 1.

## Expeditions, from the scripts and play

`init.box` registers a script per hook number (`72`). What each hook does is read from its script; when the engine
calls it is inferred from that and from play, and marked so.

- **Variables the engine keeps.** C070 the dungeon and C071 the floor (`floor.obj` columns 0 and 1), C073, C074 and
  C075 the party's cell and facing (passed to `d7`/`d8`), C150 the enemy group's id, C151 the dungeon again (for its
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
- **Encounters.** Measured: the number under the compass counts the expedition's steps, and battles came on steps
  48, 56, 69 and 81 of one expedition (7 to 12 steps after the previous one; the count also rose by one over each
  battle). Inferred: a countdown drawn from `floor.obj` column 13 ± column 12 (10 ± 2). The group comes from one of
  three lists, columns 30, 31 and 35 (on the first floor 千固定0100|千固定0101, 千05|千06 and 千00|千01), chosen by
  the weights in columns 32 to 34 (10, 60, 10); measured in support: the fixed seven-slime group 千固定0100 came up
  in the original's random battles.
- **Dungeon select.** Measured (99% of pixels exact): the hall `bg005a0` with the party on the `m998bt1a` platform,
  a plate per dungeon (`gat_bNN`, normal, hover, chosen) from (340, 40), its badge at (354, 52) (`gat_d01` NEW until
  F731 onwards is set by the setup hook, then `gat_d02`), and 戻る (`gat_c00`) at (444, 640). Choosing a dungeon
  shows its floors' plates (`gat_a01`, with `gat_d02` and the floor's name in white, 22 pixels, centred 16 pixels
  down) from (765, 40). Inferred: one row per floor reached; only the first is offered so far.
- **Battles from the dungeon** use the dungeon's platforms (`mNNNbt1a` and `bt2a` to `bt2c`, NNN the dungeon less
  one) and the top bar names the dungeon and floor. The backdrop is the dungeon view; the original's seems to look
  further along, from a different camera.

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
  イベント actions; 交換 has none there and isn't built.
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
