#ifndef VNALPHA_H
#define VNALPHA_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    int width, height, frames;
    double duration, seconds;
    uint64_t peak_bytes;
} vnalpha_result;
typedef struct vnalpha_control vnalpha_control;
vnalpha_control *vnalpha_control_create(void);
void vnalpha_control_cancel(vnalpha_control *control);
void vnalpha_control_destroy(vnalpha_control *control);
int vnalpha_is_alpha(const char *source);
// Converts VP9-alpha WebM with optional mono/stereo Opus into HEVC-alpha. Failure removes partial output.
int vnalpha_convert(const char *source, const char *destination, vnalpha_control *control, vnalpha_result *result,
                    char *error, int error_size);
#ifdef __cplusplus
}
#endif
#endif
