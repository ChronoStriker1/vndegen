#include "br.h"

static void unit_build(Unit *u, Node *def) {
    Node *head = def->n > 1 ? def->k[1] : NULL;
    u->id = head && head->n > 0 ? sx_int(head->k[0], 0) : 0;
    u->locals = head && head->n > 1 ? sx_int(head->k[1], 0) : 0;
    u->count = def->n > 2 ? def->n - 2 : 0;
    u->st = def->k + 2;
    for (int i = 0; i < u->count; i++)
        if (u->st[i]->op == A_LABEL) u->labels++;
    u->labelId = br_alloc(sizeof(int) * (size_t)(u->labels + 1));
    u->labelPc = br_alloc(sizeof(int) * (size_t)(u->labels + 1));
    int k = 0;
    for (int i = 0; i < u->count; i++)
        if (u->st[i]->op == A_LABEL) {
            u->labelId[k] = u->st[i]->n > 1 ? sx_int(u->st[i]->k[1], -1) : -1;
            u->labelPc[k++] = i;
        }
}

static void unit_free(Unit *u) {
    free(u->labelId);
    free(u->labelPc);
}

int unit_label(Unit *u, int id) {
    for (int i = 0; i < u->labels; i++)
        if (u->labelId[i] == id) return u->labelPc[i];
    return -1;
}

static Script *script_load(BR *e, int num) {
    char rel[256];
    snprintf(rel, sizeof rel, "%s/%s%0*d.%s", e->cfg.scriptDir, e->cfg.scriptBase, e->cfg.scriptDigits, num, e->cfg.scriptExt);
    size_t n;
    u8 *d = br_read_game(e, rel, &n);
    if (!d) return NULL;
    Script *sc = br_alloc(sizeof *sc);
    sc->num = num;
    sc->root = sx_parse((const char *)d, n);
    free(d);
    int subs = 0;
    for (int i = 0; i < sc->root->n; i++)
        if (sc->root->k[i]->op == A_DEFINE_SUB) subs++;
    sc->subs = br_alloc(sizeof(Unit) * (size_t)(subs + 1));
    for (int i = 0; i < sc->root->n; i++) {
        Node *def = sc->root->k[i];
        if (def->op == A_DEFINE_SET) unit_build(&sc->set, def);
        else if (def->op == A_DEFINE_SUB) unit_build(&sc->subs[sc->subCount++], def);
    }
    return sc;
}

Script *script_get(BR *e, int num) {
    if (num < 0 || num >= BR_SCRIPTS) return NULL;
    if (!e->scripts[num] && !e->scriptMissing[num]) {
        e->scripts[num] = script_load(e, num);
        if (!e->scripts[num]) {
            e->scriptMissing[num] = true;
            br_log(e, "script %d is missing", num);
        }
    }
    return e->scripts[num];
}

Unit *script_sub(Script *sc, int id) {
    if (!sc) return NULL;
    for (int i = 0; i < sc->subCount; i++)
        if (sc->subs[i].id == id) return &sc->subs[i];
    return NULL;
}

void script_free_all(BR *e) {
    for (int i = 0; i < BR_SCRIPTS; i++) {
        Script *sc = e->scripts[i];
        if (!sc) continue;
        unit_free(&sc->set);
        for (int k = 0; k < sc->subCount; k++) unit_free(&sc->subs[k]);
        free(sc->subs);
        sx_free(sc->root);
        free(sc);
        e->scripts[i] = NULL;
    }
}
