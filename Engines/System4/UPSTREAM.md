# System 4 runtime: third-party code

VNDegen's System 4 runtime is VNDegen's own code (MIT), in `Packages/VNDegenKit/Sources/CSystem4`. It was written
clean-room; `docs/system4-cleanroom.md` has the provenance. It vendors these permissively licensed libraries in
`CSystem4/third_party/`:

| File | Project | Version | License |
|---|---|---|---|
| `stb/stb_image.h` | https://github.com/nothings/stb | v2.30, commit 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5, the same file the NScripter runtime vendors | MIT or public domain |
| `stb/stb_vorbis.c` | https://github.com/nothings/stb | same | MIT or public domain |
| `pl_mpeg/pl_mpeg.h` | https://github.com/phoboslab/pl_mpeg | commit c871f2be022ece7ef4f64230b4fb8e1fb9eb6023 | MIT |

Changes:
- `pl_mpeg.h`: one line in `plm_audio_read_samples`, marked "VNDegen patch". The MP2 requantization step uses
  `65536 / levels` as MPEG-1 Layer II defines it, where upstream used `65536 / (levels + 1)` and played coarse
  subbands up to 25% too quietly. `pl_mpeg/LICENSE` notes it.
- pl_mpeg's and stb_vorbis's functions are compiled under `s4_` names (`core/plm_names.h`,
  `core/stb_vorbis_names.h`) and stb_image as static, because the NScripter runtime links its own copies into the
  same app.

The runtime links zlib from the system. On Apple systems it decodes WebP with ImageIO and draws fonts with CoreText.
The Linux test host (`Engines/System4/host/`, not part of the app) uses the system's FreeType and libwebp instead,
and `stb_image_write.h` from the Wolf RPG Editor runtime's copy of stb.

`scripts/app-licenses.sh` copies the stb and pl_mpeg notices into the app's Licenses page.
