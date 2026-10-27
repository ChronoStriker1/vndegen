# Finds a Ren'Py game's files whatever case the game uses. Valid Python 2.7 and 3, like launcher.py, which loads it.
#
# Windows ignores case in file names, so a game may ask for game/Images/BG.PNG when the disk has game/images/bg.png.
# iOS and case-sensitive Mac volumes don't ignore it. Ren'Py's loader maps names in any case to the files it found at
# startup, but not files written later, folders a game adds to its search path, or paths the game's own Python opens.
#
# install() wraps open and the os functions that take a path. A path under one of the folders it is given, the game's
# base folder and its saves folder, finds its file in any case. Other paths pass through untouched. A call on a path
# that exists as written costs the call itself, or one more lstat for a call that creates a file. A write or rename to
# a missing name whose case-folded name exists replaces that file, as on Windows.
#
# Ren'Py's modules keep the open they find when they are imported, so on iOS, where every game shares one interpreter,
# this module loads once and those copies follow whatever the current game installed. With nothing installed they pass
# every path through.

import errno
import io
import os
import stat as stat_module
import sys
import time
import unicodedata

if sys.version_info[0] == 2:
    import __builtin__ as builtins

    text_type = unicode  # noqa: F821
    filesystem_encoding = sys.getfilesystemencoding() or "utf-8"

    def decode(path):
        return path.decode(filesystem_encoding)

    def encode(path):
        return path.encode(filesystem_encoding)
else:
    import builtins

    text_type = str
    decode = os.fsdecode
    encode = os.fsencode

# The functions this replaces, saved when the module first loads. The resolver calls these, so it never calls itself.
_open = builtins.open
_io_open = io.open
_stat = os.stat
_lstat = os.lstat
_listdir = os.listdir
_scandir = getattr(os, "scandir", None)
_remove = os.remove
_unlink = os.unlink
_rename = os.rename
_replace = getattr(os, "replace", None)
_mkdir = os.mkdir
_makedirs = os.makedirs
_rmdir = os.rmdir

# (folder, folder + "/") for each folder whose paths are resolved.
roots = []

# folder -> (mtime, seconds between the folder's last change and its listing, names, case-folded name -> real name).
folders = {}


def fold(name):
    """Folds case the way Ren'Py's loader does for its own map of names."""
    return unicodedata.normalize("NFC", name.lower())


def _is_dir(path):
    try:
        return stat_module.S_ISDIR(_stat(path).st_mode)
    except EnvironmentError:
        return False


def _listing(folder, fresh):
    try:
        info = _stat(folder)
    except EnvironmentError:
        return None
    mtime = getattr(info, "st_mtime_ns", info.st_mtime)
    entry = folders.get(folder)
    if entry is None or entry[0] != mtime or fresh:
        try:
            names = _listdir(folder)
        except EnvironmentError:
            return None
        folded = {}
        # When several names fold the same, the first in sorted order wins.
        for name in sorted(name for name in names if isinstance(name, text_type)):
            folded.setdefault(fold(name), name)
        entry = (mtime, time.time() - info.st_mtime, set(names), folded)
        folders[folder] = entry
    return entry


def _find(folder, name):
    """The real name of `name` in `folder`, preferring the exact name, or None."""
    entry = _listing(folder, False)
    # A file system with coarse timestamps can change a folder again under the same mtime. A listing taken within two
    # seconds of the folder's last change may have missed that, so a miss lists again.
    if entry is not None and entry[1] < 2 and name not in entry[2] and fold(name) not in entry[3]:
        entry = _listing(folder, True)
    if entry is None:
        return None
    if name in entry[2]:
        return name
    return entry[3].get(fold(name))


def _resolve_text(path):
    for root, inside in roots:
        if path.startswith(inside):
            break
    else:
        return path
    try:
        _lstat(path)
        return path
    except EnvironmentError:
        pass
    normal = os.path.normpath(path)
    if not normal.startswith(inside):
        return path
    parts = normal[len(inside):].split(os.sep)
    # Starts from the deepest folder that exists as written.
    known = len(parts) - 1
    while known > 0 and not _is_dir(os.path.join(root, *parts[:known])):
        known -= 1
    folder = os.path.join(root, *parts[:known])
    changed = False
    for index in range(known, len(parts)):
        real = _find(folder, parts[index])
        if real is None:
            # A missing tail keeps the game's spelling.
            return os.path.join(folder, *parts[index:]) if changed else path
        changed = changed or real != parts[index]
        folder = os.path.join(folder, real)
    return folder if changed else path


def resolve(path):
    """
    The real path of `path` when it lies in an installed folder and names an existing file or folder in another case,
    or the existing part of it followed by the rest as written. Returns `path` itself, the same object, otherwise.
    """
    if not roots:
        return path
    if isinstance(path, text_type):
        return _resolve_text(path)
    if isinstance(path, bytes):
        try:
            text = decode(path)
        except UnicodeError:
            return path
        real = _resolve_text(text)
        return path if real is text else encode(real)
    fspath = getattr(path, "__fspath__", None)
    if fspath is not None:
        text = fspath()
        real = resolve(text)
        return path if real is text else real
    return path


def _retry(function, path, args, kwargs):
    """Calls `function` on `path` as written, and on its real path if that is missing."""
    try:
        return function(path, *args, **kwargs)
    except EnvironmentError as error:
        if error.errno not in (errno.ENOENT, errno.ENOTDIR):
            raise
        real = resolve(path)
        if real is path:
            raise
    return function(real, *args, **kwargs)


def _creates(args, kwargs):
    mode = args[0] if args else kwargs.get("mode", "r")
    try:
        return "w" in mode or "a" in mode or "x" in mode
    except TypeError:
        return False


def open(file, *args, **kwargs):
    if _creates(args, kwargs):
        return _open(resolve(file), *args, **kwargs)
    return _retry(_open, file, args, kwargs)


def io_open(file, *args, **kwargs):
    if _creates(args, kwargs):
        return _io_open(resolve(file), *args, **kwargs)
    return _retry(_io_open, file, args, kwargs)


def stat(path, *args, **kwargs):
    return _retry(_stat, path, args, kwargs)


def lstat(path, *args, **kwargs):
    return _retry(_lstat, path, args, kwargs)


def listdir(path="."):
    return _retry(_listdir, path, (), {})


def scandir(path="."):
    return _retry(_scandir, path, (), {})


def remove(path, *args, **kwargs):
    return _retry(_remove, path, args, kwargs)


def unlink(path, *args, **kwargs):
    return _retry(_unlink, path, args, kwargs)


def rmdir(path, *args, **kwargs):
    return _retry(_rmdir, path, args, kwargs)


def _destination(real_src, dst):
    real = resolve(dst)
    # A rename that only changes case finds its own source, and keeps the new spelling.
    return dst if real is not dst and real == real_src else real


def rename(src, dst, *args, **kwargs):
    real_src = resolve(src)
    return _rename(real_src, _destination(real_src, dst), *args, **kwargs)


def replace(src, dst, *args, **kwargs):
    real_src = resolve(src)
    return _replace(real_src, _destination(real_src, dst), *args, **kwargs)


def mkdir(path, *args, **kwargs):
    return _mkdir(resolve(path), *args, **kwargs)


def makedirs(name, *args, **kwargs):
    return _makedirs(resolve(name), *args, **kwargs)


# (owner, attribute, original, replacement). Python 3's builtins.open is io.open. genericpath's exists, isfile, isdir,
# getsize, and getmtime call os.stat, and os.walk calls os.scandir or os.listdir, so they follow.
_patches = [
    (builtins, "open", _open, open),
    (io, "open", _io_open, io_open if _io_open is not _open else open),
    (os, "stat", _stat, stat),
    (os, "lstat", _lstat, lstat),
    (os, "listdir", _listdir, listdir),
    (os, "remove", _remove, remove),
    (os, "unlink", _unlink, unlink),
    (os, "rmdir", _rmdir, rmdir),
    (os, "rename", _rename, rename),
    (os, "mkdir", _mkdir, mkdir),
    (os, "makedirs", _makedirs, makedirs),
]
if _scandir is not None:
    _patches.append((os, "scandir", _scandir, scandir))
if _replace is not None:
    _patches.append((os, "replace", _replace, replace))


def install(*paths):
    """Resolves paths under `paths` from now on, replacing the folders an earlier install gave."""
    texts = [decode(path) if isinstance(path, bytes) else path for path in paths]
    normal = [os.path.normpath(os.path.abspath(path)) for path in texts]
    roots[:] = [(root, os.path.join(root, "")) for root in normal]
    folders.clear()
    for owner, name, _original, replacement in _patches:
        setattr(owner, name, replacement)


def uninstall():
    del roots[:]
    folders.clear()
    for owner, name, original, _replacement in _patches:
        setattr(owner, name, original)
    unpatch_loader()


# Ren'Py hands the path its loader finds to C code, such as SDL's file reading, which needs the real name. Ren'Py 8
# finds it with renpy.loader.transpath, which its transfn calls, and Ren'Py 7 with transfn.
_loader = []


def loader_path(name):
    return resolve(_loader[2](name))


def patch_loader(loader):
    """
    Makes the loader return real paths, for files written after Ren'Py indexed the game and for folders a game adds to
    its search path. Call it after renpy.import_all, which backs up Ren'Py's modules: a backup holds functions by name.
    """
    name = "transpath" if hasattr(loader, "transpath") else "transfn"
    function = getattr(loader, name)
    if function is not loader_path:
        _loader[:] = [loader, name, function]
        setattr(loader, name, loader_path)


def unpatch_loader():
    if _loader:
        loader, name, function = _loader
        setattr(loader, name, function)
        del _loader[:]


_presplash = []


def presplash_file(base_name, root):
    return resolve(_presplash[0](base_name, root))


def patch_presplash(presplash):
    """Makes the presplash image, which SDL loads, use its real path. macOS only: Ren'Py shows no presplash on iOS."""
    if presplash.find_file is not presplash_file:
        _presplash[:] = [presplash.find_file]
        presplash.find_file = presplash_file
