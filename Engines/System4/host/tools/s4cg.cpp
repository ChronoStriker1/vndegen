// Decodes a picture from an ALD archive to PNG. Usage: s4cg <archive.ald> <number> <out.png> [raw-out]
// With raw-out, the entry's stored bytes are written there as well.
#include <cstdio>
#include <cstdlib>

#include "archive.h"
#include "cg.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

int main(int argc, char **argv) {
    if (argc < 4) return 2;
    auto a = s4::AldArchive::open(argv[1]);
    const s4::ArchiveEntry *e = a ? a->find(std::atoi(argv[2])) : nullptr;
    if (!e) { std::fprintf(stderr, "no entry\n"); return 1; }
    std::vector<uint8_t> d;
    a->read(*e, d);
    if (argc > 4)
        if (FILE *f = std::fopen(argv[4], "wb")) { std::fwrite(d.data(), 1, d.size(), f); std::fclose(f); }
    s4::Image img;
    if (!s4::decodeImage(d.data(), d.size(), img)) { std::fprintf(stderr, "decode failed: %s\n", e->name.c_str()); return 1; }
    std::printf("%s %dx%d at %d,%d alpha %d\n", e->name.c_str(), img.w, img.h, img.x, img.y, img.hasAlpha);
    return stbi_write_png(argv[3], img.w, img.h, 4, img.rgba.data(), img.w * 4) ? 0 : 1;
}
