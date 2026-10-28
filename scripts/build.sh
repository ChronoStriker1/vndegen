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
scripts/app-licenses.sh
scripts/fonts-licenses.sh
# The iOS app links Ren'Py's libraries, which Xcode must find before it builds.
case "${1:-}" in ios-sim|ipa|release) scripts/renpy-runtime.sh build/renpy ios ;; esac

# The app's Licenses page lists the notices that each bundled runtime writes to build/<runtime>/licenses/. An app build
# fails when one of these has none, so a missing notice can't reach a release. A new runtime adds its name here.
licensed=(rclone renpy rgss kirikiri app fonts)
require_licenses() {
  local runtime
  for runtime in "${licensed[@]}"; do
    ls "build/$runtime/licenses/"*.txt >/dev/null 2>&1 || { echo "error: build/$runtime/licenses/ has no notices" >&2; exit 1; }
  done
}

# xcodebuild prints every warning and compiler command. Errors and the final status are what matter here. The RGSS
# and KiriKiri engines build first, so a failure shows its log. The app's own build phases then find them up to date.
xcb() {
  mkdir -p build
  scripts/rgss-engine.sh build/rgss >build/rgss-engine.log 2>&1 || { tail -n 40 build/rgss-engine.log; return 1; }
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
    # The shared font lookup the native engines compile, against the bundled table.
    mkdir -p build && fonts_run=$(mktemp -d "${TMPDIR:-/tmp}/vndegen-fonts.XXXXXX")
    cc -std=c11 -Wall -Wextra -Werror -o build/vnfont_test Engines/Shared/vnfont.c Engines/Shared/vnfont_test.c
    build/vnfont_test Engines/Shared/fonts "$fonts_run" && rm -rf "$fonts_run"
    # Lookups that ignore case are tested on a case-sensitive volume, as iPhones and iPads have, through
    # VNDEGEN_TEST_DIR. The shared C lookup the RGSS and KiriKiri engines use is tested there first.
    case_volume=$(mktemp -d "${TMPDIR:-/tmp}/vndegen-case.XXXXXX")
    scripts/case-volume.sh attach build/case-sensitive.sparseimage "$case_volume"
    # The image keeps what a run leaves, so each run works in a folder of its own and removes it.
    case_run=$(mktemp -d "$case_volume/run.XXXXXX")
    trap 'rm -rf "$case_run"; scripts/case-volume.sh detach "$case_volume"; rmdir "$case_volume"' EXIT
    mkdir "$case_run/casepath"
    cc -std=c11 -Wall -Wextra -Werror -o build/casepath_test Engines/Shared/casepath.c Engines/Shared/casepath_test.c
    build/casepath_test "$case_run/casepath"
    (cd Packages/VNDegenKit && VNDEGEN_TEST_DIR="$case_run" swift test 2>&1 | filter 'error:|✘|Test run|Executed|failed') ;;
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
[ "$1" = test ] || require_licenses
