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

# xcodebuild prints every warning and compiler command. Errors and the final status are what matter here.
# Xcode needs the KiriKiri engine's framework before it can plan the app build, so the engine builds first. The
# project's KiriKiriEngine target rebuilds it too, for builds started from Xcode, once it exists.
xcb() {
  mkdir -p build
  scripts/kirikiri-engine.sh build/kirikiri >build/kirikiri-engine.log 2>&1 || { tail -n 40 build/kirikiri-engine.log; return 1; }
  xcodebuild -project VNDegen.xcodeproj -scheme VNDegen "$@" 2>&1 \
    | grep -E "error:|BUILD (SUCCEEDED|FAILED)|\*\* " | grep -v "^/Applications" | head -60 || true
  return "${PIPESTATUS[0]}"
}

case "${1:?destination}" in
  test)
    cd Packages/VNDegenKit
    swift test 2>&1 | grep -E "error:|✔|✘|Test run|passed|failed" | grep -v "^/Applications" | tail -40 || true
    exit "${PIPESTATUS[0]}" ;;
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
