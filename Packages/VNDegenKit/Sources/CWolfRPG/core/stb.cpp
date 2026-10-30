// Image decoding with stb_image (MIT or public domain), compiled static here so its names stay out of the app's
// other engines, which compile their own copies.
#include "images.h"

#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#include "../third_party/stb/stb_image.h"

#include <cstring>

namespace wolf {

bool decodeImage(const std::vector<uint8_t> &bytes, Bitmap &out) {
    int w, h, n;
    stbi_uc *px = stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &n, 4);
    if (!px) return false;
    out = Bitmap(w, h);
    std::memcpy(out.px.data(), px, out.px.size());
    stbi_image_free(px);
    return true;
}

bool imageSize(const std::vector<uint8_t> &bytes, int &w, int &h) {
    int n;
    return stbi_info_from_memory(bytes.data(), int(bytes.size()), &w, &h, &n) != 0;
}

}  // namespace wolf
