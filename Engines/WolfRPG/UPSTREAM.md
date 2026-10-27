# Wolf RPG Editor runtime upstream

The runtime in `runtime/` is VNDegen's own code under VNDegen's MIT license. `docs/wolfrpg-cleanroom.md` lists the sources its knowledge of the formats came from.

## Vendored code

| Folder | Upstream | Commit | License |
|---|---|---|---|
| `runtime/third_party/stb/` (`stb_image.h`, `stb_image_write.h`, `stb_vorbis.c`) | https://github.com/nothings/stb | `003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5` | MIT or public domain, at the user's choice |

The files are unmodified.

## Linked libraries

SDL3 and SDL3_ttf (zlib license), with FreeType (FreeType License) and HarfBuzz (MIT) under SDL3_ttf.
