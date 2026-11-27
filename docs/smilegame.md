# Smile Game Builder

Smile Game Builder (SmileBoom) makes 3D RPGs from editor data alone: a game carries no code of its own, and the
player (`sgb_rpgplayer.exe`, a .NET program over the mixed-mode `SharpKmyCore.dll` and the native `kmyCore.dll`)
interprets the data. VNDegen runs these games on a runtime of its own, `Packages/VNDegenKit/Sources/CSmileGame`,
written in C from the games' files and the original player's behaviour under Wine. `docs/smilegame-cleanroom.md`
records every layout and where it came from.

The runtime is in its first stage: it has a Linux runner and no app plugin yet.

## Test games

| Game | Player | Maps | Notes |
|---|---|---|---|
| Princess Colette's Great Adventure (Smoothly Animated Pixel Art RPG) | 1.8.1.0 | 122 | 2D pixel-art characters on 3D chip terrain. Japanese only |
| Sacred (セイクリッド～砕かれた光の冠～) ver1.04 | 1.0.6.26 | 81 | 3D FBX characters, pictures over the map, a row title menu. Japanese only |

Both games use the same data format.

## What runs

- The package, every `.sgr` file, resources from the game and its resource packs, PNG pictures and the game's font.
- The title screen: picture, overlay, the three items as a column or a row, the cursor. New game only.
- Maps: chip terrain in 3D with the map's light colour, map objects and event graphics from FBX, 2D character sheets
  drawn upright, the player walking cell by cell (stairs let it change height), talking to the event in front.
- Events: sheets chosen by every condition kind the games use; talk, touch, autorun and repeating triggers; messages
  with faces, choices and branches, captions and scrolling text, pictures, fades and tint, variables, switches, items,
  money, the party, moving and turning the player and events, event graphics, moving to another map. Colette plays from
  the title to the bridge guard who opens the way to its first battle, and Sacred from the title to the guard at (35,30)
  in the castle.
- Data tables: heroes, battle commands, items, skills, monsters and effects, and the game settings' menu text, sounds
  and windows. One party state (`Party` in `sg_B.h`, `g->b.party`) holds the heroes with their stats, the reserve,
  items, obtained items, money, hero looks and play time; event commands change it, the menu shows it, and saves write
  and read it.
- The field menu (Esc or X): items, skills, equipment, status, saving to a slot with each slot's map and party, quit
  and config, laid out as the PC draws them, with faces, and the menu sounds. Shop (37) and inn (38) commands open
  their screens and branch on the result; 59 recovers the party.
- A software rasterizer draws the 3D scene into the same pixel buffer the other runtimes hand to the frame host.
- Saves: the PC's own `savedata/Save_NN.sgs` files, read and written field for field, with everything the runtime does
  not model yet kept and written back unchanged, so saves move both ways between VNDegen and the original. `saveDir`
  in the config is the folder holding the saves; it defaults to the game's `savedata`.
- Sound: the title and map BGM and ambient sound, with the files' loop points, through `vnsmilegame_audio` as stereo
  floats at `audioRate`. Commands 21 (BGM), 22 (sound) and 64 (music effect) play through the same mixer.
- Camera command 53 (modes 0 and 3) and motions from commands 14 and 69 and from each event sheet.

## Drawing the map

`fielddraw.c` builds each map once into 8 by 8 cell chunks: terrain with stairs, slopes and sunken animated liquids,
a border of the map's outside chip, and the map objects at rest. A frame culls chunks to the view, draws them front to
back, then the characters, then blended surfaces back to front. The sun's shadows come from a depth map built once per
map; 3D characters are added to it every frame. `r3d.c` rasterizes on up to six threads, each filling a band of rows
in the order triangles were queued, so the image is the same as on one thread.

`sg_D.h` is the camera interface event commands call: `sg_camera_get`, `sg_camera_to` (a pose and a time in ms),
`sg_camera_reset` to the map's default, `sg_camera_moving`, and `sg_camera_command` for command 53's modes 0 and 3.
`sg_actor_motion` plays a named motion on a 3D character.

Frame times on a 14-core Linux VM, 960 by 544, six threads, measured with `sgview` over 300 to 1,200 frames while two
PC games and a test ran beside it (load average about 10): walking Sacred's 150 by 150 field 9.7 ms average, 99th
percentile 16.2 ms; Colette's throne room 8.6 ms and castle town 7.6 ms; Sacred's forest 11.0 ms (99th percentile
17.6 ms) and its castle courtyard, 74,000 triangles in view, 22.2 ms. On one thread the same views take 12 to 38 ms.

## The Linux host

`Engines/SmileGame/host` builds with CMake:

- `sgrun <game> <out> [wait:ms] [key:vk] [hold:vk,ms] [okuntil:n] [shot:name] [save:slot] [load:slot]`: headless;
  writes `sgrun.log` and PNG frames to `<out>`, which is also its save folder.
- `sgplay <game>`: an SDL window with the keyboard (arrows, Enter or Z, Esc or X, Shift).
- `sgtest`: the format reader tests. With `SG_COLETTE` and `SG_SACRED` set to the game folders it also checks values
  read from the games' files.
- `sgtest_B`: the data table tests against values from the PC's menus and the game files. `sgtest_B shots
  colette|sacred <game> <out>` writes every menu page as PNG for comparison with the PC.
- `sgtest_D`: the renderer's tests: coverage, the map tail and cameras of both games, the throne room's projection
  against the PC's pixels, and a skinned character's motions.
- `sgview <game> <out.png> <map> <x> <z> <dir> [frames] [hero:name] [hold:vk,ms] [threads:n]`: draws a map at a cell
  and prints frame times.
- `sgtest_A`: the event command tests; with the game folders set it plays both openings (about five minutes).
  `sgtest_A drive <game> <out> ...` plays a scripted path: `until:field|choice|message`, `upto:<text>`, `pick:N`,
  `to:x,z` (walks there), `talk:U|D|L|R`, `hold:U,ms`, `ok`, `wait:ms`, `shot:name`, `where:tag`, `save:N`, `load:N`.
- `sgtest_G`: the joined paths: new-game hero stats against the PC's saves, a load keeping the party, shop and inn
  branches, the BGM command, the menu's save page, and Sacred's castle after a load (queen, king, camera).

## Known limits

- No battles yet: command 65 counts as won.
- Effects (27) and balloons (45) are not drawn, though balloons take their time.
- Events never wander or chase the player; the movement type in their sheets is read but not used.
- Command 10's and 16's codes 4 to 7, 11's one-argument form, and 15, 55, 56, 58, 66, 68, 70, 76 and 77 are logged and
  skipped; none is on either opening path.
- The shop and inn screens were not compared with the PC; the party order page is not in these games' menus. Where
  a slot's page shows a second to fourth hero is a guess (two columns); both games have one hero there.
- Pictures on screen are not saved or restored, so after a load the help bar Sacred shows is missing until an event
  shows it again, and the PC loading a VNDegen save shows none either (see the picture body in
  `docs/smilegame-cleanroom.md`).
- Saves write the motion of events that wander as the sheet's, where the PC writes `wait` after they have moved.
- Static captions end at once; the PC fades them out over about half a second, so the camera move after them starts
  that much earlier than on the PC.
- Experience above level 3 uses a fitted curve, and a few item, skill and monster fields are inferred (see
  `docs/smilegame-cleanroom.md`).
- A hero's magic is taken from the hero tables; saves keep whatever the PC stored in that field (0 for every hero
  seen). Level-ups (round 3) must update the stats a member stores.
- Colette's opening calls for music by a resource pack's GUID; the original plays music none of the game's shipped
  files match there, and VNDegen keeps the title music.
- Sound-effect and music-effect levels and the music effect pausing the BGM follow the BGM measurement and common
  practice; they were not measured on the PC.
- Command 53's modes 1 and 2 (Colette's event rooms) are not mapped; mode 3's target point matches the PC's opening
  scene only roughly.
- Background value 1 (Sacred's field) draws black; the PC never shows it in play.
- The PC darkens wall feet slightly more. Map objects with lights stay in their unlit motion until an event changes
  them; scrolling textures (`uscrollspeed`, the noon clouds) and particle effects are not drawn.
- Sacred's castle courtyard takes about 22 ms a frame on that VM, over the 16.7 ms a 60 fps frame allows.
- The message window's fill differs between the two games on the PC and is drawn the same in both.
