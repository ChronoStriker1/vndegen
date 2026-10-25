#!/usr/bin/env bash
# Runs on a Mac. Drives one iOS simulator that belongs to the caller.
#   SIM_NAME=vn-runtime SIM_TYPE=iPhone-Duo scripts/sim.sh boot
#   scripts/sim.sh install                     installs build/sim's VNDegen.app
#   scripts/sim.sh add-game ~/vndegen-testgames/"No Socks RPG"
#   scripts/sim.sh launch -autolaunch "No Socks RPG"
#   scripts/sim.sh shot /tmp/x.png
#   scripts/sim.sh log 20                      last N seconds of the app's log
# SIM_NAME keeps parallel agents on separate simulators. SIM_TYPE is a CoreSimulator device type suffix,
# such as iPhone-18-Pro (default), iPhone-Duo, or iPad-Pro-13-inch-M5-12GB.
set -euo pipefail
cd "$(dirname "$0")/.."
name=${SIM_NAME:?set SIM_NAME to a name unique to you}
type=${SIM_TYPE:-iPhone-18-Pro}
bundle=com.chronostriker1.vndegen
full="$name ($type)"

udid() { xcrun simctl list devices -j | python3 -c "import json,sys; n=sys.argv[1]; print(next((d['udid'] for r in json.load(sys.stdin)['devices'].values() for d in r if d['name']==n), ''))" "$full"; }

case "${1:?command}" in
  boot)
    id=$(udid)
    [ -n "$id" ] || id=$(xcrun simctl create "$full" "com.apple.CoreSimulator.SimDeviceType.$type")
    xcrun simctl bootstatus "$id" -b >/dev/null
    echo "$id" ;;
  install) xcrun simctl install "$(udid)" build/sim/Build/Products/Debug-iphonesimulator/VNDegen.app ;;
  add-game)
    games="$(xcrun simctl get_app_container "$(udid)" $bundle data)/Documents/Games"
    mkdir -p "$games"
    rsync -a "${2:?game folder}" "$games/"
    echo "$games" ;;
  launch)
    shift
    xcrun simctl terminate "$(udid)" $bundle 2>/dev/null || true
    xcrun simctl launch "$(udid)" $bundle "$@" ;;
  shot) xcrun simctl io "$(udid)" screenshot "${2:?output png}" >/dev/null && echo "$2" ;;
  video) xcrun simctl io "$(udid)" recordVideo --codec h264 --force "${2:?output mp4}" ;;
  log) xcrun simctl spawn "$(udid)" log show --last "${2:-30}s" --predicate 'process == "VNDegen"' --style compact | tail -80 ;;
  *) echo "unknown command $1" >&2; exit 2 ;;
esac
