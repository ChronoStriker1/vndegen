#include "images.h"


namespace wolf {

ImageCache::ImageCache(const GameFiles &files, std::function<void(const std::string &)> log)
    : files_(files), log_(std::move(log)) {}

BitmapPtr ImageCache::get(const std::string &path) {
    auto it = images_.find(path);
    if (it != images_.end()) {
        it->second.used = ++clock_;
        return it->second.bitmap;
    }
    auto img = std::make_shared<Bitmap>();
    if (auto bytes = files_.read(path)) {
        if (!decodeImage(*bytes, *img)) {
            log_("cannot decode image " + path);
        }
    } else if (!path.empty()) {
        log_("missing image " + path);
    }
    size_t size = img->px.size();
    images_[path] = {img, size, ++clock_};
    bytes_ += size;
    if (bytes_ > kBudget) evict(path);
    return img;
}

void ImageCache::evict(const std::string &keep) {
    while (bytes_ > kBudget) {
        auto oldest = images_.end();
        for (auto it = images_.begin(); it != images_.end(); ++it) {
            if (it->second.bytes == 0 || it->first == keep) continue;
            if (oldest == images_.end() || it->second.used < oldest->second.used) oldest = it;
        }
        if (oldest == images_.end()) return;
        bytes_ -= oldest->second.bytes;
        images_.erase(oldest);
    }
}

}  // namespace wolf
