#include "render.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

namespace wolf {

Renderer::Renderer(SDL_Renderer *renderer, const Game &game)
    : r_(renderer), game_(game), images_(renderer, game.files(), game.log) {}

Renderer::~Renderer() {
    for (auto &[k, i] : textCache_) SDL_DestroyTexture(i.texture);
    if (font_) TTF_CloseFont(font_);
}

// Uses the font Game.dat names when the game ships it, and a fallback font otherwise.
TTF_Font *Renderer::font() {
    if (fontTried_) return font_;
    fontTried_ = true;
    float size = std::max(12.0f, game_.gameDat().tileSize() * 0.55f);
    const std::string &want = game_.gameDat().fonts[0];
    std::error_code ec;
    for (auto &entry : fs::directory_iterator(game_.files().root(), ec)) {
        auto ext = entry.path().extension().string();
        for (auto &c : ext) c = char(std::tolower(c));
        if (ext != ".ttf" && ext != ".otf" && ext != ".ttc") continue;
        TTF_Font *f = TTF_OpenFont(entry.path().c_str(), size);
        if (!f) continue;
        const char *family = TTF_GetFontFamilyName(f);
        if ((family && want == family) || entry.path().stem().string() == want) {
            font_ = f;
            return font_;
        }
        TTF_CloseFont(f);
    }
    if (const char *fallback = std::getenv("WOLF_FALLBACK_FONT")) font_ = TTF_OpenFont(fallback, size);
    if (!font_) game_.log("no font for " + want);
    return font_;
}

Image Renderer::text(const std::string &s, std::array<uint8_t, 3> color) {
    std::string key = s + '\x01' + std::to_string(color[0] << 16 | color[1] << 8 | color[2]);
    auto it = textCache_.find(key);
    if (it != textCache_.end()) return it->second;
    Image img;
    if (TTF_Font *f = font(); f && !s.empty()) {
        SDL_Color c{color[0], color[1], color[2], 255};
        if (SDL_Surface *surf = TTF_RenderText_Blended(f, s.c_str(), 0, c)) {
            img.texture = SDL_CreateTextureFromSurface(r_, surf);
            img.w = surf->w;
            img.h = surf->h;
            SDL_DestroySurface(surf);
        }
    }
    textCache_[key] = img;
    return img;
}

// Wolf's window skins are cut into a 3 by 3 grid: corners stay, edges and the middle stretch.
void Renderer::drawWindow(const Image &skin, SDL_FRect d, float alpha) {
    if (!skin.texture) return;
    float cw = skin.w / 3.0f, ch = skin.h / 3.0f;
    SDL_SetTextureAlphaModFloat(skin.texture, alpha);
    float xs[4] = {d.x, d.x + cw, d.x + d.w - cw, d.x + d.w};
    float ys[4] = {d.y, d.y + ch, d.y + d.h - ch, d.y + d.h};
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            SDL_FRect src{col * cw, row * ch, cw, ch};
            SDL_FRect dst{xs[col], ys[row], xs[col + 1] - xs[col], ys[row + 1] - ys[row]};
            if (dst.w > 0 && dst.h > 0) SDL_RenderTexture(r_, skin.texture, &src, &dst);
        }
    }
    SDL_SetTextureAlphaModFloat(skin.texture, 1.0f);
}

void Renderer::drawPicture(int number, const Picture &p) {
    Image img;
    switch (p.kind) {
        case PictureKind::File: img = images_.get(p.source); break;
        case PictureKind::Text: img = text(p.source, {255, 255, 255}); break;
        default: return;
    }
    if (!img.texture) return;
    float opacity = float(std::clamp(p.opacity.value(), 0.0, 255.0) / 255.0);
    if (opacity <= 0) return;
    int cellW = img.w / std::max(1, p.divX), cellH = img.h / std::max(1, p.divY);
    int cell = std::max(0, p.pattern - 1);
    SDL_FRect src{float(cell % p.divX * cellW), float(cell / p.divX % p.divY * cellH), float(cellW), float(cellH)};
    float w = cellW * float(p.zoomX.value() / 100), h = cellH * float(p.zoomY.value() / 100);
    float x = float(p.x.value()), y = float(p.y.value());
    switch (p.anchor) {
        case 1: x -= w / 2; y -= h / 2; break;
        case 2: y -= h; break;
        case 3: x -= w; break;
        case 4: x -= w; y -= h; break;
    }
    SDL_FRect dst{x, y, w, h};
    SDL_SetTextureAlphaModFloat(img.texture, opacity);
    SDL_SetTextureColorModFloat(img.texture, float(std::min(p.r.value(), 100.0) / 100),
                                float(std::min(p.g.value(), 100.0) / 100), float(std::min(p.b.value(), 100.0) / 100));
    SDL_SetTextureBlendMode(img.texture, p.blend == 1   ? SDL_BLENDMODE_ADD
                                         : p.blend == 3 ? SDL_BLENDMODE_MUL
                                                        : SDL_BLENDMODE_BLEND);
    SDL_RenderTextureRotated(r_, img.texture, &src, &dst, p.angle.value(), nullptr, SDL_FLIP_NONE);
    SDL_SetTextureAlphaModFloat(img.texture, 1.0f);
    SDL_SetTextureColorModFloat(img.texture, 1, 1, 1);
    SDL_SetTextureBlendMode(img.texture, SDL_BLENDMODE_BLEND);
    (void)number;
}

void Renderer::drawChoices() {
    const ChoiceState &c = game_.choice();
    if (!c.active || c.visible.empty()) return;
    Image skin = images_.get(game_.windowSkin());
    std::vector<Image> lines;
    float maxW = 0, lineH = 0;
    for (int i : c.visible) {
        lines.push_back(text(c.items[i], {255, 255, 255}));
        maxW = std::max(maxW, float(lines.back().w));
        lineH = std::max(lineH, float(lines.back().h));
    }
    float pad = game_.gameDat().tileSize() * 0.4f;
    float w = maxW + pad * 4, h = lineH * lines.size() + pad * 2;
    SDL_FRect box{(game_.screenWidth() - w) / 2, (game_.screenHeight() - h) / 2, w, h};
    drawWindow(skin, box, 0.9f);
    Image cursor = images_.get(game_.cursorSkin());
    float blink = 0.6f + 0.4f * float(std::sin(game_.frameCount() * 0.1));
    if (cursor.texture)
        drawWindow(cursor, {box.x + pad, box.y + pad + c.cursor * lineH, w - pad * 2, lineH}, blink);
    for (size_t i = 0; i < lines.size(); i++) {
        if (!lines[i].texture) continue;
        SDL_FRect dst{box.x + pad * 2, box.y + pad + i * lineH, float(lines[i].w), float(lines[i].h)};
        SDL_RenderTexture(r_, lines[i].texture, nullptr, &dst);
    }
}

void Renderer::drawMessage() {
    const MessageState &m = game_.message();
    if (!m.active) return;
    Image skin = images_.get(game_.windowSkin());
    float pad = game_.gameDat().tileSize() * 0.4f;
    float h = game_.screenHeight() * 0.27f;
    SDL_FRect box{pad, game_.screenHeight() - h - pad, game_.screenWidth() - pad * 2, h};
    drawWindow(skin, box, 0.9f);
    float y = box.y + pad;
    size_t start = 0;
    while (start <= m.text.size()) {
        size_t nl = m.text.find('\n', start);
        std::string line = m.text.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        Image img = text(line, {255, 255, 255});
        if (img.texture) {
            SDL_FRect dst{box.x + pad, y, float(img.w), float(img.h)};
            SDL_RenderTexture(r_, img.texture, nullptr, &dst);
            y += img.h;
        }
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
}

void Renderer::draw() {
    SDL_SetRenderDrawColor(r_, 0, 0, 0, 255);
    SDL_RenderClear(r_);
#if __has_include("maprender.h")
    map_.draw();
#endif
    for (auto &[n, p] : game_.pictures()) drawPicture(n, p);

    // Change screen colour darkens or tints everything drawn so far.
    auto tint = game_.screenColor();
    if (tint[0] < 100 || tint[1] < 100 || tint[2] < 100) {
        SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_MUL);
        SDL_SetRenderDrawColorFloat(r_, float(std::max(0.0, tint[0]) / 100), float(std::max(0.0, tint[1]) / 100),
                                    float(std::max(0.0, tint[2]) / 100), 1);
        SDL_RenderFillRect(r_, nullptr);
        SDL_SetRenderDrawBlendMode(r_, SDL_BLENDMODE_NONE);
    }
    drawMessage();
    drawChoices();
}

}  // namespace wolf
