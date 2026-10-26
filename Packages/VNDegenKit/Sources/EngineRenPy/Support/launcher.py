# VNDegen's entry point for a Ren'Py game. Runs under both bundled runtimes, so it must stay valid Python 2.7 and 3.
#
#   <runtime>/lib/py3-mac-universal/python launcher.py <game base directory>
#
# On macOS each game runs in its own helper process. On iOS VNDegen runs this file in the app's one interpreter for each
# game it starts, so everything below must hold up when it runs again in the same process.
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

if runtime not in sys.path:
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

    if not any(type(finder).__name__ == "LegacyFinders" for finder in sys.meta_path):
        sys.meta_path.insert(0, LegacyFinders())


def forget_modules(basedir):
    """
    Drops the modules a game brought along, such as a requests in game/python-packages, so the next game imports its
    own. Their submodules go too, including ones without a file, such as those six creates for six.moves. Ren'Py's and
    the standard library's modules stay, since other modules hold on to them.
    """
    inside = os.path.join(os.path.realpath(basedir), "")
    names = [name for name, module in list(sys.modules.items())
             if os.path.realpath(getattr(module, "__file__", None) or "/").startswith(inside)]
    for name in list(sys.modules):
        if any(name == root or name.startswith(root + ".") for root in names):
            del sys.modules[name]


def show_view_in_hub():
    """
    VNDegen shows Ren'Py's view inside its own window once the view exists. On mobile Ren'Py opens its display before
    the game's init code runs, so this can't wait for zz_vndegen.rpy. Ren'Py's reload keeps classes, so the patch wraps
    the original it saved.
    """
    import ctypes

    host = ctypes.CDLL(None)
    Interface = renpy.display.core.Interface
    set_mode = getattr(Interface.set_mode, "vndegen_original", Interface.set_mode)

    def set_mode_and_show(self):
        set_mode(self)
        host.vndegen_renpy_window_ready()

    set_mode_and_show.vndegen_original = set_mode
    Interface.set_mode = set_mode_and_show


def run_in_process():
    """
    Runs the game the way renpy.bootstrap.bootstrap() does, with two changes for iOS, where every game shares the app's
    one interpreter. The game ends by returning, never by exiting, and it leaves nothing behind for the next game. The
    first game imports Ren'Py. Each later one starts from Ren'Py's own reload, which an utter restart also uses.
    """
    import renpy.arguments
    import renpy.bootstrap
    import renpy.config

    renpy.__main__ = main
    state = sys.modules.get("_vndegen_state")
    if state is None:
        renpy.config.renpy_base = runtime
        import renpy.pygame

        renpy.pygame.import_as_pygame()
        import _renpy  # noqa: F401

        renpy.import_all()
        renpy.importer.init_importer()
        # What a game must not change for the next one: the finders and paths Ren'Py itself uses.
        state = types.ModuleType("_vndegen_state")
        state.meta_path = list(sys.meta_path)
        state.path_hooks = list(sys.path_hooks)
        state.path = list(sys.path)
        sys.modules["_vndegen_state"] = state
    else:
        # The last game shut its renderer down when it ended.
        renpy.session["_keep_renderer"] = True
        renpy.reload_all()
        renpy.session.pop("_keep_renderer", None)

    show_view_in_hub()
    sys.excepthook = renpy.bootstrap.excepthook
    args = renpy.arguments.bootstrap()
    basedir = os.path.abspath(args.basedir)
    sys.path.insert(0, basedir)
    game_sys_path = list(sys.path)

    try:
        restart = True
        while restart:
            restart = False
            try:
                sys.path = list(game_sys_path)
                renpy.game.args = args
                renpy.config.basedir = basedir
                renpy.config.gamedir = main.path_to_gamedir(basedir, "renpy")
                renpy.config.args = []
                renpy.config.logdir = main.path_to_logdir(basedir)
                if not os.path.exists(renpy.config.logdir):
                    os.makedirs(renpy.config.logdir)
                renpy.main.main()
            except renpy.game.UtterRestartException:
                renpy.reload_all()
                restart = True
            except renpy.game.QuitException:
                pass
            except renpy.game.ParseErrorException:
                pass
            except Exception as e:
                renpy.error.report_exception(e, editor=True)
    finally:
        renpy.display.tts.tts(None)
        if renpy.display.draw:
            renpy.display.im.cache.quit()
            renpy.display.draw.quit()
            renpy.display.draw = None
        renpy.audio.audio.quit()
        for callback in renpy.config.python_exit_callbacks:
            callback()
        forget_modules(basedir)
        sys.meta_path[:] = state.meta_path
        sys.path_hooks[:] = state.path_hooks
        sys.path[:] = state.path
        sys.path_importer_cache.clear()


sys.argv[0] = renpy_py
if renpy.ios:
    run_in_process()
else:
    main.main()
