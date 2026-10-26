#!/usr/bin/env bash
# Runs on a Mac. Installs the Ren'Py runtimes VNDegen bundles into <dest>/8 and <dest>/7. Each is an official Ren'Py
# SDK trimmed to what runs games on macOS: renpy.py, renpy/, the standard library, and the macOS python and
# librenpython.dylib.
#   scripts/renpy-runtime.sh build/renpy
#   scripts/renpy-runtime.sh build/renpy ios
# The ios argument installs <dest>/8 and, in <dest>/ios, the native side of Ren'Py 8 for iPhone and iPad from Ren'Py's
# renios package: RenPyRuntime.xcframework, one static library per platform made of renios's prebuilt libraries, and
# MetalANGLE.xcframework. iOS runs Ren'Py 7 games on Ren'Py 8, so it needs no Ren'Py 7 runtime.
# The build phase in project.yml runs this, then copies the result into the app. Downloads are checked against the
# SHA-256 sums below and cached in ~/Library/Caches/VNDegen/renpy-sdk. A runtime already installed by this version of
# the script is left alone.
# Ren'Py 7.8.7 gets one patch. Its loader compiles a game's python-packages modules from text, which Python 2 rejects
# when the module declares an encoding. 7.5.1 compiled bytes, and the patch restores that.
set -euo pipefail
dest=${1:?destination folder, such as build/renpy}
platform=${2:-mac}
cache=${VNDEGEN_RENPY_CACHE:-$HOME/Library/Caches/VNDegen/renpy-sdk}
stamp=$(shasum -a 256 "$0" | cut -c1-16)

fetch() { # <file name> <sha256> <url>
  local file="$cache/$1"
  echo "$2  $file" | shasum -a 256 -c --status 2>/dev/null && return
  mkdir -p "$cache"
  curl -fsSL "$3" -o "$file.part"
  echo "$2  $file.part" | shasum -a 256 -c --status || { echo "$1 checksum mismatch" >&2; exit 1; }
  mv "$file.part" "$file"
}

install() { # <version> <sha256> <folder> <python line>
  local version=$1 sum=$2 folder=$3 py=$4
  local out="$dest/$folder" tarball="$cache/renpy-$version-sdk.tar.bz2"
  [ "$(cat "$out/.vndegen-runtime" 2>/dev/null)" = "$version $stamp" ] && return
  echo "Ren'Py $version -> $out"
  fetch "renpy-$version-sdk.tar.bz2" "$sum" "https://www.renpy.org/dl/$version/renpy-$version-sdk.tar.bz2"
  local work sdk
  work=$(mktemp -d)
  sdk="$work/renpy-$version-sdk"
  tar -xjf "$tarball" -C "$work"
  # Copies keep their dates, so the SDK's compiled Python stays current and Python never recompiles inside the app.
  rm -rf "$out.part" && mkdir -p "$out.part/lib/$py-mac-universal"
  cp -Rp "$sdk/renpy.py" "$sdk/renpy" "$out.part/"
  cp -Rp "$sdk/lib/python"* "$out.part/lib/"
  cp -p "$sdk/lib/$py-mac-universal/python" "$sdk/lib/$py-mac-universal/librenpython.dylib" "$out.part/lib/$py-mac-universal/"
  rm -rf "$work"
  [ "$folder" = 7 ] && patch_loader "$out.part/renpy/loader.py"
  echo "$version $stamp" > "$out.part/.vndegen-runtime"
  rm -rf "$out" && mv "$out.part" "$out"
}

patch_loader() {
  python3 -I - "$1" <<'EOF'
import sys
path = sys.argv[1]
source = open(path).read()
anchor = "                code = compile(source, filename, 'exec', renpy.python.old_compile_flags, 1)"
assert source.count(anchor) == 1
fix = '                source = source.encode("raw_unicode_escape").replace(b"\\r", b"")\n'
open(path, "w").write(source.replace(anchor, fix + anchor))
EOF
  rm -f "${1%.py}.pyo" "${1%.py}.pyc"
}

# The libraries renios's own Xcode project links, minus librenpython, whose launcher VNDegen replaces.
ios_libraries=(renpy python3.12 assimp avformat avcodec swscale swresample avutil SDL2_image SDL2 avif aom yuv turbojpeg
  png16 webp harfbuzz brotlidec brotlicommon fribidi freetype ffi ssl crypto lzma bz2 z)

install_ios() { # <version> <sha256>
  local version=$1 sum=$2 out="$dest/ios"
  [ "$(cat "$out/.vndegen-runtime" 2>/dev/null)" = "$version $stamp" ] && return
  echo "Ren'Py $version renios -> $out"
  fetch "renpy-$version-renios.zip" "$sum" "https://www.renpy.org/dl/$version/renpy-$version-renios.zip"
  local work prebuilt config library libraries
  work=$(mktemp -d)
  unzip -q "$cache/renpy-$version-renios.zip" 'renios/prototype/prebuilt/*' 'renios/prototype/Frameworks/*' -d "$work"
  prebuilt="$work/renios/prototype/prebuilt"
  # renios builds simulator libraries as debug and device libraries as release.
  for config in debug release; do
    libraries=()
    for library in "${ios_libraries[@]}"; do
      [ -f "$prebuilt/$config/lib$library.a" ] || { echo "renios has no lib$library.a" >&2; exit 1; }
      libraries+=("$prebuilt/$config/lib$library.a")
    done
    mkdir -p "$work/$config"
    libtool -static -no_warning_for_no_symbols -o "$work/$config/libRenPyRuntime.a" "${libraries[@]}"
  done
  rm -rf "$out.part" && mkdir -p "$out.part"
  xcodebuild -create-xcframework -library "$work/release/libRenPyRuntime.a" -library "$work/debug/libRenPyRuntime.a" \
    -output "$out.part/RenPyRuntime.xcframework" >/dev/null
  cp -Rp "$work/renios/prototype/Frameworks/MetalANGLE.xcframework" "$out.part/"
  rm -rf "$work"
  echo "$version $stamp" > "$out.part/.vndegen-runtime"
  rm -rf "$out" && mv "$out.part" "$out"
}

install 8.5.3 eb0a9be7f0fb13632fe25ceade9a8bed5a1b4d6b6e83bd19eeeb29e1a1bb4a45 8 py3
case "$platform" in
  mac) install 7.8.7 65466068af7c181a143f13d84b0ddf7a6aa58dd8d26e7ccd97cdcadecabcae5a 7 py2 ;;
  ios) install_ios 8.5.3 c4fae153e8276ed0faed5e84ea3e0b7c4bf337f0e3208e9130c6a41748a83b2b ;;
  *) echo "unknown platform $platform" >&2; exit 2 ;;
esac
