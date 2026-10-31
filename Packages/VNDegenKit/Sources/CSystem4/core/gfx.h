// Sprites and drawing on the CPU. A sprite is a picture (pixels plus an alpha map) with a position, a Z order, a
// blend rate and a text layer of its own; the screen is the sprites drawn in Z order (then sprite number), as the
// SACT2 manual describes. Text is drawn into the text layer with the fonts in font.h.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "cg.h"

namespace s4 {

struct Surface {
    int w = 0, h = 0;
    bool hasAlpha = false;       // without an alpha map the pixels are opaque
    std::vector<uint8_t> px;     // RGBA, the A byte is the alpha map

    void create(int width, int height, bool alpha);
    uint8_t *at(int x, int y) { return px.data() + (size_t(y) * w + x) * 4; }
    const uint8_t *at(int x, int y) const { return px.data() + (size_t(y) * w + x) * 4; }
    void fill(int x, int y, int fw, int fh, int r, int g, int b, int a);  // a < 0 leaves the alpha map
};

struct TextStyle {
    int r = 255, g = 255, b = 255;
    int size = 16;
    int weight = 400;            // FW_NORMAL; 700 and up is bold
    int face = 0;                // 0 gothic, 1 mincho
    int shadowL = 0, shadowU = 0, shadowR = 0, shadowD = 0;
    int sr = 0, sg = 0, sb = 0;
    float boldWeight = 0, edgeWeight = 0;
    bool gameFont = false;       // draw with the game's own font library (font.h) where it has the character
    int er = 0, eg = 0, eb = 0;
};

// Draws text in CP932 into dst's RGBA at (x, y) as the top of the line, blending over what is there. Returns the
// width drawn, character spacing included.
int drawText(Surface &dst, int x, int y, const std::string &sjisText, const TextStyle &st, int charSpace);
int textWidth(const std::string &sjisText, const TextStyle &st, int charSpace);

struct Sprite {
    bool used = false;
    std::shared_ptr<const Image> cg;   // a CG set with SP_SetCG, shared with the cache until drawn on
    int cgNumber = 0;                  // that CG's number, for debugging
    Surface own;                       // a sprite made with SP_Create, or a CG copied before drawing on it
    bool hasOwn = false;
    int x = 0, y = 0, z = 0;
    bool show = true;
    int blendRate = 255;
    int drawMethod = 0;                // 0 normal, 1 screen, 2 multiply
    int brightness = 255;
    Surface text;                      // created on the first text draw
    int textHomeX = 0, textHomeY = 0, textX = 0, textY = 0, lineSpace = 0, charSpace = 0;
    int lineHeight = 0;                // tallest text on the current line

    int width() const { return hasOwn ? own.w : cg ? cg->w : 0; }
    int height() const { return hasOwn ? own.h : cg ? cg->h : 0; }
    bool hasAlpha() const { return hasOwn ? own.hasAlpha : cg ? cg->hasAlpha : false; }
    const uint8_t *pixels() const { return hasOwn ? own.px.data() : cg ? cg->rgba.data() : nullptr; }
    Surface &writable();               // the picture as a surface of its own, copying a shared CG first
};

class Sprites {
public:
    Sprite &get(int n) { return list_[n]; }
    Sprite *find(int n) {
        auto it = list_.find(n);
        return it == list_.end() || !it->second.used ? nullptr : &it->second;
    }
    void remove(int n) { list_.erase(n); }
    void clear() { list_.clear(); }
    int unused(int min) const;
    std::map<int, Sprite> &all() { return list_; }
    // Draws every shown sprite into the frame, back to front.
    void compose(std::vector<uint8_t> &frame, int w, int h, int offX, int offY);

private:
    std::map<int, Sprite> list_;
};

// Blends src over dst with the alpha map and an overall rate, by draw method. Clips to dst.
void blit(uint8_t *dst, int dw, int dh, int dx, int dy, const uint8_t *src, int sw, int sh, bool srcAlpha, int rate,
          int method, int brightness = 255);

}  // namespace s4
