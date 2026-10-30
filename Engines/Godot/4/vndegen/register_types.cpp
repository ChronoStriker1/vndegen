#include "register_types.h"

#include <vngd.h>

// The module registers no classes. It exists for vngd.h. Nothing in Godot calls those functions, so this table keeps
// a linker from leaving them out of a library built from Godot's static archives.
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

void initialize_vndegen_module(ModuleInitializationLevel p_level) {}
void uninitialize_vndegen_module(ModuleInitializationLevel p_level) {}
