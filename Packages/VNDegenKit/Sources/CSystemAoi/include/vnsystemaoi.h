#ifndef VNSYSTEMAOI_H
#define VNSYSTEMAOI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VNSystemAoi VNSystemAoi;

typedef struct VNSystemAoiMovieInfo {
    int width, height;
    double fps;
    double duration;
    int audioRate, audioChannels;
} VNSystemAoiMovieInfo;

typedef int64_t (*VNSystemAoiMovieRead)(void *user, uint64_t offset, void *buffer, size_t length);

typedef struct VNSystemAoiMovies {
    void *(*open)(VNSystemAoiMovieRead read, void *user, uint64_t size, int loop, VNSystemAoiMovieInfo *info, char *error,
                  size_t errorSize);
    const uint8_t *(*frame)(void *movie, double time, int *changed, int *ended);
    size_t (*audio)(void *movie, float *out, size_t frames);
    void (*seek)(void *movie, double time);
    void (*close)(void *movie);
} VNSystemAoiMovies;

const VNSystemAoiMovies *vnsystemaoi_find_movies(void);

typedef struct VNSystemAoiConfig {
    const char *gameDir;
    const char *saveDir;
    const char *logPath;
    int audioRate;
    unsigned int seed;
    const VNSystemAoiMovies *movies;
} VNSystemAoiConfig;

VNSystemAoi *vnsystemaoi_open(const VNSystemAoiConfig *config, char *err, size_t errSize);
void vnsystemaoi_close(VNSystemAoi *sa);

bool vnsystemaoi_probe(const char *gameDir, char *title, size_t titleSize);

bool vnsystemaoi_frame(VNSystemAoi *sa, uint32_t elapsedMs);
const uint32_t *vnsystemaoi_pixels(VNSystemAoi *sa, int *width, int *height);
uint32_t vnsystemaoi_frame_serial(VNSystemAoi *sa);

enum { VNSYSTEMAOI_MOUSE_LEFT = 1, VNSYSTEMAOI_MOUSE_RIGHT = 2 };
void vnsystemaoi_mouse_move(VNSystemAoi *sa, int x, int y);
void vnsystemaoi_mouse_button(VNSystemAoi *sa, int button, bool down);
void vnsystemaoi_pointer_lift(VNSystemAoi *sa);
void vnsystemaoi_key(VNSystemAoi *sa, int virtualKey, bool down);
void vnsystemaoi_wheel(VNSystemAoi *sa, int notches);
void vnsystemaoi_text(VNSystemAoi *sa, const char *utf8);

void vnsystemaoi_audio(VNSystemAoi *sa, float *out, int frames);
void vnsystemaoi_set_paused(VNSystemAoi *sa, bool paused);
void vnsystemaoi_set_skip(VNSystemAoi *sa, bool on);
void vnsystemaoi_request_quit(VNSystemAoi *sa);

uint8_t *vnsystemaoi_unpack(const uint8_t *data, size_t size, size_t *outSize);
uint8_t *vnsystemaoi_pack(const uint8_t *data, size_t size, size_t *outSize);

const char *vnsystemaoi_title(VNSystemAoi *sa);
const char *vnsystemaoi_problem(VNSystemAoi *sa);

int vnsystemaoi_wall_variant(uint32_t seed, bool sharedKey, int x, int y, int side);

#ifdef __cplusplus
}
#endif

#endif
