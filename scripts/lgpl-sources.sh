#!/usr/bin/env bash
set -euo pipefail
version=${1:?version, such as 0.42.0}
out=${2:?output folder}
root=$(cd "$(dirname "$0")/.." && pwd)
cache=${VNDEGEN_LGPL_CACHE:-$HOME/.cache/vndegen/lgpl-sources}
renios_url=https://www.renpy.org/dl/8.5.3/renpy-8.5.3-renios.zip
renios_sum=c4fae153e8276ed0faed5e84ea3e0b7c4bf337f0e3208e9130c6a41748a83b2b
renpy_tag=8.5.3.26051504
renpy_commit=39895c1e017f0b36ffea2447d97eccd69d76ee1c

sources=(
  "ffmpeg-4.3.1.tar.gz|45035f15d6f192772de2309c846e1d60472694f479679354a39c699719e53772|https://github.com/renpy/renpy-build/raw/renpy-8.5.3.26051504/source/ffmpeg-4.3.1.tar.gz|FFmpeg 4.3.1, in Ren'Py 8.5.3 and 7.8.7 (renpy-build's source folder)"
  "fribidi-1.0.7.tar.bz2|5ab5f21e9f2fc57b4b40f8ea8f14dba78a5cc46d9cf94bc5e00a58e6886a935d|https://github.com/renpy/renpy-build/raw/renpy-8.5.3.26051504/source/fribidi-1.0.7.tar.bz2|GNU FriBidi 1.0.7, in Ren'Py 8.5.3 and 7.8.7 (renpy-build's source folder)"
  "pygame_sdl2-renpy-7.8.7.25031702.tar.gz|b11067344e4a4c22aec8e50fb000132fa8537c714ac32a9f525915a756143182|https://github.com/renpy/pygame_sdl2/archive/refs/tags/renpy-7.8.7.25031702.tar.gz|pygame_sdl2 for Ren'Py 7.8.7, whose alphablit.c and surface.h come from Pygame"
  "chardet-5.2.0.tar.gz|1b3b6ff479a8c414bc3fa2c0852995695c4a026dcd6d0633b2dd092ca39c1cf7|https://files.pythonhosted.org/packages/f3/0d/f7b6ab21ec75897ed80c17d79b15951a719226b9fababf1e40ea74d69079/chardet-5.2.0.tar.gz|chardet 5.2.0, in Ren'Py 8.5.3's Python library"
  "chardet-3.0.4.tar.gz|84ab92ed1c4d4f16916e05906b6b75a6c0fb5db821cc65e70cbd64a3e2a5eaae|https://files.pythonhosted.org/packages/fc/bb/a5768c230f9ddb03acc9ef3f0d4a3cf93462473795d18e9535498c8f929d/chardet-3.0.4.tar.gz|chardet 3.0.4, in Ren'Py 7.8.7's Python library"
  "ffmpeg-n8.1.2.tar.gz|9fd092511605bbebafe095ea6d38d9e40f34d12f7386e1258372df8be0576eb7|https://github.com/ffmpeg/ffmpeg/archive/n8.1.2.tar.gz|FFmpeg 8.1.2, in the KiriKiri engine's FFmpeg.framework, as vcpkg fetches it"
)

mkdir -p "$root/build"
work=$(mktemp -d "$root/build/lgpl-sources.XXXXXX")
trap 'rm -rf "$work"' EXIT
folder="VNDegen-$version-LGPL-sources"
mkdir -p "$cache" "$work/$folder" "$out"

for entry in "${sources[@]}"; do
  IFS='|' read -r file sum url _ <<<"$entry"
  if ! echo "$sum  $cache/$file" | sha256sum -c --status >/dev/null 2>&1; then
    curl -fsSL "$url" -o "$cache/$file.part"
    echo "$sum  $cache/$file.part" | sha256sum -c --status || { echo "$file checksum mismatch" >&2; exit 1; }
    mv "$cache/$file.part" "$cache/$file"
  fi
  cp "$cache/$file" "$work/$folder/"
done

git -C "$work" init -q renpy
git -C "$work/renpy" fetch -q --depth 1 --filter=blob:none https://github.com/renpy/renpy "refs/tags/$renpy_tag"
[ "$(git -C "$work/renpy" rev-parse 'FETCH_HEAD^{commit}')" = "$renpy_commit" ] || { echo "Ren'Py's $renpy_tag tag moved" >&2; exit 1; }
git -C "$work/renpy" archive --format=tar --prefix="renpy-$renpy_tag-pygame/" "$renpy_commit" src/pygame renpy/pygame |
  gzip -n > "$work/$folder/renpy-$renpy_tag-pygame.tar.gz"

tar -C "$root" -czf "$work/$folder/vndegen-ffmpeg-port.tar.gz" --owner=0 --group=0 --sort=name Engines/KiriKiri/ports/ffmpeg

{
  echo "Source code of the GNU LGPL libraries in VNDegen $version"
  echo
  echo "VNDegen's own source, with the scripts that build these libraries into the app, is at"
  echo "https://github.com/ChronoStriker1/vndegen (tag v$version). docs/relinking.md there says how to rebuild each one."
  echo
  for entry in "${sources[@]}"; do
    IFS='|' read -r file sum url what <<<"$entry"
    echo "$file"
    echo "  $what"
    echo "  from $url"
    echo "  SHA-256 $sum"
  done
  echo "renpy-$renpy_tag-pygame.tar.gz"
  echo "  src/pygame and renpy/pygame of Ren'Py $renpy_tag (commit $renpy_commit), the pygame_sdl2 that Ren'Py 8.5.3"
  echo "  carries; alphablit.c and surface.h come from Pygame"
  echo "vndegen-ffmpeg-port.tar.gz"
  echo "  Engines/KiriKiri/ports/ffmpeg: the vcpkg port, with its patches and configure options, that builds FFmpeg 8.1.2"
  echo
  echo "The other libraries in Ren'Py's iOS runtime, which VNDegen links with these into RenPyRuntime.framework, are the"
  echo "prebuilt static libraries in Ren'Py's renios package:"
  echo "  $renios_url"
  echo "  SHA-256 $renios_sum"
} > "$work/$folder/README.txt"

tar -C "$work" -cf "$out/$folder.tar" --owner=0 --group=0 --sort=name "$folder"
ls -l "$out/$folder.tar"
