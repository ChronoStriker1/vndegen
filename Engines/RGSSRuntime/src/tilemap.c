// Tilemaps. VX and VX Ace draw two layers: tiles below characters at z 0 and tiles flagged as above them (the star,
// flag 0x10) at z 200. The tile ID ranges, the autotile quarter tables and the A1 to A4 sheet layouts are those of
// RPG Maker MV's corescript (MIT), which kept VX Ace's tile format; docs/rgss-cleanroom.md has the details.
#include "scene.h"

#include <stdlib.h>

enum { TILE = 32, HALF = 16 };
enum { ID_A5 = 1536, ID_A1 = 2048, ID_A2 = 2816, ID_A3 = 4352, ID_A4 = 5888, ID_MAX = 8192 };

// For each of the 48 floor shapes, 16 wall shapes and 4 waterfall shapes, where its four quarters (top left, top
// right, bottom left, bottom right) come from, in quarter-tile units within the autotile's block.
static const unsigned char floorTable[48][4][2] = {
    {{2,4},{1,4},{2,3},{1,3}},{{2,0},{1,4},{2,3},{1,3}},{{2,4},{3,0},{2,3},{1,3}},{{2,0},{3,0},{2,3},{1,3}},
    {{2,4},{1,4},{2,3},{3,1}},{{2,0},{1,4},{2,3},{3,1}},{{2,4},{3,0},{2,3},{3,1}},{{2,0},{3,0},{2,3},{3,1}},
    {{2,4},{1,4},{2,1},{1,3}},{{2,0},{1,4},{2,1},{1,3}},{{2,4},{3,0},{2,1},{1,3}},{{2,0},{3,0},{2,1},{1,3}},
    {{2,4},{1,4},{2,1},{3,1}},{{2,0},{1,4},{2,1},{3,1}},{{2,4},{3,0},{2,1},{3,1}},{{2,0},{3,0},{2,1},{3,1}},
    {{0,4},{1,4},{0,3},{1,3}},{{0,4},{3,0},{0,3},{1,3}},{{0,4},{1,4},{0,3},{3,1}},{{0,4},{3,0},{0,3},{3,1}},
    {{2,2},{1,2},{2,3},{1,3}},{{2,2},{1,2},{2,3},{3,1}},{{2,2},{1,2},{2,1},{1,3}},{{2,2},{1,2},{2,1},{3,1}},
    {{2,4},{3,4},{2,3},{3,3}},{{2,4},{3,4},{2,1},{3,3}},{{2,0},{3,4},{2,3},{3,3}},{{2,0},{3,4},{2,1},{3,3}},
    {{2,4},{1,4},{2,5},{1,5}},{{2,0},{1,4},{2,5},{1,5}},{{2,4},{3,0},{2,5},{1,5}},{{2,0},{3,0},{2,5},{1,5}},
    {{0,4},{3,4},{0,3},{3,3}},{{2,2},{1,2},{2,5},{1,5}},{{0,2},{1,2},{0,3},{1,3}},{{0,2},{1,2},{0,3},{3,1}},
    {{2,2},{3,2},{2,3},{3,3}},{{2,2},{3,2},{2,1},{3,3}},{{2,4},{3,4},{2,5},{3,5}},{{2,0},{3,4},{2,5},{3,5}},
    {{0,4},{1,4},{0,5},{1,5}},{{0,4},{3,0},{0,5},{1,5}},{{0,2},{3,2},{0,3},{3,3}},{{0,2},{1,2},{0,5},{1,5}},
    {{0,4},{3,4},{0,5},{3,5}},{{2,2},{3,2},{2,5},{3,5}},{{0,2},{3,2},{0,5},{3,5}},{{0,0},{1,0},{0,1},{1,1}},
};

static const unsigned char wallTable[16][4][2] = {
    {{2,2},{1,2},{2,1},{1,1}},{{0,2},{1,2},{0,1},{1,1}},{{2,0},{1,0},{2,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},
    {{2,2},{3,2},{2,1},{3,1}},{{0,2},{3,2},{0,1},{3,1}},{{2,0},{3,0},{2,1},{3,1}},{{0,0},{3,0},{0,1},{3,1}},
    {{2,2},{1,2},{2,3},{1,3}},{{0,2},{1,2},{0,3},{1,3}},{{2,0},{1,0},{2,3},{1,3}},{{0,0},{1,0},{0,3},{1,3}},
    {{2,2},{3,2},{2,3},{3,3}},{{0,2},{3,2},{0,3},{3,3}},{{2,0},{3,0},{2,3},{3,3}},{{0,0},{3,0},{0,3},{3,3}},
};

static const unsigned char waterfallTable[4][4][2] = {
    {{2,0},{1,0},{2,1},{1,1}},{{0,0},{1,0},{0,1},{1,1}},{{2,0},{3,0},{2,1},{3,1}},{{0,0},{3,0},{0,1},{3,1}},
};

static const Effects plain = {NULL, NULL, NULL, 255, 0};

static int map_tile(const Table *t, int x, int y, int z) {
    if (!t || t->xs <= 0 || t->ys <= 0 || z >= t->zs) return 0;
    // Maps repeat past their edges, as looping maps need.
    x = ((x % t->xs) + t->xs) % t->xs;
    y = ((y % t->ys) + t->ys) % t->ys;
    return t->data[((size_t)z * t->ys + y) * t->xs + x];
}

static int flags_of(const Tilemap *tm, int id) {
    if (!tm->flags || id < 0 || id >= tm->flags->xs) return 0;
    return tm->flags->data[id];
}

static const Image *sheet(const Tilemap *tm, int index) {
    Bitmap *b = tm->bitmaps[index];
    return b && !b->disposed ? &b->image : NULL;
}

// A part of a sheet onto the screen, clipped.
static void put(Image *target, IRect clip, const Image *src, int sx, int sy, int w, int h, int dx, int dy) {
    IRect area = {dx, dy, w, h};
    int x0 = area.x > clip.x ? area.x : clip.x, y0 = area.y > clip.y ? area.y : clip.y;
    int x1 = area.x + w < clip.x + clip.w ? area.x + w : clip.x + clip.w;
    int y1 = area.y + h < clip.y + clip.h ? area.y + h : clip.y + clip.h;
    if (x0 >= x1 || y0 >= y1) return;
    draw_image_rect(target, (IRect){x0, y0, x1 - x0, y1 - y0}, src, (IRect){sx, sy, w, h}, dx, dy, &plain);
}

static void draw_normal(const Tilemap *tm, int id, Image *target, IRect clip, int dx, int dy) {
    int set = id >= ID_A5 ? 4 : 5 + id / 256;
    const Image *src = sheet(tm, set);
    if (!src) return;
    int sx = ((id / 128) % 2 * 8 + id % 8) * TILE;
    int sy = (id % 256 / 8 % 16) * TILE;
    if (id >= ID_A5) {
        int n = id - ID_A5;
        sx = n % 8 * TILE;
        sy = n / 8 * TILE;
    }
    put(target, clip, src, sx, sy, TILE, TILE, dx, dy);
}

static bool is_table(const Tilemap *tm, int id) { return id >= ID_A2 && id < ID_A3 && (flags_of(tm, id) & 0x80); }

static void draw_autotile(const Tilemap *tm, int id, Image *target, IRect clip, int dx, int dy) {
    int kind = (id - ID_A1) / 48, shape = (id - ID_A1) % 48;
    int tx = kind % 8, ty = kind / 8, bx = 0, by = 0, set = 0;
    const unsigned char (*table)[2] = floorTable[shape];
    int frame = tm->animCount / 30;
    static const int waterFrames[4] = {0, 1, 2, 1};
    bool table2 = false;
    if (id < ID_A2) {
        int surface = waterFrames[frame % 4];
        if (kind == 0) { bx = surface * 2; by = 0; }
        else if (kind == 1) { bx = surface * 2; by = 3; }
        else if (kind == 2) { bx = 6; by = 0; }
        else if (kind == 3) { bx = 6; by = 3; }
        else {
            bx = tx / 4 * 8;
            by = ty * 6 + tx / 2 % 2 * 3;
            if (kind % 2 == 0) bx += surface * 2;
            else {
                bx += 6;
                if (shape >= 4) return;
                table = waterfallTable[shape];
                by += frame % 3;
            }
        }
    } else if (id < ID_A3) {
        set = 1;
        bx = tx * 2;
        by = (ty - 2) * 3;
        table2 = is_table(tm, id);
    } else if (id < ID_A4) {
        set = 2;
        bx = tx * 2;
        by = (ty - 6) * 2;
        if (shape >= 16) return;
        table = wallTable[shape];
    } else {
        set = 3;
        bx = tx * 2;
        // A4 alternates rows of wall tops (3 tiles tall, floor shapes) and wall sides (2 tiles tall, wall shapes).
        by = (int)((ty - 10) * 2.5 + (ty % 2 == 1 ? 0.5 : 0));
        if (ty % 2 == 1) {
            if (shape >= 16) return;
            table = wallTable[shape];
        }
    }
    const Image *src = sheet(tm, set);
    if (!src) return;
    for (int i = 0; i < 4; i++) {
        int qx = table[i][0], qy = table[i][1];
        int sx = (bx * 2 + qx) * HALF, sy = (by * 2 + qy) * HALF;
        int qdx = dx + i % 2 * HALF, qdy = dy + i / 2 * HALF;
        if (table2 && (qy == 1 || qy == 5)) {
            // A table's legs: the top half of the quarter comes from the table's middle row.
            int qx2 = qy == 1 ? (int[]){0, 3, 2, 1}[qx] : qx;
            put(target, clip, src, (bx * 2 + qx2) * HALF, (by * 2 + 3) * HALF, HALF, HALF, qdx, qdy);
            put(target, clip, src, sx, sy, HALF, HALF / 2, qdx, qdy + HALF / 2);
        } else {
            put(target, clip, src, sx, sy, HALF, HALF, qdx, qdy);
        }
    }
}

// The lower half of a table's front edge, drawn on the tile below a table.
static void draw_table_edge(const Tilemap *tm, int id, Image *target, IRect clip, int dx, int dy) {
    int kind = (id - ID_A1) / 48, shape = (id - ID_A1) % 48;
    int bx = kind % 8 * 2, by = (kind / 8 - 2) * 3;
    const Image *src = sheet(tm, 1);
    if (!src) return;
    for (int i = 0; i < 2; i++) {
        int qx = floorTable[shape][2 + i][0], qy = floorTable[shape][2 + i][1];
        put(target, clip, src, (bx * 2 + qx) * HALF, (by * 2 + qy) * HALF + HALF / 2, HALF, HALF / 2, dx + i * HALF, dy);
    }
}

static void draw_tile(const Tilemap *tm, int id, Image *target, IRect clip, int dx, int dy) {
    if (id <= 0 || id >= ID_MAX) return;
    if (id >= ID_A1) draw_autotile(tm, id, target, clip, dx, dy);
    else draw_normal(tm, id, target, clip, dx, dy);
}

// VX Ace's shadows: one bit per quarter of the tile, drawn as half-transparent black.
static void draw_shadow(int bits, Image *target, IRect clip, int dx, int dy) {
    for (int i = 0; i < 4; i++) {
        if (!(bits & (1 << i))) continue;
        IRect q = {dx + i % 2 * HALF, dy + i / 2 * HALF, HALF, HALF};
        int x0 = q.x > clip.x ? q.x : clip.x, y0 = q.y > clip.y ? q.y : clip.y;
        int x1 = q.x + HALF < clip.x + clip.w ? q.x + HALF : clip.x + clip.w;
        int y1 = q.y + HALF < clip.y + clip.h ? q.y + HALF : clip.y + clip.h;
        for (int y = y0; y < y1; y++) {
            Pixel *row = target->px + (size_t)y * target->w;
            for (int x = x0; x < x1; x++) {
                Pixel p = row[x];
                row[x] = px_make(px_r(p) / 2, px_g(p) / 2, px_b(p) / 2, 255);
            }
        }
    }
}

int tilemap_layers(Tilemap *tm, int *zs, int max) {
    if (!tm->visible || tm->disposed || !tm->mapData || max < 2) return 0;
    if (rgss_version == RGSS1) return 0;
    zs[0] = 0;
    zs[1] = 200;
    return 2;
}

void tilemap_draw(Tilemap *tm, int layer, Image *target, IRect clip, int offX, int offY) {
    const Table *map = tm->mapData;
    if (!map) return;
    int originX = offX - tm->ox, originY = offY - tm->oy;
    int x0 = (clip.x - originX) / TILE - 1, y0 = (clip.y - originY) / TILE - 1;
    int x1 = (clip.x + clip.w - originX) / TILE + 1, y1 = (clip.y + clip.h - originY) / TILE + 1;
    bool ace = rgss_version == RGSS3;
    for (int my = y0; my <= y1; my++) {
        for (int mx = x0; mx <= x1; mx++) {
            int dx = originX + mx * TILE, dy = originY + my * TILE;
            if (dx >= clip.x + clip.w || dy >= clip.y + clip.h || dx + TILE <= clip.x || dy + TILE <= clip.y) continue;
            for (int z = 0; z < 3; z++) {
                // The ground layer's shadows and table edges go over layers 0 and 1, under the B to E tiles.
                if (layer == 0 && z == 2 && ace) {
                    int shadow = map_tile(map, mx, my, 3) & 0x0F;
                    if (shadow) draw_shadow(shadow, target, clip, dx, dy);
                    int above = map_tile(map, mx, my - 1, 1);
                    if (is_table(tm, above) && !is_table(tm, map_tile(map, mx, my, 1))) {
                        draw_table_edge(tm, above, target, clip, dx, dy);
                    }
                }
                int id = map_tile(map, mx, my, z);
                bool upper = (flags_of(tm, id) & 0x10) != 0;
                if (upper == (layer == 1)) draw_tile(tm, id, target, clip, dx, dy);
            }
        }
    }
}

void tilemap_update(Tilemap *tm) { tm->animCount++; }
