// The runtime's C interface (vnrgss.h) and its link to the game thread. The game's scripts run on a thread of their own
// that lives as long as the process, because Ruby can be set up only once per process. Graphics.update hands each frame
// over here and waits until the host grants the next one, at the game's frame rate.
#include "host.h"

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "audio.h"
#include "vnrgss.h"

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static pthread_t thread;
static bool threadMade;

static struct {
    HostLaunch launch;
    bool startPending, running, ended, stopRequested, paused;
    int granted;   // frames the game may still draw
    double carry;  // the part of a frame not yet granted
    int frameRate;
    Pixel *frame;  // the last frame the game handed over
    int frameW, frameH;
    uint64_t serial;
    HostInput input;
    bool latched[HOST_KEY_COUNT];
    bool mouseLatched[3];
    char problem[2048];
    bool hasProblem;
    char *pendingEval; // Ruby the Linux test host asks the game thread to run
    int speed;         // frames the game draws for each one its frame rate gives
    char *message;     // the message box the game waits on
} S = {.frameRate = 60, .speed = 1};

// Ruby the host asked to run, in order, and what it gave once it has run.
typedef struct RubyRequest {
    long id;
    char *code;   // NULL once it has started running
    char *result; // set once done
    bool failed, done;
    struct RubyRequest *next;
} RubyRequest;
static RubyRequest *requests;
static long lastRequest;

// The copy the main thread reads, so the game can draw its next frame meanwhile.
static Pixel *shown;
static int shownW, shownH;
static uint64_t shownSerial;

static FILE *logFile;
static pthread_mutex_t logLock = PTHREAD_MUTEX_INITIALIZER;

void rgss_log(const char *format, ...) {
    pthread_mutex_lock(&logLock);
    FILE *out = logFile ? logFile : stderr;
    va_list args;
    va_start(args, format);
    vfprintf(out, format, args);
    va_end(args);
    fputc('\n', out);
    fflush(out);
    pthread_mutex_unlock(&logLock);
}

static char *copy_string(const char *s) { return s ? strdup(s) : NULL; }

static void free_launch(HostLaunch *l) {
    free(l->gameIni);
    free(l->saveDir);
    free(l->logPath);
    free(l->soundFont);
    free(l->fallbackSoundFont);
    for (int i = 0; i < l->rtpCount; i++) free(l->rtps[i]);
    free(l->rtps);
    memset(l, 0, sizeof *l);
}

static void *game_thread(void *unused) {
    (void)unused;
    for (;;) {
        pthread_mutex_lock(&lock);
        while (!S.startPending) pthread_cond_wait(&cond, &lock);
        S.startPending = false;
        HostLaunch launch = S.launch;
        pthread_mutex_unlock(&lock);

        rgss_run_game(&launch);

        pthread_mutex_lock(&lock);
        for (RubyRequest *r = requests; r; r = r->next) {
            if (r->done) continue;
            free(r->code);
            r->code = NULL;
            r->result = strdup("The game ended.");
            r->failed = r->done = true;
        }
        S.running = false;
        S.ended = true;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// MARK: The game thread's side

bool host_present(const Image *frame) {
    pthread_mutex_lock(&lock);
    if (S.frameW != frame->w || S.frameH != frame->h) {
        free(S.frame);
        S.frame = malloc((size_t)frame->w * frame->h * sizeof(Pixel));
        S.frameW = frame->w;
        S.frameH = frame->h;
    }
    if (S.frame) memcpy(S.frame, frame->px, (size_t)frame->w * frame->h * sizeof(Pixel));
    S.serial++;
    pthread_cond_broadcast(&cond);
    bool go = host_wait_frame_locked();
    pthread_mutex_unlock(&lock);
    return go;
}

static RubyRequest *find_request(long id) {
    for (RubyRequest *r = requests; r; r = r->next) if (r->id == id) return r;
    return NULL;
}

// Runs the host's Ruby while the game waits for a frame, so its objects are between two frames.
static void run_requests_locked(void) {
    for (;;) {
        RubyRequest *r = requests;
        while (r && !r->code) r = r->next;
        if (!r) return;
        long id = r->id;
        char *code = r->code;
        r->code = NULL;
        pthread_mutex_unlock(&lock);
        bool failed = false;
        char *result = rgss_run_ruby(code, &failed);
        free(code);
        pthread_mutex_lock(&lock);
        // The host may have read or dropped requests meanwhile.
        if ((r = find_request(id))) {
            r->result = result;
            r->failed = failed;
            r->done = true;
        } else {
            free(result);
        }
    }
}

bool host_wait_frame_locked(void) {
    for (;;) {
        if (!S.stopRequested) run_requests_locked();
        if (S.granted > 0 || S.stopRequested) break;
        pthread_cond_wait(&cond, &lock);
    }
    if (S.stopRequested) return false;
    S.granted--;
    return true;
}

bool host_wait_frame(void) {
    pthread_mutex_lock(&lock);
    bool go = host_wait_frame_locked();
    pthread_mutex_unlock(&lock);
    return go;
}

bool host_stop_requested(void) {
    pthread_mutex_lock(&lock);
    bool stop = S.stopRequested;
    pthread_mutex_unlock(&lock);
    return stop;
}

void host_set_frame_rate(int rate) {
    pthread_mutex_lock(&lock);
    S.frameRate = rate;
    pthread_mutex_unlock(&lock);
}

bool host_frame_hidden(void) {
    pthread_mutex_lock(&lock);
    bool hidden = S.speed > 1 && S.granted > 1;
    pthread_mutex_unlock(&lock);
    return hidden;
}

void host_frame_reset(void) {
    pthread_mutex_lock(&lock);
    if (S.granted > 1) S.granted = 1;
    S.carry = 0;
    pthread_mutex_unlock(&lock);
}

void host_input(HostInput *out) {
    pthread_mutex_lock(&lock);
    *out = S.input;
    // A key pressed and released since the last read still counts as down once, so quick taps reach the game.
    for (int i = 0; i < HOST_KEY_COUNT; i++) {
        if (S.latched[i]) out->keys[i] = true;
        S.latched[i] = false;
    }
    for (int i = 0; i < 3; i++) {
        if (S.mouseLatched[i]) out->mouse[i] = true;
        S.mouseLatched[i] = false;
    }
    S.input.wheel = 0;
    pthread_mutex_unlock(&lock);
}

bool host_message_box(const char *text) {
    pthread_mutex_lock(&lock);
    free(S.message);
    S.message = strdup(text);
    // A test host waiting for the next frame hears about the box instead.
    pthread_cond_broadcast(&cond);
    while (S.message && !S.stopRequested) pthread_cond_wait(&cond, &lock);
    free(S.message);
    S.message = NULL;
    bool go = !S.stopRequested;
    // The game didn't run while the box was up, so it picks up at its own pace rather than with frames owed.
    if (S.granted > 1) S.granted = 1;
    S.carry = 0;
    pthread_mutex_unlock(&lock);
    return go;
}

void host_set_problem(const char *problem) {
    pthread_mutex_lock(&lock);
    snprintf(S.problem, sizeof S.problem, "%s", problem);
    S.hasProblem = true;
    pthread_mutex_unlock(&lock);
    rgss_log("problem: %s", problem);
}

// MARK: vnrgss.h

int vnrgss_start(const VNRGSSLaunch *l) {
    vnrgss_stop();
    pthread_mutex_lock(&lock);
    // A game that didn't stop in time still owns the shared state and its launch strings.
    if (S.running) {
        snprintf(S.problem, sizeof S.problem, "The last game is still stopping. Try again in a moment.");
        S.hasProblem = true;
        pthread_mutex_unlock(&lock);
        return -1;
    }
    free_launch(&S.launch);
    S.launch.gameIni = copy_string(l->gameIni);
    S.launch.saveDir = copy_string(l->saveDir);
    S.launch.logPath = copy_string(l->logPath);
    S.launch.soundFont = copy_string(l->soundFont);
    S.launch.fallbackSoundFont = copy_string(l->fallbackSoundFont);
    S.launch.rtpCount = l->rtpCount > 0 ? l->rtpCount : 0;
    S.launch.rtps = calloc((size_t)S.launch.rtpCount + 1, sizeof(char *));
    for (int i = 0; i < S.launch.rtpCount; i++) S.launch.rtps[i] = copy_string(l->rtps[i]);
    S.hasProblem = false;
    S.problem[0] = 0;
    S.ended = S.stopRequested = S.paused = false;
    S.granted = 1;
    S.carry = 0;
    S.frameRate = 60;
    S.speed = 1;
    // Results the last game's host never read.
    while (requests) {
        RubyRequest *r = requests;
        requests = r->next;
        free(r->code);
        free(r->result);
        free(r);
    }
    memset(&S.input, 0, sizeof S.input);
    memset(S.latched, 0, sizeof S.latched);
    memset(S.mouseLatched, 0, sizeof S.mouseLatched);
    pthread_mutex_unlock(&lock);

    pthread_mutex_lock(&logLock);
    if (logFile) fclose(logFile);
    logFile = l->logPath ? fopen(l->logPath, "w") : NULL;
    pthread_mutex_unlock(&logLock);

    if (!l->gameIni) {
        host_set_problem("No Game.ini was given.");
        return -1;
    }
    audio_start(l->soundFont, l->fallbackSoundFont);

    pthread_mutex_lock(&lock);
    if (!threadMade) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        // Ruby's interpreter recurses deeply; game scripts with big data need more than a thread's default stack.
        pthread_attr_setstacksize(&attr, 64u << 20);
        int made = pthread_create(&thread, &attr, game_thread, NULL);
        pthread_attr_destroy(&attr);
        if (made != 0) {
            pthread_mutex_unlock(&lock);
            host_set_problem("The game thread could not start.");
            return -1;
        }
        threadMade = true;
    }
    S.startPending = true;
    S.running = true;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
    return 0;
}

int vnrgss_step(double seconds) {
    pthread_mutex_lock(&lock);
    if (S.ended || !S.running) {
        pthread_mutex_unlock(&lock);
        return 1;
    }
    if (!S.paused && !S.message) {
        if (seconds < 0) seconds = 0;
        if (seconds > 0.25) seconds = 0.25;
        // Rounded rather than truncated: display refreshes jitter around 1/60 s, and truncating would grant no frame on
        // one refresh and two on the next, so one of the two would never be shown. The carry stays within half a frame.
        S.carry += seconds * S.frameRate * S.speed;
        int frames = (int)(S.carry + 0.5);
        S.carry -= frames;
        S.granted += frames;
        // A game that falls behind runs at its own pace rather than racing to catch up.
        if (S.granted > 2 * S.speed) S.granted = 2 * S.speed;
        if (frames > 0) pthread_cond_broadcast(&cond);
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

const uint8_t *vnrgss_frame(int *width, int *height, bool *changed) {
    pthread_mutex_lock(&lock);
    bool fresh = S.frame && S.serial != shownSerial;
    if (fresh) {
        if (shownW != S.frameW || shownH != S.frameH) {
            free(shown);
            shown = malloc((size_t)S.frameW * S.frameH * sizeof(Pixel));
            shownW = S.frameW;
            shownH = S.frameH;
        }
        if (shown) memcpy(shown, S.frame, (size_t)S.frameW * S.frameH * sizeof(Pixel));
        shownSerial = S.serial;
    }
    pthread_mutex_unlock(&lock);
    *width = shownW;
    *height = shownH;
    *changed = fresh;
    return (const uint8_t *)shown;
}

void vnrgss_audio_render(float *out, int frames) { audio_render(out, frames); }

int vnrgss_frame_rate(void) {
    pthread_mutex_lock(&lock);
    int rate = S.frameRate;
    pthread_mutex_unlock(&lock);
    return rate;
}

void vnrgss_key(unsigned code, bool down) {
    if (code >= HOST_KEY_COUNT) return;
    pthread_mutex_lock(&lock);
    S.input.keys[code] = down;
    if (down) S.latched[code] = true;
    pthread_mutex_unlock(&lock);
}

void vnrgss_mouse_move(float x, float y) {
    pthread_mutex_lock(&lock);
    S.input.mouseX = x;
    S.input.mouseY = y;
    pthread_mutex_unlock(&lock);
}

void vnrgss_mouse_button(int button, bool down) {
    if (button < 0 || button > 2) return;
    pthread_mutex_lock(&lock);
    S.input.mouse[button] = down;
    if (down) S.mouseLatched[button] = true;
    pthread_mutex_unlock(&lock);
}

void vnrgss_wheel(float notches) {
    pthread_mutex_lock(&lock);
    S.input.wheel += notches;
    pthread_mutex_unlock(&lock);
}

void vnrgss_set_paused(bool paused) {
    pthread_mutex_lock(&lock);
    S.paused = paused;
    S.carry = 0;
    pthread_mutex_unlock(&lock);
    audio_set_paused(paused);
}

void vnrgss_set_speed(int speed) {
    pthread_mutex_lock(&lock);
    S.speed = speed < 1 ? 1 : speed;
    S.carry = 0;
    pthread_mutex_unlock(&lock);
}

long vnrgss_run_ruby(const char *ruby) {
    pthread_mutex_lock(&lock);
    if (!S.running || S.ended || !ruby) {
        pthread_mutex_unlock(&lock);
        return 0;
    }
    RubyRequest *request = calloc(1, sizeof *request);
    request->id = ++lastRequest;
    request->code = strdup(ruby);
    RubyRequest **end = &requests;
    while (*end) end = &(*end)->next;
    *end = request;
    // A game waiting for its next frame runs it now.
    pthread_cond_broadcast(&cond);
    long id = request->id;
    pthread_mutex_unlock(&lock);
    return id;
}

char *vnrgss_ruby_result(long request, bool *failed) {
    pthread_mutex_lock(&lock);
    char *result = NULL;
    bool found = false;
    for (RubyRequest **link = &requests; *link; link = &(*link)->next) {
        RubyRequest *r = *link;
        if (r->id != request) continue;
        found = true;
        if (r->done) {
            result = r->result;
            if (failed) *failed = r->failed;
            *link = r->next;
            free(r);
        }
        break;
    }
    // Starting the next game drops the last one's requests, so a waiter still gets an answer.
    if (!found && request > 0 && request <= lastRequest) {
        result = strdup("The game ended.");
        if (failed) *failed = true;
    }
    pthread_mutex_unlock(&lock);
    return result;
}

void vnrgss_stop(void) {
    pthread_mutex_lock(&lock);
    if (S.running) {
        S.stopRequested = true;
        pthread_cond_broadcast(&cond);
        // The game unwinds at its next Graphics.update. A script stuck in a loop that never draws can't be stopped;
        // after this long the host moves on and the next start waits for it.
        struct timespec deadline;
        clock_gettime(CLOCK_REALTIME, &deadline);
        deadline.tv_sec += 5;
        while (S.running) {
            if (pthread_cond_timedwait(&cond, &lock, &deadline) == ETIMEDOUT) break;
        }
        if (S.running) rgss_log("the game did not stop within 5 seconds");
    }
    pthread_mutex_unlock(&lock);
    audio_stop();
    pthread_mutex_lock(&logLock);
    if (logFile) fclose(logFile);
    logFile = NULL;
    pthread_mutex_unlock(&logLock);
}

const char *vnrgss_problem(void) { return S.hasProblem ? S.problem : NULL; }

const char *vnrgss_message(void) {
    pthread_mutex_lock(&lock);
    const char *text = S.message;
    pthread_mutex_unlock(&lock);
    return text;
}

void vnrgss_dismiss_message(void) {
    pthread_mutex_lock(&lock);
    free(S.message);
    S.message = NULL;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
}

// MARK: Test host

void host_test_eval(const char *code) {
    pthread_mutex_lock(&lock);
    free(S.pendingEval);
    S.pendingEval = strdup(code);
    pthread_mutex_unlock(&lock);
}

char *host_take_eval(void) {
    pthread_mutex_lock(&lock);
    char *code = S.pendingEval;
    S.pendingEval = NULL;
    pthread_mutex_unlock(&lock);
    return code;
}

bool host_test_step(int timeoutMs) {
    pthread_mutex_lock(&lock);
    if (S.ended || !S.running) {
        pthread_mutex_unlock(&lock);
        return false;
    }
    uint64_t before = S.serial;
    S.granted = 1;
    pthread_cond_broadcast(&cond);
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += timeoutMs / 1000;
    deadline.tv_nsec += (long)(timeoutMs % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    while (S.serial == before && S.running && !S.ended && !S.message) {
        if (pthread_cond_timedwait(&cond, &lock, &deadline) == ETIMEDOUT) break;
    }
    bool alive = S.running && !S.ended;
    pthread_mutex_unlock(&lock);
    return alive;
}
