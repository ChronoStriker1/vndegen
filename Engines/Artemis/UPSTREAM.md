# Artemis engine upstream

VNDegen runs Artemis engine games on art3m1s-core, a Rust reimplementation of the Artemis runtime with a native Metal renderer.

## Sources

| Folder | Upstream | Commit |
|---|---|---|
| `art3m1s-core/` | https://github.com/Alphaly2K/art3m1s-core | `da4f510a600b5d7059702e67e99b0f09dbb7ab94` |
| `art3m1s-core/vendor/lua-src/` | lua-src 547.0.0 from crates.io (https://github.com/mlua-rs/lua-src-rs), the Lua sources mlua builds | Version 547.0.0 |

The first commit that adds `art3m1s-core/` holds the upstream files unmodified. Every later change is its own commit, listed below, so `git log -p -- Engines/Artemis/art3m1s-core` shows each one.

## License

art3m1s-core is under MPL-2.0 (`art3m1s-core/LICENSE`), a file-level copyleft: the source of every MPL-covered file VNDegen ships, modified or not, is this folder. Parts keep their own licenses:

- `crates/pf8`, vendored by upstream from pfs-rs: MIT (`crates/pf8/LICENSE`).
- `vendor/lua-src`: MIT (`vendor/lua-src/LICENSE`). The Lua 5.1 sources inside it are MIT too.
- asb-decrypt, a git dependency of the interpreter (https://github.com/Alphaly2K/asb-decrypt): BSD-3-Clause, from its repository's `LICENSE`. Its `Cargo.toml` declares no license.
- shaderc builds glslang, whose parser is Bison output under GPL-3.0 with the Bison exception. The exception lets that output be distributed under any terms, as with krkrsdl3's TJS parsers.
- The other crates the Apple build uses are MIT, Apache-2.0, BSD, Zlib, ISC, Unicode-3.0, Unlicense, or CC0. The audit covered all 128 crates for `aarch64-apple-ios` and `aarch64-apple-darwin`.

The upstream FFmpeg media path (the `ffmpeg` feature) is LGPL and is not built. VNDegen decodes audio and video itself.

VNDegen added `flate2` as a direct dependency for compressed PC saves. It was already in the tree through `png`, and it and its `miniz_oxide` backend are MIT or Apache-2.0. The Pluto reader and writer (`crates/asb-interpreter/src/pluto.rs`) follow the format of Pluto, the Lua persistence library the PC engine embeds, which is in the public domain. No Pluto code is copied.

## Building for Apple platforms

`scripts/artemis-engine.sh build/artemis` builds the core for VNDegen with the settings below: `build/artemis/ios/Art3m1s.xcframework`, a static library for iOS devices, the iOS simulator, and macOS (arm64 and x86_64), and the notices in `build/artemis/licenses/`. It installs the pinned toolchain and its targets through rustup when they are missing, on either Mac.

- Build with `--no-default-features --features metal-backend`. Without `metal-backend` the library builds but exports no C API, and the default `gl-backend` needs ANGLE.
- Set `IPHONEOS_DEPLOYMENT_TARGET` to the app's deployment target (18.0), and build from clean after changing it: lua-src's build script doesn't rerun when it changes, so stale objects keep the SDK's version.
- MetalFX is missing from the simulator SDK. The core finds it at run time and renders without it there.
- The Rust toolchain is pinned to 1.99.1, with the `aarch64-apple-ios`, `aarch64-apple-ios-sim`, and `aarch64-apple-darwin` targets.

## VNDegen's changes to art3m1s-core

- Build only the Artemis adapter. The RFVP, KRKR, and Siglus adapters are path dependencies on sibling checkouts that Cargo resolves even with their features off.
- Decode JPEG images, and find extensionless images saved as `.jpg`.
- Read pf2 archives, whose header and entries differ from pf6.
- Evaluate `$` parameters before a builtin tag runs.
- Step past an inline `[trans]` or `[video]` when it finishes, and only an inline one: a queued one leaves the script where it is.
- Run the BOOT script from its first line when setup comes before `*top`.
- Load Lua 5.1 bytecode compiled by a 32-bit `luac`, and decode its strings with the project's charset.
- Return 0 to 65535 from `e:random()`.
- Skip `/* */` block comments in scripts, and parse a tag that runs over several lines.
- Carry on when `e:include` names a missing file, and step past a tag whose script fails.
- Try `.ttf`, `.otf`, and `.ttc` for a font named without an extension.
- Keep spaces in text layout.
- Decode included Lua source with the project's charset.
- Run event handler tags through the game's tag filter.
- Ignore `color` on `[lyprop]`, as the PC engine does, so an image layer a game first filled with a color stays an image when its helper passes the color to `[lyprop]` too.
- Run a game's own `[yesno]` macro in place of the builtin dialog, and call its Lua `yesno` when it has no macro.
- Use Lua 5.1 on iOS too, instead of Luau, which can't load `.lub` bytecode. `vendor/lua-src` makes `os.execute` raise an error on iOS, which has no `system()`.
- Read and write the PC engine's save files, so saves move between a PC and VNDegen: `system.dat`, `saveg.dat`, and numbered and quick saves, in version 1000 and in the compressed version 1002 (`src/save/bow.rs`, `pc.rs`, `pc_slot.rs`, `src/runtime/pc_save.rs`). This replaces upstream's JSON saves. It includes Pluto, the PC engine's Lua persistence format; instruction indexes counted the PC engine's way, without `[/if]`; resuming a save where the PC engine does; and writing the version of the files the game already has, else the one its archives suggest. `docs/artemis-save-format.md` in VNDegen describes the format.
- Keep the Lua state onSave writes, whatever the variable's name.
- Decode text `e:file` reads with the project's charset, let `get_layer_info` see layers `e:tag` has queued, and compile Shift_JIS Lua as bytes before converting its strings.
- Input handlers: keep the call a handler makes as its event frame, return a game's yes/no dialog's buttons to the dialog's wait, keep a click wait when a handler's own wait follows it, and wait for a transition or video a handler queues instead of for a click.
- Recreate a PC save's scene before onLoad runs. Drop a save's frame wait that has not run once the script reaches a wait of its own, as when onLoad rebuilds a choice.
- Keep text's spaces and a line's leading full-width space, as the PC engine numbers and prints them.
- Open a quoted value only at the start of a token, as the PC engine does, so a tag with a quote missing, such as `[var name="t.menu data="0"]`, still closes at its `]` instead of showing as text.
- Pass scenario text to a game's tag filter as `[print data=...]`, as the PC engine does, so games whose `print` hook marks the page and queues the click wait break their pages and allow their menu. Read a short color such as `631` as the hex number 0x000631.
- Start a new page when scenario text, though not text a UI's `[print]` writes, runs a character below its message window, with lines spaced as the PC engine spaces them: a body 1.2 times the font size, whatever the font.
- Use a host's font only for a face the game ships no font file for, instead of for every face, so games keep their own fonts.
- Draw ASCII from the bitmap fonts some games ship as `.rft` files (`src/text/rft.rs`), advanced as the PC engine advances them, so English text wraps where it does on the PC. The files store no character map, so other characters use the host's font.
- Find save files and the loose files beside a game's archives in any case, as VNDegen's `Engines/Shared/casepath.c` does, and use a save folder restored with Windows' cp1252 spelling of its name.
- Format VNDegen's changes with rustfmt.
- The C API table gains `runtime_set_pc_save_version`, `resources_file_size`, and `resources_read` at its end, which a host reads when `struct_size` covers them.
- Test tooling: the `game-probes` probe answers media commands like a host, gives a host font from `PROBE_HOST_FONT`, sets the save version from the archives, and mounts archive names as Shift_JIS. Examples dump PFS archives, scripts, and PC saves.
