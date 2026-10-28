// Windows ignores case in file names, so a game may ask for `Items/Sword.json` when the disk has `items/sword.json`.
// Files outside the pack, beside the game and in user://, are found whatever case the game names them in, through the
// lookup every engine shares. Lookups inside the pack stay exact, as they are on Windows. Copying a file onto another
// spelling of itself is caught in DirAccess::copy, by a patch, since packs bring their own folder access. fix_path is virtual through
// patches/03-virtual-fix-path.patch.
#pragma once

#include "core/os/dir_access.h"
#include "core/os/file_access.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"

#include <casepath.h>

#include <initializer_list>
#include <unistd.h>

// `p_path` as it is on disk. A relative path is resolved from the working directory, as the system resolves it.
inline String vngd_case_path(const String &p_path) {
	if (p_path.empty()) {
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

public:
	// A link's relative target counts from the link's own folder, so it keeps its text.
	Error create_link(String p_source, String p_target) override {
		if (!p_source.is_rel_path()) {
			return DirAccessUnix::create_link(p_source, p_target);
		}
		return symlink(p_source.utf8().get_data(), fix_path(p_target).utf8().get_data()) == 0 ? OK : FAILED;
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
