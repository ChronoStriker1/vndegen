#!/usr/bin/env bash
# Runs on a Mac. Builds the KiriKiri engine in Engines/KiriKiri:
#   <dest>/mac/KiriKiriHelper            macOS arm64 executable, with its font in <dest>/mac/Res/
#   <dest>/ios/KiriKiri.xcframework      iOS device and simulator (arm64) framework
#   scripts/kirikiri-engine.sh build/kirikiri
# Dependencies come from vcpkg, a shared clone in ~/Library/Caches/VNDegen/vcpkg that this script
# clones and bootstraps when missing. vcpkg's binary cache keeps built dependencies across
# checkouts. A build whose inputs have not changed since the last one is skipped.
set -euo pipefail
dest=${1:?destination folder, such as build/kirikiri}
root=$(cd "$(dirname "$0")/.." && pwd)
src="$root/Engines/KiriKiri"
cache=${VNDEGEN_KIRIKIRI_CACHE:-$HOME/Library/Caches/VNDegen}
vcpkg="$cache/vcpkg"
baseline=$(sed -n 's/.*"builtin-baseline": *"\([0-9a-f]*\)".*/\1/p' "$src/vcpkg.json")
mkdir -p "$dest"
dest=$(cd "$dest" && pwd)

stamp=$( (cat "$0" "$root/Packages/VNDegenKit/Sources/CKiriKiri/include/vnkk.h"; cd "$src" && find . -path ./test-host -prune -o -type f -print0 | LC_ALL=C sort -z | xargs -0 shasum -a 256) | shasum -a 256 | cut -c1-16)
if [ "$(cat "$dest/.vndegen-engine" 2>/dev/null)" = "$stamp" ]; then
  echo "KiriKiri engine is up to date in $dest"
  exit 0
fi

if [ ! -x "$vcpkg/vcpkg" ]; then
  rm -rf "$vcpkg.part"
  git clone --quiet https://github.com/microsoft/vcpkg "$vcpkg.part"
  "$vcpkg.part/bootstrap-vcpkg.sh" -disableMetrics
  rm -rf "$vcpkg" && mv "$vcpkg.part" "$vcpkg"
fi
git -C "$vcpkg" cat-file -e "$baseline^{commit}" 2>/dev/null || git -C "$vcpkg" fetch --quiet origin

work="$dest/.build"
configure_and_build() { # <name> <triplet> <target> [cmake args...]
  local name=$1 triplet=$2 target=$3
  shift 3
  cmake -S "$src" -B "$work/$name" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE="$vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET="$triplet" -DVCPKG_INSTALL_OPTIONS=--no-print-usage \
    -DKIRIKIRI_DOWNLOADS="$cache/kirikiri-downloads" "$@" >"$work/$name.configure.log" ||
    { tail -n 40 "$work/$name.configure.log" >&2; exit 1; }
  cmake --build "$work/$name" --target "$target"
}

start=$(date +%s)
mkdir -p "$work"
configure_and_build mac arm64-osx KiriKiriHelper
configure_and_build ios arm64-ios KiriKiri -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphoneos
configure_and_build ios-sim arm64-ios-simulator KiriKiri -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator

rm -rf "$dest/mac" "$dest/ios"
mkdir -p "$dest/mac/Res" "$dest/ios"
cp -p "$work/mac/KiriKiriHelper" "$dest/mac/"
cp -p "$work/mac/Res/DroidSansFallback.ttf" "$dest/mac/Res/"
xcodebuild -create-xcframework -framework "$work/ios/KiriKiri.framework" \
  -framework "$work/ios-sim/KiriKiri.framework" -output "$dest/ios/KiriKiri.xcframework" >/dev/null
echo "$stamp" > "$dest/.vndegen-engine"
echo "KiriKiri engine built in $dest in $(($(date +%s) - start)) s"
