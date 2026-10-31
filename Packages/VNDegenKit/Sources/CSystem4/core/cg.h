// Pictures: AliceSoft's QNT and AJP formats, and PNG, decoded to 8-bit RGBA.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace s4 {

struct Image {
    int w = 0, h = 0;
    int x = 0, y = 0;          // QNT's stored offset
    bool hasAlpha = false;
    std::vector<uint8_t> rgba;  // w * h * 4, straight alpha
    // A WebP with an "OVER" chunk is drawn over another picture of the same size, this number: where it is magenta
    // (255, 0, 255), the base shows. -1 for a picture that stands alone.
    int overBase = -1;
};

// Decodes a picture by its contents. Returns false when the format is unknown or the data is damaged.
bool decodeImage(const uint8_t *data, size_t size, Image &out);
bool decodeQNT(const uint8_t *data, size_t size, Image &out);
bool decodeAJP(const uint8_t *data, size_t size, Image &out);
bool decodePMS(const uint8_t *data, size_t size, Image &out);  // 8-bit only, its index as alpha

}  // namespace s4
