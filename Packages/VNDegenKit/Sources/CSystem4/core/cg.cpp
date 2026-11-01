#include "cg.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <zlib.h>

#if __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#else
#include <webp/decode.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "../third_party/stb/stb_image.h"

namespace s4 {
namespace {

uint32_t rd32(const uint8_t *p) { uint32_t v; std::memcpy(&v, p, 4); return v; }

bool inflateTo(const uint8_t *src, size_t n, std::vector<uint8_t> &out, size_t expect) {
    out.resize(expect);
    uLongf len = expect;
    return uncompress(out.data(), &len, src, n) == Z_OK && len == expect;
}

// QNT planes are stored as 2x2 blocks, (x,y) (x,y+1) (x+1,y) (x+1,y+1). The first pixel is stored as is; every
// other byte is the prediction minus the pixel, where the prediction is the left pixel on the first row, the one
// above in the first column, else their average. Checked against the SDK's own CG compressor.
void unpackPlane(const uint8_t *src, int w, int h, uint8_t *dst, int stride) {
    int W = (w + 1) & ~1, H = (h + 1) & ~1;
    std::vector<uint8_t> plane(size_t(W) * H);
    size_t k = 0;
    for (int y = 0; y < H; y += 2)
        for (int x = 0; x < W; x += 2) {
            plane[size_t(y) * W + x] = src[k++];
            plane[size_t(y + 1) * W + x] = src[k++];
            plane[size_t(y) * W + x + 1] = src[k++];
            plane[size_t(y + 1) * W + x + 1] = src[k++];
        }
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            int pred;
            if (y == 0 && x == 0) { dst[0] = plane[0]; continue; }
            if (y == 0) pred = dst[(x - 1) * stride];
            else if (x == 0) pred = dst[size_t(y - 1) * w * stride];
            else pred = (dst[(size_t(y - 1) * w + x) * stride] + dst[(size_t(y) * w + x - 1) * stride] ) >> 1;
            dst[(size_t(y) * w + x) * stride] = uint8_t(pred - plane[size_t(y) * W + x]);
        }
}

// 8-bit PMS rows: a byte below 0xF8 is a pixel; 0xF8 escapes the next byte as a pixel; 0xFF n and 0xFE n copy
// n + 3 pixels from one or two rows above; 0xFD n v repeats v n + 4 times; 0xFC n a b repeats the pair n + 3
// times. Worked out by encoding test bitmaps with the SDK's CG compressor.
bool unpackPMS8(const uint8_t *s, size_t n, int w, int h, std::vector<uint8_t> &out) {
    out.assign(size_t(w) * h, 0);
    size_t i = 0, total = out.size(), o = 0;
    while (o < total) {
        if (i >= n) return false;
        uint8_t c = s[i++];
        if (c < 0xF8) { out[o++] = c; continue; }
        if (i >= n) return false;
        switch (c) {
        case 0xF8: out[o++] = s[i++]; break;
        case 0xFF: case 0xFE: {
            size_t cnt = size_t(s[i++]) + 3, back = size_t(w) * (c == 0xFF ? 1 : 2);
            if (o < back || o + cnt > total) return false;
            for (size_t k = 0; k < cnt; k++, o++) out[o] = out[o - back];
            break;
        }
        case 0xFD: {
            if (n - i < 2) return false;
            size_t cnt = size_t(s[i++]) + 4;
            uint8_t v = s[i++];
            if (o + cnt > total) return false;
            std::fill(out.begin() + o, out.begin() + o + cnt, v);
            o += cnt;
            break;
        }
        case 0xFC: {
            if (n - i < 3) return false;
            size_t cnt = size_t(s[i++]) + 3;
            uint8_t a = s[i++], b = s[i++];
            if (o + cnt * 2 > total) return false;
            for (size_t k = 0; k < cnt; k++) { out[o++] = a; out[o++] = b; }
            break;
        }
        default:
            return false;  // not used by the SDK's encoder or the test games
        }
    }
    return true;
}

// AJP's JPEG and its PMS mask each have their first 16 bytes XORed with this key, found by comparing many
// files with the fixed JFIF and PMS headers they must start with.
const uint8_t kAjpKey[16] = {0x5D, 0x91, 0xAE, 0x87, 0x4A, 0x56, 0x41, 0xCD, 0x83, 0xEC, 0x4C, 0x92, 0xB5, 0xCB, 0x16, 0x34};

}  // namespace

bool decodePMS(const uint8_t *d, size_t n, Image &out) {
    if (n < 0x30 || d[0] != 'P' || d[1] != 'M') return false;
    uint32_t bpp = d[6];
    uint32_t w = rd32(d + 0x18), h = rd32(d + 0x1C), dataOff = rd32(d + 0x20), auxOff = rd32(d + 0x24);
    if (bpp != 8 || !w || !h || w > 16384 || h > 16384 || dataOff >= n || auxOff > n || n - auxOff < 768)
        return false;
    std::vector<uint8_t> idx;
    if (!unpackPMS8(d + dataOff, n - dataOff, int(w), int(h), idx)) return false;
    out.w = int(w); out.h = int(h);
    out.rgba.resize(size_t(w) * h * 4);
    const uint8_t *pal = d + auxOff;
    for (size_t i = 0; i < idx.size(); i++) {
        const uint8_t *c = pal + idx[i] * 3;
        out.rgba[i * 4] = c[0]; out.rgba[i * 4 + 1] = c[1]; out.rgba[i * 4 + 2] = c[2]; out.rgba[i * 4 + 3] = idx[i];
    }
    out.hasAlpha = true;
    return true;
}

bool decodeQNT(const uint8_t *d, size_t n, Image &out) {
    if (n < 0x30 || std::memcmp(d, "QNT", 3) != 0) return false;
    uint32_t ver = rd32(d + 4);
    size_t off = 4 + 4;  // fields follow the version
    uint32_t hdr = ver == 0 ? 0x30 : rd32(d + off);
    if (ver != 0) off += 4;
    int x = int(rd32(d + off)), y = int(rd32(d + off + 4)), w = int(rd32(d + off + 8)), h = int(rd32(d + off + 12));
    uint32_t bpp = rd32(d + off + 16);
    uint32_t psize = rd32(d + off + 24), asize = rd32(d + off + 28);
    if (w <= 0 || h <= 0 || w > 16384 || h > 16384 || bpp != 24 || hdr + uint64_t(psize) + asize > n) return false;
    out.w = w; out.h = h; out.x = x; out.y = y;
    out.rgba.assign(size_t(w) * h * 4, 255);
    int W = (w + 1) & ~1, H = (h + 1) & ~1;
    size_t plane = size_t(W) * H;
    if (psize) {
        std::vector<uint8_t> px;
        if (!inflateTo(d + hdr, psize, px, plane * 3)) return false;
        // Planes are stored blue, green, red.
        for (int c = 0; c < 3; c++) unpackPlane(px.data() + plane * c, w, h, out.rgba.data() + (2 - c), 4);
    }
    out.hasAlpha = asize != 0;
    if (asize) {
        // The alpha plane is stored row by row with the same prediction. Its width is rounded up to even; version 2
        // files (Test Game 56) leave an odd height as is, so the missing last row reads as zeros.
        std::vector<uint8_t> a(plane, 0);
        uLongf len = plane;
        if (uncompress(a.data(), &len, d + hdr + psize, asize) != Z_OK || (len != plane && len != size_t(W) * h))
            return false;
        std::vector<uint8_t> blocked(plane);
        size_t k = 0;
        for (int yy = 0; yy < H; yy += 2)
            for (int xx = 0; xx < W; xx += 2) {
                blocked[k++] = a[size_t(yy) * W + xx];
                blocked[k++] = a[size_t(yy + 1) * W + xx];
                blocked[k++] = a[size_t(yy) * W + xx + 1];
                blocked[k++] = a[size_t(yy + 1) * W + xx + 1];
            }
        unpackPlane(blocked.data(), w, h, out.rgba.data() + 3, 4);
    }
    return true;
}

std::vector<uint8_t> encodeQNT(const uint8_t *rgba, int w, int h) {
    int W = (w + 1) & ~1, H = (h + 1) & ~1;
    size_t plane = size_t(W) * H;
    std::vector<uint8_t> planes(plane * 3, 0);
    // The inverse of unpackPlane: each byte is the prediction minus the pixel, in 2x2 blocks; padding stays 0.
    for (int c = 0; c < 3; c++) {
        const uint8_t *src = rgba + (2 - c);  // stored blue, green, red
        auto px = [&](int x, int y) { return int(src[(size_t(y) * w + x) * 4]); };
        std::vector<uint8_t> diff(plane, 0);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                int pred = x == 0 && y == 0 ? 0 : y == 0 ? px(x - 1, 0) : x == 0 ? px(0, y - 1) : (px(x, y - 1) + px(x - 1, y)) >> 1;
                diff[size_t(y) * W + x] = uint8_t(x == 0 && y == 0 ? px(0, 0) : pred - px(x, y));
            }
        uint8_t *out = planes.data() + plane * c;
        size_t k = 0;
        for (int y = 0; y < H; y += 2)
            for (int x = 0; x < W; x += 2) {
                out[k++] = diff[size_t(y) * W + x];
                out[k++] = diff[size_t(y + 1) * W + x];
                out[k++] = diff[size_t(y) * W + x + 1];
                out[k++] = diff[size_t(y + 1) * W + x + 1];
            }
    }
    uLongf cap = compressBound(uLong(planes.size()));
    std::vector<uint8_t> d(0x30 + cap, 0);
    if (compress2(d.data() + 0x30, &cap, planes.data(), uLong(planes.size()), 9) != Z_OK) return {};
    d.resize(0x30 + cap);
    std::memcpy(d.data(), "QNT", 4);
    uint32_t fields[] = {0, 0, 0, uint32_t(w), uint32_t(h), 24, 1, uint32_t(cap), 0};  // version, x, y, w, h, bpp, ?, sizes
    std::memcpy(d.data() + 4, fields, sizeof fields);
    return d;
}

bool decodeWebP(const uint8_t *d, size_t n, Image &out);

bool decodeAJP(const uint8_t *d, size_t n, Image &out) {
    if (n < 0x38 || std::memcmp(d, "AJP", 3) != 0) return false;
    uint32_t jpgOff = rd32(d + 20), jpgSize = rd32(d + 24), maskOff = rd32(d + 28), maskSize = rd32(d + 32);
    if (uint64_t(jpgOff) + jpgSize > n || uint64_t(maskOff) + maskSize > n) return false;
    std::vector<uint8_t> jpg(d + jpgOff, d + jpgOff + jpgSize), mask(d + maskOff, d + maskOff + maskSize);
    for (size_t i = 0; i < 16 && i < jpg.size(); i++) jpg[i] ^= kAjpKey[i];
    for (size_t i = 0; i < 16 && i < mask.size(); i++) mask[i] ^= kAjpKey[i];
    int w, h, comp;
    uint8_t *px = stbi_load_from_memory(jpg.data(), int(jpg.size()), &w, &h, &comp, 4);
    if (!px) return false;
    out.w = w; out.h = h;
    out.rgba.assign(px, px + size_t(w) * h * 4);
    stbi_image_free(px);
    out.hasAlpha = maskSize != 0;
    if (maskSize) {
        // The mask is an 8-bit PMS whose pixel values are the alpha, or (in Test Game 46) a grey WebP.
        Image m;
        bool webp = mask.size() >= 12 && std::memcmp(mask.data(), "RIFF", 4) == 0;
        if (!(webp ? decodeWebP(mask.data(), mask.size(), m) : decodePMS(mask.data(), mask.size(), m)) || m.w != w || m.h != h)
            return false;
        for (size_t i = 0; i < size_t(w) * h; i++) out.rgba[i * 4 + 3] = webp ? m.rgba[i * 4] : m.rgba[i * 4 + 3];
    }
    return true;
}

// WebP (Test Game 46's pictures): the system's decoder on Apple systems, libwebp on the Linux test host.
bool decodeWebP(const uint8_t *d, size_t n, Image &out) {
#if __APPLE__
    CFDataRef data = CFDataCreate(nullptr, d, CFIndex(n));
    CGImageSourceRef src = CGImageSourceCreateWithData(data, nullptr);
    CFRelease(data);
    if (!src) return false;
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, nullptr);
    CFRelease(src);
    if (!img) return false;
    int w = int(CGImageGetWidth(img)), h = int(CGImageGetHeight(img));
    CGImageAlphaInfo ai = CGImageGetAlphaInfo(img);
    out.w = w;
    out.h = h;
    out.hasAlpha = ai != kCGImageAlphaNone && ai != kCGImageAlphaNoneSkipLast && ai != kCGImageAlphaNoneSkipFirst;
    out.rgba.assign(size_t(w) * h * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(out.rgba.data(), size_t(w), size_t(h), 8, size_t(w) * 4, cs,
                                             kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if (!ctx) { CGImageRelease(img); return false; }
    CGContextSetBlendMode(ctx, kCGBlendModeCopy);
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), img);
    CGContextRelease(ctx);
    CGImageRelease(img);
    // Core Graphics draws premultiplied; the runtime keeps straight alpha.
    for (size_t i = 0; i < out.rgba.size(); i += 4) {
        int a = out.rgba[i + 3];
        if (a && a < 255)
            for (int k = 0; k < 3; k++) out.rgba[i + k] = uint8_t(std::min(255, out.rgba[i + k] * 255 / a));
    }
    return true;
#else
    int w, h;
    WebPBitstreamFeatures f;
    if (WebPGetFeatures(d, n, &f) != VP8_STATUS_OK) return false;
    uint8_t *px = WebPDecodeRGBA(d, n, &w, &h);
    if (!px) return false;
    out.w = w;
    out.h = h;
    out.hasAlpha = f.has_alpha;
    out.rgba.assign(px, px + size_t(w) * h * 4);
    WebPFree(px);
    return true;
#endif
}

// The chunks after a WebP's image: "OVER" holds the number of the picture it is drawn over (see Image::overBase).
int webpOverBase(const uint8_t *d, size_t n) {
    for (size_t p = 12; p + 8 <= n;) {
        uint32_t len;
        std::memcpy(&len, d + p + 4, 4);
        if (std::memcmp(d + p, "OVER", 4) == 0 && len >= 4 && p + 12 <= n) {
            int32_t base;
            std::memcpy(&base, d + p + 8, 4);
            return base;
        }
        p += 8 + size_t(len) + (len & 1);
    }
    return -1;
}

bool decodeImage(const uint8_t *d, size_t n, Image &out) {
    if (n >= 12 && std::memcmp(d, "RIFF", 4) == 0 && std::memcmp(d + 8, "WEBP", 4) == 0) {
        if (!decodeWebP(d, n, out)) return false;
        out.overBase = webpOverBase(d, n);
        return true;
    }
    if (n >= 4 && std::memcmp(d, "QNT", 3) == 0) return decodeQNT(d, n, out);
    if (n >= 4 && std::memcmp(d, "AJP", 3) == 0) return decodeAJP(d, n, out);
    if (n >= 4 && d[0] == 'P' && d[1] == 'M') return decodePMS(d, n, out);
    int w, h, comp;
    uint8_t *px = stbi_load_from_memory(d, int(n), &w, &h, &comp, 4);
    if (!px) return false;
    out.w = w; out.h = h;
    out.hasAlpha = comp == 4 || comp == 2;
    out.rgba.assign(px, px + size_t(w) * h * 4);
    stbi_image_free(px);
    return true;
}

}  // namespace s4
