# ANIM engine upstream

VNDegen draws the ANIM engine's E-mote characters with eluna, a Rust reimplementation of M2's E-mote runtime, behind a
C interface of VNDegen's own. Everything else in the ANIM runtime (`Packages/VNDegenKit/Sources/CANIM`) is VNDegen's.

## Sources

| Folder | Upstream | Commit |
|---|---|---|
| `third_party/eluna_rs/` | https://github.com/xmoezzz/eluna, its `crates/eluna` | `9a000293968392daa34c83f82b991f7ba25ccc29` (2026-09-25) |
| `vnemote/` | VNDegen's own: the C interface in `Packages/VNDegenKit/Sources/CANIM/include/vnemote.h` | |
| `vendor/` | The crates eluna_rs builds with, from crates.io through `cargo vendor` (see `vnemote/Cargo.lock`) | |

`third_party/eluna_rs/` holds upstream's `Cargo.toml` and `src/`, with the changes below. The same source, at the
unmodified upstream state, is also vendored inside `Engines/Siglus/siglus_rs/vendor/eluna_rs`; the two copies are
kept separate so each engine can move to a newer eluna on its own.

VNDegen's changes to eluna's files, each found by comparing the ANIM games' characters with the PC engine run under
Wine, from saves both load (`docs/anim.md`):

- `src/psb/mod.rs`: PSB version 4 headers carry three more words after the checksum, the offsets of the extra
  resource offset and length arrays and of the extra resource data, encrypted with the header's key stream at the
  point where the body's begins. They are decrypted and read, and the extra resources loaded, so mesh combinators
  (`rawMeshList: extra_resource(n)`) find their meshes; before, eluna read none and took the index as an ordinary
  resource. `PsbDecryptor` derives `Clone` for this.
- `src/emote.rs`:
  - A mesh combinator's `rawMeshList` that names an extra resource reads the extra resource.
  - A layer whose `parameterize` is `null` samples its motion's time, like a layer without the field, instead of
    time 0, and a motion whose own `parameterize` names a parameter runs at that parameter's time. Expression parts
    such as the eyelids, the mouths and the blush follow their variables.
  - A sprite's centre is its icon's origin offset plus half its size: `originX` and `originY` count from the icon's
    top left. Face outlines, cut into tiles, were half a tile off.
  - A layer on a `type 0` key isn't drawn, nor are its children, and a one-shot motion stops just before its final
    key instead of on it.
  - Sprites carry their own mesh patch (`local_mesh`) and their ancestors' mesh chain (`mesh_chain`), and
    `EmoteMeshPatch::warp_local` and `MeshChainEntry::warp_world_point` are public, so a renderer can deform each
    sprite in its owner's coordinates. `EmoteStaticScene::clear_frame_history` drops the held frame state.
  - A mesh combinator's mesh is its whole list: the first variable's mesh, which is stored absolute, plus each later
    variable's mesh, stored as a displacement. That mesh deforms the layer and, through the mesh chain, its
    children. Before, the later variables reached only children's own meshes and not the chain, so turns
    (`head_LR`, `body_LR`) and nods (`head_UD`, `body_UD`) barely moved a character. eluna also read the first mesh
    as a displacement when the layer's `parameterize` named another variable, which pushed such layers out of shape
    (an arm, a neckline) once that variable moved. The data has the first mesh absolute in every case.
  - The test of a one-shot motion's end time follows the change above, and a test covers the mesh combinator.
- `src/sdk.rs`: `EmoteRuntime::reset_frame_history`, for a character whose variables are set at once, as when a save
  loads, so held keys don't keep the pictures of the default pose.

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
