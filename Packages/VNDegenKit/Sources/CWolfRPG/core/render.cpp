#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace wolf {

namespace {

void plot(Bitmap &b, int x, int y) {
    if (x < 0 || y < 0 || x >= b.w || y >= b.h) return;
    std::memset(b.row(y) + x * 4, 255, 4);
}

// A white line from (x0, y0) to (x1, y1), `thick` pixels wide.
void line(Bitmap &b, float x0, float y0, float x1, float y1, int thick) {
    int steps = std::max(1, int(std::ceil(std::max(std::abs(x1 - x0), std::abs(y1 - y0)))));
    for (int i = 0; i <= steps; i++) {
        int cx = int(std::lround(x0 + (x1 - x0) * i / steps)), cy = int(std::lround(y0 + (y1 - y0) * i / steps));
        for (int dy = 0; dy < thick; dy++)
            for (int dx = 0; dx < thick; dx++) plot(b, cx + dx - thick / 2, cy + dy - thick / 2);
    }
}

// Wolf's generated shapes, which a window picture shows when its file name is one of these codes: <SQUARE>,
// <LINE> or <LINE-n> (n pixels thick), <TRI-UP>, <TRI-DOWN>, <TRI-LEFT> and <TRI-RIGHT>, each with FRAME after it
// for the outline only, and <GRADX-aaa-bbb> and <GRADY-aaa-bbb>, gradients between two colours given as red, green
// and blue levels from 0 to 9. Shapes are white unless a gradient; the picture's colour tints them. False when the
// name is no shape.
bool makeShape(const std::string &name, int w, int h, Bitmap &out, float &offsetX, float &offsetY) {
    if (name.empty() || name[0] != '<') return false;
    size_t close = name.find('>');
    if (close == std::string::npos) return false;
    std::string code = name.substr(1, close - 1);
    bool frame = name.compare(close + 1, std::string::npos, "FRAME") == 0;
    offsetX = offsetY = 0;
    if (code == "LINE" || code.rfind("LINE-", 0) == 0) {
        // A line runs from the picture's position to that plus the size, which may be negative.
        int thick = code.size() > 5 ? std::max(1, std::atoi(code.c_str() + 5)) : 1;
        out = Bitmap(std::abs(w) + thick, std::abs(h) + thick);
        offsetX = float(std::min(0, w) - thick / 2);
        offsetY = float(std::min(0, h) - thick / 2);
        float x0 = float(thick / 2 - std::min(0, w)), y0 = float(thick / 2 - std::min(0, h));
        line(out, x0, y0, x0 + w, y0 + h, thick);
        return true;
    }
    if (w <= 0 || h <= 0) {
        out = Bitmap();
        return code == "SQUARE" || code.rfind("TRI-", 0) == 0 || code.rfind("GRAD", 0) == 0;
    }
    if (code == "SQUARE") {
        out = Bitmap(w, h);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                if (!frame || x == 0 || y == 0 || x == w - 1 || y == h - 1) plot(out, x, y);
        return true;
    }
    if (code.rfind("TRI-", 0) == 0) {
        std::string dir = code.substr(4);
        float fw = float(w - 1), fh = float(h - 1);
        float p[3][2];
        if (dir == "UP") p[0][0] = fw / 2, p[0][1] = 0, p[1][0] = 0, p[1][1] = fh, p[2][0] = fw, p[2][1] = fh;
        else if (dir == "DOWN") p[0][0] = 0, p[0][1] = 0, p[1][0] = fw, p[1][1] = 0, p[2][0] = fw / 2, p[2][1] = fh;
        else if (dir == "LEFT") p[0][0] = 0, p[0][1] = fh / 2, p[1][0] = fw, p[1][1] = 0, p[2][0] = fw, p[2][1] = fh;
        else if (dir == "RIGHT") p[0][0] = 0, p[0][1] = 0, p[1][0] = fw, p[1][1] = fh / 2, p[2][0] = 0, p[2][1] = fh;
        else return false;
        out = Bitmap(w, h);
        if (frame) {
            for (int i = 0; i < 3; i++) line(out, p[i][0], p[i][1], p[(i + 1) % 3][0], p[(i + 1) % 3][1], 1);
            return true;
        }
        auto side = [&](int i, float x, float y) {
            const float *a = p[i], *b = p[(i + 1) % 3];
            return (b[0] - a[0]) * (y - a[1]) - (b[1] - a[1]) * (x - a[0]);
        };
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++) {
                float s0 = side(0, float(x), float(y)), s1 = side(1, float(x), float(y)), s2 = side(2, float(x), float(y));
                if ((s0 >= 0 && s1 >= 0 && s2 >= 0) || (s0 <= 0 && s1 <= 0 && s2 <= 0)) plot(out, x, y);
            }
        return true;
    }
    if ((code.rfind("GRADX-", 0) == 0 || code.rfind("GRADY-", 0) == 0) && code.size() >= 13) {
        bool horizontal = code[4] == 'X';
        uint8_t from[3], to[3];
        for (int c = 0; c < 3; c++) {
            from[c] = uint8_t(std::clamp(code[6 + c] - '0', 0, 9) * 255 / 9);
            to[c] = uint8_t(std::clamp(code[10 + c] - '0', 0, 9) * 255 / 9);
        }
        out = Bitmap(w, h);
        int span = std::max(1, (horizontal ? w : h) - 1);
        for (int y = 0; y < h; y++) {
            uint8_t *row = out.row(y);
            for (int x = 0; x < w; x++) {
                int at = horizontal ? x : y;
                for (int c = 0; c < 3; c++) row[x * 4 + c] = uint8_t(from[c] + (to[c] - from[c]) * at / span);
                row[x * 4 + 3] = 255;
            }
        }
        return true;
    }
    return false;
}

}  // namespace

Renderer::Renderer(const Game &game)
    : game_(game), images_(game.files(), game.log), map_(images_, game), text_(game, &images_) {}

// Wolf's window skins are cut into a 3 by 3 grid: the corners stay, and the edges and middle stretch.
void Renderer::drawWindow(Canvas &canvas, const Bitmap &skin, RectF d, float opacity) {
    if (skin.empty()) {
        canvas.fill(d, 0, 0, 40, uint8_t(200 * opacity));
        return;
    }
    int cw = skin.w / 3, ch = skin.h / 3;
    float xs[4] = {d.x, d.x + cw, d.x + d.w - cw, d.x + d.w};
    float ys[4] = {d.y, d.y + ch, d.y + d.h - ch, d.y + d.h};
    int sx[4] = {0, cw, skin.w - cw, skin.w}, sy[4] = {0, ch, skin.h - ch, skin.h};
    DrawOptions o;
    o.opacity = opacity;
    for (int row = 0; row < 3; row++)
        for (int col = 0; col < 3; col++) {
            RectF to{xs[col], ys[row], xs[col + 1] - xs[col], ys[row + 1] - ys[row]};
            if (to.w > 0 && to.h > 0)
                canvas.draw(skin, {sx[col], sy[row], sx[col + 1] - sx[col], sy[row + 1] - sy[row]}, to, o);
        }
}

const Bitmap *Renderer::pictureImage(int number, const Picture &p, float &offsetX, float &offsetY) {
    CachedImage &cached = pictureImages_[number];
    int w = p.kind == PictureKind::Window ? p.windowW : 0, h = p.kind == PictureKind::Window ? p.windowH : 0;
    bool fromArrays = p.kind == PictureKind::File;
    // An image made from arrays is made when shown, so it follows the generation rather than the name.
    if (cached.kind != p.kind || cached.source != p.source || cached.w != w || cached.h != h ||
        (fromArrays && cached.generation != p.generation)) {
        cached.kind = p.kind;
        cached.source = p.source;
        cached.w = w;
        cached.h = h;
        cached.generation = p.generation;
        cached.offsetX = cached.offsetY = 0;
        if (fromArrays) {
            XYImage xy = resolveXYImage(p.source, game_);
            cached.bitmap = xy.w > 0 && xy.h > 0 ? Bitmap(xy.w, xy.h) : Bitmap();
            auto at = [](const XYArray *a, int x, int y, int fallback) {
                if (!a) return fallback;
                return x < int(a->size()) && y < int((*a)[x].size()) ? (*a)[x][y] : 0;
            };
            for (int y = 0; y < xy.h; y++) {
                uint8_t *row = cached.bitmap.row(y);
                for (int x = 0; x < xy.w; x++) {
                    row[x * 4 + 0] = uint8_t(std::clamp(at(xy.channels[1], x, y, 0), 0, 255));
                    row[x * 4 + 1] = uint8_t(std::clamp(at(xy.channels[2], x, y, 0), 0, 255));
                    row[x * 4 + 2] = uint8_t(std::clamp(at(xy.channels[3], x, y, 0), 0, 255));
                    row[x * 4 + 3] = uint8_t(std::clamp(at(xy.channels[0], x, y, 255), 0, 255));
                }
            }
        } else if (p.kind == PictureKind::Text) {
            TextBlock block = text_.render(p.source, baseTextStyle(game_));
            cached.bitmap = std::move(block.bitmap);
            cached.offsetX = cached.offsetY = -float(block.origin);
        } else if (!makeShape(p.source, w, h, cached.bitmap, cached.offsetX, cached.offsetY)) {
            // A window picture stretches its base image's 3 by 3 grid to the size the picture gives.
            cached.bitmap = w > 0 && h > 0 ? Bitmap(w, h) : Bitmap();
            if (!cached.bitmap.empty()) {
                Canvas c(cached.bitmap);
                drawWindow(c, *images_.get(p.source), {0, 0, float(w), float(h)}, 1);
            }
        }
    }
    offsetX = cached.offsetX;
    offsetY = cached.offsetY;
    return &cached.bitmap;
}

void Renderer::drawPicture(Canvas &canvas, int number, const Picture &p) {
    const Bitmap *img = nullptr;
    BitmapPtr file;
    float offsetX = 0, offsetY = 0;
    if (p.kind == PictureKind::File && !p.source.starts_with("<XY")) {
        file = images_.get(p.source);
        img = file.get();
    } else {
        img = pictureImage(number, p, offsetX, offsetY);
    }
    if (!img || img->empty()) return;
    if (p.flickerFrames > 0 && (p.flickerFrames / std::max(1, p.flickerInterval)) % 2) return;
    float opacity = float(std::clamp(p.opacity.value(), 0.0, 255.0) / 255);
    if (opacity <= 0) return;
    int divX = std::max(1, p.divX), divY = std::max(1, p.divY);
    int cellW = img->w / divX, cellH = img->h / divY;
    int cell = std::clamp(p.pattern - 1, 0, divX * divY - 1);
    RectI from{cell % divX * cellW, cell / divX * cellH, cellW, cellH};
    // A cut shows part of each pattern.
    if (p.cutW > 0 && p.cutH > 0) {
        from.x += p.cutX;
        from.y += p.cutY;
        cellW = from.w = p.cutW;
        cellH = from.h = p.cutH;
    }
    float zoomX = float(p.zoomX.value() / 100), zoomY = float(p.zoomY.value() / 100);
    // The anchor places the picture by its laid-out size; a text bitmap's margin and a line's offset come on top.
    float sizeW = cellW, sizeH = cellH;
    if (p.kind == PictureKind::Window && p.windowW > 0 && p.windowH > 0) sizeW = float(p.windowW), sizeH = float(p.windowH);
    else if (p.kind == PictureKind::Text) sizeW = float(p.imageW), sizeH = float(p.imageH);
    float w = cellW * zoomX, h = cellH * zoomY, aw = sizeW * zoomX, ah = sizeH * zoomY;
    float x = float(p.x.value() + p.shakeOffset + p.shiftX), y = float(p.y.value() + p.shiftY);
    switch (p.anchor) {
        case 1: x -= aw / 2; y -= ah / 2; break;
        case 2: y -= ah; break;
        case 3: x -= aw; break;
        case 4: x -= aw; y -= ah; break;
        case 5: x -= aw / 2; break;
    }
    x += offsetX * zoomX;
    y += offsetY * zoomY;
    DrawOptions o;
    o.opacity = opacity;
    float swing = 0;
    if (p.pulseInterval > 0) {
        int period = p.pulseInterval * 2, at = int(game_.frameCount() % uint64_t(period));
        swing = float(at < p.pulseInterval ? at : period - at) / p.pulseInterval;
    }
    o.r = float(p.r.value() + p.pulse[0] * swing) / 100;
    o.g = float(p.g.value() + p.pulse[1] * swing) / 100;
    o.b = float(p.b.value() + p.pulse[2] * swing) / 100;
    o.blend = p.blend == 1 ? Blend::Add : p.blend == 2 ? Blend::Subtract : p.blend == 3 ? Blend::Multiply : Blend::Normal;
    // Windows and shapes ignore the angle, as Wolf's manual says.
    o.angle = p.kind == PictureKind::Window ? 0 : p.angle.value();
    canvas.draw(*img, from, {x, y, w, h}, o);
}

void Renderer::drawMessage(Canvas &canvas) {
    const MessageState &m = game_.message();
    if (!m.active) return;
    TextStyle style = baseTextStyle(game_);
    float pad = std::max(8.0f, game_.screenHeight() / 48.0f);
    float h = float(text_.lineHeight(style) * 4) + pad * 2;
    RectF box{pad, game_.screenHeight() - h - pad, game_.screenWidth() - pad * 2, h};
    drawWindow(canvas, *images_.get(game_.windowSkin()), box, 0.85f);

    float textX = box.x + pad * 1.5f;
    if (m.face >= 0) {
        BitmapPtr face = images_.get(game_.faceGraphic(m.face));
        if (!face->empty()) {
            float fh = std::min(float(face->h), box.h - pad * 2);
            float fw = face->w * fh / face->h;
            canvas.draw(*face, {0, 0, face->w, face->h}, {box.x + pad, box.y + pad, fw, fh});
            textX = box.x + pad * 2 + fw;
        }
    }
    TextBlock block = text_.render(m.text, style, m.shown);
    canvas.draw(block.bitmap, textX - block.origin, box.y + pad - block.origin);

    // The pause icon waits at the bottom of the window once the text is all out.
    if (m.shown >= m.total) {
        BitmapPtr pause = images_.get(game_.pauseIcon());
        if (!pause->empty()) {
            int frames = std::max(1, game_.pauseIconFrames());
            int ph = pause->h / frames;
            int frame = int(game_.frameCount() / 6 % frames);
            canvas.draw(*pause, {0, frame * ph, pause->w, ph},
                        {box.x + box.w / 2 - pause->w / 2.0f, box.y + box.h - ph - 2, float(pause->w), float(ph)});
        }
    }
}

void Renderer::drawChoices(Canvas &canvas) {
    const ChoiceState &c = game_.choice();
    if (!c.active || c.visible.empty()) return;
    TextStyle style = baseTextStyle(game_);
    std::vector<TextBlock> lines;
    int maxW = 0;
    for (int i : c.visible) {
        lines.push_back(text_.render(c.items[i], style));
        maxW = std::max(maxW, lines.back().bitmap.w);
    }
    float pad = std::max(8.0f, game_.screenHeight() / 48.0f);
    float lineH = float(text_.lineHeight(style));
    float w = maxW + pad * 4, h = lineH * lines.size() + pad * 2;
    // Without a message the choices sit low on the left of the screen, as Wolf places them; with one, just above it.
    float x = game_.screenWidth() * 0.1f;
    float bottom = game_.screenHeight() * 0.87f;
    if (game_.message().active) bottom = game_.screenHeight() - (lineH * 4 + pad * 4);
    RectF box{x, bottom - h, w, h};
    // System variable 4 moves the choice window's top to a given height; -1 leaves Wolf's own placement.
    if (int top = game_.choiceTop(); top >= 0) box.y = float(top);
    game_.setChoiceBox(box.x + pad, box.y + pad, w - pad * 2, lineH);
    drawWindow(canvas, *images_.get(game_.windowSkin()), box, 0.9f);
    BitmapPtr cursor = images_.get(game_.cursorSkin());
    float blink = 0.6f + 0.4f * float(std::sin(game_.frameCount() * 0.1));
    if (!cursor->empty())
        drawWindow(canvas, *cursor, {box.x + pad, box.y + pad + c.cursor * lineH, w - pad * 2, lineH}, blink);
    for (size_t i = 0; i < lines.size(); i++)
        canvas.draw(lines[i].bitmap, box.x + pad * 2 - lines[i].origin, box.y + pad + i * lineH - lines[i].origin);
}

// The text being typed, at the place system variables 40 and 41 give, with a cursor bar after it.
void Renderer::drawTextEntry(Canvas &canvas) {
    const TextEntry &entry = game_.textEntry();
    if (!entry.active) return;
    TextStyle style = baseTextStyle(game_);
    TextBlock block = text_.render(entry.text + (game_.frameCount() / 30 % 2 ? " " : "_"), style);
    canvas.draw(block.bitmap, float(game_.systemNumber(40) - block.origin), float(game_.systemNumber(41) - block.origin));
}

void Renderer::draw(Bitmap &frame) {
    int w = game_.screenWidth(), h = game_.screenHeight();
    if (frame.w != w || frame.h != h) frame = Bitmap(w, h);
    const TransitionState &tr = game_.transition();

    // While a transition is prepared the player sees the screen as it was; the live scene changes underneath.
    if (tr.prepared && frozenGeneration_ == tr.generation && !frozen_.empty()) {
        frame = frozen_;
        return;
    }
    Canvas canvas(frame);
    canvas.clear(0, 0, 0);
    if (auto [rate, cx, cy] = game_.mapZoom(); std::abs(rate - 100) > 0.01) {
        // A zoomed map is drawn at its normal size first, then scaled around the zoom's centre.
        if (mapLayer_.w != w || mapLayer_.h != h) mapLayer_ = Bitmap(w, h);
        Canvas layer(mapLayer_);
        layer.clear(0, 0, 0);
        map_.draw(layer);
        float s = float(rate / 100);
        canvas.draw(mapLayer_, {0, 0, w, h}, {float(cx - cx * s), float(cy - cy * s), w * s, h * s});
    } else {
        map_.draw(canvas);
    }
    // The screen colour tints the map only: the PC engine shows pictures at their own colour over a screen it has
    // turned black (ANGEL QUEST's opening).
    auto tint = game_.screenColor();
    canvas.tint(float(tint[0] / 100), float(tint[1] / 100), float(tint[2] / 100));
    std::erase_if(pictureImages_, [&](auto &kv) { return !game_.pictures().count(kv.first); });
    for (auto &[n, p] : game_.pictures()) drawPicture(canvas, n, p);

    int shake = int(std::lround(game_.shake().offset));
    if (shake) {
        // Shake moves the whole picture sideways; the uncovered edge shows black.
        for (int y = 0; y < h; y++) {
            uint8_t *row = frame.row(y);
            if (shake > 0) {
                std::memmove(row + shake * 4, row, size_t(w - shake) * 4);
                std::memset(row, 0, size_t(shake) * 4);
            } else {
                std::memmove(row, row - shake * 4, size_t(w + shake) * 4);
                std::memset(row + (w + shake) * 4, 0, size_t(-shake) * 4);
            }
        }
    }
    drawMessage(canvas);
    drawChoices(canvas);
    drawTextEntry(canvas);

    if (tr.prepared) {
        frozen_ = frame;
        frozenGeneration_ = tr.generation;
    } else if (tr.frames > 0 && !frozen_.empty() && frozen_.w == w && frozen_.h == h) {
        // Crossfade from the frozen screen to the live one.
        DrawOptions o;
        o.opacity = 1.0f - float(tr.elapsed) / tr.frames;
        canvas.draw(frozen_, 0, 0, o);
    }
}

}  // namespace wolf
