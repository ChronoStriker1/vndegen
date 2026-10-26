#!/usr/bin/env bash
# Runs on a Mac. Builds one destination into ./build.
#   scripts/build.sh test      swift test for the package
#   scripts/build.sh mac       macOS app at build/mac/Build/Products/Debug/VNDegen.app
#   scripts/build.sh ios-sim   iOS simulator app at build/sim/Build/Products/Debug-iphonesimulator/VNDegen.app
#   scripts/build.sh ipa       unsigned build/VNDegen.ipa for sideloading (Sideloadly or AltStore re-signs it)
#   scripts/build.sh release 0.1.0 42
#                              Release builds of both platforms in build/release/: the unsigned IPA and the
#                              ad-hoc signed macOS app, zipped. The second argument is the build number.
set -euo pipefail
cd "$(dirname "$0")/.."
xcodegen generate --quiet
# Save sync's rclone library, which the package links on every platform.
scripts/rclone-runtime.sh
# The iOS app links Ren'Py's libraries, which Xcode must find before it builds.
case "${1:-}" in ios-sim|ipa|release) scripts/renpy-runtime.sh build/renpy ios ;; esac

# xcodebuild prints every warning and compiler command. Errors and the final status are what matter here.
# Xcode needs the KiriKiri engine's framework before it can plan the app build, so the engine builds first. The
# project's KiriKiriEngine target rebuilds it too, for builds started from Xcode, once it exists.
xcb() {
  mkdir -p build
  scripts/kirikiri-engine.sh build/kirikiri >build/kirikiri-engine.log 2>&1 || { tail -n 40 build/kirikiri-engine.log; return 1; }
  xcodebuild -project VNDegen.xcodeproj -scheme VNDegen "$@" 2>&1 | filter 'error:|BUILD (SUCCEEDED|FAILED)|\*\* '
}

# Prints the first 60 lines matching <pattern>. The command's own exit status is the pipeline's, through pipefail. The
# filter reads all of its input, so a long log cannot stop the command with SIGPIPE, and no match is no failure.
filter() {
  { grep -E "$1" | grep -v "^/Applications" || true; } | awk 'NR <= 60'
}

case "${1:?destination}" in
  test)
    cd Packages/VNDegenKit
    swift test 2>&1 | filter 'error:|✘|Test run|Executed|failed' ;;
  mac) xcb -destination 'platform=macOS' -derivedDataPath build/mac build ;;
  ios-sim) xcb -destination 'generic/platform=iOS Simulator' -derivedDataPath build/sim build ;;
  ipa)
    xcb -configuration Release -destination 'generic/platform=iOS' -derivedDataPath build/device CODE_SIGNING_ALLOWED=NO build
    rm -rf build/Payload && mkdir -p build/Payload
    cp -R build/device/Build/Products/Release-iphoneos/VNDegen.app build/Payload/
    (cd build && rm -f VNDegen.ipa && zip -qry VNDegen.ipa Payload)
    echo "built $(pwd)/build/VNDegen.ipa" ;;
  release)
    version=${2:?version} number=${3:?build number}
    stamp=(MARKETING_VERSION="$version" CURRENT_PROJECT_VERSION="$number")
    rm -rf build/release build/Payload && mkdir -p build/release build/Payload
    xcb -configuration Release -destination 'generic/platform=iOS' -derivedDataPath build/device CODE_SIGNING_ALLOWED=NO "${stamp[@]}" build
    cp -R build/device/Build/Products/Release-iphoneos/VNDegen.app build/Payload/
    (cd build && zip -qry "release/VNDegen-$version.ipa" Payload)
    xcb -configuration Release -destination 'generic/platform=macOS' -derivedDataPath build/macrelease "${stamp[@]}" build
    app=build/macrelease/Build/Products/Release/VNDegen.app
    codesign --verify --deep "$app"
    ditto -c -k --keepParent "$app" "build/release/VNDegen-$version-macOS.zip"
    ls -la build/release ;;
  *) echo "unknown destination $1" >&2; exit 2 ;;
esac
