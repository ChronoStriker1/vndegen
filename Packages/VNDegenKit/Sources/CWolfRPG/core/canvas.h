// CPU drawing: the runtime composites every frame into an RGBA bitmap that the host presents. On Apple platforms
// the host shows it through Metal; the Linux test host writes it to PNG files.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace wolf {

// 8-bit RGBA, straight (not premultiplied) alpha, rows top to bottom, in memory order R G B A.
struct Bitmap {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    Bitmap() = default;
    Bitmap(int width, int height) : w(width), h(height), px(size_t(width) * height * 4, 0) {}
    uint8_t *row(int y) { return px.data() + size_t(y) * w * 4; }
    const uint8_t *row(int y) const { return px.data() + size_t(y) * w * 4; }
    bool empty() const { return w <= 0 || h <= 0; }
};

using BitmapPtr = std::shared_ptr<const Bitmap>;

struct RectF {
    float x = 0, y = 0, w = 0, h = 0;
};

struct RectI {
    int x = 0, y = 0, w = 0, h = 0;
};

enum class Blend { Normal, Add, Subtract, Multiply };

struct DrawOptions {
    float opacity = 1;                 // 0 to 1, times the source's own alpha
    float r = 1, g = 1, b = 1;         // colour multipliers, 0 to 2; 1 leaves the colour as it is
    Blend blend = Blend::Normal;
    double angle = 0;                  // degrees clockwise, about the centre of the destination
    bool smooth = false;               // bilinear filtering; Wolf draws pixel art with nearest-neighbour by default
};

// A frame being drawn. Coordinates are in the frame's pixels; anything outside is clipped.
class Canvas {
public:
    explicit Canvas(Bitmap &target) : t_(target) {}
    Bitmap &target() { return t_; }

    void clear(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255);
    // Draws src's rectangle `from` scaled into `to`.
    void draw(const Bitmap &src, RectI from, RectF to, const DrawOptions &o = {});
    void draw(const Bitmap &src, float x, float y, const DrawOptions &o = {}) {
        draw(src, {0, 0, src.w, src.h}, {x, y, float(src.w), float(src.h)}, o);
    }
    void fill(RectF to, uint8_t r, uint8_t g, uint8_t b, uint8_t a, Blend blend = Blend::Normal);
    // Multiplies every pixel's colour by r, g, b (0 to 2), as Wolf's change screen colour does.
    void tint(float r, float g, float b);

private:
    Bitmap &t_;
};

}  // namespace wolf
