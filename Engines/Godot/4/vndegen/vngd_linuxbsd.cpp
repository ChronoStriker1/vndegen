// Linux, for testing the C interface on a Linux build. Games open their own X11 or Wayland window there.
#include "vngd_os.h"
#include "vngd_platform.h"

#include "platform/linuxbsd/os_linuxbsd.h"

OS *vngd_platform_create_os(const VNGDLaunch *p_launch) {
	return new OS_VNDegen<OS_LinuxBSD>(p_launch);
}

void vngd_platform_destroy_os(OS *p_os) {
	delete p_os;
}

void vngd_platform_add_args(std::vector<std::string> &r_args) {}
void vngd_platform_resize(float p_width, float p_height, float p_scale) {}
void vngd_platform_set_paused(bool p_paused) {}
