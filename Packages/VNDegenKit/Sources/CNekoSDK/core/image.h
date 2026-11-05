// Pictures: 24-bit BMP with a separate .alp of 8-bit alpha, or PNG. Always RGBA with straight alpha.
#pragma once
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "archive.h"

namespace neko {

struct Image {
    int w = 0, h = 0;
    bool opaque = true;
    std::vector<uint8_t> rgba;
    size_t bytes() const { return rgba.size(); }
};
using ImageRef = std::shared_ptr<const Image>;

// Decoded pictures, most recently used kept. Scripts load the same backgrounds and bars over and over.
class Images {
public:
    explicit Images(const Files &files) : files_(files) {}
    // Null when the file is missing or isn't a picture this runtime reads.
    ImageRef get(const std::string &path);
    // The same picture, blurred by this radius in pixels. Kept like any other, since scripts hold a blur for a whole scene.
    ImageRef blurred(const std::string &path, int radius);

private:
    ImageRef decode(const std::string &path) const;
    ImageRef remember(const std::string &key, ImageRef image);
    const Files &files_;
    std::list<std::pair<std::string, ImageRef>> recent_;
    std::unordered_map<std::string, std::list<std::pair<std::string, ImageRef>>::iterator> byPath_;
    size_t held_ = 0;
};

}  // namespace neko
