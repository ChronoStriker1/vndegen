// Draws the current map: its three tile layers, autotiles, and the characters on it, in Wolf's layering order.
#pragma once

#include "canvas.h"
#include "game.h"
#include "images.h"

#include <cstdint>

namespace wolf {

class MapRenderer {
public:
    MapRenderer(ImageCache &images, const Game &game);
    void draw(Canvas &canvas);

private:
    // Tile values: 0 is empty, 1 to 99999 index the tileset's base image, and 100000 or more are autotiles.
    void drawTile(Canvas &canvas, const Tileset &ts, int32_t value, int x, int y, const DrawOptions &o = {});
    void drawCharacter(Canvas &canvas, const Tileset &ts, const Character &c, int camX, int camY);
    uint32_t tileFlags(const Tileset &ts, int32_t value) const;

    ImageCache &images_;
    const Game &game_;
};

}  // namespace wolf
