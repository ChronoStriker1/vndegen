# VNDegen's entry point for a Ren'Py helper process. Runs under both bundled runtimes, so it must stay valid
# Python 2.7 and 3.
#
#   <runtime>/lib/py3-mac-universal/python launcher.py <game base directory>
#
# It runs the runtime's own renpy.py with three changes, all in memory:
# - saves, save tokens, and logs go to VNDegen's per-game data directory instead of the game folder or ~/Library;
# - Ren'Py's read-only mode (renpy.macapp, meant for games inside a signed .app) is on, so Ren'Py never compiles
#   .rpyc files, writes caches, or deletes anything in the game folder;
# - the folder holding zz_vndegen.rpy joins the search path, so every game loads VNDegen's hook.

from __future__ import print_function

import os
import sys
import types

# The runtime lives inside the signed app bundle, where a written .pyc would break the signature.
sys.dont_write_bytecode = True

runtime = os.environ["VNDEGEN_RENPY_RUNTIME"]
data = os.environ["VNDEGEN_DATA"]
scripts = os.path.join(os.path.dirname(os.path.abspath(__file__)), "scripts")

# Ren'Py's log goes to stdout, which VNDegen writes to the data directory, because read-only mode turns log.txt off.
os.environ["RENPY_LOG_TO_STDOUT"] = "1"

renpy_py = os.path.join(runtime, "renpy.py")
main = types.ModuleType("renpy_main")
main.__file__ = renpy_py
sys.modules["renpy_main"] = main
with open(renpy_py) as f:
    exec(compile(f.read(), renpy_py, "exec"), main.__dict__)


def path_to_saves(gamedir, save_directory=None):
    # Saves sit directly in <data>/saves, without Ren'Py's config.save_directory level, so saves from any PC
    # location map to one folder. Ren'Py also asks for "tokens" and "backups" folders this way.
    return os.path.join(data, save_directory or "saves")


def path_to_logdir(basedir):
    return os.path.join(data, "logs")


searchpath = main.predefined_searchpath


def predefined_searchpath(commondir):
    return searchpath(commondir) + [scripts]


main.path_to_saves = path_to_saves
main.path_to_logdir = path_to_logdir
main.predefined_searchpath = predefined_searchpath

sys.path.append(runtime)
import renpy  # noqa: E402

renpy.macapp = True

if sys.version_info >= (3, 12):
    import importlib.util

    class LegacyFinders(object):
        """
        Python 3.12 dropped find_module from the import system. Old libraries that games bundle, such as the six
        vendored by urllib3 inside requests, still register finders that only have find_module. This asks them.
        """

        def find_spec(self, fullname, path, target=None):
            for finder in list(sys.meta_path):
                if finder is self or hasattr(finder, "find_spec"):
                    continue
                find_module = getattr(finder, "find_module", None)
                loader = find_module(fullname, path) if find_module else None
                if loader is not None:
                    return importlib.util.spec_from_loader(fullname, loader)
            return None

    sys.meta_path.insert(0, LegacyFinders())

sys.argv[0] = renpy_py
main.main()
