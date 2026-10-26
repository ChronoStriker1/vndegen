// VNDegen's libretro frontend for mkxp-z: the functions in vnrgss.h. It gives the core an OpenGL ES 3 context from
// ANGLE on Metal, draws the core's framebuffer letterboxed into the host's layer, plays the core's audio through
// AVAudioEngine, and feeds it the keys and mouse VNDegen sends. The core is linked into the same framework.
#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_angle.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "libretro.h"
#include "vnrgss.h"

extern "C" struct retro_vfs_interface vndegen_posix_vfs;

// ANGLE as mkxp-z builds it routes EGL through a shim that can also load the system's EGL. VNDegen always uses ANGLE.
extern "C" bool mkxp_use_angle = true;

namespace {

// The preload script VNDegen adds to the core's own, enabled through its core option, whose key is the file name in hex.
constexpr const char *preloadOption = "mkxp-z_preload-7a7a5f766e646567656e2e7262";

struct AudioRing {
    // Stereo frames of 16-bit samples. One second at 48 kHz, far more than the few frames the core runs ahead.
    std::vector<int16_t> samples = std::vector<int16_t>(48000 * 2);
    std::atomic<size_t> readFrame{0}, writeFrame{0};

    size_t capacity() const { return samples.size() / 2; }

    void push(const int16_t *data, size_t frames) {
        size_t write = writeFrame.load(std::memory_order_relaxed);
        size_t read = readFrame.load(std::memory_order_acquire);
        size_t room = capacity() - (write - read);
        frames = std::min(frames, room);
        for (size_t i = 0; i < frames; i++) {
            size_t slot = (write + i) % capacity();
            samples[slot * 2] = data[i * 2];
            samples[slot * 2 + 1] = data[i * 2 + 1];
        }
        writeFrame.store(write + frames, std::memory_order_release);
    }

    // Fills `frames` float frames, silence where the core has not produced enough.
    void pull(float *left, float *right, size_t frames) {
        size_t read = readFrame.load(std::memory_order_relaxed);
        size_t available = writeFrame.load(std::memory_order_acquire) - read;
        size_t n = std::min(frames, available);
        for (size_t i = 0; i < n; i++) {
            size_t slot = (read + i) % capacity();
            left[i] = samples[slot * 2] / 32768.0f;
            right[i] = samples[slot * 2 + 1] / 32768.0f;
        }
        for (size_t i = n; i < frames; i++) left[i] = right[i] = 0;
        readFrame.store(read + n, std::memory_order_release);
    }

    void clear() { readFrame.store(writeFrame.load()); }
};

struct Engine {
    bool running = false;
    bool ended = false;
    bool paused = false;
    std::string systemDir, saveDir, gameIni;
    FILE *log = nullptr;
    std::string problem;
    bool problemSet = false;

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    GLuint fbo = 0, colorBuffer = 0, depthBuffer = 0;
    unsigned fboWidth = 0, fboHeight = 0;
    unsigned frameWidth = 0, frameHeight = 0;
    // Where the frame lands in the layer, normalized, for mapping the mouse.
    double viewX = 0, viewY = 0, viewWidth = 1, viewHeight = 1;

    retro_hw_render_callback hw{};
    retro_system_av_info av{};
    retro_frame_time_callback frameTime{};
    bool hasFrameTime = false;
    retro_keyboard_event_t keyboard = nullptr;

    bool keys[RETROK_LAST] = {};
    double mouseX = 0.5, mouseY = 0.5;
    bool mouseButtons[3] = {};
    double wheel = 0;

    AudioRing ring;
    AVAudioEngine *audio = nil;
};

Engine engine;

void logLine(const char *line) {
    FILE *out = engine.log ? engine.log : stderr;
    fputs(line, out);
    size_t length = strlen(line);
    if (length == 0 || line[length - 1] != '\n') fputc('\n', out);
    fflush(out);

    // The core logs the end of the game and the Ruby exception that ended it. The first exception line is the message.
    if (strstr(line, "Game exited; terminating") || strstr(line, "Game threw an exception; terminating")) engine.ended = true;
    const char *exception = strstr(line, "[mkxp-z exception] ");
    if (exception && !engine.problemSet) {
        std::string text = exception + strlen("[mkxp-z exception] ");
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        engine.problem = "The game stopped with an error: " + text;
        engine.problemSet = true;
    }
}

void logPrintf(enum retro_log_level, const char *format, ...) {
    char buffer[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    logLine(buffer);
}

uintptr_t currentFramebuffer() { return engine.fbo; }

retro_proc_address_t procAddress(const char *name) { return (retro_proc_address_t)eglGetProcAddress(name); }

const char *optionValue(const char *key) {
    if (!strcmp(key, preloadOption)) return "enabled";
    return nullptr;
}

// Core option defaults, kept so the core always gets a value back, as a libretro frontend gives it.
std::vector<std::pair<std::string, std::string>> &defaults() {
    static std::vector<std::pair<std::string, std::string>> values;
    return values;
}

const char *defaultValue(const char *key) {
    for (auto &entry : defaults()) {
        if (entry.first == key) return entry.second.c_str();
    }
    return nullptr;
}

bool environment(unsigned command, void *data) {
    switch (command) {
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        static_cast<retro_log_callback *>(data)->log = logPrintf;
        return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        return *static_cast<retro_pixel_format *>(data) == RETRO_PIXEL_FORMAT_XRGB8888;
    case RETRO_ENVIRONMENT_SET_HW_RENDER: {
        auto *callback = static_cast<retro_hw_render_callback *>(data);
        // ANGLE on Metal is OpenGL ES 3.0, which the core asks for after the desktop versions.
        if (callback->context_type != RETRO_HW_CONTEXT_OPENGLES3) return false;
        callback->get_current_framebuffer = currentFramebuffer;
        callback->get_proc_address = procAddress;
        engine.hw = *callback;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
        *static_cast<const char **>(data) = engine.systemDir.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *static_cast<const char **>(data) = engine.saveDir.c_str();
        return true;
    case RETRO_ENVIRONMENT_GET_VFS_INTERFACE: {
        auto *info = static_cast<retro_vfs_interface_info *>(data);
        if (info->required_interface_version > 3) return false;
        info->required_interface_version = 3;
        info->iface = &vndegen_posix_vfs;
        return true;
    }
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        *static_cast<bool *>(data) = true;
        return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
        return true;
    case RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK:
        engine.frameTime = *static_cast<retro_frame_time_callback *>(data);
        engine.hasFrameTime = true;
        return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        engine.av = *static_cast<retro_system_av_info *>(data);
        return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        engine.av.geometry = *static_cast<retro_game_geometry *>(data);
        return true;
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
        engine.keyboard = static_cast<retro_keyboard_callback *>(data)->callback;
        return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE: {
        std::string line = std::string("message: ") + static_cast<retro_message *>(data)->msg;
        logLine(line.c_str());
        return true;
    }
    case RETRO_ENVIRONMENT_SET_MESSAGE_EXT: {
        std::string line = std::string("message: ") + static_cast<retro_message_ext *>(data)->msg;
        logLine(line.c_str());
        return true;
    }
    case RETRO_ENVIRONMENT_SHUTDOWN:
        engine.ended = true;
        return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *static_cast<unsigned *>(data) = 2;
        return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2: {
        defaults().clear();
        for (auto *definition = static_cast<retro_core_options_v2 *>(data)->definitions; definition && definition->key; definition++) {
            const char *value = definition->default_value ? definition->default_value : definition->values[0].value;
            defaults().emplace_back(definition->key, value ? value : "");
        }
        return true;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        auto *variable = static_cast<retro_variable *>(data);
        const char *value = optionValue(variable->key);
        if (!value) value = defaultValue(variable->key);
        variable->value = value;
        return value != nullptr;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *static_cast<bool *>(data) = false;
        return true;
    default:
        return false;
    }
}

void videoRefresh(const void *data, unsigned width, unsigned height, size_t) {
    if (data == RETRO_HW_FRAME_BUFFER_VALID) {
        engine.frameWidth = width;
        engine.frameHeight = height;
    }
}

size_t audioBatch(const int16_t *data, size_t frames) {
    if (!engine.paused) engine.ring.push(data, frames);
    return frames;
}

void audioSample(int16_t left, int16_t right) {
    int16_t frame[2] = {left, right};
    audioBatch(frame, 1);
}

void inputPoll() {}

int16_t inputState(unsigned port, unsigned device, unsigned, unsigned id) {
    if (port != 0) return 0;
    switch (device) {
    case RETRO_DEVICE_MOUSE:
        switch (id) {
        case RETRO_DEVICE_ID_MOUSE_LEFT: return engine.mouseButtons[0];
        case RETRO_DEVICE_ID_MOUSE_RIGHT: return engine.mouseButtons[1];
        case RETRO_DEVICE_ID_MOUSE_MIDDLE: return engine.mouseButtons[2];
        case RETRO_DEVICE_ID_MOUSE_WHEELUP:
        case RETRO_DEVICE_ID_MOUSE_WHEELDOWN: {
            // The core reads each direction once a frame, so a notch is reported once, then cleared.
            bool up = id == RETRO_DEVICE_ID_MOUSE_WHEELUP;
            if ((up && engine.wheel >= 1) || (!up && engine.wheel <= -1)) {
                engine.wheel += up ? -1 : 1;
                return 1;
            }
            return 0;
        }
        default: return 0;
        }
    case RETRO_DEVICE_POINTER: {
        // The pointer is relative to the game's frame, -0x7fff at its top left and 0x7fff at its bottom right.
        double x = (engine.mouseX - engine.viewX) / engine.viewWidth, y = (engine.mouseY - engine.viewY) / engine.viewHeight;
        switch (id) {
        case RETRO_DEVICE_ID_POINTER_X: return (int16_t)std::lround(std::clamp(x * 2 - 1, -1.0, 1.0) * 0x7fff);
        case RETRO_DEVICE_ID_POINTER_Y: return (int16_t)std::lround(std::clamp(y * 2 - 1, -1.0, 1.0) * 0x7fff);
        case RETRO_DEVICE_ID_POINTER_PRESSED: return engine.mouseButtons[0];
        case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN: return x < 0 || x > 1 || y < 0 || y > 1;
        default: return 0;
        }
    }
    default:
        return 0;
    }
}

bool makeFramebuffer(unsigned width, unsigned height) {
    if (engine.fbo && width <= engine.fboWidth && height <= engine.fboHeight) return true;
    if (engine.fbo) {
        glDeleteFramebuffers(1, &engine.fbo);
        glDeleteRenderbuffers(1, &engine.colorBuffer);
        glDeleteRenderbuffers(1, &engine.depthBuffer);
    }
    glGenFramebuffers(1, &engine.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, engine.fbo);
    glGenRenderbuffers(1, &engine.colorBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, engine.colorBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, (GLsizei)width, (GLsizei)height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, engine.colorBuffer);
    glGenRenderbuffers(1, &engine.depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, engine.depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)width, (GLsizei)height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, engine.depthBuffer);
    engine.fboWidth = width;
    engine.fboHeight = height;
    return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

// Draws the core's last frame letterboxed into the layer. The core draws with the top row first, so the copy flips it.
void present() {
    EGLint width = 0, height = 0;
    eglQuerySurface(engine.display, engine.surface, EGL_WIDTH, &width);
    eglQuerySurface(engine.display, engine.surface, EGL_HEIGHT, &height);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (engine.frameWidth && engine.frameHeight && width > 0 && height > 0) {
        double scale = std::min((double)width / engine.frameWidth, (double)height / engine.frameHeight);
        GLint w = (GLint)std::lround(engine.frameWidth * scale), h = (GLint)std::lround(engine.frameHeight * scale);
        GLint x = (width - w) / 2, y = (height - h) / 2;
        engine.viewX = (double)x / width;
        engine.viewY = (double)y / height;
        engine.viewWidth = (double)w / width;
        engine.viewHeight = (double)h / height;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, engine.fbo);
        glBlitFramebuffer(0, 0, (GLint)engine.frameWidth, (GLint)engine.frameHeight, x, y + h, x + w, y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    }
    eglSwapBuffers(engine.display, engine.surface);
}

bool startGraphics(void *layer) {
    EGLAttrib platform[] = {EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE, EGL_NONE};
    engine.display = eglGetPlatformDisplay(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, platform);
    if (engine.display == EGL_NO_DISPLAY || !eglInitialize(engine.display, nullptr, nullptr)) return false;
    EGLint configAttributes[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                                 EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
    EGLConfig config;
    EGLint count = 0;
    if (!eglChooseConfig(engine.display, configAttributes, &config, 1, &count) || count == 0) return false;
    engine.surface = eglCreateWindowSurface(engine.display, config, (EGLNativeWindowType)layer, nullptr);
    EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    engine.context = eglCreateContext(engine.display, config, EGL_NO_CONTEXT, contextAttributes);
    if (engine.surface == EGL_NO_SURFACE || engine.context == EGL_NO_CONTEXT) return false;
    if (!eglMakeCurrent(engine.display, engine.surface, engine.surface, engine.context)) return false;
    eglSwapInterval(engine.display, 0);
    char line[512];
    snprintf(line, sizeof line, "renderer: %s, %s", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    logLine(line);
    return true;
}

void stopGraphics() {
    if (engine.display == EGL_NO_DISPLAY) return;
    eglMakeCurrent(engine.display, engine.surface, engine.surface, engine.context);
    if (engine.fbo) {
        glDeleteFramebuffers(1, &engine.fbo);
        glDeleteRenderbuffers(1, &engine.colorBuffer);
        glDeleteRenderbuffers(1, &engine.depthBuffer);
    }
    engine.fbo = engine.colorBuffer = engine.depthBuffer = 0;
    engine.fboWidth = engine.fboHeight = 0;
    eglMakeCurrent(engine.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (engine.context != EGL_NO_CONTEXT) eglDestroyContext(engine.display, engine.context);
    if (engine.surface != EGL_NO_SURFACE) eglDestroySurface(engine.display, engine.surface);
    eglTerminate(engine.display);
    engine.display = EGL_NO_DISPLAY;
    engine.context = EGL_NO_CONTEXT;
    engine.surface = EGL_NO_SURFACE;
}

bool startAudio(double sampleRate) {
    AVAudioFormat *format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:sampleRate channels:2];
    AVAudioSourceNode *source = [[AVAudioSourceNode alloc] initWithFormat:format renderBlock:^OSStatus(
        BOOL *silence, const AudioTimeStamp *, AVAudioFrameCount frames, AudioBufferList *buffers) {
        if (buffers->mNumberBuffers < 2) {
            *silence = YES;
            return noErr;
        }
        engine.ring.pull((float *)buffers->mBuffers[0].mData, (float *)buffers->mBuffers[1].mData, frames);
        return noErr;
    }];
    engine.audio = [[AVAudioEngine alloc] init];
    [engine.audio attachNode:source];
    [engine.audio connect:source to:engine.audio.mainMixerNode format:format];
    NSError *error = nil;
    if (![engine.audio startAndReturnError:&error]) {
        logPrintf(RETRO_LOG_ERROR, "audio did not start: %s", error.localizedDescription.UTF8String);
        return false;
    }
    return true;
}

void stopAudio() {
    [engine.audio stop];
    engine.audio = nil;
    engine.ring.clear();
}

} // namespace

extern "C" {

int vnrgss_start(const VNRGSSLaunch *launch) {
    engine.ended = false;
    engine.paused = false;
    engine.problem.clear();
    engine.problemSet = false;
    engine.frameWidth = engine.frameHeight = 0;
    engine.hasFrameTime = false;
    engine.keyboard = nullptr;
    memset(engine.keys, 0, sizeof engine.keys);
    memset(engine.mouseButtons, 0, sizeof engine.mouseButtons);
    engine.wheel = 0;
    engine.systemDir = launch->systemDir;
    engine.saveDir = launch->saveDir;
    engine.gameIni = launch->gameIni;
    engine.log = launch->logPath ? fopen(launch->logPath, "w") : nullptr;

    if (!startGraphics(launch->layer)) {
        logPrintf(RETRO_LOG_ERROR, "OpenGL ES through ANGLE did not start: EGL error 0x%x", eglGetError());
        engine.problem = "The game's graphics could not start.";
        engine.problemSet = true;
        return 1;
    }
    retro_set_environment(environment);
    retro_set_video_refresh(videoRefresh);
    retro_set_audio_sample(audioSample);
    retro_set_audio_sample_batch(audioBatch);
    retro_set_input_poll(inputPoll);
    retro_set_input_state(inputState);
    retro_init();
    engine.running = true;

    retro_game_info info{};
    info.path = engine.gameIni.c_str();
    if (!retro_load_game(&info)) {
        if (!engine.problemSet) engine.problem = "The engine could not load this game. Its log has the details.";
        engine.problemSet = true;
        return 1;
    }
    retro_get_system_av_info(&engine.av);
    unsigned width = engine.av.geometry.max_width ? engine.av.geometry.max_width : 640;
    unsigned height = engine.av.geometry.max_height ? engine.av.geometry.max_height : 480;
    if (!makeFramebuffer(width, height)) {
        engine.problem = "The game's graphics could not start.";
        engine.problemSet = true;
        return 1;
    }
    if (engine.hw.context_reset) engine.hw.context_reset();
    startAudio(engine.av.timing.sample_rate > 0 ? engine.av.timing.sample_rate : 48000);
    return 0;
}

int vnrgss_frame(double seconds) {
    if (!engine.running || engine.ended) return 1;
    if (engine.paused) return 0;
    eglMakeCurrent(engine.display, engine.surface, engine.surface, engine.context);
    if (engine.av.geometry.max_width > engine.fboWidth || engine.av.geometry.max_height > engine.fboHeight) {
        makeFramebuffer(std::max(engine.av.geometry.max_width, engine.fboWidth), std::max(engine.av.geometry.max_height, engine.fboHeight));
    }
    if (engine.hasFrameTime && engine.frameTime.callback) {
        engine.frameTime.callback((retro_usec_t)std::lround(std::clamp(seconds, 0.0, 0.25) * 1e6));
    }
    retro_run();
    present();
    return engine.ended ? 1 : 0;
}

void vnrgss_key(unsigned code, bool down) {
    if (!engine.running || code >= RETROK_LAST || engine.keys[code] == down) return;
    engine.keys[code] = down;
    if (!engine.keyboard) return;
    uint16_t modifiers = 0;
    if (engine.keys[RETROK_LSHIFT] || engine.keys[RETROK_RSHIFT]) modifiers |= RETROKMOD_SHIFT;
    if (engine.keys[RETROK_LCTRL] || engine.keys[RETROK_RCTRL]) modifiers |= RETROKMOD_CTRL;
    if (engine.keys[RETROK_LALT] || engine.keys[RETROK_RALT]) modifiers |= RETROKMOD_ALT;
    uint32_t character = code < 128 && code >= 32 ? code : 0;
    engine.keyboard(down, code, character, modifiers);
}

void vnrgss_mouse_move(float x, float y) {
    engine.mouseX = x;
    engine.mouseY = y;
}

void vnrgss_mouse_button(int button, bool down) {
    if (button >= 0 && button < 3) engine.mouseButtons[button] = down;
}

void vnrgss_wheel(float notches) { engine.wheel += notches; }

void vnrgss_set_paused(bool paused) {
    if (engine.paused == paused) return;
    engine.paused = paused;
    if (!engine.audio) return;
    if (paused) {
        [engine.audio pause];
        engine.ring.clear();
    } else {
        [engine.audio startAndReturnError:nil];
    }
}

void vnrgss_stop(void) {
    if (engine.audio) stopAudio();
    if (engine.running) {
        if (engine.display != EGL_NO_DISPLAY) eglMakeCurrent(engine.display, engine.surface, engine.surface, engine.context);
        retro_unload_game();
        retro_deinit();
        engine.running = false;
    }
    stopGraphics();
    if (engine.log) {
        fclose(engine.log);
        engine.log = nullptr;
    }
}

const char *vnrgss_problem(void) { return engine.problemSet ? engine.problem.c_str() : nullptr; }

}
