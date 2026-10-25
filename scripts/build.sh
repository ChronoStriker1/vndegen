#!/usr/bin/env bash
# Runs on a Mac. Builds one destination into ./build.
#   scripts/build.sh test      swift test for the package
#   scripts/build.sh mac       macOS app at build/mac/Build/Products/Debug/VNDegen.app
#   scripts/build.sh ios-sim   iOS simulator app at build/sim/Build/Products/Debug-iphonesimulator/VNDegen.app
#   scripts/build.sh ipa       unsigned build/VNDegen.ipa for sideloading (Sideloadly or AltStore re-signs it)
set -euo pipefail
cd "$(dirname "$0")/.."
xcodegen generate --quiet

# xcodebuild prints every warning and compiler command. Errors and the final status are what matter here.
xcb() {
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
  *) echo "unknown destination $1" >&2; exit 2 ;;
esac
