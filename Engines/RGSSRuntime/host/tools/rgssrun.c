// Runs an RGSS game headless, one frame at a time, pressing keys from a script and saving frames as PNG files.
//
//   rgssrun --ini Game.ini --save DIR [--rtp DIR]... [--frames N] [--log FILE] [--script FILE] [--shot N:FILE]...
//           [--audio FILE.raw] [--then ARGS...]
//
// A script has one command per line: "N tap KEY" presses a key at frame N and releases it two frames later,
// "N down KEY" and "N up KEY" hold and release it, "N shot FILE" saves frame N, "N eval CODE" runs the rest of the
// line as Ruby in the game. KEY is a libretro key code or a
// name: up, down, left, right, enter, esc, space, shift, or a single letter. Lines starting with # are comments.
// Everything after --then is a second game's arguments; it starts in the same process once the first has stopped, as
// the app starts one game after another.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "host.h"
#include "vnrgss.h"

typedef struct {
    long frame;
    int kind; // 0 down, 1 up, 2 shot, 3 eval
    unsigned key;
    char file[1024];
} Command;

static Command *commands;
static int commandCount, commandCapacity;

static void add(Command c) {
    if (commandCount == commandCapacity) {
        commandCapacity = commandCapacity ? commandCapacity * 2 : 64;
        commands = realloc(commands, sizeof(Command) * (size_t)commandCapacity);
    }
    commands[commandCount++] = c;
}

static unsigned key_named(const char *name) {
    static const struct { const char *name; unsigned code; } names[] = {
        {"up", 273}, {"down", 274}, {"right", 275}, {"left", 276}, {"enter", 13}, {"return", 13}, {"esc", 27},
        {"escape", 27}, {"space", 32}, {"shift", 304}, {"ctrl", 306}, {"alt", 308}};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) if (!strcasecmp(names[i].name, name)) return names[i].code;
    if (strlen(name) == 1) return (unsigned)(name[0] >= 'A' && name[0] <= 'Z' ? name[0] + 32 : name[0]);
    return (unsigned)atoi(name);
}

static void read_script(const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) {
        fprintf(stderr, "no script %s\n", path);
        exit(2);
    }
    char line[1024];
    while (fgets(line, sizeof line, file)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        long frame;
        char verb[32], arg[512];
        if (sscanf(line, "%ld %31s %511s", &frame, verb, arg) != 3) continue;
        Command c = {.frame = frame};
        if (!strcmp(verb, "eval")) {
            c.kind = 3;
            const char *code = strstr(line, "eval") + 5;
            snprintf(c.file, sizeof c.file, "%s", code);
            c.file[strcspn(c.file, "\n")] = 0;
            add(c);
        } else if (!strcmp(verb, "shot")) {
            c.kind = 2;
            snprintf(c.file, sizeof c.file, "%s", arg);
            add(c);
        } else if (!strcmp(verb, "tap")) {
            c.key = key_named(arg);
            add(c);
            c.frame = frame + 2;
            c.kind = 1;
            add(c);
        } else {
            c.key = key_named(arg);
            c.kind = !strcmp(verb, "up");
            add(c);
        }
    }
    fclose(file);
}

static void save_frame(const char *path) {
    int w, h;
    bool changed;
    const uint8_t *pixels = vnrgss_frame(&w, &h, &changed);
    if (!pixels) return;
    Image image = {w, h, (Pixel *)pixels};
    if (!image_write_png(&image, path)) fprintf(stderr, "could not write %s\n", path);
}

static int run(int argc, char **argv);

int main(int argc, char **argv) {
    int split = argc;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--then")) {
            split = i;
            break;
        }
    }
    int status = run(split, argv);
    if (split < argc) {
        commandCount = 0;
        argv[split] = argv[0];
        status |= run(argc - split, argv + split);
    }
    return status;
}

static int run(int argc, char **argv) {
    const char *ini = NULL, *save = NULL, *log = NULL, *audioPath = NULL;
    const char *rtps[4];
    int rtpCount = 0;
    long frames = 600;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (!strcmp(a, "--ini") && v) ini = argv[++i];
        else if (!strcmp(a, "--save") && v) save = argv[++i];
        else if (!strcmp(a, "--log") && v) log = argv[++i];
        else if (!strcmp(a, "--rtp") && v && rtpCount < 4) rtps[rtpCount++] = argv[++i];
        else if (!strcmp(a, "--frames") && v) frames = atol(argv[++i]);
        else if (!strcmp(a, "--script") && v) read_script(argv[++i]);
        else if (!strcmp(a, "--audio") && v) audioPath = argv[++i];
        else if (!strcmp(a, "--shot") && v) {
            Command c = {.kind = 2};
            sscanf(argv[++i], "%ld:%1023s", &c.frame, c.file);
            add(c);
        } else {
            fprintf(stderr, "unknown argument %s\n", a);
            return 2;
        }
    }
    if (!ini || !save) {
        fprintf(stderr, "usage: rgssrun --ini Game.ini --save DIR [options]\n");
        return 2;
    }
    const char *font = getenv("RGSS_SOUNDFONT");
    VNRGSSLaunch launch = {ini, rtps, rtpCount, save, log, font, NULL};
    if (vnrgss_start(&launch) != 0) {
        fprintf(stderr, "start failed: %s\n", vnrgss_problem() ? vnrgss_problem() : "?");
        return 1;
    }
    FILE *audio = audioPath ? fopen(audioPath, "wb") : NULL;
    long frame = 0;
    for (; frame < frames; frame++) {
        for (int i = 0; i < commandCount; i++) {
            if (commands[i].frame != frame) continue;
            if (commands[i].kind == 2) save_frame(commands[i].file);
            else if (commands[i].kind == 3) host_test_eval(commands[i].file);
            else vnrgss_key(commands[i].key, commands[i].kind == 0);
        }
        if (!host_test_step(20000)) break;
        // A 60 Hz frame's worth of sound, which also keeps the mixer's decoders exercised.
        float pcm[735 * 2];
        vnrgss_audio_render(pcm, 735);
        if (audio) fwrite(pcm, sizeof(float), 735 * 2, audio);
    }
    for (int i = 0; i < commandCount; i++) {
        if (commands[i].kind == 2 && commands[i].frame >= frame) save_frame(commands[i].file);
    }
    if (audio) fclose(audio);
    const char *problem = vnrgss_problem();
    printf("frames %ld%s%s\n", frame, problem ? ", problem: " : "", problem ? problem : "");
    vnrgss_stop();
    return problem ? 1 : 0;
}
