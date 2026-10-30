// Loads game images into CPU bitmaps and keeps them for reuse.
#pragma once

#include "canvas.h"
#include "files.h"

#include <functional>
#include <map>
#include <string>

namespace wolf {

// PNG, JPEG and BMP into RGBA, and an image's size without decoding it (stb.cpp).
bool decodeImage(const std::vector<uint8_t> &bytes, Bitmap &out);
bool imageSize(const std::vector<uint8_t> &bytes, int &w, int &h);

class ImageCache {
public:
    ImageCache(const GameFiles &files, std::function<void(const std::string &)> log);
    // Returns an empty bitmap when the file is missing or can't be decoded, and logs it once.
    BitmapPtr get(const std::string &path);

private:
    const GameFiles &files_;
    std::function<void(const std::string &)> log_;
    std::map<std::string, BitmapPtr> images_;
};

}  // namespace wolf
