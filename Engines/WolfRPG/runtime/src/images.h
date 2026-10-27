// Loads game images into SDL textures and keeps them for reuse.
#pragma once

#include "files.h"

#include <SDL3/SDL.h>

#include <functional>
#include <map>
#include <string>

namespace wolf {

struct Image {
    SDL_Texture *texture = nullptr;
    int w = 0, h = 0;
};

class ImageCache {
public:
    ImageCache(SDL_Renderer *renderer, const GameFiles &files, std::function<void(const std::string &)> log);
    ~ImageCache();
    // Returns an empty image when the file is missing or can't be decoded, and logs it once.
    Image get(const std::string &path);
    SDL_Renderer *renderer() const { return r_; }

private:
    SDL_Renderer *r_;
    const GameFiles &files_;
    std::function<void(const std::string &)> log_;
    std::map<std::string, Image> images_;
};

}  // namespace wolf
