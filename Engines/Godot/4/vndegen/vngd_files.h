// Windows ignores case in file names, so a game may ask for `Items/Sword.json` when the disk has `items/sword.json`.
// Files outside the pack, beside the game and in user://, are found whatever case the game names them in, through the
// lookup every engine shares. Lookups inside the pack stay exact, as they are on Windows.
#pragma once

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"

#include <casepath.h>

inline String vngd_case_path(const String &p_path) {
	if (!p_path.is_absolute_path()) {
		return p_path;
	}
	char found[4096];
	return casepath_resolve(p_path.utf8().get_data(), found, sizeof(found)) < 0 ? p_path : String::utf8(found);
}

// Every file Godot opens, tests, or times passes its path through fix_path.
class FileAccessVNDegen : public FileAccessUnix {
	GDSOFTCLASS(FileAccessVNDegen, FileAccessUnix);

protected:
	String fix_path(const String &p_path) const override { return vngd_case_path(FileAccessUnix::fix_path(p_path)); }
};

// So does every folder it opens, lists, or changes, after relative paths are made absolute. Dir is the platform's own
// folder access, which on Mac also normalizes listed names.
template <class Dir>
class DirAccessVNDegen : public Dir {
protected:
	String fix_path(const String &p_path) const override {
		const String path = Dir::fix_path(p_path);
		return vngd_case_path(path.is_relative_path() ? this->current_dir.path_join(path).simplify_path() : path);
	}
};

template <class Dir>
void vngd_use_case_lookup() {
	for (FileAccess::AccessType type : { FileAccess::ACCESS_RESOURCES, FileAccess::ACCESS_USERDATA, FileAccess::ACCESS_FILESYSTEM }) {
		FileAccess::make_default<FileAccessVNDegen>(type);
	}
	for (DirAccess::AccessType type : { DirAccess::ACCESS_RESOURCES, DirAccess::ACCESS_USERDATA, DirAccess::ACCESS_FILESYSTEM }) {
		DirAccess::make_default<DirAccessVNDegen<Dir>>(type);
	}
}
