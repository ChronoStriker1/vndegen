// The character gallery of Test Game 87 (Funbag Fantasy: Sideboob Story 2), from cfg\sc_view.ini: a card for each
// character and outfit, fifteen a page, and a viewer that shows the card's sprites over its background one variant at a
// time, a full-height picture that pans with the pointer, and an automatic run through the variants. Every place and
// timing here was measured on the PC game under Wine (set 1). The table names it with `part chargallery` under its
// `screen sprites`, which gives the paper, title, page seals and side bar; this part draws the cards and the viewer.
#include <algorithm>
#include <sstream>

#include "parts.h"

namespace neko {
namespace {

struct Card {
    std::string thumbnail, background, tall;
    std::vector<std::string> sprites;
};

constexpr int kPerPage = 15, kColumns = 5;
constexpr int kCardX = 39, kCardY = 85, kCardStepX = 214, kCardStepY = 200, kCardW = 204, kCardH = 174;
constexpr int kAutoMs = 5000;  // a variant every 5 s, 4.7 to 5.2 between the PC's shots

// The viewer's panel and its buttons, each a sheet's column, normal to disabled down the rows.
struct PanelButton {
    const char *sheet;
    int cols, column, x, y;
    int help;  // the help card it shows, as the PC shows it (the PC pairs them one off; kept as it is)
    int helpX;
};
const PanelButton kButtons[] = {
    {"obj\\g008_000.bmp", 1, 0, 605, 665, 0, 573},   // full-height picture
    {"obj\\g003_000.bmp", 3, 0, 766, 672, 3, 702},   // automatic
    {"obj\\g007_000.bmp", 1, 0, 827, 672, 4, 763},   // next card
    {"obj\\g002_000.bmp", 2, 0, 928, 669, 1, 858},   // previous variant
    {"obj\\g002_000.bmp", 2, 1, 1126, 669, 2, 1056}, // next variant
    {"obj\\g003_000.bmp", 3, 2, 1206, 672, 5, 1098}, // back to the cards
};
enum { kFull, kAuto, kNextCard, kPrev, kNext, kReturn };

std::vector<std::string> fieldsOf(const std::string &line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

class CharGalleryPart : public GamePart {
public:
    bool openScreen(PartHost &host, const std::string &name, const std::vector<int> &) override {
        if (name != "sprites") return false;
        if (cards_.empty()) read(host);
        viewing_ = -1;
        full_ = auto_ = false;
        return true;
    }

    bool step(PartHost &host) override {
        if (viewing_ < 0 || !auto_) return false;
        if (host.screenMs() - autoFrom_ < kAutoMs) return false;
        autoFrom_ = host.screenMs();
        variant_ = (variant_ + 1) % std::max<int>(1, int(cards_[size_t(viewing_)].sprites.size()));
        return true;
    }

    void draw(PartHost &host, Canvas &out, Layer layer, int, int) override {
        if (layer == Layer::Menu && viewing_ < 0) drawCards(host, out);
        if (layer == Layer::Screen && viewing_ >= 0) drawViewer(host, out);
    }

    void pointer(PartHost &host, int x, int y) override {
        px_ = x;
        py_ = y;
        host.redraw();
    }

    bool press(PartHost &host, int x, int y, int button) override {
        if (viewing_ < 0) {
            if (button != 0) return false;
            int card = cardAt(host, x, y);
            if (card < 0) return false;
            open(host, card);
            return true;
        }
        // The viewer keeps the input: the right button does nothing there, as on the PC.
        if (button != 0) return true;
        if (full_) {
            full_ = false;
            host.redraw();
            return true;
        }
        const Card &c = cards_[size_t(viewing_)];
        int count = std::max<int>(1, int(c.sprites.size()));
        switch (buttonAt(host, x, y)) {
        case kFull: full_ = !c.tall.empty(); break;
        case kAuto: auto_ = !auto_; autoFrom_ = host.screenMs(); break;
        case kNextCard: open(host, (viewing_ + 1) % int(cards_.size())); break;
        case kPrev: variant_ = (variant_ + count - 1) % count; break;
        case kNext: variant_ = (variant_ + 1) % count; break;
        case kReturn: viewing_ = -1; auto_ = false; break;
        default: break;
        }
        host.redraw();
        return true;
    }

    void reset() override {
        viewing_ = -1;
        full_ = auto_ = false;
    }

private:
    std::vector<Card> cards_;
    int viewing_ = -1, variant_ = 0, px_ = -1, py_ = -1;
    bool full_ = false, auto_ = false;
    int64_t autoFrom_ = 0;

    // `#p 0 THUMB BACKGROUND TALL` opens a card, `0 N sc\...` lines list its sprites, `##` or the next `#p` ends it.
    void read(PartHost &host) {
        auto data = host.read("cfg\\sc_view.ini");
        if (!data) return;
        std::istringstream in(std::string(data->begin(), data->end()));
        for (std::string line; std::getline(in, line);) {
            if (auto c = line.find("//"); c != std::string::npos) line.erase(c);
            auto f = fieldsOf(line);
            if (f.empty()) continue;
            if (f[0] == "#p" && f.size() >= 5) cards_.push_back({f[2], f[3], f[4], {}});
            else if (f[0] != "##" && f.size() >= 3 && !cards_.empty()) cards_.back().sprites.push_back(f[2]);
        }
        host.log("character gallery: " + std::to_string(cards_.size()) + " cards");
    }

    void open(PartHost &host, int card) {
        viewing_ = card;
        variant_ = 0;
        full_ = false;
        autoFrom_ = host.screenMs();
    }

    int cardAt(PartHost &host, int x, int y) {
        int first = host.page() * kPerPage;
        for (int i = 0; i < kPerPage && first + i < int(cards_.size()); i++) {
            int cx = kCardX + (i % kColumns) * kCardStepX, cy = kCardY + (i / kColumns) * kCardStepY;
            if (x >= cx && x < cx + kCardW && y >= cy && y < cy + kCardH) return first + i;
        }
        return -1;
    }

    int buttonAt(PartHost &host, int x, int y) {
        if (auto_ && py_ < 634) return -1;  // the panel is away while it runs by itself
        for (int i = 0; i < int(std::size(kButtons)); i++) {
            const PanelButton &b = kButtons[i];
            ImageRef sheet = host.image(b.sheet);
            if (!sheet) continue;
            int w = sheet->w / b.cols, h = sheet->h / 5;
            if (x >= b.x && x < b.x + w && y >= b.y && y < b.y + h) return i;
        }
        return -1;
    }

    static void part(Canvas &out, const ImageRef &img, int x, int y, int sx, int sy, int w, int h) {
        if (img) drawPart(out, *img, x, y, sx, sy, w, h, 1);
    }

    void drawNumber(PartHost &host, Canvas &out, const char *sheet, int value, int x, int y, int step) {
        ImageRef digits = host.image(sheet);
        if (!digits) return;
        int w = digits->w / 10;
        part(out, digits, x, y, value / 10 % 10 * w, 0, w, digits->h);
        part(out, digits, x + step, y, value % 10 * w, 0, w, digits->h);
    }

    void drawCards(PartHost &host, Canvas &out) {
        ImageRef frame = host.image("obj\\f002_000.bmp");
        int first = host.page() * kPerPage;
        for (int i = 0; i < kPerPage && first + i < int(cards_.size()); i++) {
            const Card &c = cards_[size_t(first + i)];
            int cx = kCardX + (i % kColumns) * kCardStepX, cy = kCardY + (i / kColumns) * kCardStepY;
            bool over = px_ >= cx && px_ < cx + kCardW && py_ >= cy && py_ < cy + kCardH;
            // The frame sits a pixel right of and below the card's contents on the PC.
            if (frame) part(out, frame, cx + 1, cy + 1, 0, (over ? 1 : 0) * (frame->h / 5), frame->w, frame->h / 5);
            part(out, host.image(c.thumbnail), cx + 9, cy + 36, 0, 0, 188, 106);
            part(out, host.image("obj\\e003_001.bmp"), cx + 5, cy + 5, 0, 0, 33, 24);
            drawNumber(host, out, "obj\\e003_002.bmp", first + i + 1, cx + 39, cy + 5, 18);
        }
    }

    void drawViewer(PartHost &host, Canvas &out) {
        const Card &c = cards_[size_t(viewing_)];
        if (ImageRef bg = host.image(c.background)) part(out, bg, 0, 0, 0, 0, bg->w, bg->h);
        if (full_) {
            if (ImageRef tall = host.image(c.tall)) {
                int travel = std::max(0, tall->h - host.height());
                int y = -travel * std::clamp(py_, 0, host.height()) / host.height();
                part(out, tall, (host.width() - tall->w) / 2, y, 0, 0, tall->w, tall->h);
            }
            return;
        }
        if (!c.sprites.empty())
            if (ImageRef s = host.image(c.sprites[size_t(variant_) % c.sprites.size()]))
                part(out, s, (host.width() - s->w) / 2, (host.height() - s->h) / 2, 0, 0, s->w, s->h);
        // While it runs by itself, the panel is away unless the pointer is down on it.
        if (auto_ && py_ < 634) return;
        if (ImageRef bar = host.image("obj\\g000_000.bmp")) part(out, bar, 0, 634, 0, 0, bar->w, bar->h);
        int over = buttonAt(host, px_, py_);
        for (int i = 0; i < int(std::size(kButtons)); i++) {
            const PanelButton &b = kButtons[i];
            ImageRef sheet = host.image(b.sheet);
            if (!sheet) continue;
            int w = sheet->w / b.cols, h = sheet->h / 5;
            int row = (i == kAuto && auto_) ? (over == i ? 3 : 2) : (over == i ? 1 : 0);
            part(out, sheet, b.x, b.y, b.column * w, row * h, w, h);
        }
        drawNumber(host, out, "obj\\g001_000.bmp", variant_ + 1, 998, 668, 21);
        drawNumber(host, out, "obj\\g001_001.bmp", int(c.sprites.size()), 1059, 674, 17);
        if (over >= 0)
            if (ImageRef help = host.image("obj\\g009_000.bmp")) {
                int w = help->w / 6;
                part(out, help, kButtons[over].helpX, 564, kButtons[over].help * w, 0, w, help->h);
            }
    }
};

}  // namespace

std::unique_ptr<GamePart> makeCharGalleryPart() { return std::make_unique<CharGalleryPart>(); }

}  // namespace neko
