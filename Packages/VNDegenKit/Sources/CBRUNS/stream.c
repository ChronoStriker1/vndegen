#include "br.h"

struct Stream {
    int num, loop;
    u32 start;
    int frames;
    Image **img;
    int *ms;
};

Stream *stream_open(BR *e, int num, int loop, u32 now) {
    Stream *s = br_alloc(sizeof *s);
    s->num = num;
    s->loop = loop;
    s->start = now;
    return s;
}

Image *stream_frame(BR *e, Stream *s, u32 now) { return NULL; }

void stream_free(Stream *s) { free(s); }
