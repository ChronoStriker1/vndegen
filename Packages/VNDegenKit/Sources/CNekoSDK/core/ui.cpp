#include "ui.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

#include "sjis.h"

namespace neko {
namespace {

std::vector<std::string> words(const std::string &line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string word;
    while (in >> word) out.push_back(word);
    return out;
}

int number(const std::string &s) { return std::atoi(s.c_str()); }

// "1,2,3" as numbers.
std::vector<int> numbers(const std::string &s) {
    std::vector<int> out;
    std::istringstream in(s);
    for (std::string part; std::getline(in, part, ',');) out.push_back(number(part));
    return out;
}

std::string withoutComment(const std::string &line) {
    auto at = line.find("//");
    return at == std::string::npos ? line : line.substr(0, at);
}

// `56,690` into two numbers.
bool pair(const std::string &s, int &a, int &b) {
    auto comma = s.find(',');
    if (comma == std::string::npos) return false;
    a = number(s.substr(0, comma));
    b = number(s.substr(comma + 1));
    return true;
}

}  // namespace

void drawPicture(Canvas &out, const Picture &p, float towardX, float towardY) {
    if (!p.image) return;
    if (p.followX != 0 || p.followY != 0) {
        drawShifted(out, *p.image, float(p.x) - p.followX * towardX, float(p.y) - p.followY * towardY, p.alpha);
        return;
    }
    if (p.partW > 0 && p.partH > 0) {
        drawPart(out, *p.image, p.x, p.y, p.partX, p.partY, p.partW, p.partH, p.alpha);
        return;
    }
    int w = p.image->w / std::max(1, p.cols), h = p.image->h / std::max(1, p.rows);
    drawPart(out, *p.image, p.x, p.y, p.column * w, p.row * h, w, h, p.alpha);
}

void drawButton(Canvas &out, const Button &b, ButtonState state, bool withHelp) {
    if (!b.sheet) return;
    int row = state == StateNormal ? b.normalRow : state == StateHover ? b.hoverRow : int(state);
    row = std::min(row, b.rows - 1);
    drawPart(out, *b.sheet, b.x, b.y, b.column * b.cellW(), row * b.cellH(), b.cellW(), b.cellH(), 1);
    if (withHelp || state == StateHover || state == StateSelectedHover) drawButtonHelp(out, b);
}

void drawButtonHelp(Canvas &out, const Button &b) {
    if (!b.help) return;
    int w = b.help->w / b.helpCols, h = b.help->h / b.helpRows;
    drawPart(out, *b.help, b.helpX, b.helpY, b.helpColumn * w, b.helpRow * h, w, h, 1);
}

std::map<std::string, Screen> parseScreens(const char *table, Images &images, const std::function<void(const std::string &)> &warn) {
    std::map<std::string, Screen> screens;
    Screen *screen = nullptr;
    std::istringstream in(table);
    std::string line;
    auto image = [&](const std::string &path) {
        ImageRef ref = images.get(path);
        if (!ref) warn("Menu table names a missing picture: " + path);
        return ref;
    };
    while (std::getline(in, line)) {
        auto w = words(line);
        if (w.empty() || w[0][0] == '#') continue;
        if (w[0] == "screen" && w.size() >= 2) {
            screen = &screens[w[1]];
            screen->name = w[1];
        } else if (!screen) {
            continue;
        } else if (w[0] == "pic" && w.size() >= 4) {
            Picture p;
            p.dim = w[1] == "dim";
            p.image = p.dim ? nullptr : image(w[1]);
            p.x = number(w[2]);
            p.y = number(w[3]);
            size_t at = 4;
            if (w.size() >= 8 && (std::isdigit(uint8_t(w[4][0])) || w[4][0] == '-')) {
                p.cols = number(w[4]);
                p.rows = number(w[5]);
                p.column = number(w[6]);
                p.row = number(w[7]);
                at = 8;
            }
            for (; at + 1 < w.size(); at += 2) {
                // `random` takes no value.
                while (at < w.size() && w[at] == "random") {
                    p.randomAlways = true;
                    at++;
                }
                if (at + 1 >= w.size()) break;
                const std::string &key = w[at], &value = w[at + 1];
                if (key == "alpha") p.alpha = float(number(value)) / 256;  // of 256
                else if (key == "tab") p.tab = number(value);              // only on that tab of a gallery
                else if (key == "stage") p.stage = number(value);
                else if (key == "group") p.group = value;
                else if (key == "blink") p.blinkMs = number(value);
                else if (key == "rise") {
                    std::istringstream in(value);
                    for (std::string point; std::getline(in, point, ',');)
                        if (auto colon = point.find(':'); colon != std::string::npos)
                            p.rise.push_back({number(point.substr(0, colon)), number(point.substr(colon + 1))});
                }
                else if (key == "if") p.needs = numbers(value);
                else if (key == "ifread") p.ifRead = value;
                else if (key == "unlessread") p.unlessRead = value;
                else if (key == "ifhover" || key == "unlesshover") {
                    std::istringstream in(value);
                    for (std::string action; std::getline(in, action, ',');) (key == "ifhover" ? p.ifHover : p.unlessHover).push_back(action);
                }
                else if ((key == "move" || key == "clamp" || key == "fadein" || key == "fadeout") && at + 2 < w.size()) {
                    const std::string &second = w[at + 2];
                    if (key == "move") {
                        p.moveFrom = number(value);
                        p.speed = std::strtof(second.c_str(), nullptr);
                    } else if (key == "clamp") {
                        p.clampTop = number(value);
                        p.clampBottom = number(second);
                    } else if (key == "fadein") {
                        p.fadeInAt = number(value);
                        p.fadeInMs = number(second);
                    } else {
                        p.fadeOutAt = number(value);
                        p.fadeOutMs = number(second);
                    }
                    at++;
                }
                else if (key == "pickby") p.pickBy = number(value);
                else if (key == "randomif") p.randomIf = numbers(value);
                else if (key == "more") {
                    p.variants = {p.image};
                    std::istringstream files(value);
                    for (std::string file; std::getline(files, file, ',');) p.variants.push_back(image(file));
                }
                else if (key == "part" && at + 4 < w.size()) {
                    p.partX = number(value);
                    p.partY = number(w[at + 2]);
                    p.partW = number(w[at + 3]);
                    p.partH = number(w[at + 4]);
                    at += 3;
                }
                else if (key == "follow" && at + 2 < w.size()) {
                    p.followX = std::strtof(value.c_str(), nullptr);
                    p.followY = std::strtof(w[at + 2].c_str(), nullptr);
                    at++;
                }
            }
            screen->pictures.push_back(p);
        } else if (w[0] == "button" && w.size() >= 8) {
            Button b;
            b.action = w[1];
            b.sheet = image(w[2]);
            b.x = number(w[3]);
            b.y = number(w[4]);
            b.cols = number(w[5]);
            b.rows = number(w[6]);
            b.column = number(w[7]);
            for (size_t i = 8; i < w.size(); i++) {
                if (w[i] == "toggle") b.toggle = true;
                if (w[i] == "stage" && i + 1 < w.size()) b.stage = number(w[++i]);
                if (w[i] == "group" && i + 1 < w.size()) b.group = w[++i];
                if (w[i] == "if" && i + 1 < w.size()) b.needs = numbers(w[++i]);
                if (w[i] == "ifread" && i + 1 < w.size()) b.ifRead = w[++i];
                if (w[i] == "unlessread" && i + 1 < w.size()) b.unlessRead = w[++i];
                if (w[i] == "swap") {  // this sheet's first row is the hovered look
                    b.normalRow = 1;
                    b.hoverRow = 0;
                }
                if (w[i] == "help" && i + 7 < w.size()) {
                    b.help = image(w[i + 1]);
                    b.helpX = number(w[i + 2]);
                    b.helpY = number(w[i + 3]);
                    b.helpCols = number(w[i + 4]);
                    b.helpRows = number(w[i + 5]);
                    b.helpColumn = number(w[i + 6]);
                    b.helpRow = number(w[i + 7]);
                    i += 7;
                }
            }
            screen->buttons.push_back(b);
        } else {
            if (w[0] == "drift" && !screen->settings.count("drift")) {
                Picture marker;
                marker.drift = true;
                screen->pictures.push_back(marker);
            }
            screen->settings.emplace(w[0], std::vector<std::string>(w.begin() + 1, w.end()));
            screen->lines.push_back(w);
        }
    }
    return screens;
}

MessageButtons parseMessageButtons(const std::vector<uint8_t> &ini, Images &images) {
    MessageButtons out;
    std::string text = sjis::toUtf8(std::string(ini.begin(), ini.end()));
    std::istringstream in(text);
    std::string line;
    std::vector<std::string> fields;
    std::string group, action;
    auto finish = [&] {
        // File, type, position, sheet size, five cell positions, help file, help position, help size, help cell, help style.
        if (fields.size() < 12 || action.empty()) return;
        Button b;
        b.action = action;
        b.sheet = images.get(fields[0]);
        if (!b.sheet) return;
        b.toggle = number(fields[1]) == 2;
        pair(fields[2], b.x, b.y);
        pair(fields[3], b.cols, b.rows);
        int row = 0;
        pair(fields[4], b.column, row);
        // A patched file could give anything here, and drawing divides by these.
        if (b.cols <= 0 || b.rows <= 0 || b.column < 0 || b.column >= b.cols) return;
        b.help = images.get(fields[9]);
        pair(fields[10], b.helpX, b.helpY);
        if (fields.size() > 12) {
            pair(fields[11], b.helpCols, b.helpRows);
            pair(fields[12], b.helpColumn, b.helpRow);
        }
        if (b.helpCols <= 0 || b.helpRows <= 0 || b.helpColumn < 0 || b.helpRow < 0 || b.helpColumn >= b.helpCols || b.helpRow >= b.helpRows) b.help = nullptr;
        (group == "under" ? out.under : out.inner).push_back(b);
    };
    while (std::getline(in, line)) {
        auto w = words(withoutComment(line));
        if (w.empty()) continue;
        if (w[0] == "##") {
            finish();
            fields.clear();
            action.clear();
        } else if (w[0][0] == '#') {
            group = w[0].substr(1);
            action = w.size() > 1 ? w[1] : "";
            fields.clear();
        } else if (!action.empty()) {
            fields.push_back(w[0]);
        }
    }
    return out;
}

}  // namespace neko
