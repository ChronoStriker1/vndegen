#include "canvas.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>

namespace neko {

void Canvas::clear() { std::memset(px.data(), 0, px.size()); for (size_t i = 3; i < px.size(); i += 4) px[i] = 255; }

void Canvas::saveRect(int x, int y, int rw, int rh, std::vector<uint8_t> &saved) const {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(w, x + rw), y1 = std::min(h, y + rh);
    saved.clear();
    if (x0 >= x1 || y0 >= y1) return;
    saved.resize(size_t(x1 - x0) * (y1 - y0) * 4);
    for (int row = y0; row < y1; row++)
        std::memcpy(&saved[size_t(row - y0) * (x1 - x0) * 4], &px[(size_t(row) * w + x0) * 4], size_t(x1 - x0) * 4);
}

void Canvas::restoreRect(int x, int y, int rw, int rh, const std::vector<uint8_t> &saved) {
    int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(w, x + rw), y1 = std::min(h, y + rh);
    if (x0 >= x1 || y0 >= y1 || saved.size() != size_t(x1 - x0) * (y1 - y0) * 4) return;
    for (int row = y0; row < y1; row++)
        std::memcpy(&px[(size_t(row) * w + x0) * 4], &saved[size_t(row - y0) * (x1 - x0) * 4], size_t(x1 - x0) * 4);
}

namespace {

// A few threads that wait for rows to draw. The main thread takes rows too, so a job finishes even if none wakes in time.
class RowPool {
public:
    static RowPool &shared() {
        // Never destroyed: its threads wait on it until the process ends.
        static RowPool *pool = new RowPool;
        return *pool;
    }
    void run(int rows, const std::function<void(int, int)> &body) {
        if (workers_.empty()) {
            body(0, rows);
            return;
        }
        // Small bands, so a core that a busy machine holds back only delays a little of the frame.
        int band = std::max(8, rows / (int(workers_.size() + 1) * 4));
        {
            std::lock_guard<std::mutex> lock(mu_);
            body_ = &body;
            rows_ = rows;
            band_ = band;
            next_.store(0);
            active_ = int(workers_.size());
            generation_++;
        }
        wake_.notify_all();
        take();
        std::unique_lock<std::mutex> lock(mu_);
        done_.wait(lock, [this] { return active_ == 0; });
        body_ = nullptr;
    }

private:
    RowPool() {
        // Fewer than every core: the display, the audio thread and the host need some too.
        int count = std::clamp(int(std::thread::hardware_concurrency()) / 2, 0, 5) - 1;
        for (int i = 0; i < count; i++) {
            workers_.emplace_back([this] { work(); });
            workers_.back().detach();
        }
    }
    void take() {
        for (;;) {
            int start = next_.fetch_add(band_);
            if (start >= rows_) return;
            (*body_)(start, std::min(rows_, start + band_));
        }
    }
    void work() {
        uint64_t seen = 0;
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(mu_);
                wake_.wait(lock, [&] { return generation_ != seen; });
                seen = generation_;
            }
            take();
            std::lock_guard<std::mutex> lock(mu_);
            if (--active_ == 0) done_.notify_one();
        }
    }
    std::mutex mu_;
    std::condition_variable wake_, done_;
    std::vector<std::thread> workers_;
    const std::function<void(int, int)> *body_ = nullptr;
    std::atomic<int> next_{0};
    int rows_ = 0, band_ = 1, active_ = 0;
    uint64_t generation_ = 0;
};

// Below this many pixels, waking the pool costs more than it saves.
constexpr size_t kParallelPixels = 100000;

inline void blendPixel(uint8_t *d, int r, int g, int b, int a256) {
    d[0] = uint8_t(d[0] + (((r - d[0]) * a256) >> 8));
    d[1] = uint8_t(d[1] + (((g - d[1]) * a256) >> 8));
    d[2] = uint8_t(d[2] + (((b - d[2]) * a256) >> 8));
}

}  // namespace

void forRows(int rows, size_t pixels, const std::function<void(int, int)> &body) {
    if (rows <= 0) return;
    if (pixels < kParallelPixels) body(0, rows);
    else RowPool::shared().run(rows, body);
}

void drawPart(Canvas &dst, const Image &image, int x, int y, int partX, int partY, int partW, int partH, float alpha) {
    if (alpha <= 0) return;
    if (x < 0) { partX -= x; partW += x; x = 0; }
    if (y < 0) { partY -= y; partH += y; y = 0; }
    partW = std::min({partW, dst.w - x, image.w - partX});
    partH = std::min({partH, dst.h - y, image.h - partY});
    if (partW <= 0 || partH <= 0) return;
    int a256 = int(alpha * 256 + 0.5f);
    forRows(partH, size_t(partW) * partH, [&](int row0, int row1) {
        for (int row = row0; row < row1; row++) {
            const uint8_t *s = image.rgba.data() + (size_t(partY + row) * image.w + partX) * 4;
            uint8_t *d = dst.px.data() + (size_t(y + row) * dst.w + x) * 4;
            if (image.opaque && a256 >= 256) {
                std::memcpy(d, s, size_t(partW) * 4);
                continue;
            }
            for (int i = 0; i < partW; i++, s += 4, d += 4) {
                int a = (s[3] * a256) >> 8;
                a += a >> 7;  // 255 becomes 256, so an opaque pixel replaces
                blendPixel(d, s[0], s[1], s[2], a);
            }
        }
    });
}

void drawImage(Canvas &dst, const Image &image, float cx, float cy, float sx, float sy, float radians, float alpha, const std::vector<int> *rowShift, bool additive) {
    if (alpha <= 0 || sx == 0 || sy == 0 || image.w == 0 || image.h == 0) return;
    float halfW = image.w * 0.5f, halfH = image.h * 0.5f;
    auto put = [additive](uint8_t *d, int r, int g, int b, int a) {
        if (!additive) {
            blendPixel(d, r, g, b, a);
            return;
        }
        d[0] = uint8_t(std::min(255, d[0] + ((r * a) >> 8)));
        d[1] = uint8_t(std::min(255, d[1] + ((g * a) >> 8)));
        d[2] = uint8_t(std::min(255, d[2] + ((b * a) >> 8)));
    };
    if (!rowShift && !additive && radians == 0 && sx == 1 && sy == 1) {
        float left = cx - halfW, top = cy - halfH;
        if (left == std::floor(left) && top == std::floor(top)) {
            drawPart(dst, image, int(left), int(top), 0, 0, image.w, image.h, alpha);
            return;
        }
    }
    // The screen rectangle that holds the transformed image.
    float c = std::cos(radians), s = std::sin(radians);
    float ex = std::abs(halfW * sx * c) + std::abs(halfH * sy * s), ey = std::abs(halfW * sx * s) + std::abs(halfH * sy * c);
    int margin = 0;
    if (rowShift)
        for (int shift : *rowShift) margin = std::max(margin, std::abs(shift));
    int x0 = std::max(0, int(std::floor(cx - ex)) - margin), x1 = std::min(dst.w, int(std::ceil(cx + ex)) + 1 + margin);
    int y0 = std::max(0, int(std::floor(cy - ey))), y1 = std::min(dst.h, int(std::ceil(cy + ey)) + 1);
    if (x0 >= x1 || y0 >= y1) return;
    // Each destination pixel's source position, in 16.16 fixed point, stepping along the row. Sampling points sit half
    // a pixel in from each edge, so an image at scale 1 samples its own pixels.
    constexpr float kOne = 65536.0f;
    float dux = c / sx, duy = s / sx, dvx = -s / sy, dvy = c / sy;
    int a256 = int(alpha * 256 + 0.5f);
    int maxU = (image.w - 1) << 16, maxV = (image.h - 1) << 16;
    bool bilinear = !(radians == 0 && sx == 1 && sy == 1);
    const uint8_t *src = image.rgba.data();
    bool solid = image.opaque && a256 >= 256;
    forRows(y1 - y0, size_t(x1 - x0) * (y1 - y0), [&](int row0, int row1) {
        for (int y = y0 + row0; y < y0 + row1; y++) {
            float rx = x0 + 0.5f - cx - (rowShift ? float((*rowShift)[size_t(y)]) : 0.0f), ry = y + 0.5f - cy;
            // Double precision would cost more than it gains: a row is at most a few thousand pixels of steps under 1.
            int32_t u = int32_t((rx * dux + ry * duy + halfW - 0.5f) * kOne), v = int32_t((rx * dvx + ry * dvy + halfH - 0.5f) * kOne);
            int32_t stepU = int32_t(dux * kOne), stepV = int32_t(dvx * kOne);
            uint8_t *d = dst.px.data() + (size_t(y) * dst.w + x0) * 4;
            for (int x = x0; x < x1; x++, u += stepU, v += stepV, d += 4) {
                // Half a source pixel beyond the edge still counts, so an edge at a fraction of a pixel isn't cut short.
                if (u < -32768 || v < -32768 || u > maxU + 32768 || v > maxV + 32768) continue;
                int ux = std::clamp(u, 0, maxU), vy = std::clamp(v, 0, maxV);
                int iu = ux >> 16, iv = vy >> 16;
                const uint8_t *p = src + (size_t(iv) * image.w + iu) * 4;
                if (!bilinear) {
                    int a = (p[3] * a256) >> 8;
                    a += a >> 7;
                    put(d, p[0], p[1], p[2], a);
                    continue;
                }
                int fu = (ux >> 8) & 255, fv = (vy >> 8) & 255;
                const uint8_t *pr = iu + 1 < image.w ? p + 4 : p;
                const uint8_t *pd = iv + 1 < image.h ? p + size_t(image.w) * 4 : p;
                const uint8_t *pdr = iu + 1 < image.w ? pd + 4 : pd;
                int w00 = (256 - fu) * (256 - fv), w10 = fu * (256 - fv), w01 = (256 - fu) * fv, w11 = fu * fv;
                int r = (p[0] * w00 + pr[0] * w10 + pd[0] * w01 + pdr[0] * w11) >> 16;
                int g = (p[1] * w00 + pr[1] * w10 + pd[1] * w01 + pdr[1] * w11) >> 16;
                int b = (p[2] * w00 + pr[2] * w10 + pd[2] * w01 + pdr[2] * w11) >> 16;
                if (solid) {
                    d[0] = uint8_t(r);
                    d[1] = uint8_t(g);
                    d[2] = uint8_t(b);
                    continue;
                }
                int sa = image.opaque ? 255 : (p[3] * w00 + pr[3] * w10 + pd[3] * w01 + pdr[3] * w11) >> 16;
                int a = (sa * a256) >> 8;
                a += a >> 7;
                put(d, r, g, b, a);
            }
        }
    });
}

void blendFrame(Canvas &dst, const Canvas &from, int amount256) {
    if (amount256 <= 0 || from.w != dst.w || from.h != dst.h) return;
    int keep = 256 - amount256;
    forRows(dst.h, size_t(dst.w) * dst.h, [&](int row0, int row1) {
        const uint8_t *s = from.px.data() + size_t(row0) * dst.w * 4;
        uint8_t *d = dst.px.data() + size_t(row0) * dst.w * 4;
        size_t n = size_t(row1 - row0) * dst.w * 4;
        for (size_t i = 0; i < n; i++) d[i] = uint8_t((d[i] * keep + s[i] * amount256) >> 8);
    });
}

void softenFrame(Canvas &dst, int divisor, int amount256) {
    if (amount256 <= 0 || divisor < 2) return;
    int sw = std::max(2, dst.w / divisor), sh = std::max(2, dst.h / divisor);
    std::vector<uint8_t> small(size_t(sw) * sh * 3);
    // Averages each block, then grows it back with bilinear filtering.
    forRows(sh, size_t(dst.w) * dst.h, [&](int small0, int small1) {
    for (int y = small0; y < small1; y++) {
        int y0 = y * dst.h / sh, y1 = (y + 1) * dst.h / sh;
        for (int x = 0; x < sw; x++) {
            int x0 = x * dst.w / sw, x1 = (x + 1) * dst.w / sw;
            int r = 0, g = 0, b = 0, n = 0;
            // Every other pixel each way is plenty for a picture that's about to be blurred.
            for (int yy = y0; yy < y1; yy += 2) {
                const uint8_t *p = dst.px.data() + (size_t(yy) * dst.w + x0) * 4;
                for (int xx = x0; xx < x1; xx += 2, p += 8, n++) {
                    r += p[0];
                    g += p[1];
                    b += p[2];
                }
            }
            uint8_t *o = small.data() + (size_t(y) * sw + x) * 3;
            o[0] = uint8_t(r / std::max(1, n));
            o[1] = uint8_t(g / std::max(1, n));
            o[2] = uint8_t(b / std::max(1, n));
        }
    }
    });
    int keep = 256 - std::min(amount256, 256);
    forRows(dst.h, size_t(dst.w) * dst.h, [&](int row0, int row1) {
    for (int y = row0; y < row1; y++) {
        int fy = (y * (sh - 1) << 8) / std::max(1, dst.h - 1);
        int iy = fy >> 8, wy = fy & 255, iy1 = std::min(iy + 1, sh - 1);
        uint8_t *d = dst.px.data() + size_t(y) * dst.w * 4;
        for (int x = 0; x < dst.w; x++, d += 4) {
            int fx = (x * (sw - 1) << 8) / std::max(1, dst.w - 1);
            int ix = fx >> 8, wx = fx & 255, ix1 = std::min(ix + 1, sw - 1);
            const uint8_t *a = small.data() + (size_t(iy) * sw + ix) * 3, *b = small.data() + (size_t(iy) * sw + ix1) * 3;
            const uint8_t *c = small.data() + (size_t(iy1) * sw + ix) * 3, *e = small.data() + (size_t(iy1) * sw + ix1) * 3;
            for (int k = 0; k < 3; k++) {
                int top = a[k] * (256 - wx) + b[k] * wx, bottom = c[k] * (256 - wx) + e[k] * wx;
                int v = (top * (256 - wy) + bottom * wy) >> 16;
                d[k] = uint8_t((d[k] * keep + v * (256 - keep)) >> 8);
            }
        }
    }
    });
}

void tintFrame(Canvas &dst, int r, int g, int b, int amount256) {
    if (amount256 <= 0) return;
    int keep = 256 - amount256;
    int cr = r * amount256, cg = g * amount256, cb = b * amount256;
    forRows(dst.h, size_t(dst.w) * dst.h, [&](int row0, int row1) {
        uint8_t *d = dst.px.data() + size_t(row0) * dst.w * 4;
        size_t n = size_t(row1 - row0) * dst.w * 4;
        for (size_t i = 0; i < n; i += 4) {
            d[i] = uint8_t((d[i] * keep + cr) >> 8);
            d[i + 1] = uint8_t((d[i + 1] * keep + cg) >> 8);
            d[i + 2] = uint8_t((d[i + 2] * keep + cb) >> 8);
        }
    });
}

void drawCoverage(Canvas &dst, const uint8_t *coverage, int w, int h, int x, int y, int r, int g, int b, int alpha256) {
    for (int row = 0; row < h; row++) {
        int yy = y + row;
        if (yy < 0 || yy >= dst.h) continue;
        const uint8_t *c = coverage + size_t(row) * w;
        for (int col = 0; col < w; col++) {
            int xx = x + col;
            if (xx < 0 || xx >= dst.w || !c[col]) continue;
            int a = (c[col] * alpha256) >> 8;
            a += a >> 7;
            blendPixel(dst.px.data() + (size_t(yy) * dst.w + xx) * 4, r, g, b, a);
        }
    }
}

}  // namespace neko
