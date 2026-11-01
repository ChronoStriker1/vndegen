// vnvideo: the movie decoder native engines share. It plays MPEG-1 video with MPEG-1 Layer II sound (pl_mpeg), and
// Ogg Theora video with Opus sound or none (libogg, libtheora, libopus). A decode thread runs ahead of the host by a
// few frames. The host asks for the frame due at its clock and pulls the sound as its audio output needs it.
#ifndef VNVIDEO_H
#define VNVIDEO_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vnvideo vnvideo;

// Reads up to `length` bytes of the movie at `offset` into `buffer`. Returns the bytes read, 0 at the end, or a
// negative number on an error. It runs on the decode thread.
typedef int64_t (*vnvideo_read_fn)(void *user, uint64_t offset, void *buffer, size_t length);

typedef struct vnvideo_info {
    int width, height;
    double fps;
    // 0 when the movie has no sound the decoder plays.
    int audio_rate, audio_channels;
} vnvideo_info;

// Opens a movie and starts its decode thread. `user` must stay valid until vnvideo_close. A looping movie starts
// over at its end, with its clock running on. Returns NULL, with a reason in `error`, for a movie it can't play.
vnvideo *vnvideo_open(vnvideo_read_fn read, void *user, uint64_t size, int loop, char *error, size_t error_size);
void vnvideo_get_info(const vnvideo *video, vnvideo_info *info);

// The newest frame due at `time` seconds, as width * height RGBA pixels, or NULL until playback starts, once a few
// frames are decoded. Sound is held back until then too, so a movie's clock starts with frames in hand.
// `changed` says whether it differs from the frame the last call returned. The pixels stay valid until the next call.
const uint8_t *vnvideo_frame(vnvideo *video, double time, int *changed);

// Copies up to `frames` frames of interleaved float sound, at the info's rate and channels. Returns the frames
// copied, fewer when the decoder hasn't caught up. Safe to call from an audio thread.
size_t vnvideo_read_audio(vnvideo *video, float *out, size_t frames);
// The seconds of sound vnvideo_read_audio has handed out, the movie's clock when it has sound.
double vnvideo_audio_time(vnvideo *video);

// Whether the movie has ended at `time`: decoding finished, its last frame is due, and its sound was handed out.
// A looping movie never ends.
int vnvideo_ended(vnvideo *video, double time);
// Why decoding stopped early, or NULL. A movie that fails partway ends there.
const char *vnvideo_error(vnvideo *video);

void vnvideo_close(vnvideo *video);

#ifdef __cplusplus
}
#endif

#endif
