// Ogg Vorbis decoding for the Artemis engine's music, sound effects, and voices, which Apple's audio frameworks don't
// read. artemis_vorbis.c builds stb_vorbis under names of its own.
#ifndef ARTEMIS_VORBIS_H
#define ARTEMIS_VORBIS_H

// Decodes a whole Ogg Vorbis file in `data`. On success returns the number of samples per channel and sets `*output`
// to interleaved 16-bit samples, which the caller frees with free(). Returns -1 for data that isn't Ogg Vorbis.
int artemis_vorbis_decode_memory(const unsigned char *data, int len, int *channels, int *sample_rate, short **output);

#endif
