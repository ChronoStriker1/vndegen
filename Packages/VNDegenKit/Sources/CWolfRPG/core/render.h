// Draws a Game into an RGBA frame on the CPU: the map, pictures, screen effects, and the message and choice windows.
#pragma once

#include "canvas.h"
#include "game.h"
#include "images.h"
#include "maprender.h"
#include "text.h"

#include <map>
#include <string>

namespace wolf {

class Renderer {
public:
    explicit Renderer(const Game &game);
    // Draws the current frame into `frame`, resizing it to the game's screen.
    void draw(Bitmap &frame);

private:
    void drawPicture(Canvas &canvas, int number, const Picture &p);
    void drawWindow(Canvas &canvas, const Bitmap &skin, RectF r, float opacity);
    void drawMessage(Canvas &canvas);
    void drawChoices(Canvas &canvas);
    // The image a text or window picture shows, made when its source or size changes. `offset` is where its top
    // left sits from the picture's position, for lines drawn up or to the left.
    const Bitmap *pictureImage(int number, const Picture &p, float &offsetX, float &offsetY);

    const Game &game_;
    ImageCache images_;
    MapRenderer map_;
    TextRenderer text_;
    // Keyed by what the image shows rather than by the picture's generation: the standard systems show the same text
    // and windows again every frame.
    struct CachedImage {
        PictureKind kind = PictureKind::None;
        std::string source;
        int w = 0, h = 0;
        float offsetX = 0, offsetY = 0;
        Bitmap bitmap;
    };
    std::map<int, CachedImage> pictureImages_;
    Bitmap mapLayer_;  // the map before a map zoom scales it
    Bitmap frozen_;  // the screen as it was when a transition was prepared
    uint64_t frozenGeneration_ = 0;
};

}  // namespace wolf
