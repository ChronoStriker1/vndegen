#!/usr/bin/env bash
# Runs on a Mac. Installs the Ren'Py runtimes VNDegen bundles into <dest>/8 and <dest>/7. Each is an official Ren'Py
# SDK trimmed to what runs games on macOS: renpy.py, renpy/, the standard library, and the macOS python and
# librenpython.dylib.
#   scripts/renpy-runtime.sh build/renpy
# The macOS build phase in project.yml runs this, then copies and signs the result into the app. SDK downloads are
# checked against the SHA-256 sums below and cached in ~/Library/Caches/VNDegen/renpy-sdk. A runtime already installed
# by this version of the script is left alone.
# Ren'Py 7.8.7 gets one patch. Its loader compiles a game's python-packages modules from text, which Python 2 rejects
# when the module declares an encoding. 7.5.1 compiled bytes, and the patch restores that.
set -euo pipefail
dest=${1:?destination folder, such as build/renpy}
cache=${VNDEGEN_RENPY_CACHE:-$HOME/Library/Caches/VNDegen/renpy-sdk}
stamp=$(shasum -a 256 "$0" | cut -c1-16)

install() { # <version> <sha256> <folder> <python line>
  local version=$1 sum=$2 folder=$3 py=$4
  local out="$dest/$folder" tarball="$cache/renpy-$version-sdk.tar.bz2"
  [ "$(cat "$out/.vndegen-runtime" 2>/dev/null)" = "$version $stamp" ] && return
  echo "Ren'Py $version -> $out"
  if ! echo "$sum  $tarball" | shasum -a 256 -c --status 2>/dev/null; then
    mkdir -p "$cache"
    curl -fsSL "https://www.renpy.org/dl/$version/renpy-$version-sdk.tar.bz2" -o "$tarball.part"
    echo "$sum  $tarball.part" | shasum -a 256 -c --status || { echo "Ren'Py $version SDK checksum mismatch" >&2; exit 1; }
    mv "$tarball.part" "$tarball"
  fi
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

install 8.5.3 eb0a9be7f0fb13632fe25ceade9a8bed5a1b4d6b6e83bd19eeeb29e1a1bb4a45 8 py3
install 7.8.7 65466068af7c181a143f13d84b0ddf7a6aa58dd8d26e7ccd97cdcadecabcae5a 7 py2
