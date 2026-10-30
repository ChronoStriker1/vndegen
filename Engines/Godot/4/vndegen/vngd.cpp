// The parts of vngd.h every platform shares: starting and stopping Godot, and turning VNDegen's input into Godot's
// input events. vngd_platform.h declares what each platform adds.

#include "vngd_platform.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "main/main.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "scene/main/viewport.h"
#include "servers/audio/audio_server.h"
#include "servers/display/display_server.h"
#include "servers/movie_writer/movie_writer.h"

#include <vnfont.h>
#include <vngd.h>

#include "../../vngd_frames.h"
#include "../../vngd_steam.h"
#include "vngd_eos.h"

#include <cstring>
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
	BitField<MouseButtonMask> buttons = MouseButtonMask::NONE;
	void (*keyboard)(bool, void *) = nullptr;
	void *context = nullptr;
};

State state;
VNGDFrames frames;

// A start that failed after setup's first phase leaves the engine half started, and Godot can't take that apart, so
// Godot stays stopped until VNDegen is opened again. What it made is left in place, since its servers still use the OS.
bool half_started = false;
constexpr const char *half_started_problem = "Godot could not start this game. Open VNDegen again to play a Godot game.";

// The device number of VNDegen's controller.
constexpr int joypad = 0;

Vector2 window_point(float p_x, float p_y) {
	Size2i size = DisplayServer::get_singleton()->window_get_size();
	return Vector2(p_x * size.width, p_y * size.height);
}

void parse(const Ref<InputEvent> &p_event) {
	// For VNDegen's tests: VNGD_TRACE_INPUT=1 logs each event VNDegen sends.
	static const bool trace = OS::get_singleton()->get_environment("VNGD_TRACE_INPUT") == "1";
	if (trace) {
		print_line("VNDegen input: " + p_event->as_text());
	}
	Input::get_singleton()->parse_input_event(p_event);
}

void mouse_button(MouseButton p_button, bool p_down, double p_factor = 1.0) {
	if (state.mouse.x < 0) {
		Size2i size = DisplayServer::get_singleton()->window_get_size();
		state.mouse = Vector2(size.width / 2.0, size.height / 2.0);
	}
	if (p_button <= MouseButton::MB_XBUTTON2 && p_button != MouseButton::NONE) {
		if (p_down) {
			state.buttons.set_flag(mouse_button_to_mask(p_button));
		} else {
			state.buttons.clear_flag(mouse_button_to_mask(p_button));
		}
	}
	Ref<InputEventMouseButton> event;
	event.instantiate();
	event->set_button_index(p_button);
	event->set_pressed(p_down);
	event->set_factor(p_factor);
	event->set_position(state.mouse);
	event->set_global_position(state.mouse);
	event->set_button_mask(state.buttons);
	parse(event);
}

void set_problem(const String &p_problem) {
	state.problem = p_problem.utf8().get_data();
}

} // namespace

void vngd_show_keyboard(bool p_show) {
	if (state.keyboard) {
		state.keyboard(p_show, state.context);
	}
}

extern "C" {

int vngd_start(const VNGDLaunch *p_launch) {
	ERR_FAIL_COND_V_MSG(state.os, 1, "A game is still running. Call vngd_stop first.");
	state = State();
	if (half_started) {
		set_problem(half_started_problem);
		return 2;
	}
	state.keyboard = p_launch->keyboard;
	state.context = p_launch->context;
	state.os = vngd_platform_create_os(p_launch);
	if (p_launch->fontTable) {
		vnfont_init(p_launch->fontTable, p_launch->fontFolders, p_launch->fontFolderCount);
	}

	std::vector<std::string> args = { "--main-pack", p_launch->pack };
	if (p_launch->renderingMethod) {
		args.insert(args.end(), { "--rendering-method", p_launch->renderingMethod });
	}
	if (p_launch->logPath) {
		args.insert(args.end(), { "--log-file", p_launch->logPath });
	}
	vngd_platform_add_args(args);
	std::vector<char *> argv;
	for (std::string &arg : args) {
		argv.push_back(arg.data());
	}
	argv.push_back(nullptr);

	vngd::steam_wanted = p_launch->steam;
	vngd::eos::wanted = p_launch->epicOnlineServices;
	// Godot decrypts a pack with the key its build was given, which this engine's build leaves empty.
	if (p_launch->encryptionKey) {
		memcpy(script_encryption_key, p_launch->encryptionKey, sizeof(script_encryption_key));
	}

	// The two phases run apart, since a failed first phase undoes itself and a failed second doesn't.
	if (Main::setup(p_launch->executable, (int)args.size(), argv.data(), false) != OK) {
		set_problem("The game's files could not be loaded.");
		return 1;
	}
	if (Main::setup2() != OK || Main::start() != EXIT_SUCCESS) {
		half_started = true;
		set_problem(half_started_problem);
		return 2;
	}
	state.setup = true;
	OS::get_singleton()->get_main_loop()->initialize();
	state.started = true;
	frames.start();
	// The game's own cap, which the display's refresh rate limits.
	const int max_fps = Engine::get_singleton()->get_max_fps();
	const float refresh = DisplayServer::get_singleton()->screen_get_refresh_rate();
	frames.set_target(max_fps > 0 ? MIN(max_fps, refresh) : refresh);

	// VNDegen's controller, with Godot's own button and axis numbers, so no mapping applies.
	Dictionary info;
	info["mapping_handled"] = true;
	Input::get_singleton()->joy_connection_changed(joypad, true, "Xbox Controller", "", info);
	return 0;
}

// For VNDegen's tests on macOS, where a window snapshot can't see Metal: with VNGD_SNAPSHOT set to a path, the frame
// the game drew is written there as a PNG every two seconds.
static void write_snapshot() {
	static const String path = OS::get_singleton()->get_environment("VNGD_SNAPSHOT");
	static uint64_t last = 0;
	const uint64_t now = OS::get_singleton()->get_ticks_msec();
	if (path.is_empty() || now - last < 2000) {
		return;
	}
	last = now;
	Ref<Image> image = SceneTree::get_singleton()->get_root()->get_texture()->get_image();
	if (image.is_valid()) {
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
	DisplayServer::get_singleton()->process_events();
	const bool quit = Main::iteration();
	frames.frame();
	write_snapshot();
	return quit ? 1 : 0;
}

void vngd_resize(float p_width, float p_height, float p_scale) {
	if (state.started) {
		vngd_platform_resize(p_width, p_height, p_scale);
	}
}

void vngd_key(int p_hid_usage, bool p_down, uint32_t p_character) {
	if (!state.started) {
		return;
	}
	Key keycode = vngd_hid_to_key(p_hid_usage);
	if (keycode == Key::NONE && !p_character) {
		return;
	}
	Ref<InputEventKey> event;
	event.instantiate();
	event->set_keycode(keycode);
	event->set_physical_keycode(keycode);
	event->set_key_label(keycode);
	event->set_pressed(p_down);
	if (p_down) {
		event->set_unicode(p_character);
	}
	// Modifier state, as Godot keeps it for a real keyboard.
	Input *input = Input::get_singleton();
	event->set_shift_pressed(keycode == Key::SHIFT ? p_down : input->is_key_pressed(Key::SHIFT));
	event->set_ctrl_pressed(keycode == Key::CTRL ? p_down : input->is_key_pressed(Key::CTRL));
	event->set_alt_pressed(keycode == Key::ALT ? p_down : input->is_key_pressed(Key::ALT));
	event->set_meta_pressed(keycode == Key::META ? p_down : input->is_key_pressed(Key::META));
	parse(event);
}

void vngd_text(const char *p_utf8) {
	if (!state.started) {
		return;
	}
	String text = String::utf8(p_utf8);
	for (int i = 0; i < text.length(); i++) {
		char32_t c = text[i];
		Ref<InputEventKey> event;
		event.instantiate();
		event->set_pressed(true);
		event->set_unicode(c);
		// Return and Backspace edit text through their keys, as on a keyboard.
		if (c == '\n' || c == '\r') {
			event->set_keycode(Key::ENTER);
			event->set_unicode(0);
		} else if (c == '\b') {
			event->set_keycode(Key::BACKSPACE);
			event->set_unicode(0);
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
	event.instantiate();
	event->set_position(position);
	event->set_global_position(position);
	event->set_relative(relative);
	event->set_relative_screen_position(relative);
	event->set_button_mask(state.buttons);
	Input::get_singleton()->set_mouse_position(position);
	parse(event);
}

void vngd_mouse_button(int p_button, bool p_down) {
	if (!state.started) {
		return;
	}
	MouseButton button = p_button == 2 ? MouseButton::RIGHT : p_button == 3 ? MouseButton::MIDDLE : MouseButton::LEFT;
	mouse_button(button, p_down);
}

void vngd_wheel(float p_notches) {
	if (!state.started || p_notches == 0) {
		return;
	}
	// Turned away from the player scrolls up, as a mouse wheel does.
	MouseButton button = p_notches > 0 ? MouseButton::WHEEL_UP : MouseButton::WHEEL_DOWN;
	double factor = Math::abs(p_notches);
	mouse_button(button, true, factor);
	mouse_button(button, false, factor);
}

void vngd_touch(int p_index, int p_phase, float p_x, float p_y) {
	if (!state.started) {
		return;
	}
	Vector2 position = window_point(p_x, p_y);
	if (p_phase == 1) {
		Ref<InputEventScreenDrag> event;
		event.instantiate();
		event->set_index(p_index);
		event->set_position(position);
		parse(event);
		return;
	}
	Ref<InputEventScreenTouch> event;
	event.instantiate();
	event->set_index(p_index);
	event->set_position(position);
	event->set_pressed(p_phase == 0);
	event->set_canceled(p_phase == 3);
	parse(event);
}

void vngd_joy_button(int p_button, bool p_down) {
	if (!state.started || p_button < VNGD_JOY_A || p_button > VNGD_JOY_DPAD_RIGHT) {
		return;
	}
	// vngd.h numbers buttons as Godot 4 does.
	Input::get_singleton()->joy_button(joypad, (JoyButton)p_button, p_down);
}

void vngd_joy_axis(int p_axis, float p_value) {
	if (!state.started || p_axis < VNGD_AXIS_LEFT_X || p_axis > VNGD_AXIS_TRIGGER_RIGHT) {
		return;
	}
	Input::get_singleton()->joy_axis(joypad, (JoyAxis)p_axis, p_value);
}

void vngd_set_paused(bool p_paused) {
	if (!state.started || state.paused == p_paused) {
		return;
	}
	state.paused = p_paused;
	if (!p_paused) {
		frames.skip();
	}
	// As when a PC game's window loses focus. Godot caps the first frame after the pause at its physics step limit.
	OS::get_singleton()->get_main_loop()->notification(p_paused ? MainLoop::NOTIFICATION_APPLICATION_FOCUS_OUT : MainLoop::NOTIFICATION_APPLICATION_FOCUS_IN);
	vngd_platform_set_paused(p_paused);
}

void vngd_request_quit(void) {
	SceneTree *tree = state.started ? SceneTree::get_singleton() : nullptr;
	if (!tree) {
		return;
	}
	// What the root window does for its close button: games see NOTIFICATION_WM_CLOSE_REQUEST, and the tree quits
	// unless the game turned auto_accept_quit off.
	Window *root = tree->get_root();
	root->propagate_notification(Node::NOTIFICATION_WM_CLOSE_REQUEST);
	root->emit_signal(SNAME("close_requested"));
}

void vngd_stop(void) {
	if (!state.os) {
		return;
	}
	if (state.started) {
		frames.stop();
		OS::get_singleton()->get_main_loop()->finalize();
	}
	if (half_started) {
		state = State();
		state.problem = half_started_problem;
		return;
	}
	if (state.setup) {
		Main::cleanup();
	} else {
		// A failed Main::setup frees what it made, but the registries Main::cleanup resets for the next start still
		// point at drivers the OS owns.
		MovieWriter::reset();
		AudioDriverManager::reset();
		DisplayServer::reset();
		SceneTree::reset_idle_callbacks();
	}
	vngd_platform_destroy_os(state.os);
	memset(script_encryption_key, 0, sizeof(script_encryption_key));
	std::string problem = state.problem;
	state = State();
	state.problem = problem;
}

const char *vngd_problem(void) {
	return state.problem.empty() ? nullptr : state.problem.c_str();
}

} // extern "C"
