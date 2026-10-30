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
    TextStyle baseStyle() const;

    const Game &game_;
    ImageCache images_;
    MapRenderer map_;
    TextRenderer text_;
    struct CachedText {
        uint64_t generation = 0;
        Bitmap bitmap;
    };
    std::map<int, CachedText> pictureText_;
    Bitmap frozen_;  // the screen as it was when a transition was prepared
    uint64_t frozenGeneration_ = 0;
};

}  // namespace wolf
