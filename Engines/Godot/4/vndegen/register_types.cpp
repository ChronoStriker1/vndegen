#include "register_types.h"

#include <vngd.h>

#include "../../vngd_steam.h"
#include "vngd_eos.h"

// The module exists for vngd.h, and registers stand-ins for the extensions games ship that VNDegen can't load.
// Nothing in Godot calls the vngd.h functions, so this table keeps a linker from leaving them out of a library built
// from Godot's static archives.
__attribute__((used)) static const void *vngd_functions[] = {
	(const void *)&vngd_start,
	(const void *)&vngd_iterate,
	(const void *)&vngd_resize,
	(const void *)&vngd_key,
	(const void *)&vngd_text,
	(const void *)&vngd_mouse_move,
	(const void *)&vngd_mouse_button,
	(const void *)&vngd_wheel,
	(const void *)&vngd_touch,
	(const void *)&vngd_joy_button,
	(const void *)&vngd_joy_axis,
	(const void *)&vngd_set_paused,
	(const void *)&vngd_set_speed,
	(const void *)&vngd_request_quit,
	(const void *)&vngd_stop,
	(const void *)&vngd_problem,
};

bool vngd::steam_wanted = false;
bool vngd::eos::wanted = false;
static vngd::Steam *steam = nullptr;
static vngd::eos::IEOSStandIn *eos = nullptr;

void initialize_vndegen_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE && vngd::steam_wanted) {
		GDREGISTER_CLASS(vngd::Steam);
		steam = memnew(vngd::Steam);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Steam", steam));
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE && vngd::eos::wanted) {
		eos = vngd::eos::start();
	}
}

void uninitialize_vndegen_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE && steam) {
		Engine::get_singleton()->remove_singleton("Steam");
		memdelete(steam);
		steam = nullptr;
	}
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE && eos) {
		Engine::get_singleton()->remove_singleton("IEOS");
		memdelete(eos);
		eos = nullptr;
	}
}
