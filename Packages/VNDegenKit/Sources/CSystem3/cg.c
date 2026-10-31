// CG decoders: PMS (8 and 16 bit), VSP (16 colours, planar) and BMP.
//
// PMS follows the layout in GARbro's ImagePMS.cs (MIT): "PM", u16 version, u16 header size,
// u8 bits per pixel, then at 0x0a a u16 palette bank mask, at 0x10 x, y, width and height as u32,
// at 0x20 the pixel data offset and at 0x24 the palette (8 bit) or alpha (16 bit) offset.
// Each row is coded with byte commands: below 0xf8 a literal, 0xff and 0xfe copy from one and two
// rows up, 0xfd a run, 0xfc an alternating pair run, 0xf8 to 0xfb escape a literal (8 bit).
//
// VSP was worked out from the game files and checked against the original engine's screens; see
// docs/system3-cleanroom.md. Header: u16 x0 and x1 in 8-pixel columns, u16 y0 and y1, u8 at 8,
// u8 palette bank at 9, sixteen palette entries of three 4-bit levels from 0x0a, data at 0x3a.
// The data is column by column, 8 pixels wide; each column holds four bit planes, each plane a run
// of bytes down the column coded with: 0 copy from the previous column, 1 run, 2 alternating
// pair run, 3 to 5 copy plane 0 to 2 of this column (inverted after a 6), 7 escaped literal,
// 8 and up a literal.
#include "s3.h"

void s3_image_free(S3Image *img) {
    free(img->pixels8);
    free(img->pixels16);
    free(img->alpha);
    memset(img, 0, sizeof *img);
}

typedef struct Rd { const u8 *p, *end; bool bad; } Rd;
static int rd8(Rd *r) {
    if (r->p >= r->end) { r->bad = true; return 0; }
    return *r->p++;
}
static int rd16(Rd *r) { int a = rd8(r); return a | rd8(r) << 8; }

static void pms8(Rd *r, u8 *out, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w && !r->bad;) {
            u8 *d = out + y * w + x;
            int c = rd8(r), n = 1;
            if (c < 0xf8) {
                *d = (u8)c;
            } else if (c == 0xff || c == 0xfe) {
                n = rd8(r) + 3;
                int back = (c == 0xff ? 1 : 2) * w;
                if (x + n > w) n = w - x;
                if (y * w + x - back < 0) { r->bad = true; break; }
                for (int i = 0; i < n; i++) d[i] = d[i - back];
            } else if (c == 0xfd) {
                n = rd8(r) + 4;
                int v = rd8(r);
                if (x + n > w) n = w - x;
                memset(d, v, (size_t)n);
            } else if (c == 0xfc) {
                n = (rd8(r) + 3) * 2;
                int a = rd8(r), b = rd8(r);
                if (x + n > w) n = w - x;
                for (int i = 0; i < n; i++) d[i] = (u8)(i & 1 ? b : a);
            } else {
                *d = (u8)rd8(r);
            }
            x += n;
        }
    }
}

static u16 pms16_expand(int a, int b) {
    int hi = ((a & 0xe0) << 8) | ((a & 0x18) << 6) | ((a & 7) << 2);
    int lo = ((b & 0xc0) << 5) | ((b & 0x3c) << 3) | (b & 3);
    return (u16)(hi | lo);
}

static void pms16(Rd *r, u16 *out, int w, int h) {
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w && !r->bad;) {
            int at = y * w + x;
            u16 *d = out + at;
            int c = rd8(r), n = 1;
            if (c < 0xf8) {
                *d = (u16)(c | rd8(r) << 8);
            } else if (c == 0xf8) {
                *d = (u16)rd16(r);
            } else if (c == 0xf9) {
                n = rd8(r) + 1;
                int a = rd8(r);
                if (x + n > w) n = w - x;
                for (int i = 0; i < n; i++) d[i] = pms16_expand(a, rd8(r));
            } else if (c == 0xfa || c == 0xfb) {
                int src = at - w + (c == 0xfa ? 1 : -1);
                if (src < 0) { r->bad = true; break; }
                *d = out[src];
            } else if (c == 0xfc) {
                n = (rd8(r) + 2) * 2;
                u16 a = (u16)rd16(r), b = (u16)rd16(r);
                if (x + n > w) n = w - x;
                for (int i = 0; i < n; i++) d[i] = i & 1 ? b : a;
            } else if (c == 0xfd) {
                n = rd8(r) + 3;
                u16 v = (u16)rd16(r);
                if (x + n > w) n = w - x;
                for (int i = 0; i < n; i++) d[i] = v;
            } else {
                n = rd8(r) + 2;
                int back = (c == 0xfe ? 2 : 1) * w;
                if (x + n > w) n = w - x;
                if (at - back < 0) { r->bad = true; break; }
                for (int i = 0; i < n; i++) d[i] = d[i - back];
            }
            x += n;
        }
    }
}

static bool decode_pms(const u8 *d, size_t size, S3Image *img) {
    if (size < 0x30) return false;
    img->bpp = d[6];
    img->palMask = s3_le16(d + 0x0a);
    img->x = (int)s3_le32(d + 0x10);
    img->y = (int)s3_le32(d + 0x14);
    img->w = (int)s3_le32(d + 0x18);
    img->h = (int)s3_le32(d + 0x1c);
    u32 dataOff = s3_le32(d + 0x20), auxOff = s3_le32(d + 0x24);
    if (img->w <= 0 || img->h <= 0 || img->w > 8192 || img->h > 8192 || dataOff >= size) return false;
    Rd r = { d + dataOff, d + size, false };
    if (img->bpp == 8) {
        if ((u64)auxOff + 768 > size) return false;
        for (int i = 0; i < 256; i++)
            for (int c = 0; c < 3; c++) img->pal[i][c] = d[auxOff + i * 3 + c];
        img->hasPal = true;
        img->pixels8 = s3_alloc((size_t)img->w * img->h);
        pms8(&r, img->pixels8, img->w, img->h);
    } else if (img->bpp == 16) {
        img->pixels16 = s3_alloc((size_t)img->w * img->h * 2);
        pms16(&r, img->pixels16, img->w, img->h);
        if (auxOff && auxOff < size) {
            Rd ra = { d + auxOff, d + size, false };
            img->alpha = s3_alloc((size_t)img->w * img->h);
            pms8(&ra, img->alpha, img->w, img->h);
        }
    } else {
        return false;
    }
    return true;
}

// The 256-colour form of VSP, flagged by 1 at byte 8: a box in pixels, ends included, the banks it keeps at 0x0a, the
// palette at 0x20, and rows coded as PMS rows from 0x320. Read from the System 3.0 games' CGs.
static bool decode_vsp256(const u8 *d, size_t size, S3Image *img) {
    if (size < 0x320) return false;
    int x0 = s3_le16(d), y0 = s3_le16(d + 2), x1 = s3_le16(d + 4), y1 = s3_le16(d + 6);
    if (x1 < x0 || y1 < y0 || x1 > 4096 || y1 > 4096) return false;
    img->bpp = 8;
    img->x = x0; img->y = y0;
    img->w = x1 - x0 + 1; img->h = y1 - y0 + 1;
    img->hasPal = true;
    // Bits set at 0x0a mark the 16-colour banks the CG leaves alone; it sets the others. Read the other way, the
    // CGs of one scene overwrite each other's colours.
    img->palMask = (u16)~s3_le16(d + 0x0a);
    for (int i = 0; i < 256; i++) for (int c = 0; c < 3; c++) img->pal[i][c] = d[0x20 + i * 3 + c];
    img->pixels8 = s3_alloc((size_t)img->w * img->h);
    Rd r = { d + 0x320, d + size, false };
    pms8(&r, img->pixels8, img->w, img->h);
    return true;
}

static bool decode_vsp(const u8 *d, size_t size, S3Image *img) {
    if (size < 0x3a) return false;
    if (d[8] == 1) return decode_vsp256(d, size, img);
    int x0 = s3_le16(d), y0 = s3_le16(d + 2), x1 = s3_le16(d + 4), y1 = s3_le16(d + 6);
    if (x1 <= x0 || y1 <= y0 || x1 > 400 || y1 > 2048) return false;
    int cols = x1 - x0, h = y1 - y0;
    img->bpp = 8;
    img->x = x0 * 8;
    img->y = y0;
    img->w = cols * 8;
    img->h = h;
    img->palBank = d[9];
    img->vsp = true;
    img->hasPal = true;
    img->palMask = (u16)(1u << (img->palBank & 15));
    for (int i = 0; i < 16; i++) {
        // Each entry stores blue, red, green levels from 0 to 15.
        const u8 *e = d + 0x0a + i * 3;
        int idx = (img->palBank & 15) * 16 + i;
        img->pal[idx][0] = (u8)((e[1] & 15) * 17);
        img->pal[idx][1] = (u8)((e[2] & 15) * 17);
        img->pal[idx][2] = (u8)((e[0] & 15) * 17);
    }
    img->pixels8 = s3_alloc((size_t)img->w * h);
    u8 *prev = s3_alloc((size_t)h * 4), *cur = s3_alloc((size_t)h * 4);
    Rd r = { d + 0x3a, d + size, false };
    int base = (img->palBank & 15) * 16;
    for (int col = 0; col < cols && !r.bad; col++) {
        for (int pl = 0; pl < 4 && !r.bad; pl++) {
            u8 *c = cur + pl * h, *pv = prev + pl * h;
            bool invert = false;
            for (int y = 0; y < h && !r.bad;) {
                int op = rd8(&r);
                if (op >= 8) { c[y++] = (u8)op; continue; }
                if (op == 7) { c[y++] = (u8)rd8(&r); continue; }
                if (op == 6) { invert = true; continue; }
                int n = rd8(&r) + 1;
                if (op == 0) {
                    for (int i = 0; i < n && y < h; i++, y++) c[y] = pv[y];
                } else if (op == 1) {
                    int v = rd8(&r);
                    for (int i = 0; i < n && y < h; i++, y++) c[y] = (u8)v;
                } else if (op == 2) {
                    int a = rd8(&r), b = rd8(&r);
                    for (int i = 0; i < n && y < h; i++) { c[y++] = (u8)a; if (y < h) c[y++] = (u8)b; }
                } else {
                    const u8 *src = cur + (op - 3) * h;
                    for (int i = 0; i < n && y < h; i++, y++) c[y] = invert ? (u8)~src[y] : src[y];
                    invert = false;
                }
            }
        }
        for (int y = 0; y < h; y++) {
            u8 b0 = cur[y], b1 = cur[h + y], b2 = cur[2 * h + y], b3 = cur[3 * h + y];
            u8 *o = img->pixels8 + y * img->w + col * 8;
            for (int bit = 0; bit < 8; bit++) {
                int s = 7 - bit;
                o[bit] = (u8)(base + (((b0 >> s) & 1) | ((b1 >> s) & 1) << 1 | ((b2 >> s) & 1) << 2 | ((b3 >> s) & 1) << 3));
            }
        }
        u8 *t = prev; prev = cur; cur = t;
    }
    free(prev);
    free(cur);
    return true;
}

static bool decode_bmp(const u8 *d, size_t size, S3Image *img) {
    if (size < 54 || d[0] != 'B' || d[1] != 'M') return false;
    u32 off = s3_le32(d + 10), hs = s3_le32(d + 14);
    int w = (int)s3_le32(d + 18), h = (int)s3_le32(d + 22);
    int bpp = s3_le16(d + 28);
    bool flip = h > 0;
    if (h < 0) h = -h;
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || s3_le32(d + 30) != 0) return false;
    img->w = w; img->h = h;
    size_t stride = (((size_t)w * bpp + 31) / 32) * 4;
    if (bpp != 8 && bpp != 24 && bpp != 32) return false;
    if (off > size || stride * (size_t)h > size - off) return false;
    if (bpp == 8) {
        if (hs > size || (size_t)14 + hs + 1024 > size) return false;
        img->bpp = 8;
        img->hasPal = true;
        img->palMask = 0xffff;
        const u8 *pal = d + 14 + hs;
        for (int i = 0; i < 256; i++) { img->pal[i][0] = pal[i * 4 + 2]; img->pal[i][1] = pal[i * 4 + 1]; img->pal[i][2] = pal[i * 4]; }
        img->pixels8 = s3_alloc((size_t)w * h);
        for (int y = 0; y < h; y++)
            memcpy(img->pixels8 + y * w, d + off + stride * (flip ? h - 1 - y : y), (size_t)w);
    } else if (bpp == 24 || bpp == 32) {
        img->bpp = 16;
        img->pixels16 = s3_alloc((size_t)w * h * 2);
        int bytes = bpp / 8;
        for (int y = 0; y < h; y++) {
            const u8 *row = d + off + stride * (flip ? h - 1 - y : y);
            for (int x = 0; x < w; x++) {
                const u8 *px = row + x * bytes;
                img->pixels16[y * w + x] = (u16)((px[2] >> 3) << 11 | (px[1] >> 2) << 5 | px[0] >> 3);
            }
        }
    } else {
        return false;
    }
    return true;
}

bool s3_cg_decode(const u8 *data, size_t size, S3Image *img) {
    memset(img, 0, sizeof *img);
    img->transparent = -1;
    bool ok;
    if (size >= 2 && data[0] == 'P' && data[1] == 'M') ok = decode_pms(data, size, img);
    else if (size >= 2 && data[0] == 'B' && data[1] == 'M') ok = decode_bmp(data, size, img);
    else ok = decode_vsp(data, size, img);
    if (!ok) s3_image_free(img);
    return ok;
}
