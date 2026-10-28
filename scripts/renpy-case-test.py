# Checks filecase.py, which lets a Ren'Py game find its files in any case, on a case-sensitive disk. Runs under Python
# 2.7 and 3. scripts/renpy-case-test.sh runs it with every Python it finds.
#   python3 scripts/renpy-case-test.py [Ren'Py folder ...]
# A Ren'Py folder holds renpy.py and renpy/, such as an SDK or build/renpy/8. For each one, the test also runs Ren'Py's
# own transpath or transfn, path_to_gamedir, and presplash find_file, copied out of its source, through the patches.

from __future__ import print_function

import ast
import codecs
import errno
import io
import os
import pickle
import shutil
import sys
import tempfile
import time
import types
import unicodedata
import warnings

# codecs.open is deprecated on newer Pythons, but games still call it.
warnings.simplefilter("ignore", DeprecationWarning)

PY2 = sys.version_info[0] == 2
if PY2:
    import __builtin__ as builtins
else:
    import builtins

support = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Packages", "VNDegenKit", "Sources",
                       "EngineRenPy", "Support")

# Loads filecase.py the way launcher.py does.
case = types.ModuleType("_vndegen_case")
case.__file__ = os.path.join(support, "filecase.py")
with open(case.__file__) as f:
    exec(compile(f.read(), case.__file__, "exec"), case.__dict__)
sys.modules["_vndegen_case"] = case

failures = []


def check(condition, message):
    if not condition:
        failures.append(message)
        print("FAIL", message)


def raises(code, function, *args):
    try:
        function(*args)
    except EnvironmentError as error:
        return error.errno == code
    return False


def write(path, text):
    """Writes a file as named, past the resolver."""
    folder = os.path.dirname(path)
    if not case._is_dir(folder):
        case._makedirs(folder)
    with case._open(path, "w") as f:
        f.write(text)


def read(path, opener=None):
    # Looks open up at the call, after install replaced it.
    with (opener or builtins.open)(path) as f:
        return f.read()


def real_names(folder):
    return sorted(case._listdir(folder))


work = os.path.realpath(tempfile.mkdtemp())
if PY2:
    # Ren'Py 7 passes text paths, as Python 3 does.
    work = work.decode(sys.getfilesystemencoding())
try:
    base = os.path.join(work, "Base")
    saves = os.path.join(work, "data", "saves")
    outside = os.path.join(work, "outside")
    write(os.path.join(base, "Game", "Images", "BG.png"), "bg")
    write(os.path.join(base, "Game", "Script.rpy"), "script")
    write(os.path.join(base, "Game", "Twins", "a.txt"), "lower")
    write(os.path.join(base, "Game", "Twins", "A.txt"), "upper")
    write(os.path.join(saves, "1-1-LT1.save"), "old save")
    write(os.path.join(outside, "File.txt"), "outside")
    game = os.path.join(base, "game")
    images = os.path.join(game, "images")

    captured_open = builtins.open
    case.install(base, saves)
    check(builtins.open is case.open and os.stat is case.stat, "install replaced open and os.stat")

    # Reads and existence checks.
    check(read(os.path.join(images, "bg.PNG")) == "bg", "open reads a file in another case")
    check(read(os.path.join(images, "bg.PNG"), io.open) == u"bg", "io.open reads a file in another case")
    check(read(os.path.join(images, "bg.PNG"), codecs.open) == u"bg", "codecs.open reads a file in another case")
    check(os.path.exists(os.path.join(images, "BG.PNG")), "os.path.exists finds a file in another case")
    check(os.path.isfile(os.path.join(game, "SCRIPT.RPY")), "os.path.isfile finds a file in another case")
    check(os.path.isdir(images), "os.path.isdir finds a folder in another case")
    check(os.path.getsize(os.path.join(images, "bg.png")) == 2, "os.path.getsize follows")
    check(not os.path.exists(os.path.join(images, "missing.png")), "a missing file stays missing")
    check(not os.path.exists(os.path.join(outside.lower(), "file.txt")), "a path outside the folders passes through")
    check(os.listdir(images) == ["BG.png"], "os.listdir lists a folder in another case")
    if hasattr(os, "scandir"):
        check([entry.path for entry in os.scandir(images)] == [os.path.join(base, "Game", "Images", "BG.png")],
              "os.scandir lists a folder in another case, with real paths")
        import pathlib

        check(pathlib.Path(images, "bg.png").read_text() == "bg", "pathlib reads a file in another case")
    check(read(os.path.join(images, "bg.png").encode("utf-8")) == "bg", "a bytes path works")
    check(read(os.path.join(game, "twins", "a.txt")) == "lower", "an exact name wins over another case")
    check(read(os.path.join(game, "twins", "A.TXT")) == "upper",
          "among names that fold the same, the first sorted wins")
    if not PY2:
        with open(os.path.join(images, "BG.png")) as f:
            check(os.stat(f.fileno()).st_size == 2, "os.stat takes a file descriptor")

    # Writes replace a file in another case, and new files are found in any case.
    with open(os.path.join(images, "bg.png"), "w") as f:
        f.write("new bg")
    check(real_names(os.path.join(base, "Game", "Images")) == ["BG.png"], "a write replaces the file in another case")
    check(read(os.path.join(base, "Game", "Images", "BG.png")) == "new bg", "the write went to the existing file")
    with open(os.path.join(game, "IMAGES", "New.png"), "w") as f:
        f.write("new")
    check(real_names(os.path.join(base, "Game", "Images")) == ["BG.png", "New.png"],
          "a new file lands in the existing folder, with the game's spelling")
    check(read(os.path.join(images, "NEW.PNG")) == "new", "a file written after startup reads in another case")

    # Ren'Py's save: write a temporary file, remove the old save if it exists, rename.
    new = os.path.join(saves, "1-1-lt1.save.new")
    final = os.path.join(saves, "1-1-lt1.save")
    with open(new, "wb") as f:
        f.write(b"new save")
    if os.path.exists(final):
        os.unlink(final)
    os.rename(new, final)
    check(real_names(saves) == ["1-1-lt1.save"], "a save replaces the save in another case")
    with open(os.path.join(saves, "1-1-LT1.save.new"), "wb") as f:
        f.write(b"newer save")
    os.rename(os.path.join(saves, "1-1-LT1.save.new"), os.path.join(saves, "1-1-LT1.SAVE"))
    check(real_names(saves) == ["1-1-lt1.save"], "a rename onto a file in another case replaces it")
    check(read(os.path.join(saves, "1-1-lt1.save")) == "newer save", "the rename moved the new contents")
    if hasattr(os, "replace"):
        write(os.path.join(saves, "tmp"), "replaced")
        os.replace(os.path.join(saves, "TMP"), os.path.join(saves, "1-1-LT1.save"))
        check(real_names(saves) == ["1-1-lt1.save"], "os.replace onto a file in another case replaces it")
    write(os.path.join(saves, "case.txt"), "x")
    os.rename(os.path.join(saves, "case.txt"), os.path.join(saves, "Case.txt"))
    check(real_names(saves) == ["1-1-lt1.save", "Case.txt"], "a rename that only changes case changes it")
    os.remove(os.path.join(saves, "CASE.TXT"))
    check(real_names(saves) == ["1-1-lt1.save"], "os.remove removes a file in another case")

    # A rename that only changes case, asked through folders in another case.
    write(os.path.join(base, "Game", "Images", "Ren.txt"), "r")
    os.rename(os.path.join(images, "ren.txt"), os.path.join(images, "REN.TXT"))
    check("REN.TXT" in real_names(os.path.join(base, "Game", "Images")), "a case-only rename keeps the real folders")
    os.remove(os.path.join(images, "REN.TXT"))

    # A relative path from a current folder inside the game.
    here = os.getcwd()
    os.chdir(base)
    try:
        check(read(os.path.join("game", "images", "bg.PNG")) == "new bg", "a relative path reads a file in another case")
        with builtins.open(os.path.join("GAME", "IMAGES", "bg.png"), "w") as f:
            f.write("relative bg")
        check(real_names(os.path.join(base, "Game", "Images")) == ["BG.png", "New.png"],
              "a relative write replaces the file in another case")
        check(read(os.path.join(images, "BG.png")) == "relative bg", "the relative write went to the existing file")
        # A relative rename that only changes case.
        write(os.path.join(base, "Game", "Images", "Rel.txt"), "r")
        os.rename(os.path.join("Game", "Images", "Rel.txt"), os.path.join("game", "images", "REL.TXT"))
        check("REL.TXT" in real_names(os.path.join(base, "Game", "Images")), "a relative case-only rename changes the case")
        os.remove(os.path.join(images, "REL.TXT"))
        # Appending to a file in another case keeps the folder's listing.
        real_images = case._absolute(os.path.join(base, "Game", "Images"))
        os.path.exists(os.path.join(images, "bg.PNG"))
        listed = case.folders.get(real_images)
        with builtins.open(os.path.join("game", "images", "bg.png"), "a") as f:
            f.write("")
        check(listed is not None and case.folders.get(real_images) is listed, "an append keeps the folder's listing")
    finally:
        os.chdir(here)

    # A file descriptor opens as before.
    fd = os.open(os.path.join(base, "Game", "Fd.txt"), os.O_WRONLY | os.O_CREAT, 0o644)
    with os.fdopen(fd, "w") as f:
        f.write("fd")
    check(read(os.path.join(images, "..", "fd.TXT")) == "fd", "os.fdopen of a descriptor writes")
    os.remove(os.path.join(base, "Game", "Fd.txt"))
    if PY2:
        fd = os.open(os.path.join(base, "Game", "Fd.txt"), os.O_WRONLY | os.O_CREAT, 0o644)
        with io.open(long(fd), "w") as f:  # noqa: F821
            f.write(u"long")
        check(read(os.path.join(base, "Game", "Fd.txt")) == "long", "io.open of a Python 2 long descriptor writes")
        os.remove(os.path.join(base, "Game", "Fd.txt"))
    # A case-only rename of bytes paths.
    write(os.path.join(base, "Game", "Images", "Bytes.txt"), "b")
    os.rename(os.path.join(images, "bytes.txt").encode("utf-8"), os.path.join(images, "BYTES.TXT").encode("utf-8"))
    check("BYTES.TXT" in real_names(os.path.join(base, "Game", "Images")), "a case-only rename of bytes paths changes the case")
    os.remove(os.path.join(images, "BYTES.TXT"))

    # A call relative to a folder descriptor isn't resolved against the current folder.
    if hasattr(os, "O_DIRECTORY") and not PY2:
        other = os.path.join(work, "other")
        write(os.path.join(other, "save.dat"), "other")
        write(os.path.join(base, "Game", "Save.dat"), "game")
        fd = os.open(other, os.O_RDONLY | os.O_DIRECTORY)
        os.chdir(os.path.join(base, "Game"))
        try:
            os.rename("save.dat", "new.dat", src_dir_fd=fd, dst_dir_fd=fd)
        finally:
            os.chdir(here)
            os.close(fd)
        check(real_names(other) == ["new.dat"] and "Save.dat" in real_names(os.path.join(base, "Game")),
              "a rename at a folder descriptor moves that folder's file")
        os.remove(os.path.join(base, "Game", "Save.dat"))

    # Python 2's current folder comes back as bytes, which must not break a text path in a non-ASCII folder.
    japanese = os.path.join(base, "Game", u"\u65e5\u672c")
    write(os.path.join(japanese, "Existing.txt"), "jp")
    os.chdir(japanese if not PY2 else japanese.encode("utf-8"))
    try:
        with builtins.open(u"existing.TXT", "w") as f:
            f.write(u"rewritten")
        check(real_names(japanese) == ["Existing.txt"], "a relative write in a non-ASCII folder replaces the file")
    finally:
        os.chdir(here)
    shutil.rmtree(japanese)

    # A folder put in place of another with the same time is listed again.
    if not PY2:
        swapped = os.path.join(base, "Game", "Swapped")
        write(os.path.join(swapped, "Old.png"), "old")
        os.utime(swapped, ns=(1000000000 * 10 ** 9, 1000000000 * 10 ** 9))
        check(os.path.exists(os.path.join(game, "swapped", "old.png")), "a lookup lists the folder to be replaced")
        case._rename(swapped, swapped + "-gone")
        case._mkdir(swapped)
        with case._open(os.path.join(swapped, "New.png"), "w") as f:
            f.write("new")
        os.utime(swapped, ns=(1000000000 * 10 ** 9, 1000000000 * 10 ** 9))
        check(os.path.exists(os.path.join(game, "swapped", "new.PNG")), "a folder replaced with the same time is listed again")
        shutil.rmtree(swapped)
        shutil.rmtree(swapped + "-gone")

    # Folders.
    check(raises(errno.EEXIST, os.mkdir, os.path.join(game, "IMAGES")), "os.mkdir on a folder in another case fails")
    os.makedirs(os.path.join(game, "images", "Sub", "Deeper"))
    check(os.path.isdir(os.path.join(base, "Game", "Images", "Sub", "Deeper")), "os.makedirs extends the real folder")
    os.rmdir(os.path.join(game, "IMAGES", "SUB", "deeper"))
    check(real_names(os.path.join(base, "Game", "Images", "Sub")) == [], "os.rmdir removes a folder in another case")
    check(raises(errno.EEXIST, os.makedirs, os.path.join(game, "IMAGES", "sub")),
          "os.makedirs of an existing folder in another case finds it")
    check(real_names(os.path.join(base, "Game", "Images")) == ["BG.png", "New.png", "Sub"],
          "os.makedirs of an existing folder in another case adds nothing")

    # A file added under the same folder mtime as the cached listing, as on a disk with coarse timestamps: a miss in a
    # listing taken within two seconds of the folder's last change lists it again.
    real_images = os.path.join(base, "Game", "Images")
    os.utime(real_images, None)
    check(os.path.exists(os.path.join(images, "BG.PNG")), "a lookup lists the folder")
    before = case._stat(real_images)
    write(os.path.join(real_images, "Late.png"), "late")
    # Python 2 sets the mtime from a float, which may not round-trip, and then the changed mtime lists the folder again.
    if not PY2:
        os.utime(real_images, ns=(before.st_atime_ns, before.st_mtime_ns))
        check(case._stat(real_images).st_mtime_ns == before.st_mtime_ns, "the folder's mtime is back to the listed one")
    check(read(os.path.join(images, "late.png")) == "late", "a file added under an unchanged folder mtime is found")

    # Ren'Py's own modules hold the open they found at import, which follows each install.
    for function in (builtins.open, io.open, os.stat):
        check(pickle.loads(pickle.dumps(function)) is function, "Ren'Py's module backup can pickle " + repr(function))
    case.uninstall()
    check(builtins.open is captured_open and os.stat is case._stat and io.open is case._io_open,
          "uninstall restores open, io.open, and os.stat")
    held = case.open
    check(raises(errno.ENOENT, held, os.path.join(images, "bg.png")),
          "a held open passes paths through after uninstall")
    case.install(base, saves)
    check(read(os.path.join(images, "bg.png"), held) == "relative bg", "a held open follows the next install")

    # Ren'Py's own functions, copied out of its source.
    def functions(path, names, namespace):
        with open(path, "rb") as f:
            tree = ast.parse(f.read(), path)
        module = types.ModuleType("renpy_part")
        module.__dict__.update(namespace)
        tree.body = [node for node in tree.body if isinstance(node, ast.FunctionDef) and node.name in names]
        exec(compile(tree, path, "exec"), module.__dict__)
        return module

    class Config(object):
        reject_backslash = False

    class Renpy(object):
        pass

    for renpy_dir in sys.argv[1:]:
        source = os.path.join(renpy_dir, "renpy", "loader.py")
        try:
            with open(source, "rb") as f:
                ast.parse(f.read(), source)
        except SyntaxError:
            print("skipped", renpy_dir, "under Python %d: its source needs a newer Python" % sys.version_info[0])
            continue
        label = "Ren'Py " + os.path.basename(os.path.normpath(renpy_dir))
        config = Config()
        config.basedir = base
        config.searchpath = [os.path.join(base, "Game")]
        renpy = Renpy()
        renpy.config = config
        lower_map = {}
        loader = functions(source, ("transpath", "transfn"), {
            "renpy": renpy, "os": os, "unicodedata": unicodedata, "lower_map": lower_map, "add_auto": lambda path: None,
        })
        for folder, _subfolders, files in os.walk(os.path.join(base, "Game")):
            for name in files:
                fn = os.path.relpath(os.path.join(folder, name), os.path.join(base, "Game"))
                lower_map[unicodedata.normalize("NFC", fn.lower())] = fn
        write(os.path.join(base, "Game", "Images", "Runtime.png"), "runtime")
        write(os.path.join(base, "extras", "Bonus.png"), "bonus")
        config.searchpath.append("Extras")
        wrong = loader.transfn(u"images/runtime.PNG")
        check(wrong == os.path.join(base, "Game", "images", "runtime.PNG"),
              label + " transfn alone returns the game's spelling for a file written after indexing, got " + wrong)
        case.patch_loader(loader)
        case.patch_loader(loader)
        for name in (u"images/runtime.PNG", u"bonus.png", u"script.rpy"):
            path = loader.transfn(name)
            check(os.path.basename(path) in case._listdir(os.path.dirname(path)),
                  label + " transfn returns a real path for " + name + ", got " + path)
        case.unpatch_loader()
        check(loader.transfn(u"images/runtime.PNG") == wrong, label + " unpatch_loader restores the loader")

        main = functions(os.path.join(renpy_dir, "renpy.py"), ("path_to_gamedir",), {"os": os})
        check(case.resolve(main.path_to_gamedir(base, "renpy")) == os.path.join(base, "Game"),
              label + " path_to_gamedir with the resolver finds Game/")
        presplash = functions(os.path.join(renpy_dir, "renpy", "display", "presplash.py"), ("find_file",), {"os": os})
        write(os.path.join(base, "Game", "Presplash.PNG"), "image")
        case.patch_presplash(presplash)
        check(presplash.find_file("presplash", root=game) == os.path.join(base, "Game", "Presplash.PNG"),
              label + " presplash find_file returns the real path")
        case._remove(os.path.join(base, "Game", "Presplash.PNG"))
        shutil.rmtree(os.path.join(base, "extras"))
        case._remove(os.path.join(base, "Game", "Images", "Runtime.png"))
        print(label, "checked")
    case.uninstall()
finally:
    case.uninstall()
    shutil.rmtree(work)

print("Python %d.%d: %s" % (sys.version_info[0], sys.version_info[1], "FAIL" if failures else "PASS"))
sys.exit(1 if failures else 0)
