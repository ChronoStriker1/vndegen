// Linux test host for VNDegen's NScripter runtime: runs a game headless and takes commands on stdin, one per line.
//
//   step MS             advance the game by MS milliseconds, in 16 ms frames
//   move X Y            move the pointer to game pixel (X, Y)
//   click [X Y]         left click (at X, Y)
//   rclick              right click
//   wheel N             turn the wheel N notches (positive is up)
//   key NAME            press and release a key: RETURN, ESCAPE, SPACE, UP, DOWN, LEFT, RIGHT, PAGEUP, PAGEDOWN,
//                       TAB, CTRL, or a letter or digit
//   hold NAME / release NAME
//   text UTF8...        type text
//   shot PATH           write the current frame as a PNG
//   load N              load save slot N
//   where               print the script line and what the game waits for
//   audio               print the peak level of the sound mixed during the last step command, 0 to 1
//   quit                end the game as closing its window would, writing system data
//
// Usage: vnns_host GAME_DIR [--wordwrap] [--log FILE]. Prints "ok" after each command, or "ended" once the game ends.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <cstdio>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "vnfont.h"
#include "vnns.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

static FT_Library gFt;
static FT_Face gFace;      // the font pack's MS Gothic substitute, as the app uses
static FT_Face gFallback;  // a system CJK face for characters it lacks

// Renders a character into its cell: an em of cellHeight, stretched to the cell's width, baseline at 86% down.
static int glyph(void *, uint32_t cp, int cellW, int cellH, int bold, uint8_t *coverage) {
    FT_Face face = gFace;
    FT_UInt index = FT_Get_Char_Index(face, cp);
    if (!index && gFallback) {
        face = gFallback;
        index = FT_Get_Char_Index(face, cp);
    }
    if (!index) return 0;
    bool half = cp < 0x100 || (cp >= 0xFF61 && cp <= 0xFF9F);
    int fullW = half ? cellW * 2 : cellW;
    FT_Set_Pixel_Sizes(face, FT_UInt(fullW), FT_UInt(cellH));
    if (FT_Load_Glyph(face, index, FT_LOAD_DEFAULT)) return 0;
    if (bold) FT_Outline_Embolden(&face->glyph->outline, 1 << 5);
    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) return 0;
    FT_GlyphSlot g = face->glyph;
    int natural = half ? cellW : cellW;
    int ox = (natural - int(g->advance.x >> 6)) / 2 + g->bitmap_left;
    int baseline = cellH * 86 / 100;
    int oy = baseline - g->bitmap_top;
    for (unsigned y = 0; y < g->bitmap.rows; y++)
        for (unsigned x = 0; x < g->bitmap.width; x++) {
            int tx = ox + int(x), ty = oy + int(y);
            if (tx < 0 || ty < 0 || tx >= cellW || ty >= cellH) continue;
            uint8_t v = g->bitmap.buffer[y * unsigned(g->bitmap.pitch) + x];
            uint8_t &d = coverage[size_t(ty) * size_t(cellW) + size_t(tx)];
            if (v > d) d = v;
        }
    return 1;
}

static int keyCode(const std::string &name) {
    static const std::map<std::string, int> keys = {
        {"RETURN", VNNS_KEY_RETURN}, {"ESCAPE", VNNS_KEY_ESCAPE}, {"SPACE", VNNS_KEY_SPACE},
        {"UP", VNNS_KEY_UP},         {"DOWN", VNNS_KEY_DOWN},     {"LEFT", VNNS_KEY_LEFT},
        {"RIGHT", VNNS_KEY_RIGHT},   {"PAGEUP", VNNS_KEY_PAGEUP}, {"PAGEDOWN", VNNS_KEY_PAGEDOWN},
        {"TAB", VNNS_KEY_TAB},       {"CTRL", VNNS_KEY_CTRL},     {"SHIFT", VNNS_KEY_SHIFT},
        {"BACKSPACE", VNNS_KEY_BACKSPACE}};
    auto it = keys.find(name);
    if (it != keys.end()) return it->second;
    if (name.size() == 1) return toupper(name[0]);
    if (name.size() > 1 && name[0] == 'F') return VNNS_KEY_F1 + atoi(name.c_str() + 1) - 1;
    return -1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: vnns_host GAME_DIR [--wordwrap] [--log FILE]\n");
        return 2;
    }
    VNNSLaunch launch{};
    launch.gameRoot = argv[1];
    for (int i = 2; i < argc; i++) {
        if (!strcmp(argv[i], "--wordwrap")) launch.englishWordWrap = true;
        else if (!strcmp(argv[i], "--log") && i + 1 < argc) launch.logPath = argv[++i];
    }
    FT_Init_FreeType(&gFt);
    const char *dirs[] = {VNNS_FONTS_DIR};
    vnfont_init(VNNS_FONTS_DIR "/fonts.tsv", dirs, 1);
    vnfont_match gothic;
    if (!vnfont_resolve("MS Gothic", 0, 0, &gothic) || FT_New_Face(gFt, gothic.path, gothic.face, &gFace)) {
        fprintf(stderr, "the font pack's MS Gothic substitute is missing from %s\n", VNNS_FONTS_DIR);
        return 1;
    }
    FT_New_Face(gFt, "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", 0, &gFallback);
    vnns_set_glyph_function(glyph, nullptr);
    VNNSEngine *e = vnns_start(&launch);
    if (!e) {
        fprintf(stderr, "start failed: %s\n", vnns_start_error());
        return 1;
    }
    int w = 0, h = 0;
    vnns_frame(e, &w, &h, nullptr);
    std::vector<float> audio(2 * 735);
    bool running = true;
    int drawn = 0;  // frames that changed, for the "drawn" command
    float peak = 0;  // loudest sample mixed during the last step, for the "audio" command
    // VNNS_SLOW_MS reports each step that takes at least that long, with where the script is.
    double slowMs = getenv("VNNS_SLOW_MS") ? atof(getenv("VNNS_SLOW_MS")) : 0;
    auto stepFor = [&](int ms) {
        while (ms > 0 && running) {
            int d = ms < 16 ? ms : 16;
            auto t0 = std::chrono::steady_clock::now();
            running = vnns_step(e, d);
            double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            if (slowMs > 0 && took >= slowMs) fprintf(stderr, "slow step %.1f ms at %s\n", took, vnns_debug_status(e));
            vnns_audio_render(e, audio.data(), d * 44100 / 1000);
            for (int i = 0; i < 2 * d * 44100 / 1000; i++) peak = std::max(peak, std::fabs(audio[size_t(i)]));
            bool changed = false;
            vnns_frame(e, &w, &h, &changed);
            if (changed) drawn++;
            ms -= d;
        }
    };
    std::string line;
    std::cout << "ready " << w << "x" << h << " " << vnns_caption(e) << std::endl;
    while (running && std::getline(std::cin, line)) {
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd == "step") {
            int ms = 0;
            in >> ms;
            peak = 0;
            stepFor(ms);
        } else if (cmd == "audio") {
            std::cout << "audio peak " << peak << std::endl;
        } else if (cmd == "move" || cmd == "click") {
            float x, y;
            if (in >> x >> y) vnns_mouse_move(e, (x + 0.5f) / w, (y + 0.5f) / h);
            if (cmd == "click") {
                vnns_mouse_button(e, 1, true);
                stepFor(16);
                vnns_mouse_button(e, 1, false);
                stepFor(16);
            } else {
                stepFor(16);
            }
        } else if (cmd == "rclick") {
            vnns_mouse_button(e, 3, true);
            stepFor(16);
            vnns_mouse_button(e, 3, false);
            stepFor(16);
        } else if (cmd == "wheel") {
            int n = 0;
            in >> n;
            vnns_wheel(e, n);
            stepFor(16);
        } else if (cmd == "key" || cmd == "hold" || cmd == "release") {
            std::string name;
            in >> name;
            int k = keyCode(name);
            if (cmd != "release") vnns_key(e, k, true);
            stepFor(16);
            if (cmd != "hold") {
                vnns_key(e, k, false);
                stepFor(16);
            }
        } else if (cmd == "text") {
            std::string rest;
            std::getline(in, rest);
            if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);
            vnns_text(e, rest.c_str());
            stepFor(16);
        } else if (cmd == "shot") {
            std::string path;
            in >> path;
            const uint8_t *px = vnns_frame(e, &w, &h, nullptr);
            // The runtime's frames are opaque RGBA; write them as RGB.
            std::vector<uint8_t> rgb(size_t(w) * size_t(h) * 3);
            for (size_t i = 0; i < size_t(w) * size_t(h); i++) memcpy(&rgb[i * 3], &px[i * 4], 3);
            stbi_write_png(path.c_str(), w, h, 3, rgb.data(), w * 3);
        } else if (cmd == "load") {
            int slot = 0;
            in >> slot;
            if (!vnns_load_game(e, slot)) std::cout << "load failed" << std::endl;
        } else if (cmd == "drawn") {
            std::cout << "drawn " << drawn << std::endl;
            drawn = 0;
        } else if (cmd == "where") {
            std::cout << vnns_debug_status(e) << std::endl;
        } else if (cmd == "quit") {
            vnns_request_quit(e);
            stepFor(16);
            break;
        } else if (!cmd.empty()) {
            std::cout << "unknown command " << cmd << std::endl;
            continue;
        }
        std::cout << (running ? "ok" : "ended") << std::endl;
    }
    vnns_stop(e);
    return 0;
}
