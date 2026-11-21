#include "canvas.h"
#include "render.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

int main() {
    wolf::Picture p;
    p.kind = wolf::PictureKind::File;
    p.angle.set(300, 0);

    wolf::Bitmap src(200, 150);
    for (auto &v : src.px) v = 255;
    wolf::Bitmap frame(800, 600);
    wolf::Canvas canvas(frame);
    wolf::DrawOptions o;
    o.angle = wolf::pictureAngleDegrees(p);
    canvas.draw(src, {0, 0, 200, 150}, {300, 200, 200, 150}, o);

    int topX = -1, topY = -1, bottomX = -1, bottomY = -1;
    for (int y = 0; y < frame.h; y++)
        for (int x = 0; x < frame.w; x++) {
            if (frame.row(y)[x * 4 + 3] < 128) continue;
            if (topY < 0) topX = x, topY = y;
            bottomX = x, bottomY = y;
        }
    std::printf("top %d,%d bottom %d,%d\n", topX, topY, bottomX, bottomY);
    assert(std::abs(topX - 351) <= 2 && std::abs(topY - 161) <= 2);
    assert(std::abs(bottomX - 448) <= 2 && std::abs(bottomY - 389) <= 2);

    p.kind = wolf::PictureKind::Window;
    assert(wolf::pictureAngleDegrees(p) == 0);

    std::puts("picture angle: ok");
}
