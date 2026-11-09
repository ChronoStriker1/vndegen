#include "image.h"
#include "webp.h"

#include <algorithm>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "../third_party/stb/stb_image.h"

namespace neko {
namespace {

constexpr size_t kCacheBytes = 256u * 1024 * 1024;

int32_t rd32(const uint8_t *p) {
    int32_t v;
    std::memcpy(&v, p, 4);
    return v;
}

bool decodeBmp(const std::vector<uint8_t> &d, Image &out) {
    if (d.size() < 54) return false;
    int32_t offset = rd32(&d[10]), w = rd32(&d[18]), h = rd32(&d[22]);
    int bits = d[28] | d[29] << 8;
    if (rd32(&d[30]) != 0 || (bits != 8 && bits != 24 && bits != 32) || w <= 0 || h == 0 || offset < 54) return false;
    bool bottomUp = h > 0;
    h = std::abs(h);
    size_t stride = (size_t(w) * (bits / 8) + 3) & ~size_t(3);
    if (size_t(offset) + stride * size_t(h) > d.size()) return false;
    // An 8-bit picture's colours, 4 bytes each after the header, as one of Test Game 83's backgrounds has (set 3): as
    // many as the header says, or 256, or as fit before the pixels.
    const uint8_t *palette = nullptr;
    size_t colours = 0;
    if (bits == 8) {
        int32_t header = rd32(&d[14]), used = rd32(&d[46]);
        if (header < 40 || size_t(14) + size_t(header) > size_t(offset)) return false;
        palette = d.data() + 14 + header;
        colours = std::min({size_t(used > 0 ? used : 256), size_t(256), (size_t(offset) - 14 - size_t(header)) / 4});
    }
    out.w = w;
    out.h = h;
    out.rgba.resize(size_t(w) * h * 4);
    for (int y = 0; y < h; y++) {
        const uint8_t *src = d.data() + offset + stride * size_t(bottomUp ? h - 1 - y : y);
        uint8_t *dst = out.rgba.data() + size_t(y) * w * 4;
        if (bits == 8) {
            // A pixel past the colours the file has is black.
            for (int x = 0; x < w; x++, src++, dst += 4) {
                const uint8_t *colour = *src < colours ? palette + size_t(*src) * 4 : nullptr;
                dst[0] = colour ? colour[2] : 0;
                dst[1] = colour ? colour[1] : 0;
                dst[2] = colour ? colour[0] : 0;
                dst[3] = 255;
            }
        } else if (bits == 24) {
            for (int x = 0; x < w; x++, src += 3, dst += 4) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = 255;
            }
        } else {
            for (int x = 0; x < w; x++, src += 4, dst += 4) {
                dst[0] = src[2];
                dst[1] = src[1];
                dst[2] = src[0];
                dst[3] = 255;
            }
        }
    }
    return true;
}

// One byte per pixel, top row first. Row padding is rare but real: some files round each row up to 4 bytes.
void applyAlpha(const std::vector<uint8_t> &alp, Image &image) {
    size_t w = size_t(image.w), h = size_t(image.h);
    size_t stride = alp.size() == w * h ? w : (w + 3) & ~size_t(3);
    if (alp.size() < stride * h) return;
    for (size_t y = 0; y < h; y++) {
        const uint8_t *src = alp.data() + y * stride;
        uint8_t *dst = image.rgba.data() + y * w * 4 + 3;
        for (size_t x = 0; x < w; x++, dst += 4) *dst = src[x];
    }
    image.opaque = false;
}

std::string withExtension(const std::string &path, const char *extension) {
    auto dot = path.rfind('.');
    return (dot == std::string::npos ? path : path.substr(0, dot)) + extension;
}

}  // namespace

ImageRef decodeBmpBytes(const std::vector<uint8_t> &data) {
    auto image = std::make_shared<Image>();
    if (data.size() < 54 || data[0] != 'B' || data[1] != 'M' || !decodeBmp(data, *image)) return nullptr;
    return image;
}

std::vector<uint8_t> encodeThumbnail(const uint8_t *rgba, int frameW, int frameH, int w, int h) {
    int stride = (w * 3 + 3) & ~3;
    uint32_t pixels = uint32_t(stride) * h, total = pixels + 54;
    std::vector<uint8_t> out(total, 0);
    out[0] = 'B';
    out[1] = 'M';
    std::memcpy(&out[2], &total, 4);
    out[10] = 54;
    out[14] = 40;
    std::memcpy(&out[18], &w, 4);
    std::memcpy(&out[22], &h, 4);
    out[26] = 1;
    out[28] = 24;
    std::memcpy(&out[34], &pixels, 4);
    // Each thumbnail pixel averages the block of the frame under it.
    for (int y = 0; y < h; y++) {
        uint8_t *row = &out[54 + size_t(h - 1 - y) * stride];
        int y0 = y * frameH / h, y1 = std::max(y0 + 1, (y + 1) * frameH / h);
        for (int x = 0; x < w; x++) {
            int x0 = x * frameW / w, x1 = std::max(x0 + 1, (x + 1) * frameW / w);
            int r = 0, g = 0, b = 0, n = 0;
            for (int yy = y0; yy < y1; yy++) {
                const uint8_t *p = rgba + (size_t(yy) * frameW + x0) * 4;
                for (int xx = x0; xx < x1; xx++, p += 4, n++) {
                    r += p[0];
                    g += p[1];
                    b += p[2];
                }
            }
            row[x * 3] = uint8_t(b / n);
            row[x * 3 + 1] = uint8_t(g / n);
            row[x * 3 + 2] = uint8_t(r / n);
        }
    }
    return out;
}

ImageRef Images::decode(const std::string &path) const {
    Bytes data = files_.read(path);
    if (!data || data->size() < 8) return nullptr;
    auto image = std::make_shared<Image>();
    if ((*data)[0] == 'B' && (*data)[1] == 'M') {
        if (!decodeBmp(*data, *image)) return nullptr;
    } else if (!std::memcmp(data->data(), "\x89PNG", 4)) {
        int w = 0, h = 0, n = 0;
        uint8_t *px = stbi_load_from_memory(data->data(), int(data->size()), &w, &h, &n, 4);
        if (!px) return nullptr;
        image->w = w;
        image->h = h;
        image->rgba.assign(px, px + size_t(w) * h * 4);
        image->opaque = n != 2 && n != 4;
        stbi_image_free(px);
    } else if (isWebP(*data)) {
        if (!decodeWebP(*data, path, [this](const std::string &base) { return decode(base); }, *image)) return nullptr;
    } else {
        return nullptr;
    }
    if (Bytes alp = files_.read(withExtension(path, ".alp"))) applyAlpha(*alp, *image);
    else if (colorKey_ >= 0 && image->opaque) {
        uint8_t r = uint8_t(colorKey_ >> 16), g = uint8_t(colorKey_ >> 8), b = uint8_t(colorKey_);
        for (size_t i = 0; i + 3 < image->rgba.size(); i += 4)
            if (image->rgba[i] == r && image->rgba[i + 1] == g && image->rgba[i + 2] == b) {
                image->rgba[i + 3] = 0;
                image->opaque = false;
            }
    }
    return image;
}

namespace {

// Three passes of a box blur each way, which is close to a Gaussian.
void boxBlur(Image &image, int radius) {
    int w = image.w, h = image.h, r = std::max(1, radius);
    std::vector<uint8_t> line(size_t(std::max(w, h)) * 4);
    auto run = [&](uint8_t *start, int count, ptrdiff_t stride) {
        int sum[4] = {0, 0, 0, 0};
        for (int i = 0; i < count; i++) std::memcpy(&line[size_t(i) * 4], start + i * stride, 4);
        auto at = [&](int i) { return &line[size_t(std::clamp(i, 0, count - 1)) * 4]; };
        for (int i = -r; i <= r; i++)
            for (int k = 0; k < 4; k++) sum[k] += at(i)[k];
        int size = 2 * r + 1;
        for (int i = 0; i < count; i++) {
            uint8_t *d = start + i * stride;
            for (int k = 0; k < 4; k++) {
                d[k] = uint8_t(sum[k] / size);
                sum[k] += at(i + r + 1)[k] - at(i - r)[k];
            }
        }
    };
    for (int pass = 0; pass < 3; pass++) {
        for (int y = 0; y < h; y++) run(image.rgba.data() + size_t(y) * w * 4, w, 4);
        for (int x = 0; x < w; x++) run(image.rgba.data() + size_t(x) * 4, h, ptrdiff_t(w) * 4);
    }
}

}  // namespace

ImageRef Images::remember(const std::string &key, ImageRef image) {
    recent_.emplace_front(key, image);
    byPath_[key] = recent_.begin();
    held_ += image->bytes();
    while (held_ > kCacheBytes && recent_.size() > 1) {
        held_ -= recent_.back().second->bytes();
        byPath_.erase(recent_.back().first);
        recent_.pop_back();
    }
    return image;
}

Images::~Images() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();
}

static std::string blurKey(const std::string &key, int radius) { return radius > 0 ? key + "#" + std::to_string(radius) : key; }

void Images::setArtSet(const std::string &patch) {
    std::lock_guard<std::mutex> lock(mu_);
    artSet_ = normalizedPath(patch);
}

std::string Images::sourceLocked(const std::string &path) const {
    std::string key = normalizedPath(path);
    if (artSet_.empty() || (key.rfind("cg\\", 0) != 0 && key.rfind("sc\\", 0) != 0)) return key;
    std::string other = artSet_ + "\\" + key;
    return files_.exists(other) ? other : key;
}

std::string Images::source(const std::string &path) {
    std::lock_guard<std::mutex> lock(mu_);
    return sourceLocked(path);
}

void Images::prefetch(const std::vector<std::pair<std::string, int>> &wanted) {
    {
        std::lock_guard<std::mutex> lock(mu_);
        queue_.clear();
        for (const auto &[path, radius] : wanted) {
            std::string from = sourceLocked(path), key = blurKey(from, radius);
            if (!byPath_.count(key) && key != decoding_) queue_.emplace_back(from, radius);
        }
        if (!worker_.joinable()) worker_ = std::thread([this] { work(); });
    }
    wake_.notify_all();
}

void Images::work() {
    std::unique_lock<std::mutex> lock(mu_);
    while (true) {
        wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (stopping_) return;
        auto [path, radius] = queue_.front();
        queue_.pop_front();
        std::string base = normalizedPath(path), key = blurKey(base, radius);
        if (byPath_.count(key)) continue;
        decoding_ = key;
        ImageRef source;
        if (radius > 0) {
            auto it = byPath_.find(base);
            if (it != byPath_.end()) source = it->second->second;
        }
        lock.unlock();
        if (!source) source = decode(path);
        ImageRef image = source;
        if (source && radius > 0) {
            auto blurred = std::make_shared<Image>(*source);
            boxBlur(*blurred, radius);
            image = blurred;
        }
        lock.lock();
        decoding_.clear();
        if (image) remember(key, image);
        done_.notify_all();
    }
}

ImageRef Images::get(const std::string &path) {
    std::unique_lock<std::mutex> lock(mu_);
    std::string key = sourceLocked(path);
    // An empty name would wait for ever: decoding_ is empty while the worker has nothing to do.
    if (key.empty()) return nullptr;
    done_.wait(lock, [&] { return decoding_ != key; });
    auto it = byPath_.find(key);
    if (it != byPath_.end()) {
        recent_.splice(recent_.begin(), recent_, it->second);
        return it->second->second;
    }
    lock.unlock();
    ImageRef image = decode(key);
    if (!image) return nullptr;
    lock.lock();
    return remember(key, image);
}

ImageRef Images::blurred(const std::string &path, int radius) {
    std::string key = blurKey(source(path), radius);
    {
        std::unique_lock<std::mutex> lock(mu_);
        done_.wait(lock, [&] { return decoding_ != key; });
        auto it = byPath_.find(key);
        if (it != byPath_.end()) {
            recent_.splice(recent_.begin(), recent_, it->second);
            return it->second->second;
        }
    }
    ImageRef source = get(path);
    if (!source) return nullptr;
    auto image = std::make_shared<Image>(*source);
    boxBlur(*image, radius);
    std::lock_guard<std::mutex> lock(mu_);
    return remember(key, image);
}

}  // namespace neko
