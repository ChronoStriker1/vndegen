# Godot

VNDegen runs Godot games on Godot itself, built into the app as two frameworks: Godot 4.7.2 runs every Godot 4 game and Godot 3.6.3 every Godot 3 game. `scripts/godot-engine.sh` builds both, and `Engines/Godot/UPSTREAM.md` lists the pinned sources and every patch.

## Which Godot runs a game

The game's pack says which Godot exported it. The pack is a `.pck` file beside the game's `.exe`, or sits inside the `.exe`: in its `pck` section for Godot 4, or appended to it. `EngineGodot` reads the header and picks the framework by the major version.

One runtime per major version covers the older minor versions:

- Godot 3.3 to 3.6 write the same compiled GDScript, so Godot 3.6 reads them all.
- Godot 4.7 reads the packs and scenes of earlier Godot 4 versions. VNDegen's patches cover what it doesn't read on its own: compiled GDScript from 4.3 and 4.4, the class cache of games exported before 4.5, and a pack inside a signed `.exe`.

Games exported with Godot 3.3 and 3.5 and with Godot 4.2, 4.3, and 4.5 were tested. A game can still behave differently from its PC version where a newer Godot changed a behavior, since VNDegen runs it on the newest Godot of its line.

## Rendering

Every game draws inside VNDegen's window, so the VNDegen menu and touch controls sit over it as over the other engines.

- Godot 4 renders on Metal, on Mac, iPhone, and iPad. VNDegen's Godot 4 has no OpenGL, so games made for the compatibility renderer run on the mobile renderer on Metal. Their 2D games look the same. Shaders written for OpenGL can look different.
- In the iOS Simulator, Godot 4 runs its compatibility renderer instead, on OpenGL ES 3 through MetalANGLE. The simulator's Metal device reports only GPU families Apple1 and Apple2, and Godot's Metal renderer needs Apple4. A few of the compatibility renderer's shaders fail to link on ANGLE there, and the games still draw. Devices and the Mac don't run that renderer.
- Godot 3 renders on OpenGL: Apple's OpenGL on Mac, into IOSurfaces a layer shows, and OpenGL ES through MetalANGLE, ANGLE's Metal backend, on iPhone and iPad.

Both frameworks are arm64 only, so an Intel Mac lists Godot games but can't run them.

## Input

VNDegen sends all input itself, through `vngd.h`. Godot reads no controller of its own.

- Controllers reach a game as one Xbox controller, in Godot's own button and axis numbers. The triggers are axes, and in Godot 3 also the buttons L2 and R2, as Godot's XInput driver reports them.
- Keys are sent by their USB HID usage, as UIKit reports them and VNDegen maps macOS keys to them.
- Touches work as the mouse, so a tap clicks where it lands. The touch mouse and a Mac's mouse are Godot's mouse.
- VNDegen's on-screen keyboard types into a game's text field, and the game sees the text as typed keys. A text field taking focus doesn't open the keyboard by itself: the player opens it with a button mapped to Show keyboard.

## Saves

Godot keeps a game's saves in `user://`. On Windows that is `%APPDATA%\Godot\app_userdata\<project name>`, or `%APPDATA%\<folder>` for a project that names its own folder. VNDegen keeps the same layout under `AppData/Roaming` in the game's data folder, taking the project name and folder from the pack's `project.binary`, and reports it to Ludusavi. Backups from Windows, a Steam Deck's Proton, and Godot's Linux and Mac builds map into it.

The game sees its `.exe` in its own folder as its executable path, so games that save beside the `.exe` save in their own folder, as on PC.

## Starting again

On iPhone and iPad every game runs inside VNDegen's process, so Godot has to start again in that process after a game quits. Godot doesn't support that upstream yet. The Godot 4 framework carries the open LibGodot pull requests that add it, ported to 4.7.2, and both frameworks carry VNDegen's fixes for the rest: classes register again, string names outlive the engine, and singletons and registries that would point at freed objects are cleared. `Engines/Godot/test-host/vngd_test.c` runs games one after another in one process on Linux, and the restart has been checked there under AddressSanitizer.

## Games VNDegen can't run

The game page says why:

- Games whose files are encrypted with a key built into their own copy of Godot.
- Games that need GDExtension or GDNative libraries, which are Windows code.
- Games whose own copy of Godot has Steam built in, read from the `.exe`'s imports. Such games call Steam from their scripts, and some quit unless Steam says the player owns them.
- Godot C# games, which need .NET.

## Testing

- `VNGD_FRAME_LOG=<file>` logs each second's frame count and slowest frame, and a summary every ten seconds and when the game stops: its average frame rate and 1% low against its target, the game's own cap or the display's refresh rate, whichever is lower.
- `VNGD_SNAPSHOT=<file>` writes the frame the game drew every two seconds, for the Mac, where a window snapshot can't see Metal.
- `VNGD_TRACE_INPUT=1` logs every input event VNDegen sends.
- In DEBUG builds, the launch arguments `-godotFrameLog`, `-godotSnapshot`, and `-godotTraceInput` set them, since a simulator's app can't be given environment variables. Relative paths go in the game's log folder.

## Known limits

- A text field taking focus doesn't open VNDegen's on-screen keyboard, as described above.
- A game's own window size and fullscreen settings change nothing, because the game fills VNDegen's view.
- Games built for the compatibility renderer run on the mobile renderer on Metal, as described above.
