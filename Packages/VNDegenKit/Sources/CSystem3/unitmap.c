// The unit map (V commands): layered tile maps with four data planes per cell, drawn from tiles kept in the DIB.
// Behaviour follows the SDK manual; the step paint (VH) was checked against System39.exe under Wine with test
// scenarios (docs/system3-cleanroom.md).
#include "vm.h"

#define LAYERS 16

typedef struct Layer {
    u16 *plane[4];      // types 1 to 4: unit number, general data, step paint data, step paint result
    int tileX, tileY;   // where unit 0's tile is in the DIB (VP, VX0)
    int across, down;   // tiles per row and rows (VP, VX1)
    int key;            // transparent palette index, 256 for none (VP, VX2)
    int clearUnit;      // a unit number drawn as nothing (VZ1), or -1
    bool on;            // VV
} Layer;

typedef struct S3UnitMap {
    int layers, w, h;   // VC
    int originX, originY, unitW, unitH;
    Layer layer[LAYERS];
    u8 *dirty;          // per cell, for VF
    bool allDirty;
} S3UnitMap;

void s3_unitmap_free(S3 *s) {
    S3UnitMap *m = s->unitMap;
    if (!m) return;
    for (int l = 0; l < LAYERS; l++) for (int t = 0; t < 4; t++) free(m->layer[l].plane[t]);
    free(m->dirty);
    free(m);
    s->unitMap = NULL;
}

static S3UnitMap *map(S3 *s) { return s->unitMap; }

static u16 *plane(S3 *s, int layer, int type) {
    S3UnitMap *m = map(s);
    if (!m || layer < 0 || layer >= m->layers || type < 1 || type > 4) return NULL;
    return m->layer[layer].plane[type - 1];
}

static bool inside(S3UnitMap *m, int x, int y) { return x >= 0 && y >= 0 && x < m->w && y < m->h; }

static void draw_cell(S3 *s, int x, int y, int dx, int dy, int outside) {
    S3UnitMap *m = map(s);
    for (int l = 0; l < m->layers; l++) {
        Layer *L = &m->layer[l];
        if (!L->on || L->across <= 0) continue;
        int u = inside(m, x, y) ? L->plane[0][y * m->w + x] : outside;
        if (u == L->clearUnit) continue;
        int sx = L->tileX + (u % L->across) * m->unitW, sy = L->tileY + (u / L->across) * m->unitH;
        if (L->key >= 0 && L->key < 256) s3_copy_key(s, sx, sy, m->unitW, m->unitH, dx, dy, L->key);
        else s3_copy(s, sx, sy, m->unitW, m->unitH, dx, dy);
    }
}

// VF: draws the cells that changed since the last VF, at the map's origin.
static void draw_changed(S3 *s) {
    S3UnitMap *m = map(s);
    if (!m || m->unitW <= 0 || m->unitH <= 0) return;
    for (int y = 0; y < m->h; y++)
        for (int x = 0; x < m->w; x++) {
            if (!m->allDirty && !m->dirty[y * m->w + x]) continue;
            int dx = m->originX + x * m->unitW, dy = m->originY + y * m->unitH;
            if (dx >= s->dib.w || dy >= s->dib.h) continue;
            draw_cell(s, x, y, dx, dy, 0);
        }
    memset(m->dirty, 0, (size_t)m->w * m->h);
    m->allDirty = false;
}

// VE: draws a window of the map at the map's origin, with `outside` for cells beyond the map's edge.
static void draw_window(S3 *s, int x0, int y0, int w, int h, int outside) {
    S3UnitMap *m = map(s);
    if (!m || m->unitW <= 0 || m->unitH <= 0) return;
    for (int j = 0; j < h; j++)
        for (int i = 0; i < w; i++)
            draw_cell(s, x0 + i, y0 + j, m->originX + i * m->unitW, m->originY + j * m->unitH, outside);
}

static void mark(S3UnitMap *m, int x, int y) { if (inside(m, x, y)) m->dirty[y * m->w + x] = 1; }

// VH: a breadth-first walk over the top-left positions of a mover w by h units, in four directions. A position is
// open when the mover stays inside the map and covers no cell whose step paint data is 255. The start gets 0, open
// positions their step count up to max, and every other cell 255.
static void step_paint(S3 *s, int layer, int sx, int sy, int w, int h, int max) {
    S3UnitMap *m = map(s);
    u16 *data = plane(s, layer, 3), *out = plane(s, layer, 4);
    if (!data || !out) return;
    int n = m->w * m->h;
    for (int i = 0; i < n; i++) out[i] = 255;
    if (!inside(m, sx, sy)) return;
    int *queue = s3_alloc(sizeof(int) * (size_t)n);
    int head = 0, tail = 0;
    out[sy * m->w + sx] = 0;
    queue[tail++] = sy * m->w + sx;
    static const int step[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    while (head < tail) {
        int c = queue[head++], cx = c % m->w, cy = c / m->w, d = out[c];
        if (d >= max || d >= 254) continue; // 255 marks unreached cells, so counts stop at 254
        for (int k = 0; k < 4; k++) {
            int x = cx + step[k][0], y = cy + step[k][1];
            if (x < 0 || y < 0 || x + w > m->w || y + h > m->h || out[y * m->w + x] != 255) continue;
            bool open = true;
            for (int j = 0; j < h && open; j++)
                for (int i = 0; i < w && open; i++)
                    if (data[(y + j) * m->w + x + i] == 255) open = false;
            if (!open) continue;
            out[y * m->w + x] = (u16)(d + 1);
            queue[tail++] = y * m->w + x;
        }
    }
    free(queue);
}

void s3_unitmap_command(S3 *s, int k, const u16 *a, const S3LV *lv) {
    S3UnitMap *m = map(s);
    switch (k) {
    case 'C': { // VC layers, origin x, y, width, height, unit width, height
        s3_unitmap_free(s);
        int layers = a[0] < LAYERS ? a[0] : LAYERS, w = a[3], h = a[4];
        if (layers <= 0 || w <= 0 || h <= 0 || (size_t)w * h > 4u << 20) { s3_var_set(s, 0, 0); return; }
        m = s->unitMap = s3_alloc(sizeof *m);
        m->layers = layers; m->w = w; m->h = h;
        m->originX = a[1]; m->originY = a[2]; m->unitW = a[5]; m->unitH = a[6];
        for (int l = 0; l < layers; l++) {
            for (int t = 0; t < 4; t++) m->layer[l].plane[t] = s3_alloc(sizeof(u16) * (size_t)w * h);
            m->layer[l].key = 256;
            m->layer[l].clearUnit = -1;
        }
        m->dirty = s3_alloc((size_t)w * h);
        m->allDirty = true;
        s3_var_set(s, 0, 1);
        return;
    }
    case 'P': if (m && a[0] < m->layers) { Layer *L = &m->layer[a[0]]; L->tileX = a[1]; L->tileY = a[2]; L->across = a[3]; L->down = a[4]; L->key = a[5]; m->allDirty = true; } return;
    case 'V': if (m && a[0] < m->layers) { m->layer[a[0]].on = a[1] != 0; m->allDirty = true; } return;
    case 'F': draw_changed(s); return;
    case 'E': draw_window(s, (int16_t)a[0], (int16_t)a[1], a[2], a[3], a[4]); return;
    case 'S': case 'G': {
        u16 *p = plane(s, a[0], a[1]);
        int x = a[2], y = a[3];
        // Outside the map VG gives 65535 and VS changes nothing, RND included (System39.exe under Wine).
        if (!p || !inside(m, x, y)) { if (k == 'G') s3_var_set(s, 0, 65535); return; }
        u16 *c = &p[y * m->w + x];
        s3_var_set(s, 0, *c);
        if (k == 'S') { if (a[1] == 1 && *c != a[4]) mark(m, x, y); *c = a[4]; }
        return;
    }
    case 'R': case 'W': { // VR: variables to the plane; VW: the plane to variables
        u16 *p = plane(s, a[0], a[1]);
        if (!p || !lv) return;
        for (int i = 0; i < m->w * m->h; i++) {
            if (k == 'R') { u16 v = s3_lv_get(s, lv, i); if (a[1] == 1 && p[i] != v) m->dirty[i] = 1; p[i] = v; }
            else s3_lv_set(s, lv, i, p[i]);
        }
        return;
    }
    case 'H': step_paint(s, a[0], a[1], a[2], a[3] ? a[3] : 1, a[4] ? a[4] : 1, a[5]); return;
    case 'T': { // VT: copies a rectangle between planes
        u16 *src = plane(s, a[0], a[1]), *dst = plane(s, a[6], a[7]);
        if (!src || !dst) return;
        int w = a[4], h = a[5];
        u16 *tmp = s3_alloc(sizeof(u16) * (size_t)(w * h + 1));
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) if (inside(m, a[2] + i, a[3] + j)) tmp[j * w + i] = src[(a[3] + j) * m->w + a[2] + i];
        for (int j = 0; j < h; j++) for (int i = 0; i < w; i++) if (inside(m, a[8] + i, a[9] + j)) { dst[(a[9] + j) * m->w + a[8] + i] = tmp[j * w + i]; if (a[7] == 1) mark(m, a[8] + i, a[9] + j); }
        free(tmp);
        return;
    }
    case 'B': { // VB: fills a rectangle of one plane
        u16 *p = plane(s, a[0], a[1]);
        if (!p) return;
        for (int j = 0; j < a[5]; j++) for (int i = 0; i < a[4]; i++) if (inside(m, a[2] + i, a[3] + j)) { p[(a[3] + j) * m->w + a[2] + i] = a[6]; if (a[1] == 1) mark(m, a[2] + i, a[3] + j); }
        return;
    }
    case 'I': // VIC (unit cells) and VIP (pixels): force cells to redraw at the next VF
        if (!m) return;
        if (a[4]) { // VIP
            if (m->unitW <= 0 || m->unitH <= 0) return;
            int x0 = (a[0] - m->originX) / m->unitW, y0 = (a[1] - m->originY) / m->unitH;
            int x1 = (a[0] + a[2] - 1 - m->originX) / m->unitW, y1 = (a[1] + a[3] - 1 - m->originY) / m->unitH;
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) mark(m, x, y);
        } else {
            for (int y = a[1]; y < a[1] + a[3]; y++) for (int x = a[0]; x < a[0] + a[2]; x++) mark(m, x, y);
        }
        return;
    case 'X': // VX sub, layer, a, b
        if (!m) return;
        if (a[0] == 3) { m->allDirty = true; return; }
        if (a[1] >= m->layers) return;
        if (a[0] == 0) { m->layer[a[1]].tileX = a[2]; m->layer[a[1]].tileY = a[3]; }
        else if (a[0] == 1) { m->layer[a[1]].across = a[2]; m->layer[a[1]].down = a[3]; }
        else if (a[0] == 2) m->layer[a[1]].key = a[2];
        m->allDirty = true;
        return;
    case 'Z': // VZ sub, a, b
        if (!m) return;
        if (a[0] == 0 && a[1] < m->layers) m->layer[a[1]].clearUnit = -1;
        else if (a[0] == 1 && a[1] < m->layers) m->layer[a[1]].clearUnit = a[2];
        else if (a[0] == 2) { m->originX = a[1]; m->originY = a[2]; m->allDirty = true; }
        else if (a[0] == 3) { m->unitW = a[1]; m->unitH = a[2]; m->allDirty = true; }
        return;
    }
}
