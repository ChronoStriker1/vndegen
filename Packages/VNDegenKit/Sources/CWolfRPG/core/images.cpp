#include "images.h"


namespace wolf {

ImageCache::ImageCache(const GameFiles &files, std::function<void(const std::string &)> log)
    : files_(files), log_(std::move(log)) {}

BitmapPtr ImageCache::get(const std::string &path) {
    auto it = images_.find(path);
    if (it != images_.end()) return it->second;
    auto img = std::make_shared<Bitmap>();
    if (auto bytes = files_.read(path)) {
        if (!decodeImage(*bytes, *img)) {
            log_("cannot decode image " + path);
        }
    } else if (!path.empty()) {
        log_("missing image " + path);
    }
    images_[path] = img;
    return img;
}

}  // namespace wolf
