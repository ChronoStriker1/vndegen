# KiriKiri feasibility spike

Spike date 2026-10-26, on branch `ws/kirikiri`. The question was whether VNDegen can run the library's KiriKiri games natively on macOS and iOS, given two known risks: per-game encryption of `.xp3` archives, and Windows plugin DLLs that a port has to reimplement.

## Answer

Yes, on krkrsdl3, not on krkrsdl2.

- **krkrsdl2 brought none of the 7 test games to the title screen on macOS.** It cannot load Windows DLLs, has no `MenuItem` class, which every KAG3 game creates, and has no archive decryption hook. Its README says unmodified commercial games are unsupported. Its published iOS bundle is a device build, so it does not run in the simulator.
- **krkrsdl3 reached a correct title screen in 4 of 7 test games, on macOS and on the iPhone simulator in landscape.** krkrsdl3 is the newer SDL3 line, derived from Kirikiroid2. It builds Kirikiroid2's reimplementations of about 40 common plugins into the engine and loads a per-game `xp3filter.tjs` decryption script. In the simulator, a tap started the story and a save to slot 1 worked. Two engine patches from this spike were needed. Of the other 3 games, one runs without script errors but its title was not captured, one draws its title mostly black, and one failed because of how the spike launched it, which the VNDegen launcher would do correctly.
- **The ciphers seen so far are small.** Three schemes turned up in the sample. Two games use a one-byte XOR keyed by each file's checksum. One game's `decoder.tpm` plugin applies a four-band byte cipher, recovered by disassembling the 40 KB DLL. One family of games hashes file names but leaves file data plain. Each scheme is about 10 lines of code.

## Evidence

Builds ran on the Mac mini (Apple M4, macOS 27.0.1, Xcode 27.1). The first macOS runs used the MacBook. After the maintainer reserved the MacBook's screen, the remaining macOS runs moved to the Mac mini, where SSH sessions cannot capture the screen, so those runs have logs only. iOS runs used a dedicated iPhone 18 Pro simulator (iOS 27.0). Screenshots and logs are in `.audit/evidence/spike/` in the checkout, which is gitignored.

| Game | Archive | Plugins it ships | krkrsdl2, macOS | krkrsdl3, macOS | krkrsdl3, iOS simulator |
|---|---|---|---|---|---|
| Test Game 03 | Plain `data.xp3`, Kirikiri 2 | krmovie, wuvorbis | Fails: `Member "MenuItem" does not exist` | Intro screen, then a title screen drawn mostly black with both renderers | Not run |
| Test Game 08 | Plain `data.xp3` | addFont, extrans, json, KAGParser, krmovie, layerExSave, menu, wuvorbis | Fails: `Cannot load Plugin plugin/menu.dll` | Title | Title |
| Test Game 01 | Unpacked `data/` folder | krmovie, wuvorbis | Fails: `MenuItem` | Title | Title |
| Test Game 70 | Plain `data.xp3` and `patch.xp3` | extrans, krmovie, wuvorbis | Fails: `MenuItem` | Crashed in the image preloader. After the fix below it ran 60 s with no script error. Title not captured | Not run |
| Test Game 71 | 6 archives, XOR by checksum | 50 DLLs | Fails: `Cannot load Plugin addFont.dll` | Fails: `System.exePath` pointed at `data/`, so `Storages.dirlist(System.exePath + "fonts/")` failed | Not run |
| Test Game 72 | `decoder.tpm` cipher | AlphaMovie, csvParser, fstat, krmovie, layerExBTOA, layerExDraw, wuvorbis | Fails: encrypted `startup.tjs` | Title, through an `xp3filter.tjs` | Title |
| Test Game 73 | Hashed names, compiled TJS bytecode, 6 archives | 22 DLLs, including PackinOne, psbfile, extNagano, k2compat | Fails: `startup failed: corrupted execution file` | Title with music and voice, after the name patch below | Title. A tap starts the prologue, and save slot 1 writes `data0.bmp` and updates `datasc.ksd` |

krkrsdl3 renders by compositing layers on the CPU and presenting each frame through `SDL_Renderer`, which SDL3 backs with Metal on Apple platforms (`-render=software`, log line `SWRender Backend: metal`). Its OpenGL backend also runs on macOS through Apple's GL-on-Metal layer, but iOS would need ANGLE for it. Test Game 03's title draws mostly black with both renderers, so it is not a renderer bug.

## Engine patches made in the spike

1. **Hashed-name archives.** Some KiriKiri Z archives store each file under the MD5 of its lowercased UTF-16LE name, with the real name in an `eliF` chunk placed just before the `File` chunk. The checksum is not a usable key, because one archive gives every entry the same value. The patch maps each `eliF` name to the `File` chunk that follows it while reading the index.
2. **Image preloader crash.** `System.touchImages` queues images on a worker thread. A name the archive lacks threw on that thread and aborted the process. The patch reports it as a load error, as the loader already does for decode failures.

## Ciphers

| Scheme | Games in sample | Rule |
|---|---|---|
| XOR by checksum | Test Game 71 and one more game in the sample | `byte ^= adler32 & 0xff` for every byte |
| `decoder.tpm` band cipher | Test Game 72 | With `k = adler32 & 0xff` and `p` the byte's offset in the file: below 0x7b, `^= k*0x15`; below 0xf6, `+= k*0xe0`; below 0x171, `^= k*0x2b`; otherwise `+= k*0xca` |
| Hashed names | Test Game 73 | Names only. Data is plain |

`decoder.tpm` ships in 6 of the 40 listed library games and `xp3dec.tpm` in 3. Their ciphers may differ per game. Ciphers compiled into a game's `.exe` are invisible in a file listing, so this count is a lower bound. The spike's `xp3filter.tjs` runs the cipher in TJS, one byte at a time. That is fast enough to reach the title, but production ciphers belong in native code.

## Plugin census

Listing the archives of the 48 library KiriKiri games read 40 of them. The other 8 are multi-part or installer archives that the listing tool could not open. The most common plugins, by number of games:

| Plugin | Games | In krkrsdl3 |
|---|---:|---|
| wuvorbis (Ogg Vorbis) | 37 | Yes |
| krmovie (video) | 30 | Video is in the core, through FFmpeg |
| extrans (transitions) | 28 | Yes |
| menu (window menus) | 16 | The core has `MenuItem` |
| win32dialog | 14 | Yes |
| layerExDraw, csvParser, fstat | 12 each | Yes |
| windowEx, AlphaMovie | 11 each | Yes |
| json, addFont | 10 each | Yes |
| KAGParser | 10 | The core has `KAGParser` |
| layerExSave | 10 | No. Its source file in krkrsdl3 is empty |
| win32ole | 8 | No. The tested games did not need it |
| getSample | 8 | Yes |
| KAGParserEx, PackinOne, psbfile, textrender, wuopus, layerExBTOA | 6 each | Yes |
| extNagano, k2compat, kagexopt | 6 each | No. Test Game 73 reached its story without them |
| decoder.tpm | 6 | Cipher, see above |
| krflash (Flash) | 6 | No |
| krkrsteam, steam_api | 5 each | No. Steam builds need a stub |
| psd | 5 | No |

Counts are games that ship the plugin as a `.dll` or a `.tpm`. The rest appear in 1 to 4 games each. They include Live2D, MPEG-2 video (`m2vdec`), a DRM wrapper (`dmmcloud`), and locale-emulator hooks, which can be ignored.

## Recommended scope

1. Build on krkrsdl3, statically linked into VNDegen on macOS and iOS, and render with `-render=software` through SDL3's Metal renderer. This needs no JIT and no OpenGL on iOS.
2. Ship ciphers as native code in a small table keyed by game, starting with the three schemes above. Detect each one by probing a known file: decrypt the first bytes of `startup.tjs` or a `.png` and check the magic number. Add more schemes as library games need them, using `decoder.tpm` disassembly and known cipher lists as references.
3. Launch with the game folder as `System.exePath` and `data.xp3` or `data/` as the project. Saves stay in the game's `savedata/` folder, as on Windows, which gives the Ludusavi mapping.
4. Fix the engine gaps that the spike found: Test Game 03's black title, black save thumbnails in the software backend, and plugin stubs for Steam builds.
5. Defer krflash, Live2D, psd, and DRM-wrapped games.

## Risks

- **License.** krkrsdl3 and Kirikiroid2 carry the Kirikiri BSD-style license. krkrsdl3's video player uses `CDVDMsgPlayer*` message classes, which suggests it is derived from Kodi's VideoPlayer, and Kodi is GPL-2.0-or-later. This is inferred from class names, not verified. Replacing that player with AVFoundation keeps the app free of GPL code and also uses hardware video decoding.
- **krkrsdl3 is young.** The repository was created in December 2025 and has 54 stars. Its build pulls dependencies through vcpkg, and its iOS preset targeted iOS 13, which Xcode 27 rejects. The spike raised the target to iOS 16.
- **Coverage.** The census found 6 games that use `decoder.tpm` with possibly different ciphers, 8 games whose archives could not be listed, and ciphers inside `.exe` files that a listing cannot see.
