// Ponscripter checks on the Linux host: the UTF-8 helpers, a 0.utf game's languages and English text, NBZ archive
// entries, ":b" images, and proportional text. The behaviour each checks was measured on the Umineko games' engine
// (docs/nscripter-cleanroom.md).
#include <bzlib.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

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
    assert(fs::exists(dir / "vndegen-saves" / "global.sav") && !fs::exists(dir / "saves"));
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
    // Hidden text makes no button or ruby, and its clickstr character still waits. A save there goes to VNDegen's
    // folder whatever savedir says; in the PC's format it keeps no text state, and loading replays the page from a line
    // that names its language, as the games' lines all do, so the hidden line is hidden again.
    run("*define\nsavedir \"saves\"\nclickstr \"\u3002\",0\ngame\n*start\nrubyon\n"
        "langjp<1 hidden>(\u65E5/\u306B)\u3002\u3053@\nlangjp^hidden^@\nlangen^~b~shown^@\nend\n",
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
            assert(engine.loadGame(1) && !engine.ponsStyle_.bold);
            for (int i = 0; i < 20; i++) engine.step(16);
            // the hidden line again
            assert(engine.page_.empty() && engine.wait_ == vnns::WaitKind::Click && engine.textLang_ == 1);
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

// Review round 2: a font whose required tables are empty isn't read; one string sprite cell keeps to the pixel
// budget; a string's lookahead reads it as putString does; indentation can't overflow; a wrapped choice is chosen by
// any of its lines.
static void reviewRound2() {
    char temp[] = "/tmp/vnns-ponscripter-round2-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    fs::create_directories(dir / "fonts");
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Regular.ttf", dir / "fonts" / "face0.ttf");
    {
        // Six tables, each empty, at the end of the file.
        vnns::Bytes f = {0, 1, 0, 0, 0, 6, 0, 0, 0, 0, 0, 0};
        for (const char *tag : {"cmap", "glyf", "head", "hhea", "hmtx", "loca"}) {
            f.insert(f.end(), tag, tag + 4);
            f.insert(f.end(), {0, 0, 0, 0, 0, 0, 0, 108, 0, 0, 0, 0});
        }
        vnns::PonsFace face;
        assert(f.size() == 108 && !face.load(f));
    }
    std::ofstream(dir / "0.utf") << "*define\ngame\n*start\n"
                                    "setwindow 10,10,4,10,23,23,0,2,0,0,0,#000000,0,0,639,479\n"
                                    "select \"^Hi mmmm^\",*a,\"^Two^\",*b\n*a\n*b\n"
                                    "puttext \"Hi !s0dog\"\nclick\n"
                                    "setwindow 0,0,8,10,512,512,0,0,0,0,0,#000000,0,0,639,479\n"
                                    "logsp 1,\"HHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH\",0,0\n"
                                    "setwindow 0,0,8,10,2147483647,23,1,0,0,0,0,#000000,0,0,639,479\nindent 1\n"
                                    "puttext \"A\"\nclick\nend\n";
    vnns::Engine engine({root, root});
    std::string error;
    assert(engine.start(&error));
    engine.step(16);
    assert(engine.wait_ == vnns::WaitKind::Select);
    // "mmmm" wrapped to the second line; a click there picks the first choice.
    int secondRow = -1;
    for (const vnns::TextChar &c : engine.page_)
        if (c.ch == "m") secondRow = c.y + 5;
    assert(secondRow > engine.page_[0].y + 5);
    engine.mouseMove(14, secondRow);
    engine.step(16);
    assert(engine.selectHover_ == 0 && engine.page_[3].color == engine.selectColor(true));
    engine.mouseButton(1, true);
    engine.mouseButton(1, false);
    for (int i = 0; i < 10; i++) engine.step(16);
    // puttext measured "!s0dog" as it puts it, so the whole word went to the next line.
    int bang = -1, g = -1;
    for (const vnns::TextChar &c : engine.page_) {
        if (c.ch == "!") bang = c.y2;
        if (c.ch == "g") g = c.y2;
    }
    assert(bang > 0 && bang == g && bang > engine.page_[0].y2);
    engine.mouseButton(1, true);
    engine.mouseButton(1, false);
    for (int i = 0; i < 10; i++) engine.step(16);
    const vnns::Sprite &s = engine.scene_.sp[1];
    assert(s.img.image && int64_t(s.img.image->w) * s.img.image->h <= (int64_t(1) << 22));
    fs::remove_all(dir);
}

// A dialog a Lua callback opens during a credits roll keeps the roll's wait to put back, and the roll finishes.
static void endRollUnderDialog() {
    char temp[] = "/tmp/vnns-ponscripter-dialog-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    writeNbzArchive(dir / "arc.nsa", "pic.bmp", testBitmap());
    std::ofstream(dir / "system.lua") << "NSLuaAnimationInterval(1)\nNSLuaAnimationMode(true)\n"
                                         "function NSCALL_animation()\n  if NSGetIntValue(50)==1 then\n"
                                         "    NSSetIntValue(50,2)\n    NSExec('yesnobox %51,\"hello\"')\n  end\n"
                                         "  return false\nend\n";
    std::ofstream(dir / "0.utf") << "*define\nnsa\nluacall animation\ngame\n*start\nbg #112233,1\n"
                                    "btndef \"pic.bmp\"\nmov %50,1\nendroll 0,0,4,1,0,0,4,1,2,1000,10\nclick\nend\n";
    vnns::Engine engine({root, root});
    std::string error;
    assert(engine.start(&error));
    for (int i = 0; i < 5; i++) engine.step(16);
    assert(engine.ui_ && engine.endRoll_.on);
    engine.key(VNNS_KEY_RETURN, true);
    engine.key(VNNS_KEY_RETURN, false);
    for (int i = 0; i < 100; i++) engine.step(16);
    assert(!engine.ui_ && !engine.endRoll_.on && engine.wait_ == vnns::WaitKind::Click);
    assert(engine.bltLayer_.row(0)[0] != vnns::rgba(255, 0, 0));  // scrolled past the red top row
    fs::remove_all(dir);
}

// Third review round: an endroll a Lua command starts through NSExec, a redefined csp before next, and lines holding
// only tags or !sd.
static void reviewRound3() {
    char temp[] = "/tmp/vnns-ponscripter-round3-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    fs::create_directories(dir / "fonts");
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Regular.ttf", dir / "fonts" / "face0.ttf");
    writeNbzArchive(dir / "arc.nsa", "pic.bmp", testBitmap());
    std::ofstream(dir / "system.lua") << "function NSCOM_credits()\n  NSExec('endroll 0,0,4,1,0,0,4,1,2,1000,10')\nend\n";
    // The Umineko games redefine csp and write "for %tmp = 100 to r_used_lsp : csp %tmp next".
    std::ofstream(dir / "0.utf") << "*define\nnsa\nluasub credits\ndefsub csp\ngame\n"
                                    "*csp\ngetparam %1\nadd %2,1\n_csp %1\nreturn\n"
                                    "*start\nfor %3 = 1 to 5 : csp %3 next\n"
                                    "setwindow 10,10,20,10,23,23,0,2,0,0,0,#000000,0,0,639,479\n"
                                    "langen^aa^@\nlangen!sd\nlangen~c0~\nlangjp日本\nlangen^bb^@\nlangen^cc^@\n"
                                    "bg #112233,1\nbtndef \"pic.bmp\"\ncredits\nclick\nend\n";
    vnns::Engine engine({root, root});
    std::string error;
    assert(engine.start(&error));
    engine.step(16);
    assert(engine.getNumVar(2) == 5);
    for (int i = 0; i < 3; i++) {
        assert(engine.wait_ == vnns::WaitKind::Click);
        engine.key(VNNS_KEY_RETURN, true);
        engine.key(VNNS_KEY_RETURN, false);
        for (int j = 0; j < 30; j++) engine.step(16);
    }
    std::map<std::string, int> rows;
    for (const vnns::TextChar &c : engine.page_) rows[c.ch] = c.y2;
    assert(rows.count("a") && rows.count("b") && rows.count("c"));
    assert(rows["b"] - rows["a"] == rows["c"] - rows["b"] && rows["b"] > rows["a"]);
    // The roll started through NSExec keeps scrolling to its end.
    assert(engine.endRoll_.on);
    for (int i = 0; i < 20; i++) engine.step(16);
    assert(engine.endRoll_.on);
    for (int i = 0; i < 60; i++) engine.step(16);
    assert(!engine.endRoll_.on && engine.wait_ == vnns::WaitKind::Click);
    assert(engine.bltLayer_.row(0)[0] != vnns::rgba(255, 0, 0));  // scrolled past the red top row
    fs::remove_all(dir);
}

// Where a save resumes in a game with a textgosub, as the PC's saves recorded it in lab scripts shaped like the games':
// the head of the page, at a text line's head while saveon is in effect, and a hidden page wait doesn't clear the page.
static std::string savepointAfter(const std::string &body, int clicks) {
    char temp[] = "/tmp/vnns-ponscripter-savepoint-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    fs::create_directories(dir / "fonts");
    fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Regular.ttf", dir / "fonts" / "face0.ttf");
    std::string script = "*define\npretextgosub *pt\ntextgosub *tg\ngame\n*pt\nsaveon\nreturn\n"
                         "*tg\ngetreadlang %70\nif %70 = 1 goto *tgend\nsaveoff\nclick\n*tgend\ntexec\nreturn\n"
                         "*start\nsaveoff\nsetwindow 31,16,25,20,23,23,0,2,0,1,1,#999999,0,0,639,479\nclick\n"
                         "langjp!sd\nlangen!sd\n" + body + "wait 100000\n";
    std::ofstream(dir / "0.utf") << script;
    vnns::Engine engine({root, root});
    std::string error;
    assert(engine.start(&error));
    for (int i = 0; i < clicks; i++) {
        for (int j = 0; j < 20; j++) engine.step(16);
        assert(engine.wait_ == vnns::WaitKind::Click);
        engine.key(VNNS_KEY_RETURN, true);
        engine.key(VNNS_KEY_RETURN, false);
    }
    for (int j = 0; j < 20; j++) engine.step(16);
    std::vector<std::string> lines;
    std::istringstream in(script);
    for (std::string l; std::getline(in, l);) lines.push_back(l);
    fs::remove_all(dir);
    return engine.savepointLive_ ? "live" : lines.at(size_t(engine.savepointLine_));
}

static void savepoints() {
    // The click on "Two" is the third; the page that holds it began at its hidden line.
    assert(savepointAfter("langjp日本\\\nlangen^Again^\\\nlangjp日本二\\\nlangen^Two^\\\n", 3) == "langjp日本二\\");
    // On "Again", the page still began at the !sd lines: the hidden page wait before it didn't clear it.
    assert(savepointAfter("langjp日本\\\nlangen^Again^\\\n", 2) == "langjp!sd");
    // "Two" opens its page, but saveoff was in effect at its head, so the savepoint stays.
    assert(savepointAfter("langjp日本\\\nlangen^Again^\\\nlangen^Two^\\\n", 3) == "langjp!sd");
}

// A small TrueType or CFF font, its tables in tag order, for the font checks: 'H' maps to glyph 1.
static vnns::Bytes testFont(const std::map<std::string, vnns::Bytes> &tables) {
    vnns::Bytes out;
    auto u16 = [&](uint32_t v) { out.push_back(uint8_t(v >> 8)); out.push_back(uint8_t(v)); };
    auto u32 = [&](uint32_t v) { u16(v >> 16); u16(v & 0xffff); };
    bool cff = tables.count("CFF ");
    u32(cff ? 0x4f54544f : 0x00010000);
    u16(uint32_t(tables.size()));
    u16(0); u16(0); u16(0);
    uint32_t at = 12 + 16 * uint32_t(tables.size());
    for (const auto &[tag, data] : tables) {
        out.insert(out.end(), tag.begin(), tag.end());
        u32(0); u32(at); u32(uint32_t(data.size()));
        at += uint32_t(data.size());
    }
    for (const auto &[tag, data] : tables) out.insert(out.end(), data.begin(), data.end());
    return out;
}

static vnns::Bytes be(std::initializer_list<int> words) {
    vnns::Bytes b;
    for (int w : words) { b.push_back(uint8_t(w >> 8)); b.push_back(uint8_t(w)); }
    return b;
}

static std::map<std::string, vnns::Bytes> trueTypeTables(int glyphs, const vnns::Bytes &glyf, const std::vector<int> &loca,
                                                          int rangeOffset = 0) {
    std::map<std::string, vnns::Bytes> t;
    t["head"] = vnns::Bytes(54, 0);
    t["head"][18] = 1000 >> 8; t["head"][19] = 1000 & 0xff;  // unitsPerEm; short loca
    t["hhea"] = vnns::Bytes(36, 0);
    t["hhea"][4] = 800 >> 8; t["hhea"][5] = 800 & 0xff;
    t["hhea"][34] = uint8_t(glyphs >> 8); t["hhea"][35] = uint8_t(glyphs);
    t["maxp"] = be({0, 0x5000, glyphs});
    t["hmtx"] = vnns::Bytes(size_t(glyphs) * 4, 0);
    for (int g = 0; g < glyphs; g++) t["hmtx"][size_t(g) * 4 + 1] = 200;
    vnns::Bytes l;
    for (int o : loca) { vnns::Bytes w = be({o / 2}); l.insert(l.end(), w.begin(), w.end()); }
    t["loca"] = l;
    t["glyf"] = glyf;
    vnns::Bytes cmap = be({0, 1, 3, 1, 0, 12});
    vnns::Bytes sub = be({4, 32, 0, 4, 4, 1, 0, 'H', 0xffff, 0, 'H', 0xffff, (1 - 'H') & 0xffff, 1, rangeOffset, 0});
    cmap.insert(cmap.end(), sub.begin(), sub.end());
    t["cmap"] = cmap;
    return t;
}

// Fonts a game could ship broken: each loads or is refused, and drawing 'H' stays inside the font (the sanitizer build
// runs this too).
static void brokenFonts() {
    auto draw = [](const vnns::Bytes &font) {
        vnns::PonsFace face;
        if (!face.load(font)) return false;
        face.glyph('H', 46);
        return true;
    };
    vnns::Bytes composite = be({0xffff, 0, 0, 0, 0, 0, 1, 0});  // one component: glyph 1, byte offsets
    // A glyph cut off after its first two bytes, at the end of the file.
    assert(draw(testFont(trueTypeTables(2, be({1}), {0, 0, 2}))));
    // A cmap segment whose glyph array lies far past the subtable.
    assert(draw(testFont(trueTypeTables(2, {}, {0, 0, 0}, 65534))));
    // A composite that contains itself.
    assert(!draw(testFont(trueTypeTables(2, composite, {0, 0, 16}))));
    // Composites nested 34 deep are refused, 2 deep drawn.
    auto chain = [&](int depth) {
        vnns::Bytes glyf;
        std::vector<int> loca = {0, 0};
        for (int g = 1; g <= depth; g++) {
            vnns::Bytes c = be({0xffff, 0, 0, 0, 0, 0, g + 1, 0});
            glyf.insert(glyf.end(), c.begin(), c.end());
            loca.push_back(int(glyf.size()));
        }
        loca.push_back(int(glyf.size()));  // the last, empty
        return testFont(trueTypeTables(depth + 2, glyf, loca));
    };
    assert(!draw(chain(34)));
    assert(draw(chain(2)));
    // An empty CFF table at the end of the file.
    auto cff = trueTypeTables(2, {}, {0, 0, 0});
    cff.erase("glyf");
    cff.erase("loca");
    cff["CFF "] = {};
    draw(testFont(cff));
}

// Saves in the PC Ponscripter engine's format, laid out as its own saves of the same script are (lab saves): a save made
// inside a for loop and two gosubs loads and finishes the loop. The same script as a Shift-JIS NScripter game keeps
// NScripter's format and round-trips too.
static void saveFormats() {
    std::string body = "*define\ngame\n*sub2\nmov %7,%7+1\nif %7 = 1 savegame 1\nreturn\n*sub\ngosub *sub2\nreturn\n*start\n"
                       "setwindow 10,20,10,5,20,20,3,4,7,1,0,#abcdef,1,2,600,400\nlsp 3,\":c;pic.bmp\",33,44\n"
                       "lsp2 5,\":c;pic.bmp\",100,80,50,60,30,200\n"
                       "for %3 = 1 to 5 step 2\ngosub *sub\nnext\nmov %9,1\nclick\nend\n";
    for (bool pons : {true, false}) {
        char temp[] = "/tmp/vnns-saveformat-XXXXXX";
        const char *root = mkdtemp(temp);
        assert(root);
        fs::path dir(root);
        fs::create_directories(dir / "fonts");
        fs::copy_file(fs::path(VNNS_FONTS_DIR) / "LiberationSans-Regular.ttf", dir / "fonts" / "face0.ttf");
        std::ofstream(dir / "pic.bmp", std::ios::binary).write(reinterpret_cast<const char *>(testBitmap().data()),
                                                               std::streamsize(testBitmap().size()));
        std::ofstream(dir / (pons ? "0.utf" : "0.txt")) << body;
        auto play = [&]() {
            auto engine = std::make_unique<vnns::Engine>(vnns::Engine::Config{root, root});
            std::string error;
            assert(engine->start(&error));
            for (int i = 0; i < 10 && engine->wait_ != vnns::WaitKind::Click; i++) engine->step(16);
            assert(engine->wait_ == vnns::WaitKind::Click && engine->getNumVar(7) == 3 && engine->getNumVar(9) == 1);
            return engine;
        };
        play();
        fs::path save = dir / (pons ? "vndegen-saves" : "") / "save1.dat";
        vnns::Bytes b;
        assert(vnns::readFile(save.string(), b));
        auto at = [&](size_t p) { return int32_t(vnns::le32(&b[p])); };
        if (pons) {
            assert(memcmp(b.data(), "PNS\x02\x06", 5) == 0);
            // The window: origin, text area, font height, pitch and spacing, all doubled; then color and speed.
            const int window[] = {20, 40, 10 * 2 * (20 + 6), 5 * 2 * (20 + 8), 40, 0, 6, 8, 0xabcdef, 7, 2, 4, 1200, 800};
            for (size_t k = 0; k < 14; k++) assert(at(56 + 4 * k) == window[k]);
            // Sprite 3 and lsp2 sprite 5: doubled positions, and the shown flag with bit 1.
            std::string tag(":c;pic.bmp");
            auto sp = std::search(b.begin(), b.end(), tag.begin(), tag.end()) - b.begin();
            size_t p = size_t(sp) + tag.size() + 1;
            assert(at(p) == 66 && at(p + 4) == 88 && at(p + 8) == 3);
            auto sp2 = std::search(b.begin() + sp + 1, b.end(), tag.begin(), tag.end()) - b.begin();
            p = size_t(sp2) + tag.size() + 1;
            assert(at(p) == 200 && at(p + 4) == 160 && at(p + 8) == 50 && at(p + 20) == 3 && at(p + 24) == 200);
            // The resume line (savegame's, 0-based), statement 0, and an empty note.
            assert(at(b.size() - 11) == 4 && at(b.size() - 7) == 0 && memcmp(&b[b.size() - 3], "\"\"*", 3) == 0);
        } else {
            assert(at(0) == 2);
        }
        // Loading in a fresh engine resumes inside the loop and finishes it: three more passes through *sub2.
        auto engine = std::make_unique<vnns::Engine>(vnns::Engine::Config{root, root});
        std::string error;
        assert(engine->start(&error));
        engine->step(16);
        assert(engine->loadGame(1));
        for (int i = 0; i < 10 && engine->wait_ != vnns::WaitKind::Click; i++) engine->step(16);
        assert(engine->wait_ == vnns::WaitKind::Click && engine->getNumVar(9) == 1 && engine->getNumVar(3) == 7);
        assert(engine->scene_.sp[3].x == 33 && engine->scene_.sp[3].visible && engine->scene_.sp2[5].cx == 100);
        assert(engine->win_.tx == 10 && engine->win_.cols == 10 && engine->win_.rows == 5 && engine->win_.fh == 20);
        fs::remove_all(dir);
    }
}

int main() {
    encodingHelpers();
    runGame();
    layout();
    commands();
    endRollAndBounds();
    reviewCases();
    reviewRound2();
    endRollUnderDialog();
    reviewRound3();
    savepoints();
    brokenFonts();
    saveFormats();
}
