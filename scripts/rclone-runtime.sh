#!/usr/bin/env bash
# Runs on a Mac with Go (brew install go). Builds rclone's library, as VNDegen uses it, into
# Packages/VNDegenKit/build/CRclone.xcframework: Packages/VNDegenKit/Rclone/rclone.go as a C static library for iPhone
# and iPad, the iOS Simulator (arm64 and x86_64), and the Mac (arm64 and x86_64). Go compiles ahead of time, so nothing
# needs a JIT on iOS. Writes the notices of the code linked into it to build/rclone/licenses/, one text file each:
# rclone's, the Go standard library's, and those of every Go module the library links. A framework or notices already
# made from the same sources and Go version are left alone.
#   scripts/rclone-runtime.sh
set -euo pipefail
self=$(cd "$(dirname "$0")" && pwd)/$(basename "$0")
cd "$(dirname "$0")/.."
src=Packages/VNDegenKit/Rclone
out=Packages/VNDegenKit/build/CRclone.xcframework
licenses=build/rclone/licenses
# go.mod and go.sum pin every module, and rclone.go's imports pick which ones link, so these decide the notices too.
stamp=$( { cat "$self" "$src/go.mod" "$src/go.sum" "$src/rclone.go" "$src/include/"*; go version; } | shasum -a 256 | cut -c1-16)
current() { [ "$(cat "$1" 2>/dev/null)" = "$stamp" ]; }
targets=(ios/arm64 ios/amd64 darwin/arm64 darwin/amd64)

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

build() { # <name> <GOOS> <GOARCH> <sdk> <clang target>
  local sdk
  sdk=$(xcrun --sdk "$4" --show-sdk-path)
  (cd "$src" && CGO_ENABLED=1 GOOS=$2 GOARCH=$3 CC="$(xcrun --sdk "$4" -f clang) -isysroot $sdk -target $5" \
    go build -trimpath -ldflags='-s -w' -buildmode=c-archive -o "$work/$1/librclone.a" .)
}

build_framework() {
  echo "rclone -> $out"
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
}

# A module links when the build compiles one of its packages, which go list -deps reports for each target with the
# build's own settings. The notices cover the union over every target in the framework.
write_licenses() {
  echo "rclone notices -> $licenses"
  local target module version dir file files name goroot
  for target in "${targets[@]}"; do
    (cd "$src" && CGO_ENABLED=1 GOOS=${target%/*} GOARCH=${target#*/} \
      go list -deps -f '{{with .Module}}{{if not .Main}}{{.Path}}|{{.Version}}|{{.Dir}}{{end}}{{end}}' .)
  done | sort -u > "$work/modules"
  [ -s "$work/modules" ] || { echo "go list found no modules in $src" >&2; exit 1; }
  rm -rf "$licenses.part" && mkdir -p "$licenses.part"
  while IFS='|' read -r module version dir; do
    files=()
    for file in "$dir"/*; do
      case "$(basename "$file" | tr '[:upper:]' '[:lower:]')" in
        licen[cs]e*|copying*|notice*|patents*|unlicense*) if [ -f "$file" ]; then files+=("$file"); fi ;;
      esac
    done
    [ ${#files[@]} -gt 0 ] || { echo "Go module $module $version has no license file in $dir" >&2; exit 1; }
    if [ "$module" = github.com/rclone/rclone ]; then name=rclone; else name="rclone - ${module//\//-}"; fi
    {
      echo "$module $version"
      for file in "${files[@]}"; do printf '\n\n-------- %s --------\n\n' "$(basename "$file")"; cat "$file"; done
    } > "$licenses.part/$name.txt"
  done < "$work/modules"
  # Homebrew moves Go's LICENSE out of GOROOT into the folder above it.
  goroot=$(go env GOROOT)
  file=$goroot/LICENSE
  [ -f "$file" ] || file=$goroot/../LICENSE
  [ -f "$file" ] || { echo "no LICENSE in or beside $goroot" >&2; exit 1; }
  {
    echo "The Go standard library and runtime, $(go env GOVERSION)"
    printf '\n\n-------- LICENSE --------\n\n'; cat "$file"
    if [ -f "$goroot/PATENTS" ]; then printf '\n\n-------- PATENTS --------\n\n'; cat "$goroot/PATENTS"; fi
  } > "$licenses.part/rclone - Go standard library.txt"
  rm -rf "$licenses" && mv "$licenses.part" "$licenses"
  echo "$stamp" > "${licenses%/*}/.vndegen-runtime"
}

current "$out/.vndegen-runtime" || build_framework
{ current "${licenses%/*}/.vndegen-runtime" && [ -d "$licenses" ]; } || write_licenses
