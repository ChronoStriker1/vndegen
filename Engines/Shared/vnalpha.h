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
// Both read the WebM from an open descriptor, which stays open and keeps its file offset.
// 1 for VP9 with alpha, 0 for any other WebM, -1 when the file can't be read, with the reason in error.
int vnalpha_inspect(int source, char *error, int error_size);
// Converts VP9-alpha WebM with optional mono/stereo Opus into HEVC-alpha. Failure removes partial output.
int vnalpha_convert(int source, const char *destination, vnalpha_control *control, vnalpha_result *result,
                    char *error, int error_size);
#ifdef __cplusplus
}
#endif
#endif
