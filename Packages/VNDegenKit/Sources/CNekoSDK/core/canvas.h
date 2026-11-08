// The frame the runtime composites on the CPU: RGBA8, opaque.
#pragma once
#include <cstdint>
#include <functional>
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
    // Copies a rectangle to or from `saved`, clipped to the canvas, so a small overlay can be redrawn alone.
    void saveRect(int x, int y, int w, int h, std::vector<uint8_t> &saved) const;
    void restoreRect(int x, int y, int w, int h, const std::vector<uint8_t> &saved);
};

// Runs body(row0, row1) over ranges that cover 0..rows, across the CPU's cores when `pixels` is enough to be worth it. Each
// range is a separate set of rows, so bodies that write only their own rows don't overlap. Returns when all have finished.
void forRows(int rows, size_t pixels, const std::function<void(int, int)> &body);

// Draws `image` with its centre at (cx, cy), scaled and rotated (radians, clockwise on screen) about its centre.
// `rowShift`, when given, holds a sideways shift in pixels for each row of dst.
// `additive` adds the image's colour to what's there, as the PC game draws effects painted on black.
void drawImage(Canvas &dst, const Image &image, float cx, float cy, float sx, float sy, float radians, float alpha, const std::vector<int> *rowShift = nullptr, bool additive = false);
// Draws a whole image unscaled with its top-left corner at (left, top), which may fall between pixels: each pixel mixes
// the four source pixels around it, so a picture can move by a fraction of a pixel.
void drawShifted(Canvas &dst, const Image &image, float left, float top, float alpha);
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
