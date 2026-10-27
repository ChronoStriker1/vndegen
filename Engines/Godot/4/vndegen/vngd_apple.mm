// macOS and iOS: Godot's own OS for each, inside VNDegen's process, drawing through DisplayServerVNDegen.
#import "display_server_vndegen.h"
#include "vngd_os.h"
#include "vngd_platform.h"

#import "drivers/coreaudio/audio_driver_coreaudio.h"

#if TARGET_OS_IPHONE
#import "platform/ios/os_ios.h"

// Godot's iOS export generates these for the game's iOS plugins. VNDegen runs Windows games, which have none.
void godot_apple_embedded_plugins_initialize() {}
void godot_apple_embedded_plugins_deinitialize() {}

// Godot's iOS OS, without the controllers it reads itself, which VNDegen reads instead.
class OS_VNDegenApple : public OS_VNDegen<OS_IOS> {
public:
	void initialize_joypads() override {}
	OS_VNDegenApple(const VNGDLaunch *p_launch) :
			OS_VNDegen<OS_IOS>(p_launch) {}
};
#else
#import "platform/macos/os_macos.h"

// Godot's macOS OS, without its crash handler, which would take over VNDegen's, and without the controllers it reads
// itself. VNDegen runs the main loop, so run() is never called.
class OS_VNDegenApple : public OS_VNDegen<OS_MacOS> {
public:
	void initialize() override { initialize_core(); }
	void initialize_joypads() override {}
	void run() override {}
	OS_VNDegenApple(const VNGDLaunch *p_launch) :
			OS_VNDegen<OS_MacOS>(p_launch, p_launch->executable, 0, nullptr) {}
};
#endif

OS *vngd_platform_create_os(const VNGDLaunch *p_launch) {
	DisplayServerVNDegen::host_layer = (__bridge CAMetalLayer *)p_launch->layer;
	DisplayServerVNDegen::host_size = Size2(p_launch->width, p_launch->height);
	DisplayServerVNDegen::host_scale = p_launch->scale > 0 ? p_launch->scale : 1;
	OS *os = new OS_VNDegenApple(p_launch);
	DisplayServerVNDegen::register_vndegen_driver();
	return os;
}

void vngd_platform_destroy_os(OS *p_os) {
	delete p_os;
	DisplayServerVNDegen::host_layer = nil;
}

void vngd_platform_add_args(std::vector<std::string> &r_args) {
	r_args.insert(r_args.end(), { "--display-driver", "vndegen", "--rendering-driver", "metal", "--audio-driver", "CoreAudio" });
}

void vngd_platform_resize(float p_width, float p_height, float p_scale) {
	DisplayServerVNDegen::get_singleton()->resize(Size2(p_width, p_height), p_scale);
}

void vngd_platform_set_paused(bool p_paused) {
	DisplayServerVNDegen::get_singleton()->set_focused(!p_paused);
	// The audio thread mixes on its own, so pausing the frames alone would leave the music playing.
	if (AudioDriverCoreAudio *audio = dynamic_cast<AudioDriverCoreAudio *>(AudioDriver::get_singleton())) {
		if (p_paused) {
			audio->stop();
		} else {
			audio->start();
		}
	}
}
