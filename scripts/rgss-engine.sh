#!/usr/bin/env bash
# Runs on a Mac. Builds the RGSS engine (RPG Maker XP, VX and VX Ace) from Engines/RGSS:
#   <dest>/RGSS.xcframework   the framework VNDegen embeds: iOS device and simulator, and macOS, all arm64. It holds
#                             mkxp-z's libretro core, ANGLE, and VNDegen's frontend (Engines/RGSS/host)
#   <dest>/licenses/          the core's, ANGLE's, and their dependencies' licenses, one text file each, plus where
#                             to get the source of this build
#   scripts/rgss-engine.sh build/rgss
# The core comes from the pinned commit of mkxp-z's libretro branch below, with Engines/RGSS/patches applied. ANGLE's
# Meson build comes from a pinned mkxp-z commit too. Builds happen in two stages, as the core's own libretro/README.md
# describes: stage 1 compiles Ruby to WebAssembly and back to C, the same for every platform, and stage 2 compiles
# the core for each platform. Sources, tools, and stage 1 are cached in ~/Library/Caches/VNDegen/rgss. A build whose
# inputs have not changed since the last one is skipped.
#
# Needs, from Homebrew: meson ninja cmake bison make automake autoconf libtool universal-ctags ruby@3.3 gnu-sed grep.
# Stage 1 runs the build machine's Ruby against the Ruby 3.3 it builds, which a newer Ruby refuses.
set -euo pipefail
dest=${1:?destination folder, such as build/rgss}
root=$(cd "$(dirname "$0")/.." && pwd)
src="$root/Engines/RGSS"
api="$root/Packages/VNDegenKit/Sources/CRGSS/include"
cache=${VNDEGEN_RGSS_CACHE:-$HOME/Library/Caches/VNDegen/rgss}
jobs=${VNDEGEN_RGSS_JOBS:-4}
export PATH="/opt/homebrew/opt/bison/bin:/opt/homebrew/opt/ruby@3.3/bin:/opt/homebrew/bin:$PATH"

core_repo=https://github.com/white-axe/mkxp-z
core_commit=650cb0888a07d0b5044e131160ddfa53feaf595b
angle_repo=https://github.com/mkxp-z/mkxp-z
angle_commit=37a04d14413454551a7b6aadbb4e93e67168accb
wasi_sdk_url=https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-30/wasi-sdk-30.0-arm64-macos.tar.gz
wasi_sdk_sha256=2c2ed99296857e60fd14c3f40fe226231f296409502491094704089c31a16740
binaryen_url=https://github.com/WebAssembly/binaryen/releases/download/version_126/binaryen-version_126-arm64-macos.tar.gz
binaryen_sha256=3f1e1379894944531501a8ccc6b59c57f0c7a00fc136dd933f8a6b0c53515a88

for tool in meson ninja cmake gmake gsed ggrep automake autoreconf glibtoolize ctags ruby zip; do
  command -v "$tool" >/dev/null || { echo "error: $tool is missing, see the header of $0" >&2; exit 1; }
done
ctags --version 2>/dev/null | grep -q "Universal Ctags" || { echo "error: ctags must be Universal Ctags (brew install universal-ctags)" >&2; exit 1; }

mkdir -p "$dest" "$cache"
dest=$(cd "$dest" && pwd)
stamp=$( (cat "$0" "$api/vnrgss.h"; cd "$src" && find . -type f -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256) | shasum -a 256 | cut -c1-16)
if [ "$(cat "$dest/.vndegen-engine" 2>/dev/null)" = "$stamp" ]; then
  echo "RGSS engine is up to date in $dest"
  exit 0
fi
start=$(date +%s)

# fetch <repo> <commit> <folder>: a clean checkout of one commit, kept in the cache.
fetch() {
  [ -f "$3/.vndegen-commit" ] && [ "$(cat "$3/.vndegen-commit")" = "$2" ] && return
  rm -rf "$3.part" && mkdir -p "$3.part"
  git -C "$3.part" init --quiet
  git -C "$3.part" fetch --quiet --depth 1 "$1" "$2"
  git -C "$3.part" checkout --quiet FETCH_HEAD
  echo "$2" > "$3.part/.vndegen-commit"
  rm -rf "$3" && mv "$3.part" "$3"
}

# tool <url> <sha256> <folder>: an unpacked release archive, checked against its SHA-256.
tool() {
  [ -f "$3/.vndegen-sha256" ] && [ "$(cat "$3/.vndegen-sha256")" = "$2" ] && return
  rm -rf "$3.part" && mkdir -p "$3.part"
  curl -fsSL -o "$3.part/download.tar.gz" "$1"
  echo "$2  $3.part/download.tar.gz" | shasum -a 256 -c --quiet
  tar -xzf "$3.part/download.tar.gz" -C "$3.part" --strip-components=1
  rm "$3.part/download.tar.gz"
  echo "$2" > "$3.part/.vndegen-sha256"
  rm -rf "$3" && mv "$3.part" "$3"
}

fetch "$core_repo" "$core_commit" "$cache/src/core-$core_commit"
fetch "$angle_repo" "$angle_commit" "$cache/src/angle-$angle_commit"
tool "$wasi_sdk_url" "$wasi_sdk_sha256" "$cache/tools/wasi-sdk-30"
tool "$binaryen_url" "$binaryen_sha256" "$cache/tools/binaryen-126"

work="$dest/.build"
rm -rf "$work"
mkdir -p "$work"

# The patched sources. VNDegen's preload script joins the core's own, which stage 1 packs into the core.
rsync -a --exclude .git "$cache/src/core-$core_commit/" "$work/core/"
for patch in "$src"/patches/*.patch; do patch -s -p1 -d "$work/core" < "$patch"; done
cp "$src/preload/zz_vndegen.rb" "$work/core/scripts/preload/"
rsync -a --exclude .git "$cache/src/angle-$angle_commit/subprojects/" "$work/subprojects/"
for patch in "$src"/patches/angle/*.patch; do patch -s -p1 -d "$work" < "$patch"; done

# Stage 1, cached by its inputs: the core with its patches, and the tools. Its Makefile expects GNU sed and grep, and an
# echo that prints backslashes as they are, as Linux's /bin/sh does but macOS's does not, so it runs under bash.
stage1_stamp=$( (echo "$core_commit $wasi_sdk_sha256 $binaryen_sha256"; cat "$src"/patches/*.patch "$src/preload/zz_vndegen.rb") | shasum -a 256 | cut -c1-16)
stage1="$cache/stage1-$stage1_stamp"
if [ ! -f "$stage1/.complete" ]; then
  echo "Building RGSS stage 1, Ruby through WebAssembly to C. This takes a while the first time."
  (cd "$work/core/libretro" && gmake -j "$jobs" WASI_SDK="$cache/tools/wasi-sdk-30" \
    WASM_OPT="$cache/tools/binaryen-126/bin/wasm-opt" CTAGS="$(command -v ctags)" RUBY="$(command -v ruby)" \
    SED=gsed GREP=ggrep SHELL=/bin/bash) \
    >"$work/stage1.log" 2>&1 || { tail -n 40 "$work/stage1.log" >&2; exit 1; }
  rm -rf "$stage1.part"
  cp -R "$work/core/libretro/build/libretro-stage1" "$stage1.part"
  touch "$stage1.part/.complete"
  rm -rf "$stage1" && mv "$stage1.part" "$stage1"
fi
mkdir -p "$work/core/libretro/build"
rm -rf "$work/core/libretro/build/libretro-stage1"
cp -R "$stage1" "$work/core/libretro/build/libretro-stage1"

# Stage 2 and the framework, per platform: <name> <cross file> <framework layout: ios or macos> <bundle platform>.
build_slice() {
  local name=$1 cross="$src/cross/$2.txt" layout=$3 platform=$4
  echo "Building the RGSS engine for $name"
  meson setup "$work/core-$name" "$work/core" --cross-file "$cross" --buildtype release -Db_lto=false \
    -Dlibretro=true -Dlibretro_static=true -Dlibretro_save_states=false -Dcpp_args=-DMKXPZ_RETRO_FLAT_SAVES \
    >"$work/core-$name.configure.log" 2>&1 || { tail -n 40 "$work/core-$name.configure.log" >&2; exit 1; }
  ninja -C "$work/core-$name" -j "$jobs" >"$work/core-$name.log" 2>&1 || { tail -n 40 "$work/core-$name.log" >&2; exit 1; }

  mkdir -p "$work/host-$name"
  rsync -a "$src/host/" "$work/host-$name/src/"
  ln -s "$work/subprojects" "$work/host-$name/src/subprojects"
  meson setup "$work/host-$name/build" "$work/host-$name/src" --cross-file "$cross" \
    -Dcore="$work/core-$name/mkxp-z_libretro.a" -Dapi_include="$api" \
    >"$work/host-$name.configure.log" 2>&1 || { tail -n 40 "$work/host-$name.configure.log" >&2; exit 1; }
  ninja -C "$work/host-$name/build" -j "$jobs" RGSS.dylib >"$work/host-$name.log" 2>&1 || { tail -n 40 "$work/host-$name.log" >&2; exit 1; }

  local framework="$work/frameworks/$name/RGSS.framework" binary resources
  rm -rf "$framework"
  if [ "$layout" = macos ]; then
    mkdir -p "$framework/Versions/A/Resources"
    binary="$framework/Versions/A/RGSS"
    resources="$framework/Versions/A/Resources"
    ln -s A "$framework/Versions/Current"
    ln -s Versions/Current/RGSS "$framework/RGSS"
    ln -s Versions/Current/Resources "$framework/Resources"
  else
    mkdir -p "$framework"
    binary="$framework/RGSS"
    resources="$framework"
  fi
  cp "$work/host-$name/build/RGSS.dylib" "$binary"
  xcrun strip -x "$binary"
  sed "s/@PLATFORM@/$platform/" "$src/host/Info.plist" > "$resources/Info.plist"
  if [ "$layout" = macos ]; then
    plutil -remove MinimumOSVersion "$resources/Info.plist"
    plutil -insert LSMinimumSystemVersion -string 15.0 "$resources/Info.plist"
  fi
}

build_slice macos macos-arm64 macos MacOSX
build_slice ios ios-arm64 ios iPhoneOS
build_slice ios-simulator ios-simulator-arm64 ios iPhoneSimulator

rm -rf "$dest/RGSS.xcframework" "$dest/licenses"
xcodebuild -create-xcframework -framework "$work/frameworks/ios/RGSS.framework" \
  -framework "$work/frameworks/ios-simulator/RGSS.framework" -framework "$work/frameworks/macos/RGSS.framework" \
  -output "$dest/RGSS.xcframework" >/dev/null

# Licenses: the core's, then every dependency the core or ANGLE builds from source, then where the source is.
mkdir -p "$dest/licenses"
cp "$work/core/COPYING" "$dest/licenses/RGSS engine, mkxp-z (GPL-2.0-or-later).txt"
for folder in "$work/core/subprojects"/*/ "$work/subprojects"/angle-*/ "$work/subprojects"/xxHash-*/; do
  name=$(basename "$folder")
  [ "$name" = packagefiles ] || [ "$name" = packagecache ] || [ "$name" = common ] && continue
  for file in "$folder"COPYING "$folder"COPYING.* "$folder"LICENSE "$folder"LICENSE.* "$folder"LICENCE "$folder"License.txt; do
    [ -f "$file" ] && cat "$file" >> "$dest/licenses/RGSS engine, $name.txt"
  done
done
# Ruby is built in stage 1, whose sources are not kept, so its license text is kept beside the patches.
cp "$src/licenses/Ruby.txt" "$dest/licenses/RGSS engine, Ruby.txt"
cat "$src/licenses/source.txt" > "$dest/licenses/RGSS engine source.txt"
cat >> "$dest/licenses/RGSS engine source.txt" <<EOF

This build used:
  mkxp-z libretro core: $core_repo/tree/$core_commit
  ANGLE's Meson build: $angle_repo/tree/$angle_commit
  VNDegen's patches, frontend, and build script: the Engines/RGSS folder and scripts/rgss-engine.sh of VNDegen's
  source at https://github.com/ChronoStriker1/vndegen
EOF

echo "$stamp" > "$dest/.vndegen-engine"
echo "RGSS engine built in $dest in $(($(date +%s) - start)) s"
