#!/usr/bin/env bash
# Runs on a Mac. Checks where the KiriKiri engine puts a game's saves when its <exe>.cf names a
# -datapath, using a script-only game that writes a file into System.dataPath and quits:
#   scripts/kirikiri-datapath-check.sh build/kirikiri/mac/KiriKiriHelper <work folder>
# The cases are the table in Packages/VNDegenKit/Tests/EngineKiriKiriTests/Fixtures/datapath-cases.tsv,
# which VNDegen's save mapping is tested against too. Each case gets a folder in the work folder, laid
# out as the table describes. The helper built with sanitizers, from scripts/kirikiri-engine.sh's
# restart-asan build folder, also catches memory errors. Exits nonzero if a check fails.
set -euo pipefail
helper=${1:?KiriKiriHelper path}
work=${2:?work folder}
root=$(cd "$(dirname "$0")/.." && pwd)
cases="$root/Packages/VNDegenKit/Tests/EngineKiriKiriTests/Fixtures/datapath-cases.tsv"
mkdir -p "$work"
work=$(cd "$work" && pwd)
failed=0

# run <case> <datapath> <setup>: builds the game and runs it until it quits.
run() {
  local dir="$work/$1" game="$work/$1/a/game" item
  rm -rf "$dir"
  mkdir -p "$game" "$dir/data/AppData" "$dir/data/Documents" "$dir/victim"
  : >"$game/game.exe"
  printf 'datapath=%s\n' "$2" >"$game/game.cf"
  cat >"$game/startup.tjs" <<'EOF'
Debug.message("check: dataPath " + System.dataPath);
["probe"].saveStruct(System.dataPath + "probe.txt");
System.terminate();
EOF
  for item in $3; do
    case "$item" in
      -) ;;
      *'->'*) ln -s "${item#*->}" "$game/${item%%->*}" ;;
      */) mkdir -p "$game/$item" ;;
    esac
  done
  ASAN_OPTIONS=detect_leaks=0 "$helper" --game-root "$game" --data "$game/" --app-data "$dir/data/AppData" \
    --documents "$dir/data/Documents" --log "$dir/engine.log" </dev/null >"$dir/helper.out" 2>&1 &
  local pid=$! deadline=$(($(date +%s) + 20))
  while kill -0 $pid 2>/dev/null && [ "$(date +%s)" -le "$deadline" ]; do sleep 0.1; done
  if kill -0 $pid 2>/dev/null; then
    echo "fail $1: the game did not quit in 20 s"
    kill -TERM $pid
    failed=1
  fi
  wait $pid || true
  grep -a "datapath\|Savedata path\|check: \|rror" "$dir/engine.log" | sed "s|$work/||g; s/^[0-9:]* /  /" || true
  if grep -aq "ERROR: AddressSanitizer\|runtime error" "$dir/helper.out"; then
    grep -a -m1 -A6 "ERROR: AddressSanitizer\|runtime error" "$dir/helper.out" | sed 's/^/  /'
    echo "fail $1: a sanitizer reported"
    failed=1
  fi
}

# expect <case> <folder relative to the case folder>: the probe is there and nowhere else.
expect() {
  local found
  found=$(cd "$work/$1" && find . -name probe.txt | sed 's|^\./||' | sort | tr '\n' ' ')
  if [ "$found" = "$2/probe.txt " ]; then
    echo "pass $1: saves went to $2"
  else
    echo "fail $1: expected saves in $2, found ${found:-none}"
    failed=1
  fi
}

while IFS=$'\t' read -r name datapath setup saves; do
  case "$name" in '#'* | '') continue ;; esac
  run "$name" "$datapath" "$setup"
  expect "$name" "$saves"
done <"$cases"

[ $failed = 0 ] && echo "datapath check passed" || echo "datapath check failed"
exit $failed
