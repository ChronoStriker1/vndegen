#include "th.h"

Image *image_new(int cw, int ch, int x, int y, int w, int h) {
    Image *im = th_alloc(sizeof *im);
    im->cw = cw;
    im->ch = ch;
    im->x = x;
    im->y = y;
    im->w = w > 0 ? w : 0;
    im->h = h > 0 ? h : 0;
    im->px = th_alloc(sizeof(u32) * (size_t)(im->w ? im->w : 1) * (size_t)(im->h ? im->h : 1));
    return im;
}

void image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im);
}

Image *image_copy(const Image *im) {
    Image *c = image_new(im->cw, im->ch, im->x, im->y, im->w, im->h);
    memcpy(c->px, im->px, sizeof(u32) * (size_t)im->w * (size_t)im->h);
    c->opaque = im->opaque;
    return c;
}

typedef struct Bits {
    const u8 *p, *end;
    u32 cur;
    int left;
} Bits;

static int bit(Bits *b) {
    if (!b->left) {
        if (b->p >= b->end) return 0;
        b->cur = *b->p++;
        b->left = 8;
    }
    int v = b->cur & 1;
    b->cur >>= 1;
    b->left--;
    return v;
}

static u8 *huffman(const u8 *src, size_t avail, u32 *outSize) {
    if (avail < 8 + 1024) return NULL;
    u32 unpacked = rd32(src);
    u32 freq[512] = {0};
    int left[512] = {0}, right[512] = {0};
    int list[512], n = 0;
    for (int i = 0; i < 256; i++) {
        freq[i] = rd32(src + 8 + i * 4);
        int k = 0;
        while (k < n && freq[list[k]] <= freq[i]) k++;
        memmove(list + k + 1, list + k, sizeof(int) * (size_t)(n - k));
        list[k] = i;
        n++;
    }
    int next = 256;
    while (n > 1) {
        int l = list[0], r = list[1];
        memmove(list, list + 2, sizeof(int) * (size_t)(n - 2));
        n -= 2;
        freq[next] = freq[l] + freq[r];
        left[next] = l;
        right[next] = r;
        int k = 0;
        while (k < n && freq[list[k]] <= freq[next]) k++;
        memmove(list + k + 1, list + k, sizeof(int) * (size_t)(n - k));
        list[k] = next;
        n++;
        next++;
    }
    int root = list[0];
    u8 *out = th_alloc(unpacked + 1);
    Bits b = {src + 8 + 1024, src + avail, 0, 0};
    for (u32 d = 0; d < unpacked; d++) {
        int node = root;
        while (node > 0xff) node = bit(&b) ? right[node] : left[node];
        out[d] = (u8)node;
    }
    *outSize = unpacked;
    return out;
}

static void rle(const u8 *src, size_t size, u8 *dst, size_t n) {
    size_t s = 0, d = 0;
    while (s < size && d < n) {
        u8 c = src[s++];
        if (c > 0x7f) {
            int k = c & 0x7f;
            u8 v = s < size ? src[s++] : 0;
            while (k-- && d < n) dst[d++] = v;
        } else {
            for (int k = 0; k < c && s < size && d < n; k++) dst[d++] = src[s++];
        }
    }
}

static void lz(const u8 *src, size_t size, u8 *dst, size_t n) {
    if (size < 12) return;
    u8 special = src[8];
    size_t s = 12, d = 0;
    while (d < n && s < size) {
        u8 b = src[s++];
        if (b != special) { dst[d++] = b; continue; }
        if (s >= size) break;
        u8 off = src[s++];
        if (off == special) { dst[d++] = off; continue; }
        if (s >= size) break;
        u8 count = src[s++];
        if (off > special) off--;
        for (int k = 0; k < count && d < n; k++, d++) dst[d] = d >= off ? dst[d - off] : 0;
    }
}

Image *grd_decode(const u8 *data, size_t size) {
    if (size < 32) return NULL;
    int kind = data[0], pack = data[1];
    if (pack == 0xf0 && size == 32) return image_new(1, 1, 0, 0, 0, 0);
    if ((kind != 1 && kind != 2) || (pack != 1 && pack != 0xa1 && pack != 0xa2)) return NULL;
    int cw = rd16(data + 2), ch = rd16(data + 4), bpp = rd16(data + 6);
    int l = rd16(data + 8), r = rd16(data + 10), t = rd16(data + 12), b = rd16(data + 14);
    u32 sizes[4] = {rd32(data + 16), rd32(data + 20), rd32(data + 24), rd32(data + 28)};
    if (bpp != 24 && bpp != 32) return NULL;
    int w = r - l, h = b - t;
    if (w <= 0 || h <= 0) return image_new(cw, ch, 0, 0, 0, 0);
    Image *im = image_new(cw, ch, l, ch - b, w, h);
    size_t n = (size_t)w * (size_t)h;
    u8 *plane = th_alloc(n);
    bool alpha = bpp == 32 && sizes[0] > 0;
    for (size_t i = 0; i < n; i++) im->px[i] = 0xff000000u;
    size_t pos = 32;
    const int shifts[4] = {24, 0, 8, 16};
    for (int c = 0; c < 4; c++) {
        if (c == 0 && !alpha) {
            pos += sizes[0];
            continue;
        }
        if (pos + sizes[c] > size) break;
        memset(plane, 0, n);
        if (pack == 1) {
            rle(data + pos, sizes[c], plane, n);
        } else {
            u32 hn = 0;
            u8 *hd = huffman(data + pos, sizes[c], &hn);
            if (hd) {
                if (pack == 0xa2) lz(hd, hn, plane, n);
                else rle(hd, hn, plane, n);
                free(hd);
            }
        }
        pos += sizes[c];
        u32 mask = ~(0xffu << shifts[c]);
        for (int y = 0; y < h; y++) {
            const u8 *srow = plane + (size_t)(h - 1 - y) * (size_t)w;
            u32 *drow = im->px + (size_t)y * (size_t)w;
            for (int x = 0; x < w; x++) drow[x] = (drow[x] & mask) | (u32)srow[x] << shifts[c];
        }
    }
    free(plane);
    im->opaque = !alpha && im->x == 0 && im->y == 0 && w == cw && h == ch;
    return im;
}
