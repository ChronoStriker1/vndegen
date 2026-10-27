# Third-party notices

The app's Licenses page lists every text file its build collects from `build/*/licenses/`. Each runtime script writes
its own:

- `scripts/rclone-runtime.sh` writes `build/rclone/licenses/` from Go's module cache: rclone's license, the Go standard
  library's, and those of every module `go list -deps` reports for the library's targets.
- `scripts/renpy-runtime.sh` writes `build/renpy/licenses/` from the texts in `renpy/`, choosing each by evidence in
  the runtimes it installed: a symbol in Ren'Py's native library or a file in the runtime. It copies Ren'Py's own
  `LICENSE.txt` and the font licenses from the runtime itself.
- `scripts/kirikiri-engine.sh` writes `build/kirikiri/licenses/`: krkrsdl3's license, the Android Open Source Project's
  Apache-2.0 text for the bundled fallback font and the velocity tracker, and each vcpkg dependency's `copyright` file.
- `scripts/rgss-engine.sh` writes `build/rgss/licenses/`: mkxp-z's license with where to get the source of the build,
  the license files of ANGLE and of every dependency the core builds from source, and the texts in
  `Engines/RGSS/licenses/` for code whose source folder has none: Ruby and libyaml (built in stage 1, whose sources
  aren't kept), header-only Boost, libretro-common, the Khronos headers, and what the core embeds (the Liberation Sans
  font, SDL's controller database, and the GMGSx soundfont). It fails on a source folder with no license and no text.
- `scripts/app-licenses.sh` writes `build/app/licenses/` for third-party code in VNDegen's own sources.
- `scripts/fonts-licenses.sh` writes `build/fonts/licenses/` from the license files in `Engines/Shared/fonts/licenses/`
  for the fonts the app bundles: Liberation, DejaVu, and the Ume fonts, whose license page it turns into text. The
  Chinese and Korean font downloads install Adobe's `LICENSE.txt` beside the fonts.

`scripts/build.sh` fails when one of these folders has no notices.

## Where the Ren'Py texts come from

Each file in `renpy/` starts with a line naming the code and version it covers, followed by the upstream license
files, unchanged. They match Ren'Py 8.5.3.26051504 and 7.8.7.25031702, whose library versions are pinned in the
`renpy-8.5.3.26051504` and `renpy-7.8.7.25031702` tags of https://github.com/renpy/renpy-build.

- From the source tarballs in renpy-build's `source/` folder: Python 3.12.8 and 2.7.18 (`LICENSE`, and the
  incorporated software section of `Doc/license.rst`; HACL* from the header of `Modules/_hacl/Hacl_Hash_SHA2.c`),
  SDL2 2.0.20, SDL2_image 2.6.2, FFmpeg 4.3.1, FriBidi 1.0.7, FreeType 2.13.3, HarfBuzz 8.0.1, libpng 1.6.43,
  libjpeg-turbo 1.5.3, libwebp 1.3.2, Brotli 1.1.0, XZ Utils 5.4.2, bzip2 1.0.8, zlib 1.3.1, libffi 3.4.5,
  OpenSSL 3.3.2 and 1.1.1s, pyjnius 1.7.0, and pyobjus 1.1.0.
- From the projects' repositories at the version renpy-build checks out: aom v3.5.0, libavif v0.11.1, libyuv
  331c361581896292fb46c8c6905e41262b7ca95f, and assimp v5.4.3 with the licenses of the `contrib/` libraries its
  binaries contain. MetalANGLE, pygame_sdl2, and SDL_GameControllerDB from their default branches, MetalANGLE with
  the licenses of its `third_party` code and of glslang, SPIRV-Cross, and SPIRV-Headers at the revisions in its
  `DEPS`, and Chromium's `LICENSE` at the revision its `base` helpers came from.
- SDL2's file adds the notices of the code SDL carries: HIDAPI (`src/hidapi`, under its BSD-style option), yuv2rgb,
  and the libm header from `src/libm/e_atan2.c`. FreeType's adds the licenses of its BDF and PCF drivers (their
  READMEs and `src/pcf/pcfutil.c`), `src/base/fthash.c`, and `src/base/md5.c`. HarfBuzz's adds the header of
  `src/hb-ucd.cc`, and SDL2_image's the headers of `nanosvg.h` and `nanosvgrast.h`.
- From Ren'Py's repository at the 8.5.3.26051504 tag: libhydrogen (`src/libhydrogen/LICENSE`) and tinyfiledialogs
  (the license in `src/tinyfiledialogs/tinyfiledialogs.h`).
- From PyPI, at the versions renpy-build installs for Ren'Py 8: future, six, rsa, pyasn1, ecdsa, urllib3, chardet,
  certifi, idna, requests, pefile, websockets, and PySocks. typing and win_inet_pton at Ren'Py 7's versions. The
  Apache License 2.0 in `rsa.txt` is the copy in requests' `LICENSE`, and the Mozilla Public License 2.0 in
  `certifi.txt` is https://www.mozilla.org/media/MPL/2.0/index.txt.

Notices for code a library carries from other projects go in that library's file. To find such code, look for its
symbols in the built binaries (`nm`), as was done for the files above.

`How VNDegen meets the LGPL.txt` is VNDegen's own.

When Ren'Py's version changes, check renpy-build's tag for changed library versions, and update a text when its
license or copyright lines changed. The notice table in `scripts/renpy-runtime.sh` fails the build when a library
renios links has no notice, or when its evidence no longer matches.
