#include "maprender.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace wolf {

namespace {

// Tileset passability flags that change drawing.
constexpr uint32_t kAboveCharacters = 0x10;  // drawn over every character
constexpr uint32_t kHideBehind = 0x100;      // drawn over characters on its row or above it, under those below

// Frames each autotile animation frame stays on screen. The original engine under Wine changed frames every 324 ms
// on average.
constexpr uint64_t kAutotileFrameTicks = 20;

int floorDiv(int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

Blend blendFor(int blend) {
    switch (blend) {
        case 1: return Blend::Add;
        case 2: return Blend::Subtract;
        case 3: return Blend::Multiply;
        default: return Blend::Normal;
    }
}

}  // namespace

MapRenderer::MapRenderer(ImageCache &images, const Game &game) : images_(images), game_(game) {}

uint32_t MapRenderer::tileFlags(const Tileset &ts, int32_t value) const { return tilePassability(ts, value); }

void MapRenderer::drawTile(Canvas &canvas, const Tileset &ts, int32_t value, int x, int y, const DrawOptions &o) {
    int tile = game_.tileSize();
    if (value <= 0) return;
    if (value < 100000) {
        BitmapPtr base = images_.get(ts.baseFile);
        if (base->empty()) return;
        int cols = std::max(1, base->w / tile);
        canvas.draw(*base, {value % cols * tile, value / cols * tile, tile, tile},
                    {float(x), float(y), float(tile), float(tile)}, o);
        return;
    }
    // An autotile image is one tile wide per animation frame, with five variants stacked downwards. Each digit
    // picks the variant for one quarter, in the order top left, top right, bottom left, bottom right, and the
    // quarter comes from the same corner of that variant.
    int id = value / 100000;
    if (id < 2 || size_t(id - 2) >= ts.autotileFiles.size()) return;
    BitmapPtr img = images_.get(ts.autotileFiles[id - 2]);
    if (img->empty()) return;
    int frames = std::max(1, img->w / tile);
    int frame = int(game_.frameCount() / kAutotileFrameTicks % uint64_t(frames));
    int half = tile / 2;
    int digits[4] = {value / 1000 % 10, value / 100 % 10, value / 10 % 10, value % 10};
    for (int q = 0; q < 4; q++) {
        int qx = q % 2 * half, qy = q / 2 * half;
        canvas.draw(*img, {frame * tile + qx, std::min(digits[q], 4) * tile + qy, half, half},
                    {float(x + qx), float(y + qy), float(half), float(half)}, o);
    }
}

// A character image holds one cell per animation pattern across and per direction down: down, left, right and
// up, then down-left, down-right, up-left and up-right when the game uses eight-direction images. A cell is drawn
// centred on its tile horizontally with its bottom on the tile's bottom edge.
void MapRenderer::drawCharacter(Canvas &canvas, const Tileset &ts, const Character &c, int camX, int camY) {
    if (c.opacity <= 0) return;
    int tile = game_.tileSize();
    int tx = int(std::lround(c.x * tile)) - camX, ty = int(std::lround(c.y * tile)) - camY - c.lift;
    DrawOptions o;
    o.opacity = float(std::min(c.opacity, 255)) / 255;
    o.blend = blendFor(c.blend);
    if (c.graphic.empty()) {
        drawTile(canvas, ts, c.chip, tx, ty, o);
        return;
    }
    BitmapPtr img = images_.get(c.graphic);
    if (img->empty()) return;
    const auto &settings = game_.gameDat().settings;
    bool eight = settings.size() > 1 && settings[1] == 8;
    int patterns = settings.size() > 7 && settings[7] == 5 ? 5 : 3;
    int cellW = img->w / patterns, cellH = img->h / (eight ? 8 : 4);
    if (cellW <= 0 || cellH <= 0) return;
    int row;
    switch (c.direction) {
        case 4: row = 1; break;
        case 6: row = 2; break;
        case 8: row = 3; break;
        // Four-direction images show diagonal movement with the side views.
        case 1: row = eight ? 4 : 1; break;
        case 3: row = eight ? 5 : 2; break;
        case 7: row = eight ? 6 : 1; break;
        case 9: row = eight ? 7 : 2; break;
        default: row = 0; break;
    }
    int col = std::clamp(c.pattern, 0, patterns - 1);
    canvas.draw(*img, {col * cellW, row * cellH, cellW, cellH},
                {float(tx + (tile - cellW) / 2), float(ty + tile - cellH), float(cellW), float(cellH)}, o);
}

void MapRenderer::draw(Canvas &canvas) {
    const Map *map = game_.map();
    if (!map || map->tileset < 0 || size_t(map->tileset) >= game_.tilesets().size()) return;
    const Tileset &ts = game_.tilesets()[map->tileset];
    int tile = game_.tileSize();
    int camX = int(std::floor(game_.cameraX())), camY = int(std::floor(game_.cameraY()));
    int x0 = std::max(0, floorDiv(camX, tile)), y0 = std::max(0, floorDiv(camY, tile));
    int x1 = std::min(map->width - 1, floorDiv(camX + game_.screenWidth() - 1, tile));
    int y1 = std::min(map->height - 1, floorDiv(camY + game_.screenHeight() - 1, tile));

    // Characters and hide-behind tiles are drawn together, ordered by the bottom edge of what they stand on.
    struct Sprite {
        double feet;  // in map pixels
        const Character *chara = nullptr;
        int32_t value = 0;  // a tile, when chara is null
        int x = 0, y = 0;
    };
    struct Tile {
        int32_t value;
        int x, y;
    };
    std::vector<Sprite> sprites;
    std::vector<Tile> above;

    for (int layer = 0; layer < map->layers; layer++) {
        for (int ty = y0; ty <= y1; ty++) {
            for (int tx = x0; tx <= x1; tx++) {
                int32_t v = map->tile(layer, tx, ty);
                if (v <= 0 || v / 100000 == 1) continue;  // empty, or the empty autotile slot
                int sx = tx * tile - camX, sy = ty * tile - camY;
                uint32_t flags = tileFlags(ts, v);
                if (flags & kAboveCharacters) above.push_back({v, sx, sy});
                // After the characters standing on its row, before those below it.
                else if (flags & kHideBehind) sprites.push_back({(ty + 1) * double(tile) + 0.5, nullptr, v, sx, sy});
                else drawTile(canvas, ts, v, sx, sy);
            }
        }
    }

    std::vector<const Character *> top;
    for (const Character *c : game_.characters()) {
        if (!c->visible || (c->graphic.empty() && c->chip <= 0)) continue;
        if (c->above) top.push_back(c);
        else sprites.push_back({(c->y + 1) * tile, c});
    }
    std::stable_sort(sprites.begin(), sprites.end(), [](const Sprite &a, const Sprite &b) { return a.feet < b.feet; });
    for (auto &s : sprites) {
        if (s.chara) drawCharacter(canvas, ts, *s.chara, camX, camY);
        else drawTile(canvas, ts, s.value, s.x, s.y);
    }
    for (auto &t : above) drawTile(canvas, ts, t.value, t.x, t.y);
    for (const Character *c : top) drawCharacter(canvas, ts, *c, camX, camY);
}

}  // namespace wolf
