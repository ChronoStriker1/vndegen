#include "register_types.h"

#include <vngd.h>

#include "../../vngd_steam.h"

// The module exists for vngd.h, and registers GodotSteam's Steam class for the games that use it.
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
	(const void *)&vngd_request_quit,
	(const void *)&vngd_stop,
	(const void *)&vngd_problem,
};

bool vngd::steam_wanted = false;
static vngd::Steam *steam = nullptr;

void register_vndegen_types() {
	if (vngd::steam_wanted) {
		ClassDB::register_class<vngd::Steam>();
		steam = memnew(vngd::Steam);
		Engine::get_singleton()->add_singleton(Engine::Singleton("Steam", steam));
	}
}

// Godot frees the Engine, with its list of singletons, and every registered class when the game stops.
void unregister_vndegen_types() {
	if (steam) {
		memdelete(steam);
		steam = nullptr;
	}
}

#if defined(IPHONE_ENABLED)
// Godot's iOS export generates these for the game's iOS plugins. VNDegen leaves out Godot's iOS platform code, and runs
// Windows games, which have none.
void godot_ios_plugins_initialize() {}
void godot_ios_plugins_deinitialize() {}
#endif
