# The RGSS framework's binary: VNDegen's libretro frontend, ANGLE, and mkxp-z's libretro core, linked as one library
# that exports only the functions in vnrgss.h. scripts/rgss-engine.sh builds it once per Apple platform, with mkxp-z's
# subprojects folder beside this file for ANGLE, Engines/Shared beside it as `shared`, and the static core passed in as
# `core`.
project('vnrgss', 'c', 'cpp', 'objcpp', meson_version: '>=1.4.0',
    default_options: ['default_library=static', 'buildtype=release', 'cpp_std=c++17'])

is_macos = meson.get_compiler('c').compute_int('!!TARGET_OS_OSX', prefix: '#include <TargetConditionals.h>', low: 0, high: 1) != 0

subproject('common', default_options: {'c_std': 'gnu11,c11', 'cpp_std': 'gnu++14,c++14'})
subproject('xxhash', default_options: {'default_library': 'static', 'c_std': 'gnu11,c11'})
meson.override_dependency('zlib', declare_dependency(link_args: ['-lz']))
angle = subproject('angle', default_options: {
    'default_library': 'static', 'c_std': 'gnu11,c11', 'cpp_std': 'gnu++14,c++14',
    'direct3d11': 'disabled', 'metal': 'enabled', 'null': 'disabled', 'opengl': 'disabled', 'vulkan': 'disabled',
    'wayland': 'disabled', 'x11': 'disabled',
})

frameworks = ['Foundation', 'AVFoundation', 'AudioToolbox', 'CoreAudio', 'CoreGraphics', 'IOSurface', 'Metal', 'QuartzCore']
frameworks += is_macos ? ['AppKit', 'IOKit'] : ['UIKit']

shared_library('RGSS', 'vnrgss.mm', 'vfs.c', 'shared/casepath.c',
    name_prefix: '',
    name_suffix: 'dylib',
    include_directories: include_directories(get_option('api_include'), 'shared'),
    dependencies: [angle.get_variable('angle_dep'), dependency('appleframeworks', modules: frameworks)],
    objcpp_args: ['-fobjc-arc'],
    link_args: [
        '-Wl,-force_load,' + get_option('core'), '-lc++', '-lz', '-liconv',
        '-Wl,-exported_symbols_list,' + meson.current_source_dir() / 'vnrgss.exports',
        '-Wl,-install_name,@rpath/RGSS.framework/RGSS', '-Wl,-dead_strip',
    ],
    gnu_symbol_visibility: 'hidden',
    # The core is a link argument, so Meson would not relink when only it changes.
    link_depends: get_option('core'),
)
