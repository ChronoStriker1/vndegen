// Draws a Game with an SDL renderer: Metal on Apple platforms, software for the headless Linux checks.
#pragma once

#include "game.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <map>
#include <string>

namespace wolf {

class Renderer {
public:
    Renderer(SDL_Renderer *renderer, const Game &game);
    ~Renderer();
    void draw();

private:
    struct Image {
        SDL_Texture *texture = nullptr;
        int w = 0, h = 0;
    };
    Image image(const std::string &path);
    Image text(const std::string &s, std::array<uint8_t, 3> color);
    void drawWindow(const Image &skin, SDL_FRect r, float alpha);
    void drawPicture(int number, const Picture &p);
    void drawChoices();
    void drawMessage();
    TTF_Font *font();

    SDL_Renderer *r_;
    const Game &game_;
    std::map<std::string, Image> images_;
    std::map<int, std::pair<uint64_t, Image>> pictureText_;
    std::map<std::string, Image> textCache_;
    TTF_Font *font_ = nullptr;
    bool fontTried_ = false;
};

}  // namespace wolf
