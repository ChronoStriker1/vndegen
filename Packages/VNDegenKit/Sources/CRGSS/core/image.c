#include "rgss.h"

#include <stdlib.h>
#include <string.h>

// Internal linkage, since other engines in the app compile their own stb copies.
#define STB_IMAGE_STATIC
#define STB_IMAGE_WRITE_STATIC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "../third_party/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../third_party/stb_image_write.h"
#pragma GCC diagnostic pop

bool image_alloc(Image *image, int w, int h) {
    image->w = w;
    image->h = h;
    image->px = calloc((size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1), sizeof(Pixel));
    return image->px != NULL;
}

void image_free(Image *image) {
    free(image->px);
    image->px = NULL;
    image->w = image->h = 0;
}

bool image_decode(const uint8_t *data, size_t size, Image *out) {
    int w, h, channels;
    stbi_uc *pixels = stbi_load_from_memory(data, (int)size, &w, &h, &channels, 4);
    if (!pixels) return false;
    out->w = w;
    out->h = h;
    out->px = (Pixel *)pixels;
    return true;
}

bool image_write_png(const Image *image, const char *path) {
    return stbi_write_png(path, image->w, image->h, 4, image->px, image->w * 4) != 0;
}
