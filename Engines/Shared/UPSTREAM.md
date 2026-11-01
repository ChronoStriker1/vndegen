# Shared engine code upstream

The libraries vnvideo, the movie decoder in `vnvideo.[ch]`, builds from `third_party/`. `scripts/video-licenses.sh` writes their notices to `build/video/licenses/`.

| Folder | Upstream | Version | License |
|---|---|---|---|
| `third_party/pl_mpeg/` | https://github.com/phoboslab/pl_mpeg | commit `c871f2be022ece7ef4f64230b4fb8e1fb9eb6023` | MIT |
| `third_party/ogg/` | https://downloads.xiph.org/releases/ogg/libogg-1.3.6.tar.xz | 1.3.6 | BSD-3-Clause |
| `third_party/theora/` | https://downloads.xiph.org/releases/theora/libtheora-1.2.0.tar.xz | 1.2.0 | BSD-3-Clause |
| `third_party/opus/` | https://github.com/xiph/opus | tag `v1.5.2`, commit `ddbe48383984d56acd9e1ab6a090c54ca6b735a6` | BSD-3-Clause |
| `third_party/libwebm/` | https://github.com/webmproject/libwebm | tag `libwebm-1.0.0.32`, commit `f2a982d748b80586ae53b89a2e6ebbc305848b8c` | BSD-3-Clause |
| `third_party/libvpx/` | https://chromium.googlesource.com/webm/libvpx | tag `v1.15.2`, commit `d168454ecd099805c675d4a98c66f4891373302a` | BSD-3-Clause |

## What is kept, and VNDegen's changes

The source files are unmodified. Only what the decoders need is kept:

- pl_mpeg: the header, with its license as `LICENSE.txt`. `third_party/vnvideo_libs.h` gives its functions a `vnvideo_` prefix, so it links beside the copies NScripter and System 4 compile.
- libogg: `framing.c`, `bitwise.c`, and their headers. `include/ogg/config_types.h` is VNDegen's, with the types libogg's configure writes for platforms that have `stdint.h`.
- libtheora: the decoder's sources and headers, without the encoder or the x86 and ARM assembly.
- libopus: the 63 files of the CELT and SILK decoders and `opus_decoder.c`, found by linking a decoder-only program against the whole library, and the headers they include. `config.h` is VNDegen's: the float decoder, with C99 variable-length arrays for scratch space.
- libwebm: the unmodified Matroska parser, file reader, element identifiers and notices. The alpha converter uses its EBML reader to stream WebM block additions without keeping the movie's decoded frames.
- libvpx: unmodified upstream source. The alpha converter builds only the VP9 decoder, with ARM NEON intrinsics or portable C on Intel, without assembly, newer ARM instruction sets, encoders, examples or tests. Each destination has its own generated configuration under `build/alpha/`. `scripts/alpha-video.sh bundle` packages the static framework for Mac, iOS and the simulator.
