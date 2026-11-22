#include "sa.h"

bool sa_table_load(Table *t, Vfs *vfs, const char *name) {
    memset(t, 0, sizeof *t);
    size_t n;
    u8 *d = sa_vfs_read(vfs, name, &n);
    if (!d) return false;
    for (size_t i = 0; i < n; i++) d[i] = (u8)~d[i];
    bool ok = false;
    if (n >= 8) {
        i32 cols = ~(i32)rd32(d + n - 8), cells = ~(i32)rd32(d + n - 4);
        if (cols > 0 && cells >= 0 && cells % cols == 0 && (size_t)cells * 4 + (size_t)cols + 8 <= n) {
            t->cols = cols;
            t->rows = cells / cols;
            t->cell = sa_alloc(sizeof(i32) * (size_t)(cells ? cells : 1));
            t->isText = sa_alloc((size_t)cols);
            t->text = sa_alloc(sizeof(char *) * (size_t)(cells ? cells : 1));
            const u8 *types = d + n - 8 - cols;
            const u8 *pool = d + (size_t)cells * 4;
            size_t poolSize = (size_t)(types - pool);
            for (int c = 0; c < cols; c++) t->isText[c] = types[c] == 0x00;
            for (int i = 0; i < cells; i++) {
                t->cell[i] = (i32)rd32(d + (size_t)i * 4);
                if (!t->isText[i % cols]) continue;
                u32 off = (u32)t->cell[i];
                if (off + 1 >= poolSize) {
                    t->text[i] = sa_strdup("");
                    continue;
                }
                size_t k = off;
                while (k + 1 < poolSize && (pool[k] || pool[k + 1])) k += 2;
                t->text[i] = sa_utf16_to_utf8(pool + off, (int)((k - off) / 2));
            }
            ok = true;
        }
    }
    free(d);
    return ok;
}

void sa_table_free(Table *t) {
    for (int i = 0; i < t->rows * t->cols; i++) free(t->text[i]);
    free(t->text);
    free(t->cell);
    free(t->isText);
    memset(t, 0, sizeof *t);
}

i32 sa_table_int(const Table *t, int row, int col) {
    if (row < 0 || row >= t->rows || col < 0 || col >= t->cols) return 0;
    return t->cell[row * t->cols + col];
}

const char *sa_table_text(const Table *t, int row, int col) {
    if (row < 0 || row >= t->rows || col < 0 || col >= t->cols || !t->isText[col]) return "";
    const char *s = t->text[row * t->cols + col];
    return s ? s : "";
}
