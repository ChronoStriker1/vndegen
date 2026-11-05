# Unity feasibility study

Roadmap item 10. **Decided 2026-11-05:** VNDegen detects Unity games and says on their page why it can't run them (option 1, done). Option 2's half-day proof ran, with its results under [Option 2 proof](#option-2-proof). Options 3 and 4 aren't being done.

This study classifies the library's Unity games from their files, weighs each way of running them, and ends with options for the maintainer. No engine was built. It ran on Linux, reading metadata and assemblies only. It decoded no game art, ran no game code, and used no Unity runtime.

Each claim is marked **verified** (measured from game files or run here), **read** (from a named source), or **inferred** (my judgment from those).

## Summary

- **The library holds 123 Unity folders, not the 60 in the roadmap.** The 60 predates the 2026-10-31 additions. 118 were read: 95 Mono and 23 IL2CPP, 336 GB unpacked. 2 were left out under the project's leave-out rule, and 3 are installers that can't be read without running them.
- **On Mac, they run today through Rosetta and CrossOver.** The [option 2 proof](#option-2-proof) took four games to their titles and played two, with no per-game setup beyond a Windows font. VNDegen could hand a game to the player's own copy at small cost, shipping no Wine. Nothing about this route is native, and it depends on Rosetta, which Apple is cutting back after macOS 27.
- **On iPhone and iPad, the only real route is a reimplementation of Unity's engine running the game's .NET code in an interpreter.** It can't reach IL2CPP games. That's months of work for a first group of 18 to 46 games, with no project anywhere that has shown it working on a commercial game.
- **Unity's own players are out.** The terms grant distribution only to a game's own developer, inside that developer's project. They don't work technically either: the games' shaders are compiled for Direct3D only.
- **Recommendation: don't build a Unity engine now.** Add the small Mac launcher if having these games in one library is worth about a week. Run a time-boxed spike on the engine only if playing these games on iPad is worth months of work.

## What's in the library

Per-game results are in [unity-games.tsv](unity-games.tsv). The groups below are **verified** from the files, except that "kind" is a heuristic over object counts: skinned meshes, lights and 3D rigid bodies mean 3D unless sprites far outnumber meshes, and a framework's assemblies mean a visual novel (VN).

| | Mono | IL2CPP |
|---|---:|---:|
| Games read | 95 | 23 |
| Unpacked size | 281 GB | 55 GB |
| VN framework, 2D | 30 | 3 |
| Other 2D | 38 | 14 |
| 3D or VR | 27 | 6 |
| Built-in render pipeline | 64 | 4 |
| URP | 30 | 19 |
| HDRP | 1 | 0 |
| 32-bit x86 | 30 | 0 |
| 64-bit x64 | 65 | 23 |

The 6 games the old survey couldn't check, and the 2 SuperHot rows added since:

| Game | Result |
|---|---|
| Hollow Knight | Mono, 2020.2.2f1, x64, built-in pipeline |
| Hollow Knight: Silksong | Mono, 6000.0.50f1, x64, built-in pipeline |
| Cloud Meadow | Mono, 2017.4.2f2, x64, built-in pipeline |
| Bunny's Rent-a-Mom Agency | IL2CPP, 6000.0.58f2, x64, URP |
| SuperHot | Mono, 2019.2.9f1, x86, built-in pipeline |
| Actraiser Renaissance | Not read: repack installer. Unity from public knowledge only |
| HoneyCome come come party | Not read: repack installer. Unity from public knowledge only |
| SuperHot VR | Not read: its installer's data file is in an unknown format. Unity from public knowledge only |

### Versions

Versions don't fall into a few ranges. The 118 games use 80 different Unity versions, from 4.5.5 to 6000.4.6.

| Unity | Mono | IL2CPP |
|---|---:|---:|
| 4.x | 5 | 0 |
| 5.x | 7 | 0 |
| 2017 to 2018 | 9 | 0 |
| 2019 | 17 | 1 |
| 2020 | 5 | 1 |
| 2021 | 19 | 1 |
| 2022 | 15 | 7 |
| 6000 (Unity 6) | 18 | 13 |

New games lean toward Unity 6 and IL2CPP. 13 of the 23 IL2CPP games are Unity 6.

### Frameworks and middleware

| Component | Games | Why it matters |
|---|---:|---|
| TextMeshPro | 101 | Plain .NET code the game ships, so it would run as is on top of an engine |
| Steamworks | 63 | 47 Mono games call `steam_api` directly, so it needs a stub |
| Addressables | 43 | Assets in separate bundles |
| Spine | 32 | C# runtime in the game, rebuilding meshes each frame, so it's interpreter-heavy |
| Live2D | 30 | `Live2DCubismCore.dll` is native x86 in 29 of them, under Live2D's own license |
| Naninovel | 25 | The largest VN framework. 13 also use Live2D. Versions span 2019.1 to 6000.3 |
| PlayMaker | 9 | |
| Utage | 7 | |
| Rewired | 6 | Input middleware with native parts |
| Fungus | 5 | |

- **Native plugins.** 64 games ship `lib_burst_generated.dll`, native x86 compiled from their C#. Burst normally falls back to the C# version when it's absent (inferred, not tested). 59 games ship some other native plugin beyond Steam, Burst, GOG and SOVEREIGN, most often Live2D.
- **Shaders.** 21 games use only stock shaders. 33 add 1 to 5 of their own, 32 add 6 to 30, and 32 add more than 30.
- **SOVEREIGN.** 25 games carry `SOVEREIGN.dll` or `SOVEREIGN64.dll`. Its exports match `steam_api`, so it stands in for Steam, and 17 games also carry a `winmm.dll` proxy beside it. A route that stubs Steam ignores both.

### Graphics APIs

**Verified:** Windows builds carry Direct3D shaders and almost nothing else. In all 109 games whose shaders record their platforms, at most 6 of a median 109 shaders have Metal, GL or Vulkan versions, and 7 games have none. In the five sampled by name, those are Unity's own fallback shaders: `Hidden/InternalErrorShader`, `Hidden/InternalClear`, `Hidden/Internal-Colored`, `GUI/Text Shader`, `Hidden/FrameDebuggerRenderTargetDisplay`, and, on newer versions, `Hidden/Internal-Loading`. No game's own shaders can run on Metal without translation. 4.x and older 5.x files don't record this field.

Textures in the sampled games are RGBA32, DXT1 and DXT5. Apple silicon Macs and M-series iPads can read DXT. Other iPhones and iPads would need decoding or transcoding at import (inferred from Metal's feature tables, not tested).

## What games call

`scripts/unity/scan_game.py` reads each Mono game's assembly references. "Members" are distinct fields and methods of `UnityEngine` types backed by native code. "With packages" adds calls from package code the game ships, such as `UnityEngine.UI` and TextMeshPro. That code would run as is, but it needs its engine calls answered too. **Verified:**

| Group | Games | Median, game code | Median, with packages | Union, with packages |
|---|---:|---:|---:|---:|
| All Mono | 95 | 818 | 1,766 | 8,271 |
| 2D or VN | 68 | 774 | 1,728 | 6,597 |
| And built-in pipeline | 46 | 680 | 1,616 | 5,783 |
| And no Live2D | 27 | 645 | 1,620 | 5,596 |
| And no other plugin, at most 5 custom shaders | 18 | 621 | 1,471 | 4,835 |

The surface doesn't converge. Only 554 members are used by 90% of Mono games, and 1,480 are used by just one game. Within the easiest 18, 1,275 members are used by only one of them, so each new game keeps asking for new API.

The list understates the work, because it doesn't show behavior under the API. Shipped engine assemblies declare a median of 8,220 native internal calls, and IL2CPP builds name 1,329 to 3,945. Of the 68 Mono 2D or VN games, in their own code: 63 use Animator, 63 build meshes, 55 render to textures, 55 use 2D physics, 55 use 3D physics APIs, 44 use command buffers, 36 play video, and 54 use the old IMGUI system.

Beyond Unity's API, 45 of the 95 reference the Windows registry API, 55 reference `Process`, and 17 P/Invoke into Windows DLLs such as `user32` and `kernel32`. Some of those are in code paths a game may never run. 9 reference `Reflection.Emit`, which Mono's AOT-only mode doesn't offer (read). I didn't check it under the interpreter.

## Routes

### A reimplementation of UnityEngine, running the game's .NET code (Mono games only)

It's technically possible for the smallest games, and it's the only route that could reach iPad. The whole cost sits in the engine.

**.NET on iOS** (no JIT, per project rules):

- The Mono interpreter runs IL with no JIT and is MIT licensed. Microsoft documents it for iOS release builds through .NET 10 (read). Loading a game's assembly that wasn't present at build time should work through it, but neither Microsoft's pages nor this study confirm that on iOS (inferred). Half a day on a simulator would settle it.
- .NET 11 replaces Mono with a CoreCLR interpreter as MAUI's iOS default. Microsoft's post says Mono stays selectable "through .NET 11 servicing" and that the CoreCLR move doesn't include embedding APIs (read). So Mono, the runtime with a known embedding API, has an end in sight, and CoreCLR has no documented way to host it from a native iOS app that I could find (inferred). .NET 11 was still at RC 3 on 2026-11-05 (verified from the installer channel).
- Native AOT can't load code it didn't compile (inferred from Microsoft's statement that it has no JIT or interpreter). Ahead-of-time compiling at import doesn't work on iOS, since generated native code would have to be signed at install (inferred). A Mac could do it, or simply use a JIT.
- Apple's guideline 2.5.2 bars executing code that "introduces or changes features or functionality of the app". How that applies to a user's imported game isn't settled (read). VNDegen's other engines already interpret game code, so this adds no new kind of exposure for sideloading (inferred). It would matter for an App Store release, as it does for those engines.

**Measured here** (`scripts/unity/Bench.cs`, x64 Linux, load about 9 on 32 cores, 2 runs agreeing within about 6%). Mono 8 is the newest Mono runtime pack NuGet offered for linux-x64. Milliseconds, scaled to the same work:

| Workload | CoreCLR 11 JIT | Mono 8 JIT | Mono 8 interpreter | Interpreter vs Mono JIT |
|---|---:|---:|---:|---:|
| Vertex loop, like Spine or TextMeshPro | 190 | 2,450 | about 7,200 | 2.9x |
| 2,000 components updating | 39 | 275 | about 855 | 3.1x |
| Script parsing and strings | 175 | 640 | about 2,800 | 4.4x |

Unity's Windows player is its own fork of Mono with a JIT, so Mono JIT is the nearer baseline (inferred). So interpreted code should run about 3 to 4 times slower than on the PC player, which is workable for VN scenes and doubtful for heavy 2D action. The ratio on an iPad wasn't measured. AOT-compiling the runtime libraries ahead of time should improve the string-heavy case (inferred). The linux-x64 CoreCLR pack has no interpreter library, and a run asking for it stayed on the JIT, so it wasn't measured.

**Binding proof** (`scripts/unity/LoadProbe.cs`, stock .NET 10, no network). It loaded six games from Unity 4.5 to 6000.3 and JIT-prepared every non-generic method, calling none of them. .NET's own base libraries replaced the games' copies.

| Game | Unity | Methods prepared | Failed |
|---|---|---:|---:|
| Demon Angel SAKURA vol.1 | 4.5.5f1 | 2,861 | 46 |
| Demon Wants Have Fun! | 2018.3.0f2 | 4,967 | 32 |
| The Rural Homecoming | 2019.1.0f2 | 8,706 | 28 |
| Girls! Girls! Girls! | 2021.3.16f1 | 43,637 | 1,131 |
| Bounce 'n' Spurt | 6000.0.48f1 | 33,383 | 120 |
| Bunny Hero Works Part-Time | 6000.3.10f1 | 48,575 | 239 |

Old assemblies bind to modern .NET. Failures are under 3% everywhere, mostly missing native libraries (`steam_api64`, `Live2DCubismCore`), "Operation is not supported on this platform", which I take to be native engine calls (inferred), and a few type-load errors in Unity's 2D Animation packages. So the .NET runtime isn't the obstacle. This shows binding, not behavior.

**What a shim would need that isn't API** (inferred, from the object counts and the API list above):

- A serialized-file and bundle reader across 80 versions. The 5 sampled games from 2018 on have no type trees, though the 4.5 game does (verified), so layouts must be derived from the game's assemblies. UnityPy and AssetsTools.NET (both MIT) show this is solvable.
- The scene graph and component lifecycle, and native parts of uGUI such as canvas batching.
- Shaders. Stock shaders can be rewritten by name, but custom ones need a DXBC translator. DXVK's DXBC compiler (zlib) through SPIRV-Cross (Apache-2.0) is a permissive path, untested for this. 64 games add more than 5 shaders of their own.
- Mecanim animation with its compressed clips, 2D physics (Box2D, MIT), 3D physics (PhysX is BSD-3, Jolt is MIT), audio containers, video, and fonts.
- URP on top of all that, if a game uses it: its C# drives a scriptable render API that would need implementing too.
- Live2D, for 30 games. Its core is native and proprietary, and the game's copy is x86 Windows. I didn't check Live2D's terms.

**Legal position** (read, not legal advice):

- Unity's C# source is published under a reference-only license, so the project shouldn't read it or copy from it.
- The Terms of Service bar decompiling the software, except for a licensee inspecting CLI assemblies for its own projects. VNDegen's workers should never accept those terms, and never read method bodies from Unity's assemblies. This study read only metadata from the copies games ship: names, and which methods are flagged as internal calls.
- Public documentation and the list of names a game calls are the clean sources, as in the project's other clean-room runtimes.
- A reimplementation of an API is a known legal position, but not a free pass. A neutral name avoids a trademark problem.

### Unity's own players with the game's data moved onto them

Not allowed, and it wouldn't work. No Unity runtime was downloaded for this study.

- **Distribution** (Editor Software Terms, 2026-06-30, section 2.2): "you may distribute the Unity Runtime as an integrated part of your Projects, solely as embedded or incorporated into your Projects". VNDegen has no such project.
- **Third parties** (section 2): the grants are "primarily for creators creating first-party content", and others "may need a separate grant of rights".
- **Porting** (section 2.1 and the definition of User Modifications): modifications include "conversion, transpilation, porting, or wrapping", and preparing a project for a platform that isn't supported "requires a separate grant of rights from Unity". Terms of Service section 17.2(a) bars porting any offering, and section 17.2(cc) bars letting end users build one into their own products.
- **Having the player download it instead** doesn't help: that player still comes from Unity under the same terms, and the games would need 80 player versions.
- **Technically** (verified): the games' shaders are Direct3D only, so a Mac or iOS player would have nothing to draw with except its error shader.

### Translating x86

**Mac: real, and available now.** On Apple silicon, Wine runs as an x86-64 program under Rosetta 2 and handles 32-bit code, which covers the 30 x86 games (read, from WineHQ's notes and press reports). D3D11 reaches Metal through DXVK and MoltenVK or Apple's D3DMetal (public knowledge, not checked here). Rosetta 2 stays fully available through macOS 27, then a reduced version remains for older games (read, reports of Apple's documentation, as Apple's page wouldn't load here). Whether that subset will cover Wine is unknown. This route covers Mono and IL2CPP alike. No game was tested for the study itself, because the Macs were off limits then. The [option 2 proof](#option-2-proof) later ran four, through D3DMetal and, for a 32-bit game, wined3d on MoltenVK.

Wine is LGPL (public knowledge, as its license page refused automated fetches), which the roadmap's License rules don't allow in shipped new engines. Launching the player's own CrossOver or Wine avoids shipping it. Apple's Game Porting Toolkit is free to download, but I couldn't confirm its redistribution terms, so VNDegen shouldn't ship that either.

**iPhone and iPad: ruled out.** Box64 and FEX (both MIT) are JIT translators for Linux. Without a JIT only an interpreter remains, which is far too slow for real-time games, and Windows would still need a Wine port to iOS, which has neither `fork` nor free control of memory mapping (inferred). Translating ahead of time at import hits the same signing wall as AOT. JIT tricks for sideloaded apps conflict with project rules, and iOS 26 reportedly broke them for now (read, a forum report).

**IL2CPP on iPad therefore stays out of reach.** Those 23 games' code exists only as 38 to 165 MB of compiled x86-64 in `GameAssembly.dll`. Il2CppDumper (MIT) recovers names and signatures, not working code. Cpp2IL (MIT) goes further but calls itself work in progress, and I didn't test it. HybridCLR (MIT) adds an interpreter to IL2CPP, but only inside a Unity build its developer makes.

### Decompiling or rebuilding each game

Possible for one game at a time, and not suitable for VNDegen. AssetRipper (GPL-3.0, as a tool only) rebuilds a Unity project from a game, ILSpy (MIT) decompiles its C#, and the Unity Editor could then build for Mac or iOS.

- It needs the Unity Editor and acceptance of its terms, and the result isn't a project you own. The June 2026 terms name automated "conversion, transpilation, porting or repackaging" services when a platform is involved.
- AssetRipper's own disclaimer warns that using its output may break copyright law. Game EULAs often bar it too.
- It's manual and fragile per game (inferred), the result can't be shared, and it doesn't work for IL2CPP without far more work.

So this is a private choice the maintainer could make for a favorite game, not a feature.

### A data player for one framework (Naninovel)

Like the NekoSDK runtime, this would read a framework's data and redraw it with no Unity API, for up to 25 games. The evidence is against it (inferred):

- UI is built from uGUI prefabs, so most of the shim's UI work returns anyway.
- Games add their own C# commands, which a data player can't run. In a sample of three, one game adds 11 beyond Naninovel's built-in 49, one adds none, and one compiles Naninovel into its own assembly.
- 13 of the 25 need Live2D, and 3 are IL2CPP. Its games span 2019 to Unity 6, so its data likely changes across them, and it's a commercial package whose source goes only to licensees. I found no public specification of its compiled scripts.
- Work on a shim would run the same games, including their custom commands, and also serve other games. This route's work wouldn't carry over.

## Existing projects

Licenses and activity are from GitHub on 2026-11-05. For code, I read only the two closest projects' READMEs and one post-mortem, and no GPL code.

| Project | License | Last push | What it has actually run or does |
|---|---|---|---|
| [OpenUnity](https://github.com/nanashili/OpenUnity) | None stated, so not usable | 2026-10-14 | Started 2026-10-12. Clean-room in Rust. Its own README: "early", no general project import, and its scene route renders authored test scenes. No shipped game |
| [unirecomp-tool](https://github.com/BluetufieBugado/unirecomp-tool) | MIT | 2026-10-19 | The same idea on Android, archived. Ran one tiny game's menu (55 scripts), then stopped on a commercial one. Its author: "You can read a Unity game; you cannot reimplement Unity". Its reading tools are reusable |
| [UnityPy](https://github.com/K0lb3/UnityPy) | MIT | 2026-10-05 | Reads Unity assets. Used for this study |
| [AssetsTools.NET](https://github.com/nesrak1/AssetsTools.NET) | MIT | 2026-09-10 | Reads and writes Unity assets from .NET |
| [AssetStudio](https://github.com/Perfare/AssetStudio) | MIT | 2022, archived | Asset explorer |
| [AssetRipper](https://github.com/AssetRipper/AssetRipper) | GPL-3.0 | 2026-11-04 | Rebuilds projects from games. Not read, not to be shipped |
| [Il2CppDumper](https://github.com/Perfare/Il2CppDumper) | MIT | 2024-08 | Dumps IL2CPP metadata |
| [Cpp2IL](https://github.com/SamboyCoding/Cpp2IL) | MIT | 2026-11-05 | Work-in-progress IL2CPP reversal |
| [Il2CppInspector](https://github.com/djkaty/Il2CppInspector) | AGPL-3.0 | 2022-05 | IL2CPP analysis. Not read |
| [HybridCLR](https://github.com/focus-creative-games/hybridclr) | MIT | 2026-10-18 | IL2CPP plus an interpreter, inside Unity builds |
| [BepInEx](https://github.com/BepInEx/BepInEx) | LGPL-2.1 | 2026-10-23 | Mod loader inside the real player. Not read |
| [MelonLoader](https://github.com/LavaGang/MelonLoader) | Apache-2.0 | 2026-11-04 | Mod loader inside the real player |
| [dotnet/runtime](https://github.com/dotnet/runtime) | MIT | 2026-11-05 | .NET, with Mono and the CoreCLR interpreter |
| [ILSpy](https://github.com/icsharpcode/ILSpy) | MIT | 2026-11-05 | .NET decompiler |
| [FNA](https://github.com/FNA-XNA/FNA) | MS-PL by public knowledge, GitHub detects none | 2026-11-01 | XNA reimplementation: proof that this kind of project works, for a far smaller and fixed API |
| [Prowl](https://github.com/ProwlEngine/Prowl), [Stride](https://github.com/stride3d/stride) | MIT | 2026-11-05 | Unity-like engines. They don't run Unity games |
| [Box64](https://github.com/ptitSeb/box64), [FEX](https://github.com/FEX-Emu/FEX) | MIT | 2026-11 | x86 on Arm Linux, by JIT |
| [DXVK](https://github.com/doitsujin/dxvk) | Zlib | 2026-11-05 | D3D to Vulkan, with a DXBC compiler |
| [MoltenVK](https://github.com/KhronosGroup/MoltenVK), [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross) | Apache-2.0 | 2026-10 | Vulkan on Metal, and SPIR-V to Metal shaders |
| [Whisky](https://github.com/Whisky-App/Whisky) | GPL-3.0 | 2025-05, archived | Wine wrapper for Mac. Not read |
| [Sikarugir](https://github.com/Sikarugir-App/Sikarugir) | None detected | 2026-11-05 | Wine wrappers for Mac. Not read |
| [UnityCsReference](https://github.com/Unity-Technologies/UnityCsReference) | Unity Reference Only | 2026-11-03 | Unity's C# source. The project shouldn't read it |

I found no project that runs commercial Unity games without Unity's player, under any license.

## How VNDegen would hold it

Option 1 is built: the Unity plugin (`EngineUnity`) recognizes these games as below, names the Unity version and whether the game is Mono or IL2CPP, and runs on no device, so Play explains itself instead of starting. [engines.md](engines.md#unity) has the rules.

- **Plugin interface.** `EnginePlugin` fits every option without a VNCore change. `detect` can match `UnityPlayer.dll`, `*_Data/globalgamemanagers`, `mainData` or `data.unity3d`, and `compatibilityNote` can tell the player which route applies. A Mac launcher would need its session to start an outside app and wait for it, which the Wine path needs but no engine does today (inferred). I don't expect that to require VNCore to change.
- **Saves.** 89 of 95 Mono games use `persistentDataPath`, and 77 use PlayerPrefs (verified). On Windows those are `AppData/LocalLow/<company>/<product>` and `HKCU\Software\<company>\<product>` (read, for PlayerPrefs). Company and product names are in `<game>_Data/app.info` (inferred, from Unity's layout).
  - Files map through `saveLocations` directly. For the Mac route they sit inside the Wine prefix, which belongs to the player, so VNDegen would read them and never move the prefix.
  - **Ludusavi's registry data is only carried along.** Since 0.24.1 a backup VNDegen writes keeps the registry data a Windows PC's backup holds, but VNDegen never applies it on a Mac, iPhone, or iPad (verified, `LudusaviStore.swift` and the 0.24.1 notes). So a game's PlayerPrefs, often its settings and sometimes its progress, wouldn't move between devices until VNDegen reads and writes Ludusavi's registry data. A native engine would convert those values to and from its own store, and the Mac route would write them into the bottle's registry.
- **Controls.** 93 of 95 games use the legacy `Input` API, which a shim could feed from `EngineSession`'s existing keyboard, mouse and controller mapping, with touch as the mouse (inferred). Games using Rewired (6) or `InControl` need more. On the Mac route, input goes straight to Wine, so VNDegen's mappings and overlay don't apply.
- **Remove engine files.** With a shim, `UnityPlayer.dll`, `MonoBleedingEdge`, `baselib.dll` and the crash handler would go: 25 to 112 MB in 12 sampled games (verified), a small part of each game. On the Mac route, nothing can go, since the Windows player is what runs.
- **File name case.** A shim's file calls go through its own `System.IO` layer, which would need `casepath` like the other engines (inferred).
- **Shipped Mac builds.** SiNiSistar 2's archive includes a universal x86-64 and arm64 Mac app (verified), and it's the only library game the survey flags that way. Launching one is a cheap win. Hollow Knight, Silksong and SuperHot are sold with Mac versions (public knowledge, not checked), but the library holds their Windows copies.

## Options

Sizes are my estimates in agent-thread time, assuming work like the project's other engines. They're rough, especially options 3 and 4, where the unknown is how far behavior differs from Unity.

| | 1. Say no clearly | 2. Mac launcher | 3. Engine spike | 4. Mono 2D and VN engine |
|---|---|---|---|---|
| **What** | Detect Unity and explain it on the game page | Option 1, plus launch a shipped Mac build or hand off to the player's CrossOver or Wine | One easy game to its title, then playing, on Mac and then an iPad simulator | A real engine with its own UnityEngine, on Mono's interpreter |
| **Size** | 1 day | About 1 week | 2 to 3 weeks, time-boxed | 3 to 6 months for the 18, 6 to 12 or more for up to 46 to 68, then upkeep |
| **Mac** | None | In principle all 118 read. 4 reached their titles in the proof | 1 game | 18 to 68 games |
| **iPhone, iPad** | None | None | 1 game | 18 to 68 games |
| **IL2CPP** | None | Mac only | None | None |
| **3D games** | None | Mac only | None | None |
| **Lost** | Nothing | Native running, overlay, VNDegen's mappings. PlayerPrefs sync until the registry gap is closed | The time | Fidelity: some games will misbehave in ways hard to diagnose |
| **Main risk** | None | Rosetta after macOS 27. Depends on software VNDegen doesn't control | It succeeds on one game and says little about the rest | Scope that never ends, as unirecomp-tool found. Mono's future. Unity 6's share keeps rising, and IL2CPP with it |

### Option 2 proof

Run 2026-11-05 on a MacBook Pro with an M5 Max, macOS 27.0.1, in CrossOver Preview 27.0.0 (build 20261006). Everything ran in a new `win10_64` bottle kept in a scratch folder through `CX_BOTTLE_PATH`, with its user folders made real folders inside the bottle and its home drive removed, and was deleted afterwards. A `sandbox-exec` profile blocked every IP connection for Wine and the game (checked with `curl` and `nc` under the same profile). Input was real key presses and clicks, each sent only while the game was the frontmost app. Frame rates are what Metal's performance HUD showed at that moment, not averages, on a 120 Hz display.

| Game | Build | Graphics path | Title | Input | Played | Frame rate |
|---|---|---|---|---|---|---|
| Super Mamono Sisters | Mono, x86, Unity 2019.1.7f1 | wined3d on Vulkan through MoltenVK | Yes | Keyboard works | About 50 s of the first stage: moving and attacking | 59 in play, 39 at the tutorial's start |
| Demon Wants Have Fun! | Mono, x64, Unity 2018.3.0f2 | D3DMetal (Game Porting Toolkit 4.0b2) | Yes, after adding Arial | The pointer follows the mouse. Clicks didn't register, see below | No | 119 to 120 |
| Hospital Seduction | IL2CPP, x64, Unity 2022.3.25f1 | D3DMetal | Yes | Keyboard works in menus and story | About 45 s from New Game into the story | 117 to 120 |
| Succubus Showdown | IL2CPP, x64, Unity 6000.3.5f2 | D3DMetal | Yes | Not tried | No | 83 |

Every game ran under Rosetta, which the HUD shows as x86_64. CrossOver picked the graphics path itself: D3DMetal for the 64-bit games, and for the 32-bit one, wined3d's Vulkan renderer, as its log says.

What broke or needs work:

- **Windows fonts.** Demon Wants Have Fun! drew its menu with no text, because it uses Unity's built-in Arial, which the player reads from `C:\Windows\Fonts`, and a new bottle has none there. A copy of the Mac's own Arial in the bottle's Fonts folder brought back "Start game". A launcher would need the bottle to have Windows' core fonts, such as from CrossOver's own installer for them.
- **Clicks.** Synthetic clicks moved the game's pointer but didn't press anything, in both games tried, while keys worked. Another app's system dialog stayed on screen through those tries, which may explain it, so mouse input is unproven rather than broken.
- **Window and picture.** Succubus Showdown opened full screen and ignored Unity's window-size arguments. Hospital Seduction's story scene filled only the top left of its window, with black at the right and bottom, which wasn't compared with Windows.
- **Video.** Hospital Seduction's log says Media Foundation couldn't use the D3D11 device for video and fell back to software decoding. Whether its clip played wasn't seen.
- **Starting it.** Wine's Mac driver needs the login session's window server, so Wine started over SSH failed to make a window, and Wine started from an app through LaunchServices worked. VNDegen is such an app, and it isn't sandboxed, so it can start CrossOver's command-line `wine` with a bottle as it starts Ren'Py today (inferred).

**Is option 2 worth building?** Yes, if having these games in one library on Mac is worth it: all four reached their titles in an untouched CrossOver bottle, with fonts the one fix that every Unity game using Arial will need. It stays a hand-off. The game runs in its own Wine window, and VNDegen's overlay, controller mapping and save sync don't reach it, while Rosetta's future after macOS 27 decides how long it lasts. Size: about a week, as estimated, made of finding CrossOver, one bottle VNDegen makes for its games with core fonts installed, starting a game and noticing that it quit, showing the shipped Mac build instead where there is one, and pointing save backups at the bottle's `AppData/LocalLow`. Free Wine builds without D3DMetal weren't tried, so a player without CrossOver may get less.

What's left out of every option: Unity's own players (terms, shaders), x86 on iOS (no JIT), rebuilding games (legal and fragile), and a Naninovel-only player (it duplicates the engine and doesn't carry over).

Option 4 has one advantage unirecomp-tool lacked: the real game runs under Wine on the Linux machine, so its behavior can be compared directly. That helps, and doesn't change the scale.

### Recommendation

**Not worth building an engine for these games now.** It's the largest engine family in the library, but it's also the worst fit for how VNDegen has grown: every shipped engine has a runtime or a format small enough to finish, and this one has neither. The 23 IL2CPP games and the 33 3D games wouldn't come along, and the newest games are moving away from it.

1. **Do option 1 now**, whichever else is chosen, so a game's page says plainly why it can't run.
2. **Do option 2 if the maintainer wants these games in one library on Mac.** It's the only route that reaches most of the library at a small cost. First, spend half a day on three games on a Mac: one x86 Mono, one x64 Mono and one IL2CPP, in CrossOver. That's the bigger proof worth doing, since this study couldn't touch a Mac. (Done: see [Option 2 proof](#option-2-proof).)
3. **Run option 3 only if iPad play of the 2D and VN Mono games is worth months.** Start with half a day on a simulator, showing the Mono interpreter loading an assembly that wasn't in the app. If that fails, stop there. Otherwise take one of the 18 easiest games, such as The Rural Homecoming (Naninovel) or Super Mamono Sisters, with a firm stop at three weeks. Go on to option 4 only if the second game costs much less than the first.

The decision for the maintainer: is iPad play of these games worth months, and is a Mac hand-off, not native, acceptable?

## Method

`scripts/unity/scan_game.py` describes one unpacked game as JSON, and `summarize.py` builds [unity-games.tsv](unity-games.tsv) and the numbers here. Both need [dnfile](https://github.com/malwarefrank/dnfile) (MIT) and UnityPy (MIT), and neither ships in the app. `LoadProbe.cs` and `Bench.cs` are the two proofs.

```sh
uv run --with dnfile --with UnityPy python -I scripts/unity/scan_game.py GAME_DIR > game.json
```

Limits on these numbers:

- Member counts come from metadata, so they include calls in code a game never reaches, and miss calls made by reflection or by name.
- IL2CPP games have no member counts, only the internal calls their builds name.
- Object counts skip serialized files over 1 GB (4 games) and stop after 600 files (10 games). Addressables games may show too few objects, so a 3D game could look 2D. Silksong is an example: only 14 sprites were counted, so its "2D" here rests on very little, even if it's right.
- "Kind" is a heuristic, not a look at each game. A framework's name found in code means the framework is there, not that the whole game uses it.
- Sample checks (shader names, texture formats, type trees) cover 6 games, not all 118.
- An out-of-memory kill stopped one unpacking mid-scan. The five archives in progress then were all scanned again from scratch, so no game was skipped because of it.

Sources for the terms and platform claims: [Unity Editor Software Terms](https://unity.com/legal/editor-terms-of-service/software) and [Unity Terms of Service](https://unity.com/legal/terms-of-service) (both 2026-06-30; the Terms of Service was read word for word only through section 24.3), [Apple App Review Guidelines](https://developer.apple.com/app-store/review/guidelines/), [.NET interpreter for iOS](https://learn.microsoft.com/dotnet/maui/macios/interpreter), [MAUI moving to CoreCLR](https://devblogs.microsoft.com/dotnet/dotnet-maui-moves-to-coreclr-in-dotnet-11/), [Rosetta's phase-out](https://www.macrumors.com/2025/06/10/apple-to-phase-out-rosetta-2/), [Wine on macOS](https://forge.fsky.io/brunodev85/wine/wiki/MacOS), [Unity PlayerPrefs](https://docs.unity3d.com:443/ScriptReference/PlayerPrefs.html), [Naninovel FAQ](https://naninovel.com/faq), and [iOS 26 and JIT](https://forums.libretro.com/t/what-is-the-current-situation-with-ra-on-ios-26-and-jit-support/49244).
