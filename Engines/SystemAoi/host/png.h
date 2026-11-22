#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned crc_table[256];
static unsigned crc(const unsigned char *b, size_t n, unsigned c) {
    if (!crc_table[1])
        for (unsigned i = 0; i < 256; i++) {
            unsigned v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
    for (size_t i = 0; i < n; i++) c = crc_table[(c ^ b[i]) & 0xff] ^ (c >> 8);
    return c;
}

static void be32(unsigned char *p, unsigned v) {
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

static void chunk(FILE *f, const char *type, const unsigned char *data, size_t n) {
    unsigned char h[8];
    be32(h, (unsigned)n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    unsigned c = crc((const unsigned char *)type, 4, 0xffffffffu);
    c = crc(data, n, c) ^ 0xffffffffu;
    be32(h, c);
    fwrite(h, 1, 4, f);
}

static void write_png(const char *path, const uint32_t *src, int sw, int sh, int scale, int alpha) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    int w = sw / scale, h = sh / scale;
    fwrite("\x89PNG\r\n\x1a\n", 1, 8, f);
    unsigned char ihdr[13];
    be32(ihdr, w);
    be32(ihdr + 4, h);
    ihdr[8] = 8;
    ihdr[9] = alpha ? 6 : 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    chunk(f, "IHDR", ihdr, 13);
    int bpp = alpha ? 4 : 3;
    size_t row = (size_t)w * bpp + 1, raw = row * h;
    unsigned char *r = malloc(raw);
    for (int y = 0; y < h; y++) {
        r[y * row] = 0;
        for (int x = 0; x < w; x++) {
            uint32_t p = src[(size_t)y * scale * sw + (size_t)x * scale];
            r[y * row + 1 + x * bpp] = p & 0xff;
            r[y * row + 2 + x * bpp] = p >> 8 & 0xff;
            r[y * row + 3 + x * bpp] = p >> 16 & 0xff;
            if (alpha) r[y * row + 4 + x * bpp] = p >> 24;
        }
    }
    size_t blocks = (raw + 65534) / 65535;
    unsigned char *z = malloc(raw + blocks * 5 + 6);
    size_t zn = 0;
    z[zn++] = 0x78;
    z[zn++] = 0x01;
    unsigned a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) {
        a = (a + r[i]) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t off = 0; off < raw; off += 65535) {
        size_t len = raw - off < 65535 ? raw - off : 65535;
        z[zn++] = off + len >= raw;
        z[zn++] = len & 0xff;
        z[zn++] = len >> 8;
        z[zn++] = ~len & 0xff;
        z[zn++] = (~len >> 8) & 0xff;
        memcpy(z + zn, r + off, len);
        zn += len;
    }
    be32(z + zn, b << 16 | a);
    zn += 4;
    chunk(f, "IDAT", z, zn);
    chunk(f, "IEND", NULL, 0);
    free(z);
    free(r);
    fclose(f);
}
