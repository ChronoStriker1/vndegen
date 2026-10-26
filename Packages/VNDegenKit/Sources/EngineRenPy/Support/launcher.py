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
# Ren'Py on iOS logs to stdout already, and with this set it would drop what the game prints.
if not os.environ.get("RENPY_PLATFORM", "").startswith("ios"):
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


def use_python2_division():
    """
    Ren'Py 7 games are Python 2, where dividing two integers gives an integer, such as range(n / 3). Ren'Py 8 fixes some
    Python 2 syntax but not that, so in the game's own code each / becomes a call that divides the Python 2 way. Ren'Py's
    own code keeps Python 3 division. Returns a function that undoes this.
    """
    import __future__
    import ast
    import builtins
    import renpy.python

    original_compile = renpy.python.py_compile
    original_wrap = renpy.python.wrap_node
    compiling_game = [False]

    def python2_divide(a, b):
        if isinstance(a, int) and isinstance(b, int):
            return a // b
        return a / b

    # obj.name /= value and obj[key] /= value evaluate obj and key once, as the augmented assignment does.
    def python2_divide_attribute(obj, name, value):
        setattr(obj, name, python2_divide(getattr(obj, name), value))

    def python2_divide_item(obj, key, value):
        obj[key] = python2_divide(obj[key], value)

    def call(function, *args):
        return ast.Call(func=ast.Name(id="_vndegen_python2_" + function, ctx=ast.Load()), args=list(args), keywords=[])

    class Python2Division(ast.NodeTransformer):
        def visit_BinOp(self, node):
            self.generic_visit(node)
            if isinstance(node.op, ast.Div):
                return ast.copy_location(call("divide", node.left, node.right), node)
            return node

        def visit_AugAssign(self, node):
            self.generic_visit(node)
            if not isinstance(node.op, ast.Div):
                return node
            target = node.target
            if isinstance(target, ast.Name):
                value = call("divide", ast.Name(id=target.id, ctx=ast.Load()), node.value)
                replacement = ast.Assign(targets=[target], value=value)
            elif isinstance(target, ast.Attribute):
                replacement = ast.Expr(call("divide_attribute", target.value, ast.Constant(target.attr), node.value))
            elif isinstance(target, ast.Subscript) and not isinstance(target.slice, ast.Slice):
                replacement = ast.Expr(call("divide_item", target.value, target.slice, node.value))
            else:
                return node
            return ast.copy_location(replacement, node)

    def future_division(tree):
        return any(isinstance(node, ast.ImportFrom) and node.module == "__future__"
                   and any(alias.name == "division" for alias in node.names) for node in ast.walk(tree))

    class WrapGameCode(object):
        def visit(self, tree):
            if compiling_game[0] and not future_division(tree):
                tree = ast.fix_missing_locations(Python2Division().visit(tree))
            return original_wrap.visit(tree)

    # A file can ask for Python 3 division itself, with rpy python 3 or rpy python division.
    division_flag = __future__.division.compiler_flag

    def py_compile(source, mode, filename="<none>", *args, **kwargs):
        name = getattr(source, "filename", None) or filename
        compiling_game[0] = (str(name).replace("\\", "/").startswith("game/")
                             and not renpy.python.file_compiler_flags.get(name, 0) & division_flag)
        try:
            return original_compile(source, mode, filename, *args, **kwargs)
        finally:
            compiling_game[0] = False

    helpers = {
        "_vndegen_python2_divide": python2_divide,
        "_vndegen_python2_divide_attribute": python2_divide_attribute,
        "_vndegen_python2_divide_item": python2_divide_item,
    }

    def clear_compile_caches():
        # Compiled code is cached by file name, line, and flags, not by this division, and the last game can have
        # compiled a file of the same name.
        renpy.python.py_compile_cache.clear()
        renpy.python.old_py_compile_cache.clear()

    for name, helper in helpers.items():
        setattr(builtins, name, helper)
    renpy.python.py_compile = py_compile
    renpy.python.wrap_node = WrapGameCode()
    clear_compile_caches()

    def undo():
        renpy.python.py_compile = original_compile
        renpy.python.wrap_node = original_wrap
        for name in helpers:
            delattr(builtins, name)
        clear_compile_caches()

    return undo


def forget_modules(basedir):
    """
    Drops the modules a game brought along, such as a requests in game/python-packages or in an archive, so the next
    game imports its own. Their submodules go too, including ones without a file, such as those six creates for six.moves. Ren'Py's and
    the standard library's modules stay, since other modules hold on to them.
    """
    inside = os.path.join(os.path.realpath(basedir), "")

    def from_game(path):
        # Ren'Py's importer names modules it loads from the game's archives $game/<path>.
        return bool(path) and (path.startswith("$game/") or os.path.realpath(path).startswith(inside))

    def owned(module):
        spec = getattr(module, "__spec__", None)
        paths = [getattr(module, "__file__", None), getattr(spec, "origin", None)] + list(getattr(module, "__path__", None) or [])
        return any(from_game(path) for path in paths if isinstance(path, str))

    names = [name for name, module in list(sys.modules.items()) if module is not None and owned(module)]
    for name in list(sys.modules):
        if any(name == root or name.startswith(root + ".") for root in names):
            del sys.modules[name]


def show_view_in_hub(host):
    """
    VNDegen shows Ren'Py's view inside its own window once the view exists. On mobile Ren'Py opens its display before
    the game's init code runs, so this can't wait for zz_vndegen.rpy. Ren'Py's reload keeps classes, so the patch wraps
    the original it saved.
    """
    Interface = renpy.display.core.Interface
    set_mode = getattr(Interface.set_mode, "vndegen_original", Interface.set_mode)

    def set_mode_and_show(self):
        set_mode(self)
        host.vndegen_renpy_window_ready()

    set_mode_and_show.vndegen_original = set_mode
    Interface.set_mode = set_mode_and_show


def answer_host(host):
    """
    VNDegen runs the game inside the app, so it asks through its host instead of with a signal: to open its menu, to
    quit, or to pause while VNDegen covers the game, such as with the unlock screen. This starts before the game's init
    code runs, so a game that stops with an error there can still be paused and quit.
    """

    def check_host():
        requests = host.vndegen_renpy_take_requests()
        if requests & 2:
            hook = sys.modules.get("store._vndegen")
            if getattr(hook, "quit_game", None):
                hook.quit_game()
            else:
                renpy.exports.quit()
        elif requests & 1:
            renpy.exports.queue_event("vndegen_menu")
        if host.vndegen_renpy_is_paused():
            renpy.audio.audio.pause_all()
            host.vndegen_renpy_wait_while_paused()
            renpy.audio.audio.unpause_all()

    renpy.config.periodic_callbacks.append(check_host)


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

    import ctypes

    host = ctypes.CDLL(None)
    show_view_in_hub(host)
    undo_division = use_python2_division() if renpy7_game else None
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
                answer_host(host)
                renpy.main.main()
            except renpy.game.UtterRestartException:
                renpy.reload_all()
                # The reload brings back Ren'Py's own transformer.
                if undo_division:
                    undo_division()
                    undo_division = use_python2_division()
                restart = True
            except renpy.game.QuitException:
                pass
            except renpy.game.ParseErrorException:
                pass
            except Exception as e:
                renpy.error.report_exception(e, editor=True)
    finally:
        # Ren'Py stops waiting for an autosave after 3 seconds when a game quits, but the autosave goes on writing
        # through Ren'Py's save location, which the next game shares. So the game ends once it is done, and the hub
        # keeps running meanwhile.
        while not renpy.loadsave.autosave_not_running.is_set():
            host.vndegen_renpy_pump()
        renpy.display.tts.tts(None)
        if renpy.display.draw:
            renpy.display.im.cache.quit()
            renpy.display.draw.quit()
            renpy.display.draw = None
        renpy.audio.audio.quit()
        for callback in renpy.config.python_exit_callbacks:
            callback()
        if undo_division:
            undo_division()
        forget_modules(basedir)
        sys.meta_path[:] = state.meta_path
        sys.path_hooks[:] = state.path_hooks
        sys.path[:] = state.path
        sys.path_importer_cache.clear()


# Set for a Ren'Py 7 game on Ren'Py 8, which zz_vndegen.rpy also reads.
renpy7_game = os.environ.get("VNDEGEN_RENPY7_GAME") == "1" and sys.version_info[0] >= 3

sys.argv[0] = renpy_py
if renpy.ios:
    run_in_process()
else:
    if renpy7_game:
        # Ren'Py's modules can only be imported once its bootstrap has started importing them.
        import_all = renpy.import_all

        def import_all_then_divide():
            import_all()
            use_python2_division()

        renpy.import_all = import_all_then_divide
    main.main()
