# Godot engines

VNDegen runs Godot games on its own builds of Godot, made by `scripts/godot-engine.sh` into `build/godot/`. Godot 4 games run on Godot 4.7.2 and Godot 3 games on Godot 3.6.3. Nothing in this folder is a copy of Godot's code. The script fetches the sources below at pinned commits, applies the patches here, and builds them with VNDegen's module.

## Sources

| What | Repository | Commit |
|---|---|---|
| Godot 4.7.2-stable | `https://github.com/godotengine/godot` | `ed1daf0bf001b61586d9930840f2f1394092c079` |
| Godot 3.6.3-stable | `https://github.com/godotengine/godot` | `a3378686f7196db78d7b3cc7947f72a13bc4864c` |
| SCons 4.10.1, to build Godot | PyPI, into a venv in the build cache | |

Godot is MIT. Its third-party code is listed in its `COPYRIGHT.txt`, which the build copies to `build/godot/licenses/`. Where a component offers a choice of licenses, VNDegen uses the permissive one: BSD for the ODE-derived collision code and SDL's HIDAPI, and MPL-2.0 for graphite2. The build leaves out graphite2, SDL's input, OpenGL (except in the iOS Simulator), Vulkan, AccessKit, OpenXR, and the camera, WebXR, and mobile VR modules. glslang's Bison-generated parser is under GPL-3.0 with the Bison exception, which lets the generated file be used under any license.

Godot 3.6.3's `COPYRIGHT.txt` names no GPL or LGPL code, and the build copies it to `build/godot/licenses/` too. Its build leaves out Mono, UPnP, and the camera, WebXR, and mobile VR modules.

## Patches to Godot 4 (`4/patches/`)

- `01-older-exports.patch`: lets Godot 4.7 run games exported with Godot 4.2 to 4.6.
  - Binary GDScript (`.gdc`) from Godot 4.3 and 4.4 is tokenizer version 100, which 4.5 and later refuse. Version 100 has one more header field and no `PERIOD_PERIOD_PERIOD` token, so later token types are one lower. The tokenizer reads both versions.
  - The global class cache of games exported before 4.5 has no `is_abstract` or `is_tool` keys, and 4.7 skips such entries, so every `class_name` of those games went missing. Both keys now default to false.
  - A game exported as one Windows `.exe` keeps its pack in the executable's `pck` section. Signed executables have their signature after it, so Godot's search from the end of the file fails. The pack loader also reads the section table of the file it is given.
- `02-restart.patch`: starts Godot again in the same process after it stops, which iOS needs, since a game can't run in a process of its own there. Godot doesn't support this upstream yet. The patch is the open LibGodot pull requests below, ported to 4.7.2, plus VNDegen's own fixes for what remained:
  - `ObjectDB::cleanup` clears its slot table.
  - The navigation server managers and the scene and servers debuggers clear their singletons.
  - `ClassDB::cleanup` calls each class's deinitializer, where 4.7.2's code differs from the pull request's base.
  - `StringName`'s table keeps `StringName::configured`, which 4.7.2 has in place of the table's own flag.
- `03-simulator-metal.patch`: keeps the Metal driver in the iOS Simulator build, which Godot's build turns off, so the simulator build compiles the same code as the device's. The simulator runs Godot on OpenGL ES (patch 05).
- `04-no-swift-app.patch`: leaves out `drivers/apple_embedded/app.swift`, the SwiftUI app of Godot's own iOS template. VNDegen is the app.
- `05-simulator-angle.patch`: lets the iOS Simulator build take its OpenGL ES headers from MetalANGLE. The simulator's Metal device reports only GPU families Apple1 and Apple2, and Godot's Metal renderer needs Apple4: with the family check skipped, Metal's validation aborts on the first texture Godot creates. So in the simulator only, Godot 4 runs its compatibility renderer on OpenGL ES 3 through MetalANGLE, the OpenGL ES on Metal the app embeds for Ren'Py. Devices and the Mac use Metal.
- `06-simulator-blur.patch`: in the iOS Simulator only, builds the mipmaps of the 2D back buffer with `glGenerateMipmap` instead of Godot's Gaussian blur. The blur draws into one mip level of a texture while it samples the level above, and MetalANGLE aborts on that draw with a Metal pipeline that has no color format. Shaders that read a blurred screen look slightly different there.
- `07-restart-setup.patch`: clears the pointers `Main::cleanup` frees, at the start of `Main::setup`, since a setup that fails frees whatever it finds set.
- `08-copy-same-file.patch`: `DirAccess::copy` does nothing when the destination is the file on disk the source opened, compared by inode. VNDegen finds names whatever their case, so two spellings can name one file, and copying it onto itself would empty it. A file read from the pack never matches.

## Patches to Godot 3 (`3/patches/`)

- `01-angle-headers.patch`: lets the iOS builds take their OpenGL ES headers from MetalANGLE, which Godot 3 renders through on iPhone and iPad.
- `02-restart.patch`: starts Godot 3 again in the same process after it stops. Godot 3 has no LibGodot, so these are VNDegen's own fixes:
  - `ClassDB` counts its generations, and each class registers again in a new one.
  - `StringName` setup and cleanup run once, so names outlive the engine.
  - Singletons whose destructors left their pointer set now clear it: IP, InputMap, PackedData, ResourceFormatImporter, Performance, Input, ShaderTypes, PhysicsServerSW, VisualServerScene, NavigationMeshGenerator, ZipArchive, and JavaScript.
  - The audio, physics, and 2D physics server managers forget their drivers and servers at cleanup.
- `03-virtual-fix-path.patch`: makes `FileAccess::fix_path` and `DirAccess::fix_path` virtual, as they are in Godot 4, so VNDegen's file access can find names whatever their case.
- `04-restart-setup.patch`: clears the pointers `Main::cleanup` frees, at the start of `Main::setup`, as Godot 4's patch 07 does.
- `05-copy-same-file.patch`: the same check in `DirAccess::copy` as Godot 4's patch 08.

| Pull request | Title | Author | Commit ported |
|---|---|---|---|
| [#123552](https://github.com/godotengine/godot/pull/123552) | LibGodot: Reset cached global settings on reinitialization | kisg | `fa4567396c3a0442de800f1237340b099fca7483` |
| [#123720](https://github.com/godotengine/godot/pull/123720) | LibGodot: Support class and StringName reinitialization | kisg | `814e5c9f9928ee27566632589d456a61fdd918a0` |
| [#123999](https://github.com/godotengine/godot/pull/123999) | LibGodot: Reset singleton pointers on reinitialization | kisg | `7c229a5a1df841424aa784426226accace16a51d` |
| [#124000](https://github.com/godotengine/godot/pull/124000) | LibGodot: Reset global registries for reinitialization | kisg | `5811b8e94b86b7143ec506c92b8c2671a216c3d2` |
| [#124001](https://github.com/godotengine/godot/pull/124001) | LibGodot: Clean up GDScript state for reinitialization | kisg | `c0927f42a2059f0104e6b22a51a495d393dead86` |
| [#124003](https://github.com/godotengine/godot/pull/124003) | LibGodot: Clean up rendering state for reinitialization | kisg | `3a232266a79db94514c4fa9dcdc2ad596fa83797` |
| [#124004](https://github.com/godotengine/godot/pull/124004) | LibGodot: Clean up desktop platform instances | kisg | `4a50e88b067a727ab61aaa0e2f562b77e7db246a` |
| [#124005](https://github.com/godotengine/godot/pull/124005) | LibGodot: Fix EGL cleanup for reinitialization | kisg | `4fd5bb8e303e037889f73af36a8643b56647a92b` |
| [#124006](https://github.com/godotengine/godot/pull/124006) | LibGodot: Restore the working directory after shutdown | kisg | `81b69aafe202964532b125b800c81e12089435ca` |
| [#124011](https://github.com/godotengine/godot/pull/124011) | LibGodot: Initialize GDExtensionManager early | kisg | `a5ed6bba5d8239235fc34e58dd1dddf6bec154a4` |

## VNDegen's additions

- `4/vndegen/`: the Godot 4 module, built in with `custom_modules`.
  - The functions in `Packages/VNDegenKit/Sources/CGodot/include/vngd.h` (`vngd.cpp`).
  - `DisplayServerVNDegen`, which draws into the CAMetalLayer VNDegen hands it and takes all input through `vngd.h` (`display_server_vndegen.mm`).
  - The OS for each platform (`vngd_apple.mm`, and `vngd_linuxbsd.cpp` for testing on Linux). It gives games their `.exe` as the executable path and keeps `user://` where Windows does, under VNDegen's stand-in for `%APPDATA%`.
  - File and folder access that finds files outside the pack whatever case the game names them in, through `Engines/Shared/casepath.c` (`vngd_files.h`, also in the Godot 3 module).
- `3/vndegen/`: the Godot 3 module, with the same functions. Its OS draws with MetalANGLE on iPhone and iPad and with Apple's OpenGL on Mac, into two IOSurfaces a layer shows (`vngd_apple.mm`), and `vngd_x11.cpp` runs it on Linux for testing.
- `vngd_frames.h`: the frame timing both modules log for `VNGD_FRAME_LOG`.
- `test-host/vngd_test.c`: runs games through `vngd.h` on Linux, one after another in one process, to test that each engine starts again.
- `Info.plist`: the frameworks' Info.plist.
