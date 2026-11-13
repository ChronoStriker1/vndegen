#include "yr.h"

u8 *yr_snappy_decode(const u8 *src, size_t n, size_t *outLen) {
    size_t o = 0, len = 0;
    for (int shift = 0; o < n && shift < 35; shift += 7) {
        u8 b = src[o++];
        len |= (size_t)(b & 0x7f) << shift;
        if (!(b & 0x80)) break;
    }
    if (len > n * 22) return NULL;
    u8 *out = yr_alloc(len ? len : 1);
    size_t w = 0;
    while (o < n && w < len) {
        u8 tag = src[o++];
        size_t l, off = 0;
        switch (tag & 3) {
        case 0:
            l = (tag >> 2) + 1;
            if (l > 60) {
                int extra = (int)l - 60;
                l = 0;
                for (int i = 0; i < extra && o < n; i++) l |= (size_t)src[o++] << (8 * i);
                l++;
            }
            if (o + l > n || w + l > len) { free(out); return NULL; }
            memcpy(out + w, src + o, l);
            o += l;
            w += l;
            continue;
        case 1:
            if (o >= n) { free(out); return NULL; }
            l = 4 + ((tag >> 2) & 7);
            off = (size_t)(tag >> 5) << 8 | src[o++];
            break;
        case 2:
            if (o + 2 > n) { free(out); return NULL; }
            l = (tag >> 2) + 1;
            off = yr_le16(src + o);
            o += 2;
            break;
        default:
            if (o + 4 > n) { free(out); return NULL; }
            l = (tag >> 2) + 1;
            off = yr_le32(src + o);
            o += 4;
            break;
        }
        if (!off || off > w || w + l > len) { free(out); return NULL; }
        for (size_t i = 0; i < l; i++, w++) out[w] = out[w - off];
    }
    if (w != len) { free(out); return NULL; }
    *outLen = len;
    return out;
}

static void lit(u8 *dst, size_t *o, const u8 *s, size_t n) {
    while (n) {
        size_t k = n > 65536 ? 65536 : n;
        if (k <= 60) dst[(*o)++] = (u8)((k - 1) << 2);
        else if (k <= 256) { dst[(*o)++] = 60 << 2; dst[(*o)++] = (u8)(k - 1); }
        else { dst[(*o)++] = 61 << 2; dst[(*o)++] = (u8)(k - 1); dst[(*o)++] = (u8)((k - 1) >> 8); }
        memcpy(dst + *o, s, k);
        *o += k;
        s += k;
        n -= k;
    }
}

static void copy(u8 *dst, size_t *o, size_t off, size_t l) {
    while (l) {
        size_t k = l > 64 ? 64 : l;
        if (l > 64 && l - 64 < 4) k = 60;
        dst[(*o)++] = (u8)(((k - 1) << 2) | 2);
        dst[(*o)++] = (u8)off;
        dst[(*o)++] = (u8)(off >> 8);
        l -= k;
    }
}

u8 *yr_snappy_encode(const u8 *src, size_t n, size_t *outLen) {
    u8 *dst = yr_alloc(32 + n + n / 6);
    size_t o = 0, v = n;
    do {
        u8 b = (u8)(v & 0x7f);
        v >>= 7;
        dst[o++] = v ? (u8)(b | 0x80) : b;
    } while (v);
    enum { HB = 14 };
    int32_t *table = yr_alloc(sizeof(int32_t) << HB);
    for (size_t i = 0; i < ((size_t)1 << HB); i++) table[i] = -1;
    size_t i = 0, start = 0;
    while (i + 4 <= n) {
        u32 x = yr_le32(src + i);
        u32 h = (x * 0x1e35a7bdu) >> (32 - HB);
        int32_t c = table[h];
        table[h] = (int32_t)i;
        if (c >= 0 && i - (size_t)c <= 0xffff && yr_le32(src + c) == x) {
            size_t l = 4;
            while (i + l < n && src[c + l] == src[i + l]) l++;
            lit(dst, &o, src + start, i - start);
            copy(dst, &o, i - (size_t)c, l);
            i += l;
            start = i;
        } else i++;
    }
    lit(dst, &o, src + start, n - start);
    free(table);
    *outLen = o;
    return dst;
}
