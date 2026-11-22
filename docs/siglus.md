# SiglusEngine

VNDegen runs VisualArt's SiglusEngine games on siglus_rs, an open-source Rust reimplementation of the engine (https://github.com/xmoezzz/siglus_rs, MPL-2.0), built as the VNSiglus framework the app embeds. The game draws inside VNDegen's window through wgpu's Metal backend, so the VNDegen menu and touch controls work as for the other engines. `Engines/Siglus/UPSTREAM.md` lists the pinned commit, the licenses, how the engine is built, and every change VNDegen made to it.

Tested with planetarian HD (SiglusEngine 1.1.132) and Harmonia (1.1.108), both the English Steam releases, in the Mac app and on the iPad and iPhone simulators: title, a new game, the story, saving, quitting, and loading after a restart, by touch, the touch gamepad, and on a case-sensitive disk.

## Games

A game is found by `Scene.pck` or `Gameexe.dat` in the copied folder or one folder down. siglus_rs finds the key each `Scene.pck` is encrypted with from the game's own files, in about a second, and writes it to `key.toml` in the game folder so the next start skips that step.

siglus_rs also runs RealLive games, but this build leaves RealLive out: no library game uses it, and the `reallive` crate cites rlvm (GPL-3.0) in two comments, which hasn't been checked. RealLive is a feature of VNDegen's `vnsiglus` crate, off by default; the plugin, `RealLivePlugin`, stays in the package but isn't registered.

## Languages

Steam releases such as planetarian HD ship one script and Gameexe per language beside the Japanese ones: `SceneEN.pck` and `GameexeEN.dat`, and the same for DE, FR, ES, ZH and ZHTW. The PC engine picks one by Steam's language. VNDegen picks the player's first preferred language (iOS and macOS Settings) that the game ships, and English when none matches, never Chinese. The plain `Scene.pck` is Japanese when the game ships an English pack and English otherwise, as in Harmonia. The script asks the engine for the language code (`GET_LANGUAGE`) and reads its text from the matching column of its database, so the code and the pack go to the engine together. A per-game language choice isn't offered yet.

## Saves

Saves are the PC engine's own files in the PC's folder beside the game: `savedata/` for the plain pack, and `savedata_en/` and so on for a language pack, as the PC version names them. A folder of another spelling, such as `SAVEDATA/` from a Windows backup, is used as it is. The files are `0000.sav` and up for the slots with `0000.bmp` thumbnails, `global.sav`, `read.sav` and `config.sav`; siglus_rs adds `twitter_state.dat`, which the PC ignores.

Checked against both games' own engines under Wine: Harmonia (1.1.108) and planetarian HD (1.1.132). planetarian HD's exe runs only `Scene.pck` or `SceneZH.pck` and picks the Chinese pack when Steam doesn't answer, so its checks used the Chinese pack on both sides; the slot layout doesn't depend on the language.

- PC to VNDegen: slots, `global.sav`, `read.sav` and `config.sav` load, and the story resumes at the saved line with the page's earlier lines in place. Saving a just-loaded PC slot again gives the same data byte for byte.
- VNDegen to PC: slots load and resume with their message text and play on, both from a playthrough VNDegen started and from one continued from a PC slot. `global.sav` and `read.sav` load. `config.sav` is written in the version the game's engine reads: the version of the one already there, or for a new one the version that goes with the game's `.exe` (1.2 up to build 132, 1.3 from 133, 1.4 from 141). Harmonia's engine refuses to start with any other version.

### Slot layout

A slot file is a header followed by the LZSS-packed payload. The header is the version (1.0), the save time, seven UTF-16 strings of 256 characters (append folder and name, scene title, message, full message, two comments), 256 flag values and the packed size, 4,652 bytes in both games. siglus_rs packs without matches, as literals only, which the PC engine reads. The payload holds the save id, the strings again, the local stream, the 44-byte extra stream and the selection snapshots.

The local stream is read in order with no field tags, so every record has to be the size the game's engine expects. The engines before build 133 use the older local layout: no script font name, a 344-byte local data block, no backlog check flag, and the `.gan` work after each object. What VNDegen settled against the PC versions:

- The current proc and the proc stack. A slot is taken at a save point while the script runs: the current proc is the script proc (type 1) and the stack holds one empty proc (type 0), 147 bytes each. With an empty current proc and no stack the PC loads the slot but never runs the script, and the message window stays empty.
- Fixed arrays are an end offset, a count and the records. The PC can write fewer records than the count: planetarian HD writes 90 of its 100 stage objects and puts the end offset after the 90th. A reader stops at the end offset.
- A world record before build 133 is the mode, the nine camera events (44 bytes each), the view angle, mono, and the xz camera rotation event: current time, end time, delay, loop type, speed type, start x and z, end x and z. That is 444 bytes. Newer engines save order, layer and the two wipe fields in place of the rotation, 424 bytes. A slot with the newer record crashes Harmonia's engine. Only the current time and the loop type (-1) differ from zero in either game, so the order of the other rotation fields is inferred.
- A message page's PARAM record holds the text cursor (0,36 after one line of Harmonia's 24-pixel text with 12 pixels between lines), the current talk margins (0 in every slot seen) and the template's name text size. The window's own message offset and margins are in the window record. Writing the offset as the cursor made the PC draw the first line of a page indented.

Differences the PC versions accept: siglus_rs writes some fixed arrays empty where the PC writes the full count from the Gameexe (scene properties, masks, g00 buffers, screen effects, PCM events, edit boxes, the frame's face and object lists), 11 counters where Harmonia writes 16, world mode 1 where the PC writes 0, and its own clock values in the message window's time fields.

## Text

Text is drawn in the game's own font when it ships one (planetarian HD's `NotoSansMonoCJKjp-Regular.otf`), and otherwise in the font pack's substitute for the Windows font it names (Harmonia's ＭＳ ゴシック), through the same table as the other engines ([fonts.md](fonts.md)). MS PGothic's substitute stands in for anything else.

Changes for the English releases, beyond the original engine's layout:

- Latin words wrap whole, in the message window and the backlog, where the original breaks lines by character cells.
- planetarian HD's English text stores most commas as a tab: before a space (1,918 times) or between digits as a thousands separator (20 times), with 4 real commas, where the German, Spanish and French texts have about 2,000 commas and no tabs. A tab before a space or between two digits shows as a comma in messages, names, the backlog and save descriptions. A tab used any other way is left alone.
- A message window whose Gameexe sets `NAME_DISP_MODE` 2 shows no speaker name, as both test games expect; planetarian's English script still names Yumemi in Japanese for its voice table.
- Confirmations the game's Gameexe doesn't word, such as loading a save, use the engine's English texts when the language isn't Japanese.

## Movies

`.omv` movies (Ogg Theora, as Harmonia's) play through siglus_rs's own decoder. `.wmv` movies (planetarian HD's opening, WMV3 and VC-1 with WMA sound) play through the app's FFmpeg: vnsiglus finds vnffmovie's functions in the KiriKiri framework with `dlsym`, as the NekoSDK runtime does ([video.md](video.md#vnffmovie-wmv-through-the-apps-ffmpeg)), and the engine takes the frame due at the movie clock and pulls the sound for kira. siglus_rs's own WMV and WMA decoder is LGPL and isn't in the app. Without the KiriKiri framework, `.wmv` movies are skipped.

## Controls

Games start on the "Visual novel" layout: A reads on (Return), B right-clicks, which opens the game's menu, X holds Control to skip, Y presses Escape, and the bumpers turn the wheel, back through the message history and on. Touches act as the mouse. A game's message box shows as a system alert; Confirm, Return or Space picks its first button, and Cancel, Escape or E its last.

While VNDegen's menu is open the game stops and its sound is muted. Kira 0.9 can't pause, so sounds keep playing unheard. The game writes its global save then, since iOS can end a paused app without warning.

## Known limits

- The language follows the system's preferred languages; there is no per-game choice.
- Sound keeps playing, muted, while VNDegen's menu is open.
- SiglusEngine games read a gamepad on the PC. VNDegen sends them keys and clicks instead, so a game's own gamepad prompts don't show.
- Network features, such as planetarian HD's Tweet button, do nothing.
- RealLive games aren't supported in this build.
- Tomoyo After's dungeon game and Little Busters!' baseball game, which are RealLive DLLs, would not run: siglus_rs's ports of them are decompiled code with no license, and VNDegen leaves them out.
