// Ogg Vorbis decoding for the Artemis engine's music, sound effects, and voices, which Apple's audio frameworks don't
// read. The decoder is stb_vorbis (stb_vorbis.c beside this header, MIT or public domain), whose own declaration of
// this function is in its source rather than a header.
#ifndef ARTEMIS_VORBIS_H
#define ARTEMIS_VORBIS_H

// Decodes a whole Ogg Vorbis file in `data`. On success returns the number of samples per channel and sets `*output`
// to interleaved 16-bit samples, which the caller frees with free(). Returns -1 for data that isn't Ogg Vorbis.
int stb_vorbis_decode_memory(const unsigned char *data, int len, int *channels, int *sample_rate, short **output);

#endif
