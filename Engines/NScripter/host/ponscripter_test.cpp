// Ponscripter checks on the Linux host: the UTF-8 helpers, a 0.utf game's languages and English text, NBZ archive
// entries, and ":b" images. The behaviour each checks was measured on the Umineko games' engine
// (docs/nscripter-cleanroom.md).
#include <bzlib.h>

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>

#include "engine.h"

namespace fs = std::filesystem;
using vnns::EncodingScope;
using vnns::TextEncoding;

static void encodingHelpers() {
    const std::string utf = "a\xE6\x97\xA5\xF0\x9F\x98\x80";  // a, U+65E5, U+1F600
    {
        EncodingScope scope(TextEncoding::Utf8);
        assert(vnns::charLen(utf, 0) == 1 && vnns::charLen(utf, 1) == 3 && vnns::charLen(utf, 4) == 4);
        size_t n = 0;
        assert(vnns::decodeChar(utf, 1, &n) == 0x65E5 && n == 3);
        assert(vnns::isHalfWidth(utf, 0) && !vnns::isHalfWidth(utf, 1));
        // A sequence cut off at the end counts one byte at a time, and completeLength drops it.
        assert(vnns::charLen(std::string("\xE6\x97"), 0) == 1);
        assert(vnns::completeLength(utf.substr(0, 3)) == 1 && vnns::completeLength(utf) == utf.size());
        assert(vnns::isWideSpace("\xE3\x80\x80", 0));
        assert(vnns::toUtf8(utf) == utf && vnns::fromUtf8(utf) == utf);
        assert(vnns::lowerAscii("AB\xE6\x97\xA5") == "ab\xE6\x97\xA5");
    }
    // Shift-JIS is untouched: a lead byte and its trail byte, which may be an ASCII letter, are one character.
    const std::string sjis = "A\x93\xfa\x82\x60";  // A, U+65E5, U+FF21
    assert(vnns::textEncoding() == TextEncoding::ShiftJis);
    assert(vnns::charLen(sjis, 1) == 2 && vnns::lowerAscii(sjis) == "a\x93\xfa\x82\x60");
    assert(vnns::toUtf8(sjis) == "A\xE6\x97\xA5\xEF\xBC\xA1");
    assert(vnns::completeLength(sjis.substr(0, 2)) == 1);
}

// Writes an .nsa archive of one entry stored as NBZ (compression 4), as the Umineko games store their bitmaps.
static void writeNbzArchive(const fs::path &path, const std::string &name, const vnns::Bytes &file) {
    std::vector<char> packed(file.size() * 2 + 600);
    unsigned packedSize = unsigned(packed.size());
    int r = BZ2_bzBuffToBuffCompress(packed.data(), &packedSize,
                                     const_cast<char *>(reinterpret_cast<const char *>(file.data())),
                                     unsigned(file.size()), 9, 0, 0);
    assert(r == BZ_OK);
    vnns::Bytes entry = {uint8_t(file.size() >> 24), uint8_t(file.size() >> 16), uint8_t(file.size() >> 8),
                         uint8_t(file.size())};
    entry.insert(entry.end(), packed.begin(), packed.begin() + packedSize);
    auto be32 = [](vnns::Bytes &b, uint32_t v) {
        for (int s = 24; s >= 0; s -= 8) b.push_back(uint8_t(v >> s));
    };
    vnns::Bytes index;
    index.insert(index.end(), name.begin(), name.end());
    index.push_back(0);
    index.push_back(4);
    be32(index, 0);
    be32(index, uint32_t(entry.size()));
    be32(index, uint32_t(file.size()));
    vnns::Bytes out = {0, 1};
    be32(out, uint32_t(6 + index.size()));
    out.insert(out.end(), index.begin(), index.end());
    out.insert(out.end(), entry.begin(), entry.end());
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char *>(out.data()), std::streamsize(out.size()));
}

// A 4 x 2 24-bit BMP: the top row red, green, blue, white; the bottom row black.
static vnns::Bytes testBitmap() {
    const int w = 4, h = 2, stride = 12;
    vnns::Bytes b(54 + stride * h, 0);
    auto le32 = [&](size_t at, uint32_t v) {
        for (int i = 0; i < 4; i++) b[at + size_t(i)] = uint8_t(v >> (8 * i));
    };
    b[0] = 'B';
    b[1] = 'M';
    le32(2, uint32_t(b.size()));
    le32(10, 54);
    le32(14, 40);
    le32(18, w);
    le32(22, h);
    b[26] = 1;
    b[28] = 24;
    const uint8_t top[4][3] = {{0, 0, 255}, {0, 255, 0}, {255, 0, 0}, {255, 255, 255}};  // BGR
    for (int x = 0; x < w; x++)  // rows are stored bottom up, so the top row comes second
        for (int c = 0; c < 3; c++) b[54 + stride + size_t(x * 3 + c)] = top[x][c];
    return b;
}

static void runGame() {
    char temp[] = "/tmp/vnns-ponscripter-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    writeNbzArchive(dir / "arc.nsa", "pic\\test.bmp", testBitmap());
    std::ofstream(dir / "0.utf") << ";value500\r\n*define\r\nnsa\r\nglobalon\r\nh_mapfont 0, \"fonts/face0.otf\"\r\n"
                                    "language english\r\ngame\r\n*start\r\nmov %600,7\r\n"
                                    "getreadlang %1\r\nlangjp\r\ngetreadlang %2\r\nlangen\r\ngetreadlang %3\r\n"
                                    "langall\r\ngetreadlang %4\r\n"
                                    "bg \"pic/TEST.bmp\",1\r\nlsp 1,\":bc;pic\\test.bmp\",100,100\r\nprint 1\r\nclick\r\n"
                                    "mov $5,\"var\"\r\n"
                                    "langjp「日本語」@\r\nlangen^Hello $5 {x} <y>^@\r\n^ and more^\r\n^ joined.^@\r\n";
    {
        vnns::Engine engine({root, root});
        std::string error;
        assert(engine.start(&error));
        assert(engine.ponscripter_ && vnns::textEncoding() == TextEncoding::Utf8);
        assert(engine.step(16));
        assert(engine.fontFiles_[0] == "fonts/face0.otf");
        assert(engine.getNumVar(1) == -1 && engine.getNumVar(2) == 1 && engine.getNumVar(3) == 0 &&
               engine.getNumVar(4) == -1);
        // The NBZ entry decodes, under any case and slash; ":bc" draws it opaque at half size, each pixel a 2 x 2
        // average.
        // A background smaller than the screen is centred.
        auto px = [&](int x, int y) { return engine.frame().row(y)[x]; };
        assert(px(318, 239) == vnns::rgba(255, 0, 0) && px(321, 239) == vnns::rgba(255, 255, 255));
        assert(px(100, 100) == vnns::rgba(63, 63, 0) && px(101, 100) == vnns::rgba(63, 63, 127));
        assert(px(102, 100) == vnns::rgba(0, 0, 0) && px(100, 101) == vnns::rgba(0, 0, 0));
        auto click = [&] {
            engine.mouseButton(1, true);
            engine.mouseButton(1, false);
            for (int i = 0; i < 100; i++) engine.step(16);
        };
        click();
        // The Japanese line isn't shown, but its click wait runs.
        assert(engine.wait_ == vnns::WaitKind::Click && engine.pageText_.empty());
        for (int clicks = 0; clicks < 2; clicks++) {
            engine.mouseButton(1, true);
            engine.mouseButton(1, false);
            for (int i = 0; i < 100; i++) engine.step(16);
        }
        // Inside ^...^, $ { and < are plain text; a line ending in ^ doesn't end the line on screen.
        assert(engine.pageText_ == "Hello $5 {x} <y>\n and more joined.");
        engine.requestQuit();
        assert(!engine.step(16));
    }
    // System data goes to a folder of VNDegen's own, never the PC engine's saves folder.
    assert(fs::exists(dir / "vndegen-saves" / "gloval.sav") && !fs::exists(dir / "saves"));
    fs::remove_all(dir);
}

int main() {
    encodingHelpers();
    runGame();
}
