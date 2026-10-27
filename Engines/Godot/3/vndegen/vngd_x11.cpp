// Linux, for testing the C interface on a Linux build. Games open their own X11 window there. Godot's X11 OS starts and
// ends the main loop in its own run(), which VNDegen doesn't call, so this does that part.
#include "vngd_files.h"
#include "vngd_platform.h"

#include "core/os/main_loop.h"
#include "main/main.h"

#include <vngd.h>

#include "platform/x11/os_x11.h"

namespace {

class OS_VNDegenX11 : public OS_X11 {
	String executable;
	String data_path;

public:
	String get_executable_path() const override { return executable; }
	String get_data_path() const override { return data_path; }
	// Godot on Windows names its folder this way, where Linux uses "godot".
	String get_godot_dir_name() const override { return "Godot"; }

	// Godot's X11 alert is a dialog that blocks the test run.
	void alert(const String &p_alert, const String &p_title = "ALERT!") override {
		ERR_PRINT(p_title + ": " + p_alert);
	}

protected:
	void initialize_core() override {
		OS_X11::initialize_core();
		vngd_use_case_lookup();
	}

public:
	OS_VNDegenX11(const VNGDLaunch *p_launch) {
		executable = String::utf8(p_launch->executable);
		data_path = String::utf8(p_launch->dataDir);
	}
};

} // namespace

OS *vngd_platform_create_os(const VNGDLaunch *p_launch) {
	return new OS_VNDegenX11(p_launch);
}

// Tests run without sound.
const char *vngd_platform_audio_driver() {
	return "Dummy";
}

void vngd_platform_started() {
	OS::get_singleton()->get_main_loop()->init();
}

void vngd_platform_stopping() {
	OS::get_singleton()->get_main_loop()->finish();
}

void vngd_platform_begin_frame() {
	static_cast<OS_X11 *>(OS::get_singleton())->force_process_input();
}

void vngd_platform_resize(float p_width, float p_height, float p_scale) {}
void vngd_platform_set_audio_paused(bool p_paused) {}
