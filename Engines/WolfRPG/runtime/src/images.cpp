#include "images.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "../third_party/stb/stb_image.h"

namespace wolf {

ImageCache::ImageCache(SDL_Renderer *renderer, const GameFiles &files, std::function<void(const std::string &)> log)
    : r_(renderer), files_(files), log_(std::move(log)) {}

ImageCache::~ImageCache() {
    for (auto &[k, i] : images_) SDL_DestroyTexture(i.texture);
}

Image ImageCache::get(const std::string &path) {
    auto it = images_.find(path);
    if (it != images_.end()) return it->second;
    Image img;
    if (auto bytes = files_.read(path)) {
        int w, h, n;
        if (stbi_uc *px = stbi_load_from_memory(bytes->data(), int(bytes->size()), &w, &h, &n, 4)) {
            if (SDL_Surface *s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32, px, w * 4)) {
                img.texture = SDL_CreateTextureFromSurface(r_, s);
                img.w = w;
                img.h = h;
                SDL_DestroySurface(s);
            }
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
