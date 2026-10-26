#!/usr/bin/env bash
# Runs on a Mac with Go (brew install go). Builds rclone's library, as VNDegen uses it, into
# Packages/VNDegenKit/build/CRclone.xcframework: Packages/VNDegenKit/Rclone/rclone.go as a C static library for iPhone
# and iPad, the iOS Simulator (arm64 and x86_64), and the Mac (arm64 and x86_64). Go compiles ahead of time, so nothing
# needs a JIT on iOS. A framework already built from the same sources and Go version is left alone.
#   scripts/rclone-runtime.sh
set -euo pipefail
self=$(cd "$(dirname "$0")" && pwd)/$(basename "$0")
cd "$(dirname "$0")/.."
src=Packages/VNDegenKit/Rclone
out=Packages/VNDegenKit/build/CRclone.xcframework
stamp=$( { cat "$self" "$src/go.mod" "$src/go.sum" "$src/rclone.go" "$src/include/"*; go version; } | shasum -a 256 | cut -c1-16)
[ "$(cat "$out/.vndegen-runtime" 2>/dev/null)" = "$stamp" ] && exit 0
echo "rclone -> $out"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
build() { # <name> <GOOS> <GOARCH> <sdk> <clang target>
  local sdk
  sdk=$(xcrun --sdk "$4" --show-sdk-path)
  (cd "$src" && CGO_ENABLED=1 GOOS=$2 GOARCH=$3 CC="$(xcrun --sdk "$4" -f clang) -isysroot $sdk -target $5" \
    go build -trimpath -ldflags='-s -w' -buildmode=c-archive -o "$work/$1/librclone.a" .)
}
build ios ios arm64 iphoneos arm64-apple-ios18.0
build sim-arm64 ios arm64 iphonesimulator arm64-apple-ios18.0-simulator
build sim-x86_64 ios amd64 iphonesimulator x86_64-apple-ios18.0-simulator
build mac-arm64 darwin arm64 macosx arm64-apple-macos15.0
build mac-x86_64 darwin amd64 macosx x86_64-apple-macos15.0
mkdir -p "$work/sim" "$work/mac"
lipo -create "$work/sim-arm64/librclone.a" "$work/sim-x86_64/librclone.a" -output "$work/sim/librclone.a"
lipo -create "$work/mac-arm64/librclone.a" "$work/mac-x86_64/librclone.a" -output "$work/mac/librclone.a"
rm -rf "$out"
xcodebuild -create-xcframework -output "$out" \
  -library "$work/ios/librclone.a" -headers "$src/include" \
  -library "$work/sim/librclone.a" -headers "$src/include" \
  -library "$work/mac/librclone.a" -headers "$src/include" >/dev/null
echo "$stamp" > "$out/.vndegen-runtime"
