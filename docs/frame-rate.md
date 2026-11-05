# Frame rate check

This is the check from item 1 of the roadmap's Next section, done on 2026-11-03. [development.md](development.md#frame-rate) has the rule. It covers games that aren't strictly visual novels, and visual novels' video scenes.

## How to read the table

- Platform says which machine ran it. "Mac app, MacBook" is an M5 Max with a 120 Hz display, and "Mac app, mini" is an M4 with a 4K display at 60 Hz. "iPad simulator" is an iPad Pro 13-inch simulator on the mini unless the row says otherwise. Simulator timing is not iPad timing.
- Average and 1% low are frames a second. A range covers the windows of that run.
- Each engine's numbers come from its own source:
  - RPG Maker MV and MZ and Flash: frames drawn to the canvas, from the DEBUG input script's `fps` step. "Engine" in a row is `Graphics.frameCount` per second, the game's own update rate.
  - TyranoScript: new video frames from `requestVideoFrameCallback`, in a script in the input script's `js` step, because the `fps` step counts WebGL drawing and a Tyrano video has none.
  - Ren'Py: new movie frames from `renpy.display.video.get_movie_texture`, by a probe `.rpy` placed in a scratch copy of the game, because Ren'Py also redraws between them.
  - KiriKiri: display refreshes from `-frameMonitor`'s `KKFRAMES` line, and a movie's frames from the `movie:` lines in the game's log.
  - Godot: `-godotFrameLog`, without `-godotSnapshot`. The snapshot writes a PNG on the main thread every 2 s, which stalls the game for about 80 to 90 ms each time. It took Barely Working from 59.9 to 57.5 and Project Hadal from 59.4 to 56.9, with seconds alternating between 54 to 56 and 60 to 61 frames. The native runtimes log their own pacing with `-frameMonitor`.
- Load is the load average at the start of the run. The MacBook's runs had nothing else of ours running. The mini's simulator runs also had the simulator's own processes and agent-device's runner.
- Reused rows come from earlier threads' records. They stand because no later change touched their drawing path. Commit is `not recorded` where the record gives none, and the same goes for load.

## Results

| Engine | Game and scene | Platform | Target | Average | 1% low | Length | Load | Date | Commit |
|---|---|---|---|---|---|---|---|---|---|
| RPG Maker MZ | No Socks RPG, map | Mac app, MacBook | 60 | 59.9 to 60.0 | 52.6 | 90 s | 5.8 | 11-03 | 7a50a58 |
| RPG Maker MZ | No Socks RPG, battle | Mac app, MacBook | 60 | 60.0 | 41.7 to 45.5 | 90 s | 5.8 | 11-03 | 7a50a58 |
| RPG Maker MZ | No Socks RPG, map | Mac app, mini | 60 | 51.8 to 60.0 | 40.0 to 47.6 | 720 s | 5.9 | 11-03 | 972c708 |
| RPG Maker MZ | No Socks RPG, battle | Mac app, mini | 60 | **40.5 to 45.2**, engine 54 to 59 | 23.8 to 25.6 | 120 s | 5 | 11-03 | 972c708 |
| RPG Maker MZ | No Socks RPG, battle, filters removed | Mac app, mini | 60 | 59.8 | 37.0 | 30 s | 5 | 11-03 | 972c708 |
| RPG Maker MZ | No Socks RPG, map | iPad simulator | 60 | **38.9 to 50.4** | 30.3 to 43.5 | 90 s | 3.9 | 11-03 | 7a50a58 |
| RPG Maker MZ | No Socks RPG, battle | iPad simulator | 60 | **27.4 to 28.1**, engine 55 | 17.9 to 18.2 | 90 s | 3.9 | 11-03 | 7a50a58 |
| RPG Maker MZ | No Socks RPG, battle, filters removed | iPad simulator | 60 | 59.8 | 32.3 | 20 s | 4.3 | 11-03 | 972c708 |
| RPG Maker MZ | No Socks RPG, map | iPad simulator on the MacBook | 60 | 60.0 | 52.6 to 55.6 | 90 s | 11 | 11-03 | 972c708 |
| RPG Maker MZ | No Socks RPG, battle | iPad simulator on the MacBook | 60 | 55.5 to 56.5 | 34.5 to 37.0 | 90 s | 11 | 11-03 | 972c708 |
| RPG Maker MZ | Fallen Kunoichi, its title map | Mac app, MacBook | 50 drawn, engine 60 | 50.0, engine 60.0 | 28.6 | 60 s | 1.8 | 11-03 | 7a50a58 |
| RPG Maker MZ | Fallen Kunoichi, same map, game's frame skip off | Mac app, MacBook | 60 | 60.0 | 52.6 | 30 s | 1.8 | 11-03 | 7a50a58 |
| RPG Maker MZ | Fallen Kunoichi, same map, 1080p clip | Mac app, MacBook | 50 drawn, engine 60 | 50.0, engine 60.0 | 28.6 | 30 s | 1.8 | 11-03 | 7a50a58 |
| RPG Maker MZ | Fallen Kunoichi, battle | Mac app, MacBook | 30 to 40 drawn, engine 60 | 30.0, engine 60.0 | 27.8 | 30 s | 1.8 | 11-03 | 7a50a58 |
| RPG Maker MZ | Fallen Kunoichi, its title map | iPad simulator | 50 drawn, engine 60 | 50.0, engine 60.0 | 28.6 | 60 s | 4.7 | 11-03 | 7a50a58 |
| RPG Maker MZ | Fallen Kunoichi, same map, game's frame skip off | iPad simulator | 60 | 60.0 | 55.6 | 30 s | 4.7 | 11-03 | 7a50a58 |
| RPG Maker MZ | Fallen Kunoichi, same map, 1080p clip | iPad simulator | 50 drawn, engine 60 | 50.0, engine 60.0 | 28.6 | 30 s | 4.7 | 11-03 | 7a50a58 |
| RPG Maker MZ | Fallen Kunoichi, battle | iPad simulator | 30 to 40 drawn, engine 60 | 38.2, engine 60.0 | 25.0 | 30 s | 4.7 | 11-03 | 7a50a58 |
| RPG Maker MZ | Demon Queen game, 1080p `DQPMBathing.mp4` after two alpha clips loaded and unloaded, 5 runs | Mac app, Mac mini | 30 clip, 60 page | clip 28.7 to 29.5, page 60.1 to 60.2 | clip 10.9 to 14.9, page 43.5 to 45.5 | 70 s each | 2.5 to 4.0 | 11-04 | 9e0a10f |
| RPG Maker MV | Karryn's Prison, Mode Select map | Mac app, MacBook | 60 | 60.0 | 45.5 to 47.6 | 90 s | 1.9 | 11-03 | 7a50a58 |
| RPG Maker MV | Karryn's Prison, battle with 4 enemies | Mac app, MacBook | 60 | 60.0 | 50.0 | 90 s | 1.9 | 11-03 | 7a50a58 |
| RPG Maker MV | Karryn's Prison, Mode Select map | iPad simulator | 60 | 60.0 | 55.6 | 90 s | 4.6 | 11-03 | 7a50a58 |
| RPG Maker MV | Karryn's Prison, battle with 4 enemies | iPad simulator | 60 | 60.0 | 50.0 | 90 s | 4.6 | 11-03 | 7a50a58 |
| Godot | Project Hadal, title, intro and play | Mac app, MacBook | 60 | 59.9 | 40.6 | 200 s | 2.4 | 11-03 | 7a50a58 |
| Godot | Project Hadal, title, intro and play | iPad simulator | 60 | 59.5 | 31.5 | 192 s | 4.2 | 11-03 | 7a50a58 |
| Godot | Barely Working, title and Level 1 (Godot 3) | Mac app, Mac mini | 60 | 59.9, Level 1 60.0 | 43.1 to 43.6 | 202 s | 2.2 to 7.9 | 11-04 | 9e0a10f |
| Godot | Sexual Sealing Dungeon: Eronest, opening with no input (Godot 3) | Mac app, Mac mini | 60 | 59.9 | 44.0 | 101 s | 2.8 | 11-04 | 9e0a10f |
| Godot | Project Hadal, opening with no input | Mac app, Mac mini | 60 | 59.4 | 29.6 | 101 s | 3.0 | 11-04 | 9e0a10f |
| Flash | Wolf Girl With You, animated title | Mac app, MacBook | 24 | 24.0 | 19.2 | 150 s | 2.3 | 11-03 | 7a50a58 |
| Flash | Wolf Girl With You, animated title | iPad simulator | 24 | 24.0 | 19.2 to 19.6 | 150 s | 4.9 | 11-03 | 7a50a58 |
| KiriKiri | Sisters x Lovers, logo movie | Mac app, MacBook | 30 | 150 of 150 frames, 0 dropped | see notes | 5 s | 4.9 | 11-03 | 7a50a58 |
| KiriKiri | Sisters x Lovers, logo movie | iPad simulator | 30 | 150 of 150 frames, 0 dropped | see notes | 5 s | 4.5 | 11-03 | 7a50a58 |
| KiriKiri | Violated Hero IV's `iris_s.amv`, from a test script | Mac app, MacBook | 30 clip, 120 display | 120.0 display, a clip frame each tick | 120.0 | 60 s | 2.8 | 11-03 | 7a50a58 |
| KiriKiri | Violated Hero IV's `iris_s.amv`, from a test script | iPad simulator | 30 clip, 60 display | 60.0 display, a clip frame each tick | 60.0 | 60 s | 3.3 | 11-03 | 7a50a58 |
| KiriKiri | Taimanin Kurenai, opening and ending movies, from a test script | Mac app, mini | 30 | 3096 of 3096 and 3000 of 3000 frames, 0 dropped | see notes | 103 s and 100 s | 35 | 11-04 | 0b1e1d5 |
| KiriKiri | Taimanin Kurenai, opening and ending movies, from a test script | iPad simulator | 30 | 3096 of 3096 and 3000 of 3000 frames, 0 dropped | see notes | 103 s and 100 s | 25 | 11-04 | 0b1e1d5 |
| Ren'Py | Give an imp a chance!, main menu movie, 1080p VP8 | Mac app, MacBook | 60 | 59.9 to 60.1 | 32.6 to 38.9 | 110 s | 2.2 | 11-03 | 7a50a58 |
| Ren'Py | Give an imp a chance!, main menu movie, 1080p VP8 | iPad simulator | 60 | 59.3 to 60.0 | 26.3 to 44.0 | 110 s | 4.2 | 11-03 | 7a50a58 |
| TyranoScript | Jingai Seitairoku, looping 1536x1152 VP8 clip | Mac app, MacBook | 30 | **28.6**, was 27.9 | 5.0, was 3.4 | 120 s | 2.9 | 11-03 | 972c708 |
| TyranoScript | Jingai Seitairoku, looping 1536x1152 VP8 clip | Mac app, mini | 30 | **28.9** | 5.8 | 120 s | 2.8 | 11-03 | dcea258 |
| TyranoScript | Jingai Seitairoku, looping 1536x1152 VP8 clip | iPad simulator | 30 | 29.7, froze at its first loop before | 14.1 | 120 s | 6.2 | 11-03 | 972c708 |
| RGSS | Monster Girl Quest Paradox, walking in the village | Mac app, mini | 60 | 59.5 to 60.0 | 30.0 to 59.9 | 80 s | 13 | 11-04 | 2f48b68 |
| RGSS | Monster Girl Quest Paradox, battle with 5 enemies | Mac app, mini | 60 | 59.5 to 60.0 | 30.0 to 59.9 | 80 s | 7.4 | 11-04 | 2f48b68 |
| RGSS | Monster Girl Quest Paradox, walking in the village | iPad simulator | 60 | 58.5 to 60.0 | 15.0 to 60.0 | 80 s | 12 | 11-04 | 2f48b68 |
| RGSS | Monster Girl Quest Paradox, battle with 5 enemies | iPad simulator | 60 | 59.6 to 60.0 | 33.3 to 60.0 | 80 s | 7.6 | 11-04 | 2f48b68 |
| RGSS, reused | Test Games 33 (VX), 55 (VX Ace), 14 (VX Ace), 37 (XP) | Mac app, mini | 60, or 120 shown at 60 | 59.7 to 60.0 | 43 to 60 | not recorded | not recorded | 10-30 | ed7a60e |
| RGSS, reused | Test Games 33, 55, 14, 37 | iPad simulator | 60, or 120 shown at 60 | 59.4 to 60.0 | 30 to 60 | not recorded | not recorded | 10-30 | ed7a60e |
| RGSS, reused | BLACK SOULS II, Play Movie | Mac app, MacBook, and iPad simulator | 60 | 60 | not recorded | 11 s | not recorded | 11-01 | 4a97884 |
| Wolf RPG, reused | Test Game 53, battle | Mac app, mini | 60 | 60.0 | 59.9 median, 42.8 lowest | 110 s | 400 to 490 | 10-30 | 2875833 |
| Wolf RPG, reused | Test Game 53, battle | iPad simulator | 60 | 59.6 | 37.5 median, 23.1 lowest | 120 s | 26 to 65 | 10-30 | fb8335a |
| Wolf RPG, reused | Parade Buster, battle | Mac app, mini | 60 | 59.5 to 59.8 | 30 to 42 | not recorded | not recorded | 11-01 | 788b0c8 |
| Wolf RPG, reused | YUME-SAME, battle, after its image fix | iPad simulator | 60 | 59.4 to 60.0 | 27.3 to 60.0 | 130 s | not recorded | 11-01 | 04606b3 |
| AliceSoft System 3, reused | Panyon, tactical battle | Mac app, mini | 60 | 60.0 | 60 | not recorded | not recorded | 10-31 | 2008a53 |
| AliceSoft System 3, reused | Panyon, tactical battle | iPad simulator | 60 | 60.0 | one hitch | 147 s | not recorded | 10-31 | 2008a53 |
| AliceSoft System 4, reused | Sengoku Rance, battle | Mac app, mini | 60 | 60.0 | 60 median, 41 lowest | 250 s | low | 10-31 | not recorded |
| AliceSoft System 4, reused | Sengoku Rance, battle | iPad simulator | 60 | 59.2 | 37 median | 200 s | 80 | 10-31 | not recorded |
| AliceSoft System 4, reused | Rance 02, opening movie | Mac app, mini | 30 | 29.89, 6787 of 6810 frames | 42 median, display | 227 s | low | 10-31 | not recorded |
| AliceSoft System 4, reused | Rance 02, opening movie | iPad simulator | 30 | 29.98, 6806 of 6810 frames | 60 median, display | 227 s | 10 | 10-31 | not recorded |
| NScripter, reused | Test Game 26, dungeon walking | Mac app, mini | 60 | 60.0 | 60.0 | 50 s | 3 | 10-28 | not recorded |
| NScripter, reused | Test Game 26, dungeon walking | iPad simulator | 60 | 60.0 | 60.0 | 40 s | 3 | 10-28 | not recorded |
| NScripter, reused | Test Game 98, movies | Mac app, mini | 29.97 | 30.6 drawn | 60.0 display | 40 s | 3 | 10-28 | not recorded |
| NScripter, reused | Test Game 98, movies | iPad simulator | 29.97 | 30.6 drawn | 60.0 display | 50 s | 3 | 10-28 | not recorded |
| Artemis, reused | Test Game 81, opening movie | Mac app, MacBook | 29.97 | 29.97 | 19.95 | 101 s | 30 | 10-31 | afaeec9 |
| Artemis, reused | Test Game 81, opening movie | iPad simulator | 29.97 | 29.97 | 20.00 | 101 s | quiet | 10-31 | afaeec9 |

A 1% low near 20 for a 30 fps or 24 fps movie is that movie's cadence on a 60 Hz display, where each frame shows for two or three refreshes. It isn't a dropped frame.

On the MacBook's 120 Hz display a 60 fps game reads correctly: WebKit's page runs at 60, Godot's log gives its target as 60, and Ren'Py's movie runs at its own 60. KiriKiri's pacing line reports the display's 120, since the engine runs once per display refresh.

## Measured with limits

- Karryn's Prison: the battle ran into its first turn and stayed in that turn's phase while measured, with Karryn and four enemies on screen. The prison floors weren't reached.
- Wolf Girl With You: its title is a full-screen animation of a character and moving shadows. Whether a few taps entered play isn't known, so its heaviest scenes may not be covered.
- Fallen Kunoichi: the map measured is the one the game calls its title screen, with 14 pictures, because the game skips the usual title. Its story maps weren't reached.
- KiriKiri: Violated Hero IV's title needs clicks on buttons, and its battles weren't reached. A script plays the game's clip through the same plugin instead, and draws a clip frame each tick, 2 to 4 times the clip's 30 fps, so it asks more than the game does. It isn't the game's scene. The movie log gives frames shown and dropped but no 1% low, and the 5 s logo plays while the game is still loading: the display's own 1% low over that first 10 s window was 58.9 on the Mac and 20.8 on the simulator.
- KiriKiri: Taimanin Kurenai's movies are plain 1024x768 MPEG-1 at 30 fps. A test script calls the game's own `sysmovie` for both, on a case-sensitive volume with the case of 39 names swapped, so play didn't reach them through the story. The movie log gives no 1% low. The Mac's display held 60.0 with a 1% low of 59.8 to 59.9 through both movies. The simulator run had no `-frameMonitor`. Other threads' work had the mini at a high load, and no frame dropped.
- The MacBook's simulator rows started at a load of 11 to 30, while the simulator's test runner started.

## Shortfalls and their causes

### No Socks RPG on the iPad simulator and the mini's Mac app

The game draws at about 28 fps in battle on the mini's simulator, and about 44 in the mini's Mac app. Its engine keeps updating at 54 to 60 a second, so the game's speed holds and the picture drops frames. The cause is the game's own PIXI filters on a GPU that can't finish them in time, not VNDegen:

- The battle scene carries filters on 87 objects. Removing them takes the simulator from 27.3 to 59.8 fps, and restoring them takes it back to 28.5. The mini's Mac app goes from 43.7 to 59.8.
- The page's own work is small: 2.2 ms of updates and 1.4 ms of drawing per engine frame on the simulator. WebKit's web content and GPU processes sampled well under one core each, and the GPU process's threads were mostly waiting.
- The same simulator on the MacBook's M5 Max holds 60.0 on the map and 55.5 to 56.5 in battle. The MacBook's Mac app holds 60.0.
- Other heavy games don't fall short on the same simulator. Karryn's Prison at 1440x810 and Fallen Kunoichi at 1920x1080 with its frame skip off both hold 60.

A real iPad's GPU is likely to do better than the simulator's path, but that isn't measured. VNDegen could only change this by altering the game's filters, which would change how it looks, so nothing was changed. The game wasn't compared on its own engine, since NW.js for Windows doesn't run on these Macs.

### The Mac mini's fall to 30 Hz

This looks like the same thing, not a machine fault. No Socks RPG ran 15 minutes in the mini's Mac app, unlocked, at a load of about 5, with the display kept awake and no thermal warning. Its frame rate didn't fall with time. The map ran at 52 to 57 for the first 7 minutes and at 60 for the last 5, past the 10 minute display sleep setting, while the player walked at random. Only the battle fell, to 40 to 45, and it came back to 59.8 with the game's filters removed.

The likely mechanism, inferred from these numbers, is that a frame that misses the display's deadline makes WebKit's next animation frame wait for the one after, so the rate steps toward half while MZ runs two updates per frame to keep up. An earlier thread's log at a load of 46 to 250 shows that exact pair: 30 to 34 drawn with the engine at 60 to 67. So load makes it worse. Earlier runs' other cases of this weren't rerun, so this cause is inferred for them.

The run without `caffeinate -d` wasn't done. The display wasn't what changed the rate, and Droppy, an app on the mini, holds a display sleep assertion, so that run couldn't have slept the display without changing the machine's setup.

### Fallen Kunoichi at 50 fps

This is the game's own setting. Its MultiTweaks plugin ships with "General frame skip" at 6, which draws 5 of every 6 frames, "Battle frame skip" at 3, and "Effekseer frame skip" at 2. With the general skip turned off from a script, the same map draws 60.0 fps with a 1% low of 52.6 on the Mac and 55.6 on the simulator. The 1% low of 28.6 with the skip on is the skipped frame's gap, and clips make no difference. Nothing to fix.

### Looping clips in TyranoScript, RPG Maker MV and MZ

These were VNDegen's to fix. On the iPad simulator no seek ever finishes in a WebM with Vorbis sound: `seeking` fires, `seeked` doesn't, and the picture stays on its frame. Pausing first or seeking again doesn't help, and only a reload gets it moving. It's the sound, not the picture: Living with the Absolute Bustiest College Girl!'s VP8 `ido5_01.webm` hangs, the same video with its sound removed loops, and a VP9 clip given that Vorbis track hangs. A blob of the Jingai Seitairoku clip did the same, so VNDegen's URL handler isn't the cause. On a Mac those seeks finish, but every WebM loop stops for 500 to 535 ms, and on the simulator every WebM without Vorbis stops for 485 to 517 ms.

`tyrano.js` and `rpgmaker.js` now loop a WebM by reloading it at its end. In Jingai Seitairoku the simulator plays on at 29.7 fps with a slowest gap of about 130 ms. The Mac's gap there is 300 to 317 ms, once per loop, and with this 10 s clip it leaves the average at 28.6 to 28.9. A second video element loaded ahead could shorten it, but that's more than this check's fix, so it stays as a known limit.

In MV and MZ the gap at each loop is now 34 to 125 ms on both the Mac and the simulator, shown in the table below. `rpgmaker.js` holds the last frame on the texture during the reload, so nothing flashes. It also keeps the clip's speed. A seek to 0 while a WebM plays also reloads, because MoviePicture makes that seek as each clip starts, so its Vorbis clips froze on their first frames before the loop was ever reached. Any other seek still stuck after a second restarts the clip from the start.

| Clip, 1440x810 WebM unless noted | Clip fps | Mac before | Mac after | Simulator before | Simulator after |
| --- | --- | --- | --- | --- | --- |
| Living's `ido5_03`, 1.5 s VP8 with Vorbis, MoviePicture | 30 | 22.4, gap 535 ms | 28.3, gap 116 ms | not measured on this clip | 29.3, gap 67 ms |
| Living's `ido5_01`, the same kind | 30 | not measured | not measured | froze at about 0.18 s | 29.4, gap 69 ms |
| Living's `18_2`, 2 s VP9 with Opus, at 0.81 speed, MoviePicture | 48.6 | 40.2, gap 501 ms | 47.3, gap 100 ms | 40.0, gap 500 ms | 47.0, gap 111 ms |
| Living's `18_7`, 2 s VP9, MoviePicture | 60 | 48.0, gap 517 ms | 56.6, gap 34 ms | 54.5 over 6 s, gap 500 ms | 59.5, gap 37 ms |
| Living's `18_7`, Play Movie and MovieManager | 60 | 50.5, gap 517 ms | 59.4, gap 34 ms | 45.9, gap 517 ms | 59.5, gap 34 ms |
| Living's `ido15_01`, 33 s VP9 with Opus, MoviePicture | 30 | 29.5, gap 568 ms | 29.9, gap 116 ms | 29.6, gap 435 ms | 29.9, gap 125 ms |
| Cornelica's `mini_cut1`, 0.52 s 600x400 VP9, MoviePicture | 60 | not measured | not measured | 30.2, gap 518 ms | 56.8, gap 35 ms |

Each figure is new video frames per second from `requestVideoFrameCallback` over 12 to 40 s, with the slowest gap. The mini's load average was 3 to 6, or 7 to 13 for `mini_cut1`, with another thread's app open on the Mac. Simulator timing isn't iPad timing, and no physical device has run this. On the simulator the engine held 60 fps during these clips. The Mac runs didn't log it.

An MP4 keeps WebKit's loop, because reloading made it worse: the Demon Queen game's 2560x1440 H.264 `IntroHandjob2.mp4` stopped 415 to 450 ms by reloading and 116 to 248 ms by seeking. Alpha clips, which VNDegen converts to HEVC, keep it too, along with kept frames. Fallen Kunoichi's `Disguise3`, Cornelica's `ddd`, and No Socks RPG's `loafer1` were checked on the simulator.

A scan of the library's 118 MV and MZ games found 4,325 clips in 28 of them. 470 are over 10 s, as an earlier scan counted, but length was the wrong measure: 1,146 clips are ones the games ask to loop, once 257 MP4 copies that sit beside a WebM and don't play here are set aside, and 852 of those, in 10 games, took WebKit's loop. 827 of the 852 are VP9 clips of 2 s or less. Only 7 looped clips have Vorbis sound, 6 in Living and 1 in Welcome to the Monmusu Park, and 16 Vorbis clips play through MoviePicture at all. The scan reads event commands, so it misses clips a script names at run time, and 3 games packed into an exe couldn't be read.

Known limits:

- A seek to anywhere but 0 can't work in a Vorbis WebM on the simulator, so after a second the clip restarts from its start. Returning to a map whose MoviePicture clip saved its position does this.
- HENTAI CAVEMAN skips a Vorbis movie by seeking to its end. By the rule above it should play again from the start and then end, rather than skip. This wasn't tried.
- A paused Vorbis WebM that seeks still hangs, and so does a finished, non-looping one that the game plays again, which is a seek inside WebKit.
- A seek still unfinished after a second counts as hung, so one that slow on a loaded machine would restart its clip.
- A short MZ WebM with Vorbis sound that ends with its kept frames only part done takes WebKit's loop and hangs. The picture still plays from the kept frames if they complete, but its sound stops. No library game loops such a clip.

## One line each

- Rance IX's 3D scenes can't be reached: the game stops with a message where the first one starts, in `docs/system4.md`'s known limits.
- The publisher-encrypted KiriKiri games don't run.
- No Socks RPG at 4x is a known limit in [cheats.md](cheats.md). This check measures 1x.
- The Legend of Arcadieu, custom HTML5 with Live2D, has no engine plugin.
- Monster Girl Quest Paradox now reaches play, after the script loading fixes in [rgss.md](rgss.md). Its battle rows use the game's largest troop, five Angel Soldiers, started by a script from a village save and held open for the timing, because the game ends that fight after about 45 s. The 15.0 low is one window of 8, and the rest are 50 or higher. No build ran during these four runs, and other threads' iPad simulators or Mac app were open during some of them.

## Found along the way

- Taimanin Kurenai's two movies play. The earlier `unknown encryption in video.xp3` message was a false warning: both movies are plain MPEG-1 files whose checksums match. Only an incomplete warning entry fit the encryption check's byte budget, and its made-up checksum caused the message. VNDegen now leaves such incomplete evidence unclassified instead of reporting unsupported encryption.
