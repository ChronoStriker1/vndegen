#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sg.h"

static int failures, checks;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        checks++;                                                                                                      \
        if (!(cond)) {                                                                                                 \
            failures++;                                                                                                \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #cond);                                                 \
        }                                                                                                              \
    } while (0)

static Guid guid(const char *s) {
    Guid g = {{0}};
    guid_parse(s, &g);
    return g;
}

static void put32(u8 *p, u32 v) {
    for (int i = 0; i < 4; i++) p[i] = (u8)(v >> (8 * i));
}

static void put16(u8 *p, u16 v) {
    p[0] = (u8)v;
    p[1] = (u8)(v >> 8);
}

static void test_pack_synthetic(void) {
    const char *name = "Map\\Map_001.sgr";
    const char *body = "YUKAR-test";
    size_t nl = strlen(name), bl = strlen(body);
    u8 buf[512] = {0};
    size_t o = 0;
    memcpy(buf, "YUKARPKG", 8);
    put16(buf + 8, 0);
    put32(buf + 18, (u32)bl);
    put32(buf + 22, (u32)bl);
    put16(buf + 26, (u16)nl);
    memcpy(buf + 30, name, nl);
    memcpy(buf + 30 + nl, body, bl);
    o = 30 + nl + bl;
    size_t dir = o;
    put32(buf + o, 0x02014b50);
    put32(buf + o + 20, (u32)bl);
    put32(buf + o + 24, (u32)bl);
    put16(buf + o + 28, (u16)nl);
    put32(buf + o + 42, 0);
    memcpy(buf + o + 46, name, nl);
    o += 46 + nl;
    size_t dirSize = o - dir;
    put32(buf + o, 0x06054b50);
    put16(buf + o + 8, 1);
    put16(buf + o + 10, 1);
    put32(buf + o + 12, (u32)dirSize);
    put32(buf + o + 16, (u32)dir);
    o += 22;
    char path[] = "/tmp/sgtest-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    if (fd < 0) return;
    CHECK(write(fd, buf, o) == (ssize_t)o);
    close(fd);
    Pack p;
    char err[256];
    CHECK(pack_open(&p, path, err, sizeof err));
    CHECK(p.count == 1);
    size_t size = 0;
    u8 *data = pack_read(&p, "map/MAP_001.SGR", &size);
    CHECK(data && size == bl && memcmp(data, body, bl) == 0);
    free(data);
    pack_close(&p);
    unlink(path);
}

static void test_script_synthetic(void) {
    u8 b[128] = {0};
    size_t o = 16;
    b[o++] = 0;
    put32(b + o, 1), o += 4;
    put32(b + o, 2), o += 4;
    put32(b + o, 44), o += 4;
    put32(b + o, 0), o += 4;
    put32(b + o, 0x17), o += 4;
    b[o++] = 3;
    memcpy(b + o, "abc", 3), o += 3;
    put32(b + o, 5), o += 4;
    put32(b + o, 6), o += 4;
    put32(b + o, 37), o += 4;
    put32(b + o, 0), o += 4;
    put32(b + o, 0x40000006), o += 4;
    put32(b + o, 0x1), o += 4;
    put32(b + o, 7), o += 4;
    memset(b + o, 0xab, 16), o += 16;
    put32(b + o, 9), o += 4;
    b[o++] = 1;
    b[o++] = 0;
    SgrRec rec = {16, SGR_EVENT, b, (u32)o};
    Script s;
    CHECK(script_parse(&s, &rec));
    CHECK(s.trigger == 1 && s.count == 2);
    CHECK(s.cmds[0].type == 44 && s.cmds[0].argCount == 3 && strcmp(cmd_str(&s.cmds[0], 0), "abc") == 0 && cmd_int(&s.cmds[0], 2) == 6);
    CHECK(s.cmds[1].type == 37 && s.cmds[1].argCount == 3 && cmd_int(&s.cmds[1], 0) == 7 && cmd_guid(&s.cmds[1], 1).b[0] == 0xab && cmd_int(&s.cmds[1], 2) == 9);
    CHECK(s.tail[0] == 1);
    script_free(&s);
}

static SG *open_game(const char *dir) {
    if (!dir) return NULL;
    VNSmileGameConfig cfg = {.gameDir = dir, .audioRate = 44100};
    char err[512];
    SG *g = (SG *)vnsmilegame_open(&cfg, err, sizeof err);
    if (!g) fprintf(stderr, "open %s: %s\n", dir, err);
    return g;
}

static const SgrRec *map_rec(SG *g, const char *id) { return game_record(&g->data, SGR_MAP, 0, guid(id), NULL); }

static void test_colette(const char *dir) {
    SG *g = open_game(dir);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(g->data.pack.count == 1898);
    CHECK(g->data.mapCount == 122);
    CHECK(g->data.packCount == 3);
    size_t size = 0;
    u8 *gs = pack_read(&g->data.pack, "GameSettings.sgr", &size);
    CHECK(gs && size == 5398);
    free(gs);
    CHECK(g->data.settingsFile.version == 6 && g->data.settingsFile.count == 1 && g->data.settingsFile.recs[0].size == 5375);
    CHECK(g->data.resFile.version == 10);
    CHECK(g->data.heroFile.count == 30);
    const Settings *s = &g->data.settings;
    CHECK(strcmp(s->name, "NewGame") == 0);
    CHECK(s->startX == 11 && s->startZ == 14);
    CHECK(guid_eq(s->startMap, guid("bd9a0e30-981c-4806-8b19-ae4ebce57a5b")));
    CHECK(s->switchCount == 71 && strcmp(s->switchNames[1], "kasin0") == 0);
    CHECK(s->varCount == 19 && strcmp(s->varNames[0], "プレイヤー進行度") == 0);
    CHECK(strcmp(s->titleItems[0], "はじめから") == 0 && strcmp(s->titleItems[1], "つづきから") == 0 && strcmp(s->titleItems[2], "コンフィグ") == 0);
    CHECK(s->titleLayout == 0 && s->screenW == 960 && s->screenH == 544);
    const Res *title = res_find(&g->res, s->titleImage);
    CHECK(title && strcmp(title->name, "ttl") == 0 && strcmp(title->path, "res\\image\\ttl.png") == 0);
    const Res *bgm = res_find(&g->res, s->titleBgm);
    CHECK(bgm && strcmp(bgm->path, "samples/dignified_fantasy_music_vol1/res\\bgm\\BGM_sky_DFM.ogg") == 0);
    Image *im = sg_image(g, s->titleImage);
    CHECK(im && im->w == 960 && im->h == 544);
    const Res *hero = NULL;
    for (int i = 0; i < g->res.count; i++)
        if (strcmp(g->res.items[i].name, "dot_cha019") == 0) hero = &g->res.items[i];
    CHECK(hero && hero->kind == RES_CHARACTER && hero->frames == 3 && hero->frameMs == 250 && hero->divisions == 1);
    Map m;
    const SgrRec *mr = map_rec(g, "94ae69b6-668a-437c-9b14-67c3cb1a6de4");
    CHECK(mr && map_parse(&m, mr));
    if (mr) {
        CHECK(m.w == 30 && m.h == 30 && m.eventCount == 15 && m.objectCount == 2 && m.stairCount == 3 && m.chipCount == 6);
        CHECK(m.events[0].x == 15 && m.events[0].z == 6);
        CHECK(m.objects[0].x == 14 && m.objects[0].z == 5 && guid_eq(m.objects[0].id, guid("50313359-a2b2-4aab-9ae2-961ff2a5a24d")));
        CHECK(m.height[4 * 30 + 7] == 6 && m.height[5 * 30 + 8] == 3 && m.height[13 * 30 + 7] == 2);
        const Res *chip = res_find(&g->res, map_chip_guid(&m, 8));
        CHECK(chip && strcmp(chip->name, "016_RedCarpet") == 0);
        CHECK(m.colors[1] == 214 && m.colors[2] == 202 && m.colors[3] == 188);
        Sgr *file = NULL;
        game_record(&g->data, SGR_MAP, 0, m.id, &file);
        int advisor = -1, chest = -1;
        EventDef ev[15];
        for (int i = 0; i < m.eventCount; i++) {
            memset(&ev[i], 0, sizeof ev[i]);
            for (int k = 0; k < file->count; k++)
                if (file->recs[k].type == SGR_EVENT && file->recs[k].sub == 0 && memcmp(file->recs[k].body, m.events[i].id.b, 16) == 0)
                    CHECK(event_parse(&ev[i], &file->recs[k]));
            if (m.events[i].x == 15 && m.events[i].z == 6) advisor = i;
            if (m.events[i].x == 13 && m.events[i].z == 5) chest = i;
        }
        CHECK(advisor >= 0 && ev[advisor].sheetCount == 20 && ev[advisor].sheets[0].condCount == 1);
        CHECK(advisor >= 0 && ev[advisor].sheets[0].conds[0].kind == 1 && ev[advisor].sheets[0].conds[0].index == 0 && ev[advisor].sheets[0].conds[0].value == 1);
        CHECK(chest >= 0 && ev[chest].sheetCount == 2 && ev[chest].sheets[0].conds[0].kind == 0 && ev[chest].sheets[0].conds[0].index == 58);
        const Res *talker = advisor >= 0 ? res_find(&g->res, ev[advisor].sheets[0].graphic) : NULL;
        CHECK(talker && strcmp(talker->name, "dot_cha012") == 0);
        const SgrRec *sr = advisor >= 0 ? game_record(&g->data, SGR_EVENT, 16, ev[advisor].sheets[0].script, NULL) : NULL;
        Script sc;
        CHECK(sr && script_parse(&sc, sr));
        if (sr) {
            CHECK(sc.trigger == 2 && sc.cmds[0].type == 43);
            CHECK(strncmp(cmd_str(&sc.cmds[0], 0), "副官：さぁ、新王様、クーデターは成功しました。", strlen("副官：さぁ、新王様、クーデターは成功しました。")) == 0);
            script_free(&sc);
        }
        for (int i = 0; i < m.eventCount; i++) event_free(&ev[i]);
        map_free(&m);
    }
    const SgrRec *intro = map_rec(g, "bd9a0e30-981c-4806-8b19-ae4ebce57a5b");
    CHECK(intro && map_parse(&m, intro));
    if (intro) {
        Sgr *file = NULL;
        game_record(&g->data, SGR_MAP, 0, m.id, &file);
        int scripts = 0;
        for (int k = 0; k < file->count; k++) {
            if (file->recs[k].type != SGR_EVENT || file->recs[k].sub != 16) continue;
            Script sc;
            CHECK(script_parse(&sc, &file->recs[k]));
            scripts++;
            CHECK(sc.trigger == 1 && sc.count == 6);
            static const int types[6] = {21, 51, 44, 60, 31, 20};
            for (int i = 0; i < 6 && i < sc.count; i++) CHECK(sc.cmds[i].type == types[i]);
            if (sc.count == 6) {
                CHECK(strncmp(cmd_str(&sc.cmds[2], 0), "剣と魔法の世界", strlen("剣と魔法の世界")) == 0);
                CHECK(guid_eq(cmd_guid(&sc.cmds[5], 0), guid("94ae69b6-668a-437c-9b14-67c3cb1a6de4")));
                CHECK(cmd_int(&sc.cmds[5], 1) == 14 && cmd_int(&sc.cmds[5], 2) == 6);
            }
            script_free(&sc);
        }
        CHECK(scripts == 1);
        map_free(&m);
    }
    int parsed = 0, total = 0;
    for (int i = 0; i < g->data.mapCount; i++)
        for (int k = 0; k < g->data.maps[i].count; k++) {
            const SgrRec *r = &g->data.maps[i].recs[k];
            if (r->type == SGR_MAP) {
                total++;
                Map mm;
                if (map_parse(&mm, r)) parsed++;
                map_free(&mm);
            } else if (r->type == SGR_EVENT && r->sub == 16) {
                total++;
                Script sc;
                if (script_parse(&sc, r)) parsed++;
                script_free(&sc);
            } else if (r->type == SGR_EVENT && r->sub == 0) {
                total++;
                EventDef e;
                if (event_parse(&e, r)) parsed++;
                event_free(&e);
            }
        }
    CHECK(parsed == total);
    Model *throne = model_load(g, "samples/common/res/mapobject/Indoor/Throne B.fbx");
    CHECK(throne && throne->mesh.count > 0);
    if (throne) {
        float minY = 1e9f, maxY = -1e9f;
        for (int i = 0; i < throne->mesh.count; i++)
            for (int k = 0; k < 3; k++) {
                if (throne->mesh.tris[i].v[k].y < minY) minY = throne->mesh.tris[i].v[k].y;
                if (throne->mesh.tris[i].v[k].y > maxY) maxY = throne->mesh.tris[i].v[k].y;
            }
        CHECK(minY > -0.01f && maxY > 1.5f && maxY < 1.6f);
        model_free(throne);
    }
    vnsmilegame_close((VNSmileGame *)g);
}

static void test_sacred(const char *dir) {
    SG *g = open_game(dir);
    CHECK(g != NULL);
    if (!g) return;
    CHECK(g->data.mapCount == 81);
    const Settings *s = &g->data.settings;
    CHECK(strcmp(s->name, "セイクリッド～砕かれた光の冠～") == 0);
    CHECK(s->startX == 14 && s->startZ == 11 && guid_eq(s->startMap, guid("30d66a54-95fd-4d0e-be08-8be2dc640e6a")));
    CHECK(s->switchCount == 52 && s->varCount == 4 && s->titleLayout == 1);
    const Res *overlay = res_find(&g->res, s->titleOverlay);
    CHECK(overlay && strcmp(overlay->name, "title01") == 0);
    const Res *bg = res_find(&g->res, s->titleImage);
    CHECK(bg && strcmp(bg->name, "title_castle") == 0);
    Map m;
    const SgrRec *mr = map_rec(g, "38016e57-fc5f-4187-9363-2c17f231f1b6");
    CHECK(mr && map_parse(&m, mr));
    if (mr) {
        CHECK(strcmp(m.name, "王家の洞窟") == 0 && m.encounterRate == 40 && m.w == 40 && m.h == 24 && m.eventCount == 8);
        map_free(&m);
    }
    mr = map_rec(g, "cad84106-d545-4789-97d1-a871aca902d3");
    CHECK(mr && map_parse(&m, mr));
    if (mr) {
        CHECK(m.w == 150 && m.h == 150 && m.chipCount == 14);
        map_free(&m);
    }
    int maps = 0, parsed = 0;
    for (int i = 0; i < g->data.mapCount; i++)
        for (int k = 0; k < g->data.maps[i].count; k++)
            if (g->data.maps[i].recs[k].type == SGR_MAP) {
                maps++;
                Map mm;
                if (map_parse(&mm, &g->data.maps[i].recs[k])) parsed++;
                map_free(&mm);
            }
    CHECK(maps == 81 && parsed == 81);
    vnsmilegame_close((VNSmileGame *)g);
}

int main(void) {
    test_pack_synthetic();
    test_script_synthetic();
    const char *colette = getenv("SG_COLETTE"), *sacred = getenv("SG_SACRED");
    if (colette) test_colette(colette);
    else printf("SG_COLETTE not set: Colette checks skipped\n");
    if (sacred) test_sacred(sacred);
    else printf("SG_SACRED not set: Sacred checks skipped\n");
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
