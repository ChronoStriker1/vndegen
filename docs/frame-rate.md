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
  - Godot: `-godotFrameLog`. The native runtimes log their own pacing with `-frameMonitor`.
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
| RPG Maker MV | Karryn's Prison, Mode Select map | Mac app, MacBook | 60 | 60.0 | 45.5 to 47.6 | 90 s | 1.9 | 11-03 | 7a50a58 |
| RPG Maker MV | Karryn's Prison, battle with 4 enemies | Mac app, MacBook | 60 | 60.0 | 50.0 | 90 s | 1.9 | 11-03 | 7a50a58 |
| RPG Maker MV | Karryn's Prison, Mode Select map | iPad simulator | 60 | 60.0 | 55.6 | 90 s | 4.6 | 11-03 | 7a50a58 |
| RPG Maker MV | Karryn's Prison, battle with 4 enemies | iPad simulator | 60 | 60.0 | 50.0 | 90 s | 4.6 | 11-03 | 7a50a58 |
| Godot | Project Hadal, title, intro and play | Mac app, MacBook | 60 | 59.9 | 40.6 | 200 s | 2.4 | 11-03 | 7a50a58 |
| Godot | Project Hadal, title, intro and play | iPad simulator | 60 | 59.5 | 31.5 | 192 s | 4.2 | 11-03 | 7a50a58 |
| Flash | Wolf Girl With You, animated title | Mac app, MacBook | 24 | 24.0 | 19.2 | 150 s | 2.3 | 11-03 | 7a50a58 |
| Flash | Wolf Girl With You, animated title | iPad simulator | 24 | 24.0 | 19.2 to 19.6 | 150 s | 4.9 | 11-03 | 7a50a58 |
| KiriKiri | Sisters x Lovers, logo movie | Mac app, MacBook | 30 | 150 of 150 frames, 0 dropped | see notes | 5 s | 4.9 | 11-03 | 7a50a58 |
| KiriKiri | Sisters x Lovers, logo movie | iPad simulator | 30 | 150 of 150 frames, 0 dropped | see notes | 5 s | 4.5 | 11-03 | 7a50a58 |
| KiriKiri | Violated Hero IV's `iris_s.amv`, from a test script | Mac app, MacBook | 30 clip, 120 display | 120.0 display, a clip frame each tick | 120.0 | 60 s | 2.8 | 11-03 | 7a50a58 |
| KiriKiri | Violated Hero IV's `iris_s.amv`, from a test script | iPad simulator | 30 clip, 60 display | 60.0 display, a clip frame each tick | 60.0 | 60 s | 3.3 | 11-03 | 7a50a58 |
| Ren'Py | Give an imp a chance!, main menu movie, 1080p VP8 | Mac app, MacBook | 60 | 59.9 to 60.1 | 32.6 to 38.9 | 110 s | 2.2 | 11-03 | 7a50a58 |
| Ren'Py | Give an imp a chance!, main menu movie, 1080p VP8 | iPad simulator | 60 | 59.3 to 60.0 | 26.3 to 44.0 | 110 s | 4.2 | 11-03 | 7a50a58 |
| TyranoScript | Jingai Seitairoku, looping 1536x1152 VP8 clip | Mac app, MacBook | 30 | **28.6**, was 27.9 | 5.0, was 3.4 | 120 s | 2.9 | 11-03 | 972c708 |
| TyranoScript | Jingai Seitairoku, looping 1536x1152 VP8 clip | iPad simulator | 30 | 29.7, froze at its first loop before | 14.1 | 120 s | 6.2 | 11-03 | 972c708 |
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

### TyranoScript's looping clips

This one was VNDegen's to fix. WebKit's own loop seeks to the start at the clip's end. For this 1536x1152 VP8 clip, that seek never finished on the iPad simulator: `seeking` fired, `seeked` didn't, and the picture stayed frozen on the first frame. A blob of the same file did the same, so VNDegen's URL handler isn't the cause. On the Mac the picture stopped for 500 to 600 ms each loop.

`tyrano.js` now loops a video by reloading it at its end. The simulator plays on at 29.7 fps with a slowest gap of about 130 ms. The Mac's gap is about 317 ms, once per loop, which is WebKit loading the file again, and with this 10 s clip it leaves the average at 28.6. A second video element loaded ahead could shorten it, but that's more than this check's fix, so it stays as a known limit.

An alpha clip that VNDegen converts to HEVC loops fine with WebKit's own loop on the simulator: Fallen Kunoichi's 3 s `Disguise3.webm` was checked over 30 s. The freeze wasn't tried on RPG Maker MV or MZ clips that aren't converted and are too long or too large for the kept-frame path. A scan of the library's MV and MZ games found 470 clips over 10 s, so they may have it too.

## One line each

- Rance IX's 3D scenes can't be reached: the game stops with a message where the first one starts, in `docs/system4.md`'s known limits.
- The publisher-encrypted KiriKiri games don't run.
- No Socks RPG at 4x is a known limit in [cheats.md](cheats.md). This check measures 1x.
- The Legend of Arcadieu, custom HTML5 with Live2D, has no engine plugin.
- Monster Girl Quest Paradox doesn't reach play, see below.

## Found along the way, not fixed here

- Monster Girl Quest Paradox stops before its first frame. Script section 135 starts with a UTF-8 byte order mark before `=begin`, which Ruby 3.4's `eval` rejects as a syntax error. The runtime's own error description fails on that error too, so the player sees only "The game stopped with an error."
- Taimanin Kurenai's log says `unknown encryption in video.xp3`, so its 2 movies can't play.
