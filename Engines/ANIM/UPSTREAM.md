# ANIM engine upstream

VNDegen draws the ANIM engine's E-mote characters with eluna, a Rust reimplementation of M2's E-mote runtime, behind a
C interface of VNDegen's own. Everything else in the ANIM runtime (`Packages/VNDegenKit/Sources/CANIM`) is VNDegen's.

## Sources

| Folder | Upstream | Commit |
|---|---|---|
| `third_party/eluna_rs/` | https://github.com/xmoezzz/eluna, its `crates/eluna` | `9a000293968392daa34c83f82b991f7ba25ccc29` (2026-09-25) |
| `vnemote/` | VNDegen's own: the C interface in `Packages/VNDegenKit/Sources/CANIM/include/vnemote.h` | |
| `vendor/` | The crates eluna_rs builds with, from crates.io through `cargo vendor` (see `vnemote/Cargo.lock`) | |

`third_party/eluna_rs/` holds upstream's `Cargo.toml` and `src/` unmodified. The same source, at the same upstream
state, is also vendored inside `Engines/Siglus/siglus_rs/vendor/eluna_rs`; the two copies are kept separate so each
engine can move to a newer eluna on its own.

VNDegen's changes to eluna's files: none.

## Licenses

eluna declares its licenses in each crate's `Cargo.toml` (`license = ...`); its repository has no license file. The
library crate, `eluna_rs`, is MPL-2.0 (`third_party/eluna_rs/LICENSE-MPL-2.0`), a file-level copyleft: the source of
every MPL-covered file VNDegen ships is `third_party/eluna_rs/`. The player and the PSB extractor are MIT OR Apache-2.0
(`third_party/eluna_rs/LICENSE-MIT.txt`, `LICENSE-APACHE-2.0.txt`). VNDegen doesn't build them. `vnemote`'s sprite
vertex code and the ANIM runtime's E-mote rasterizer (`CANIM/emote.c`: mesh vertices, blend modes, stencil masks)
follow the player's software and GPU paths and are used under its MIT license.

The vendored crates are MIT, Apache-2.0, BSD-2-Clause, Zlib, 0BSD or Unicode-3.0, each also offering MIT or Apache-2.0
where it offers several (`cargo metadata` lists each one's license). `r-efi`, offered under MIT, Apache-2.0 or
LGPL-2.1-or-later, is used under MIT and only builds for UEFI targets, which VNDegen doesn't.

## Building

`cargo build --release --offline --locked` in `vnemote/` builds from `vendor/` without the network
(`vnemote/.cargo/config.toml`). The Linux hosts in `host/` link the static library. Apple builds wrap the dynamic
library as a framework, as `scripts/siglus-engine.sh` does for siglus_rs, and the runtime finds
`vnemote_get_api_v1` at run time; without it, characters aren't drawn.
