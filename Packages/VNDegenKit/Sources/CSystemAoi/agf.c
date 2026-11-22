#include "sa.h"

static void copy_overlapped(u8 *px, size_t dst, size_t src, size_t n) {
    for (size_t i = 0; i < n; i++) px[dst + i] = px[src + i];
}

Image *sa_agf_decode(const u8 *d, size_t size) {
    if (size < 0x80 || d[0] != 'A' || d[1] != 'G' || d[2] != 'F') return NULL;
    u32 ver = rd32(d + 4);
    if (ver != 1 && ver != 2) return NULL;
    u32 w = rd32(d + 0x1c), h = rd32(d + 0x20);
    u32 off = rd32(d + (ver == 1 ? 0x0c : 0x10));
    if (!w || !h || w > 8192 || h > 8192 || off >= size) return NULL;
    Image *im = sa_image_new((int)w, (int)h);
    u8 *px = (u8 *)im->px;
    size_t total = (size_t)w * h * 4, dst = 0, p = off;
    while (dst < total) {
        if (p + 4 > size) break;
        u32 op = rd32(d + p);
        p += 4;
        size_t count = op >> 8;
        switch (op & 0xff) {
        case 1:
            count *= 4;
            if (p + count > size || dst + count > total) goto fail;
            memcpy(px + dst, d + p, count);
            p += count;
            break;
        case 2:
            count *= 4;
            if (p + 4 > size || dst + count > total || count < 4) goto fail;
            memcpy(px + dst, d + p, 4);
            p += 4;
            copy_overlapped(px, dst + 4, dst, count - 4);
            break;
        case 3: {
            size_t chunk = (count >> 8) * 4;
            count = (count & 0xff) * chunk;
            if (!chunk || p + chunk > size || dst + count > total || count < chunk) goto fail;
            memcpy(px + dst, d + p, chunk);
            p += chunk;
            copy_overlapped(px, dst + chunk, dst, count - chunk);
            break;
        }
        case 4: {
            size_t back = (count & 0xfff) * 4;
            count = (count >> 12) * 4;
            if (back > dst || dst + count > total) goto fail;
            copy_overlapped(px, dst, dst - back, count);
            break;
        }
        case 5: {
            size_t n = (count >> 8) & 0xff;
            p += (n - n / 4) * 4;
            count = n * 4;
            if (dst + count > total) goto fail;
            break;
        }
        default:
            goto fail;
        }
        dst += count;
    }
    for (size_t i = 0; i < total; i += 4) {
        u8 b = px[i];
        px[i] = px[i + 2];
        px[i + 2] = b;
    }
    return im;
fail:
    sa_image_free(im);
    return NULL;
}
