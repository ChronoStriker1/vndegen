# SiglusEngine and RealLive engine upstream

VNDegen runs SiglusEngine and RealLive games on siglus_rs, a Rust reimplementation of VisualArt's engines, behind a C interface of VNDegen's own.

## Sources

| Folder | Upstream | Commit |
|---|---|---|
| `siglus_rs/` | https://github.com/xmoezzz/siglus_rs | `5f358ef1360b0439e92aed2b749e8f25d8721954` (1.0.0, 2026-10-08) |
| `vnsiglus/` | VNDegen's own: the C interface in `Packages/VNDegenKit/Sources/CSiglus/include/vnsiglus.h` | |
| `wmv_stub/` | VNDegen's own: stands in for the WMV and WMA decoder VNDegen leaves out | |

`siglus_rs/` holds the crates the engine builds from: `siglus_scene_vm` (SiglusEngine), `reallive`, and the crates they use (`avg32`, `engine-detect`, `game_fs`, `siglus_assets`, `siglus_key_recovery`, `siglus_omv_decoder`, `theora-rs`, `na_mpeg2_decoder`, `shion-xfile`, and `vendor/eluna_rs`), with the workspace's `Cargo.toml`, `Cargo.lock`, license and README. The crates' tests and examples, upstream's apps under `platform/`, its other crates, images and manual are not copied.

The first commit that adds `siglus_rs/` holds the upstream files unmodified, except for the files left out below. Every later change is its own commit, listed below, so `git log -p -- Engines/Siglus/siglus_rs` shows each one.

## Left out

- `crates/na_wmv_player`, the WMV and WMA decoder. Its WMA Pro files carry `SPDX-License-Identifier: LGPL-2.1-or-later`, and its video decoder is described as an integer-for-integer translation of FFmpeg's `libavcodec/vc1dsp.c`, which is LGPL. `wmv_stub/` has the types `siglus_scene_vm` names and fails to open any file, so `.wmv` movies don't play.
- `crates/siglus_scene_vm/assets/fonts/default.ttf`, Microsoft's MS PGothic, which upstream compiles into the engine. The host passes a substitute at run time.
- `crates/reallive/src/dt00` and `crates/reallive/src/pt00`, machine translations of the decompiled DLLs of Tomoyo After's dungeon game (`dt00.dll`) and Little Busters!' baseball game (`PT00.DLL`). That code has no license of its own.
- `icon/Icon.png`, the desktop player's window icon.

## License

siglus_rs is under MPL-2.0 (`siglus_rs/LICENSE-MPL-2.0`), a file-level copyleft: the source of every MPL-covered file VNDegen ships, modified or not, is `siglus_rs/`. `vnsiglus/` and `wmv_stub/` are VNDegen's, under MIT.

- `theora-rs` describes itself as a rewrite of libtheora 1.2.0, so its notice carries libtheora's BSD-3-Clause license (`licenses/libtheora.txt`, from https://github.com/xiph/theora).
- `reallive` cites rlvm (GPL-3.0) in two comments, as the source of its reading of the HIK animation format and of one effect's parabola, and its tests, which VNDegen doesn't copy, can run against rlvm's test data. No code is attributed to rlvm. rlvm itself was not read for this port.
- The crates of the Apple build, 263 for `aarch64-apple-ios` and audited with `cargo license` and `cargo tree`, are MIT, Apache-2.0, BSD, Zlib, ISC, Unicode-3.0, CC0-1.0, Unlicense, or MPL-2.0 (Symphonia's audio decoders and `triple_buffer`). egui, its fonts under the Ubuntu Font License, ureq, and webpki-roots (CDLA-Permissive-2.0) are upstream's desktop player's, which VNDegen doesn't build.

`scripts/siglus-engine.sh` writes the notices to `build/siglus/licenses/`: siglus_rs's license, libtheora's, and each crate's license files, or for a crate that ships none, the text in `licenses/` of the license its manifest names.

## Building for Apple platforms

`scripts/siglus-engine.sh build/siglus` builds `vnsiglus` as a dynamic library for iOS devices, the iOS simulator, and macOS (arm64 and x86_64), and wraps it as `build/siglus/VNSiglus.xcframework`. The app links and embeds the framework, and `EngineSiglus` finds `vnsiglus_get_api_v1` in it at run time.

- A framework rather than a static library: the Artemis engine is a Rust static library too, and two in one binary define the same standard-library symbols. The framework's two-level namespace keeps its copy to itself.
- `siglus_scene_vm` is built without its default `desktop` feature, so on macOS it takes the path upstream's iOS host takes, with VNDegen's view instead of a winit window.
- wgpu draws through its Metal backend into the view's `CAMetalLayer`. Shaders are WGSL, which wgpu translates to Metal shading language when it loads them, so nothing compiles code at run time beyond what Metal itself does.
- Sound plays through kira and cpal on Core Audio.
- Set `IPHONEOS_DEPLOYMENT_TARGET` and `MACOSX_DEPLOYMENT_TARGET` to the app's deployment targets. The script rebuilds a target from clean when its deployment target changes.
- The Rust toolchain is pinned to 1.99.1, with the `aarch64-apple-ios`, `aarch64-apple-ios-sim`, `aarch64-apple-darwin`, and `x86_64-apple-darwin` targets.

On Linux, `cargo test -p vnsiglus -p game_fs` in `siglus_rs/` tests VNDegen's interface and the file layer. `cargo run --release -p vnsiglus --bin vnsiglus-run -- <game> <saves> <frames> <out.png> [font]` runs a RealLive game headless through the same C table the app calls, pressing Return every two seconds, and writes its last frame. SiglusEngine needs a view, and `cargo build --release -p siglus_scene_vm -p reallive --features reallive/desktop-player --bins` builds upstream's desktop players, `siglus_engine` and `reallive_player`, for running a game there.

## VNDegen's changes to siglus_rs

- Build without the WMV and WMA decoder: `siglus_scene_vm` depends on `wmv_stub/` in its place.
- Leave out the DT00 and PT00 ports. Those DLLs load as unknown ones, whose calls return 0.
- Take the default font from the host (`text_render::set_default_font`) instead of the embedded MS PGothic.
- Create the renderer from a view on macOS and Linux too (`Renderer::new_from_raw_handles`).
- Put upstream's desktop player behind `siglus_scene_vm`'s `desktop` feature, on by default: its egui windows, its Twitter client and HTTP client (ureq), and the winit pump host. Without it the engine's HTTP requests fail, as on the consoles, and the desktop window icon is blank instead of upstream's image.
- Find RealLive's files whatever case the game uses: `game_fs` looks a path up as written, then matches each missing name against its folder's listing without regard to case, for reads, writes, existence checks and listings. A write to a name that exists in another case replaces that file.
- Let the host mute both engines (`AudioHub::set_muted`, `Sound::set_muted`) while VNDegen's menu is open.
