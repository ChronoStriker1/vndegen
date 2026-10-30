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
- `05-case-insensitive-lookups.patch`: games written for Windows name files in any case. `vfs.c` matches paths to the disk ignoring case, which covers every file the core opens by path. The patch fixes the three places where the core compares names itself. `FileSystem::openRead`, which loads graphics, audio, and movies, compares names ignoring case. When its path cache, made at startup, has no match, it also looks in the folder itself, so it finds files the game wrote later. The RGSSAD archive reader looks entries up by their lowercased paths and lists them as the archive names them. A WASI rename between paths that differ only in case does nothing, since both name the same file and the core's copy-then-delete rename would empty it. `System.desensitize` still answers from the path cache and returns other names as given, which then open through `vfs.c` in any case.

`patches/stage2/06-windows-fonts.patch`: Windows fonts a game names and doesn't ship, such as MS PGothic or Arial, draw in their substitutes from VNDegen's font table (`Engines/Shared/vnfont.h`, see `docs/fonts.md`). `SharedFontState::getFont` opens the substitute's file when the family isn't among the game's fonts, and `fontPresent` reports such a family as present, so `Font.exist?` and the choice of `Font.default_name` see it, as an installed font is seen on Windows. The core asks the frontend's `vnrgss_font_substitute`, which `vnrgss_set_fonts` configures. Upstream drew every missing font in its built-in Liberation Sans. Patches in `stage2/` change only code that stage 2 builds, so they stay out of stage 1's cache key and don't rebuild Ruby.

`patches/angle/01-ios-gpu-info.patch` fixes mkxp-z's ANGLE build for iOS, which names `src/gpu_info_util/SystemInfo_ios.mm`. ANGLE renamed that file to `SystemInfo_ios.cpp`.

## VNDegen's additions

- `host/`: the libretro frontend (`vnrgss.mm`, the functions in `Packages/VNDegenKit/Sources/CRGSS/include/vnrgss.h`), the libretro file system on POSIX files (`vfs.c`), libretro's `libretro.h` (MIT), and the framework's Meson build. `vnrgss.mm` also answers the core's font lookups through `Engines/Shared/vnfont.c`. `vfs.c` looks every path up through `Engines/Shared/casepath.c`, so a game finds its files whatever case it names them in. A write to a file that exists in another case replaces that file, and `mkdir` of a folder that exists in another case reports that it exists. The script links `Engines/Shared` into the build as `host/shared`.
- `preload/zz_vndegen.rb`: a preload script the core runs before each game's scripts. It joins the core's own preload scripts. It sets `Font.default_name` to ＭＳ Ｐゴシック for games on the Japanese RPG Maker XP player, which Game.ini names, as that player does. It covers Windows API calls games make that mkxp-z's emulation lacks or mishandles, makes `load_data` raise `Errno::ENOENT` for a missing file, which the core otherwise turns into a crash, and makes `Dir.glob` and `Dir[]` ignore case, as they do on Windows.
- `cross/`: Meson cross files for macOS, iOS, and the iOS simulator, all arm64.
- `licenses/`: Ruby's license, since stage 1 doesn't keep Ruby's source, and the notice that says where the source is.
