// vngd.h for Godot 3: starting and stopping Godot, and turning VNDegen's input into Godot's input events. vngd_platform.h
// declares what each platform adds.

#include "vngd_keys.h"
#include "vngd_platform.h"

#include "core/os/input_event.h"
#include "main/input_default.h"
#include "main/main.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"

#include <vngd.h>

#include <string>
#include <vector>

namespace {

struct State {
	OS *os = nullptr;
	bool setup = false;
	bool started = false;
	bool paused = false;
	std::string problem;
	Vector2 mouse = Vector2(-1, -1);
	int buttons = 0;
};

State state;

InputDefault *input() {
	return static_cast<InputDefault *>(Input::get_singleton());
}

void parse(const Ref<InputEvent> &p_event) {
	input()->parse_input_event(p_event);
}

Vector2 window_point(float p_x, float p_y) {
	Size2 size = state.os->get_window_size();
	return Vector2(p_x * size.width, p_y * size.height);
}

void mouse_button(int p_button, bool p_down, float p_factor = 1) {
	if (state.mouse.x < 0) {
		state.mouse = state.os->get_window_size() / 2;
	}
	if (p_button <= BUTTON_MIDDLE) {
		int mask = 1 << (p_button - 1);
		state.buttons = p_down ? state.buttons | mask : state.buttons & ~mask;
	}
	Ref<InputEventMouseButton> event;
	event.instance();
	event->set_button_index(p_button);
	event->set_pressed(p_down);
	event->set_factor(p_factor);
	event->set_position(state.mouse);
	event->set_global_position(state.mouse);
	event->set_button_mask(state.buttons);
	parse(event);
}

void notify(int p_notification) {
	if (MainLoop *loop = state.os->get_main_loop()) {
		loop->notification(p_notification);
	}
}

} // namespace

extern "C" {

int vngd_start(const VNGDLaunch *p_launch) {
	ERR_FAIL_COND_V_MSG(state.os, 1, "A game is still running. Call vngd_stop first.");
	state = State();
	state.os = vngd_platform_create_os(p_launch);

	std::vector<std::string> args = { "--main-pack", p_launch->pack, "--audio-driver", vngd_platform_audio_driver() };
	std::vector<char *> argv;
	for (std::string &arg : args) {
		argv.push_back(&arg[0]);
	}
	argv.push_back(nullptr);

	if (Main::setup(p_launch->executable, (int)args.size(), argv.data(), true) != OK) {
		state.problem = "The game's files could not be loaded.";
		return 1;
	}
	state.setup = true;
	if (!Main::start()) {
		state.problem = "The game did not start.";
		return 1;
	}
	vngd_platform_started();
	state.started = true;
	// VNDegen's controller. With no mapping for it, Godot passes its button and axis numbers on as they are.
	input()->joy_connection_changed(0, true, "Xbox Controller", "");
	return 0;
}

// For VNDegen's tests on macOS, where a window snapshot can't see what the game draws: with VNGD_SNAPSHOT set to a path,
// the frame the game drew is written there as a PNG every two seconds.
static void write_snapshot() {
	static const String path = OS::get_singleton()->get_environment("VNGD_SNAPSHOT");
	static uint64_t last = 0;
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	if (path.empty() || now - last < 2000) {
		return;
	}
	last = now;
	Ref<Image> image = SceneTree::get_singleton()->get_root()->get_texture()->get_data();
	if (image.is_valid()) {
		// OpenGL's rows run bottom up.
		image->flip_y();
		image->save_png(path);
	}
}

int vngd_iterate(void) {
	if (!state.started) {
		return 1;
	}
	if (state.paused) {
		return 0;
	}
	vngd_platform_begin_frame();
	input()->flush_buffered_events();
	const bool quit = Main::iteration();
	write_snapshot();
	return quit ? 1 : 0;
}

void vngd_resize(float p_width, float p_height, float p_scale) {
	if (state.started) {
		vngd_platform_resize(p_width, p_height, p_scale);
	}
}

void vngd_key(int p_hid_usage, bool p_down) {
	if (!state.started) {
		return;
	}
	uint32_t key = vngd_hid_to_key(p_hid_usage);
	if (!key) {
		return;
	}
	Ref<InputEventKey> event;
	event.instance();
	event->set_scancode(key);
	event->set_physical_scancode(key);
	event->set_pressed(p_down);
	event->set_shift(key == KEY_SHIFT ? p_down : input()->is_key_pressed(KEY_SHIFT));
	event->set_control(key == KEY_CONTROL ? p_down : input()->is_key_pressed(KEY_CONTROL));
	event->set_alt(key == KEY_ALT ? p_down : input()->is_key_pressed(KEY_ALT));
	event->set_metakey(key == KEY_META ? p_down : input()->is_key_pressed(KEY_META));
	parse(event);
}

void vngd_text(const char *p_utf8) {
	if (!state.started) {
		return;
	}
	String text = String::utf8(p_utf8);
	for (int i = 0; i < text.length(); i++) {
		CharType c = text[i];
		Ref<InputEventKey> event;
		event.instance();
		event->set_pressed(true);
		// Return and Backspace edit text through their keys, as on a keyboard.
		if (c == '\n' || c == '\r') {
			event->set_scancode(KEY_ENTER);
		} else if (c == '\b') {
			event->set_scancode(KEY_BACKSPACE);
		} else {
			event->set_unicode(c);
		}
		parse(event);
		Ref<InputEventKey> release = event->duplicate();
		release->set_pressed(false);
		parse(release);
	}
}

void vngd_mouse_move(float p_x, float p_y) {
	if (!state.started) {
		return;
	}
	Vector2 position = window_point(p_x, p_y);
	Vector2 relative = state.mouse.x < 0 ? Vector2() : position - state.mouse;
	state.mouse = position;
	Ref<InputEventMouseMotion> event;
	event.instance();
	event->set_position(position);
	event->set_global_position(position);
	event->set_relative(relative);
	event->set_button_mask(state.buttons);
	input()->set_mouse_position(position);
	parse(event);
}

void vngd_mouse_button(int p_button, bool p_down) {
	if (state.started) {
		mouse_button(p_button == 2 ? BUTTON_RIGHT : p_button == 3 ? BUTTON_MIDDLE : BUTTON_LEFT, p_down);
	}
}

void vngd_wheel(float p_notches) {
	if (!state.started || p_notches == 0) {
		return;
	}
	// Turned away from the player scrolls up, as a mouse wheel does.
	int button = p_notches > 0 ? BUTTON_WHEEL_UP : BUTTON_WHEEL_DOWN;
	mouse_button(button, true, Math::abs(p_notches));
	mouse_button(button, false, Math::abs(p_notches));
}

void vngd_touch(int p_index, int p_phase, float p_x, float p_y) {
	if (!state.started) {
		return;
	}
	Vector2 position = window_point(p_x, p_y);
	if (p_phase == 1) {
		Ref<InputEventScreenDrag> event;
		event.instance();
		event->set_index(p_index);
		event->set_position(position);
		parse(event);
		return;
	}
	Ref<InputEventScreenTouch> event;
	event.instance();
	event->set_index(p_index);
	event->set_position(position);
	event->set_pressed(p_phase == 0);
	parse(event);
}

void vngd_joy_button(int p_button, bool p_down) {
	if (!state.started) {
		return;
	}
	// vngd.h numbers buttons as Godot 4 and SDL do. Godot 3 numbers them its own way.
	static const int buttons[] = {
		JOY_BUTTON_0, JOY_BUTTON_1, JOY_BUTTON_2, JOY_BUTTON_3, JOY_SELECT, JOY_GUIDE, JOY_START, JOY_L3, JOY_R3,
		JOY_L, JOY_R, JOY_DPAD_UP, JOY_DPAD_DOWN, JOY_DPAD_LEFT, JOY_DPAD_RIGHT,
	};
	if (p_button >= 0 && p_button < (int)(sizeof(buttons) / sizeof(buttons[0]))) {
		input()->joy_button(0, buttons[p_button], p_down);
	}
}

void vngd_joy_axis(int p_axis, float p_value) {
	if (!state.started) {
		return;
	}
	static const int axes[] = { JOY_ANALOG_LX, JOY_ANALOG_LY, JOY_ANALOG_RX, JOY_ANALOG_RY, JOY_ANALOG_L2, JOY_ANALOG_R2 };
	if (p_axis >= 0 && p_axis < 6) {
		input()->joy_axis(0, axes[p_axis], p_value);
		// Godot 3 games read the triggers as buttons as often as axes, as its own XInput driver reports them.
		if (p_axis >= VNGD_AXIS_TRIGGER_LEFT) {
			input()->joy_button(0, p_axis == VNGD_AXIS_TRIGGER_LEFT ? JOY_L2 : JOY_R2, p_value > 0.5);
		}
	}
}

void vngd_set_paused(bool p_paused) {
	if (!state.started || state.paused == p_paused) {
		return;
	}
	state.paused = p_paused;
	// As when a PC game's window loses focus.
	notify(p_paused ? MainLoop::NOTIFICATION_WM_FOCUS_OUT : MainLoop::NOTIFICATION_WM_FOCUS_IN);
	vngd_platform_set_audio_paused(p_paused);
}

void vngd_request_quit(void) {
	// What the window's close button does: games see NOTIFICATION_WM_QUIT_REQUEST, and the tree quits unless the game
	// turned auto_accept_quit off.
	if (state.started) {
		notify(MainLoop::NOTIFICATION_WM_QUIT_REQUEST);
	}
}

void vngd_stop(void) {
	if (!state.os) {
		return;
	}
	if (state.started) {
		vngd_platform_stopping();
	}
	if (state.setup) {
		Main::cleanup();
	}
	delete state.os;
	std::string problem = state.problem;
	state = State();
	state.problem = problem;
}

const char *vngd_problem(void) {
	return state.problem.empty() ? nullptr : state.problem.c_str();
}

} // extern "C"
