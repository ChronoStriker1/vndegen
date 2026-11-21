#include "pj.h"

static bool unpack(const u8 *src, size_t size, size_t *pos, u8 *out, size_t n, const int *tbl) {
    size_t dst = 0, p = *pos;
    while (dst < n) {
        if (p >= size) return false;
        int flag = src[p++];
        size_t count;
        if (!(flag & 0xf0)) {
            count = (size_t)flag;
            if (dst + count > n) count = n - dst;
            if (p + count > size) return false;
            memcpy(out + dst, src + p, count);
            p += count;
        } else {
            if (flag & 8) {
                if (p >= size) return false;
                count = (size_t)src[p++] + ((size_t)(flag & 7) << 8);
            } else
                count = (size_t)(flag & 7);
            if (dst + count > n) break;
            size_t back = (size_t)tbl[flag >> 4];
            if (back > dst) return false;
            for (size_t i = 0; i < count; i++) out[dst + i] = out[dst + i - back];
        }
        dst += count;
    }
    *pos = p;
    return true;
}

Image *pj_epa_decode(const u8 *d, size_t size) {
    if (size < 16 || d[0] != 'E' || d[1] != 'P') return NULL;
    int mode = d[3], type = d[4];
    int w = (int)rd32(d + 8), h = (int)rd32(d + 12);
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192) return NULL;
    size_t pos = mode == 2 ? 0x18 : 0x10;
    int ps = type == 0 ? 1 : type == 1 ? 3 : type == 2 ? 4 : type == 4 ? 1 : 0;
    if (!ps) return NULL;
    const u8 *pal = NULL;
    if (ps == 1) {
        if (pos + 768 > size) return NULL;
        pal = d + pos;
        pos += 768;
    }
    int tbl[16] = {0, 1, w, w + 1, 2, w - 1, w * 2, 3, (w + 1) * 2, w + 2, w * 2 + 1, w * 2 - 1, (w - 1) * 2, w - 2, w * 3, 4};
    size_t n = (size_t)w * (size_t)h;
    u8 *raw = pj_alloc(n * (size_t)ps);
    u8 *alpha = NULL;
    bool ok = unpack(d, size, &pos, raw, n * (size_t)ps, tbl);
    if (ok && type == 4) {
        alpha = pj_alloc(n);
        ok = unpack(d, size, &pos, alpha, n, tbl);
    }
    Image *im = NULL;
    if (ok) {
        im = pj_image_new(w, h);
        for (size_t i = 0; i < n; i++) {
            u32 r, g, b, a = 255;
            if (ps == 1) {
                const u8 *c = pal + raw[i] * 3;
                b = c[0];
                g = c[1];
                r = c[2];
                if (alpha) a = alpha[i];
            } else {
                b = raw[i];
                g = raw[n + i];
                r = raw[2 * n + i];
                if (ps == 4) a = raw[3 * n + i];
            }
            im->px[i] = a << 24 | b << 16 | g << 8 | r;
        }
    }
    free(raw);
    free(alpha);
    return im;
}
