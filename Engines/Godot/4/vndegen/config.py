# VNDegen's module: the C interface in vngd.h, the OS and display server that run a game inside VNDegen's view.
def can_build(env, platform):
    return platform in ("macos", "ios", "linuxbsd")


def configure(env):
    pass
