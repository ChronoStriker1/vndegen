# Shared engine code upstream

The libraries vnvideo, the movie decoder in `vnvideo.[ch]`, builds from `third_party/`. `scripts/video-licenses.sh` writes their notices to `build/video/licenses/`.

| Folder | Upstream | Version | License |
|---|---|---|---|
| `third_party/pl_mpeg/` | https://github.com/phoboslab/pl_mpeg | commit `c871f2be022ece7ef4f64230b4fb8e1fb9eb6023` | MIT |
| `third_party/dr_mp3/` | https://github.com/mackron/dr_libs | v0.7.4 (unreleased), commit `a466dbe88fe2576d46c43877d602f3d66b8561a7` | Public domain (Unlicense) or MIT No Attribution |
| `third_party/ogg/` | https://downloads.xiph.org/releases/ogg/libogg-1.3.6.tar.xz | 1.3.6 | BSD-3-Clause |
| `third_party/theora/` | https://downloads.xiph.org/releases/theora/libtheora-1.2.0.tar.xz | 1.2.0 | BSD-3-Clause |
| `third_party/opus/` | https://github.com/xiph/opus | tag `v1.5.2`, commit `ddbe48383984d56acd9e1ab6a090c54ca6b735a6` | BSD-3-Clause |
| `third_party/libwebm/` | https://github.com/webmproject/libwebm | tag `libwebm-1.0.0.32`, commit `f2a982d748b80586ae53b89a2e6ebbc305848b8c` | BSD-3-Clause |
| `third_party/libvpx/` | https://chromium.googlesource.com/webm/libvpx | tag `v1.15.2`, commit `d168454ecd099805c675d4a98c66f4891373302a` | BSD-3-Clause |

## What is kept, and VNDegen's changes

The source files are unmodified except pl_mpeg's header, which carries the patch below. Only what the decoders need is kept:

- pl_mpeg: the header, with its license as `LICENSE.txt`. `third_party/vnvideo_libs.h` gives its functions a `vnvideo_` prefix, so it links beside the copies NScripter and System 4 compile.
  VNDegen patches it against damaged and crafted movies, found by fuzzing vnvideo (#126). Reapply the patch after an update unless upstream has fixed the same cases:
  - `plm_video_decode_macroblock`: a slice whose first macroblock address falls outside the picture, below 0 or at `mb_size` and past it, stops there. Upstream wrote the decoded block outside the frame planes.
  - `plm_video_process_macroblock`: a motion vector whose source block, half-pixel neighbours included, leaves the reference plane skips the copy. Upstream's bound let it read up to a row past the plane.
  - `plm_audio_decode_header`: a Layer II header with bitrate index 0 (free format) is rejected. Upstream read `PLM_AUDIO_BIT_RATE[-1]`.
  - `plm_video_decode_sequence_header`: a frame rate code of 0 or a reserved code plays at 30 fps. Upstream reported 0 fps, so frame times were infinite and the movie never ended.
  - `plm_demux_decode_time`: the clock's top three bits are shifted as a 64-bit value, not an `int`, which overflowed.
  - MPEG-1 Layer I sound, which upstream rejects (Eushully AGE's battle cut-ins carry it). `plm_audio_decode_header`
    accepts Layer I with its own bitrate table and frame size, `(12 * bitrate / sample rate + padding) * 4` bytes;
    `plm_audio_decode_frame_layer_i` reads the 4-bit allocations, the 6-bit scale factors and the 12 samples per
    subband, requantises them as ISO 11172-3 gives, `2^nb / (2^nb - 1) * (s + 2^(1 - nb))`, and hands each group of
    three to the synthesis filterbank, which is factored out of `plm_audio_decode_frame` unchanged as
    `plm_audio_synthesize`. A Layer I frame yields 384 samples, so `samples.count` and the decoded-sample clock follow
    the frame. Layer II decodes to the same bytes as before, and Layer III still goes to dr_mp3, checked on movies of
    AGE, BSX (MK3 and MK4), EAGLS, YU-RIS, N2System, Tmr-Hiro and Artemis. NScripter's and System 4's copies don't have this change.
  NScripter's identical copy (`Packages/VNDegenKit/Sources/CNScripter/third_party/pl_mpeg.h`) and System 4's (`Packages/VNDegenKit/Sources/CSystem4/third_party/pl_mpeg/pl_mpeg.h`, which also has its own Layer II quantizer fix) carry the same patch.
- dr_mp3: the unmodified header, the same copy NScripter, System 3, Wolf RPG Editor and RGSS vendor, with its license as `LICENSE.txt`. `third_party/vnvideo_libs.h` compiles it with static functions and float output, for MPEG-1 movies whose sound is Layer III.
- libogg: `framing.c`, `bitwise.c`, and their headers. `include/ogg/config_types.h` is VNDegen's, with the types libogg's configure writes for platforms that have `stdint.h`.
- libtheora: the decoder's sources and headers, without the encoder or the x86 and ARM assembly.
- libopus: the 63 files of the CELT and SILK decoders and `opus_decoder.c`, found by linking a decoder-only program against the whole library, and the headers they include. `config.h` is VNDegen's: the float decoder, with C99 variable-length arrays for scratch space.
- libwebm: the unmodified Matroska parser, file reader, element identifiers and notices. The alpha converter uses its EBML reader to stream WebM block additions without keeping the movie's decoded frames.
- libvpx: unmodified upstream source. The alpha converter builds only the VP9 decoder, with ARM NEON intrinsics or portable C on Intel, without assembly, newer ARM instruction sets, encoders, examples or tests. Each destination has its own generated configuration under `build/alpha/`. `scripts/alpha-video.sh bundle` packages the static framework for Mac, iOS and the simulator.
