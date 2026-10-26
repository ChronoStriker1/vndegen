# RGSS engine

VNDegen runs RPG Maker XP, VX, and VX Ace games on mkxp-z's libretro core, built by `scripts/rgss-engine.sh` into `build/rgss/RGSS.xcframework`. Apart from `host/libretro.h`, nothing here is a copy of upstream code. The script fetches the sources below at pinned commits, applies the patches in this folder, and builds them with VNDegen's frontend.

## Sources

| What | Repository | Commit |
|---|---|---|
| mkxp-z libretro core, from the pull request that adds it ([mkxp-z#255](https://github.com/mkxp-z/mkxp-z/pull/255)) | `https://github.com/white-axe/mkxp-z`, branch `libretro` | `650cb0888a07d0b5044e131160ddfa53feaf595b` |
| ANGLE, through mkxp-z's Meson build of it (`subprojects/`) | `https://github.com/mkxp-z/mkxp-z`, branch `dev` | `37a04d14413454551a7b6aadbb4e93e67168accb` |
| WASI SDK, to build Ruby for stage 1 | `wasi-sdk-30.0-arm64-macos.tar.gz` from WebAssembly/wasi-sdk | SHA-256 in the script |
| Binaryen, to optimize stage 1 | `binaryen-version_126-arm64-macos.tar.gz` from WebAssembly/binaryen | SHA-256 in the script |

The core builds Ruby 3.3.10, its other dependencies, and the GMGSx soundfont from the versions its own `subprojects/` and `libretro/Makefile` pin.

## Patches to the core (`patches/`)

- `01-iconv-utf-8-mac.patch`: on Apple platforms mkxp-z converts file names from NFD to NFC with iconv's `utf-8-mac`, which the GNU libiconv the core bundles doesn't have. `iconv_open` fails and the next `iconv` call crashes while the core lists a game's files. The patch skips the conversion when iconv can't do it. APFS keeps names as written, so names copied from Windows are already NFC. Upstream [mkxp-z#397](https://github.com/mkxp-z/mkxp-z/pull/397) concerns the same code.
- `02-static-core.patch`: a `libretro_static` Meson option that builds the core as a static library holding its dependencies, so VNDegen links it into one framework with ANGLE and its frontend.
- `03-flat-saves.patch`: with `MKXPZ_RETRO_FLAT_SAVES` defined, the core writes saves straight into the save folder the frontend gives it, instead of `mkxp-z/Saves/<game title in punycode>` inside it. VNDegen's save folder is already one per game.
- `04-ruby-toplevel-class-variables.patch`: adds `ruby-rgss-cvar.patch` to stage 1. Ruby 3 raises on a class variable read or written at the top level, such as `@@x` inside `class << SomeModule`, which Ruby 1.8 and 1.9, the Rubies of RGSS, allowed. The patch makes Ruby warn instead, as Ruby 2 did. It joins the core's own `ruby-compat.patch`, which restores other old Ruby behavior.

`patches/angle/01-ios-gpu-info.patch` fixes mkxp-z's ANGLE build for iOS, which names `src/gpu_info_util/SystemInfo_ios.mm`. ANGLE renamed that file to `SystemInfo_ios.cpp`.

## VNDegen's additions

- `host/`: the libretro frontend (`vnrgss.mm`, the functions in `Packages/VNDegenKit/Sources/CRGSS/include/vnrgss.h`), the libretro file system on POSIX files (`vfs.c`), libretro's `libretro.h` (MIT), and the framework's Meson build.
- `preload/zz_vndegen.rb`: a preload script the core runs before each game's scripts. It joins the core's own preload scripts. It covers Windows API calls games make that mkxp-z's emulation lacks or mishandles, and makes `load_data` raise `Errno::ENOENT` for a missing file, which the core otherwise turns into a crash.
- `cross/`: Meson cross files for macOS, iOS, and the iOS simulator, all arm64.
- `licenses/`: Ruby's license, since stage 1 doesn't keep Ruby's source, and the notice that says where the source is.
