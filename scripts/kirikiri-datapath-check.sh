#!/usr/bin/env bash
# Runs on a Mac. Checks where the KiriKiri engine puts a game's saves when its <exe>.cf names a
# -datapath, using a script-only game that writes a file into System.dataPath and quits:
#   scripts/kirikiri-datapath-check.sh build/kirikiri/mac/KiriKiriHelper <work folder>
# Each case gets a folder in the work folder, with the game in <case>/a/game/. A data path that
# leaves the game's folder, through KiriKiri's "..." (two folders up) or a symbolic link, must be
# ignored, and the saves stay in savedata/. Exits nonzero if a check fails.
set -euo pipefail
helper=${1:?KiriKiriHelper path}
work=${2:?work folder}
mkdir -p "$work"
work=$(cd "$work" && pwd)
failed=0

# run <case> <datapath> [setup command]: builds the game and runs it until it quits.
run() {
  local dir="$work/$1" game="$work/$1/a/game"
  rm -rf "$dir"
  mkdir -p "$game" "$dir/victim"
  : >"$game/game.exe"
  printf 'datapath="%s"\n' "$2" >"$game/game.cf"
  cat >"$game/startup.tjs" <<'EOF'
Debug.message("check: dataPath " + System.dataPath);
["probe"].saveStruct(System.dataPath + "probe.txt");
System.terminate();
EOF
  if [ $# -gt 2 ]; then (cd "$game" && eval "$3"); fi
  "$helper" --game-root "$game" --data "$game/" --log "$dir/engine.log" >"$dir/helper.out" 2>&1 &
  local pid=$! deadline=$(($(date +%s) + 20))
  while kill -0 $pid 2>/dev/null && [ "$(date +%s)" -le "$deadline" ]; do sleep 0.1; done
  if kill -0 $pid 2>/dev/null; then
    echo "fail $1: the game did not quit in 20 s"
    kill -TERM $pid
    failed=1
  fi
  wait $pid || true
  grep -a "datapath\|Savedata path\|check: \|rror" "$dir/engine.log" | sed "s|$work/||g; s/^[0-9:]* /  /" || true
}

# expect <case> <probe path relative to the case folder>: the probe is there and nowhere else.
expect() {
  local found
  found=$(cd "$work/$1" && find . -name probe.txt | sed 's|^\./||' | sort | tr '\n' ' ')
  if [ "$found" = "$2 " ]; then
    echo "pass $1: saves went to $2"
  else
    echo "fail $1: expected saves in $2, found ${found:-none}"
    failed=1
  fi
}

run dots '$(exepath)/.../victim/savedata'
expect dots a/game/savedata/probe.txt

run symlink '$(exepath)/link/savedata' 'ln -s ../../victim link'
expect symlink a/game/savedata/probe.txt

run inside '$(exepath)/saves'
expect inside a/game/saves/probe.txt

run dots-inside '$(exepath)/x/y/.../saves'
expect dots-inside a/game/saves/probe.txt

run symlink-inside '$(exepath)/inner' 'mkdir real && ln -s real inner'
expect symlink-inside a/game/real/probe.txt

[ $failed = 0 ] && echo "datapath check passed" || echo "datapath check failed"
exit $failed
