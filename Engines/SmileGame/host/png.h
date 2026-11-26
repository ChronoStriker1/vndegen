#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t png_crc_table[256];

static uint32_t png_crc(const uint8_t *b, size_t n, uint32_t c) {
    if (!png_crc_table[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t v = i;
            for (int k = 0; k < 8; k++) v = v & 1 ? 0xedb88320u ^ (v >> 1) : v >> 1;
            png_crc_table[i] = v;
        }
    for (size_t i = 0; i < n; i++) c = png_crc_table[(c ^ b[i]) & 0xff] ^ (c >> 8);
    return c;
}

static void png_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void png_chunk(FILE *f, const char *type, const uint8_t *data, size_t n) {
    uint8_t h[8];
    png_be32(h, (uint32_t)n);
    memcpy(h + 4, type, 4);
    fwrite(h, 1, 8, f);
    if (n) fwrite(data, 1, n, f);
    uint32_t c = png_crc((const uint8_t *)type, 4, 0xffffffffu);
    c = png_crc(data, n, c) ^ 0xffffffffu;
    uint8_t t[4];
    png_be32(t, c);
    fwrite(t, 1, 4, f);
}

static int png_write(const char *path, const uint32_t *px, int w, int h) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13];
    png_be32(ihdr, (uint32_t)w);
    png_be32(ihdr + 4, (uint32_t)h);
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = ihdr[11] = ihdr[12] = 0;
    png_chunk(f, "IHDR", ihdr, 13);
    size_t rowBytes = (size_t)w * 4 + 1, raw = rowBytes * (size_t)h;
    size_t blocks = (raw + 65534) / 65535;
    uint8_t *z = malloc(2 + raw + blocks * 5 + 4);
    uint8_t *rows = malloc(raw);
    for (int y = 0; y < h; y++) {
        rows[(size_t)y * rowBytes] = 0;
        memcpy(rows + (size_t)y * rowBytes + 1, px + (size_t)y * (size_t)w, (size_t)w * 4);
    }
    size_t o = 0;
    z[o++] = 0x78;
    z[o++] = 0x01;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw; i++) {
        a = (a + rows[i]) % 65521;
        b = (b + a) % 65521;
    }
    for (size_t off = 0; off < raw; off += 65535) {
        size_t n = raw - off < 65535 ? raw - off : 65535;
        z[o++] = off + n >= raw ? 1 : 0;
        z[o++] = (uint8_t)(n & 255);
        z[o++] = (uint8_t)(n >> 8);
        z[o++] = (uint8_t)(~n & 255);
        z[o++] = (uint8_t)(~n >> 8 & 255);
        memcpy(z + o, rows + off, n);
        o += n;
    }
    png_be32(z + o, b << 16 | a);
    o += 4;
    png_chunk(f, "IDAT", z, o);
    png_chunk(f, "IEND", NULL, 0);
    free(z);
    free(rows);
    fclose(f);
    return 1;
}
