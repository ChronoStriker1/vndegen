# Wolf RPG Editor runtime upstream

The runtime in `Packages/VNDegenKit/Sources/CWolfRPG/` is VNDegen's own code under VNDegen's MIT license.
`docs/wolfrpg-cleanroom.md` lists the sources its knowledge of the formats came from.

## Vendored code

All in `Packages/VNDegenKit/Sources/CWolfRPG/third_party/`, unmodified, each file as of the commit that last changed it
upstream. The runtime's sources include them directly; `scripts/app-licenses.sh` copies their notices into the app.

| Files | Upstream | Commit | License |
|---|---|---|---|
| `stb/stb_image.h`, `stb/stb_image_write.h`, `stb/stb_vorbis.c` | https://github.com/nothings/stb | `003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5` | MIT or public domain |
| `dr_libs/dr_mp3.h` | https://github.com/mackron/dr_libs | `bad753d5ade4f30e40fcf80a757210b650681f8f` | Public domain or MIT No Attribution |
| `dr_libs/dr_wav.h` | https://github.com/mackron/dr_libs | `a466dbe88fe2576d46c43877d602f3d66b8561a7` | Public domain or MIT No Attribution |

MIDI plays through VNDegen's shared MIDI player, `Engines/Shared/vnmidi.h` (`docs/midi.md`).

`stb_image_write.h` is used only by the Linux test tools. The decoders are compiled into one file each, static or under
names of their own, so they don't clash with the copies other engines in the app compile.

## Linked libraries

On Apple systems the runtime uses the system's CoreText and CoreGraphics for text. The Linux test host links FreeType
(FreeType License) and SDL3 (zlib, for `wolfaudio`'s device output only).
