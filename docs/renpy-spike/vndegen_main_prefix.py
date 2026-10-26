# VNDegen spike: boot a game folder from Documents instead of the bundled project.
import os as _os
import sys as _sys


def _vndegen_basedir():
    games = _os.path.expanduser("~/Documents/Games")
    names = sorted(n for n in _os.listdir(games) if not n.startswith("."))
    name = _os.environ.get("VNDEGEN_GAME") or names[0]
    folder = _os.path.join(games, name)
    for candidate in [folder] + [_os.path.join(folder, n) for n in sorted(_os.listdir(folder))]:
        if _os.path.isdir(_os.path.join(candidate, "game")) and _os.path.isdir(_os.path.join(candidate, "renpy")):
            return candidate
    raise SystemExit("VNDegen: no Ren'Py game in " + folder)


_sys.argv = [_sys.argv[0], _vndegen_basedir()]
print("VNDegen basedir:", _sys.argv[1])
