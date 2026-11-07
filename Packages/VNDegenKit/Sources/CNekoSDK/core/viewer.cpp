// The CG viewer's control panel, from the cg screen's `viewer...` lines of a game's menu table. Measured from the PC games
// under Wine: the panel's buttons step through an entry's pictures and between entries, play the scene the picture is
// from, zoom it, run a slideshow, and play the entry's music. A right click hides or shows the panel, and a click on the
// picture does nothing. A game whose table has no `viewer` line keeps the plain viewer, where a click shows the next picture.
#include "engine.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace neko {
namespace {

int number(const std::vector<std::string> &line, size_t index) { return index < line.size() ? std::atoi(line[index].c_str()) : 0; }

// A `viewerbutton` line's sheet columns and which is its: `COLS COL` after the place, or one column.
void cellOf(const std::vector<std::string> &f, int &cols, int &col) {
    bool given = f.size() >= 6 && std::isdigit(uint8_t(f[4][0]));
    cols = given ? std::max(1, number(f, 4)) : 1;
    col = given ? number(f, 5) : 0;
}

// A button's sheet rows, as the menus' are.
constexpr int kNormal = 0, kHover = 1, kSelected = 2, kDisabled = 4;
// Zoom in hundredths: from the picture's own size up to three times it, in twentieths, in every game measured.
constexpr int kZoomMin = 100, kZoomMax = 300, kZoomStep = 5;
// The slideshow moves on every 8 seconds unless the table says otherwise.
constexpr int kAutoMs = 8000;

}  // namespace

bool Engine::viewerPanel() const { return screen_ && screen_->name == "cg" && screen_->setting("viewer"); }

void Engine::viewerShow(size_t entry, size_t image) {
    bool newEntry = entry != viewEntry_ || !viewingImage_;
    viewEntry_ = entry;
    viewImage_ = image;
    viewAutoFrom_ = viewMs_;
    viewing_ = {cgEntries_[entry].images[image]};
    viewingIndex_ = 0;
    viewingImage_ = true;
    if (newEntry) viewerMusic();
    dirty_ = true;
}

// Opening a picture from the gallery: its entry's first unlocked one, with the panel up, the picture at its own size and
// the entry's music playing.
void Engine::openViewer(size_t entry) {
    const auto &images = cgEntries_[entry].images;
    for (size_t i = 0; i < images.size(); i++)
        if (seen(images[i])) {
            viewPanelHidden_ = false;
            viewAuto_ = false;
            viewZoom_ = kZoomMin;
            viewMuted_ = false;
            viewingImage_ = false;
            viewerShow(entry, i);
            return;
        }
}

// The entry's music, from cg_view.ini, plays at the music screen's volume while the viewer is open. Its button only mutes
// it, so it goes on playing underneath, as the PC's does.
void Engine::viewerMusic() {
    const auto *bgm = screen_ ? screen_->setting("viewerbgm") : nullptr;
    if (!bgm) return;
    const std::string &path = cgEntries_[viewEntry_].bgm;
    if (path == viewMusicPath_ && viewMusic_) return;
    stopViewerMusic();
    if (path.empty() || path == "0") return;
    Bytes data = files_.read(path);
    if (!data) return;
    viewMusic_ = audio_.open(data);
    if (!viewMusic_) return;
    viewMusicPath_ = path;
    audio_.setGroup(viewMusic_, 3);
    audio_.setLoops(viewMusic_, 0);
    audio_.fade(viewMusic_, 0, viewMuted_ ? 0 : 100, false);
    audio_.play(viewMusic_);
    if (titleMusic_) audio_.pause(titleMusic_, true);
}

void Engine::stopViewerMusic() {
    if (viewMusic_) {
        audio_.close(viewMusic_);
        viewMusic_ = 0;
    }
    viewMusicPath_.clear();
    if (titleMusic_) audio_.pause(titleMusic_, false);
}

void Engine::viewerVariant(int direction) {
    const auto &images = cgEntries_[viewEntry_].images;
    int n = int(images.size());
    for (int k = 1; k < n; k++) {
        int i = ((int(viewImage_) + direction * k) % n + n) % n;
        if (seen(images[size_t(i)])) {
            viewerShow(viewEntry_, size_t(i));
            return;
        }
    }
}

// Between entries with a picture unlocked, wrapping at the ends. Next lands on the entry's first unlocked picture.
// Previous does too within a gallery tab, but on the last when it crosses into another tab or wraps round.
void Engine::viewerCg(int direction) {
    int n = int(cgEntries_.size());
    for (int k = 1; k <= n; k++) {
        int at = int(viewEntry_) + direction * k;
        bool wrapped = at < 0 || at >= n;
        size_t e = size_t((at % n + n) % n);
        const auto &images = cgEntries_[e].images;
        int first = -1, last = -1;
        for (int i = 0; i < int(images.size()); i++)
            if (seen(images[size_t(i)])) {
                if (first < 0) first = i;
                last = i;
            }
        if (first < 0) continue;
        bool fromEnd = direction < 0 && (wrapped || cgEntries_[e].page != cgEntries_[viewEntry_].page);
        viewerShow(e, size_t(fromEnd ? last : first));
        return;
    }
}

void Engine::viewerAdvance() {
    const auto &images = cgEntries_[viewEntry_].images;
    for (size_t i = viewImage_ + 1; i < images.size(); i++)
        if (seen(images[i])) {
            viewerShow(viewEntry_, i);
            return;
        }
    viewerCg(1);
    viewAutoFrom_ = viewMs_;
}

// Return goes back to the gallery at the page of the picture shown.
void Engine::closeViewer() {
    viewingImage_ = false;
    viewAuto_ = false;
    stopViewerMusic();
    int perPage = std::max(1, int(screen_->settings.count("cell")));
    int before = int(viewEntry_);
    if (screen_->settings.count("gallerytabs")) {
        const GalleryEntry &e = cgEntries_[viewEntry_];
        before = int(std::count_if(cgEntries_.begin(), cgEntries_.begin() + std::ptrdiff_t(viewEntry_), [&](const GalleryEntry &o) { return o.page == e.page; }));
        viewerTab(e.page);
    }
    page_ = before / perPage;
    dirty_ = true;
}

// Scene shows, or works, only while the picture's scene is unlocked in the Scene gallery.
const Engine::GalleryEntry *Engine::viewerScene() const {
    const std::string &flow = cgEntries_[viewEntry_].flow;
    if (flow.empty()) return nullptr;
    for (const GalleryEntry &scene : sceneEntries_)
        if (normalizedPath(scene.flow) == normalizedPath(flow) && seen(scene.key)) return &scene;
    return nullptr;
}

// The picture fills the screen's width. Zoomed, the point of it under the pointer stays where it is. The art set picks
// which of The Alchemist's two versions of it comes up.
void Engine::composeViewerPicture(Canvas &out) {
    ImageRef image = viewing_.empty() ? nullptr : images_->get(viewing_[viewingIndex_]);
    if (!image) return;
    float base = float(out.w) / float(image->w), zoom = viewerPanel() ? float(viewZoom_) / 100 : 1;
    float w = float(image->w) * base, h = float(image->h) * base;
    float left = (float(out.w) - w) / 2, top = (float(out.h) - h) / 2;
    float px = pointerSeen_ ? float(pointerX_) : float(out.w) / 2, py = pointerSeen_ ? float(pointerY_) : float(out.h) / 2;
    left = px + (left - px) * zoom;
    top = py + (top - py) * zoom;
    drawImage(out, *image, left + w * zoom / 2, top + h * zoom / 2, base * zoom, base * zoom, 0, 1);
}

// A panel button's look: hidden, disabled, or its row for normal, hovered, selected and selected and hovered.
int Engine::viewerButtonRow(const std::vector<std::string> &f, bool over) const {
    const std::string &action = f[0];
    bool hideWhenOff = std::find(f.begin(), f.end(), "hide") != f.end();
    bool off = (action == "replay" && !viewerScene()) || ((action == "zoomout" || action == "zoomreset") && viewZoom_ <= kZoomMin) ||
               (action == "zoomin" && viewZoom_ >= kZoomMax) || (action == "bgm" && !viewMusic_);
    if (off) return hideWhenOff ? -1 : kDisabled;
    if (action == "auto" && viewAuto_) return kSelected + (over ? 1 : 0);
    return over ? kHover : kNormal;
}

const std::vector<std::string> *Engine::viewerButtonAt(int x, int y) const {
    for (auto [it, end] = screen_->settings.equal_range("viewerbutton"); it != end; ++it) {
        const auto &f = it->second;
        ImageRef sheet = f.size() >= 4 ? images_->get(f[1]) : nullptr;
        if (!sheet) continue;
        int cols, col;
        cellOf(f, cols, col);
        int w = sheet->w / cols, h = sheet->h / 5;
        if (x >= number(f, 2) && y >= number(f, 3) && x < number(f, 2) + w && y < number(f, 3) + h) return &f;
    }
    return nullptr;
}

// Table lines, all on the cg screen:
//   `viewer PANEL X Y`
//   `viewerbutton ACTION SHEET X Y [COLS COL] [help ROW] [helppic PICTURE X Y] [hide]`, five rows to a sheet. ACTION is
//     replay, prevcg, nextcg, prevvar, nextvar, auto, return, hide, zoomout, zoomreset, zoomin, bgm or art, which switches
//     the game's art set. `hide` leaves it out, rather than disabled, while it can't be used.
//   `viewerhelp SHEET X Y ROWS`, where a hovered button's help row goes.
//   `viewernumber WHAT SHEET X Y STEP DIGITS [CELLS]`, WHAT being index, total, zoom (the whole part), zoomfrac or volume.
//   `viewercell SHEET X Y CELLS CELL`, a fixed cell, such as a slash.
//   `viewerslider SHEET X Y [ROW]`, the music volume's fill, with its help row.
//   `viewerauto EVERY FILL` in ms: with FILL, the slideshow button fills from the right over FILL ms, as Test Game 84's does.
//   `viewerbgm PLAYCOL PLAYROW MUTECOL MUTEROW`, the music button's column and row playing and muted.
void Engine::composeViewer(Canvas &out) {
    if (!viewerPanel() || viewPanelHidden_) return;
    const auto *panel = screen_->setting("viewer");
    if (ImageRef art = panel->size() >= 3 ? images_->get((*panel)[0]) : nullptr) drawPart(out, *art, number(*panel, 1), number(*panel, 2), 0, 0, art->w, art->h, 1);
    const auto *bgmLook = screen_->setting("viewerbgm");
    const auto *timing = screen_->setting("viewerauto");
    int helpRow = -1;
    const std::string *helpPicture = nullptr;  // `helppic PICTURE X Y`, a help that isn't a row of the sheet
    for (auto [it, end] = screen_->settings.equal_range("viewerbutton"); it != end; ++it) {
        const auto &f = it->second;
        ImageRef sheet = f.size() >= 4 ? images_->get(f[1]) : nullptr;
        if (!sheet) continue;
        int cols, col;
        cellOf(f, cols, col);
        int x = number(f, 2), y = number(f, 3), w = sheet->w / cols, h = sheet->h / 5;
        bool over = pointerSeen_ && x <= pointerX_ && y <= pointerY_ && pointerX_ < x + w && pointerY_ < y + h;
        int row = viewerButtonRow(f, over);
        if (row < 0) continue;
        if (f[0] == "bgm" && row != kDisabled && bgmLook && bgmLook->size() >= 4) {
            col = number(*bgmLook, viewMuted_ ? 2 : 0);
            row = number(*bgmLook, viewMuted_ ? 3 : 1) + (over ? 1 : 0);
        }
        // As the message window's art button: ORIGINAL.ver while the arranged art shows, ARRANGE.ver while the original does.
        if (f[0] == "art") row = (setting("art") ? 2 : 0) + (over ? 1 : 0);
        if (over && row != kDisabled)
            for (size_t i = 4; i + 1 < f.size(); i++) {
                if (f[i] == "help") helpRow = number(f, i + 1);
                if (f[i] == "helppic" && i + 3 < f.size()) helpPicture = &f[i + 1];
            }
        int fillMs = timing && timing->size() >= 2 ? number(*timing, 1) : 0;
        if (f[0] != "auto" || !viewAuto_ || fillMs <= 0) {
            drawPart(out, *sheet, x, y, col * w, row * h, w, h, 1);
            continue;
        }
        // Test Game 84's slideshow button fills from the right with its selected look as the next picture nears. The
        // rest is its hovered look tinted toward red, 100 of 256.
        int left = w, right = 0;
        for (int cx = 0; cx < w; cx++)
            for (int cy = 0; cy < h; cy++)
                if (sheet->rgba[(size_t(2 * h + cy) * size_t(sheet->w) + size_t(col * w + cx)) * 4 + 3] >= 250) {
                    left = std::min(left, cx);
                    right = std::max(right, cx + 1);
                    break;
                }
        float done = std::clamp(float(viewMs_ - viewAutoFrom_) / float(fillMs), 0.0f, 1.0f);
        int split = std::max(0, right - int(float(right - left) * done + 0.5f));
        drawPart(out, *sheet, x, y, col * w, 3 * h, split, h, 1);
        for (int cy = 0; cy < h && y + cy < out.h; cy++)
            for (int cx = 0; cx < split && x + cx < out.w; cx++) {
                int a = sheet->rgba[(size_t(3 * h + cy) * size_t(sheet->w) + size_t(col * w + cx)) * 4 + 3];
                if (!a || x + cx < 0 || y + cy < 0) continue;
                uint8_t *d = &out.px[(size_t(y + cy) * size_t(out.w) + size_t(x + cx)) * 4];
                int k = 100 * a / 255;
                d[0] = uint8_t(d[0] + ((255 - d[0]) * k >> 8));
                d[1] = uint8_t(d[1] - (d[1] * k >> 8));
                d[2] = uint8_t(d[2] - (d[2] * k >> 8));
            }
        drawPart(out, *sheet, x + split, y, col * w + split, 2 * h, w - split, h, 1);
    }
    if (const auto *slider = screen_->setting("viewerslider"); slider && slider->size() >= 3)
        if (ImageRef fill = images_->get((*slider)[0])) {
            int x = number(*slider, 1), y = number(*slider, 2);
            drawPart(out, *fill, x, y, 0, 0, (fill->w * setting("music") + 50) / 100, fill->h, 1);
            if (slider->size() >= 4 && pointerSeen_ && pointerX_ >= x && pointerY_ >= y && pointerX_ < x + fill->w && pointerY_ < y + fill->h) helpRow = number(*slider, 3);
        }
    for (auto [it, end] = screen_->settings.equal_range("viewercell"); it != end; ++it) {
        const auto &f = it->second;
        ImageRef sheet = f.size() >= 5 ? images_->get(f[0]) : nullptr;
        if (!sheet) continue;
        int w = sheet->w / std::max(1, number(f, 3));
        drawPart(out, *sheet, number(f, 1), number(f, 2), number(f, 4) * w, 0, w, sheet->h, 1);
    }
    for (auto [it, end] = screen_->settings.equal_range("viewernumber"); it != end; ++it) {
        const auto &f = it->second;
        ImageRef sheet = f.size() >= 6 ? images_->get(f[1]) : nullptr;
        if (!sheet) continue;
        int cells = f.size() >= 7 ? std::max(10, number(f, 6)) : 10, cw = sheet->w / cells, digits = std::max(1, number(f, 5));
        int value = f[0] == "index" ? int(viewImage_) + 1 : f[0] == "total" ? int(cgEntries_[viewEntry_].images.size()) : f[0] == "zoom" ? viewZoom_ / 100 :
                    f[0] == "zoomfrac" ? viewZoom_ % 100 : setting("music");
        for (int i = digits - 1; i >= 0; i--, value /= 10)
            drawPart(out, *sheet, number(f, 2) + i * number(f, 4), number(f, 3), value % 10 * cw, 0, cw, sheet->h, 1);
    }
    if (helpPicture)
        if (ImageRef pic = images_->get(helpPicture[0])) drawPart(out, *pic, std::atoi(helpPicture[1].c_str()), std::atoi(helpPicture[2].c_str()), 0, 0, pic->w, pic->h, 1);
    if (const auto *help = screen_->setting("viewerhelp"); help && help->size() >= 4 && helpRow >= 0)
        if (ImageRef sheet = images_->get((*help)[0])) {
            int h = sheet->h / std::max(1, number(*help, 3));
            drawPart(out, *sheet, number(*help, 1), number(*help, 2), 0, helpRow * h, sheet->w, h, 1);
        }
}

void Engine::pressViewer(int x, int y) {
    if (viewPanelHidden_) return;
    if (const auto *slider = screen_->setting("viewerslider"); slider && slider->size() >= 3)
        if (ImageRef fill = images_->get((*slider)[0])) {
            int left = number(*slider, 1), top = number(*slider, 2);
            if (x >= left && y >= top && x < left + fill->w && y < top + fill->h) {
                changeSetting("music", std::clamp((x - left) * 100 / std::max(1, fill->w - 1), 0, 100));
                saveSettings();
                return;
            }
        }
    const auto *f = viewerButtonAt(x, y);
    if (!f || viewerButtonRow(*f, false) == kDisabled || viewerButtonRow(*f, false) < 0) return;
    const std::string &action = (*f)[0];
    if (action == "replay") {
        if (const GalleryEntry *scene = viewerScene()) {
            replayFromViewer_ = true;
            stopViewerMusic();
            startReplay(*scene);
        }
    } else if (action == "prevcg" || action == "nextcg") {
        viewerCg(action == "nextcg" ? 1 : -1);
    } else if (action == "prevvar" || action == "nextvar") {
        viewerVariant(action == "nextvar" ? 1 : -1);
    } else if (action == "auto") {
        viewAuto_ = !viewAuto_;
        viewAutoFrom_ = viewMs_;
    } else if (action == "return") {
        closeViewer();
    } else if (action == "hide") {
        viewPanelHidden_ = true;
    } else if (action == "zoomin" || action == "zoomout") {
        viewerZoom(action == "zoomin" ? 1 : -1);
    } else if (action == "zoomreset") {
        viewZoom_ = kZoomMin;
    } else if (action == "bgm") {
        viewMuted_ = !viewMuted_;
        if (viewMusic_) audio_.fade(viewMusic_, 0, viewMuted_ ? 0 : 100, false);
    } else if (action == "art") {
        // The same setting as Config's Change Illustration, as on the PC.
        runAction("ModeChange");
    }
    dirty_ = true;
}

void Engine::viewerZoom(int notches) {
    viewZoom_ = std::clamp(viewZoom_ + notches * kZoomStep, kZoomMin, kZoomMax);
    dirty_ = true;
}

// Called each frame while a menu is up: the viewer's clock and slideshow.
void Engine::stepViewer(int ms) {
    viewMs_ += ms;
    if (!viewingImage_ || !viewerPanel()) return;
    if (viewAuto_) {
        dirty_ = true;
        const auto *timing = screen_->setting("viewerauto");
        if (viewMs_ - viewAutoFrom_ >= (timing && !timing->empty() ? number(*timing, 0) : kAutoMs)) viewerAdvance();
    }
}

}  // namespace neko
