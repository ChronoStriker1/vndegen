# SystemAoi

VNDegen is getting a runtime of its own for Soft House Chara's SystemAoi engine, version 5 (`Aoi5.dll`, `Ags5.dll`),
`Packages/VNDegenKit/Sources/CSystemAoi`, written in C from the game's files, GARbro's MIT-licensed archive, script and
picture readers, the game's own manuals, and the original engine run under Wine. It is MIT licensed like the rest of
the app. `docs/systemaoi-cleanroom.md` lists where each part came from, rule by rule for the game's systems. Work in
progress on branch `feat/systemaoi`; nothing ships yet.

## Test game

| Game | Engine | Notes |
|---|---|---|
| BUNNY BLACK 3 (DVD, `Bunny3.exe` 2013-06) | SystemAoi 5, 1280x720 | Japanese only. A dungeon RPG with town building: story scripts, and battles, the 3D dungeon, unit building and the town in the exe |

## How the game is built

The game is the `Bunny3/` folder of the DVD image. The installer only copies it and writes the registry value
`HKLM\Software\SoftHouseChara\Bunny3\path`; saves go to `Bunny3.sav\` beside the game.

- Archives: `VF` version 0x200 (`.vfs`), a 16-byte header (count, entry size 23, index size, file size), 23-byte
  entries (name offset, six bytes VNDegen doesn't use, offset, size, unpacked size, a packed flag), then the UTF-16
  name table. Nothing is packed. Big archives are split into parts (`evA.vfs` to `evD.vfs`) with a `VL` index (`.vfl`)
  over all parts, whose last entry byte is the part letter; each part also has its own full index, so VNDegen mounts
  the parts and ignores the `VL` files. `sam.vfs` repeats the event pictures' names as small samples, so names resolve
  to the first archive in name order.
- Scripts: `box.vfs` holds 33 `AOIMY01` archives (a UTF-16 magic, a big-endian count, 40-byte entries with a UTF-16
  name and big-endian offset and size), each entry XORed with a key computed from its byte offset in the archive.
- Bytecode: one-byte opcodes, each followed by tagged operands: `0x82` a big-endian int32, `0x84` a byte, `0x88` a
  length byte and that many UTF-16 characters, `0x92` an absolute address in the script, and a variable as a bank byte
  and a big-endian 16-bit index. Banks: `02` C (integers), `04` F (flags), `22` CL and `24` FL (the running script's
  own integers and flags), `44` FG (system flags). Conditions are text after `1a` and `1c`, such as `F133 on`,
  `C055 == 0` or `CL000 >= 1`, naming the same banks; `1a` jumps when its condition is false.
- Scripts are the story and hooks. `bo0000.box` script 0 runs once a turn and calls the turn's events; the town, the
  dungeon and battles are the exe's, and the scripts call into them by name (a facility, a unit joining, an enemy
  party from `party.obj`).
- Tables: `obj.vfs`, 24 tables with every byte inverted: integer records, a UTF-16 string pool and a column signature.
- Pictures: AGF version 1 and 2, run-length and back-reference operations over BGRA pixels.
- Sound: BGM Ogg Vorbis, voices `AoiOgg`-wrapped Ogg named by the script (`!PC0001` is `PC0001.aog`), effects WAV.
- Movies: `data1.dat` and `data2.dat`, ASF with WMV1 video (1280x720, 30 fps) and WMA version 2 sound.
- 3D: `model.vfs` holds Pegasus3D text models (`.pvm`: a tree of layers with a material, a texture, a blend, and
  triangles over position, normal and uv), a text camera (`Cam.pvc`) and text lights (`Light.pvl`), with BMP
  textures. Each dungeon has a part set (`m000set.pvm` to `m100set.pvm`: floors, poles, roofs, walls, doors, stairs).
  `map.vfs` holds 117 floor maps (`mpDDFF.amd`), 15 by 15 cells of 104 bytes after a 48-byte header, each cell with an
  open-side mask (1 north, 2 east, 4 south, 8 west) and an event number.

## What the runtime does so far

- **Title.** The game's title with its six items. A first start plays the prologue, as the original does before
  `Bunny3.sav` exists. Start runs `init.box` and then the turn script in `bo0000.box`, then opens the town. Load,
  Option and Extra aren't built yet.
- **Script machine.** Text, speakers, waits, choices, integer and flag variables in five banks, text conditions with
  indexed variables (`CL000[CL019]`), jumps and calls within a script and into other scripts, random numbers, and the
  loop the quiz uses.
- **Story drawing.** Backgrounds and event pictures, standing pictures by slot, fades between them, the message window
  with its Auto, Skip, LOG, Repeat, Option and close buttons, the click-wait feather, and choices in the game's own
  box. Speakers with a face set in `evntFace.obj` show the face whose expression matches their last standing picture.
  Placement was measured against the original under Wine; text lines up within two pixels.
- **Backlog.** Each page's speaker and lines, with a button that plays a voiced line again.
- **Input.** A click or Enter goes on; the wheel opens and scrolls the backlog; a right click fast-forwards, the
  original's default.
- **Sound.** BGM by its `bgm.asd` entry with the loop points the file gives, and voices as their lines show.
- **Movies.** `data1.dat` and `data2.dat` play through the app's movie decoder, `vnffmovie` (see
  [video.md](video.md)); a click ends one.
- **Town.** The map with its facilities, the two facility windows, the town window, the commands the story has opened,
  LOG, SAVE, LOAD and Option, and the party's diorama at the bottom left with each unit's idle animation and bars.
  The tab bar shows and hides the windows; each window folds, closes and (the 役職 windows) narrows, and a section's
  header folds it. From turn 42 the ダークス宮 button opens the cross-section of the 迷宮 with 最近の収支 and
  迷宮職情報, and the town gains 迷宮情報 and 迷宮職情報; 街役職情報 lists the town's post holders.
- **ダークス宮.** Each turn adventurers walk the 迷宮 floor by floor: battles, traps, chests and getting lost send
  parties back, and those who reach the bottom take a share of food, money and resources (the turn's box then
  says the defence failed). 迷宮情報's numbers and marks come from the post holders, the staffed facilities and
  the depth; 評判 from the facilities sets how many come. The 迷宮 deepens to seven floors as the town grows and
  time passes, each floor widens with its 階層長, and 補佐, 支援 and 遊撃 open with depth. 迷宮 post holders gain
  EXP every turn and stand on their floors in the cross-section, which also shows each area's event and the
  breakthrough.
  Each command first runs the game's hook for it (`bo3000.box` script 0 with `C095` set to the command), so its
  first-use help shows as in the original. 建設 opens the build screen: the list, the board, the card, Reset, Undo, 決定
  and 戻る, with the town window showing the result before 決定. 内政 shows the throne room with the 視察 and イベント actions
  from `DomeEvnt.obj`; 実行 plays the action's scene and ends the turn. 休憩 ends the turn: the income box (residents,
  food, money and resources before and after) over the bare map, then the next turn's script.
- **Message box.** Script command `8a` opens the game's message box (`ret_a02`, `a04`, `a00` and the OK button
  `ret_a01`) with its text, `|` breaking lines, and the script waits for OK. 装備 gives members the party's items, 商会 buys and sells
  them, 育成 spends skill points on stats, and 配置 arranges
  the party and its reserve.
- **Battles.** The board with the party's and the enemies' platforms, idle animations and bars, the Backs and Forward
  panels, the side buttons, 戦闘開始, 自動戦闘, ゲージON and the log. Each unit's action and target buttons open the
  game's skill and target pickers; the side buttons set every unit's order or target and save and load three sets.
  Rounds play in speed order with the damage, hit and target rules in `docs/systemaoi-cleanroom.md`. A win dims the
  screen and shows the resources, EXP and any item found; EXP fills the panel's bar and raises levels. Only a debug
  hook starts a battle so far.
- **Expeditions.** 遠征 opens the dungeon select; an area's map is drawn in 3D from its part set (software rendered) in
  the original's own coordinates, walked with the pad, the arrow keys or the walk arrows that appear under the pointer
  on the view (a click or tap walks, turns or sidesteps), with the compass, the step count, a minimap of visited cells
  with the door, crystals and enemy groups, the 目標/時間 panel and the party's diorama. The party starts beside the door
  home; 帰る on the door's cell brings the finds home. Stepping onto an edge passage moves to the neighbouring area.
  Each area's enemy groups walk the routes in its map file, or wander, as red crosses on the minimap, moving with the
  party's actions as in the original, and fight when met; random encounters begin after 40 steps. The game's hooks run
  on entering, on stepping onto floor objects and walls, before and after battles and on the way home; a loss ends the
  expedition.
- **Saves.** SAVE and LOAD in the town and LOAD on the title open the DATA LOG screen: ten pages of ten slots, the
  newest autosave in its panel (load only), and beside it the chosen or hovered slot's details (date, play time,
  count, turn and place, town, Darkus) or the current game (現在). A chosen slot offers DEL and DATA LOAD or DATA SAVE;
  loading and deleting ask YES or NO, saving shows the original's comment box with OK and CANCEL. Saves are the
  original's own files, `Bunny3.sav/Bunny3NN.dat` and `Bunny3AN.dat` in the save folder, packed with its codec
  (`aoicmp.c`, `pcsave.c`): the variables, the town, every member with their place (main or reserve tile), stats,
  resistances and position bonuses, the equipment, the map memory (visited cells and the steps walked) and, in a
  dungeon, the cell, the area's enemy groups and their units. The original loads VNDegen's saves, town or dungeon,
  and VNDegen lists and loads the original's. Beside each, `vndegen/slotNN.sav` keeps what the image doesn't carry
  (battle orders); it is used only while its time matches the original file's. As in the original, the game
  autosaves at the start of each turn and on entering each area, through ten slots in turn; a dungeon save loads
  back onto its cell, any other save into the town. New members join off the board and are shown on cards in the
  message box, as in the original. 配置's lower tiles are the reserve.
- **App.** `EngineSystemAoi` finds a game by `box.vfs` holding `init.box` and `pro.box`, and runs the runtime in a
  FrameHost view at 1280x720. Ctrl or the controller's dash button holds skip. Games start on the Visual novel
  controller layout: the town, battles and menus are mouse-only, so the right stick moves the pointer and RT clicks;
  A (Return) reads on; the D-pad and left stick are the arrow keys, which walk the dungeon. Mouse would leave the
  dungeon to pointer clicks on its pad, and RPG Maker controls have no pointer for the menus.

## Known limits

- Built and measured: the town's commands (建設, 内政, 休憩, 配置, 装備, 育成, 商会, 提出), units, items, battles,
  千樹庭's areas, 炎山洞 (opened by its event on 遠征 from turn 10 after five expeditions) with its stairs between
  areas, area entries registered at door spots, the missions (マクドゥの試験) from turn 17, recruits from the town's housing, and area bosses whose
  defeat recruits their leaders.
  Hidden doors (wall 100) open with a 感知 unit in the party (inferred). 配置's 組織図 posts (the town's and, from
  turn 42, the 迷宮's) and 内政's 交換 are there, with the holders' ability raises; posts change the town's
  production. The 迷宮's turn is built from 330 turns of the original: its numbers, marks, depth, widths and
  breakthrough damage are measured; the odds of each event are fitted to the original's counts. Facilities gain
  EXP each turn and level up, and town posts raise their area's production. 配置 has 一覧, 組織図 and 編成, and the
  status card switches to 能力詳細. The later dungeons' floor objects work: traps with their messages, timers and
  minimap icons, teleporters and stairs from the maps' link records, spinners, dark zones, healing magic circles and 堕天牢's floor
  hazards; several effects' sizes are inferred. These live in the original's exe, not its scripts, and are rebuilt
  from play (`docs/systemaoi-cleanroom.md`).
- Battles: the 3D dungeon behind the board (a placeholder picture for now), the area names in the top bar, the
  screen after a loss, and the panels' numbers, which use the app's font rather than the original's.
- Dungeon: enemy groups that appear partway through a visit aren't rebuilt, and how a group behaves after a battle
  is inferred; a floor's taken pickups and opened doors aren't saved (the original's image doesn't seem to hold
  them either). 絶命院 still draws about 6 to 9% darker than the original.
- Saves: a few ability numbers in the battle blocks (凶暴者 and the like) and the script cache after the town grid
  are written empty; the original loads such files. The save comment box takes no text; the comment stays empty.
  The Option screen and the Extra menu aren't built yet.
- The original's option settings (text speed, window transparency, volumes) are its defaults.
