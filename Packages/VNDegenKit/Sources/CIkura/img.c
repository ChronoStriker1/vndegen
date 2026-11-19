#include "ikura.h"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "third_party/stb_image.h"

Image *image_new(int w, int h) {
    Image *im = ik_alloc(sizeof *im);
    im->w = w;
    im->h = h;
    im->px = ik_alloc(sizeof(u32) * (size_t)(w > 0 ? w : 1) * (size_t)(h > 0 ? h : 1));
    return im;
}

void image_free(Image *im) {
    if (!im) return;
    free(im->px);
    free(im->cells);
    free(im);
}

Image *image_decode(const u8 *d, size_t n) {
    u8 *png = NULL;
    const u8 *src = d;
    size_t len = n;
    Cell *cells = NULL;
    int cellCount = 0;
    if (n >= 0x24 && !memcmp(d, "GGPFAIKE", 8)) {
        u32 tab = rd32(d + 0x1c), tabSize = rd32(d + 0x20);
        if (tab >= 0x24 && tabSize >= 12 && (size_t)tab + tabSize <= n) {
            cellCount = (int)(tabSize / 12);
            cells = ik_alloc(sizeof(Cell) * (size_t)cellCount);
            for (int i = 0; i < cellCount; i++) {
                const u8 *c = d + tab + 12 * (size_t)i;
                cells[i] = (Cell){rd16(c), rd16(c + 2), rd16(c + 4), rd16(c + 6), rd16(c + 8), rd16(c + 10)};
            }
        }
        u8 key[8];
        for (int i = 0; i < 8; i++) key[i] = d[i] ^ d[i + 12];
        u32 off = rd32(d + 0x14), sz = rd32(d + 0x18);
        if ((size_t)off + sz > n) {
            free(cells);
            return NULL;
        }
        png = ik_alloc(sz);
        for (u32 i = 0; i < sz; i += 8)
            for (u32 k = 0; k < 8 && i + k < sz; k++) png[i + k] = d[off + i + k] ^ key[k];
        src = png;
        len = sz;
    }
    int w, h, comp;
    u8 *rgba = stbi_load_from_memory(src, (int)len, &w, &h, &comp, 4);
    free(png);
    if (!rgba) {
        free(cells);
        return NULL;
    }
    Image *im = image_new(w, h);
    memcpy(im->px, rgba, (size_t)w * (size_t)h * 4);
    im->cells = cells;
    im->cellCount = cellCount;
    stbi_image_free(rgba);
    return im;
}
