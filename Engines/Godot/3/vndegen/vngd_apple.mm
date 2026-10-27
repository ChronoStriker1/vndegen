// macOS and iOS: an OS of VNDegen's own, built on Godot's Unix one, that draws in the layer VNDegen hands it. iOS draws
// with OpenGL ES through MetalANGLE, in an MGLLayer inside VNDegen's layer. macOS draws with Apple's OpenGL into
// IOSurfaces that a layer inside VNDegen's shows.

#include "vngd_platform.h"

#include "vngd_keys.h"

#include "core/io/logger.h"
#include "core/os/input_event.h"
#include "core/project_settings.h"
#include "drivers/coreaudio/audio_driver_coreaudio.h"
#include "drivers/gles2/rasterizer_gles2.h"
#include "drivers/gles3/rasterizer_gles3.h"
#include "drivers/unix/os_unix.h"
#include "main/input_default.h"
#include "main/main.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "servers/visual/visual_server_raster.h"
#include "servers/visual/visual_server_wrap_mt.h"

#include <vngd.h>

#import <QuartzCore/QuartzCore.h>
#if TARGET_OS_IPHONE
#import <MetalANGLE/MGLContext.h>
#import <UIKit/UIKit.h>
#else
#import <AppKit/AppKit.h>
#import <IOSurface/IOSurface.h>
#import <OpenGL/CGLCurrent.h>
#import <OpenGL/CGLIOSurface.h>
#import <OpenGL/OpenGL.h>
#endif

#include <string>
#include <vector>

namespace {

// Where the game draws, and how its frames reach VNDegen's layer.
class Surface {
#if TARGET_OS_IPHONE
	MGLLayer *gl_layer = nil;
	MGLContext *context = nil;
#else
	CALayer *content = nil;
	CGLContextObj context = nullptr;
	IOSurfaceRef surfaces[2] = {};
	GLuint textures[2] = {};
	GLuint framebuffers[2] = {};
	int back = 0;
	Size2i pixels;

	void release_buffers() {
		if (framebuffers[0]) {
			glDeleteFramebuffers(2, framebuffers);
			glDeleteTextures(2, textures);
		}
		for (IOSurfaceRef &surface : surfaces) {
			if (surface) {
				CFRelease(surface);
				surface = nullptr;
			}
		}
		framebuffers[0] = framebuffers[1] = 0;
	}

	// Two IOSurfaces the size of the view, each a framebuffer the game draws in. The layer shows the last one drawn.
	void make_buffers(Size2i p_pixels) {
		release_buffers();
		pixels = p_pixels;
		for (int i = 0; i < 2; i++) {
			NSDictionary *properties = @{
				(id)kIOSurfaceWidth : @(MAX(pixels.width, 1)),
				(id)kIOSurfaceHeight : @(MAX(pixels.height, 1)),
				(id)kIOSurfaceBytesPerElement : @4,
				(id)kIOSurfacePixelFormat : @((uint32_t)'BGRA'),
			};
			surfaces[i] = IOSurfaceCreate((__bridge CFDictionaryRef)properties);
		}
		glGenTextures(2, textures);
		glGenFramebuffers(2, framebuffers);
		for (int i = 0; i < 2; i++) {
			glBindTexture(GL_TEXTURE_RECTANGLE, textures[i]);
			CGLTexImageIOSurface2D(context, GL_TEXTURE_RECTANGLE, GL_RGBA, MAX(pixels.width, 1), MAX(pixels.height, 1), GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surfaces[i], 0);
			glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[i]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, textures[i], 0);
		}
		glBindTexture(GL_TEXTURE_RECTANGLE, 0);
	}
#endif

public:
	Error create(CALayer *p_host, Size2 p_points, float p_scale, bool p_gles3) {
#if TARGET_OS_IPHONE
		gl_layer = [MGLLayer new];
		gl_layer.drawableColorFormat = MGLDrawableColorFormatRGBA8888;
		gl_layer.drawableDepthFormat = MGLDrawableDepthFormat24;
		[p_host addSublayer:gl_layer];
		resize(p_points, p_scale);
		context = [[MGLContext alloc] initWithAPI:p_gles3 ? kMGLRenderingAPIOpenGLES3 : kMGLRenderingAPIOpenGLES2];
		ERR_FAIL_NULL_V_MSG(context, ERR_UNAVAILABLE, "OpenGL ES is not available through MetalANGLE.");
		make_current();
		return OK;
#else
		CGLPixelFormatAttribute attributes[] = {
			kCGLPFAOpenGLProfile, (CGLPixelFormatAttribute)(p_gles3 ? kCGLOGLPVersion_3_2_Core : kCGLOGLPVersion_Legacy),
			kCGLPFAColorSize, (CGLPixelFormatAttribute)24, kCGLPFAAlphaSize, (CGLPixelFormatAttribute)8,
			kCGLPFADepthSize, (CGLPixelFormatAttribute)24, kCGLPFAAccelerated, kCGLPFAAllowOfflineRenderers,
			(CGLPixelFormatAttribute)0
		};
		CGLPixelFormatObj format = nullptr;
		GLint count = 0;
		CGLChoosePixelFormat(attributes, &format, &count);
		ERR_FAIL_NULL_V_MSG(format, ERR_UNAVAILABLE, "OpenGL is not available.");
		CGLCreateContext(format, nullptr, &context);
		CGLReleasePixelFormat(format);
		ERR_FAIL_NULL_V_MSG(context, ERR_UNAVAILABLE, "OpenGL is not available.");
		CGLSetCurrentContext(context);
		content = [CALayer layer];
		content.anchorPoint = CGPointZero;
		// OpenGL's rows run bottom up.
		content.geometryFlipped = YES;
		content.contentsGravity = kCAGravityResize;
		[p_host addSublayer:content];
		resize(p_points, p_scale);
		return OK;
#endif
	}

	// Sets the frame layer's size. On macOS the IOSurfaces follow once OpenGL is loaded, at the next make_current.
	void resize(Size2 p_points, float p_scale) {
		[CATransaction begin];
		[CATransaction setDisableActions:YES];
#if TARGET_OS_IPHONE
		gl_layer.frame = CGRectMake(0, 0, p_points.width, p_points.height);
		gl_layer.contentsScale = p_scale;
#else
		content.frame = CGRectMake(0, 0, p_points.width, p_points.height);
		content.contentsScale = p_scale;
#endif
		[CATransaction commit];
	}

	// Makes the game's context current, and returns the framebuffer it draws its frame in.
	GLuint make_current(Size2i p_pixels = Size2i()) {
#if TARGET_OS_IPHONE
		[MGLContext setCurrentContext:context forLayer:gl_layer];
		return gl_layer.defaultOpenGLFrameBufferID;
#else
		CGLSetCurrentContext(context);
		if (p_pixels != Size2i() && p_pixels != pixels) {
			make_buffers(p_pixels);
		}
		return framebuffers[back];
#endif
	}

	void present() {
#if TARGET_OS_IPHONE
		[context present:gl_layer];
#else
		if (!surfaces[back]) {
			return;
		}
		glFlush();
		[CATransaction begin];
		[CATransaction setDisableActions:YES];
		content.contents = (__bridge id)surfaces[back];
		[CATransaction commit];
		back = 1 - back;
#endif
	}

	void destroy() {
#if TARGET_OS_IPHONE
		if ([MGLContext currentContext] == context) {
			[MGLContext setCurrentContext:nil];
		}
		context = nil;
		[gl_layer removeFromSuperlayer];
		gl_layer = nil;
#else
		if (context) {
			CGLSetCurrentContext(context);
			release_buffers();
			CGLSetCurrentContext(nullptr);
			CGLDestroyContext(context);
			context = nullptr;
		}
		[content removeFromSuperlayer];
		content = nil;
#endif
	}
};

class OS_VNDegen3 : public OS_Unix {
	String executable;
	String data_path;
	String log_path;
	CALayer *host = nil;
	Size2 points;
	float scale = 1;
	void (*keyboard)(bool, void *) = nullptr;
	void *keyboard_context = nullptr;

	Surface surface;
	bool gles3 = true;
	int video_driver = VIDEO_DRIVER_GLES3;
	VisualServer *visual_server = nullptr;
	AudioDriverCoreAudio audio_driver;
	InputDefault *input = nullptr;
	MainLoop *main_loop = nullptr;
	VideoMode video_mode;

	void set_system_framebuffer(GLuint p_fbo) {
		if (gles3) {
			RasterizerStorageGLES3::system_fbo = p_fbo;
		} else {
			RasterizerStorageGLES2::system_fbo = p_fbo;
		}
	}

public:
	OS_VNDegen3(const VNGDLaunch *p_launch) {
		executable = String::utf8(p_launch->executable);
		data_path = String::utf8(p_launch->dataDir);
		log_path = p_launch->logPath ? String::utf8(p_launch->logPath) : String();
		host = (__bridge CALayer *)p_launch->layer;
		points = Size2(p_launch->width, p_launch->height);
		scale = p_launch->scale > 0 ? p_launch->scale : 1;
		keyboard = p_launch->keyboard;
		keyboard_context = p_launch->context;
		Vector<Logger *> loggers;
		loggers.push_back(memnew(StdLogger));
		_set_logger(memnew(CompositeLogger(loggers)));
		AudioDriverManager::add_driver(&audio_driver);
	}

	InputDefault *get_input() const { return input; }

	Size2i pixel_size() const { return Size2i(Math::round(points.width * scale), Math::round(points.height * scale)); }

	// Before each frame: the game's context current, drawing into this frame's framebuffer.
	void begin_frame() { set_system_framebuffer(surface.make_current(pixel_size())); }

	void resize(Size2 p_points, float p_scale) {
		points = p_points;
		scale = p_scale;
		Size2i pixels = pixel_size();
		video_mode.width = pixels.width;
		video_mode.height = pixels.height;
		surface.resize(points, scale);
	}

	void set_audio_paused(bool p_paused) {
		if (p_paused) {
			audio_driver.stop();
		} else {
			audio_driver.start();
		}
	}

	// MARK: OS

	String get_name() const override {
#if TARGET_OS_IPHONE
		return "iOS";
#else
		return "OSX";
#endif
	}
	String get_executable_path() const override { return executable; }
	String get_data_path() const override { return data_path; }
	// Godot on Windows names its folder this way, where other platforms use "godot".
	String get_godot_dir_name() const override { return "Godot"; }

	// Godot's PC exports carry S3TC textures, which Metal reads on Apple silicon, and so does ANGLE on it.
	bool _check_internal_feature_support(const String &p_feature) override {
		return p_feature == "pc" || p_feature == "s3tc";
	}

	int get_video_driver_count() const override { return 2; }
	const char *get_video_driver_name(int p_driver) const override { return p_driver == VIDEO_DRIVER_GLES2 ? "GLES2" : "GLES3"; }
	int get_current_video_driver() const override { return video_driver; }

	// The log file needs Godot's file access, which starts here.
	void initialize_core() override {
		OS_Unix::initialize_core();
		if (!log_path.empty()) {
			Vector<Logger *> loggers;
			loggers.push_back(memnew(StdLogger));
			loggers.push_back(memnew(RotatedFileLogger(log_path, 1)));
			_set_logger(memnew(CompositeLogger(loggers)));
		}
	}

	Error initialize(const VideoMode &p_desired, int p_video_driver, int p_audio_driver) override {
		Size2i pixels = pixel_size();
		video_mode = VideoMode(pixels.width, pixels.height, true, false, false);
		gles3 = p_video_driver != VIDEO_DRIVER_GLES2;
		bool fallback = GLOBAL_GET("rendering/quality/driver/fallback_to_gles2");
		Error err = surface.create(host, points, scale, gles3);
		if (err != OK && gles3 && fallback) {
			gles3 = false;
			err = surface.create(host, points, scale, false);
		}
		ERR_FAIL_COND_V(err != OK, err);
		// The context only: OpenGL's functions load in is_viable, and the macOS framebuffers need them.
		surface.make_current();
		if (gles3 && RasterizerGLES3::is_viable() == OK) {
			RasterizerGLES3::register_config();
			RasterizerGLES3::make_current();
		} else if (RasterizerGLES2::is_viable() == OK) {
			gles3 = false;
			RasterizerGLES2::register_config();
			RasterizerGLES2::make_current();
		} else {
			ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "No OpenGL renderer Godot 3 can use.");
		}
		video_driver = gles3 ? VIDEO_DRIVER_GLES3 : VIDEO_DRIVER_GLES2;
		// make_buffers runs once OpenGL is loaded.
		set_system_framebuffer(surface.make_current(pixels));

		visual_server = memnew(VisualServerRaster);
		visual_server->init();
		// The rasterizer resets this while it starts.
		set_system_framebuffer(surface.make_current(pixels));

		AudioDriverManager::initialize(p_audio_driver);
		input = memnew(InputDefault);
		return OK;
	}

	void set_main_loop(MainLoop *p_main_loop) override {
		main_loop = p_main_loop;
		input->set_main_loop(p_main_loop);
		main_loop->init();
	}

	MainLoop *get_main_loop() const override { return main_loop; }

	void delete_main_loop() override {
		if (main_loop) {
			main_loop->finish();
			memdelete(main_loop);
			main_loop = nullptr;
		}
	}

	void finalize() override {
		delete_main_loop();
		if (input) {
			memdelete(input);
			input = nullptr;
		}
		if (visual_server) {
			visual_server->finish();
			memdelete(visual_server);
			visual_server = nullptr;
		}
		surface.destroy();
	}

	void swap_buffers() override {
		surface.present();
	}

	bool can_draw() const override { return true; }

	Error get_entropy(uint8_t *r_buffer, int p_bytes) override {
		arc4random_buf(r_buffer, p_bytes);
		return OK;
	}

	void alert(const String &p_alert, const String &p_title = "ALERT!") override {
		ERR_PRINT(p_title + ": " + p_alert);
	}

	Point2 get_mouse_position() const override { return input ? input->get_mouse_position() : Point2(); }
	int get_mouse_button_state() const override { return input ? input->get_mouse_button_mask() : 0; }
	void set_window_title(const String &p_title) override {}

	// The game fills VNDegen's view, so its own window size requests change nothing.
	void set_video_mode(const VideoMode &p_video_mode, int p_screen = 0) override {}
	VideoMode get_video_mode(int p_screen = 0) const override { return video_mode; }
	void get_fullscreen_mode_list(List<VideoMode> *p_list, int p_screen = 0) const override { p_list->push_back(video_mode); }
	Size2 get_window_size() const override { return Size2(video_mode.width, video_mode.height); }
	Size2 get_screen_size(int p_screen = -1) const override { return get_window_size(); }
	int get_screen_dpi(int p_screen = -1) const override {
#if TARGET_OS_IPHONE
		return 160 * scale;
#else
		return 72 * scale;
#endif
	}
	float get_screen_scale(int p_screen = -1) const override { return scale; }
	float get_screen_refresh_rate(int p_screen = -1) const override {
#if TARGET_OS_IPHONE
		return UIScreen.mainScreen.maximumFramesPerSecond;
#else
		NSScreen *screen = NSScreen.mainScreen;
		return screen ? screen.maximumFramesPerSecond : SCREEN_REFRESH_RATE_FALLBACK;
#endif
	}

	void set_clipboard(const String &p_text) override {
		NSString *text = [NSString stringWithUTF8String:p_text.utf8().get_data()];
#if TARGET_OS_IPHONE
		UIPasteboard.generalPasteboard.string = text;
#else
		[NSPasteboard.generalPasteboard clearContents];
		[NSPasteboard.generalPasteboard setString:text forType:NSPasteboardTypeString];
#endif
	}

	String get_clipboard() const override {
#if TARGET_OS_IPHONE
		NSString *text = UIPasteboard.generalPasteboard.string;
#else
		NSString *text = [NSPasteboard.generalPasteboard stringForType:NSPasteboardTypeString];
#endif
		return text ? String::utf8(text.UTF8String) : String();
	}

	bool has_virtual_keyboard() const override { return keyboard != nullptr; }
	void show_virtual_keyboard(const String &p_existing_text, const Rect2 &p_screen_rect = Rect2(), VirtualKeyboardType p_type = KEYBOARD_TYPE_DEFAULT, int p_max_input_length = -1, int p_cursor_start = -1, int p_cursor_end = -1) override {
		if (keyboard) {
			keyboard(true, keyboard_context);
		}
	}
	void hide_virtual_keyboard() override {
		if (keyboard) {
			keyboard(false, keyboard_context);
		}
	}

	bool has_touchscreen_ui_hint() const override {
#if TARGET_OS_IPHONE
		return true;
#else
		return false;
#endif
	}
};

} // namespace

OS *vngd_platform_create_os(const VNGDLaunch *p_launch) {
	return new OS_VNDegen3(p_launch);
}

const char *vngd_platform_audio_driver() {
	return "CoreAudio";
}

void vngd_platform_started() {}
void vngd_platform_stopping() {}

void vngd_platform_begin_frame() {
	static_cast<OS_VNDegen3 *>(OS::get_singleton())->begin_frame();
}

void vngd_platform_resize(float p_width, float p_height, float p_scale) {
	static_cast<OS_VNDegen3 *>(OS::get_singleton())->resize(Size2(p_width, p_height), p_scale);
}

void vngd_platform_set_audio_paused(bool p_paused) {
	static_cast<OS_VNDegen3 *>(OS::get_singleton())->set_audio_paused(p_paused);
}
