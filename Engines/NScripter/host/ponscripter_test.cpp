// Ponscripter checks on the Linux host: the UTF-8 helpers, a 0.utf game's languages and English text, NBZ archive
// entries, ":b" images, and proportional text. The behaviour each checks was measured on the Umineko games' engine
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
        // Surrogates, values past U+10FFFF and overlong forms aren't characters: one byte at a time, U+FFFD.
        for (const char *bad : {"\xED\xA0\x80", "\xF4\x90\x80\x80", "\xC0\x80", "\xE0\x80\x80"}) {
            assert(vnns::charLen(bad, 0) == 1 && vnns::decodeChar(bad, 0, &n) == 0xFFFD && n == 1);
        }
    }
    // Shift-JIS is untouched: a lead byte and its trail byte, which may be an ASCII letter, are one character.
    const std::string sjis = "A\x93\xfa\x82\x60";  // A, U+65E5, U+FF21
    assert(vnns::textEncoding() == TextEncoding::ShiftJis);
    assert(vnns::charLen(sjis, 1) == 2 && vnns::lowerAscii(sjis) == "a\x93\xfa\x82\x60");
    assert(vnns::toUtf8(sjis) == "A\xE6\x97\xA5\xEF\xBC\xA1");
    assert(vnns::completeLength(sjis.substr(0, 2)) == 1);
    // itoa2's and prnum's full-width digits, in each encoding.
    assert(vnns::fullWidthDigits("-12") == "\x81\x7c\x82\x50\x82\x51");
    {
        EncodingScope scope(TextEncoding::Utf8);
        assert(vnns::fullWidthDigits("-12") == "\uFF0D\uFF11\uFF12");
    }
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

// Proportional text, with a font from the font pack standing in for a game's: positions on the doubled screen, quote
// substitution, faces from ~tags~, word wrap, paragraph breaks and string sprites follow the measured rules.
static void layout() {
    char temp[] = "/tmp/vnns-ponscripter-layout-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    fs::create_directory(dir / "fonts");
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Regular.ttf", dir / "fonts" / "regular.ttf");
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Bold.ttf", dir / "fonts" / "face2.ttf");
    std::ofstream(dir / "0.utf") << "*define\nh_mapfont 0, \"fonts/regular.ttf\"\ngame\n*start\n"
                                    "setwindow 10,20,10,20,23,23,0,2,0,0,1,#ffffff,0,0,639,479\nshadedistance 3,3\nautoclick 50\n"
                                    "lsp 5,\":s/30,30,1;#FFFFFF^AB^\",0,0\n"
                                    "^Hi 'a' ``b'' ~b~B~b~ end^@\nbr\n"
                                    "^mmmm oooo pppp qqqq rrrr ssss tttt uuuu vvvv wwww xxxx^@\nwait 100000\n";
    vnns::Engine engine({root, root});
    std::string error;
    assert(engine.start(&error));
    for (int i = 0; i < 200; i++) engine.step(16);
    vnns::PonsFace regular;
    vnns::Bytes data;
    assert(vnns::readFile((dir / "fonts" / "regular.ttf").string(), data) && regular.load(data));
    const int px = 46, ascent = 2 * regular.ascent(23), lineHeight = regular.height(px) + 4;
    // 'H' at the text origin on the doubled screen; 'i' a whole-pixel advance on; the baseline the ascent below.
    const auto &page = engine.page_;
    assert(page.size() >= 2 && page[0].x2 == 20 && page[0].y2 == 40 + ascent && page[0].x == 10);
    assert(page[1].x2 == 20 + regular.glyph('H', px)->advance && page[1].x == page[1].x2 / 2);
    // The page's text, as gettext gives it, keeps the tags (as the PC engine's does) for a backlog to draw.
    assert(engine.pageText_.rfind("Hi \u2019a\u2019 \u201Cb\u201D ~b~B~b~ end", 0) == 0);
    for (const vnns::TextChar &c : page) assert(c.face == (c.ch == "B" ? 2 : 0) && c.shadow);
    // The paragraph: br's half line below the first line's end, then lines that break between words.
    size_t first = 0;
    while (page[first].ch != "m") first++;
    assert(page[first].y2 == page[0].y2 + lineHeight + (regular.height(px) + 1) / 2 + 4);
    int lines = 1;
    for (size_t i = first + 1; i < page.size(); i++) {
        if (page[i].y2 == page[i - 1].y2) continue;
        assert(page[i].y2 == page[i - 1].y2 + lineHeight && page[i].x2 == 20 && page[i - 1].ch != " ");
        assert(page[i].ch == page[i + 1].ch);  // a word's first letter
        lines++;
    }
    for (const vnns::TextChar &c : page) assert(c.x2 + regular.glyph(c.ch[0], px)->advance <= 20 + 2 * 10 * 23);
    assert(lines >= 2);
    // A string sprite's size is its em on the doubled screen, its pitch added per character, halved.
    const vnns::Sprite &s = engine.scene_.sp[5];
    int width2 = regular.glyph('A', 30)->advance + regular.glyph('B', 30)->advance + 2;
    assert(s.img.cellWidth == ((width2 + 1) & ~1) / 2);
    fs::remove_all(dir);
}

// The commands the games add or use from NScripter's direct drawing: tachistate, endroll, drawsp and draw, logsp, and
// effect masks given as image tags.
static void commands() {
    char temp[] = "/tmp/vnns-ponscripter-commands-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    fs::create_directory(dir / "fonts");
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Regular.ttf", dir / "fonts" / "face0.ttf");
    writeNbzArchive(dir / "arc.nsa", "pic\\test.bmp", testBitmap());
    std::ofstream(dir / "0.utf") << "*define\nnsa\ngame\n*start\nbg #000000,1\n"
                                    "tachistate %1,%2,%3\nld l,\":c;pic\\test.bmp\",1\ntachistate %4,%5,%6\n"
                                    "lsp 1,\":c;pic\\test.bmp\",600,0\nprint 1\n"
                                    "btndef \"pic\\test.bmp\"\nresettimer\nendroll 0,0,4,1,0,0,4,1,2,1000,1\ngettimer %7\n"
                                    "drawclear\ndrawsp 1,0,255,10,10\ndraw\nlogsp 7,\"ab\",20,40\nclick\n"
                                    "effect 5,18,800,\":c;pic\\test.bmp\"\nbg #FFFFFF,5\nend\n";
    vnns::Engine engine({root, root});
    std::string error;
    assert(engine.start(&error));
    assert(engine.step(16));
    assert(engine.getNumVar(1) == 0 && engine.getNumVar(2) == 0 && engine.getNumVar(3) == 0);
    assert(engine.getNumVar(4) == 1 && engine.getNumVar(5) == 0 && engine.getNumVar(6) == 0);
    // endroll waits a pixel's time per pixel, a tenth of its speed in ms, and a click doesn't end it.
    assert(engine.wait_ == vnns::WaitKind::Timer && engine.endRoll_.on);
    engine.mouseButton(1, true);
    engine.mouseButton(1, false);
    for (int i = 0; i < 4; i++) engine.step(16);
    assert(engine.wait_ == vnns::WaitKind::Timer);
    for (int i = 0; i < 6; i++) engine.step(16);
    assert(engine.getNumVar(7) >= 100 && engine.getNumVar(7) < 140);
    // draw shows the drawsp picture: the sprite's red top-left pixel at (10, 10), black around it.
    assert(engine.bltState_ == 1 && engine.bltLayer_.row(10)[10] == vnns::rgba(255, 0, 0));
    assert(engine.bltLayer_.row(9)[10] == vnns::rgba(0, 0, 0));
    // logsp's position is on the doubled screen.
    assert(engine.scene_.sp[7].visible && engine.scene_.sp[7].x == 10 && engine.scene_.sp[7].y == 20);
    // A mask given as an image tag is the file after the tag.
    engine.mouseButton(1, true);
    engine.mouseButton(1, false);
    for (int i = 0; i < 3; i++) engine.step(16);
    assert(engine.effect_.active && engine.effect_.mask);
    fs::remove_all(dir);
}

// endroll's wait and scroll end on the same whole millisecond, the scroll moves a pixel per speed / 10 ms, and a roll
// whose blt drawing is dropped stops. A logsp with an absurd font size stays a bounded image.
static void endRollAndBounds() {
    char temp[] = "/tmp/vnns-ponscripter-roll-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    fs::create_directory(dir / "fonts");
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Regular.ttf", dir / "fonts" / "face0.ttf");
    writeNbzArchive(dir / "arc.nsa", "pic.bmp", testBitmap());
    std::ofstream(dir / "0.utf") << "*define\nnsa\ngame\n*start\nbtndef \"pic.bmp\"\n"
                                    "endroll 0,0,4,1,0,0,4,1,2,19,1\nclick\n"
                                    "endroll 0,0,4,1,0,0,4,1,2,1000,1\nclick\n"
                                    "setwindow 0,0,10,10,1073741824,1073741824,0,0,0,0,0,#000000,0,0,639,479\n"
                                    "logsp 1,\"a\",0,0\nclick\nend\n";
    vnns::Engine engine({root, root});
    std::string error;
    assert(engine.start(&error));
    auto click = [&] {
        engine.mouseButton(1, true);
        engine.mouseButton(1, false);
    };
    // 1 pixel at 1.9 ms takes 2 ms, rounded up; the top row shows until then, the bottom row after.
    assert(engine.step(1) && engine.endRoll_.on && engine.endRoll_.end - engine.endRoll_.start == 2);
    assert(engine.bltLayer_.row(0)[0] == vnns::rgba(255, 0, 0));
    assert(engine.step(1) && engine.wait_ == vnns::WaitKind::Timer);
    assert(engine.step(1) && !engine.endRoll_.on && engine.wait_ == vnns::WaitKind::Click);
    assert(engine.bltLayer_.row(0)[0] == vnns::rgba(0, 0, 0));
    click();
    assert(engine.step(1) && engine.endRoll_.on);
    engine.clearBlt();  // as a load or a reset does
    assert(!engine.endRoll_.on);
    for (int i = 0; i < 10; i++) engine.step(16);
    assert(engine.bltState_ == 0);
    click();
    for (int i = 0; i < 10; i++) engine.step(16);
    const vnns::Sprite &s = engine.scene_.sp[1];
    assert(s.loaded && s.img.image && s.img.image->w <= 8192 && s.img.image->h <= 8192 && s.alpha == 255);
    fs::remove_all(dir);
}

// The review cases: saves keep a Ponscripter line's language and style, savedir can't move saves into the PC's folder,
// hidden text makes no buttons or ruby but keeps clickstr waits, a textgosub keeps the line's carets, select items
// break between words, !s0 isn't read into a word, and a font file that ends early isn't read.
static void reviewCases() {
    char temp[] = "/tmp/vnns-ponscripter-review-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    fs::create_directories(dir / "fonts");
    fs::create_directories(dir / "saves");
    std::ofstream(dir / "saves" / "save1.dat") << "the PC's save";
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Regular.ttf", dir / "fonts" / "face0.ttf");
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Bold.ttf", dir / "fonts" / "face2.ttf");
    std::ofstream(dir / "fonts" / "face5.ttf", std::ios::binary).write("\x00\x01\x00\x00", 4);
    auto run = [&](const std::string &script, const auto &check) {
        std::ofstream(dir / "0.utf") << script;
        vnns::Engine engine({root, root});
        std::string error;
        assert(engine.start(&error));
        engine.step(16);
        check(engine);
    };
    auto click = [](vnns::Engine &engine) {
        engine.mouseButton(1, true);
        engine.mouseButton(1, false);
        for (int i = 0; i < 20; i++) engine.step(16);
    };
    // Hidden text makes no button or ruby, and its clickstr character still waits. A save there keeps the line's
    // language and style, and goes to VNDegen's folder whatever savedir says.
    run("*define\nsavedir \"saves\"\nclickstr \"\u3002\",0\ngame\n*start\nrubyon\n"
        "langjp<1 hidden>(\u65E5/\u306B)\u3002\u3053@\nlangjp\n^hidden^@\nlangen\n^~b~shown^@\nend\n",
        [&](vnns::Engine &engine) {
            assert(engine.textButtons_.empty() && engine.page_.empty() && engine.wait_ == vnns::WaitKind::Click);
            click(engine);
            click(engine);  // at ^hidden^@
            assert(engine.wait_ == vnns::WaitKind::Click && engine.page_.empty() && engine.textLang_ == 1);
            assert(engine.saveGame(1, "") && fs::exists(dir / "vndegen-saves" / "save1.dat"));
            std::string pc;
            std::getline(std::ifstream(dir / "saves" / "save1.dat"), pc);
            assert(pc == "the PC's save");
            click(engine);  // on to the English line, in bold
            assert(engine.textLang_ == 0 && !engine.page_.empty() && engine.ponsStyle_.bold);
            assert(engine.loadGame(1) && engine.textLang_ == 1 && !engine.ponsStyle_.bold);
            for (int i = 0; i < 20; i++) engine.step(16);
            assert(engine.page_.empty() && engine.wait_ == vnns::WaitKind::Click);  // the hidden line again
            click(engine);
            assert(engine.ponsStyle_.bold && engine.page_[0].face == 2);  // ~b~ applied once, not toggled back
        });
    // A textgosub routine that draws caret text of its own leaves the line's carets as they were.
    run("*define\ntextgosub *tg\ngame\n*tg\n^x^\nclick\nreturn\n*start\nmov $5,\"VALUE\"\n^A@ $5^@\nend\n",
        [&](vnns::Engine &engine) {
            for (int i = 0; i < 20; i++) engine.step(16);
            click(engine);
            assert(engine.pageText_.find("$5") != std::string::npos && engine.pageText_.find("VALUE") == std::string::npos);
        });
    // A face file that ends early isn't read; text in it falls back to fixed cells.
    run("*define\ngame\n*start\n^~c5~X^@\nend\n", [&](vnns::Engine &engine) {
        assert(engine.ponsFace(5) == nullptr && engine.ponsFace(0) != nullptr);
    });
    // A select item breaks between words, and !s0 doesn't eat the word after it.
    run("*define\ngame\n*start\nsetwindow 0,0,4,10,23,23,0,2,0,0,0,#000000,0,0,639,479\n^HiHiii !s0dog^@\nend\n",
        [&](vnns::Engine &engine) {
            for (int i = 0; i < 20; i++) engine.step(16);
            int row = engine.page_[0].y2;
            for (const vnns::TextChar &c : engine.page_)
                if (c.ch == "d" || c.ch == "o" || c.ch == "g") assert(c.y2 > row);
            engine.clearPage();
            engine.putString("Hi mmmm");
            int mRow = -1;
            for (const vnns::TextChar &c : engine.page_)
                if (c.ch == "m") assert(mRow < 0 ? (mRow = c.y2) > engine.page_[0].y2 : c.y2 == mRow);
        });
    fs::remove_all(dir);
}

int main() {
    encodingHelpers();
    runGame();
    layout();
    commands();
    endRollAndBounds();
    reviewCases();
}
