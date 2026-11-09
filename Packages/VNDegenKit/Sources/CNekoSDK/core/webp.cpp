// Test Game 82's pictures: lossless WebP stored under `.bmp` names. A base picture carries its own alpha. A difference
// picture is opaque and ends in an "OVER" chunk naming its base, such as `H100a.webp` for `cg\H100a.bmp` beside it:
// where it is magenta (255, 0, 255) the base's pixel shows, alpha and all, and bases chain. Read from the files.
#include "webp.h"

#include <algorithm>
#include <cstring>

#if __APPLE__
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#else
#include <webp/decode.h>
#endif

namespace neko {
namespace {

uint32_t rd32(const uint8_t *p) {
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

void wr32(uint8_t *p, uint32_t v) { std::memcpy(p, &v, 4); }

// The file without its OVER chunk, which decoders don't know, and the base that chunk names.
std::vector<uint8_t> withoutOver(const std::vector<uint8_t> &d, std::string &over) {
    std::vector<uint8_t> out(d.begin(), d.begin() + 12);
    for (size_t p = 12; p + 8 <= d.size();) {
        size_t len = rd32(&d[p + 4]), end = std::min(d.size(), p + 8 + len + (len & 1));
        if (!std::memcmp(&d[p], "OVER", 4))
            over.assign(reinterpret_cast<const char *>(&d[p + 8]), std::min(len, d.size() - p - 8));
        else
            out.insert(out.end(), d.begin() + ptrdiff_t(p), d.begin() + ptrdiff_t(end));
        p = end;
    }
    wr32(&out[4], uint32_t(out.size() - 8));
    while (!over.empty() && over.back() == '\0') over.pop_back();
    return out;
}

bool decode(const std::vector<uint8_t> &d, Image &out) {
#if __APPLE__
    CFDataRef data = CFDataCreate(nullptr, d.data(), CFIndex(d.size()));
    CGImageSourceRef src = CGImageSourceCreateWithData(data, nullptr);
    CFRelease(data);
    if (!src) return false;
    CGImageRef img = CGImageSourceCreateImageAtIndex(src, 0, nullptr);
    CFRelease(src);
    if (!img) return false;
    int w = int(CGImageGetWidth(img)), h = int(CGImageGetHeight(img));
    out.w = w;
    out.h = h;
    out.rgba.assign(size_t(w) * h * 4, 0);
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate(out.rgba.data(), size_t(w), size_t(h), 8, size_t(w) * 4, cs,
                                             kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
    CGColorSpaceRelease(cs);
    if (!ctx) {
        CGImageRelease(img);
        return false;
    }
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
#else
    int w = 0, h = 0;
    uint8_t *px = WebPDecodeRGBA(d.data(), d.size(), &w, &h);
    if (!px) return false;
    out.w = w;
    out.h = h;
    out.rgba.assign(px, px + size_t(w) * h * 4);
    WebPFree(px);
#endif
    out.opaque = true;
    for (size_t i = 3; i < out.rgba.size() && out.opaque; i += 4) out.opaque = out.rgba[i] == 255;
    return true;
}

}  // namespace

bool isWebP(const std::vector<uint8_t> &d) {
    return d.size() >= 12 && !std::memcmp(d.data(), "RIFF", 4) && !std::memcmp(d.data() + 8, "WEBP", 4);
}

bool decodeWebP(const std::vector<uint8_t> &data, const std::string &path,
                const std::function<ImageRef(const std::string &)> &picture, Image &out) {
    std::string over;
    if (!decode(withoutOver(data, over), out)) return false;
    if (over.empty()) return true;
    // The base is a file beside this one, under the `.bmp` name the archive keeps it by.
    std::string base = path.substr(0, path.find_last_of('\\') + 1) + over.substr(0, over.find_last_of('.')) + ".bmp";
    // A chain that came back on itself would never end; the deepest seen is three.
    thread_local int depth = 0;
    if (base == path || depth >= 16) return true;
    depth++;
    ImageRef under = picture(base);
    depth--;
    if (!under || under->w != out.w || under->h != out.h) return true;
    for (size_t i = 0; i < out.rgba.size(); i += 4)
        if (out.rgba[i] == 255 && out.rgba[i + 1] == 0 && out.rgba[i + 2] == 255) std::memcpy(&out.rgba[i], &under->rgba[i], 4);
    out.opaque = under->opaque;
    return true;
}

}  // namespace neko
