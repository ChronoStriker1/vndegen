// vnvideo: MPEG-1 and Ogg Theora movies for the native engines. See vnvideo.h.
//
// One decode thread per movie decodes in stream order, video and sound interleaved by time, into a ring of RGBA
// frames and a ring of float samples. It waits while either ring is full, so it runs a few frames ahead of the host.
#define _POSIX_C_SOURCE 200809L
#include "vnvideo.h"

#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "third_party/vnvideo_libs.h"

#include <ogg/ogg.h>
#include <opus.h>
#include <theora/theoradec.h>

enum {
    FRAME_SLOTS = 4,
    // Opus packets hold at most 120 ms, 5760 samples a channel at 48 kHz.
    OPUS_MAX_FRAMES = 5760,
    AUDIO_SECONDS = 4,
    READ_CHUNK = 64 * 1024,
};

typedef enum { FORMAT_MPEG, FORMAT_OGG } format_t;

typedef struct ogg_state {
    ogg_sync_state sync;
    ogg_stream_state video_stream, audio_stream;
    int has_video_stream, has_audio_stream;
    th_info info;
    th_comment comment;
    th_setup_info *setup;
    th_dec_ctx *decoder;
    // A data packet read while the headers were being parsed, decoded first.
    ogg_packet first_video;
    unsigned char *first_video_data;
    OpusDecoder *opus;
    int opus_skip;
    int eof;
} ogg_state;

struct vnvideo {
    vnvideo_read_fn read;
    void *user;
    uint64_t size, position;
    int loop;
    format_t format;
    vnvideo_info info;

    plm_t *plm;
    ogg_state ogg;

    // Decode thread side: time decoded so far in this pass, and the clock offset of earlier passes of a loop.
    double video_time, audio_time, base;
    // The end of this pass's last frame, repeated frames included, and whether its sound stream has ended, after which
    // the sound is padded with silence up to the picture so the sound clock never stops short of it.
    double pass_end;
    int sound_ended;
    uint64_t audio_frames_decoded;
    float *pcm;

    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t wake;
    int stop, done;
    char error[160];

    // Frame ring: `count` decoded frames from `head`, each with the time it is due.
    uint8_t *slots[FRAME_SLOTS];
    double due[FRAME_SLOTS];
    int head, count;
    uint8_t *current;
    double current_due;
    // When the last frame of the last pass ends, set as decoding finishes, and whether the picture stream has ended.
    double end_time;
    int video_ended;
    int has_current;
    // Nothing is handed out until the frame ring first fills, the picture stream ends, the sound buffer is half full,
    // or decoding ends, so playback starts with frames in hand, even when the decoder starts slowly while the engine
    // loads a screen.
    int started;

    // Sound ring of interleaved floats.
    float *audio;
    size_t audio_capacity, audio_read, audio_fill;
    uint64_t audio_frames_out;
};

// MARK: Reading

static int64_t read_at(vnvideo *v, void *buffer, size_t length) {
    if (v->position >= v->size) return 0;
    if (length > v->size - v->position) length = (size_t)(v->size - v->position);
    int64_t got = v->read(v->user, v->position, buffer, length);
    if (got > 0) v->position += (uint64_t)got;
    return got;
}

// pl_mpeg's buffer, filled from the read callback as its own file callbacks fill it from a FILE.
static void buffer_load(plm_buffer_t *buffer, void *user) {
    vnvideo *v = user;
    if (buffer->discard_read_bytes) plm_buffer_discard_read_bytes(buffer);
    int64_t got = read_at(v, buffer->bytes + buffer->length, buffer->capacity - buffer->length);
    if (got > 0) buffer->length += (size_t)got;
    else buffer->has_ended = 1;
}

static void buffer_seek(plm_buffer_t *buffer, size_t offset, void *user) {
    (void)buffer;
    ((vnvideo *)user)->position = offset;
}

static size_t buffer_tell(plm_buffer_t *buffer, void *user) {
    (void)buffer;
    return (size_t)((vnvideo *)user)->position;
}

// MARK: Pixels

// Rec. 601 video range YCbCr 4:2:0 to RGBA, the colors MPEG-1 and Theora both use. `x0` and `y0` place the picture
// inside the planes, as Theora's picture region does.
static void yuv420_to_rgba(const uint8_t *y_plane, int y_stride, const uint8_t *cb_plane, int cb_stride,
                           const uint8_t *cr_plane, int cr_stride, int x0, int y0, int width, int height,
                           uint8_t *out) {
    for (int row = 0; row < height; row++) {
        const uint8_t *ys = y_plane + (size_t)(y0 + row) * y_stride + x0;
        const uint8_t *cbs = cb_plane + (size_t)((y0 + row) >> 1) * cb_stride;
        const uint8_t *crs = cr_plane + (size_t)((y0 + row) >> 1) * cr_stride;
        uint8_t *o = out + (size_t)row * width * 4;
        for (int x = 0; x < width; x++) {
            int c = (x0 + x) >> 1;
            int luma = 298 * (ys[x] - 16), cb = cbs[c] - 128, cr = crs[c] - 128;
            int r = (luma + 409 * cr + 128) >> 8;
            int g = (luma - 100 * cb - 208 * cr + 128) >> 8;
            int b = (luma + 516 * cb + 128) >> 8;
            o[0] = (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
            o[1] = (uint8_t)(g < 0 ? 0 : g > 255 ? 255 : g);
            o[2] = (uint8_t)(b < 0 ? 0 : b > 255 ? 255 : b);
            o[3] = 255;
            o += 4;
        }
    }
}

// MARK: Handing over

static void fail(vnvideo *v, const char *reason) {
    pthread_mutex_lock(&v->lock);
    if (!v->error[0]) snprintf(v->error, sizeof v->error, "%s", reason);
    pthread_mutex_unlock(&v->lock);
}

// The slot the next frame decodes into, after waiting for room. NULL when the movie is closing.
static uint8_t *free_slot(vnvideo *v) {
    pthread_mutex_lock(&v->lock);
    while (!v->stop && v->count == FRAME_SLOTS) pthread_cond_wait(&v->wake, &v->lock);
    uint8_t *slot = v->stop ? NULL : v->slots[(v->head + v->count) % FRAME_SLOTS];
    pthread_mutex_unlock(&v->lock);
    return slot;
}

static void push_frame(vnvideo *v, double due) {
    pthread_mutex_lock(&v->lock);
    v->due[(v->head + v->count) % FRAME_SLOTS] = due;
    v->count++;
    pthread_mutex_unlock(&v->lock);
}

static void end_video(vnvideo *v) {
    v->video_time = INFINITY;
    pthread_mutex_lock(&v->lock);
    v->video_ended = 1;
    pthread_cond_broadcast(&v->wake);
    pthread_mutex_unlock(&v->lock);
}

// Queues interleaved samples, waiting for room. Returns 0 when the movie is closing.
static int push_audio(vnvideo *v, const float *samples, size_t frames) {
    size_t channels = (size_t)v->info.audio_channels, total = frames * channels;
    pthread_mutex_lock(&v->lock);
    while (!v->stop && v->audio_capacity - v->audio_fill < total) pthread_cond_wait(&v->wake, &v->lock);
    if (v->stop) {
        pthread_mutex_unlock(&v->lock);
        return 0;
    }
    size_t write = (v->audio_read + v->audio_fill) % v->audio_capacity;
    for (size_t i = 0; i < total; i++) v->audio[(write + i) % v->audio_capacity] = samples[i];
    v->audio_fill += total;
    pthread_mutex_unlock(&v->lock);
    v->audio_frames_decoded += frames;
    v->audio_time = (double)v->audio_frames_decoded / v->info.audio_rate - v->base;
    return 1;
}

// Once the sound stream has ended, pads it with a frame's worth of silence at a time, through the end of the
// picture's last frame, then stops it. Returns 0 when the movie is closing.
static int pad_sound(vnvideo *v) {
    size_t frames = (size_t)ceil(v->info.audio_rate / v->info.fps);
    if (!isfinite(v->video_time)) {
        // The picture has ended: pad only up to the end of its last frame.
        double left = v->pass_end - v->audio_time;
        if (left <= 0) {
            v->audio_time = INFINITY;
            return 1;
        }
        size_t needed = (size_t)ceil(left * v->info.audio_rate);
        if (needed < frames) frames = needed;
    }
    if (frames > OPUS_MAX_FRAMES) frames = OPUS_MAX_FRAMES;
    memset(v->pcm, 0, frames * (size_t)v->info.audio_channels * sizeof(float));
    return push_audio(v, v->pcm, frames);
}

// MARK: MPEG-1

static int mpeg_open(vnvideo *v) {
    plm_buffer_t *buffer = plm_buffer_create_with_callbacks(buffer_load, buffer_seek, buffer_tell, (size_t)v->size, v);
    v->plm = plm_create_with_buffer(buffer, 1);
    // The system header's stream counts. plm_probe, which scans packets instead, misses the video stream behind a
    // callback buffer.
    if (!v->plm || !plm_has_headers(v->plm) || plm_get_num_video_streams(v->plm) == 0) return 0;
    plm_set_loop(v->plm, 0);
    v->info.width = plm_get_width(v->plm);
    v->info.height = plm_get_height(v->plm);
    v->info.fps = plm_get_framerate(v->plm);
    if (plm_get_num_audio_streams(v->plm) > 0) {
        plm_set_audio_enabled(v->plm, 1);
        plm_set_audio_stream(v->plm, 0);
        v->info.audio_rate = plm_get_samplerate(v->plm);
        v->info.audio_channels = 2;
    } else {
        plm_set_audio_enabled(v->plm, 0);
    }
    return v->info.width > 0 && v->info.height > 0;
}

// Decodes the next frame or block of sound, whichever is due first. Returns 0 at the end of the stream.
static int mpeg_step(vnvideo *v) {
    if (isfinite(v->audio_time) && v->audio_time <= v->video_time) {
        if (v->sound_ended) return pad_sound(v);
        plm_samples_t *samples = plm_decode_audio(v->plm);
        if (samples) return push_audio(v, samples->interleaved, samples->count);
        v->sound_ended = 1;
        return 1;
    }
    if (!isfinite(v->video_time)) return 0;
    uint8_t *slot = free_slot(v);
    if (!slot) return 0;
    plm_frame_t *frame = plm_decode_video(v->plm);
    if (!frame) {
        end_video(v);
        return isfinite(v->audio_time);
    }
    yuv420_to_rgba(frame->y.data, (int)frame->y.width, frame->cb.data, (int)frame->cb.width, frame->cr.data,
                   (int)frame->cr.width, 0, 0, v->info.width, v->info.height, slot);
    v->video_time = frame->time;
    v->pass_end = frame->time + 1.0 / v->info.fps;
    push_frame(v, v->base + frame->time);
    return 1;
}

static void mpeg_rewind(vnvideo *v) {
    plm_rewind(v->plm);
}

// MARK: Ogg Theora and Opus

// Feeds the next page to the stream it belongs to. Returns 0 at the end of the file.
static int ogg_next_page(vnvideo *v) {
    ogg_state *s = &v->ogg;
    ogg_page page;
    while (ogg_sync_pageout(&s->sync, &page) != 1) {
        if (s->eof) return 0;
        char *buffer = ogg_sync_buffer(&s->sync, READ_CHUNK);
        int64_t got = read_at(v, buffer, READ_CHUNK);
        if (got <= 0) {
            s->eof = 1;
            continue;
        }
        ogg_sync_wrote(&s->sync, (long)got);
    }
    int serial = ogg_page_serialno(&page);
    if (s->has_video_stream && serial == s->video_stream.serialno) ogg_stream_pagein(&s->video_stream, &page);
    else if (s->has_audio_stream && serial == s->audio_stream.serialno) ogg_stream_pagein(&s->audio_stream, &page);
    return 1;
}

static int ogg_next_packet(vnvideo *v, ogg_stream_state *stream, ogg_packet *packet) {
    while (ogg_stream_packetout(stream, packet) != 1)
        if (!ogg_next_page(v)) return 0;
    return 1;
}

static void ogg_close_streams(ogg_state *s) {
    if (s->decoder) th_decode_free(s->decoder);
    if (s->setup) th_setup_free(s->setup);
    th_comment_clear(&s->comment);
    th_info_clear(&s->info);
    if (s->opus) opus_decoder_destroy(s->opus);
    if (s->has_video_stream) ogg_stream_clear(&s->video_stream);
    if (s->has_audio_stream) ogg_stream_clear(&s->audio_stream);
    free(s->first_video_data);
    ogg_sync_clear(&s->sync);
    memset(s, 0, sizeof *s);
}

// Reads the beginning-of-stream pages, keeps the first Theora and the first Opus stream, and parses their headers.
// The movie's format goes to `info`, so a loop that reopens the stream leaves the published format alone.
static int ogg_open(vnvideo *v, vnvideo_info *info, char *error, size_t error_size) {
    ogg_state *s = &v->ogg;
    memset(s, 0, sizeof *s);
    ogg_sync_init(&s->sync);
    th_info_init(&s->info);
    th_comment_init(&s->comment);
    int theora_headers = 0, opus_headers = 0, channels = 0;
    // Beginning-of-stream pages come first in an Ogg file, one per stream.
    for (;;) {
        ogg_page page;
        if (ogg_sync_pageout(&s->sync, &page) != 1) {
            char *buffer = ogg_sync_buffer(&s->sync, READ_CHUNK);
            int64_t got = read_at(v, buffer, READ_CHUNK);
            if (got <= 0) break;
            ogg_sync_wrote(&s->sync, (long)got);
            continue;
        }
        if (!ogg_page_bos(&page)) {
            // A data page: give it to its stream and stop looking for new streams.
            int serial = ogg_page_serialno(&page);
            if (s->has_video_stream && serial == s->video_stream.serialno) ogg_stream_pagein(&s->video_stream, &page);
            if (s->has_audio_stream && serial == s->audio_stream.serialno) ogg_stream_pagein(&s->audio_stream, &page);
            break;
        }
        ogg_stream_state stream;
        ogg_stream_init(&stream, ogg_page_serialno(&page));
        ogg_stream_pagein(&stream, &page);
        ogg_packet packet;
        if (ogg_stream_packetout(&stream, &packet) != 1) {
            ogg_stream_clear(&stream);
            continue;
        }
        if (!s->has_video_stream && th_decode_headerin(&s->info, &s->comment, &s->setup, &packet) > 0) {
            s->video_stream = stream;
            s->has_video_stream = 1;
            theora_headers = 1;
        } else if (!s->has_audio_stream && packet.bytes >= 19 && !memcmp(packet.packet, "OpusHead", 8)) {
            channels = packet.packet[9];
            s->opus_skip = packet.packet[10] | packet.packet[11] << 8;
            s->audio_stream = stream;
            s->has_audio_stream = 1;
            opus_headers = 1;
        } else {
            // Vorbis sound and other streams aren't played.
            ogg_stream_clear(&stream);
        }
    }
    if (!s->has_video_stream) {
        snprintf(error, error_size, "no Theora video stream");
        return 0;
    }
    // Theora has three header packets. The packet after them is the first frame, kept for the decode thread.
    ogg_packet packet;
    while (theora_headers < 3) {
        if (!ogg_next_packet(v, &s->video_stream, &packet)) break;
        int result = th_decode_headerin(&s->info, &s->comment, &s->setup, &packet);
        if (result < 0) break;
        if (result == 0) {
            s->first_video_data = malloc((size_t)packet.bytes);
            if (!s->first_video_data) break;
            memcpy(s->first_video_data, packet.packet, (size_t)packet.bytes);
            s->first_video = packet;
            s->first_video.packet = s->first_video_data;
            theora_headers = 3;
            break;
        }
        theora_headers++;
    }
    if (theora_headers < 3 || !s->setup) {
        snprintf(error, error_size, "incomplete Theora headers");
        return 0;
    }
    if (s->info.pixel_fmt != TH_PF_420) {
        snprintf(error, error_size, "Theora pixel format %d isn't 4:2:0", s->info.pixel_fmt);
        return 0;
    }
    s->decoder = th_decode_alloc(&s->info, s->setup);
    if (!s->decoder) {
        snprintf(error, error_size, "the Theora decoder could not start");
        return 0;
    }
    info->width = (int)s->info.pic_width;
    info->height = (int)s->info.pic_height;
    info->fps = s->info.fps_denominator ? (double)s->info.fps_numerator / s->info.fps_denominator : 30;
    // Opus has a second header, its tags, which carries nothing the decoder needs.
    if (s->has_audio_stream && opus_headers == 1 && ogg_next_packet(v, &s->audio_stream, &packet)) {
        int status = 0;
        if (channels >= 1 && channels <= 2) s->opus = opus_decoder_create(48000, channels, &status);
        if (s->opus && status == OPUS_OK) {
            info->audio_rate = 48000;
            info->audio_channels = channels;
        }
    }
    return 1;
}

static int ogg_step(vnvideo *v) {
    ogg_state *s = &v->ogg;
    ogg_packet packet;
    if (isfinite(v->audio_time) && v->audio_time <= v->video_time) {
        if (v->sound_ended) return pad_sound(v);
        if (!ogg_next_packet(v, &s->audio_stream, &packet)) {
            v->sound_ended = 1;
            return 1;
        } else {
            int frames = opus_decode_float(s->opus, packet.packet, (opus_int32)packet.bytes, v->pcm, OPUS_MAX_FRAMES, 0);
            if (frames < 0) return 1;
            const float *start = v->pcm;
            // The encoder's pre-skip: samples at the start that are only there to prime the decoder.
            int skip = s->opus_skip < frames ? s->opus_skip : frames;
            s->opus_skip -= skip;
            start += (size_t)skip * (size_t)v->info.audio_channels;
            return frames == skip || push_audio(v, start, (size_t)(frames - skip));
        }
    }
    if (!isfinite(v->video_time)) return 0;
    if (s->first_video.packet) {
        packet = s->first_video;
        s->first_video.packet = NULL;
    } else if (!ogg_next_packet(v, &s->video_stream, &packet)) {
        end_video(v);
        return isfinite(v->audio_time);
    }
    double due = v->video_time;
    v->video_time += 1.0 / v->info.fps;
    v->pass_end = v->video_time;
    // A duplicate frame repeats the last picture, which the host keeps showing.
    if (th_decode_packetin(s->decoder, &packet, NULL) != 0) return 1;
    th_ycbcr_buffer planes;
    if (th_decode_ycbcr_out(s->decoder, planes) != 0) return 1;
    uint8_t *slot = free_slot(v);
    if (!slot) return 0;
    yuv420_to_rgba(planes[0].data, planes[0].stride, planes[1].data, planes[1].stride, planes[2].data, planes[2].stride,
                   (int)s->info.pic_x, (int)s->info.pic_y, v->info.width, v->info.height, slot);
    push_frame(v, v->base + due);
    return 1;
}

static int ogg_rewind(vnvideo *v) {
    ogg_close_streams(&v->ogg);
    v->position = 0;
    vnvideo_info info = {0};
    char error[64];
    return ogg_open(v, &info, error, sizeof error);
}

// MARK: Decode thread

// Each pass of a movie starts both clocks at 0. A movie without sound never waits for it.
static void reset_clocks(vnvideo *v) {
    v->video_time = 0;
    v->audio_time = v->info.audio_rate ? 0 : INFINITY;
    v->pass_end = 0;
    v->sound_ended = 0;
}

static void *decode_thread(void *argument) {
    vnvideo *v = argument;
    reset_clocks(v);
    for (;;) {
        int more = v->format == FORMAT_MPEG ? mpeg_step(v) : ogg_step(v);
        pthread_mutex_lock(&v->lock);
        int stop = v->stop;
        pthread_mutex_unlock(&v->lock);
        if (stop) break;
        if (more) continue;
        if (!v->loop) break;
        // Start over, with the clock running on from the end of this pass.
        double length = v->pass_end;
        if (v->info.audio_rate) {
            double sound = (double)v->audio_frames_decoded / v->info.audio_rate - v->base;
            if (sound > length) length = sound;
        }
        if (length <= 0) {
            fail(v, "a looping movie decoded nothing");
            break;
        }
        v->base += length;
        reset_clocks(v);
        if (v->format == FORMAT_MPEG) mpeg_rewind(v);
        else if (!ogg_rewind(v)) {
            fail(v, "a looping movie could not start over");
            break;
        }
    }
    pthread_mutex_lock(&v->lock);
    v->end_time = v->base + v->pass_end;
    v->done = 1;
    pthread_cond_broadcast(&v->wake);
    pthread_mutex_unlock(&v->lock);
    return NULL;
}

// MARK: Interface

static void destroy(vnvideo *v) {
    if (v->plm) plm_destroy(v->plm);
    if (v->format == FORMAT_OGG) ogg_close_streams(&v->ogg);
    for (int i = 0; i < FRAME_SLOTS; i++) free(v->slots[i]);
    free(v->current);
    free(v->audio);
    free(v->pcm);
    pthread_mutex_destroy(&v->lock);
    pthread_cond_destroy(&v->wake);
    free(v);
}

vnvideo *vnvideo_open(vnvideo_read_fn read, void *user, uint64_t size, int loop, char *error, size_t error_size) {
    char ignored[8];
    if (!error || !error_size) error = ignored, error_size = sizeof ignored;
    error[0] = 0;
    vnvideo *v = calloc(1, sizeof *v);
    if (!v) return NULL;
    v->read = read;
    v->user = user;
    v->size = size;
    v->loop = loop;
    pthread_mutex_init(&v->lock, NULL);
    pthread_cond_init(&v->wake, NULL);
    uint8_t magic[4] = {0};
    if (read_at(v, magic, 4) != 4) {
        snprintf(error, error_size, "the movie is empty");
        destroy(v);
        return NULL;
    }
    v->position = 0;
    int opened;
    if (!memcmp(magic, "OggS", 4)) {
        v->format = FORMAT_OGG;
        opened = ogg_open(v, &v->info, error, error_size);
    } else if (magic[0] == 0 && magic[1] == 0 && magic[2] == 1 && (magic[3] == 0xBA || magic[3] == 0xB3)) {
        v->format = FORMAT_MPEG;
        opened = mpeg_open(v);
        if (!opened) snprintf(error, error_size, "not an MPEG-1 movie pl_mpeg can read");
    } else {
        snprintf(error, error_size, "unknown movie format %02x%02x%02x%02x", magic[0], magic[1], magic[2], magic[3]);
        opened = 0;
    }
    size_t pixels = (size_t)v->info.width * (size_t)v->info.height * 4;
    if (opened) {
        for (int i = 0; i < FRAME_SLOTS && opened; i++) opened = (v->slots[i] = malloc(pixels)) != NULL;
        opened = opened && (v->current = malloc(pixels)) != NULL;
        if (v->info.audio_rate) {
            v->audio_capacity = (size_t)v->info.audio_rate * (size_t)v->info.audio_channels * AUDIO_SECONDS;
            opened = opened && (v->audio = malloc(v->audio_capacity * sizeof(float))) != NULL &&
                     (v->pcm = malloc((size_t)OPUS_MAX_FRAMES * 2 * sizeof(float))) != NULL;
        }
        if (!opened) snprintf(error, error_size, "out of memory");
    }
    if (opened && pthread_create(&v->thread, NULL, decode_thread, v) != 0) {
        snprintf(error, error_size, "the decode thread could not start");
        opened = 0;
    }
    if (!opened) {
        destroy(v);
        return NULL;
    }
    return v;
}

void vnvideo_get_info(const vnvideo *video, vnvideo_info *info) {
    *info = video->info;
}

// Whether playback may start. The caller holds the lock.
static int ready(vnvideo *v) {
    if (!v->started && (v->count == FRAME_SLOTS || v->done || v->video_ended ||
                        (v->audio_capacity && v->audio_fill * 2 >= v->audio_capacity)))
        v->started = 1;
    return v->started;
}

const uint8_t *vnvideo_frame(vnvideo *v, double time, int *changed) {
    int moved = 0;
    pthread_mutex_lock(&v->lock);
    while (ready(v) && v->count > 0 && (v->due[v->head] <= time || !v->has_current)) {
        // Swap the due frame's buffer with the shown one, so nothing is copied.
        uint8_t *shown = v->current;
        v->current = v->slots[v->head];
        v->slots[v->head] = shown;
        v->current_due = v->due[v->head];
        v->head = (v->head + 1) % FRAME_SLOTS;
        v->count--;
        v->has_current = 1;
        moved = 1;
    }
    if (moved) pthread_cond_broadcast(&v->wake);
    int has = v->has_current;
    pthread_mutex_unlock(&v->lock);
    if (changed) *changed = moved;
    return has ? v->current : NULL;
}

size_t vnvideo_read_audio(vnvideo *v, float *out, size_t frames) {
    if (!v->info.audio_rate) return 0;
    size_t channels = (size_t)v->info.audio_channels;
    pthread_mutex_lock(&v->lock);
    size_t available = ready(v) ? v->audio_fill / channels : 0;
    if (frames > available) frames = available;
    size_t total = frames * channels;
    for (size_t i = 0; i < total; i++) out[i] = v->audio[(v->audio_read + i) % v->audio_capacity];
    v->audio_read = (v->audio_read + total) % v->audio_capacity;
    v->audio_fill -= total;
    v->audio_frames_out += frames;
    if (frames) pthread_cond_broadcast(&v->wake);
    pthread_mutex_unlock(&v->lock);
    return frames;
}

double vnvideo_audio_time(vnvideo *v) {
    if (!v->info.audio_rate) return 0;
    pthread_mutex_lock(&v->lock);
    double time = (double)v->audio_frames_out / v->info.audio_rate;
    pthread_mutex_unlock(&v->lock);
    return time;
}

int vnvideo_ended(vnvideo *v, double time) {
    pthread_mutex_lock(&v->lock);
    int ended = v->done && v->count == 0 && v->audio_fill == 0 && time >= v->end_time;
    pthread_mutex_unlock(&v->lock);
    return ended;
}

const char *vnvideo_error(vnvideo *v) {
    pthread_mutex_lock(&v->lock);
    const char *error = v->error[0] ? v->error : NULL;
    pthread_mutex_unlock(&v->lock);
    return error;
}

void vnvideo_close(vnvideo *v) {
    if (!v) return;
    pthread_mutex_lock(&v->lock);
    v->stop = 1;
    pthread_cond_broadcast(&v->wake);
    pthread_mutex_unlock(&v->lock);
    pthread_join(v->thread, NULL);
    destroy(v);
}
