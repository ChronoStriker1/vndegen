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

## Building for Apple platforms

- Build with `--no-default-features --features metal-backend`. Without `metal-backend` the library builds but exports no C API, and the default `gl-backend` needs ANGLE.
- Set `IPHONEOS_DEPLOYMENT_TARGET` to the app's deployment target (18.0), and build from clean after changing it: lua-src's build script doesn't rerun when it changes, so stale objects keep the SDK's version.
- MetalFX is missing from the simulator SDK. The core finds it at run time and renders without it there.
- The Rust toolchain is pinned to 1.99.1, with the `aarch64-apple-ios`, `aarch64-apple-ios-sim`, and `aarch64-apple-darwin` targets.

## VNDegen's changes to art3m1s-core

- Build only the Artemis adapter. The RFVP, KRKR, and Siglus adapters are path dependencies on sibling checkouts that Cargo resolves even with their features off.
- Decode JPEG images, and find extensionless images saved as `.jpg`.
- Read pf2 archives, whose header and entries differ from pf6.
- Evaluate `$` parameters before a builtin tag runs.
- Step past an inline `[trans]` or `[video]` when it finishes.
- Run the BOOT script from its first line when setup comes before `*top`.
- Load Lua 5.1 bytecode compiled by a 32-bit `luac`, and decode its strings with the project's charset.
- Return 0 to 65535 from `e:random()`.
- Skip `/* */` block comments in scripts, and parse a tag that runs over several lines.
- Carry on when `e:include` names a missing file, and step past a tag whose script fails.
- Try `.ttf`, `.otf`, and `.ttc` for a font named without an extension.
- Keep spaces in text layout.
- Decode included Lua source with the project's charset.
- Run event handler tags through the game's tag filter.
- Call a game's own Lua `yesno` for `[yesno]`.
- Use Lua 5.1 on iOS too, instead of Luau, which can't load `.lub` bytecode. `vendor/lua-src` makes `os.execute` raise an error on iOS, which has no `system()`.
- Test tooling: the `game-probes` probe answers media commands like a host, and examples dump PFS archives and scripts.
