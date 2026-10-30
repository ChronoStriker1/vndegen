// What each platform adds to vngd.cpp: its OS, and the parts of a frame that depend on how the game draws.
#pragma once

#include "core/os/os.h"

struct VNGDLaunch;

OS *vngd_platform_create_os(const VNGDLaunch *p_launch);
const char *vngd_platform_audio_driver();
// After Main::start, and before Main::cleanup.
void vngd_platform_started();
void vngd_platform_stopping();
// Before each frame.
void vngd_platform_begin_frame();
void vngd_platform_resize(float p_width, float p_height, float p_scale);
void vngd_platform_set_audio_paused(bool p_paused);
