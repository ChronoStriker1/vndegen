// The frame the runtime composites on the CPU: RGBA8, opaque.
#pragma once
#include <cstdint>
#include <vector>

#include "image.h"

namespace neko {

struct Canvas {
    int w = 0, h = 0;
    std::vector<uint8_t> px;
    void resize(int width, int height) {
        w = width;
        h = height;
        px.assign(size_t(w) * h * 4, 0);
    }
    void clear();
};

// Draws `image` with its centre at (cx, cy), scaled and rotated (radians, clockwise on screen) about its centre.
void drawImage(Canvas &dst, const Image &image, float cx, float cy, float sx, float sy, float radians, float alpha);
// Draws part of an image with its top-left corner at (x, y), unscaled.
void drawPart(Canvas &dst, const Image &image, int x, int y, int partX, int partY, int partW, int partH, float alpha);
// Blends `from` over dst at `amount` 0..1 of from.
void blendFrame(Canvas &dst, const Canvas &from, int amount256);
// Mixes in a blurred copy, made by shrinking by `divisor` and growing back, at amount256 of 256.
void softenFrame(Canvas &dst, int divisor, int amount256);
// Fills with a colour at `amount256` of 256.
void tintFrame(Canvas &dst, int r, int g, int b, int amount256);
// A coverage mask drawn in one colour, at (x, y).
void drawCoverage(Canvas &dst, const uint8_t *coverage, int w, int h, int x, int y, int r, int g, int b, int alpha256);

}  // namespace neko
