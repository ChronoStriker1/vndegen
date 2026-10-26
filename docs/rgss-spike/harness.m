// Headless libretro host for the RGSS spike: loads the mkxp-z core, renders through ANGLE on Metal
// into an offscreen framebuffer, feeds scripted joypad input, and writes chosen frames as PNG.
//   harness <core.dylib> <Game.ini> <outdir> <frames> [shot frames,comma separated] [input script]
// Input script: "frame:duration:button" entries separated by commas, buttons a b x y start select up down left right l r.
#import <Foundation/Foundation.h>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <signal.h>
#include <pthread.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include "libretro.h"

#ifndef EGL_PLATFORM_ANGLE_ANGLE
#define EGL_PLATFORM_ANGLE_ANGLE 0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE 0x3203
#endif
#ifndef EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE
#define EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE 0x3489
#endif

// ANGLE as mkxp-z builds it routes EGL through a shim that can also load the system EGL. Always use ANGLE.
bool mkxp_use_angle = true;

static EGLDisplay display;
static EGLContext context;
static EGLSurface surface;
static GLuint fbo, colorRb, depthRb;
extern struct retro_vfs_interface vndegen_posix_vfs;
static struct retro_hw_render_callback hw;
static struct retro_system_av_info av;
static struct retro_frame_time_callback frameTime;
static bool hasFrameTime, shutdownRequested;
static NSString *outDir;
static long frame;
static NSMutableIndexSet *shots;
static uint16_t buttons;
static unsigned lastW, lastH;
static double audioSum;
static long audioSamples, audioNonZero;
static FILE *wav;
// Core option values: the core's defaults, overridden by RGSS_OPT_<key> environment variables.
static NSMutableDictionary<NSString *, NSString *> *options;

static void crashed(int sig) {
    void *frames[64];
    int n = backtrace(frames, 64);
    fprintf(stderr, "[host] signal %d at frame %ld\n", sig, frame);
    backtrace_symbols_fd(frames, n, 2);
    _exit(128 + sig);
}

static void logcb(enum retro_log_level level, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[core %d] ", level);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static uintptr_t currentFramebuffer(void) { return fbo; }
static retro_proc_address_t procAddress(const char *sym) { return (retro_proc_address_t)eglGetProcAddress(sym); }

static bool environment(unsigned cmd, void *data) {
    switch (cmd) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = logcb;
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *(enum retro_pixel_format *)data == RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        struct retro_hw_render_callback *cb = data;
        if (cb->context_type != RETRO_HW_CONTEXT_OPENGLES3) return false;
        hw = *cb;
        cb->get_current_framebuffer = currentFramebuffer;
        cb->get_proc_address = procAddress;
        hw.get_current_framebuffer = currentFramebuffer;
        hw.get_proc_address = procAddress;
        fprintf(stderr, "[host] accepted GLES3 context, bottom_left_origin=%d depth=%d stencil=%d\n", cb->bottom_left_origin, cb->depth, cb->stencil);
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY: {
        static char path[1024];
        snprintf(path, sizeof path, "%s/system", outDir.UTF8String);
        [NSFileManager.defaultManager createDirectoryAtPath:@(path) withIntermediateDirectories:YES attributes:nil error:nil];
        *(const char **)data = path;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY: {
        static char path[1024];
        snprintf(path, sizeof path, "%s/saves", outDir.UTF8String);
        [NSFileManager.defaultManager createDirectoryAtPath:@(path) withIntermediateDirectories:YES attributes:nil error:nil];
        *(const char **)data = path;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
        struct retro_vfs_interface_info *info = data;
        if (info->required_interface_version > 3) return false;
        info->required_interface_version = 3;
        info->iface = &vndegen_posix_vfs;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *(bool *)data = true;
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;
    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
        frameTime = *(struct retro_frame_time_callback *)data;
        hasFrameTime = true;
        return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        av = *(struct retro_system_av_info *)data;
        fprintf(stderr, "[host] av info %ux%u max %ux%u fps %.2f rate %.0f\n", av.geometry.base_width, av.geometry.base_height, av.geometry.max_width, av.geometry.max_height, av.timing.fps, av.timing.sample_rate);
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        av.geometry = *(struct retro_game_geometry *)data;
        return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE:
        fprintf(stderr, "[host] message: %s\n", ((struct retro_message *)data)->msg);
        return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE_EXT:
        fprintf(stderr, "[host] message: %s\n", ((struct retro_message_ext *)data)->msg);
        return true;
    case RETRO_ENVIRONMENT_SHUTDOWN:
        shutdownRequested = true;
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *(unsigned *)data = 2;
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
        const struct retro_core_options_v2 *o = data;
        for (const struct retro_core_option_v2_definition *d = o->definitions; d && d->key; d++) {
            const char *def = d->default_value ? d->default_value : d->values[0].value;
            options[@(d->key)] = @(def ? def : "");
        }
        return true;
    }
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        for (const struct retro_variable *v = data; v && v->key; v++) {
            NSString *spec = @(v->value);
            NSRange semi = [spec rangeOfString:@"; "];
            NSString *values = semi.location == NSNotFound ? spec : [spec substringFromIndex:NSMaxRange(semi)];
            options[@(v->key)] = [values componentsSeparatedByString:@"|"].firstObject;
        }
        return true;
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
        return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;
        NSString *key = @(v->key);
        NSString *value = NSProcessInfo.processInfo.environment[[@"RGSS_OPT_" stringByAppendingString:key]] ?: options[key];
        if (!value) {
            fprintf(stderr, "[host] unknown option %s\n", v->key);
            return false;
        }
        static NSMutableDictionary<NSString *, NSData *> *strings;
        if (!strings) strings = [NSMutableDictionary dictionary];
        NSData *c = strings[value] ?: [NSData dataWithBytes:value.UTF8String length:strlen(value.UTF8String) + 1];
        strings[value] = c;
        v->value = c.bytes;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)data = false;
        return true;
    default:
        fprintf(stderr, "[host] unhandled environment %u\n", cmd & 0xffff);
        return false;
    }
}

static void writePNG(unsigned w, unsigned h, long n) {
    NSMutableData *pixels = [NSMutableData dataWithLength:(NSUInteger)w * h * 4];
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glReadPixels(0, 0, (GLsizei)w, (GLsizei)h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.mutableBytes);
    if (hw.bottom_left_origin) {
        uint8_t *p = pixels.mutableBytes;
        NSMutableData *row = [NSMutableData dataWithLength:w * 4];
        for (unsigned y = 0; y < h / 2; y++) {
            memcpy(row.mutableBytes, p + y * w * 4, w * 4);
            memcpy(p + y * w * 4, p + (h - 1 - y) * w * 4, w * 4);
            memcpy(p + (h - 1 - y) * w * 4, row.bytes, w * 4);
        }
    }
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef dp = CGDataProviderCreateWithCFData((__bridge CFDataRef)pixels);
    CGImageRef img = CGImageCreate(w, h, 8, 32, w * 4, cs, kCGImageAlphaNoneSkipLast | kCGBitmapByteOrderDefault, dp, NULL, false, kCGRenderingIntentDefault);
    NSURL *url = [NSURL fileURLWithPath:[outDir stringByAppendingFormat:@"/frame-%05ld.png", n]];
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL((__bridge CFURLRef)url, CFSTR("public.png"), 1, NULL);
    CGImageDestinationAddImage(dest, img, NULL);
    CGImageDestinationFinalize(dest);
    CFRelease(dest);
    CGImageRelease(img);
    CGDataProviderRelease(dp);
    CGColorSpaceRelease(cs);
    fprintf(stderr, "[host] wrote frame %ld (%ux%u)\n", n, w, h);
}

static void videoRefresh(const void *data, unsigned w, unsigned h, size_t pitch) {
    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        lastW = w;
        lastH = h;
    }
    if ([shots containsIndex:(NSUInteger)frame] && lastW) writePNG(lastW, lastH, frame);
}

static size_t audioBatch(const int16_t *data, size_t frames) {
    for (size_t i = 0; i < frames * 2; i++) {
        audioSum += (double)data[i] * data[i];
        if (data[i]) audioNonZero++;
    }
    audioSamples += (long)frames;
    if (wav) fwrite(data, 4, frames, wav);
    return frames;
}

static void audioSample(int16_t l, int16_t r) {
    int16_t s[2] = {l, r};
    audioBatch(s, 1);
}

static void inputPoll(void) {}

static int16_t inputState(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (port != 0 || device != RETRO_DEVICE_JOYPAD) return 0;
    if (id == RETRO_DEVICE_ID_JOYPAD_MASK) return (int16_t)buttons;
    return (buttons >> id) & 1;
}

static int buttonID(NSString *name) {
    NSDictionary *ids = @{@"a": @(RETRO_DEVICE_ID_JOYPAD_A), @"b": @(RETRO_DEVICE_ID_JOYPAD_B), @"x": @(RETRO_DEVICE_ID_JOYPAD_X),
                          @"y": @(RETRO_DEVICE_ID_JOYPAD_Y), @"start": @(RETRO_DEVICE_ID_JOYPAD_START), @"select": @(RETRO_DEVICE_ID_JOYPAD_SELECT),
                          @"up": @(RETRO_DEVICE_ID_JOYPAD_UP), @"down": @(RETRO_DEVICE_ID_JOYPAD_DOWN), @"left": @(RETRO_DEVICE_ID_JOYPAD_LEFT),
                          @"right": @(RETRO_DEVICE_ID_JOYPAD_RIGHT), @"l": @(RETRO_DEVICE_ID_JOYPAD_L), @"r": @(RETRO_DEVICE_ID_JOYPAD_R)};
    NSNumber *n = ids[name];
    return n ? n.intValue : -1;
}

#define SYM(name) typeof(&name) p_##name = dlsym(core, #name); if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 1; }

static int run(int argc, char **argv) {
    @autoreleasepool {
        if (argc < 5) {
            fprintf(stderr, "usage: harness core.dylib Game.ini outdir frames [shots] [inputs]\n");
            return 2;
        }
        signal(SIGSEGV, crashed);
        signal(SIGBUS, crashed);
        signal(SIGABRT, crashed);
        outDir = [NSURL fileURLWithPath:@(argv[3])].path;
        [NSFileManager.defaultManager createDirectoryAtPath:outDir withIntermediateDirectories:YES attributes:nil error:nil];
        long frames = atol(argv[4]);
        options = [NSMutableDictionary dictionary];
        shots = [NSMutableIndexSet indexSet];
        if (argc > 5) for (NSString *s in [@(argv[5]) componentsSeparatedByString:@","]) if (s.length) [shots addIndex:(NSUInteger)s.integerValue];
        NSMutableArray *inputs = [NSMutableArray array];
        if (argc > 6) for (NSString *s in [@(argv[6]) componentsSeparatedByString:@","]) {
            NSArray *p = [s componentsSeparatedByString:@":"];
            if (p.count == 3) [inputs addObject:@[@([p[0] integerValue]), @([p[1] integerValue]), @(buttonID(p[2]))]];
        }
        wav = fopen([outDir stringByAppendingString:@"/audio.raw"].UTF8String, "wb");

        void *core = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
        if (!core) { fprintf(stderr, "dlopen: %s\n", dlerror()); return 1; }
        SYM(retro_set_environment) SYM(retro_set_video_refresh) SYM(retro_set_audio_sample) SYM(retro_set_audio_sample_batch)
        SYM(retro_set_input_poll) SYM(retro_set_input_state) SYM(retro_init) SYM(retro_load_game) SYM(retro_get_system_av_info)
        SYM(retro_run) SYM(retro_unload_game) SYM(retro_deinit)

        EGLint platform[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE, EGL_NONE};
        PFNEGLGETPLATFORMDISPLAYEXTPROC getPlatformDisplay = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
        display = getPlatformDisplay ? getPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, platform) : eglGetDisplay(EGL_DEFAULT_DISPLAY);
        EGLint major, minor;
        if (!eglInitialize(display, &major, &minor)) { fprintf(stderr, "eglInitialize failed 0x%x\n", eglGetError()); return 1; }
        EGLint cfgAttrs[] = {EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                             EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
        EGLConfig cfg;
        EGLint n;
        eglChooseConfig(display, cfgAttrs, &cfg, 1, &n);
        EGLint pbAttrs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
        surface = eglCreatePbufferSurface(display, cfg, pbAttrs);
        EGLint ctxAttrs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, cfg, EGL_NO_CONTEXT, ctxAttrs);
        if (!eglMakeCurrent(display, surface, surface, context)) { fprintf(stderr, "eglMakeCurrent failed 0x%x\n", eglGetError()); return 1; }
        fprintf(stderr, "[host] EGL %d.%d GL_RENDERER=%s GL_VERSION=%s\n", major, minor, glGetString(GL_RENDERER), glGetString(GL_VERSION));

        p_retro_set_environment(environment);
        p_retro_set_video_refresh(videoRefresh);
        p_retro_set_audio_sample(audioSample);
        p_retro_set_audio_sample_batch(audioBatch);
        p_retro_set_input_poll(inputPoll);
        p_retro_set_input_state(inputState);
        p_retro_init();

        struct retro_game_info info = {.path = argv[2]};
        NSDate *t0 = [NSDate date];
        if (!p_retro_load_game(&info)) { fprintf(stderr, "retro_load_game failed\n"); return 1; }
        p_retro_get_system_av_info(&av);
        fprintf(stderr, "[host] geometry %ux%u max %ux%u fps %.2f rate %.0f\n", av.geometry.base_width, av.geometry.base_height, av.geometry.max_width, av.geometry.max_height, av.timing.fps, av.timing.sample_rate);

        unsigned fw = av.geometry.max_width ? av.geometry.max_width : 640, fh = av.geometry.max_height ? av.geometry.max_height : 480;
        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glGenRenderbuffers(1, &colorRb);
        glBindRenderbuffer(GL_RENDERBUFFER, colorRb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, (GLsizei)fw, (GLsizei)fh);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRb);
        glGenRenderbuffers(1, &depthRb);
        glBindRenderbuffer(GL_RENDERBUFFER, depthRb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)fw, (GLsizei)fh);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRb);
        fprintf(stderr, "[host] fbo %u status 0x%x size %ux%u\n", fbo, glCheckFramebufferStatus(GL_FRAMEBUFFER), fw, fh);
        if (hw.context_reset) hw.context_reset();

        retro_usec_t usec = (retro_usec_t)(1000000.0 / (av.timing.fps > 0 ? av.timing.fps : 60));
        NSDate *t1 = [NSDate date];
        for (frame = 0; frame < frames && !shutdownRequested; frame++) {
            buttons = 0;
            for (NSArray *e in inputs) {
                long start = [e[0] longValue], dur = [e[1] longValue];
                int b = [e[2] intValue];
                if (b >= 0 && frame >= start && frame < start + dur) buttons |= (uint16_t)(1 << b);
            }
            if (hasFrameTime) frameTime.callback(frameTime.reference ? frameTime.reference : usec);
            p_retro_run();
        }
        double runSecs = -[t1 timeIntervalSinceNow];
        fprintf(stderr, "[host] ran %ld frames in %.2fs (%.1f fps, load %.2fs) shutdown=%d\n", frame, runSecs, frame / runSecs, [t1 timeIntervalSinceDate:t0], shutdownRequested);
        fprintf(stderr, "[host] audio %ld frames, rms %.1f, nonzero %ld\n", audioSamples, audioSamples ? sqrt(audioSum / (audioSamples * 2)) : 0, audioNonZero);
        if (wav) fclose(wav);
        p_retro_unload_game();
        p_retro_deinit();
    }
    return 0;
}

struct args { int argc; char **argv; int result; };

static void *runThread(void *p) {
    struct args *a = p;
    a->result = run(a->argc, a->argv);
    return NULL;
}

// The core runs on a thread whose stack size comes from RGSS_STACK_MB, because wasm2c code uses the native stack.
int main(int argc, char **argv) {
    const char *mb = getenv("RGSS_STACK_MB");
    if (!mb) return run(argc, argv);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, (size_t)atol(mb) << 20);
    struct args a = {argc, argv, 0};
    pthread_t t;
    pthread_create(&t, &attr, runThread, &a);
    pthread_join(t, NULL);
    return a.result;
}
