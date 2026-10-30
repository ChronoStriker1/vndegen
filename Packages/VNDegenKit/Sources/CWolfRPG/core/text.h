// Wolf's text: its display codes, line layout, and drawing into a bitmap. Messages, choices and text pictures all use
// it. Codes that read variables, such as \v[n] and \s[n], are expanded by Game::expandText before text gets here.
#pragma once

#include "canvas.h"
#include "font.h"
#include "images.h"

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wolf {

class Game;

struct TextStyle {
    int size = 18;
    std::array<uint8_t, 3> color{255, 255, 255};
    int lineSpacing = 4;    // extra pixels between lines
    int letterSpacing = 0;  // extra pixels between characters
    bool shadow = true;     // Wolf draws a dark shadow down and to the right of each character
};

struct TextBlock {
    Bitmap bitmap;
    size_t chars = 0;  // characters laid out, for typing
};

// One piece of text after its codes are read.
struct TextToken {
    enum Kind { Char, Newline, Color, Size, Icon, AbsX, AbsY, MoveX, MoveY, Ruby, Speed, Wait, Instant, AutoClose };
    Kind kind = Char;
    uint32_t cp = 0;
    int value = 0;
    std::string base, ruby;  // for Ruby
};

std::vector<TextToken> parseText(const std::string &text);
// The characters the player sees appear one by one, for message typing.
size_t visibleChars(const std::string &text);

class TextRenderer {
public:
    TextRenderer(const Game &game, ImageCache &images);
    // Lays out text and draws it into a new bitmap sized to fit. `visible` limits the characters drawn.
    TextBlock render(const std::string &text, const TextStyle &style, size_t visible = SIZE_MAX);
    int lineHeight(const TextStyle &style) const { return style.size + style.lineSpacing; }

private:
    const GlyphImage *glyph(uint32_t cp, int size);
    Font &font();

    const Game &game_;
    ImageCache &images_;
    std::unique_ptr<Font> font_;
    std::map<std::pair<uint32_t, int>, GlyphImage> glyphs_;
};

}  // namespace wolf
