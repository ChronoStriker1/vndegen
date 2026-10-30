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
#include <tuple>
#include <vector>

namespace wolf {

class Game;

struct TextStyle {
    int size = 18;
    std::array<uint8_t, 3> color{255, 255, 255};
    int lineSpacing = 4;    // extra pixels between lines
    int letterSpacing = 0;  // extra pixels between characters
    bool shadow = true;     // Wolf draws a dark shadow down and to the right of each character
    // The game's text pixels per screen pixel. Wolf draws 320 by 240 games at twice the size, and the pixel counts in
    // \ax, \ay, \mx, \my and \- are in those doubled pixels.
    int unit = 1;
};

// The style messages, choices and text pictures start from: the size and spacing Game.dat and the screen size give.
TextStyle baseTextStyle(const Game &game);

struct TextBlock {
    Bitmap bitmap;
    size_t chars = 0;  // characters laid out, for typing
    int origin = 1;    // the text starts this many pixels right of and below the bitmap's top left
};

// One piece of text after its codes are read.
struct TextToken {
    enum Kind {
        Char, Newline, Color, Size, Icon, AbsX, AbsY, MoveX, MoveY, Ruby, Speed, Wait, Instant, AutoClose,
        Align,      // <L>, <C>, <R>: 0 left, 1 centre, 2 right, for the rest of the text
        LineSpace,  // \space[n]: the gap between lines from here on
        Tighten,    // \-[n]: pixels taken off each character's advance from here on
        Edge,       // \E (1) and \N (0): outlined text or plain
        EdgeColor,  // \cE[n]
        Face,       // \font[n]: the base font (0) or a sub font (1 to 3)
        LineMax,    // \m[n]: the line is at least as tall as text of size n
    };
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
    // `images` draws \i icons; without it the renderer only measures, and icons take their space.
    TextRenderer(const Game &game, ImageCache *images);
    // Lays out text and draws it into a new bitmap sized to fit. `visible` limits the characters drawn.
    TextBlock render(const std::string &text, const TextStyle &style, size_t visible = SIZE_MAX);
    // The width and height of the laid-out text, as a text picture reports its size.
    std::array<int, 2> measure(const std::string &text, const TextStyle &style);
    int lineHeight(const TextStyle &style) const { return style.size + style.lineSpacing; }

private:
    struct Layout;
    Layout layout(const std::string &text, const TextStyle &style, size_t visible);
    const GlyphImage *glyph(uint32_t cp, int size, int face);
    Font &font(int face);

    const Game &game_;
    ImageCache *images_;
    std::array<std::unique_ptr<Font>, 4> fonts_;
    std::map<std::tuple<uint32_t, int, int>, GlyphImage> glyphs_;
};

}  // namespace wolf
