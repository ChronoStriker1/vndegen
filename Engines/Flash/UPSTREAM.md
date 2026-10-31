# Flash upstream

VNDegen plays Flash games in Ruffle, a Flash Player emulator written in Rust, built for the web and run in the same
WebKit web view as the RPG Maker MV/MZ and TyranoScript games.

## Source

| What | Upstream | Version |
|---|---|---|
| Ruffle | https://github.com/ruffle-rs/ruffle | commit `07d3e430631333f80f754df01a968bdcd80d98c5` (2026-10-27), web package 0.7.1 |

`scripts/ruffle-web.sh` builds it on Linux and writes the result to `ruffle/`, which is committed, so the app's Mac
build needs no Rust, Java, or Node. `scripts/ruffle-runtime.sh` checks `ruffle/` against `ruffle/SHA256SUMS` at every
app build and stages it for the app bundle.

## Toolchain

| Tool | Version |
|---|---|
| Rust | 1.99.1, with the `wasm32-unknown-unknown` target |
| wasm-bindgen-cli | 0.2.127, the version Ruffle's `Cargo.lock` pins |
| binaryen (`wasm-opt`) | version_133 |
| Java | 21 (Eclipse Temurin 21.0.13), for Ruffle's AS3 library |
| Node | 26.12.1, with npm 12.2.1 |

## Build output

| File | SHA-256 |
|---|---|
| `ruffle/ruffle.js` | `efadfd558d756887b557b670d5a9faabad2543933a4377be019871f9c4fbed0b` |
| `ruffle/ruffle_web_bg.VWXCL2CU.wasm` | `9314c4d69c63c55db86857df5180b340c1fdf84563b6e5e1b3bd20b77ebe6d33` |

The build is reproducible. The script remaps the source tree to `/ruffle` and Cargo's home to `/cargo` in the wasm
module's file names, and pins Ruffle's version seal, the version text and build date it would otherwise take from the
clock, to the commit's own. `scripts/ruffle-web.sh --check` builds in a scratch folder and compares the result with
`ruffle/SHA256SUMS`. A clean build from a fresh clone in another folder matched both sums. A different Rust,
wasm-bindgen, or binaryen version gives different bytes, which is why the script refuses to run with any other.

The script builds Ruffle's default web package: one wasm module that uses the WebAssembly extensions Safari has
supported since iOS 16.4 and macOS 13.3 (bulk memory, SIMD, reference types, and the rest), without the fallback module
for browsers that lack them.

## Patches

`patches/` holds VNDegen's changes, applied in order on top of the commit above.

- `0001-avm2-dispatch-unload-for-content-that-left-the-loader.patch`: `LoaderInfo` sends `unload` when its content
  is unloaded even if the movie moved that content out of the `Loader` first, as Flash Player does. Ruffle sent it only
  while the content was still the Loader's child. Test Game 127 moves each scene's movie into its own container, then
  waits for `unload` before it loads the next scene, and stopped on a black screen after its opening without this.
- `0002-web-list-a-movies-own-context-menu-items-for-the-host-page.patch`: the player element gets
  `movieContextMenuItems()` and `runContextMenuItem(index)`, through a new `prepare_movie_context_menu` in the wasm
  module that prepares the context menu as a right click does and lists only the items the movie added, without the
  built-in ones such as Play and Quality. VNDegen turns Ruffle's context menu off, because a right click belongs to
  the game, and lists these items in its Game menu instead (`flash-page.js`). Ruffle kept the list private to its
  player. Usashitsuke (Test Game 126) puts Return to title and Quit there.

## Licenses

Ruffle is under MIT or Apache-2.0. Everything its web build contains is under a permissive license:

- 221 third-party Rust crates in the wasm module, under MIT, Apache-2.0, BSD, ISC, Zlib, 0BSD, Unlicense, and
  Unicode-3.0, except the five symphonia crates, Ruffle's MP3 and AAC decoders, which are under MPL-2.0 and used
  unmodified.
- `self_cell` is under "Apache-2.0 OR GPL-2.0-only". VNDegen takes it under Apache-2.0.
- The VP6 video decoder from NihAV, relicensed by its author under MIT for Ruffle.
- JavaScript bundled into `ruffle.js`: `@fluent/bundle` and `@fluent/langneg` (Apache-2.0), `wasm-feature-detect`
  (Apache-2.0), and `tsx-dom` and `tsx-dom-types` (MIT).
- Ruffle's fallback device font, a subset of Noto Sans and Noto Sans Math compiled into the wasm module, under the SIL
  Open Font License 1.1.
- Ruffle's AS3 library (`playerglobal`) is Ruffle's own code. The Apache Flex compiler that builds it runs at build
  time only and isn't shipped.

The script writes the notices to `ruffle/licenses/`: Ruffle's own, each Rust crate's license files, the JavaScript
packages', and the texts in `licenses/` for what the source tree lacks: the Noto fonts' OFL, and the MPL-2.0 with where
symphonia's source is. A few crates and packages name their license but ship no file. Their entries name the license
and authors and refer to the standard MIT and Apache-2.0 texts in Ruffle's notice.
