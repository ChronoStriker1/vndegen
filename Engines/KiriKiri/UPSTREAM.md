# KiriKiri engine upstream

VNDegen runs KiriKiri games on krkrsdl3, an SDL3 port of the Kirikiri 2 and Kirikiri Z engine derived from Kirikiroid2.

## Sources

| Folder | Upstream | Commit |
|---|---|---|
| `krkrsdl3/` | https://github.com/krkrsdl3/krkrsdl3 | `dd1de3d2fd6a10c4f5ef8801e6649f520fc5bc3e` |
| `CMakeLists.txt`, `vcpkg.json`, `triplets/` | Adapted from https://github.com/krkrsdl3/krkrsdl3_build (`CMakeLists.txt`, `cmake/*.txt`, `vcpkg.json`, `CMakePresets.json`, `vcpkg/triplets`) | `6059f7857bf6c56bad33a781ec0fc2cc5151b86a` |
| `DroidSansFallback.ttf`, downloaded at build time | `Res/` in krkrsdl3_build, from the Android Open Source Project | Same |
| `ports/ffmpeg/` | `ports/ffmpeg` in https://github.com/microsoft/vcpkg, the builtin baseline of `vcpkg.json` | `8e8dfb4ba483886936ded5ca201b500b8d8b0096` |

The first commit that adds `krkrsdl3/` holds the upstream files unmodified. Every later change to them is listed below, so `git log -p -- Engines/KiriKiri/krkrsdl3` shows each one.

## License

krkrsdl3 carries the Kirikiri license in `krkrsdl3/LICENSE`: a BSD-style license with a krkrsdl3 clause. A binary distribution must include the copyright notice, the conditions, and the disclaimer in its documentation, so VNDegen's app must show the text of `krkrsdl3/LICENSE`. The krkrsdl3 clause requires anyone who distributes ports of commercial games that run on modified krkrsdl3 to publish the modified source under the same terms. This folder, in a public repository, does that.

Bundled third-party code keeps its own license: SQLite (public domain), the LZMA SDK in `plugins/Kirikiroid2/7zip` (public domain), and minizip (zlib license). The Bison-generated parsers in `tjs2/*.tab.*` carry Bison's exception, which lets them be distributed under the engine's license. DroidSansFallback.ttf is under the Apache License 2.0. Dependencies from vcpkg keep theirs: SDL3, SDL3_ttf, zlib, and libpng (zlib-style), FreeType (FreeType License), libjpeg-turbo (BSD-style and IJG), libwebp, libogg, libvorbis, Opus, and opusfile (BSD), oniguruma (BSD), plutovg (MIT), and glm (MIT).

## FFmpeg

VNDegen's movie decoder, `vndegen/movie_decoder.cpp`, uses FFmpeg 8.1.2. vcpkg builds it from the overlay port in `ports/ffmpeg/`, whose first commit is vcpkg's port unmodified. It has two changes. The x86_64 macOS slice builds without FFmpeg's x86 assembly (`--disable-x86asm`), so the build Mac needs no nasm, and the configure options gain the component list:

```
--disable-everything --disable-network
--enable-demuxer=mpegps,mpegvideo,asf,mov
--enable-decoder=mpeg1video,mpeg2video,mp1,mp2,mp3,wmv1,wmv2,wmv3,vc1,wmav1,wmav2,h264,hevc,aac
--enable-parser=mpegvideo,mpegaudio,vc1,h264,hevc,aac
--enable-hwaccel=h264_videotoolbox,hevc_videotoolbox
```

The port adds the rest of the configure line: `--enable-pic --disable-doc --enable-runtime-cpudetect --disable-autodetect`, the platform options (`--target-os=darwin --enable-appkit --enable-avfoundation --enable-coreimage --enable-audiotoolbox --enable-videotoolbox` for macOS, `--enable-avfoundation --enable-coreimage --enable-videotoolbox` for iOS), the libraries that `vcpkg.json` selects (`--enable-avcodec --enable-avformat --enable-swresample --enable-swscale --disable-avdevice --disable-avfilter --disable-ffmpeg --disable-ffplay --disable-ffprobe`), and a `--disable-` option for every optional external library. The port's `gpl`, `nonfree`, and `version3` features stay off, so the line has no `--enable-gpl`, `--enable-nonfree`, or `--enable-version3`, and FFmpeg's configure reports `License: LGPL version 2.1 or later`. The `mpegvideo` demuxer is there because MPEG program streams identify their video streams by probing with it. FFmpeg enables the `h263` decoder too, which `wmv1` and `wmv2` need.

With FFmpeg and the player, the iOS device framework grew from 11.0 MB to 16.7 MB.

FFmpeg is linked statically. LGPL 2.1 allows that when the recipient can relink the program against a modified library, which VNDegen's public source permits. The app must show FFmpeg's LGPL notice alongside the krkrsdl3 license.

## Left out

- The movie player in `core/media/movie/`. Its `CDVDMsg*`, `CDVDMessageQueue`, `VideoReferenceClock`, and `VideoPlayer*` classes derive from Kodi's VideoPlayer, which is GPL-2.0, and VNDegen stays free of GPL code. `krmovie.cpp`, `krmovie.h`, and `krnull.h` remain, as the interface the rest of the engine calls.
- The `wuffmpeg` audio decoder (`plugins/wuffmpeg.cpp`), which read sound files through FFmpeg. The engine decodes Ogg Vorbis, Opus, and WAV sound files, but not formats only FFmpeg reads, such as MP3. FFmpeg is built for movies only.
- The BPG image loader (`plugins/Kirikiroid2/LoadBPG.cpp`). It needs libbpg, which is LGPL, and BPG is a Kirikiroid2 conversion format that Windows games do not ship.
- The OpenGL and Vulkan render backends and their shaders. VNDegen renders with the software backend, which presents through SDL3's renderer on Metal.
- The `environ/` folders for Android, OpenHarmony, WebAssembly, Windows, Linux, SDL2, and the console build. `environ/apple/apple_video.mm` and `environ/PlatformVideo.h`, an unimplemented system video interface, are removed too.

## VNDegen's additions

- `vndegen/ciphers.cpp`: native decryption of encrypted archives. Each scheme is one row in its table.
- `vndegen/movie_decoder.cpp`: decodes a movie with FFmpeg on a worker thread, reading it through the engine's storage streams, so archive ciphers apply. `vndegen/movie_test.cpp` builds `KiriKiriMovieTest`, which decodes movies with it and checks frame counts, audio, and seeking.
- `vndegen/movie_player.cpp`: the movie player behind `krmovie.h`'s `iTVPVideoOverlay`. It shows each decoded frame when the engine's clock reaches it, as an overlay sprite over the window or into the two bitmaps a layer-mode `VideoOverlay` shows, and plays the audio through the engine's sound output. The clock is the engine's tick count, which stops while the engine is paused, kept within 40 ms of the audio played. `vndegen/movie-check/startup.tjs` is a script-only game that `scripts/kirikiri-movie-check.sh` runs to check overlay and layer mode, loops, segment loops, period events, seeking, and pausing.
  Left out: only the first audio stream plays, and `audioBalance`, `playRate`, the picture adjustments such as `contrast`, and the mixer mode's mixing bitmap have no effect. Mixer and Media Foundation modes draw as overlay mode does. Movie audio follows `audioVolume` but not the global sound volume.
- `vndegen/sigcheck.cpp`: a `sigcheck.dll` stub. Its `Window.checkSignature` reports every file's signature as valid without checking it.

## VNDegen's changes to upstream files

1. `core/archive/XP3Archive.cpp`: reads hashed-name archives. Some Kirikiri Z archives store each file under the MD5 of its name and keep the real name in an `eliF` chunk just before the `File` chunk. The index reader maps each `eliF` name to the `File` chunk after it.
2. `core/media/image/GraphicsLoadThread.cpp`: a name the archive lacks, queued by `System.touchImages`, threw on the image loader's thread and aborted the process. It is now reported as a load error, as decode failures already were.
3. `core/media/movie/krmovie.cpp`, `core/media/movie/krnull.h`: every movie logs "video not supported, skipping <name>" and is replaced by one null player whose `Play()` posts a single `EC_COMPLETE`, so the movie ends at once and a game waiting on it continues. Upstream's null layer player posted `EC_COMPLETE` on every frame. Change 27 puts VNDegen's player in front of it.
4. `environ/iOS/ios_core.mm`: drops the include of the removed `PlatformVideo.h`.
5. `plugins/Kirikiroid2/kirikiroid2.cpp`: no longer registers the removed BPG loader.
6. `core/utils/TVPSettings.cpp`: a `-gameroot=<dir>` option sets the project folder, which is `System.exePath`, the base for `savedata/`, and where plugins and fonts are looked up. Without it a `data/` folder became `System.exePath`, and games that list `System.exePath + "fonts/"` failed. Command-line options are now parsed before the paths so the option can take part.
7. `environ/sdl3/sdl3_utils.cpp`, `environ/Platform.h`: `TVPSetClockPaused` stops and resumes the engine clock. Paused time is left out of `TVPGetRoughTickCount`, so timers and transitions resume where they stopped.
8. `environ/sdl3/sdl3_audio.cpp`, `environ/PlatformAudio.h`: `TVPSetSoundPaused` pauses and resumes every open sound buffer's audio device.
9. `environ/sdl3/sdl3_app.cpp`, `environ/PlatformView.h`: mouse events are handled on iOS too, so VNDegen can inject them, and SDL no longer turns touches into mouse events there, since touches have their own handler. `TVPGetSDLWindow` exposes the engine's SDL window.
10. `environ/apple/apple_core.mm`: resources such as the fallback font are looked up in the bundle that contains the engine, which is the framework when the engine is embedded in an app, instead of the main bundle.
11. `core/archive/XP3Archive.cpp`, `core/archive/XP3Archive.h`: each archive detects its encryption when it opens, through `VNKKDetectCipher` in `vndegen/ciphers.cpp`, and keeps the scheme in `Cipher`. Reads decrypt with it unless a game's `xp3filter.tjs` has installed an extraction filter, which takes its place.
12. `core/media/font/TVPFont.cpp`: the scan of the game's `fonts/` folder passed bare file names to the font loader, which needs storage names, so a game with a `fonts/` folder stopped at startup with `Not supported media type ""`. It now passes each file's full name, and skips folders.
13. `core/utils/TVPSettings.cpp`, `core/script/tjsNativeSystem.cpp`, `core/archive/TVPStorage.cpp`, `core/archive/TVPStorage.h`: with `-gameroot`, `System.exeName` is the game's `.exe` in that folder, as on Windows, instead of the project data. Games check for files beside it, such as `<exe>.cf`. When the folder holds several, the `.exe` with a matching `.cf` wins, then a lone `.exe`. The value is no longer cached across games in one process.
14. `tjs2/tjs.h`, `tjs2/tjsObject.cpp`, `tjs2/tjsNative.cpp`, `tjs2/tjsScriptBlock.cpp`: `tTJSObjectPool`, which tracks live script objects so a stopped engine can delete the ones it leaked, is rewritten as a locked slot vector with a free list. Upstream's pool counted 0 of its 1024 starting slots, so growing it handed out slots still in use, and clearing it handed out indexes past its end. A second game in the same process then crashed in the `tTJSScriptBlock` constructor. Objects now always unregister in their destructors, and the pool deletes what is left by reading each slot again just before deleting it, so objects that a deletion frees along the way are not deleted twice.
15. `core/main/TVPWindow.cpp`: a window's destructor cancels its queued input events and window updates. `TVPClearAllWindows` deletes windows at engine shutdown without the script-side invalidation that used to cancel them, and the stale updates were delivered to the next game's freed windows.
16. `core/archive/TVPArchive.cpp`: an archive stream released after the archive handle pool has shut down, as script objects release theirs during engine shutdown, is closed instead of leaked. Each game leaked file handles until a later game could not open its archive.
17. `environ/sdl3/sdl3_file.cpp`: a file that cannot be opened for reading throws "cannot open storage". It used to produce a stream with no handle whose seeks returned -1.
18. `core/main/TVPApplication.cpp`: a script error during startup is logged and fails `StartApplication`, so `vnkk_start` returns an error. It used to escape as an uncaught exception and end the host process.
19. `core/script/tjsNativeMenuItem.cpp`: the shortcut key tables, whose absence is what makes the engine register `Window.menu`, are cleared with the script engine that owns them. Upstream cleared only the first table, and only once a game had created a menu, so after a game that failed before that, the next game had no `Window.menu`.
20. `core/utils/StringUtil.h`: `Trim` for `ttstr` trims leading and trailing spaces and control characters. Upstream's helpers tested the opposite condition and read past a one-character stack buffer, which AddressSanitizer reported for every font name.
21. `core/script/tjsNativeKAGParser.cpp`, `plugins/KAGParserEx/KAGParserEx.cpp`, `plugins/ExtKAGParser/ExtKAGParser.cpp`, `core/script/tjsNativeWaveSoundBuffer.cpp`: the parsers initialize their line position and `LineBufferUsing`, and the sound buffer initializes `BufferCanControlFrequency`. Copying a fresh parser or setting a sound's frequency read them uninitialized.
22. `core/render/gl/tvpgl.cpp`: the TLG5 and TLG6 decoders read 16- and 32-bit values at any byte offset with `memcpy` instead of misaligned pointer loads.
23. `core/utils/TVPSettings.cpp`, `core/archive/TVPStorage.cpp`, `core/archive/TVPStorage.h`, `core/script/tjsNativeSystem.cpp`: `-appdatadir=<dir>` and `-personaldir=<dir>` set what `System.appDataPath` and `System.personalPath` report, the Windows AppData and Documents folders, where some games keep their saves. Without them both stay the game folder, as before.
24. `environ/sdl3/sdl3_app.cpp`, `environ/PlatformView.h`: `TVPSetHostWindow` hands SDL an NSWindow and NSView to wrap instead of creating its own window. While it is set, the game cannot resize the window or make it full screen, since the host sizes it.
25. `core/main/TVPSystem.cpp`: `TVPTerminateSync`, which `System.exit` and an unhandled script error call, closes the game the way `System.terminate` does instead of calling `exit()`, because the engine runs inside VNDegen's process.
26. `environ/sdl3/sdl3_thread.cpp`: a thread waits to be resumed under its mutex, so a `Resume` that comes first is not lost, and `WaitFor` wakes a thread that was never resumed and lets it end. Stopping the engine after a failed start used to wait forever on the never-started image loader thread.
27. `core/media/movie/krmovie.cpp`, `core/media/movie/krnull.h`: every `VideoOverlay` mode plays through VNDegen's player in `vndegen/movie_player.cpp`. A movie it cannot open, such as one in a format the FFmpeg build leaves out, is logged and replaced by change 3's null player, so a game waiting on it continues.
28. `core/main/NativeEventQueue.cpp`, `core/main/NativeEventQueue.h`: each posted event carries its message number, so `Clear(msg)` removes only that kind. Upstream posted every event as message 0, so `VideoOverlay`'s clearing of stale `WM_GRAPHNOTIFY` events removed none of them. `Deallocate` clears the owner's queued events, which would otherwise reach a deleted `VideoOverlay`, and `Clear` does nothing once the application is gone at shutdown.
29. `core/render/TVPCompositor.cpp`, `environ/PlatformView.h`: a sprite can carry a placement, a rectangle in the window's content pixels, and is then drawn over that part of the content, scaled and letterboxed with it, instead of filling the window. Overlay-mode movies use it for `VideoOverlay`'s position and size.
30. `core/script/tjsNativeVideoOverlay.cpp`: reopening the movie that a closed `VideoOverlay` keeps cached reuses that player, which reads its own stream, and leaked the stream `Open` had just created. That stream is now deleted.
31. `environ/PlatformAudio.h`, `environ/sdl3/sdl3_audio.cpp`: a sound buffer reports `GetQueuedSamples`, the sample frames appended but not yet taken by the device, which the movie player's clock reads, and `Flush` marks the end of the appended audio. SDL holds back the last few samples of a stream it resamples, such as 44.1 kHz movie audio on a 48 kHz device, until the stream is flushed.
32. `core/main/TVPApplication.cpp`: `OnExit` deletes the movie players still alive once the script engine is gone, through `VNKKDestroyMoviePlayers`. Engine shutdown deletes leftover script objects without invalidating them, so a `VideoOverlay` a game never closed kept its player, whose decoder thread then read a sound buffer that `SDL_Quit` had destroyed and crashed the process.

Engine shutdown runs `tTVPApplication::OnExit`, which runs the at-exit handlers that `TVPAddAtExitHandler` registered, deletes the script engine and every script object it leaked, and clears the auto path table and the windows. `vnkk_stop` also resets `vnkk.mm`'s own per-game state. Together they let `vnkk_start` run another game in the same process. `KiriKiriRestart`, built by `scripts/kirikiri-engine.sh <dest> restart` or `restart-asan`, runs a list of games that way and reports the memory and file handles each one leaves behind.

Text files without a byte order mark are left as upstream decodes them. krkrsdl3 tries strict UTF-8 first, then Shift_JIS, then GBK. Real Shift_JIS text is not valid UTF-8, so Japanese scripts decode as Shift_JIS, as on Windows. UTF-8 fan translations also keep working.
