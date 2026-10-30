#import "display_server_vndegen.h"

#include "vngd_platform.h"

#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/os/os.h"

#if defined(RD_ENABLED)
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#endif
#if defined(METAL_ENABLED)
#import "drivers/metal/rendering_context_driver_metal.h"
#endif

#if defined(VNDEGEN_ANGLE)
#include "drivers/gles3/rasterizer_gles3.h"
#include "drivers/gles3/storage/texture_storage.h"
#import <MetalANGLE/MGLContext.h>
#endif

#import <QuartzCore/CAMetalLayer.h>
#if TARGET_OS_IPHONE
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#endif

CAMetalLayer *DisplayServerVNDegen::host_layer = nil;
Size2 DisplayServerVNDegen::host_size;
float DisplayServerVNDegen::host_scale = 1;

DisplayServerVNDegen *DisplayServerVNDegen::get_singleton() {
	return static_cast<DisplayServerVNDegen *>(DisplayServer::get_singleton());
}

void DisplayServerVNDegen::register_vndegen_driver() {
	register_create_function("vndegen", create_func, get_rendering_drivers_func);
}

Vector<String> DisplayServerVNDegen::get_rendering_drivers_func() {
#if defined(VNDEGEN_ANGLE)
	return { "opengl3" };
#else
	return { "metal" };
#endif
}

DisplayServer *DisplayServerVNDegen::create_func(const String &p_rendering_driver, DisplayServerEnums::WindowMode p_mode, DisplayServerEnums::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, DisplayServerEnums::Context p_context, int64_t p_parent_window, Error &r_error) {
	return memnew(DisplayServerVNDegen(p_rendering_driver, p_vsync_mode, r_error));
}

DisplayServerVNDegen::DisplayServerVNDegen(const String &p_rendering_driver, DisplayServerEnums::VSyncMode p_vsync_mode, Error &r_error) {
	r_error = ERR_UNAVAILABLE;
	layer = host_layer;
	size_points = host_size;
	scale = host_scale;
	vsync_mode = p_vsync_mode;
	ERR_FAIL_NULL_MSG(layer, "VNDegen gave the engine no layer to draw in.");
#if defined(VNDEGEN_ANGLE)
	// The iOS Simulator: OpenGL ES through MetalANGLE, in a layer of its own over VNDegen's.
	ERR_FAIL_COND_MSG(p_rendering_driver != "opengl3", vformat("The %s rendering driver is not available in the iOS Simulator.", p_rendering_driver));
	gl_layer = [MGLLayer new];
	gl_layer.drawableColorFormat = MGLDrawableColorFormatRGBA8888;
	gl_layer.frame = CGRectMake(0, 0, size_points.width, size_points.height);
	gl_layer.contentsScale = scale;
	[layer addSublayer:gl_layer];
	gl_context = [[MGLContext alloc] initWithAPI:kMGLRenderingAPIOpenGLES3];
	ERR_FAIL_NULL_MSG(gl_context, "OpenGL ES 3 is not available through MetalANGLE.");
	gl_window_make_current(DisplayServerEnums::MAIN_WINDOW_ID);
	RasterizerGLES3::make_current(false);
#elif defined(RD_ENABLED) && defined(METAL_ENABLED)
	ERR_FAIL_COND_MSG(p_rendering_driver != "metal", vformat("The %s rendering driver is not available in VNDegen.", p_rendering_driver));
	layer.contentsScale = scale;
	layer.drawableSize = CGSizeMake(size_points.width * scale, size_points.height * scale);

	RenderingContextDriverMetal::WindowPlatformData wpd;
	wpd.layer = (__bridge CA::MetalLayer *)layer;
	rendering_context = memnew(RenderingContextDriverMetal);
	if (rendering_context->initialize() != OK) {
		memdelete(rendering_context);
		rendering_context = nullptr;
		ERR_FAIL_MSG("Metal is not available.");
	}
	if (rendering_context->window_create(DisplayServerEnums::MAIN_WINDOW_ID, &wpd) != OK) {
		memdelete(rendering_context);
		rendering_context = nullptr;
		ERR_FAIL_MSG("Could not draw in VNDegen's layer.");
	}
	Size2i pixels = window_get_size();
	rendering_context->window_set_size(DisplayServerEnums::MAIN_WINDOW_ID, pixels.width, pixels.height);
	rendering_context->window_set_vsync_mode(DisplayServerEnums::MAIN_WINDOW_ID, p_vsync_mode);

	rendering_device = memnew(RenderingDevice);
	if (rendering_device->initialize(rendering_context, DisplayServerEnums::MAIN_WINDOW_ID) != OK) {
		memdelete(rendering_device);
		rendering_device = nullptr;
		rendering_context->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
		memdelete(rendering_context);
		rendering_context = nullptr;
		ERR_FAIL_MSG("Could not start Godot's renderer on Metal.");
	}
	rendering_device->screen_create(DisplayServerEnums::MAIN_WINDOW_ID);
	RendererCompositorRD::make_current();
#endif

	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);
	r_error = OK;
}

DisplayServerVNDegen::~DisplayServerVNDegen() {
#if defined(RD_ENABLED)
	if (rendering_device) {
		rendering_device->screen_free(DisplayServerEnums::MAIN_WINDOW_ID);
		memdelete(rendering_device);
		rendering_device = nullptr;
	}
	if (rendering_context) {
		rendering_context->window_destroy(DisplayServerEnums::MAIN_WINDOW_ID);
		memdelete(rendering_context);
		rendering_context = nullptr;
	}
#endif
#if defined(VNDEGEN_ANGLE)
	if ([MGLContext currentContext] == gl_context) {
		[MGLContext setCurrentContext:nil];
	}
	gl_context = nil;
	[gl_layer removeFromSuperlayer];
	gl_layer = nil;
#endif
	layer = nil;
}

#if defined(VNDEGEN_ANGLE)
void DisplayServerVNDegen::gl_window_make_current(DisplayServerEnums::WindowID p_window_id) {
	[MGLContext setCurrentContext:gl_context forLayer:gl_layer];
	GLES3::TextureStorage::system_fbo = gl_layer.defaultOpenGLFrameBufferID;
}

void DisplayServerVNDegen::swap_buffers() {
	[gl_context present:gl_layer];
}
#endif

void DisplayServerVNDegen::resize(Size2 p_points, float p_scale) {
	if (p_points == size_points && p_scale == scale) {
		return;
	}
	size_points = p_points;
	scale = p_scale;
	Size2i pixels = window_get_size();
#if defined(VNDEGEN_ANGLE)
	[CATransaction begin];
	[CATransaction setDisableActions:YES];
	gl_layer.frame = CGRectMake(0, 0, size_points.width, size_points.height);
	gl_layer.contentsScale = scale;
	[CATransaction commit];
#else
	layer.contentsScale = scale;
	layer.drawableSize = CGSizeMake(pixels.width, pixels.height);
#endif
#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_context->window_set_size(DisplayServerEnums::MAIN_WINDOW_ID, pixels.width, pixels.height);
	}
#endif
	_window_callback(window_resize_callback, Rect2i(Point2i(), pixels));
}

void DisplayServerVNDegen::set_focused(bool p_focused) {
	if (focused == p_focused) {
		return;
	}
	focused = p_focused;
	_window_callback(window_event_callback, int(p_focused ? DisplayServerEnums::WINDOW_EVENT_FOCUS_IN : DisplayServerEnums::WINDOW_EVENT_FOCUS_OUT));
}

bool DisplayServerVNDegen::has_feature(DisplayServerEnums::Feature p_feature) const {
	switch (p_feature) {
		case DisplayServerEnums::FEATURE_CLIPBOARD:
		case DisplayServerEnums::FEATURE_HIDPI:
		case DisplayServerEnums::FEATURE_MOUSE:
		case DisplayServerEnums::FEATURE_VIRTUAL_KEYBOARD:
#if TARGET_OS_IPHONE
		case DisplayServerEnums::FEATURE_TOUCHSCREEN:
#endif
			return true;
		default:
			return false;
	}
}

void DisplayServerVNDegen::process_events() {
	// The window has focus from the start, as a desktop game's window does once it opens.
	if (!sent_focus) {
		sent_focus = true;
		_window_callback(window_event_callback, int(focused ? DisplayServerEnums::WINDOW_EVENT_FOCUS_IN : DisplayServerEnums::WINDOW_EVENT_FOCUS_OUT));
	}
	Input::get_singleton()->flush_buffered_events();
}

void DisplayServerVNDegen::_dispatch_input_events(const Ref<InputEvent> &p_event) {
	DisplayServerVNDegen *server = get_singleton();
	static const bool trace = OS::get_singleton()->get_environment("VNGD_TRACE_INPUT") == "1";
	if (trace) {
		print_line(vformat("VNDegen dispatch: %s, callback %s", p_event->as_text(), server->input_event_callback.is_valid() ? "set" : "missing"));
	}
	server->_window_callback(server->input_event_callback, p_event);
}

void DisplayServerVNDegen::_window_callback(const Callable &p_callable, const Variant &p_arg) const {
	if (p_callable.is_valid()) {
		p_callable.call(p_arg);
	}
}

Point2i DisplayServerVNDegen::mouse_get_position() const {
	return Input::get_singleton()->get_mouse_position();
}

BitField<MouseButtonMask> DisplayServerVNDegen::mouse_get_button_state() const {
	return Input::get_singleton()->get_mouse_button_mask();
}

void DisplayServerVNDegen::clipboard_set(const String &p_text) {
	NSString *text = [NSString stringWithUTF8String:p_text.utf8().get_data()];
#if TARGET_OS_IPHONE
	UIPasteboard.generalPasteboard.string = text;
#else
	[NSPasteboard.generalPasteboard clearContents];
	[NSPasteboard.generalPasteboard setString:text forType:NSPasteboardTypeString];
#endif
}

String DisplayServerVNDegen::clipboard_get() const {
#if TARGET_OS_IPHONE
	NSString *text = UIPasteboard.generalPasteboard.string;
#else
	NSString *text = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
#endif
	return text ? String::utf8(text.UTF8String) : String();
}

Size2i DisplayServerVNDegen::screen_get_size(int p_screen) const {
	return window_get_size();
}

Rect2i DisplayServerVNDegen::screen_get_usable_rect(int p_screen) const {
	return Rect2i(Point2i(), window_get_size());
}

int DisplayServerVNDegen::screen_get_dpi(int p_screen) const {
	// A point is 1/160 inch on iPhone and iPad and 1/72 inch on macOS, as Godot's own Apple platforms assume.
#if TARGET_OS_IPHONE
	return 160 * scale;
#else
	return 72 * scale;
#endif
}

float DisplayServerVNDegen::screen_get_refresh_rate(int p_screen) const {
#if TARGET_OS_IPHONE
	return UIScreen.mainScreen.maximumFramesPerSecond;
#else
	NSScreen *screen = NSScreen.mainScreen;
	return screen ? screen.maximumFramesPerSecond : SCREEN_REFRESH_RATE_FALLBACK;
#endif
}

void DisplayServerVNDegen::virtual_keyboard_show(const String &p_existing_text, const Rect2 &p_screen_rect, DisplayServerEnums::VirtualKeyboardType p_type, int p_max_length, int p_cursor_start, int p_cursor_end) {
	vngd_show_keyboard(true);
}

void DisplayServerVNDegen::virtual_keyboard_hide() {
	vngd_show_keyboard(false);
}

Vector<DisplayServerEnums::WindowID> DisplayServerVNDegen::get_window_list() const {
	return { DisplayServerEnums::MAIN_WINDOW_ID };
}

Size2i DisplayServerVNDegen::window_get_size(DisplayServerEnums::WindowID p_window) const {
	return Size2i(Math::round(size_points.width * scale), Math::round(size_points.height * scale));
}

void DisplayServerVNDegen::window_set_vsync_mode(DisplayServerEnums::VSyncMode p_vsync_mode, DisplayServerEnums::WindowID p_window) {
	vsync_mode = p_vsync_mode;
#if defined(RD_ENABLED)
	if (rendering_context) {
		rendering_context->window_set_vsync_mode(DisplayServerEnums::MAIN_WINDOW_ID, p_vsync_mode);
	}
#endif
}
