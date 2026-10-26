#!/usr/bin/env bash
# Runs on a Mac. Checks two things zz_vndegen.rpy does, on both bundled runtimes, with a small Ren'Py project written to
# a temporary folder:
# - SIGTERM, which VNDegen sends when the player quits from the hub or quits VNDegen, quits the game without Ren'Py's
#   quit prompt, after an autosave and with the persistent data written;
# - a stick direction remapped to a key releases that key when the stick reverses straight to the other direction.
#   scripts/renpy-hook-test.sh [--keep]
# --keep leaves the project, logs, and screenshots in the temporary folder and prints its path.
set -euo pipefail
cd "$(dirname "$0")/.."
scripts/renpy-runtime.sh build/renpy >&2
work=$(mktemp -d)
[ "${1:-}" = --keep ] && echo "output in $work" || trap 'rm -rf "$work"' EXIT

mkdir -p "$work/game/game"
cat > "$work/game/game/script.rpy" <<'EOF'
init python:
    import os
    import pygame

    # Records the keys the hook posts.
    hooktest_post = pygame.event.post

    def hooktest_record(ev):
        if ev.type in (pygame.KEYDOWN, pygame.KEYUP):
            with open(os.path.join(os.environ["VNDEGEN_DATA"], "keys.txt"), "a") as f:
                name = {pygame.K_w: "w", pygame.K_s: "s"}.get(ev.key, str(ev.key))
                f.write("%s %s\n" % ("down" if ev.type == pygame.KEYDOWN else "up", name))
        hooktest_post(ev)

    pygame.event.post = hooktest_record

# The quit prompt a real game's screens.rpy defines.
screen confirm(message, yes_action, no_action):
    modal True
    vbox:
        text message
        textbutton "Yes" action yes_action
        textbutton "No" action no_action

label main_menu:
    return

label start:
    "Line one."
    "Line two."
    return
EOF

failed=0
fail() { echo "FAIL Ren'Py $runtime: $*"; failed=1; }

for runtime in 8 7; do
  data="$work/data-$runtime" probe="$work/probe-$runtime"
  mkdir -p "$data" "$probe"
  python=$(echo "build/renpy/$runtime"/lib/py*-mac-universal/python)
  env -u PYTHONHOME -u PYTHONPATH PYTHONDONTWRITEBYTECODE=1 VNDEGEN_RENPY_RUNTIME="$PWD/build/renpy/$runtime" \
    VNDEGEN_DATA="$data" VNDEGEN_WINDOW=80,80,640,360 VNDEGEN_BUTTONS=up=key:K_w,down=key:K_s VNDEGEN_PROBE="$probe" \
    VNDEGEN_PROBE_STEPS="wait:2 axis:lefty:neg axis:lefty:pos axis:lefty:zero wait:1 shot:ready" \
    "$python" Packages/VNDegenKit/Sources/EngineRenPy/Support/launcher.py "$work/game" > "$data/stdout.txt" 2>&1 &
  pid=$!

  for _ in $(seq 120); do [ -f "$probe/ready.png" ] || ! kill -0 "$pid" 2>/dev/null && break; sleep 0.5; done
  [ -f "$probe/ready.png" ] || { fail "the game did not reach its first line"; kill -KILL "$pid" 2>/dev/null; continue; }
  touch "$work/signal-$runtime"
  kill -TERM "$pid"
  for _ in $(seq 20); do kill -0 "$pid" 2>/dev/null || break; sleep 0.5; done
  if kill -0 "$pid" 2>/dev/null; then
    fail "still running 10 seconds after SIGTERM"
    kill -KILL "$pid"
  fi
  status=0
  wait "$pid" || status=$?
  [ "$status" = 0 ] || fail "exited with status $status"
  [ ! -f "$data/logs/traceback.txt" ] || fail "raised $(grep -m1 -E '(Error|Exception)' "$data/logs/traceback.txt")"
  ls "$data/saves"/auto-*.save > /dev/null 2>&1 || fail "no autosave in $data/saves"
  [ "$data/saves/persistent" -nt "$work/signal-$runtime" ] || fail "persistent data not written on quit"
  keys=$(tr '\n' ' ' < "$data/keys.txt" 2>/dev/null || true)
  [ "$keys" = "down w up w down s up s " ] || fail "stick reversal posted '$keys', expected 'down w up w down s up s '"
  echo "Ren'Py $runtime: exit $status, saves: $(ls "$data/saves" | tr '\n' ' '), keys: $keys"
done
[ "$failed" = 0 ] && echo PASS
exit "$failed"
