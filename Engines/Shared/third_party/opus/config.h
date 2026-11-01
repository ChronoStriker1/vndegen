/* The build settings libopus's configure would write for VNDegen's targets: the float decoder, with C99
 * variable-length arrays for scratch space (VNDegen). */
#ifndef VNDEGEN_OPUS_CONFIG_H
#define VNDEGEN_OPUS_CONFIG_H
#define OPUS_BUILD 1
#define VAR_ARRAYS 1
#define HAVE_LRINTF 1
#define HAVE_LRINT 1
#endif
