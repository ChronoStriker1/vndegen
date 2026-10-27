#!/usr/bin/env bash
# Runs on a Mac. Builds the RGSS engine (RPG Maker XP, VX and VX Ace) from Engines/RGSS:
#   <dest>/RGSS.xcframework   the framework VNDegen embeds: iOS device and simulator, and macOS, all arm64. It holds
#                             mkxp-z's libretro core, ANGLE, and VNDegen's frontend (Engines/RGSS/host, with the file
#                             name matching in Engines/Shared)
#   <dest>/licenses/          the notices of mkxp-z, with where to get the source of this build, and of ANGLE and
#                             every dependency, one text file each, as scripts/licenses/README.md describes
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
shared="$root/Engines/Shared"
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

mkdir -p "$dest" "$cache"
dest=$(cd "$dest" && pwd)
# Engines/Shared counts by its C sources only, so its fonts changing rebuilds nothing.
stamp=$( (cat "$0" "$api/vnrgss.h"; cd "$root/Engines" && { find RGSS -type f -print0; find Shared -type f \( -name "*.c" -o -name "*.h" \) -print0; } | LC_ALL=C sort -z | xargs -0 shasum -a 256) | shasum -a 256 | cut -c1-16)
if [ "$(cat "$dest/.vndegen-engine" 2>/dev/null)" = "$stamp" ]; then
  echo "RGSS engine is up to date in $dest"
  exit 0
fi
# The tools are needed only to build, so a Mac with a copy of an up-to-date build/rgss needs none of them.
for tool in meson ninja cmake gmake gsed ggrep automake autoreconf glibtoolize ctags ruby zip; do
  command -v "$tool" >/dev/null || { echo "error: $tool is missing, see the header of $0" >&2; exit 1; }
done
ctags --version 2>/dev/null | grep -q "Universal Ctags" || { echo "error: ctags must be Universal Ctags (brew install universal-ctags)" >&2; exit 1; }

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

# The work folder stays between builds, so a change rebuilds only what it touches. The patched sources are made fresh
# in `fresh/`, then copied over the sources the builds use by content, so unchanged files keep their times.
work="$dest/.build"
rm -rf "$work/fresh"
mkdir -p "$work/fresh"

# The patched sources.
rsync -a --exclude .git "$cache/src/core-$core_commit/" "$work/fresh/core/"
for patch in "$src"/patches/*.patch; do patch -s -p1 -d "$work/fresh/core" < "$patch"; done
# Patches in stage2/ change only code stage 2 builds, so they stay out of stage 1's cache key below.
for patch in "$src"/patches/stage2/*.patch; do patch -s -p1 -d "$work/fresh/core" < "$patch"; done
rsync -a --exclude .git "$cache/src/angle-$angle_commit/subprojects/" "$work/fresh/subprojects/"
for patch in "$src"/patches/angle/*.patch; do patch -s -p1 -d "$work/fresh" < "$patch"; done
# The core's libretro/build folder holds stage 1 and its downloads, which the fresh sources don't have. The subprojects
# Meson downloaded into either subprojects folder aren't in the fresh sources either, and the notices below read them.
rsync -a --checksum --delete --exclude /libretro/build --filter 'P /subprojects/*/' "$work/fresh/core/" "$work/core/"
rsync -a --checksum --delete --filter 'P /*/' "$work/fresh/subprojects/" "$work/subprojects/"
rm -rf "$work/fresh"

# Stage 1, cached by its inputs: the core with its patches outside stage2/, and the tools. Its Makefile expects GNU sed and grep, and an
# echo that prints backslashes as they are, as Linux's /bin/sh does but macOS's does not, so it runs under bash. Bump
# stage1_format to rebuild every cached stage 1, as 2 did for caches an earlier build made without starting clean.
stage1_format=2
stage1_stamp=$( (echo "$stage1_format $core_commit $wasi_sdk_sha256 $binaryen_sha256"; cat "$src"/patches/*.patch) | shasum -a 256 | cut -c1-16)
stage1="$cache/stage1-$stage1_stamp"
if [ ! -f "$stage1/.complete" ]; then
  echo "Building RGSS stage 1, Ruby through WebAssembly to C. This takes a while the first time."
  # Ruby's patches apply only when its source is first fetched, and its outputs don't depend on them, so stage 1 always
  # starts from nothing.
  rm -rf "$work/core/libretro/build"
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
if [ "$(cat "$work/core/libretro/build/libretro-stage1/.stamp" 2>/dev/null)" != "$stage1_stamp" ]; then
  rm -rf "$work/core/libretro/build/libretro-stage1"
  cp -R "$stage1" "$work/core/libretro/build/libretro-stage1"
  echo "$stage1_stamp" > "$work/core/libretro/build/libretro-stage1/.stamp"
fi

# Stage 2 and the framework, per platform: <name> <cross file> <framework layout: ios or macos> <bundle platform>.
build_slice() {
  local name=$1 cross="$src/cross/$2.txt" layout=$3 platform=$4
  echo "Building the RGSS engine for $name"
  if [ ! -f "$work/core-$name/build.ninja" ]; then
    meson setup "$work/core-$name" "$work/core" --cross-file "$cross" --buildtype release -Db_lto=false \
      -Dlibretro=true -Dlibretro_static=true -Dlibretro_save_states=false -Dcpp_args=-DMKXPZ_RETRO_FLAT_SAVES \
      >"$work/core-$name.configure.log" 2>&1 || { rm -rf "$work/core-$name"; tail -n 40 "$work/core-$name.configure.log" >&2; exit 1; }
  fi
  ninja -C "$work/core-$name" -j "$jobs" >"$work/core-$name.log" 2>&1 || { tail -n 40 "$work/core-$name.log" >&2; exit 1; }

  mkdir -p "$work/host-$name/src"
  rsync -a --checksum --delete --exclude /subprojects --exclude /shared "$src/host/" "$work/host-$name/src/"
  ln -sfn "$work/subprojects" "$work/host-$name/src/subprojects"
  ln -sfn "$shared" "$work/host-$name/src/shared"
  if [ ! -f "$work/host-$name/build/build.ninja" ]; then
    meson setup "$work/host-$name/build" "$work/host-$name/src" --cross-file "$cross" \
      -Dcore="$work/core-$name/mkxp-z_libretro.a" -Dapi_include="$api" \
      >"$work/host-$name.configure.log" 2>&1 || { rm -rf "$work/host-$name/build"; tail -n 40 "$work/host-$name.configure.log" >&2; exit 1; }
  fi
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
  # VNDegen's preload script, which a session puts in the engine's Scripts/Preload folder beside the core's own.
  cp "$src/preload/zz_vndegen.rb" "$resources/"
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

# Notices, one text file each, as scripts/licenses/README.md describes: mkxp-z's own with where to get the source of
# this build, then those of every dependency the core or ANGLE builds from source, then the code with no license file
# in its source folder, whose texts are in Engines/RGSS/licenses. A source folder with no license and no text there
# fails the build, so a new dependency can't ship without its notice.
licenses="$dest/licenses"
rm -rf "$licenses" && mkdir -p "$licenses"
{
  echo "mkxp-z libretro core, $core_repo at $core_commit"
  echo
  cat "$src/licenses/source.txt"
  echo
  echo "This build used:"
  echo "  mkxp-z libretro core: $core_repo/tree/$core_commit"
  echo "  ANGLE's Meson build: $angle_repo/tree/$angle_commit"
  echo "  VNDegen's patches, frontend, and build script: the Engines/RGSS folder and scripts/rgss-engine.sh of VNDegen's"
  echo "  source at https://github.com/ChronoStriker1/vndegen"
  printf '\n\n-------- COPYING --------\n\n'
  cat "$work/core/COPYING"
  printf '\n\n-------- GNU GPL version 3 --------\n\n'
  cat "$src/licenses/GNU GPL version 3.txt"
} > "$licenses/mkxp-z.txt"
for folder in "$work/core/subprojects"/*/ "$work/subprojects"/angle-*/ "$work/subprojects"/xxHash-*/; do
  name=$(basename "$folder")
  case "$name" in packagefiles | packagecache | common) continue ;; esac
  files=()
  for file in "$folder"*; do
    case "$(basename "$file" | tr '[:upper:]' '[:lower:]')" in
      licen[cs]e* | copying* | notice*) if [ -f "$file" ]; then files+=("$file"); fi ;;
    esac
  done
  if [ ${#files[@]} -eq 0 ]; then
    case "$name" in
      boost_*) continue ;; # header-only Boost libraries, all under the Boost license below
      libretro-common) file="$src/licenses/libretro-common.txt" ;;
      egl-registry) file="$src/licenses/Khronos EGL headers.txt" ;;
      opengl-registry) file="$src/licenses/Khronos OpenGL headers.txt" ;;
      *) echo "error: $name has no license file, add its notice to Engines/RGSS/licenses" >&2; exit 1 ;;
    esac
    cp "$file" "$licenses/mkxp-z - $name.txt"
    continue
  fi
  {
    echo "$name"
    for file in "${files[@]}"; do printf '\n\n-------- %s --------\n\n' "$(basename "$file")"; cat "$file"; done
  } > "$licenses/mkxp-z - $name.txt"
done
# What stage 1 builds into the core, whose sources are not kept (Ruby, libyaml, wasi-libc, and wasm2c's templates),
# Boost, and what the core embeds.
for name in Ruby libyaml wasi-libc "wabt (wasm2c)" Boost "Liberation Sans font" "SDL GameControllerDB" "GMGSx soundfont"; do
  cp "$src/licenses/$name.txt" "$licenses/mkxp-z - $name.txt"
done

echo "$stamp" > "$dest/.vndegen-engine"
echo "RGSS engine built in $dest in $(($(date +%s) - start)) s"
