// What each platform adds to vngd.cpp: its OS, the arguments that pick its display and audio drivers, and the
// key table every platform shares.
#pragma once

#include "core/os/keyboard.h"
#include "core/os/os.h"

#include <string>
#include <vector>

struct VNGDLaunch;

OS *vngd_platform_create_os(const VNGDLaunch *p_launch);
void vngd_platform_destroy_os(OS *p_os);
void vngd_platform_add_args(std::vector<std::string> &r_args);
void vngd_platform_resize(float p_width, float p_height, float p_scale);
void vngd_platform_set_paused(bool p_paused);

// The Godot key for a USB HID keyboard usage, or Key::NONE.
Key vngd_hid_to_key(int p_hid_usage);
// Asks VNDegen to show or hide its on-screen keyboard.
void vngd_show_keyboard(bool p_show);
