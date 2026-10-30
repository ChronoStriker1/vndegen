// The paths a game sees, the same on every platform: the game's .exe as the executable, and user:// where Windows
// keeps it, under VNDegen's stand-in for %APPDATA%. Each platform's OS installs vngd_files.h's case lookup when its file
// access starts, since Godot's OS classes start it in different ways.
#pragma once

#include "vngd_files.h"

#include "core/os/os.h"
#include "core/string/ustring.h"

#include <vnfont.h>
#include <vngd.h>

template <class Base>
class OS_VNDegen : public Base {
	String executable;
	String data_path;

public:
	using Base::get_user_data_dir;

	String get_executable_path() const override { return executable; }
	// Files under res:// that the pack doesn't hold are in the game's folder. Windows starts a game in its own folder,
	// and Godot finds those files from there, which is where games that write to res:// keep their saves.
	String get_resource_dir() const override { return executable.get_base_dir(); }
	String get_data_path() const override { return data_path; }
	// Godot on Windows names its folder this way, where Linux uses "godot".
	String get_godot_dir_name() const override { return "Godot"; }
	// In its spelling on disk, which a backup from another platform can differ in.
	String get_user_data_dir(const String &p_user_dir) const override { return vngd_case_path(data_path.path_join(p_user_dir)); }

	// Godot's own alert is a modal dialog, which would stop VNDegen's app. VNDegen reports a game that can't start
	// itself, so alerts go to the log.
	void alert(const String &p_alert, const String &p_title = "ALERT!") override { OS::alert(p_alert, p_title); }

	// A Windows font the game asks for by name, such as through a SystemFont, comes from VNDegen's substitutes first.
	String get_system_font_path(const String &p_font_name, int p_weight = 400, int p_stretch = 100, bool p_italic = false) const override {
		vnfont_match match;
		if (vnfont_resolve(p_font_name.utf8().get_data(), p_weight >= 600, p_italic, &match)) {
			return String::utf8(match.path);
		}
		return Base::get_system_font_path(p_font_name, p_weight, p_stretch, p_italic);
	}

	template <class... Args>
	OS_VNDegen(const VNGDLaunch *p_launch, Args... p_args) :
			Base(p_args...) {
		executable = String::utf8(p_launch->executable);
		data_path = String::utf8(p_launch->dataDir);
	}
};
