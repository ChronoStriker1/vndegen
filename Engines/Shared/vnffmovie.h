// vnffmovie: a movie decoder over FFmpeg, for the formats vnvideo doesn't play, such as WMV in ASF. It's compiled into
// the KiriKiri framework, the one place the app links FFmpeg, and exported from there, so an engine finds these five
// functions at run time with dlsym and needs no FFmpeg of its own. Nothing here touches the KiriKiri engine. A decode
// thread runs ahead of the host by a few frames; the host asks for the frame due at its clock and pulls the sound.
#ifndef VNFFMOVIE_H
#define VNFFMOVIE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The KiriKiri framework builds with hidden visibility, so these opt back in to be found with dlsym.
#if defined(__GNUC__) || defined(__clang__)
#define VNFFMOVIE_API __attribute__((visibility("default")))
#else
#define VNFFMOVIE_API
#endif

typedef struct vnffmovie vnffmovie;

// Reads up to `length` bytes of the movie at `offset` into `buffer`. Returns the bytes read, 0 at the end, or a
// negative number on an error. It runs on the decode thread.
typedef int64_t (*vnffmovie_read_fn)(void *user, uint64_t offset, void *buffer, size_t length);

typedef struct vnffmovie_info {
    int width, height;
    double fps;
    double duration;  // seconds, 0 when the container doesn't say
    // 0 when the movie has no sound the decoder plays. The sound comes out as interleaved floats.
    int audio_rate, audio_channels;
} vnffmovie_info;

// Opens a movie, fills `info` and starts decoding. `user` must stay valid until vnffmovie_close. A looping movie starts
// over at its end with its clock running on. Returns NULL, with a reason in `error`, for a movie it can't play.
VNFFMOVIE_API vnffmovie *vnffmovie_open(vnffmovie_read_fn read, void *user, uint64_t size, int loop, vnffmovie_info *info,
                          char *error, size_t error_size);

// The newest frame due at `time` seconds, as width * height RGBA pixels, or NULL until the first is decoded. `changed`
// says whether it differs from the frame the last call returned, and `ended` whether the movie has finished: decoding
// done, its last frame due and its sound handed out. A looping movie never ends. The pixels stay valid until the next
// call. Sound that has fallen behind `time` is dropped.
VNFFMOVIE_API const uint8_t *vnffmovie_frame(vnffmovie *movie, double time, int *changed, int *ended);

// Copies up to `frames` frames of sound. Returns the frames copied, fewer when the decoder hasn't caught up, and the
// host plays silence for the rest: the sound's clock moves on by all `frames` either way. Safe to call from an audio
// thread.
VNFFMOVIE_API size_t vnffmovie_audio(vnffmovie *movie, float *out, size_t frames);

// Decodes again from `time` seconds. Frames and sound before it are dropped.
VNFFMOVIE_API void vnffmovie_seek(vnffmovie *movie, double time);

VNFFMOVIE_API void vnffmovie_close(vnffmovie *movie);

#ifdef __cplusplus
}
#endif

#endif
