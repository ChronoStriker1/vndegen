// Images, sprites, standing images, the background, compositing and transition effects.
#include <algorithm>
#include <cctype>
#include <cmath>

#include "engine.h"

namespace vnns {

static uint32_t parseColor(std::string_view s) {
    if (!s.empty() && s[0] == '#') s.remove_prefix(1);
    unsigned v = 0;
    for (size_t i = 0; i < 6 && i < s.size(); i++) {
        char c = s[i];
        v = v << 4 | unsigned(isdigit(uint8_t(c)) ? c - '0' : (tolower(c) - 'a' + 10) & 15);
    }
    return rgba(int(v >> 16 & 255), int(v >> 8 & 255), int(v & 255));
}

ImageRef Engine::loadFileImage(const std::string &file, bool *hasAlpha) {
    std::string key = normalizePath(file);
    if (auto it = imageCache_.find(key); it != imageCache_.end()) {
        if (hasAlpha) *hasAlpha = imageAlpha_.count(key) != 0;
        return it->second;
    }
    Image img;
    bool alpha = false;
    if (key.size() > 1 && key[0] == '>') {
        // ">w,h,#color#color..." makes a solid rectangle, split into equal columns per color.
        int w = 0, h = 0;
        size_t p = 1;
        w = atoi(key.c_str() + p);
        p = key.find(',', p);
        if (p != std::string::npos) h = atoi(key.c_str() + p + 1);
        std::vector<uint32_t> colors;
        for (size_t k = key.find('#'); k != std::string::npos; k = key.find('#', k + 1))
            colors.push_back(parseColor(key.substr(k)));
        if (w <= 0 || h <= 0 || colors.empty()) return nullptr;
        img = Image(w, h);
        for (size_t c = 0; c < colors.size(); c++) {
            int x0 = int(w * c / colors.size()), x1 = int(w * (c + 1) / colors.size());
            fillRect(img, x0, 0, x1 - x0, h, colors[c]);
        }
    } else {
        Bytes data;
        if (!fs_.read(file, data)) {
            errorf("image %s not found", sjisToUtf8(file).c_str());
            return nullptr;
        }
        if (!decodeImage(data, img, &alpha)) {
            errorf("image %s could not be decoded", sjisToUtf8(file).c_str());
            return nullptr;
        }
    }
    auto ref = std::make_shared<const Image>(std::move(img));
    imageCache_[key] = ref;
    if (alpha) imageAlpha_.insert(key);
    if (hasAlpha) *hasAlpha = alpha;
    imageCacheOrder_.push_back(key);
    // Keep the cache to a sensible size; images in use stay alive through their shared pointers.
    if (imageCacheOrder_.size() > 64) {
        imageCache_.erase(imageCacheOrder_.front());
        imageAlpha_.erase(imageCacheOrder_.front());
        imageCacheOrder_.erase(imageCacheOrder_.begin());
    }
    return ref;
}

// Parses an image tag and loads the image with its transparency applied.
bool Engine::loadImage(const std::string &tag, LoadedImage &out) {
    out = LoadedImage();
    out.tag = tag;
    std::string mode = transMode_;
    std::string maskFile, file = tag;
    uint32_t keyColor = 0;
    bool keyByColor = false;
    if (!tag.empty() && tag[0] == ':') {
        size_t semi = std::string::npos;
        // String sprites have no file part; everything after the settings is text.
        if (tag.size() > 1 && (tag[1] == 's' || tag[1] == 'S')) {
            int cells = 1;
            ImageRef img = renderStringImage(tag.substr(2), cells);
            if (!img) return false;
            out.image = img;
            out.cells = cells;
            out.cellWidth = img->w / std::max(1, cells);
            out.loop = 3;
            return true;
        }
        size_t p = 1;
        if (p < tag.size()) {
            char m = char(tolower(uint8_t(tag[p])));
            if (m == 'a' || m == 'l' || m == 'r' || m == 'c') {
                mode = std::string(1, m);
                p++;
            } else if (m == 'm') {
                mode = "m";
                size_t end = tag.find(';', p);
                maskFile = tag.substr(p + 1, end == std::string::npos ? std::string::npos : end - p - 1);
                p = end == std::string::npos ? tag.size() : end;
            } else if (m == '#') {
                mode = "#";
                keyColor = parseColor(tag.substr(p));
                keyByColor = true;
                p += 7;
            } else if (m == '!') {
                mode = "c";  // palette-index keys: modern images have no palette to key on
                while (p < tag.size() && tag[p] != '/' && tag[p] != ';') p++;
            }
        }
        if (p < tag.size() && tag[p] == '/') {
            // /cells,time,loop  or /cells,,loop  or /cells
            p++;
            out.cells = std::max(1, atoi(tag.c_str() + p));
            while (p < tag.size() && isdigit(uint8_t(tag[p]))) p++;
            int time = 0;
            out.loop = 3;
            if (p < tag.size() && tag[p] == ',') {
                p++;
                if (tag[p] == '<') {
                    // per-cell times <100,200,...>
                    p++;
                    while (p < tag.size() && tag[p] != '>') {
                        out.cellTimes.push_back(atoi(tag.c_str() + p));
                        while (p < tag.size() && tag[p] != ',' && tag[p] != '>') p++;
                        if (tag[p] == ',') p++;
                    }
                    if (p < tag.size()) p++;
                } else {
                    time = atoi(tag.c_str() + p);
                    while (p < tag.size() && isdigit(uint8_t(tag[p]))) p++;
                }
                out.loop = 0;
                if (p < tag.size() && tag[p] == ',') {
                    p++;
                    out.loop = atoi(tag.c_str() + p);
                }
            }
            if (out.cellTimes.empty()) out.cellTimes.assign(size_t(out.cells), time);
        }
        semi = tag.find(';', p);
        file = semi == std::string::npos ? tag.substr(p) : tag.substr(semi + 1);
    }
    bool ownAlpha = false;
    ImageRef base = loadFileImage(file, &ownAlpha);
    if (!base) return false;
    // An image with an alpha channel of its own (a PNG) keeps it, whatever transparency the tag names.
    if (ownAlpha) mode = "c";
    if (fileLogOn_) noteFileUsed(file);
    Image img = *base;
    int cells = std::max(1, out.cells);
    if (mode == "a") {
        // Alpha bitmaps: each cell is its image followed by an equally wide grayscale mask, white = transparent.
        int cw = img.w / cells, half = cw / 2;
        Image res(half * cells, img.h);
        for (int c = 0; c < cells; c++)
            for (int y = 0; y < img.h; y++) {
                const uint32_t *src = img.row(y) + c * cw;
                uint32_t *dst = res.row(y) + c * half;
                for (int x = 0; x < half; x++) {
                    uint32_t p = src[x], m = src[x + half];
                    int a = 255 - (chR(m) + chG(m) * 2 + chB(m)) / 4;
                    dst[x] = rgba(int(mul255(chR(p), a)), int(mul255(chG(p), a)), int(mul255(chB(p), a)), a);
                }
            }
        img = std::move(res);
    } else if (mode == "m") {
        ImageRef mask = loadFileImage(maskFile);
        if (mask)
            for (int y = 0; y < img.h; y++)
                for (int x = 0; x < img.w; x++) {
                    uint32_t m = mask->row(y % mask->h)[x % mask->w];
                    int a = 255 - (chR(m) + chG(m) * 2 + chB(m)) / 4;
                    img.row(y)[x] = scalePx(img.row(y)[x], uint32_t(a));
                }
    } else if (mode == "l" || mode == "r" || keyByColor) {
        // Color keys apply only to images without an alpha channel of their own.
        bool opaque = std::all_of(img.px.begin(), img.px.end(), [](uint32_t p) { return chA(p) == 255; });
        if (opaque && !img.empty()) {
            uint32_t key = keyByColor ? keyColor : (mode == "l" ? img.row(0)[0] : img.row(0)[img.w - 1]);
            key |= 0xFF000000u;
            for (uint32_t &p : img.px)
                if (p == key) p = 0;
        }
    }
    out.image = std::make_shared<const Image>(std::move(img));
    out.cellWidth = out.image->w / cells;
    return true;
}

void Engine::noteFileUsed(const std::string &file) { fileLog_.insert(normalizePath(file)); }

Sprite *Engine::spriteRef(int n) {
    if (n < 0 || n >= kSpriteCount) {
        errorf("sprite %d is out of range", n);
        return nullptr;
    }
    return &scene_.sp[size_t(n)];
}

// ---- effects ----

bool Engine::readEffectArgs(Effect &e) {
    // ",n" names a defined effect; ",type,duration[,mask]" gives one directly.
    if (!readComma()) {
        e = Effect{1, 0, ""};
        return false;
    }
    int n = readInt();
    if (readComma()) {
        e.type = n;
        e.duration = readInt();
        e.mask.clear();
        if (readComma()) e.mask = readStr();
        return true;
    }
    if (n == 0) {
        e.type = 0;
        return true;
    }
    if (n == 1) {
        e = Effect{1, 0, ""};
        return true;
    }
    auto it = effects_.find(n);
    if (it == effects_.end()) {
        errorf("effect %d is not defined", n);
        e = Effect{1, 0, ""};
    } else {
        e = it->second;
    }
    return true;
}

void Engine::commitScene() {
    shown_ = scene_;
    sceneDirty_ = true;
}

void Engine::printWithEffect(int effectNo) {
    Effect e{1, 0, ""};
    if (effectNo == 0) return;
    if (effectNo != 1) {
        auto it = effects_.find(effectNo);
        if (it != effects_.end()) e = it->second;
    }
    printWithEffect(e);
}

void Engine::printWithEffect(const Effect &e) {
    if (e.type == 0) return;
    if (eraseOnEffect_ && windowShown_) {
        windowShown_ = false;
        windowHiddenByEffect_ = true;
    }
    if (e.type == 1 || e.duration <= 0 || (effectCut_ && (skipMode_ || ctrlSkip()))) {
        commitScene();
        composeFrame();
        return;
    }
    composeFrame();
    effect_.from = frame_;
    commitScene();
    effect_.e = e;
    effect_.mask = e.mask.empty() ? nullptr : loadFileImage(e.mask);
    effect_.start = now_;
    effect_.active = true;
    wait_ = WaitKind::Effect;
    waitSkippable_ = effectSkip_;
    waitUntil_ = now_ + e.duration;
}

void Engine::finishEffect() {
    effect_.active = false;
    effect_.from = Image();
    effect_.mask = nullptr;
    sceneDirty_ = true;
}

// Blends effect_.from into `out` (the new frame) at progress t in [0, 1].
void Engine::drawEffect(Image &out, double t) {
    const Image &from = effect_.from;
    if (from.w != out.w || from.h != out.h) return;
    int W = out.w, H = out.h;
    int type = effect_.e.type;
    auto keepOld = [&](int x, int y) { out.row(y)[x] = from.row(y)[x]; };
    switch (type) {
    case 2: case 3: case 4: case 5: {
        // Shutters: the new image opens in 40-pixel strips.
        const int strip = 40;
        int open = int(t * strip);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int k = type == 2 ? x % strip : type == 3 ? strip - 1 - x % strip : type == 4 ? y % strip : strip - 1 - y % strip;
                if (k >= open) keepOld(x, y);
            }
        break;
    }
    case 6: case 7: case 8: case 9: {
        // Curtains: strips open in turn along the direction, each over part of the time.
        const int strip = 40;
        bool horizontal = type == 6 || type == 7;
        int span = horizontal ? W : H;
        double front = t * (span + strip);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int along = type == 6 ? x : type == 7 ? W - 1 - x : type == 8 ? y : H - 1 - y;
                int k = along % strip;
                int stripStart = along - k;
                double opened = front - stripStart;
                if (k >= opened) keepOld(x, y);
            }
        break;
    }
    case 11: case 12: case 13: case 14: {
        // Scrolls: the new image slides in, pushing the old one out.
        int dx = 0, dy = 0;
        if (type == 11) dx = int(t * W);
        if (type == 12) dx = -int(t * W);
        if (type == 13) dy = int(t * H);
        if (type == 14) dy = -int(t * H);
        Image next = out;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int sx = x - dx, sy = y - dy;
                if (sx >= 0 && sx < W && sy >= 0 && sy < H) out.row(y)[x] = from.row(sy)[sx];
                else {
                    int nx = type == 11 ? sx + W : type == 12 ? sx - W : sx;
                    int ny = type == 13 ? sy + H : type == 14 ? sy - H : sy;
                    out.row(y)[x] = next.row(std::clamp(ny, 0, H - 1))[std::clamp(nx, 0, W - 1)];
                }
            }
        break;
    }
    case 15: case 18: {
        const Image *mask = effect_.mask.get();
        if (!mask || mask->empty()) {
            int k = int(t * 256);
            for (int y = 0; y < H; y++)
                for (int x = 0; x < W; x++) out.row(y)[x] = lerpPx(from.row(y)[x], out.row(y)[x], k);
            break;
        }
        // Dark mask pixels change first. 18 cross-fades each pixel over a 64-level window.
        const int range = type == 18 ? 64 : 1;
        int level = int(t * (256 + range));
        for (int y = 0; y < H; y++) {
            const uint32_t *m = mask->row(y % mask->h);
            for (int x = 0; x < W; x++) {
                int v = chG(m[x % mask->w]);
                int k = std::clamp((level - v) * 256 / range, 0, 256);
                out.row(y)[x] = lerpPx(from.row(y)[x], out.row(y)[x], k);
            }
        }
        break;
    }
    case 16: case 17: {
        // Mosaic out (16) grows blocks over the old image; mosaic in (17) shrinks them over the new.
        const Image &src = type == 16 ? from : Image(out);
        Image copy = src;
        int block = std::max(1, int((type == 16 ? t : 1 - t) * 40) + 1);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) out.row(y)[x] = copy.row(y - y % block)[x - x % block];
        break;
    }
    default: {
        int k = int(t * 256);
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) out.row(y)[x] = lerpPx(from.row(y)[x], out.row(y)[x], k);
    }
    }
}

// ---- compositing ----

static void drawSprite(Image &out, const Sprite &s, int ox = 0, int oy = 0) {
    if (!s.loaded || !s.visible || !s.img.image) return;
    const Image &img = *s.img.image;
    int cw = s.img.cellWidth > 0 ? s.img.cellWidth : img.w;
    int cell = std::clamp(s.cell, 0, std::max(0, s.img.cells - 1));
    blit(out, img, s.x + ox, s.y + oy, cell * cw, 0, cw, img.h, s.alpha);
}

void Engine::renderScene(const Scene &s, Image &out, bool withWindow) {
    if (out.w != width_ || out.h != height_) out = Image(width_, height_);
    if (s.bg.image) {
        std::fill(out.px.begin(), out.px.end(), rgba(0, 0, 0));
        const Image &bg = *s.bg.image;
        // Backgrounds smaller than the screen are centred, as NScripter does.
        blit(out, bg, (width_ - bg.w) / 2, (height_ - bg.h) / 2, 0, 0, bg.w, bg.h, 255);
    } else {
        std::fill(out.px.begin(), out.px.end(), s.bgColor);
    }
    int hz = std::clamp(humanz_, -1, kSpriteCount - 1);
    if (!s.allSpHidden)
        for (int i = kSpriteCount - 1; i >= hz && i >= 0; i--) drawSprite(out, s.sp[size_t(i)]);
    // Standing images: the first letter of humanorder is drawn on top.
    for (int k = int(humanOrder_.size()) - 1; k >= 0; k--) {
        char c = humanOrder_[size_t(k)];
        int idx = c == 'l' ? 0 : c == 'c' ? 1 : c == 'r' ? 2 : -1;
        if (idx >= 0) drawSprite(out, s.ld[size_t(idx)]);
    }
    bool effectsOnScene = s.monocro >= 0 || s.nega;
    if (withWindow && windowBack_) drawWindow(out);
    if (!s.allSpHidden)
        for (int i = std::min(hz - 1, kSpriteCount - 1); i >= 0; i--) drawSprite(out, s.sp[size_t(i)]);
    if (!s.allSp2Hidden)
        for (int i = kSprite2Count - 1; i >= 0; i--) {
            const Sprite2 &q = s.sp2[size_t(i)];
            if (!q.loaded || !q.visible || !q.img.image) continue;
            const Image &img = *q.img.image;
            int cw = q.img.cellWidth > 0 ? q.img.cellWidth : img.w;
            double rad = q.rot * M_PI / 180.0;
            double sx = q.scaleX / 100.0, sy = q.scaleY / 100.0;
            // Counter-clockwise on screen, with y pointing down.
            double m[4] = {std::cos(rad) * sx, std::sin(rad) * sy, -std::sin(rad) * sx, std::cos(rad) * sy};
            blitTransformed(out, img, std::clamp(q.cell, 0, q.img.cells - 1) * cw, 0, cw, img.h, q.cx, q.cy, m,
                            q.alpha, q.blend);
        }
    for (const Scene::Bar &b : s.bars)
        if (b.on && b.max > 0) fillRect(out, b.x, b.y, b.w * std::clamp(b.value, 0, b.max) / b.max, b.h, b.color);
    drawLayers(out);
    if (effectsOnScene) {
        for (uint32_t &p : out.px) {
            int r = chR(p), g = chG(p), b = chB(p);
            if (s.nega) {
                r = 255 - r;
                g = 255 - g;
                b = 255 - b;
            }
            if (s.monocro >= 0) {
                int y = (r * 77 + g * 151 + b * 28) >> 8;
                uint32_t c = uint32_t(s.monocro);
                r = int(mul255(uint32_t(y), uint32_t(chR(c))));
                g = int(mul255(uint32_t(y), uint32_t(chG(c))));
                b = int(mul255(uint32_t(y), uint32_t(chB(c))));
            }
            p = rgba(r, g, b);
        }
    }
    if (withWindow && !windowBack_) drawWindow(out);
}

void Engine::animateSprites() {
    auto step = [&](Sprite &s) {
        if (!s.loaded || s.img.cells <= 1 || s.img.loop == 3 || s.img.cellTimes.empty()) return false;
        int t = s.img.cellTimes[size_t(std::clamp(s.cell, 0, int(s.img.cellTimes.size()) - 1))];
        if (t <= 0 || now_ - s.animClock < t) return false;
        s.animClock = now_;
        int next = s.cell + s.animDir;
        if (next >= s.img.cells) {
            if (s.img.loop == 0) next = 0;
            else if (s.img.loop == 1) next = s.img.cells - 1;
            else {
                s.animDir = -1;
                next = std::max(0, s.img.cells - 2);
            }
        } else if (next < 0) {
            s.animDir = 1;
            next = std::min(1, s.img.cells - 1);
        }
        if (next == s.cell) return false;
        s.cell = next;
        return true;
    };
    bool changed = false;
    for (int i = 0; i < kSpriteCount; i++) {
        bool a = step(shown_.sp[size_t(i)]);
        if (a) {
            scene_.sp[size_t(i)].cell = shown_.sp[size_t(i)].cell;
            scene_.sp[size_t(i)].animClock = shown_.sp[size_t(i)].animClock;
            scene_.sp[size_t(i)].animDir = shown_.sp[size_t(i)].animDir;
        }
        changed |= a;
    }
    for (size_t i = 0; i < 3; i++) changed |= step(shown_.ld[i]);
    if (changed) sceneDirty_ = true;
}

void Engine::composeFrame() {
    renderScene(shown_, frame_, true);
    if (effect_.active) {
        double t = effect_.e.duration > 0 ? double(now_ - effect_.start) / effect_.e.duration : 1;
        drawEffect(frame_, std::clamp(t, 0.0, 1.0));
    }
    drawOverlays(frame_);
    drawMovie(frame_);
    if (quakeX_ || quakeY_) {
        Image shifted(frame_.w, frame_.h, rgba(0, 0, 0));
        blit(shifted, frame_, quakeX_, quakeY_, 0, 0, frame_.w, frame_.h, 255);
        frame_ = std::move(shifted);
    }
    frameChanged_ = true;
    sceneDirty_ = false;
}

// ---- commands ----

namespace {
using E = Engine;

void cmdBg(E &e, const std::string &) {
    e.skipSpaces();
    std::string s = e.readStr();
    std::string low = sjisLower(s);
    e.scene_.bgTag = s;
    e.scene_.bg = LoadedImage();
    if (low == "black") {
        e.scene_.bgColor = rgba(0, 0, 0);
    } else if (low == "white") {
        e.scene_.bgColor = rgba(255, 255, 255);
    } else if (!s.empty() && s[0] == '#') {
        e.scene_.bgColor = parseColor(s);
    } else {
        LoadedImage img;
        if (e.loadImage(s, img)) e.scene_.bg = img;
        e.scene_.bgColor = rgba(0, 0, 0);
    }
    // A new background clears the standing images.
    for (Sprite &s2 : e.scene_.ld) s2 = Sprite();
    Effect ef;
    e.readEffectArgs(ef);
    e.printWithEffect(ef);
}

int ldIndex(E &e) {
    std::string pos = e.readWord();
    return pos == "l" ? 0 : pos == "r" ? 2 : pos == "c" ? 1 : pos == "a" ? 3 : -1;
}

void placeStanding(E &e, int idx) {
    Sprite &s = e.scene_.ld[size_t(idx)];
    if (!s.img.image) return;
    int w = s.img.cellWidth, h = s.img.image->h;
    int center = idx == 0 ? e.width_ / 4 : idx == 1 ? e.width_ / 2 : e.width_ * 3 / 4;
    s.x = center - w / 2;
    s.y = e.underline_ - h + 1;
}

void cmdLd(E &e, const std::string &) {
    int idx = ldIndex(e);
    e.readComma();
    std::string tag = e.readStr();
    if (idx >= 0 && idx < 3) {
        Sprite &s = e.scene_.ld[size_t(idx)];
        s = Sprite();
        if (e.loadImage(tag, s.img)) {
            s.loaded = s.visible = true;
            s.animClock = e.now_;
            placeStanding(e, idx);
        }
    }
    Effect ef;
    e.readEffectArgs(ef);
    e.printWithEffect(ef);
}

void cmdCl(E &e, const std::string &) {
    int idx = ldIndex(e);
    if (idx == 3) {
        for (Sprite &s : e.scene_.ld) s = Sprite();
    } else if (idx >= 0) {
        e.scene_.ld[size_t(idx)] = Sprite();
    }
    Effect ef;
    e.readEffectArgs(ef);
    e.printWithEffect(ef);
}

void cmdTal(E &e, const std::string &) {
    int idx = ldIndex(e);
    e.readComma();
    int a = e.readInt();
    if (idx >= 0 && idx < 3) e.scene_.ld[size_t(idx)].alpha = std::clamp(a, 0, 255);
    Effect ef;
    if (e.readEffectArgs(ef)) e.printWithEffect(ef);
}

void cmdPrint(E &e, const std::string &) {
    e.skipSpaces();
    Effect ef{1, 0, ""};
    int n = e.readInt();
    if (e.readComma()) {
        ef.type = n;
        ef.duration = e.readInt();
        if (e.readComma()) ef.mask = e.readStr();
    } else if (n == 0) {
        return;
    } else if (n != 1) {
        auto it = e.effects_.find(n);
        if (it != e.effects_.end()) ef = it->second;
    }
    e.printWithEffect(ef);
}

void cmdEffect(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    Effect ef;
    ef.type = e.readInt();
    if (e.readComma()) ef.duration = e.readInt();
    if (e.readComma()) ef.mask = e.readStr();
    e.effects_[n] = ef;
}

void cmdWindowEffect(E &e, const std::string &) {
    Effect ef;
    ef.type = e.readInt();
    if (e.readComma()) ef.duration = e.readInt();
    if (e.readComma()) ef.mask = e.readStr();
    e.windowEffect_ = ef;
}

void loadSprite(E &e, bool visible) {
    int n = e.readInt();
    e.readComma();
    std::string tag = e.readStr();
    int x = 0, y = 0, alpha = 255;
    if (e.readComma()) x = e.readInt();
    if (e.readComma()) y = e.readInt();
    if (e.readComma()) alpha = e.readInt();
    Sprite *s = e.spriteRef(n);
    if (!s) return;
    *s = Sprite();
    if (!e.loadImage(tag, s->img)) {
        s->img.tag = tag;
        return;
    }
    s->loaded = true;
    s->visible = visible;
    s->x = x;
    s->y = y;
    s->alpha = std::clamp(alpha, 0, 255);
    s->animClock = e.now_;
}

void cmdLsp(E &e, const std::string &name) { loadSprite(e, name == "lsp"); }

void cmdCsp(E &e, const std::string &) {
    int n = e.readInt();
    if (n == -1) {
        for (Sprite &s : e.scene_.sp) s = Sprite();
    } else if (Sprite *s = e.spriteRef(n)) {
        *s = Sprite();
    }
}

void cmdVsp(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    int v = e.readInt();
    if (n == -1) {
        for (Sprite &s : e.scene_.sp) s.visible = v != 0 && s.loaded;
    } else if (Sprite *s = e.spriteRef(n)) {
        s->visible = v != 0;
    }
}

void cmdMsp(E &e, const std::string &name) {
    int n = e.readInt();
    e.readComma();
    int x = e.readInt();
    e.readComma();
    int y = e.readInt();
    bool hasAlpha = e.readComma();
    int a = hasAlpha ? e.readInt() : 0;
    Sprite *s = e.spriteRef(n);
    if (!s) return;
    if (name == "amsp") {
        s->x = x;
        s->y = y;
        if (hasAlpha) s->alpha = std::clamp(a, 0, 255);
    } else {
        s->x += x;
        s->y += y;
        s->alpha = std::clamp(s->alpha + a, 0, 255);
    }
}

void cmdCell(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    int c = e.readInt();
    if (Sprite *s = e.spriteRef(n)) s->cell = std::clamp(c, 0, std::max(0, s->img.cells - 1));
}

void cmdAllSp(E &e, const std::string &name) {
    if (name == "allsphide") e.scene_.allSpHidden = true;
    else if (name == "allspresume") e.scene_.allSpHidden = false;
    else if (name == "allsp2hide") e.scene_.allSp2Hidden = true;
    else e.scene_.allSp2Hidden = false;
}

void cmdGetSpSize(E &e, const std::string &) {
    int n = e.readInt();
    Sprite *s = e.spriteRef(n);
    int w = s && s->img.image ? s->img.cellWidth : 0, h = s && s->img.image ? s->img.image->h : 0;
    e.readComma();
    e.setVar(e.readVar(), w);
    e.readComma();
    e.setVar(e.readVar(), h);
    if (e.readComma()) e.setVar(e.readVar(), s ? s->img.cells : 0);
}

void cmdGetSpMode(E &e, const std::string &) {
    E::VarRef v = e.readVar();
    e.readComma();
    Sprite *s = e.spriteRef(e.readInt());
    e.setVar(v, s && s->visible ? 1 : 0);
}

void cmdSpStr(E &e, const std::string &) { e.applySpStr(e.readStr()); }

void cmdSpWait(E &e, const std::string &) {
    // Waits for the sprite's animation to finish one round.
    Sprite *s = e.spriteRef(e.readInt());
    if (!s || s->img.cells <= 1 || s->img.cellTimes.empty()) return;
    int total = 0;
    for (int t : s->img.cellTimes) total += t;
    e.waitTimer(total, false);
}

void cmdMonocro(E &e, const std::string &) {
    e.skipSpaces();
    std::string s = e.readStr();
    e.scene_.monocro = sjisLower(s) == "off" ? -1 : int(parseColor(s));
}

void cmdNega(E &e, const std::string &) { e.scene_.nega = e.readInt(); }

void cmdQuake(E &e, const std::string &name) {
    int a = e.readInt();
    e.readComma();
    int ms = e.readInt();
    e.quakeAmp_ = std::max(1, a);
    e.quakeDuration_ = ms;
    e.quakeEnd_ = e.now_ + ms;
    e.quakeKind_ = name == "quakex" ? 1 : name == "quakey" ? 2 : 0;
    e.commitScene();
    e.wait_ = WaitKind::Quake;
    e.waitUntil_ = e.quakeEnd_;
    e.waitSkippable_ = false;
}

void cmdHumanOrder(E &e, const std::string &) {
    e.humanOrder_ = sjisLower(e.readStr());
    Effect ef;
    e.readEffectArgs(ef);
    e.printWithEffect(ef);
}

void cmdBar(E &e, const std::string &) {
    int n = e.readInt();
    Scene::Bar b;
    b.on = true;
    e.readComma();
    b.value = e.readInt();
    e.readComma();
    b.x = e.readInt();
    e.readComma();
    b.y = e.readInt();
    e.readComma();
    b.w = e.readInt();
    e.readComma();
    b.h = e.readInt();
    e.readComma();
    b.max = e.readInt();
    e.readComma();
    b.color = parseColor(e.readStr());
    if (n >= 0 && n < int(e.scene_.bars.size())) e.scene_.bars[size_t(n)] = b;
}

void cmdBarClear(E &e, const std::string &) {
    for (auto &b : e.scene_.bars) b = Scene::Bar();
}

void loadSprite2(E &e, bool visible, Blend blend) {
    int n = e.readInt();
    e.readComma();
    std::string tag = e.readStr();
    Sprite2 q;
    e.readComma();
    q.cx = e.readInt();
    e.readComma();
    q.cy = e.readInt();
    e.readComma();
    q.scaleX = e.readInt();
    e.readComma();
    q.scaleY = e.readInt();
    e.readComma();
    q.rot = e.readInt();
    if (e.readComma()) q.alpha = std::clamp(e.readInt(), 0, 255);
    q.blend = blend;
    q.visible = visible;
    if (n < 0 || n >= kSprite2Count) return;
    if (e.loadImage(tag, q.img)) q.loaded = true;
    e.scene_.sp2[size_t(n)] = q;
}

void cmdLsp2(E &e, const std::string &name) {
    bool visible = name.rfind("lsph", 0) != 0;
    Blend blend = name.find("add") != std::string::npos ? Blend::Add : name.find("sub") != std::string::npos ? Blend::Sub : Blend::Normal;
    loadSprite2(e, visible, blend);
}

void cmdCsp2(E &e, const std::string &) {
    int n = e.readInt();
    if (n == -1) {
        for (auto &q : e.scene_.sp2) q = Sprite2();
    } else if (n >= 0 && n < kSprite2Count) {
        e.scene_.sp2[size_t(n)] = Sprite2();
    }
}

void cmdVsp2(E &e, const std::string &) {
    int n = e.readInt();
    e.readComma();
    int v = e.readInt();
    if (n == -1) {
        for (auto &q : e.scene_.sp2) q.visible = v != 0 && q.loaded;
    } else if (n >= 0 && n < kSprite2Count) {
        e.scene_.sp2[size_t(n)].visible = v != 0;
    }
}

void cmdMsp2(E &e, const std::string &name) {
    int n = e.readInt();
    int v[6] = {0, 0, 0, 0, 0, 0};
    bool hasAlpha = false;
    for (int i = 0; i < 5; i++) {
        e.readComma();
        v[i] = e.readInt();
    }
    if (e.readComma()) {
        v[5] = e.readInt();
        hasAlpha = true;
    }
    if (n < 0 || n >= kSprite2Count) return;
    Sprite2 &q = e.scene_.sp2[size_t(n)];
    if (name == "amsp2") {
        q.cx = v[0];
        q.cy = v[1];
        q.scaleX = v[2];
        q.scaleY = v[3];
        q.rot = v[4];
        if (hasAlpha) q.alpha = std::clamp(v[5], 0, 255);
    } else {
        q.cx += v[0];
        q.cy += v[1];
        q.scaleX += v[2];
        q.scaleY += v[3];
        q.rot += v[4];
        q.alpha = std::clamp(q.alpha + v[5], 0, 255);
    }
}

void cmdBgCopy(E &e, const std::string &) {
    // The current screen becomes the background.
    Image shot;
    e.renderScene(e.shown_, shot, false);
    e.scene_.bg = LoadedImage();
    e.scene_.bg.tag = "*bgcpy";
    e.scene_.bg.image = std::make_shared<const Image>(std::move(shot));
    e.scene_.bg.cellWidth = e.width_;
}

void cmdEraseTextWindow(E &e, const std::string &) { e.eraseOnEffect_ = e.readInt() != 0; }

}  // namespace

// exbtn and spstr control strings: Cn hides sprite n (or Cn-m a range), Pn shows it, Pn,c shows cell c,
// Sch,(file) plays a sound, Mn,x,y moves a sprite.
void Engine::applySpStr(const std::string &ctrl) {
    size_t p = 0;
    auto num = [&]() {
        bool neg = p < ctrl.size() && ctrl[p] == '-';
        if (neg) p++;
        int v = 0;
        while (p < ctrl.size() && isdigit(uint8_t(ctrl[p]))) v = v * 10 + (ctrl[p++] - '0');
        return neg ? -v : v;
    };
    while (p < ctrl.size()) {
        char c = char(toupper(uint8_t(ctrl[p++])));
        if (c == 'C') {
            int a = num(), b = a;
            if (p < ctrl.size() && ctrl[p] == '-') {
                p++;
                b = num();
            }
            for (int i = a; i <= b; i++)
                if (i >= 0 && i < kSpriteCount) scene_.sp[size_t(i)].visible = false;
        } else if (c == 'P') {
            int n = num();
            int cell = -1;
            if (p < ctrl.size() && ctrl[p] == ',') {
                p++;
                cell = num();
            }
            if (n >= 0 && n < kSpriteCount) {
                Sprite &s = scene_.sp[size_t(n)];
                s.visible = s.loaded;
                if (cell >= 0) s.cell = std::clamp(cell, 0, std::max(0, s.img.cells - 1));
            }
        } else if (c == 'S') {
            int chn = num();
            std::string file;
            if (p < ctrl.size() && ctrl[p] == ',') p++;
            if (p < ctrl.size() && ctrl[p] == '(') {
                size_t end = ctrl.find(')', p);
                file = ctrl.substr(p + 1, end == std::string::npos ? std::string::npos : end - p - 1);
                p = end == std::string::npos ? ctrl.size() : end + 1;
            }
            if (!file.empty()) playSound(chn, file, false);
        } else if (c == 'M') {
            int n = num();
            int x = 0, y = 0;
            if (p < ctrl.size() && ctrl[p] == ',') {
                p++;
                x = num();
            }
            if (p < ctrl.size() && ctrl[p] == ',') {
                p++;
                y = num();
            }
            if (n >= 0 && n < kSpriteCount) {
                scene_.sp[size_t(n)].x = x;
                scene_.sp[size_t(n)].y = y;
            }
        }
    }
}

void registerSceneCommands(std::unordered_map<std::string, CommandFn> &t) {
    t["bg"] = cmdBg;
    t["ld"] = cmdLd;
    t["cl"] = cmdCl;
    t["tal"] = cmdTal;
    t["print"] = cmdPrint;
    t["effect"] = cmdEffect;
    t["windoweffect"] = cmdWindowEffect;
    t["lsp"] = cmdLsp;
    t["lsph"] = cmdLsp;
    t["csp"] = cmdCsp;
    t["vsp"] = cmdVsp;
    t["msp"] = cmdMsp;
    t["amsp"] = cmdMsp;
    t["cell"] = cmdCell;
    for (const char *n : {"allsphide", "allspresume", "allsp2hide", "allsp2resume"}) t[n] = cmdAllSp;
    t["getspsize"] = cmdGetSpSize;
    t["getspmode"] = cmdGetSpMode;
    t["spstr"] = cmdSpStr;
    t["spwait"] = cmdSpWait;
    t["monocro"] = cmdMonocro;
    t["nega"] = cmdNega;
    t["quake"] = cmdQuake;
    t["quakex"] = cmdQuake;
    t["quakey"] = cmdQuake;
    t["humanorder"] = cmdHumanOrder;
    t["bar"] = cmdBar;
    t["barclear"] = cmdBarClear;
    for (const char *n : {"lsp2", "lsph2", "lsp2add", "lsph2add", "lsp2sub", "lsph2sub"}) t[n] = cmdLsp2;
    t["csp2"] = cmdCsp2;
    t["vsp2"] = cmdVsp2;
    t["msp2"] = cmdMsp2;
    t["amsp2"] = cmdMsp2;
    t["bgcopy"] = cmdBgCopy;
    t["bgcpy"] = cmdBgCopy;
    t["erasetextwindow"] = cmdEraseTextWindow;
}

}  // namespace vnns
