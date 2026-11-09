// Test Game 88's sprite gallery (立ち絵鑑賞), from its menu table's `sprites` screen and cfg\sc_view.ini, measured from
// the PC game under Wine. An explanation page first, then a heroine over a background, with pickers for the background,
// her face, outfit and pose and which heroine, each stepping with its arrows or chosen from a list its thumbnail opens;
// the outfit lock, which the story's sprites follow; music; a blurred background; zoom; the figure following the pointer
// up and down; and panels that hide a few seconds after they show unless pinned.
#include "engine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace neko {
namespace {

int number(const std::vector<std::string> &line, size_t index) { return index < line.size() ? std::atoi(line[index].c_str()) : 0; }

// The number in a file name's underscore-separated fields: `face_001_000_000_000_007.bmp` has 7 in field 5.
int nameField(const std::string &path, int index) {
    std::string name = path.substr(path.find_last_of('\\') + 1);
    size_t at = 0;
    for (int i = 0; i < index && at != std::string::npos; i++) at = name.find('_', at + 1);
    return at == std::string::npos ? 0 : std::atoi(name.c_str() + at + 1);
}

// A sheet's buttons' rows, as the menus' run.
constexpr int kNormal = 0, kHover = 1, kSelected = 2, kSelectedHover = 3;
// Zoom in hundredths, a twentieth a click, as the PC's ×1.05 and ×1.10 show.
constexpr int kZoomMin = 100, kZoomMax = 300, kZoomStep = 5;
// How long the panels take to slide off the screen or back, as the PC's timed shots show.
constexpr int kSlideMs = 300;

const std::vector<std::string> *listLine(const Screen &screen, const char *key, int picker) {
    for (auto [it, end] = screen.settings.equal_range(key); it != end; ++it)
        if (!it->second.empty() && std::atoi(it->second[0].c_str()) == picker) return &it->second;
    return nullptr;
}

std::pair<int, int> point(const std::string &text) {
    auto comma = text.find(',');
    return {std::atoi(text.c_str()), comma == std::string::npos ? 0 : std::atoi(text.c_str() + comma + 1)};
}

// The file with PREFIX before its name: obj\sc\akiho\face\face_... becomes obj\sc\akiho\face\thumb_face_....
std::string prefixed(const std::string &path, const std::string &prefix) {
    size_t at = path.find_last_of('\\') + 1;
    return path.substr(0, at) + prefix + path.substr(at);
}

// A copy of a picture with each colour multiplied, as the lists light a figure.
ImageRef tinted(const Image &image, const int *colour) {
    auto out = std::make_shared<Image>(image);
    for (size_t i = 0; i + 3 < out->rgba.size(); i += 4)
        for (size_t c = 0; c < 3; c++) out->rgba[i + c] = uint8_t(out->rgba[i + c] * colour[c] / 255);
    return out;
}

}  // namespace

bool Engine::spriteScreen() const { return screen_ && screen_->name == "sprites" && !spriteHeroines_.empty(); }

void Engine::openSprites() {
    sprite_ = {};
    sprite_.face.assign(spriteHeroines_.size(), 0);
    sprite_.pose.assign(spriteHeroines_.size(), 0);
    sprite_.restingSince = screenMs_;
    playSpriteMusic();
}

void Engine::closeSprites() {
    spriteTinted_.clear();
    if (spriteMusic_) audio_.close(spriteMusic_);
    spriteMusic_ = 0;
    holdMusic();
    saveGlobals();
}

void Engine::playSpriteMusic() {
    if (spriteMusic_) audio_.close(spriteMusic_);
    spriteMusic_ = 0;
    const auto *music = screen_->setting("spritemusic");
    if (sprite_.music && music && !music->empty())
        if (Bytes data = files_.read((*music)[0])) {
            spriteMusic_ = audio_.open(data);
            if (spriteMusic_) {
                audio_.setGroup(spriteMusic_, 0);
                audio_.setLoops(spriteMusic_, 0);
                audio_.play(spriteMusic_);
            }
        }
    holdMusic();
}

// Whether a list entry has its file: outfits and backgrounds that come with a bonus the copy lacks don't.
bool Engine::spriteAvailable(const std::string &thumbnail) const { return files_.exists(thumbnail); }

std::vector<int> Engine::spriteChoices(int picker) const {
    std::vector<int> out;
    const SpriteHeroine &h = spriteHeroines_[size_t(sprite_.heroine)];
    auto add = [&](size_t count, auto thumbnail) {
        for (size_t i = 0; i < count; i++)
            if (spriteAvailable(thumbnail(i))) out.push_back(int(i));
    };
    switch (picker) {
    case 0: add(spriteBackgrounds_.size(), [&](size_t i) { return spriteBackgrounds_[i].first; }); break;
    case 1: add(h.faces.size(), [&](size_t i) { return h.faces[i]; }); break;
    case 2: add(h.outfits.size(), [&](size_t i) { return h.outfits[i].thumbnail; }); break;
    case 3: add(h.poses.size(), [&](size_t i) { return h.poses[i]; }); break;
    default: for (size_t i = 0; i < spriteHeroines_.size(); i++) out.push_back(int(i)); break;
    }
    return out;
}

int &Engine::spriteChoice(int picker) {
    size_t h = size_t(sprite_.heroine);
    switch (picker) {
    case 0: return sprite_.background;
    case 1: return sprite_.face[h];
    case 2: return spriteOutfit_[h];
    case 3: return sprite_.pose[h];
    default: return sprite_.heroine;
    }
}

void Engine::stepSpriteChoice(int picker, int direction) {
    log("sprite step %d %+d", picker, direction);
    std::vector<int> choices = spriteChoices(picker);
    if (choices.empty()) return;
    int &value = spriteChoice(picker);
    auto at = std::find(choices.begin(), choices.end(), value);
    int index = at == choices.end() ? 0 : int(at - choices.begin());
    value = choices[size_t((index + direction + int(choices.size())) % int(choices.size()))];
}

std::string Engine::spriteThumbnail(int picker, int index) const {
    const SpriteHeroine &h = spriteHeroines_[size_t(sprite_.heroine)];
    auto pick = [&](const std::vector<std::string> &list) { return index >= 0 && size_t(index) < list.size() ? list[size_t(index)] : std::string(); };
    switch (picker) {
    case 0: return index >= 0 && size_t(index) < spriteBackgrounds_.size() ? spriteBackgrounds_[size_t(index)].first : "";
    case 1: return pick(h.faces);
    case 2: return index >= 0 && size_t(index) < h.outfits.size() ? h.outfits[size_t(index)].thumbnail : "";
    case 3: return pick(h.poses);
    default: {
        // `spriteheroines FILE...`: each heroine's picture for the heroine picker, in her order.
        const auto *list = screen_->setting("spriteheroines");
        return list && index >= 0 && size_t(index) < list->size() ? (*list)[size_t(index)] : "";
    }
    }
}

// The heroine as drawn: her sprite in the chosen outfit, pose and face, and the background, sharp or blurred.
std::string Engine::spriteFigure() const {
    const SpriteHeroine &h = spriteHeroines_[size_t(sprite_.heroine)];
    size_t index = size_t(sprite_.heroine);
    int outfit = h.outfits.empty() ? 0 : h.outfits[size_t(std::clamp(spriteOutfit_[index], 0, int(h.outfits.size()) - 1))].id;
    int pose = h.poses.empty() ? 0 : nameField(h.poses[size_t(std::clamp(sprite_.pose[index], 0, int(h.poses.size()) - 1))], 3);
    int face = h.faces.empty() ? 0 : nameField(h.faces[size_t(std::clamp(sprite_.face[index], 0, int(h.faces.size()) - 1))], 5);
    char name[64];
    std::snprintf(name, sizeof name, "sc\\sc_%03d_%03d_%03d_000_%03d.bmp", h.number, outfit, pose, face);
    return name;
}

std::string Engine::spriteBackground() const {
    if (spriteBackgrounds_.empty()) return "";
    std::string path = spriteBackgrounds_[size_t(std::clamp(sprite_.background, 0, int(spriteBackgrounds_.size()) - 1))].first;
    // obj\sc\bg\bg_N.bmp shows as bg\bg_N.bmp, and a patch's sys_1\obj\sc\bg\... as sys_1\bg\....
    if (auto at = path.find("obj\\sc\\"); at != std::string::npos) path.erase(at, 7);
    if (sprite_.blur) {
        std::string blurred = path.substr(0, path.size() - 4) + "_b.bmp";
        if (files_.exists(blurred)) return blurred;
    }
    return path;
}

bool Engine::spritePanelsShown() const { return sprite_.pinned || !sprite_.panelsHidden; }

// The control under a point, as `kind index`: `arrow` 0 to 9 (left then right of each picker), `thumb` 0 to 4, `lock`,
// `music` and `blur` with 0 for on and 1 for off, `zoom` 0 to 2 and `pin`. Empty for none.
std::pair<std::string, int> Engine::spriteControlAt(int x, int y) const {
    if (!spritePanelsShown()) return {};
    auto within = [&](const char *key, size_t sheetAt, int cols, int rows, int px, int py) {
        const auto *line = screen_->setting(key);
        ImageRef sheet = line && line->size() > sheetAt ? images_->get((*line)[sheetAt]) : nullptr;
        return sheet && x >= px && y >= py && x < px + sheet->w / cols && y < py + sheet->h / rows;
    };
    if (const auto *a = screen_->setting("spritearrows"); a && a->size() >= 5)
        for (int p = 0; p < 5; p++)
            for (int side = 0; side < 2; side++)
                if (within("spritearrows", 0, 2, 5, number(*a, 1 + side) + p * number(*a, 4), number(*a, 3))) return {"arrow", p * 2 + side};
    if (const auto *t = screen_->setting("spritethumbs"); t && t->size() >= 3)
        for (int p = 0; p < 5; p++) {
            int tx = number(*t, 0) + p * number(*t, 2), ty = number(*t, 1);
            if (x >= tx && y >= ty && x < tx + 137 && y < ty + 100) return {"thumb", p};
        }
    for (auto [it, end] = screen_->settings.equal_range("spriteradio"); it != end; ++it) {
        const auto &f = it->second;
        if (f.size() < 5) continue;
        for (int side = 0; side < 2; side++) {
            ImageRef sheet = images_->get(f[1]);
            int px = number(f, 2 + side), py = number(f, 4);
            if (sheet && x >= px && y >= py && x < px + sheet->w / 2 && y < py + sheet->h / 5) return {f[0], side};
        }
    }
    if (const auto *z = screen_->setting("spritezoom"); z && z->size() >= 5)
        for (int i = 0; i < 3; i++)
            if (within("spritezoom", 0, 3, 5, number(*z, 1 + i), number(*z, 4))) return {"zoom", i};
    if (const auto *p = screen_->setting("spritepin"); p && p->size() >= 3 && within("spritepin", 0, 1, 5, number(*p, 1), number(*p, 2))) return {"pin", 0};
    return {};
}

// The panels show for `spriteresting` milliseconds from when they appear (the explanation or a list closing, or a click
// bringing them back) or a click changes something, as the PC's timed shots measure; a click on nothing, on the side
// already chosen or the pointer moving doesn't keep them.
void Engine::pressSprites(int x, int y) {
    dirty_ = true;
    if (sprite_.explaining) {
        sprite_.explaining = false;
        sprite_.restingSince = sprite_.slideSince = screenMs_;
        return;
    }
    if (sprite_.list >= 0) {
        pressSpriteList(x, y);
        if (sprite_.list < 0) sprite_.restingSince = screenMs_;
        return;
    }
    // A click while the panels are hidden only brings them back, as on the PC.
    if (!spritePanelsShown()) {
        sprite_.panelsHidden = false;
        sprite_.restingSince = sprite_.slideSince = screenMs_;
        return;
    }
    if (Button *b = buttonAt(x, y, screen_->buttons)) {
        sprite_.restingSince = screenMs_;
        std::string action = b->action;
        runAction(action);
        return;
    }
    auto [kind, index] = spriteControlAt(x, y);
    size_t h = size_t(sprite_.heroine);
    // A click that changes something keeps the panels; the side already chosen doesn't, as on the PC.
    bool same = (kind == "lock" && spriteLocked_[h] == (index == 0)) || (kind == "music" && sprite_.music == (index == 0)) || (kind == "blur" && sprite_.blur == (index == 1));
    if (!kind.empty() && !same) sprite_.restingSince = screenMs_;
    if (kind == "arrow") stepSpriteChoice(index / 2, index % 2 ? 1 : -1);
    else if (kind == "thumb") {
        sprite_.list = index;
        log("sprite list %d", index);
        sprite_.listScroll = 0;
        // Opened scrolled to the chosen entry's row when it lies past the rows showing.
        const auto *g = listLine(*screen_, "spritelist", index);
        auto entries = spriteListEntries(index);
        auto at = std::find_if(entries.begin(), entries.end(), [&](const SpriteListEntry &e) { return e.index == spriteChoice(index); });
        if (g && g->size() >= 3 && number(*g, 2) > 0 && at != entries.end()) {
            int row = int(at - entries.begin()) / std::max(1, number(*g, 1));
            if (row >= number(*g, 2)) sprite_.listScroll = row;
        }
    } else if (kind == "lock") spriteLocked_[h] = index == 0;
    else if (kind == "music") {
        sprite_.music = index == 0;
        playSpriteMusic();
    } else if (kind == "blur") sprite_.blur = index == 1;
    else if (kind == "zoom") sprite_.zoom = index == 1 ? kZoomMin : std::clamp(sprite_.zoom + (index == 2 ? kZoomStep : -kZoomStep), kZoomMin, kZoomMax);
    else if (kind == "pin") sprite_.pinned = !sprite_.pinned;
}

void Engine::stepSprites() {
    if (!spriteScreen()) return;
    // `spriteresting MS`: the panels start to hide that long after they last appeared or a click changed something,
    // unless pinned.
    const auto *rest = screen_->setting("spriteresting");
    int ms = rest && !rest->empty() ? number(*rest, 0) : 3000;
    if (!sprite_.explaining && sprite_.list < 0 && !sprite_.pinned && !sprite_.panelsHidden && screenMs_ - sprite_.restingSince >= ms) {
        sprite_.panelsHidden = true;
        sprite_.slideSince = screenMs_;
    }
    if (screenMs_ - sprite_.slideSince <= kSlideMs) dirty_ = true;
}

// How far the panels are off the screen, 0 shown to 1 gone, as they slide.
float Engine::spriteSlide() const {
    if (sprite_.pinned) return 0;
    float p = std::clamp(float(screenMs_ - sprite_.slideSince) / kSlideMs, 0.0f, 1.0f);
    return sprite_.panelsHidden ? p : 1 - p;
}

// The figure follows the pointer, so a move redraws; it doesn't bring the panels back, as on the PC.
void Engine::spritePointerMoved() { dirty_ = true; }

void Engine::composeSprites(Canvas &out) {
    out.clear();
    // Where the group being drawn has slid to.
    int ox = 0, oy = 0;
    auto draw = [&](const std::string &path, int x, int y) {
        if (ImageRef image = images_->get(path)) drawPart(out, *image, x + ox, y + oy, 0, 0, image->w, image->h, 1);
    };
    auto cell = [&](const std::string &path, int cols, int rows, int col, int row, int x, int y) {
        if (ImageRef sheet = images_->get(path)) {
            int w = sheet->w / cols, h = sheet->h / rows;
            drawPart(out, *sheet, x + ox, y + oy, col * w, std::min(row, rows - 1) * h, w, h, 1);
        }
    };
    if (sprite_.explaining) {
        if (const auto *e = screen_->setting("spriteexplain"); e && !e->empty()) draw((*e)[0], 0, 0);
        return;
    }
    draw(spriteBackground(), 0, 0);
    if (ImageRef figure = images_->get(spriteFigure())) {
        // `spritefigure CX`: the figure centred on CX at its zoom. Taller than the screen, she moves with the pointer, her
        // top showing with it at the top of the screen and her feet with it at the bottom, as the PC's shots measure.
        const auto *place = screen_->setting("spritefigure");
        float cx = place ? float(number(*place, 0)) : float(out.w) / 2, zoom = float(sprite_.zoom) / 100, h = float(figure->h) * zoom;
        float top = h > float(out.h) ? -(h - float(out.h)) * float(pointerSeen_ ? std::clamp(pointerY_, 0, out.h) : out.h / 2) / float(out.h) : 0;
        drawImage(out, *figure, cx, top + h / 2, zoom, zoom, 0, 1, nullptr, false);
    }
    float slide = spriteSlide();
    if (slide >= 1) {
        if (sprite_.list >= 0) composeSpriteList(out);
        return;
    }
    // As they hide, the panel slides off to the left, the bar and the pin down, and the hint and 終了 to the right, each
    // by its own size, together.
    // How far a piece has slid: off the left edge (edge -1), or to the given right or bottom edge.
    auto slid = [&](const char *key, int edge, bool across) {
        const auto *line = screen_->setting(key);
        ImageRef image = line && line->size() >= 3 ? images_->get((*line)[0]) : nullptr;
        if (!image) return 0;
        int at = number(*line, across ? 1 : 2), size = across ? image->w : image->h;
        return int(std::lround(slide * float(edge < 0 ? -(at + size) : edge - at)));
    };
    int panelDx = slid("spritepanel", -1, true), barDy = slid("spritebar", out.h, false), hintDx = slid("spritehint", out.w, true);
    auto [overKind, overIndex] = pointerSeen_ ? spriteControlAt(pointerX_, pointerY_) : std::pair<std::string, int>{};
    size_t h = size_t(sprite_.heroine);
    ox = panelDx;
    if (const auto *p = screen_->setting("spritepanel"); p && p->size() >= 3) draw((*p)[0], number(*p, 1), number(*p, 2));
    if (const auto *n = screen_->setting("spritename"); n && n->size() >= 3) cell((*n)[0], 1, int(spriteHeroines_.size()), 0, sprite_.heroine, number(*n, 1), number(*n, 2));
    for (auto [it, end] = screen_->settings.equal_range("spriteradio"); it != end; ++it) {
        const auto &f = it->second;
        if (f.size() < 5) continue;
        // The first of each pair is ON, ON and 通常 (sharp), the second OFF, OFF and ぼかし (blurred).
        int picked = f[0] == "lock" ? (spriteLocked_[h] ? 0 : 1) : f[0] == "music" ? (sprite_.music ? 0 : 1) : (sprite_.blur ? 1 : 0);
        for (int side = 0; side < 2; side++) {
            bool chosen = side == picked, over = overKind == f[0] && overIndex == side;
            // A chosen side stays red under the pointer, as on the PC.
            cell(f[1], 2, 5, side, chosen ? kSelected : over ? kHover : kNormal, number(f, 2 + side), number(f, 4));
        }
    }
    // The zoom as a units digit and two decimals, with the panel's own × and point.
    if (const auto *d = screen_->setting("spritezoomdigits"); d && d->size() >= 5) {
        int digits[3] = {sprite_.zoom / 100, sprite_.zoom / 10 % 10, sprite_.zoom % 10};
        for (int i = 0; i < 3; i++) cell((*d)[0], 10, 1, digits[i], 0, number(*d, 1 + i), number(*d, 4));
    }
    if (const auto *z = screen_->setting("spritezoom"); z && z->size() >= 5)
        for (int i = 0; i < 3; i++) {
            bool off = (i < 2 && sprite_.zoom <= kZoomMin) || (i == 2 && sprite_.zoom >= kZoomMax);
            cell((*z)[0], 3, 5, i, off ? 4 : overKind == "zoom" && overIndex == i ? kHover : kNormal, number(*z, 1 + i), number(*z, 4));
        }
    // A control's help: `spritehelp CONTROL FILE X Y [ROWS]`, the lock's by heroine.
    for (auto [it, end] = screen_->settings.equal_range("spritehelp"); it != end; ++it) {
        const auto &f = it->second;
        if (f.size() >= 4 && f[0] == overKind) {
            int rows = f.size() >= 5 ? std::max(1, number(f, 4)) : 1;
            cell(f[1], 1, rows, 0, rows > 1 ? sprite_.heroine : 0, number(f, 2), number(f, 3));
        }
    }
    ox = hintDx;
    if (const auto *t = screen_->setting("spritehint"); t && t->size() >= 3) draw((*t)[0], number(*t, 1), number(*t, 2));
    ox = 0;
    oy = barDy;
    if (const auto *p = screen_->setting("spritepin"); p && p->size() >= 3)
        cell((*p)[0], 1, 5, 0, sprite_.pinned ? (overKind == "pin" ? kSelectedHover : kSelected) : overKind == "pin" ? kHover : kNormal, number(*p, 1), number(*p, 2));
    if (const auto *b = screen_->setting("spritebar"); b && b->size() >= 3) draw((*b)[0], number(*b, 1), number(*b, 2));
    if (const auto *a = screen_->setting("spritearrows"); a && a->size() >= 5)
        for (int p = 0; p < 5; p++)
            for (int side = 0; side < 2; side++)
                cell((*a)[0], 2, 5, side, overKind == "arrow" && overIndex == p * 2 + side ? kHover : kNormal, number(*a, 1 + side) + p * number(*a, 4), number(*a, 3));
    if (const auto *t = screen_->setting("spritethumbs"); t && t->size() >= 3)
        for (int p = 0; p < 5; p++) draw(spriteThumbnail(p, spriteChoice(p)), number(*t, 0) + p * number(*t, 2), number(*t, 1));
    // Each picker's label: `spritelabel PICKER FILE X Y ROWS` draws the row of the choice, and `spritenumber PICKER
    // DIGITS Y X...` its place in the list, from 1, in as many digits as places given.
    for (auto [it, end] = screen_->settings.equal_range("spritelabel"); it != end; ++it) {
        const auto &f = it->second;
        if (f.size() < 5) continue;
        int picker = number(f, 0), rows = std::max(1, number(f, 4));
        cell(f[1], 1, rows, 0, rows > 1 ? spriteChoice(picker) : 0, number(f, 2), number(f, 3));
    }
    for (auto [it, end] = screen_->settings.equal_range("spritenumber"); it != end; ++it) {
        const auto &f = it->second;
        if (f.size() < 4) continue;
        int value = spriteChoice(number(f, 0)) + 1, places = int(f.size()) - 3;
        for (int i = places - 1; i >= 0; i--, value /= 10) cell(f[1], 10, 1, value % 10, 0, number(f, 3 + size_t(i)), number(f, 2));
    }
    for (const Button &b : screen_->buttons) {
        Button shown = b;
        shown.x += int(std::lround(slide * float(out.w - b.x)));
        drawButton(out, shown, pointerSeen_ && b.contains(pointerX_, pointerY_) ? StateHover : StateNormal);
    }
    if (sprite_.list >= 0) composeSpriteList(out);
}

// A thumbnail's list, laid out by `spritelist*` lines for each picker (0 to 4, as the thumbnails run):
//   spritelistframe P FILE X Y          the window
//   spritelist P COLS ROWS X Y DX DY    entries as tiles in a grid, ROWS of them showing (0 for all), scrolled by rows
//   spritelisttile P PREFIX             a tile is its thumbnail's file with PREFIX before the name, as thumb_face_...
//   spritelistfigure P Y SCALE CX...    or the heroine herself, each centred unscaled on her CX with her top at Y, then
//                                       scaled from her top left corner, as the pose and heroine lists show; a click
//                                       goes to the nearest
//   spritelisttint P R,G,B R,G,B R,G,B  the colours a figure is multiplied by: others, under the pointer, chosen
//   spritelistlit P FILE                five rows of light over a tile: hovered 1, chosen 2
//   spritelistpicture P FILE DX DY      a picture by each entry
//   spritelistlabel P FILE DX DY ROWS [HEROINE]   the entry's row of names, by its place in sc_view.ini
//   spritelistnumber P FILE DY DX...    its place in the list, from 1, in as many digits as places
//   spritelistcheck P FILE DX DY        a box, ticked on the chosen entry; a click on it chooses, as the tile does
//   spritelistplace P FILE X,Y...       names at fixed places, for the figure lists
//   spritelistfade P FILE X Y           a picture over the figures
//   spritelistpreview P PREFIX X Y      the chosen entry large, PREFIX before its thumbnail's name
//   spritelistpreviewlabel P FILE X Y ROWS [HEROINE] and spritelistpreviewnumber P FILE Y X...: its name and number
//   spritelistscroll P FILE X Y W H ALPHA   the scroll bar's thumb, FILE stretched over its share of the track
//   spritelistarrows P FILE X Y...      first, up, down and last, a column each of FILE
// A click on an entry chooses it and leaves the list open, as on the PC; the × or a click outside the window closes it.

// What a list shows: each entry whose file the copy has, and a bonus's name alone when its patch folder is there
// without the picture, as Test Game 88's mail-order outfits and backgrounds show. Others, such as an absent campaign
// bonus, aren't listed.
std::vector<Engine::SpriteListEntry> Engine::spriteListEntries(int picker) const {
    std::vector<SpriteListEntry> out;
    size_t count = picker == 4 ? spriteHeroines_.size() : 0;
    const SpriteHeroine &h = spriteHeroines_[size_t(sprite_.heroine)];
    if (picker == 0) count = spriteBackgrounds_.size();
    else if (picker == 1) count = h.faces.size();
    else if (picker == 2) count = h.outfits.size();
    else if (picker == 3) count = h.poses.size();
    for (size_t i = 0; i < count; i++) {
        std::string thumbnail = spriteThumbnail(picker, int(i));
        if (picker == 4 || spriteAvailable(thumbnail)) out.push_back({int(i), true});
        else if (auto slash = thumbnail.find('\\'); slash != std::string::npos && thumbnail.compare(0, 4, "sys_") == 0) {
            // Whether the patch folder is there, looked up once, since a lookup goes through every file.
            std::string folder = thumbnail.substr(0, slash);
            auto known = spritePatches_.find(folder);
            if (known == spritePatches_.end()) known = spritePatches_.emplace(folder, !files_.list(folder).empty()).first;
            if (known->second) out.push_back({int(i), false});
        }
    }
    return out;
}

// The figure a pose or heroine list shows: the heroine in her outfit and face in that pose, or each heroine as her
// thumbnail's name gives her.
std::string Engine::spriteListFigure(int picker, int index) const {
    std::string thumbnail = spriteThumbnail(picker, index);
    if (picker == 4) {
        std::string name = thumbnail.substr(thumbnail.find_last_of('\\') + 1);
        auto at = name.find('_');
        return at == std::string::npos ? "" : "sc\\sc" + name.substr(at);
    }
    const SpriteHeroine &h = spriteHeroines_[size_t(sprite_.heroine)];
    size_t heroine = size_t(sprite_.heroine);
    int outfit = h.outfits.empty() ? 0 : h.outfits[size_t(std::clamp(spriteOutfit_[heroine], 0, int(h.outfits.size()) - 1))].id;
    int face = h.faces.empty() ? 0 : nameField(h.faces[size_t(std::clamp(sprite_.face[heroine], 0, int(h.faces.size()) - 1))], 5);
    char name[64];
    std::snprintf(name, sizeof name, "sc\\sc_%03d_%03d_%03d_000_%03d.bmp", h.number, outfit, nameField(thumbnail, 3), face);
    return name;
}

// Where the entry at a place in the list sits, scrolled; y is past the screen for one scrolled away.
std::pair<int, int> Engine::spriteListPlace(int position) const {
    const auto *g = listLine(*screen_, "spritelist", sprite_.list);
    if (!g || g->size() < 7) return {0, 0};
    int cols = std::max(1, number(*g, 1)), rows = number(*g, 2), row = position / cols - sprite_.listScroll;
    if (row < 0 || (rows > 0 && row >= rows)) return {0, 1 << 20};
    return {number(*g, 3) + position % cols * number(*g, 5), number(*g, 4) + row * number(*g, 6)};
}

int Engine::spriteListRows() const {
    const auto *g = listLine(*screen_, "spritelist", sprite_.list);
    int cols = g && g->size() >= 2 ? std::max(1, number(*g, 1)) : 1;
    return (int(spriteListEntries(sprite_.list).size()) + cols - 1) / cols;
}

// The place in the list of the entry under a point, or -1; onCheck says whether the point is on its box.
int Engine::spriteListAt(int x, int y, bool *onCheck) const {
    auto entries = spriteListEntries(sprite_.list);
    if (const auto *f = listLine(*screen_, "spritelistfigure", sprite_.list); f && f->size() >= 4) {
        // The PC's pointer chose by column, whatever the figure's shape: the nearest centre, below the figures' top.
        if (y < number(*f, 1)) return -1;
        int best = -1;
        for (size_t i = 0; i < entries.size() && i + 3 < f->size(); i++)
            if (best < 0 || std::abs(x - number(*f, i + 3)) < std::abs(x - number(*f, size_t(best) + 3))) best = int(i);
        return best;
    }
    const auto *tile = listLine(*screen_, "spritelisttile", sprite_.list);
    for (size_t i = 0; i < entries.size(); i++) {
        if (!entries[i].shown) continue;
        auto [tx, ty] = spriteListPlace(int(i));
        if (const auto *c = listLine(*screen_, "spritelistcheck", sprite_.list); c && c->size() >= 4)
            if (ImageRef box = images_->get((*c)[1])) {
                int bx = tx + number(*c, 2), by = ty + number(*c, 3);
                if (x >= bx && y >= by && x < bx + box->w && y < by + box->h / 5) {
                    if (onCheck) *onCheck = true;
                    return int(i);
                }
            }
        ImageRef thumb = tile && tile->size() >= 2 ? images_->get(prefixed(spriteThumbnail(sprite_.list, entries[i].index), (*tile)[1])) : nullptr;
        if (thumb && x >= tx && y >= ty && x < tx + thumb->w && y < ty + thumb->h) return int(i);
    }
    return -1;
}

void Engine::composeSpriteList(Canvas &out) {
    const int picker = sprite_.list;
    auto line = [&](const char *key) { return listLine(*screen_, key, picker); };
    auto draw = [&](const std::string &path, int x, int y) {
        if (ImageRef image = images_->get(path)) drawPart(out, *image, x, y, 0, 0, image->w, image->h, 1);
    };
    auto cell = [&](const std::string &path, int cols, int rows, int col, int row, int x, int y) {
        if (ImageRef sheet = images_->get(path)) {
            int w = sheet->w / cols, h = sheet->h / rows;
            drawPart(out, *sheet, x, y, std::clamp(col, 0, cols - 1) * w, std::clamp(row, 0, rows - 1) * h, w, h, 1);
        }
    };
    auto digits = [&](const std::vector<std::string> &f, int value, int y, size_t firstX, int dx) {
        int places = int(f.size() - firstX);
        for (int i = places - 1; i >= 0; i--, value /= 10) cell(f[1], 10, 1, value % 10, 0, dx + number(f, firstX + size_t(i)), y);
    };
    // A heroine's names: a label line's optional last field picks whose they are.
    auto forHeroine = [&](const std::vector<std::string> &f, size_t at) { return f.size() <= at || number(f, at) == sprite_.heroine; };
    if (const auto *f = line("spritelistframe"); f && f->size() >= 4) draw((*f)[1], number(*f, 2), number(*f, 3));
    auto entries = spriteListEntries(picker);
    int chosen = spriteChoice(picker);
    int hovered = pointerSeen_ ? spriteListAt(pointerX_, pointerY_) : -1;
    const auto *lit = line("spritelistlit");
    auto litRow = [&](size_t i) { return entries[i].index == chosen ? kSelected : int(i) == hovered ? kHover : kNormal; };
    if (const auto *f = line("spritelistfigure"); f && f->size() >= 4) {
        float scale = float(std::atof((*f)[2].c_str())), top = float(number(*f, 1));
        const auto *tint = line("spritelisttint");
        // Only the copies drawn now are kept, so moving through outfits and faces doesn't pile them up.
        std::map<std::string, ImageRef> kept;
        for (size_t i = 0; i < entries.size() && i + 3 < f->size(); i++) {
            std::string path = spriteListFigure(picker, entries[i].index);
            ImageRef figure = images_->get(path);
            if (!figure) continue;
            // Others, the hovered and the chosen, as the tint line lists their colours.
            size_t state = entries[i].index == chosen ? 2 : int(i) == hovered ? 1 : 0;
            if (tint && tint->size() >= 4) {
                auto [r, g] = point((*tint)[1 + state]);
                const std::string &text = (*tint)[1 + state];
                int colour[3] = {r, g, std::atoi(text.c_str() + text.rfind(',') + 1)};
                std::string key = path + '#' + std::to_string(state);
                auto &copy = kept[key];
                if (auto old = spriteTinted_.find(key); old != spriteTinted_.end()) copy = old->second;
                if (!copy) copy = tinted(*figure, colour);
                figure = copy;
            }
            float left = float(number(*f, i + 3) - figure->w / 2);
            drawImage(out, *figure, left + float(figure->w) * scale / 2, top + float(figure->h) * scale / 2, scale, scale, 0, 1, nullptr, false);
        }
        spriteTinted_ = std::move(kept);
        if (const auto *fade = line("spritelistfade"); fade && fade->size() >= 4) draw((*fade)[1], number(*fade, 2), number(*fade, 3));
        if (const auto *p = line("spritelistplace"); p && p->size() >= 3)
            for (size_t i = 0; i < entries.size() && i + 2 < p->size(); i++) {
                auto [x, y] = point((*p)[i + 2]);
                ImageRef names = images_->get((*p)[1]);
                int rows = names ? std::max(1, int(picker == 4 ? spriteHeroines_.size() : spriteHeroines_[size_t(sprite_.heroine)].poses.size())) : 1;
                cell((*p)[1], 1, rows, 0, entries[i].index, x, y);
            }
    } else {
        const auto *tile = line("spritelisttile");
        for (size_t i = 0; i < entries.size(); i++) {
            auto [x, y] = spriteListPlace(int(i));
            if (y >= out.h) continue;
            if (entries[i].shown && tile && tile->size() >= 2) {
                std::string path = prefixed(spriteThumbnail(picker, entries[i].index), (*tile)[1]);
                draw(path, x, y);
                if (lit && lit->size() >= 2) cell((*lit)[1], 1, 5, 0, litRow(i), x, y);
            }
            for (auto [it, end] = screen_->settings.equal_range("spritelistpicture"); it != end; ++it)
                if (it->second.size() >= 4 && number(it->second, 0) == picker) draw(it->second[1], x + number(it->second, 2), y + number(it->second, 3));
            for (auto [it, end] = screen_->settings.equal_range("spritelistlabel"); it != end; ++it) {
                const auto &f = it->second;
                if (f.size() >= 5 && number(f, 0) == picker && forHeroine(f, 5)) cell(f[1], 1, std::max(1, number(f, 4)), 0, entries[i].index, x + number(f, 2), y + number(f, 3));
            }
            for (auto [it, end] = screen_->settings.equal_range("spritelistnumber"); it != end; ++it) {
                const auto &f = it->second;
                if (f.size() >= 4 && number(f, 0) == picker) digits(f, int(i) + 1, y + number(f, 2), 3, x);
            }
            if (const auto *c = line("spritelistcheck"); c && c->size() >= 4) {
                bool on = entries[i].index == chosen, over = int(i) == hovered;
                cell((*c)[1], 1, 5, 0, on ? (over ? kSelectedHover : kSelected) : over ? kHover : kNormal, x + number(*c, 2), y + number(*c, 3));
            }
        }
        // The chosen entry large, with its name and number.
        auto at = std::find_if(entries.begin(), entries.end(), [&](const SpriteListEntry &e) { return e.index == chosen; });
        if (at != entries.end()) {
            if (const auto *p = line("spritelistpreview"); p && p->size() >= 4) draw(prefixed(spriteThumbnail(picker, chosen), (*p)[1]), number(*p, 2), number(*p, 3));
            for (auto [it, end] = screen_->settings.equal_range("spritelistpreviewlabel"); it != end; ++it) {
                const auto &f = it->second;
                if (f.size() >= 5 && number(f, 0) == picker && forHeroine(f, 5)) cell(f[1], 1, std::max(1, number(f, 4)), 0, chosen, number(f, 2), number(f, 3));
            }
            if (const auto *n = line("spritelistpreviewnumber"); n && n->size() >= 4) digits(*n, int(at - entries.begin()) + 1, number(*n, 2), 3, 0);
        }
        if (const auto *s = line("spritelistscroll"); s && s->size() >= 7) {
            const auto *g = line("spritelist");
            int shown = g && g->size() >= 3 ? number(*g, 2) : 0, rows = spriteListRows();
            if (shown > 0 && rows > shown) {
                // The track covers every place the top row can take, a page of rows past the last.
                int span = rows + shown - 1, track = number(*s, 5);
                if (ImageRef bar = images_->get((*s)[1]))
                    drawImage(out, *bar, float(number(*s, 2)) + float(number(*s, 4)) / 2, float(number(*s, 3)) + float(track) * (float(sprite_.listScroll) + float(shown) / 2) / float(span),
                              float(number(*s, 4)) / float(bar->w), float(track) * float(shown) / float(span) / float(bar->h), 0, float(number(*s, 6)) / 255, nullptr, false);
            }
        }
        if (const auto *a = line("spritelistarrows"); a && a->size() >= 7)
            for (int i = 0; i < 4; i++) {
                ImageRef sheet = images_->get((*a)[1]);
                int x = number(*a, 2), y = number(*a, 3 + size_t(i));
                bool over = sheet && pointerSeen_ && pointerX_ >= x && pointerY_ >= y && pointerX_ < x + sheet->w / 4 && pointerY_ < y + sheet->h / 5;
                cell((*a)[1], 4, 5, i, over ? kHover : kNormal, x, y);
            }
    }
    if (const auto *c = screen_->setting("spritelistclose"); c && c->size() >= 3)
        if (ImageRef sheet = images_->get((*c)[0])) {
            int w = sheet->w, h = sheet->h / 5, x = number(*c, 1), y = number(*c, 2);
            bool over = pointerSeen_ && pointerX_ >= x && pointerY_ >= y && pointerX_ < x + w && pointerY_ < y + h;
            drawPart(out, *sheet, x, y, 0, (over ? kHover : kNormal) * h, w, h, 1);
        }
}

void Engine::pressSpriteList(int x, int y) {
    auto inside = [&](const std::string &path, int px, int py, int rows) {
        ImageRef image = images_->get(path);
        return image && x >= px && y >= py && x < px + image->w && y < py + image->h / rows;
    };
    if (const auto *c = screen_->setting("spritelistclose"); c && c->size() >= 3 && inside((*c)[0], number(*c, 1), number(*c, 2), 5)) {
        sprite_.list = -1;
        spriteTinted_.clear();
        return;
    }
    if (const auto *f = listLine(*screen_, "spritelistframe", sprite_.list); f && f->size() >= 4 && !inside((*f)[1], number(*f, 2), number(*f, 3), 1)) {
        sprite_.list = -1;
        spriteTinted_.clear();
        return;
    }
    if (const auto *a = listLine(*screen_, "spritelistarrows", sprite_.list); a && a->size() >= 7) {
        ImageRef sheet = images_->get((*a)[1]);
        const auto *g = listLine(*screen_, "spritelist", sprite_.list);
        int shown = g && g->size() >= 3 ? number(*g, 2) : 0, last = std::max(0, spriteListRows() - 1);
        for (int i = 0; sheet && shown > 0 && i < 4; i++) {
            int ax = number(*a, 2), ay = number(*a, 3 + size_t(i));
            if (x >= ax && y >= ay && x < ax + sheet->w / 4 && y < ay + sheet->h / 5) {
                int to[4] = {0, sprite_.listScroll - 1, sprite_.listScroll + 1, last};
                sprite_.listScroll = std::clamp(to[i], 0, last);
                return;
            }
        }
    }
    auto entries = spriteListEntries(sprite_.list);
    int at = spriteListAt(x, y);
    if (at >= 0 && entries[size_t(at)].shown) spriteChoice(sprite_.list) = entries[size_t(at)].index;
}

}  // namespace neko
