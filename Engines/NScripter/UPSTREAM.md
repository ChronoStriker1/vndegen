# NScripter runtime: third-party code

VNDegen's NScripter runtime is VNDegen's own code (MIT), in `Packages/VNDegenKit/Sources/CNScripter`. It vendors these
permissively licensed single-file libraries, unmodified, in `CNScripter/third_party/`:

| File | Project | Version | License |
|---|---|---|---|
| `stb_image.h` | https://github.com/nothings/stb | v2.30, commit 003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5 | MIT or public domain |
| `stb_vorbis.c` | https://github.com/nothings/stb | same commit | MIT or public domain |
| `pl_mpeg.h` | https://github.com/phoboslab/pl_mpeg | commit c871f2be022ece7ef4f64230b4fb8e1fb9eb6023 | MIT |

The Linux test host (`Engines/NScripter/host/`, not part of the app) also uses `stb_image_write.h` from the same stb
commit, and the system's FreeType.

`scripts/app-licenses.sh` copies their notices into the app's Licenses page.
