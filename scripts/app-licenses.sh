#!/usr/bin/env bash
# Writes build/app/licenses/, the notices of the third-party code in VNDegen's own sources, one text file each. The
# runtimes write their own: see scripts/licenses/README.md. The app uses no Swift package from outside the repository,
# so a dependency added to Package.swift or project.yml fails here until it has a notice of its own.
#   scripts/app-licenses.sh
set -euo pipefail
cd "$(dirname "$0")/.."
kit=Packages/VNDegenKit
if grep -n -E '\.package\(|^ +url:' "$kit/Package.swift" project.yml; then
  echo "add the license of the Swift package above to scripts/app-licenses.sh" >&2
  exit 1
fi
out=build/app/licenses
rm -rf "$out.part" && mkdir -p "$out.part"
# The rest of the bundled code is VNDegen's: the JavaScript shims and the Ren'Py launcher.
cp "$kit/Sources/WebRuntime/vorbis.wasm.LICENSE" "$out.part/Ogg Vorbis decoder (libvorbis, libogg).txt"
cp "$kit/Sources/ArchiveImport/CArchive/LICENSE-zlib.txt" "$out.part/zlib (Deflate64 decoder).txt"
cp "$kit/Sources/CArtemis/third_party/LICENSE-stb_vorbis.txt" "$out.part/stb_vorbis (Artemis audio decoder).txt"
cp "$kit/Sources/RuntimePackages/LICENSE-innoextract.txt" "$out.part/innoextract (Inno Setup reader).txt"
cp "$kit/Sources/RuntimePackages/LICENSE-LZMA-SDK.txt" "$out.part/LZMA SDK (installer decoder).txt"
cp "$kit/Sources/CNScripter/third_party/LICENSE-stb.txt" "$out.part/stb_image and stb_vorbis (NScripter images and Ogg).txt"
cp "$kit/Sources/CNScripter/third_party/LICENSE-pl_mpeg.txt" "$out.part/pl_mpeg (NScripter movies).txt"
cp "$kit/Sources/CNScripter/third_party/LICENSE-dr_mp3.txt" "$out.part/dr_mp3 (NScripter MP3).txt"
cp "$kit/Sources/CNScripter/third_party/LICENSE-lua.txt" "$out.part/Lua (NScripter NSLua scripts).txt"
cp "$kit/Sources/CNScripter/third_party/LICENSE-bzip2.txt" "$out.part/bzip2 (NScripter NBZ sounds).txt"
cp "$kit/Sources/CWolfRPG/third_party/LICENSE-stb.txt" "$out.part/stb_image and stb_vorbis (Wolf RPG Editor images and Ogg).txt"
cp "$kit/Sources/CWolfRPG/third_party/LICENSE-dr_libs.txt" "$out.part/dr_mp3 and dr_wav (Wolf RPG Editor MP3 and WAV).txt"
cp "$kit/Sources/CSystem3/third_party/LICENSE-stb_truetype.txt" "$out.part/stb_truetype (AliceSoft System 3 text).txt"
cp "$kit/Sources/CSystem3/third_party/LICENSE-dr_mp3.txt" "$out.part/dr_mp3 (AliceSoft System 3 CD music).txt"
rm -rf "$out" && mv "$out.part" "$out"
