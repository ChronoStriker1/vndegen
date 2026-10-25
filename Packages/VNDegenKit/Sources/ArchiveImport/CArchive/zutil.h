/* The parts of zlib's private zutil.h that the vendored infback9 files use. The system zlib does not export them. */
#ifndef ZUTIL_H
#define ZUTIL_H

#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define local static
#define zmemcpy memcpy
#define ZALLOC(strm, items, size) (*((strm)->zalloc))((strm)->opaque, (items), (size))
#define ZFREE(strm, addr) (*((strm)->zfree))((strm)->opaque, (voidpf)(addr))
#define Assert(cond, msg)
#define Trace(x)
#define Tracev(x)
#define Tracevv(x)
#define Tracec(c, x)
#define Tracecv(c, x)

static inline __attribute__((unused)) voidpf zcalloc(voidpf opaque, unsigned items, unsigned size) {
    (void)opaque;
    return calloc(items, size);
}

static inline __attribute__((unused)) void zcfree(voidpf opaque, voidpf ptr) {
    (void)opaque;
    free(ptr);
}

#endif
