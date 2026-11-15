# Relinking VNDegen's LGPL libraries

VNDegen contains a few libraries under the GNU LGPL version 2.1 or later. The LGPL lets an app include them as long as
whoever receives the app can change such a library and use the changed version with it. VNDegen keeps each one in a
library or file of its own that the app loads when it runs, so a changed copy can take its place. This page says what
those libraries are, where to get their exact source, and how to swap in a changed copy on each platform.
`scripts/licenses/How VNDegen meets the LGPL.txt`, which the app's Licenses page shows, is the short version for
users, with the written offer for the source.

The maintainers read the license this way; this is not legal advice. Questions it leaves open are at the end.

## What is LGPL, and where it is

| Library | Used by | iPhone and iPad | Mac |
|---|---|---|---|
| FFmpeg 8.1.2 (libavformat, libavcodec, libavutil, libswscale, libswresample) | KiriKiri movies, NekoSDK WMV movies | `Frameworks/FFmpeg.framework` | `Contents/Frameworks/FFmpeg.framework` |
| FFmpeg 4.3.1 | Ren'Py: all sound and movies | `Frameworks/RenPyRuntime.framework` | `librenpython.dylib` of each Ren'Py runtime |
| GNU FriBidi 1.0.7 | Ren'Py: right-to-left text | same | same |
| pygame's `alphablit.c` and `surface.h` (the rest of pygame_sdl2 is zlib) | Ren'Py: drawing images with transparency | same | same |
| chardet 5.2.0 (Ren'Py 8), 3.0.4 (Ren'Py 7) | Ren'Py's `requests` | `RenPy/8/lib/python3.12/chardet` | `Contents/Resources/RenPy/8/lib/python3.12/chardet`, `RenPy/7/lib/python2.7/chardet` |

The Mac's Ren'Py libraries are `Contents/Resources/RenPy/8/lib/py3-mac-universal/librenpython.dylib` and
`Contents/Resources/RenPy/7/lib/py2-mac-universal/librenpython.dylib`, Ren'Py's official builds, unchanged. On iPhone
and iPad, `scripts/renpy-runtime.sh` links RenPyRuntime.framework from the prebuilt static libraries in Ren'Py's renios
package, as described below.

Every FFmpeg build in the app is configured without `--enable-gpl`, `--enable-nonfree` or `--enable-version3`, and
reports "LGPL version 2.1 or later".

Up to 0.41.8, Ren'Py's iOS libraries were linked into the app's executable, FFmpeg 8.1.2 was inside
KiriKiri.framework, and rclone carried a module under the LGPL version 3 (`github.com/cloudsoda/sddl`, which its SMB
code imports). VNDegen now builds rclone with a small module of its own in place of that one
(`Packages/VNDegenKit/Rclone/sddl`), since rclone never uses Windows security descriptors.

## Getting the source

Each GitHub release has `VNDegen-<version>-LGPL-sources.tar`, which `scripts/lgpl-sources.sh` makes as part of
`scripts/release.sh`. It holds:

- `ffmpeg-4.3.1.tar.gz` and `fribidi-1.0.7.tar.bz2`, from the `source` folder of
  https://github.com/renpy/renpy-build at tag `renpy-8.5.3.26051504`. The same files are at `renpy-7.8.7.25031702`.
- `renpy-8.5.3.26051504-pygame.tar.gz`: `src/pygame` and `renpy/pygame` of Ren'Py at tag `8.5.3.26051504`, the
  pygame_sdl2 Ren'Py 8.5 carries. `pygame_sdl2-renpy-7.8.7.25031702.tar.gz`: pygame_sdl2 for Ren'Py 7.8.7.
- `chardet-5.2.0.tar.gz` and `chardet-3.0.4.tar.gz` from PyPI.
- `ffmpeg-n8.1.2.tar.gz`, FFmpeg's tag `n8.1.2` as vcpkg fetches it (its SHA-512 is pinned in
  `Engines/KiriKiri/ports/ffmpeg/portfile.cmake`), and `vndegen-ffmpeg-port.tar.gz`, that vcpkg port with its patches
  and configure options (`Engines/KiriKiri/UPSTREAM.md` lists VNDegen's changes to it).
- `README.txt`, with each file's source address and SHA-256, and the address and SHA-256 of Ren'Py's renios package,
  `https://www.renpy.org/dl/8.5.3/renpy-8.5.3-renios.zip`, SHA-256
  `c4fae153e8276ed0faed5e84ea3e0b7c4bf337f0e3208e9130c6a41748a83b2b`. Its `renios/prototype/prebuilt` folder holds the
  object code of the rest of Ren'Py's iOS runtime, which RenPyRuntime.framework is linked from.

The tar is about 36 MB. `scripts/lgpl-sources.sh <version> <folder>` makes it anywhere, checking every download
against its pinned SHA-256 and caching it in `~/.cache/vndegen/lgpl-sources`.

## Putting a changed library into the app

On iPhone and iPad, a copy installed from the App Store can't be changed: the store encrypts and signs it. A changed
library goes into the IPA from VNDegen's GitHub releases, which isn't encrypted, or into an app you build yourself.
Unzip the IPA, replace the framework in `Payload/VNDegen.app/Frameworks/`, zip `Payload` again, and install it with
AltStore, SideStore or Sideloadly, which sign it with your own certificate. `scripts/build.sh ipa` builds the whole
IPA from source instead.

On the Mac, replace the file in `VNDegen.app`, then sign it and the app again:

```
codesign --force --sign - VNDegen.app/Contents/Frameworks/FFmpeg.framework
codesign --force --deep --sign - VNDegen.app
```

### FFmpeg 8.1.2 (KiriKiri and NekoSDK)

`scripts/kirikiri-engine.sh` builds FFmpeg with vcpkg from the overlay port in `Engines/KiriKiri/ports/ffmpeg` and
links it into FFmpeg.framework, which exports FFmpeg's public interface (every `av*`, `sws*` and `swr*` symbol), for
iOS devices, the iOS Simulator and the Mac (arm64 and x86_64). To change FFmpeg, add a patch to the port's `PATCHES`
list in `portfile.cmake`, or change its configure options, then run `scripts/build.sh ipa` or `scripts/build.sh mac`
on a Mac. The finished frameworks are in `build/kirikiri/ios/FFmpeg.xcframework`.

A framework of your own works too, if it is FFmpeg 8.1 with the same major library versions, built with at least the
components in `UPSTREAM.md`, as a framework named `FFmpeg` with the install name `@rpath/FFmpeg.framework/FFmpeg` on
iOS or `@rpath/FFmpeg.framework/Versions/A/FFmpeg` on the Mac.

### Ren'Py on iPhone and iPad

`scripts/renpy-runtime.sh build/renpy ios` links RenPyRuntime.framework from renios's prebuilt libraries: `release` for
devices and `debug` for the Simulator. It exports every global symbol of the libraries it links, which covers the
functions VNDegen calls (`init_librenpy`, `Py_InitializeEx` and the others in `ios_entry_points`) and the SDL functions
games reach through `ctypes`. renios's SDL hides `SDL_iPhoneSetEventPump`, so the framework also exports it as
`vndegen_SDL_iPhoneSetEventPump`, which is the name VNDegen calls.

To change FFmpeg 4.3.1, FriBidi or Pygame's code:

1. Build renios's libraries with renpy-build at tag `renpy-8.5.3.26051504` (`./build.sh --platform ios`, as its
   README says), with your change to the library's source. Or build just the changed library yourself as a static
   library for the same architecture.
2. Put the changed libraries in a folder laid out like renios's `prototype/prebuilt`: `release/libavcodec.a` and so on
   for devices, `debug/` for the Simulator. Only the files you put there replace renios's.
3. On a Mac, run `VNDEGEN_RENIOS_LIBRARIES=<that folder> scripts/build.sh ipa`. While that variable is set,
   `scripts/renpy-runtime.sh` links the framework again on every run, with your libraries copied over renios's.
4. Take `Frameworks/RenPyRuntime.framework` from the new IPA's app, or install the whole IPA.

### Ren'Py on the Mac

The Mac app uses Ren'Py's own `librenpython.dylib` for each runtime. Build Ren'Py's Mac libraries with renpy-build at
tag `renpy-8.5.3.26051504` for Ren'Py 8 or `renpy-7.8.7.25031702` for Ren'Py 7 (`--platform mac`, both
architectures), and replace `librenpython.dylib` in the runtime's `lib/py3-mac-universal` or `lib/py2-mac-universal`
folder. Sign it and the app again as above.

### chardet

chardet is Python bytecode. Compile a changed copy with the matching Python (3.12 for Ren'Py 8, 2.7 for Ren'Py 7) and
replace the `chardet` folder in the runtime's `lib/python3.12` or `lib/python2.7` folder, then sign the app again. On
iPhone and iPad, that folder is `Payload/VNDegen.app/RenPy/8/lib/python3.12/chardet` in the IPA.

## Open questions

These need a lawyer, not this page:

- Whether distributing through the App Store, under Apple's usage rules, adds "further restrictions" that section 10
  of the LGPL 2.1 forbids. The FSF argued this for the GPL in 2011, when VLC left the App Store. For the LGPL it is
  common practice and hasn't been tested.
- Whether the written offer and the release asset on GitHub meet sections 4 and 6(d), which ask for the source to be
  available "from the same place" as the binary, when the binary comes from the App Store.
- Whether, for a paid App Store release, relinking a framework from public object code, as for Ren'Py's runtime on
  iPhone and iPad and on the Mac, is enough, or whether each LGPL library must be a library of its own, as FFmpeg 8.1.2
  is.
