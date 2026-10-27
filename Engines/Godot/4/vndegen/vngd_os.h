// The paths a game sees, the same on every platform: the game's .exe as the executable, and user:// where Windows
// keeps it, under VNDegen's stand-in for %APPDATA%. Its files are found whatever case the game names them in, as on
// Windows, through Dir, the platform's folder access, and vngd_files.h.
#pragma once

#include "vngd_files.h"

#include "core/string/ustring.h"

#include <vnfont.h>
#include <vngd.h>

template <class Base, class Dir = DirAccessUnix>
class OS_VNDegen : public Base {
	String executable;
	String data_path;

public:
	using Base::get_user_data_dir;

	String get_executable_path() const override { return executable; }
	String get_data_path() const override { return data_path; }
	// Godot on Windows names its folder this way, where Linux uses "godot".
	String get_godot_dir_name() const override { return "Godot"; }
	String get_user_data_dir(const String &p_user_dir) const override { return data_path.path_join(p_user_dir); }

	void initialize_core() override {
		Base::initialize_core();
		vngd_use_case_lookup<Dir>();
	}

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
