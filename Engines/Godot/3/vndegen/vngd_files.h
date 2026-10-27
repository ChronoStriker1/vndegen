// Windows ignores case in file names, so a game may ask for `Items/Sword.json` when the disk has `items/sword.json`.
// Files outside the pack, beside the game and in user://, are found whatever case the game names them in, through the
// lookup every engine shares. Lookups inside the pack stay exact, as they are on Windows. fix_path is virtual through
// patches/03-virtual-fix-path.patch.
#pragma once

#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"

#include <casepath.h>

#include <initializer_list>

inline String vngd_case_path(const String &p_path) {
	if (!p_path.is_abs_path()) {
		return p_path;
	}
	char found[4096];
	return casepath_resolve(p_path.utf8().get_data(), found, sizeof(found)) < 0 ? p_path : String::utf8(found);
}

// Every file Godot opens, tests, or times passes its path through fix_path.
class FileAccessVNDegen : public FileAccessUnix {
protected:
	String fix_path(const String &p_path) const override { return vngd_case_path(FileAccessUnix::fix_path(p_path)); }
};

// So does every folder it opens, lists, or changes, after relative paths are made absolute.
class DirAccessVNDegen : public DirAccessUnix {
protected:
	String fix_path(String p_path) const override {
		const String path = DirAccessUnix::fix_path(p_path);
		return vngd_case_path(path.is_rel_path() ? current_dir.plus_file(path).simplify_path() : path);
	}
};

inline void vngd_use_case_lookup() {
	for (FileAccess::AccessType type : { FileAccess::ACCESS_RESOURCES, FileAccess::ACCESS_USERDATA, FileAccess::ACCESS_FILESYSTEM }) {
		FileAccess::make_default<FileAccessVNDegen>(type);
	}
	for (DirAccess::AccessType type : { DirAccess::ACCESS_RESOURCES, DirAccess::ACCESS_USERDATA, DirAccess::ACCESS_FILESYSTEM }) {
		DirAccess::make_default<DirAccessVNDegen>(type);
	}
}
