# VNDegen's module: the C interface in vngd.h and the OS that runs a game inside VNDegen's view.
def can_build(env, platform):
    return platform in ("iphone", "osx", "x11")


def configure(env):
    pass
