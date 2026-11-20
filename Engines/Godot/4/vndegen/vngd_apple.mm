// macOS and iOS: Godot's own OS for each, inside VNDegen's process, drawing through DisplayServerVNDegen.
#import "display_server_vndegen.h"
#include "vngd_os.h"
#include "vngd_platform.h"

#import "drivers/coreaudio/audio_driver_coreaudio.h"

#import <CoreText/CoreText.h>

#include <vector>

static uint32_t vngd_be32(const uint8_t *p_bytes) {
	return uint32_t(p_bytes[0]) << 24 | uint32_t(p_bytes[1]) << 16 | uint32_t(p_bytes[2]) << 8 | p_bytes[3];
}

static bool vngd_font_has_outlines(NSString *p_path) {
	NSFileHandle *file = [NSFileHandle fileHandleForReadingAtPath:p_path];
	NSData *head = [file readDataOfLength:16];
	if (head.length < 16) {
		return false;
	}
	const uint8_t *bytes = (const uint8_t *)head.bytes;
	uint64_t face = 0;
	if (memcmp(bytes, "ttcf", 4) == 0) {
		face = vngd_be32(bytes + 12);
		[file seekToFileOffset:face];
		head = [file readDataOfLength:12];
		if (head.length < 12) {
			return false;
		}
		bytes = (const uint8_t *)head.bytes;
	}
	const uint32_t tables = uint32_t(bytes[4]) << 8 | bytes[5];
	[file seekToFileOffset:face + 12];
	NSData *directory = [file readDataOfLength:tables * 16];
	const uint8_t *entries = (const uint8_t *)directory.bytes;
	for (NSUInteger i = 0; i + 16 <= directory.length; i += 16) {
		for (const char *tag : { "glyf", "CFF ", "CFF2", "sbix", "CBDT", "EBDT" }) {
			if (memcmp(entries + i, tag, 4) == 0) {
				return true;
			}
		}
	}
	return false;
}

static Vector<String> vngd_readable_fonts_for_text(const Vector<String> &p_paths, const String &p_font_name, const String &p_text, const String &p_locale) {
	Vector<String> readable;
	for (const String &path : p_paths) {
		if (vngd_font_has_outlines([NSString stringWithUTF8String:path.utf8().get_data()])) {
			readable.push_back(path);
		}
	}
	if (!readable.is_empty() || p_paths.is_empty() || p_text.is_empty()) {
		return readable;
	}
	NSString *text = [NSString stringWithUTF8String:p_text.utf8().get_data()];
	CTFontRef base = CTFontCreateWithName((__bridge CFStringRef)[NSString stringWithUTF8String:p_font_name.utf8().get_data()], 16, nullptr);
	NSArray *languages = p_locale.is_empty() ? @[] : @[ [NSString stringWithUTF8String:p_locale.replace("_", "-").utf8().get_data()] ];
	NSArray *cascade = CFBridgingRelease(CTFontCopyDefaultCascadeListForLanguages(base, (__bridge CFArrayRef)languages));
	CFRelease(base);
	const NSUInteger length = text.length;
	const NSUInteger first = [text rangeOfComposedCharacterSequenceAtIndex:0].length;
	std::vector<unichar> characters(length);
	[text getCharacters:characters.data() range:NSMakeRange(0, length)];
	std::vector<CGGlyph> glyphs(length);
	String partial;
	for (id descriptor in cascade) {
		NSURL *url = CFBridgingRelease(CTFontDescriptorCopyAttribute((__bridge CTFontDescriptorRef)descriptor, kCTFontURLAttribute));
		if (!url || !vngd_font_has_outlines(url.path)) {
			continue;
		}
		CTFontRef font = CTFontCreateWithFontDescriptor((__bridge CTFontDescriptorRef)descriptor, 16, nullptr);
		const bool all = CTFontGetGlyphsForCharacters(font, characters.data(), glyphs.data(), length);
		const bool starts = all || CTFontGetGlyphsForCharacters(font, characters.data(), glyphs.data(), first);
		CFRelease(font);
		if (all) {
			readable.push_back(String::utf8(url.path.UTF8String));
			return readable;
		}
		if (starts && partial.is_empty()) {
			partial = String::utf8(url.path.UTF8String);
		}
	}
	if (!partial.is_empty()) {
		readable.push_back(partial);
	}
	return readable;
}

#if TARGET_OS_IPHONE
#import "platform/ios/os_ios.h"

// Godot's iOS export generates these for the game's iOS plugins. VNDegen runs Windows games, which have none.
void godot_apple_embedded_plugins_initialize() {}
void godot_apple_embedded_plugins_deinitialize() {}

// Godot's iOS OS, without the controllers it reads itself, which VNDegen reads instead.
class OS_VNDegenApple : public OS_VNDegen<OS_IOS> {
public:
	// OS_AppleEmbedded's own initialize_core, which is private, does only this.
	void initialize_core() override {
		OS_Unix::initialize_core();
		vngd_use_case_lookup<DirAccessUnix>();
	}
	void initialize_joypads() override {}
	Vector<String> get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale, const String &p_script, int p_weight, int p_stretch, bool p_italic) const override {
		return vngd_readable_fonts_for_text(OS_VNDegen<OS_IOS>::get_system_font_path_for_text(p_font_name, p_text, p_locale, p_script, p_weight, p_stretch, p_italic), p_font_name, p_text, p_locale);
	}
	OS_VNDegenApple(const VNGDLaunch *p_launch) :
			OS_VNDegen<OS_IOS>(p_launch) {}
};
#else
#import "platform/macos/dir_access_macos.h"
#import "platform/macos/os_macos.h"

// Godot's macOS OS, without its crash handler, which would take over VNDegen's, and without the controllers it reads
// itself. VNDegen runs the main loop, so run() is never called.
class OS_VNDegenApple : public OS_VNDegen<OS_MacOS> {
public:
	void initialize() override { initialize_core(); }
	void initialize_core() override {
		OS_MacOS::initialize_core();
		vngd_use_case_lookup<DirAccessMacOS>();
	}
	void initialize_joypads() override {}
	Vector<String> get_system_font_path_for_text(const String &p_font_name, const String &p_text, const String &p_locale, const String &p_script, int p_weight, int p_stretch, bool p_italic) const override {
		return vngd_readable_fonts_for_text(OS_VNDegen<OS_MacOS>::get_system_font_path_for_text(p_font_name, p_text, p_locale, p_script, p_weight, p_stretch, p_italic), p_font_name, p_text, p_locale);
	}
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
#if defined(VNDEGEN_ANGLE)
	const char *driver = "opengl3";
#else
	const char *driver = "metal";
#endif
	r_args.insert(r_args.end(), { "--display-driver", "vndegen", "--rendering-driver", driver, "--audio-driver", "CoreAudio" });
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
