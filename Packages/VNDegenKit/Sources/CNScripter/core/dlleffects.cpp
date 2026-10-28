// Effect 99: transitions NScripter hands to a Windows DLL (`print 99,ms,"dir\name.dll/param"`). The runtime draws the
// ones a test game uses, as the original DLLs draw them under the PC engine (docs/nscripter-cleanroom.md):
//   breakup.dll  "urb", "lrB" and the like: the changed area breaks into round pieces that fly off.
//   lngtwave.dll "vwi": the screen ripples vertically while it crossfades.
//   trvswave.dll "h": the changed area ripples horizontally while it crossfades.
// Any other DLL or parameter falls back to a crossfade.
#include "engine.h"

#include <cmath>

namespace vnns {

namespace {

constexpr int kCell = 24;      // breakup piece size
constexpr int kPieceLife = 44;  // steps from a piece's start until it is gone: 4 shrinking in place, 40 flying

// Radius of a breakup piece j steps after it starts. Measured from the pixel counts of the original's pieces.
double pieceRadius(int j) {
    static const double inPlace[4] = {15.9, 14.6, 13.9, 12.85};
    if (j < 4) return inPlace[j];
    int pairs = (j - 4) / 2;  // in flight it shrinks every second step
    return std::max(11.85 - 0.995 * pairs, 0.905);
}

// The breakup parameter: the corner the break starts from ('u' or 'l', then 'r' or 'l'), then 'b' for pieces that
// fly towards that corner's side or 'B' for pieces that fly away from it.
struct Breakup {
    int hx = 1, vy = -1, sign = 1;
    explicit Breakup(const std::string &p) {
        if (p.size() > 0 && p[0] == 'l') vy = 1;
        if (p.size() > 1 && p[1] == 'l') hx = -1;
        if (p.size() > 2 && p[2] == 'B') sign = -1;
    }
};

}  // namespace

bool Engine::startDllEffect(const Image &to) {
    const std::string &spec = effect_.e.mask;
    size_t slash = spec.find('/');
    std::string path = sjisLower(spec.substr(0, slash));
    size_t sep = path.find_last_of("\\/");
    effect_.dll = sep == std::string::npos ? path : path.substr(sep + 1);
    effect_.param = slash == std::string::npos ? std::string() : spec.substr(slash + 1);
    effect_.dirs.clear();
    bool known = (effect_.dll == "breakup.dll" && effect_.param.size() >= 3) ||
                 (effect_.dll == "lngtwave.dll" && effect_.param == "vwi") ||
                 (effect_.dll == "trvswave.dll" && effect_.param == "h");
    if (!known) {
        logf("effect DLL %s is not emulated; crossfading instead", sjisToUtf8(spec).c_str());
        return false;
    }
    // The DLL gets the rectangle that changes. The original engine passes the rectangle of what it redrew; the
    // smallest one holding every changed pixel looks the same.
    const Image &from = effect_.from;
    int x0 = from.w, y0 = from.h, x1 = -1, y1 = -1;
    if (to.w == from.w && to.h == from.h)
        for (int y = 0; y < from.h; y++) {
            const uint32_t *a = from.row(y), *b = to.row(y);
            for (int x = 0; x < from.w; x++)
                if (a[x] != b[x]) {
                    x0 = std::min(x0, x);
                    x1 = std::max(x1, x);
                    y0 = std::min(y0, y);
                    y1 = std::max(y1, y);
                }
        }
    if (x1 < 0) {
        x0 = y0 = 0;
        x1 = from.w - 1;
        y1 = from.h - 1;
    }
    effect_.rx = x0;
    effect_.ry = y0;
    effect_.rw = x1 - x0 + 1;
    effect_.rh = y1 - y0 + 1;
    if (effect_.dll == "breakup.dll") {
        // Each piece flies in one of 8 directions, picked from a fixed sequence as the original's are, so a
        // transition looks the same every time.
        int cols = (effect_.rw + kCell - 1) / kCell, rows = (effect_.rh + kCell - 1) / kCell;
        uint32_t seed = 12345;
        effect_.dirs.resize(size_t(cols * rows));
        for (auto &d : effect_.dirs) {
            seed = seed * 1103515245u + 12345u;
            d = uint8_t(seed >> 16 & 7);
        }
    }
    return true;
}

void Engine::drawDllEffect(Image &out, double t) {
    const Image &from = effect_.from;
    if (from.w != out.w || from.h != out.h) return;
    const Image to = out;  // the new frame
    int W = out.w, H = out.h;
    int rx = effect_.rx, ry = effect_.ry, rw = effect_.rw, rh = effect_.rh;
    int k = int(t * 256);

    if (effect_.dll == "breakup.dll") {
        // The break sweeps from the start corner one cell per step. A piece shrinks in place for 4 steps, then flies
        // 7 pixels a step and keeps shrinking. Only pixels that change take part; the new frame shows behind them.
        Breakup b(effect_.param);
        int cols = (rw + kCell - 1) / kCell, rows = (rh + kCell - 1) / kCell;
        int total = (cols - 1) + (rows - 1) + kPieceLife;
        int step = std::min(int(t * total), total - 1);
        auto changed = [&](int x, int y) { return from.row(y)[x] != to.row(y)[x]; };
        // Pieces are numbered in rows from the start corner's row, left to right.
        auto drawCell = [&](int cx, int cy, bool waiting) {
            int dist = (b.hx > 0 ? cols - 1 - cx : cx) + (b.vy < 0 ? cy : rows - 1 - cy);
            int j = step - dist;
            if (waiting != (j < 0) || j >= kPieceLife) return;
            int x0 = rx + cx * kCell, y0 = ry + cy * kCell;
            int x1 = std::min(x0 + kCell, rx + rw), y1 = std::min(y0 + kCell, ry + rh);
            if (j < 0) {
                for (int y = y0; y < y1; y++)
                    for (int x = x0; x < x1; x++) out.row(y)[x] = from.row(y)[x];
                return;
            }
            int order = (b.vy < 0 ? cy : rows - 1 - cy) * cols + cx;
            double a = effect_.dirs[size_t(order)] * (135.0 / 7) * M_PI / 180;
            int vx = int(std::lround(7 * std::cos(a))) * b.hx * b.sign;
            int vy = int(std::lround(7 * std::sin(a))) * b.vy * b.sign;
            int moved = j < 4 ? 0 : j - 3;
            double r = pieceRadius(j), c = (kCell - 1) / 2.0;
            for (int y = y0; y < y1; y++) {
                int dy = y + vy * moved;
                if (dy < 0 || dy >= H) continue;
                for (int x = x0; x < x1; x++) {
                    double ex = x - x0 - c, ey = y - y0 - c;
                    int dx = x + vx * moved;
                    if (dx < 0 || dx >= W || ex * ex + ey * ey > r * r || !changed(x, y)) continue;
                    out.row(dy)[dx] = from.row(y)[x];
                }
            }
        };
        for (int cy = 0; cy < rows; cy++)
            for (int cx = 0; cx < cols; cx++) drawCell(cx, cy, true);
        for (int cy = 0; cy < rows; cy++)
            for (int cx = 0; cx < cols; cx++) drawCell(cx, cy, false);
        return;
    }

    if (effect_.dll == "lngtwave.dll") {
        // Rows ripple down the area in a wave whose period is half its height and which is pinned at the top, as the
        // two frames crossfade. Source row s is drawn at s + D(s); the amplitude dies away towards the end.
        double period = std::max(rh / 2.0, 1.0);
        double amp = 38 * std::pow(1 - t, 1.2), phase = 1.5 - 24.7 * t;
        std::vector<int> srcRow(size_t(rh), -1);
        int s = 0;
        for (int y = 0; y < rh; y++) {
            auto at = [&](int s) { return s + amp * (std::sin(2 * M_PI * s / period + phase) - std::sin(phase)); };
            while (s + 1 < rh && at(s + 1) <= y) s++;
            srcRow[size_t(y)] = s;
        }
        for (int y = 0; y < rh; y++) {
            int sy = ry + srcRow[size_t(y)];
            const uint32_t *a = from.row(sy), *n = to.row(sy);
            uint32_t *o = out.row(ry + y);
            for (int x = rx; x < rx + rw; x++) o[x] = lerpPx(a[x], n[x], k);
        }
        return;
    }

    if (effect_.dll == "trvswave.dll") {
        // Rows shift sideways in a sine wave that grows tighter and taller until halfway, then eases back, as the
        // two frames crossfade.
        double q = std::min(t, 1 - t);
        for (int y = 0; y < rh; y++) {
            int shift = int(std::lround(-16 * q * std::sin(2 * M_PI * q * y / 16)));
            const uint32_t *a = from.row(ry + y), *n = to.row(ry + y);
            uint32_t *o = out.row(ry + y);
            for (int x = rx; x < rx + rw; x++) {
                int sx = std::clamp(x + shift, 0, W - 1);  // edge rows reach past the area, as the original's do
                o[x] = lerpPx(a[sx], n[sx], k);
            }
        }
        return;
    }
}

}  // namespace vnns
