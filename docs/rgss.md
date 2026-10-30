# RPG Maker XP, VX and VX Ace

VNDegen runs RPG Maker XP, VX and VX Ace games (RGSS1, RGSS2 and RGSS3) on its own RGSS runtime, in the app's process
on Mac, iPhone and iPad. [rgss-cleanroom.md](rgss-cleanroom.md) says where each part's behavior comes from.

## How it works

- `Packages/VNDegenKit/Sources/CRGSS` is the runtime, in C, with `include/vnrgss.h` as its interface. The game's
  scripts run on MRI Ruby 3.4.9 on a thread of the runtime's own. `Graphics.update` composes the frame on the CPU
  and hands it over; `EngineRGSS` shows it through `FrameHost`'s Metal view and plays the mixed sound through its
  `PCMAudioOutput`. Each display refresh lets the game run for the time that passed, at the game's own frame rate
  (40 for XP, 60 for VX and VX Ace, or what the game sets).
- `scripts/rgss-ruby.sh` builds Ruby static, with no JIT and only the zlib and stringio extensions, into
  `Packages/VNDegenKit/build/RGSSRuby.xcframework` for macOS (arm64 and x86_64), iOS and the iOS simulator. Two
  patches in `Engines/RGSSRuntime/patches` restore Ruby 1.8 and 1.9 behavior games need.
- `Engines/RGSSRuntime/ruby` holds the Ruby VNDegen adds to every game: the RPG module of each version, Ruby 1.8
  and 1.9 compatibility, relative file paths, and the Win32API emulation. `tools/embed.py` turns them into
  `CRGSS/core/prelude.c`.
- Ruby starts once per process. Between games, the runtime removes everything the last game added: its classes,
  constants and globals, its threads, and its changes to Ruby's own classes.

## Files and saves

The game's folder is the working directory, as on PC. Reads look in the game's save folder in VNDegen's data
directory, then the game folder, then its archive (`Game.rgssad`, `Game.rgss2a`, `Game.rgss3a`), then the RTPs Game.ini
names, whatever case the game uses. Writes go to the save folder, so the game folder is never written to. Saves
copied in with a PC game still load.

Saves are the PC engine's format: Ruby's Marshal format, with XP's and VX's strings written without encodings as Ruby
1.8 wrote them. VX saves from the PC engine load in VNDegen, and VNDegen's VX and VX Ace saves load in the PC engine.

## Test games

| Game | Version | Packed | Checked |
| --- | --- | --- | --- |
| Test Game 14 | VX Ace | `Game.rgss3a`, VX Ace RTP | title, new game, events with pictures and choices, save, load in the PC engine |
| Test Game 33 | VX | `Game.rgss2a`, VX RTP | title, PC saves load, map, save, load in the PC engine |
| Test Game 37 | XP | `Game.rgssad`, XP RTP | title, new game, events with pictures and messages, autotiles match the PC engine |
| Test Game 55 | VX Ace | unpacked, no RTP | first map, HUD, windows; resizes the screen to 1024x768 |

## Win32API

The emulation covers what the test games call: FindWindow, GetSystemMetrics, SystemParametersInfo,
GetWindowRect, GetClientRect, MoveWindow, SetWindowPos, ShowWindow, UpdateWindow, SetWindowLong, GetWindowText,
GetForegroundWindow, SetForegroundWindow, SendInput, GetDC, ReleaseDC, FillRect, CreateSolidBrush, DeleteObject,
CreateWindowEx, GetAsyncKeyState, GetKeyState, GetPrivateProfileString, GetPrivateProfileInt,
WritePrivateProfileString, MultiByteToWideChar, WideCharToMultiByte, AllocConsole, SetConsoleTitle, GetTickCount.
Window calls act on one window the size of the screen. Settings a game writes to its ini last until it ends. Any
other call logs once and returns 0.

## Known limits

- `Graphics.play_movie` does nothing: no test game plays a movie.
- F12's reset isn't available, as iPhones and iPads have no F12.
- Message boxes (`msgbox`, and `print` and `p` in XP and VX) go to the game's log instead of stopping the game.
- Tilemap flash data (`flash_data`) isn't drawn.
- Blending onto a transparent bitmap, the order of a sprite's color and tone, held-key repeat timing and the
  volume curve follow common formulas, not measurements of the PC engine.
- A module a game includes into one of Ruby's own classes stays included for the next game in the same session,
  because Ruby can't remove it.
- A script that loops without calling `Graphics.update` can't be stopped; the app moves on after 5 seconds.
