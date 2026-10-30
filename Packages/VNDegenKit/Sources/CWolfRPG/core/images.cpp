#include "images.h"

#include "../third_party/stb/stb_image.h"

#include <cstring>

namespace wolf {

ImageCache::ImageCache(const GameFiles &files, std::function<void(const std::string &)> log)
    : files_(files), log_(std::move(log)) {}

BitmapPtr ImageCache::get(const std::string &path) {
    auto it = images_.find(path);
    if (it != images_.end()) return it->second;
    auto img = std::make_shared<Bitmap>();
    if (auto bytes = files_.read(path)) {
        int w, h, n;
        if (stbi_uc *px = stbi_load_from_memory(bytes->data(), int(bytes->size()), &w, &h, &n, 4)) {
            *img = Bitmap(w, h);
            std::memcpy(img->px.data(), px, img->px.size());
            stbi_image_free(px);
        } else {
            log_("cannot decode image " + path);
        }
    } else if (!path.empty()) {
        log_("missing image " + path);
    }
    images_[path] = img;
    return img;
}

}  // namespace wolf
