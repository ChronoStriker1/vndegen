#include "sg.h"

#include <dirent.h>

static bool load_sgr(Pack *p, const char *name, Sgr *out) {
    size_t size = 0;
    u8 *data = pack_read(p, name, &size);
    if (!data) return false;
    if (!sgr_parse(out, data, size)) {
        sgr_free(out);
        return false;
    }
    return true;
}

static char *find_pack_file(const char *dir) {
    DIR *d = opendir(dir);
    if (!d) return NULL;
    char *found = NULL;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (strcasecmp(e->d_name, "data.sgbpack") == 0) {
            found = sg_fmt("%s/%s", dir, e->d_name);
            break;
        }
    }
    closedir(d);
    return found;
}

static bool dlc_guid(Pack *p, const char *dir, Guid *out) {
    static const char *names[] = {"dlcinfo_en.xml", "dlcinfo.xml", "dlcinfo_ja.xml"};
    for (int i = 0; i < 3; i++) {
        char *path = sg_fmt("%s/%s", dir, names[i]);
        size_t size = 0;
        u8 *xml = pack_read(p, path, &size);
        free(path);
        if (!xml) continue;
        xml[size] = 0;
        char *a = strstr((char *)xml, "<guid>");
        char *end = a ? strstr(a, "</guid>") : NULL;
        if (end) *end = 0;
        bool ok = end && guid_parse(a + 6, out);
        free(xml);
        if (ok) return true;
    }
    return false;
}

bool game_load(SG *g, char *err, size_t errSize) {
    Game *d = &g->data;
    char *packPath = find_pack_file(g->gameDir);
    if (!packPath) {
        snprintf(err, errSize, "no data.sgbpack in %s", g->gameDir);
        return false;
    }
    bool ok = pack_open(&d->pack, packPath, err, errSize);
    free(packPath);
    if (!ok) return false;
    if (!load_sgr(&d->pack, "GameSettings.sgr", &d->settingsFile) || d->settingsFile.count < 1 ||
        !settings_parse(&d->settings, &d->settingsFile.recs[0])) {
        snprintf(err, errSize, "can't read GameSettings.sgr");
        return false;
    }
    load_sgr(&d->pack, "ResourceItem.sgr", &d->resFile);
    load_sgr(&d->pack, "Hero.sgr", &d->heroFile);
    int capPacks = 8, capMaps = 64;
    d->packRes = sg_alloc(sizeof(Sgr) * (size_t)capPacks);
    d->packDirs = sg_alloc(sizeof(char *) * (size_t)capPacks);
    d->packGuids = sg_alloc(sizeof(Guid) * (size_t)capPacks);
    d->maps = sg_alloc(sizeof(Sgr) * (size_t)capMaps);
    for (int i = 0; i < d->pack.count; i++) {
        const char *key = d->pack.entries[i].key;
        size_t n = strlen(key);
        if (strncmp(key, "map/", 4) == 0 && n > 4 && strcmp(key + n - 4, ".sgr") == 0) {
            if (d->mapCount == capMaps) {
                capMaps *= 2;
                d->maps = sg_realloc(d->maps, sizeof(Sgr) * (size_t)capMaps);
            }
            if (load_sgr(&d->pack, key, &d->maps[d->mapCount])) d->mapCount++;
        } else if (strncmp(key, "samples/", 8) == 0 && n > 17 && strcmp(key + n - 17, "/resourceitem.sgr") == 0) {
            if (d->packCount == capPacks) {
                capPacks *= 2;
                d->packRes = sg_realloc(d->packRes, sizeof(Sgr) * (size_t)capPacks);
                d->packDirs = sg_realloc(d->packDirs, sizeof(char *) * (size_t)capPacks);
                d->packGuids = sg_realloc(d->packGuids, sizeof(Guid) * (size_t)capPacks);
            }
            if (!load_sgr(&d->pack, key, &d->packRes[d->packCount])) continue;
            char *dir = sg_strdup(key);
            dir[n - 17] = 0;
            d->packDirs[d->packCount] = dir;
            memset(&d->packGuids[d->packCount], 0, sizeof(Guid));
            dlc_guid(&d->pack, dir, &d->packGuids[d->packCount]);
            d->packCount++;
        }
    }
    sg_log(g, "pack: %d entries, %d maps, %d resource packs", d->pack.count, d->mapCount, d->packCount);
    return true;
}

void game_free(Game *d) {
    pack_close(&d->pack);
    sgr_free(&d->settingsFile);
    sgr_free(&d->resFile);
    sgr_free(&d->heroFile);
    for (int i = 0; i < d->packCount; i++) {
        sgr_free(&d->packRes[i]);
        free(d->packDirs[i]);
    }
    free(d->packRes);
    free(d->packDirs);
    free(d->packGuids);
    for (int i = 0; i < d->mapCount; i++) sgr_free(&d->maps[i]);
    free(d->maps);
    settings_free(&d->settings);
    memset(d, 0, sizeof *d);
}

static const SgrRec *find_in(Sgr *s, u8 type, u8 sub, Guid id) {
    for (int i = 0; i < s->count; i++) {
        const SgrRec *r = &s->recs[i];
        if (r->type == type && r->sub == sub && r->size >= 16 && memcmp(r->body, id.b, 16) == 0) return r;
    }
    return NULL;
}

const SgrRec *game_record(Game *d, u8 type, u8 sub, Guid id, Sgr **file) {
    for (int i = 0; i < d->mapCount; i++) {
        const SgrRec *r = find_in(&d->maps[i], type, sub, id);
        if (r) {
            if (file) *file = &d->maps[i];
            return r;
        }
    }
    return NULL;
}

const SgrRec *game_hero(Game *d, Guid id) { return find_in(&d->heroFile, SGR_HERO, 0, id); }

static void add_res(ResTable *t, int *cap, const SgrRec *rec, const char *dir, const Game *d) {
    if (rec->type != SGR_RESOURCE) return;
    Rd r = rd_make(rec->body, rec->size);
    Res res = {0};
    res.id = rd_guid(&r);
    res.kind = rec->sub;
    res.name = rd_str(&r);
    char *path = rd_str(&r);
    if (res.kind == RES_CHARACTER) {
        res.frames = rd_i32(&r);
        res.frameMs = rd_i32(&r);
        res.flagA = rd_i32(&r);
        res.divisions = rd_i32(&r);
    } else if (res.kind == RES_WINDOW) {
        for (int i = 0; i < 4; i++) res.border[i] = rd_i32(&r);
    }
    const char *rel = path;
    const char *base = dir;
    if (strncmp(path, "dlc:", 4) == 0) {
        const char *bar = strchr(path, '|');
        Guid dg;
        char gs[64] = {0};
        if (bar && (size_t)(bar - path - 4) < sizeof gs) memcpy(gs, path + 4, (size_t)(bar - path - 4));
        base = "samples/common";
        if (guid_parse(gs, &dg))
            for (int i = 0; i < d->packCount; i++)
                if (guid_eq(d->packGuids[i], dg)) base = d->packDirs[i];
        rel = bar ? bar + 1 : path + 4;
    } else if (strncmp(path, "sys:", 4) == 0) {
        rel = path + 4;
        base = "samples/common";
    }
    if (rel[0] == '.' && (rel[1] == '\\' || rel[1] == '/')) rel += 2;
    res.path = base[0] ? sg_fmt("%s/%s", base, rel) : sg_strdup(rel);
    free(path);
    if (t->count == *cap) {
        *cap *= 2;
        t->items = sg_realloc(t->items, sizeof(Res) * (size_t)*cap);
    }
    t->items[t->count++] = res;
}

bool res_load(SG *g) {
    Game *d = &g->data;
    ResTable *t = &g->res;
    int cap = 256;
    t->items = sg_alloc(sizeof(Res) * (size_t)cap);
    for (int i = 0; i < d->resFile.count; i++) add_res(t, &cap, &d->resFile.recs[i], "", d);
    for (int p = 0; p < d->packCount; p++)
        for (int i = 0; i < d->packRes[p].count; i++) add_res(t, &cap, &d->packRes[p].recs[i], d->packDirs[p], d);
    sg_log(g, "resources: %d", t->count);
    return true;
}

void res_free(ResTable *t) {
    for (int i = 0; i < t->count; i++) {
        free(t->items[i].name);
        free(t->items[i].path);
    }
    free(t->items);
    memset(t, 0, sizeof *t);
}

const Res *res_find(const ResTable *t, Guid id) {
    for (int i = 0; i < t->count; i++)
        if (guid_eq(t->items[i].id, id)) return &t->items[i];
    return NULL;
}

Image *sg_image(SG *g, Guid id) {
    if (guid_zero(id)) return NULL;
    for (int i = 0; i < g->imageCacheCount; i++)
        if (guid_eq(g->imageCache[i].id, id)) return g->imageCache[i].im;
    const Res *r = res_find(&g->res, id);
    Image *im = NULL;
    if (r) {
        size_t size = 0;
        u8 *data = pack_read(&g->data.pack, r->path, &size);
        if (data) im = img_decode(data, size);
        free(data);
        if (!im) sg_log(g, "image missing: %s", r->path);
    }
    g->imageCache = sg_realloc(g->imageCache, sizeof *g->imageCache * (size_t)(g->imageCacheCount + 1));
    g->imageCache[g->imageCacheCount].id = id;
    g->imageCache[g->imageCacheCount].im = im;
    g->imageCacheCount++;
    return im;
}
