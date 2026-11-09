// Test Game 82's pictures, WebP under `.bmp` names, with difference pictures laid over the base they name.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "image.h"

namespace neko {

bool isWebP(const std::vector<uint8_t> &data);
// `picture` reads another picture by path, for a difference picture's base. False when the file can't be decoded.
bool decodeWebP(const std::vector<uint8_t> &data, const std::string &path,
                const std::function<ImageRef(const std::string &)> &picture, Image &out);

}  // namespace neko
