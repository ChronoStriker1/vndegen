#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace wolf {

Renderer::Renderer(const Game &game)
    : game_(game), images_(game.files(), game.log), map_(images_, game), text_(game, images_) {}

TextStyle Renderer::baseStyle() const {
    TextStyle s;
    // Wolf sizes message text to the screen: 18 pixels at 640 by 480.
    s.size = std::max(12, int(std::lround(game_.screenHeight() * 18.0 / 480)));
    const auto &settings = game_.gameDat().settings;
    if (settings.size() > 11) s.lineSpacing = settings[11];
    if (settings.size() > 10) s.letterSpacing = 0;
    return s;
}

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

void Renderer::drawPicture(Canvas &canvas, int number, const Picture &p) {
    const Bitmap *img = nullptr;
    BitmapPtr file;
    if (p.kind == PictureKind::File) {
        file = images_.get(p.source);
        img = file.get();
    } else if (p.kind == PictureKind::Text) {
        auto &cached = pictureText_[number];
        if (cached.generation != p.generation) {
            cached.bitmap = text_.render(p.source, baseStyle()).bitmap;
            cached.generation = p.generation;
        }
        img = &cached.bitmap;
    } else if (p.kind == PictureKind::Window) {
        file = images_.get(p.source);
        float w = float(p.windowW ? p.windowW : 100), h = float(p.windowH ? p.windowH : 100);
        float x = float(p.x.value()), y = float(p.y.value());
        if (p.anchor == 1) {
            x -= w / 2;
            y -= h / 2;
        }
        drawWindow(canvas, *file, {x, y, w, h}, float(std::clamp(p.opacity.value(), 0.0, 255.0) / 255));
        return;
    }
    if (!img || img->empty()) return;
    if (p.flickerFrames > 0 && (p.flickerFrames / std::max(1, p.flickerInterval)) % 2) return;
    float opacity = float(std::clamp(p.opacity.value(), 0.0, 255.0) / 255);
    if (opacity <= 0) return;
    int divX = std::max(1, p.divX), divY = std::max(1, p.divY);
    int cellW = img->w / divX, cellH = img->h / divY;
    int cell = std::clamp(p.pattern - 1, 0, divX * divY - 1);
    RectI from{cell % divX * cellW, cell / divX * cellH, cellW, cellH};
    float w = cellW * float(p.zoomX.value() / 100), h = cellH * float(p.zoomY.value() / 100);
    float x = float(p.x.value() + p.shakeOffset), y = float(p.y.value());
    switch (p.anchor) {
        case 1: x -= w / 2; y -= h / 2; break;
        case 2: y -= h; break;
        case 3: x -= w; break;
        case 4: x -= w; y -= h; break;
    }
    DrawOptions o;
    o.opacity = opacity;
    o.r = float(p.r.value() / 100);
    o.g = float(p.g.value() / 100);
    o.b = float(p.b.value() / 100);
    o.blend = p.blend == 1 ? Blend::Add : p.blend == 2 ? Blend::Subtract : p.blend == 3 ? Blend::Multiply : Blend::Normal;
    o.angle = p.angle.value();
    canvas.draw(*img, from, {x, y, w, h}, o);
}

void Renderer::drawMessage(Canvas &canvas) {
    const MessageState &m = game_.message();
    if (!m.active) return;
    TextStyle style = baseStyle();
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
    canvas.draw(block.bitmap, textX, box.y + pad);

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
    TextStyle style = baseStyle();
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
    drawWindow(canvas, *images_.get(game_.windowSkin()), box, 0.9f);
    BitmapPtr cursor = images_.get(game_.cursorSkin());
    float blink = 0.6f + 0.4f * float(std::sin(game_.frameCount() * 0.1));
    if (!cursor->empty())
        drawWindow(canvas, *cursor, {box.x + pad, box.y + pad + c.cursor * lineH, w - pad * 2, lineH}, blink);
    for (size_t i = 0; i < lines.size(); i++) canvas.draw(lines[i].bitmap, box.x + pad * 2, box.y + pad + i * lineH);
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
    map_.draw(canvas);
    for (auto &[n, p] : game_.pictures()) drawPicture(canvas, n, p);
    auto tint = game_.screenColor();
    canvas.tint(float(tint[0] / 100), float(tint[1] / 100), float(tint[2] / 100));

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
