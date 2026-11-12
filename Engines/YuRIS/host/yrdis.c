#include "yr.h"
#include "vm.h"
#include "vnfont.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *cat(const char *a, const char *b, const char *c) {
    size_t n = strlen(a) + strlen(b) + strlen(c) + 1;
    char *r = malloc(n);
    snprintf(r, n, "%s%s%s", a, b, c);
    return r;
}

static char *var_name(YR *y, u8 sigil, u32 vid) {
    char buf[64];
    YVar *v = yr_var(y, vid);
    char s = sigil ? (char)sigil : v && v->t == YT_STR ? '$' : '@';
    snprintf(buf, sizeof buf, "%c%s%u", s, vid < 1000 ? "S" : "v", vid);
    return strdup(buf);
}

static char *expr(YR *y, const u8 *p, u32 n) {
    char *st[256];
    int sn = 0;
    int marks[32], nm = 0;
    char *markName[32];
    u32 o = 0;
    while (o + 3 <= n) {
        u8 op = p[o];
        u32 len = yr_le16(p + o + 1);
        const u8 *d = p + o + 3;
        if (o + 3 + len > n) break;
        o += 3 + len;
        char buf[128];
        char *t = NULL;
        switch (op) {
        case 'B': snprintf(buf, sizeof buf, "%d", (int8_t)d[0]); t = strdup(buf); break;
        case 'W': snprintf(buf, sizeof buf, "%d", (int16_t)yr_le16(d)); t = strdup(buf); break;
        case 'I': snprintf(buf, sizeof buf, "%d", (int32_t)yr_le32(d)); t = strdup(buf); break;
        case 'L': snprintf(buf, sizeof buf, "%lld", (long long)yr_le64(d)); t = strdup(buf); break;
        case 'F': { double f; u64 b = yr_le64(d); memcpy(&f, &b, 8); snprintf(buf, sizeof buf, "%g", f); t = strdup(buf); break; }
        case 'M': { char *raw = yr_strndup((const char *)d, len); t = yr_to_utf8(y->enc, raw); free(raw); break; }
        case 'H': t = var_name(y, d[0], yr_le16(d + 1)); break;
        case 'V': case 'v':
            if (nm < 32) { marks[nm] = sn; markName[nm++] = var_name(y, d[0], yr_le16(d + 1)); }
            break;
        case ',': break;
        case ')':
            if (nm) {
                int base = marks[--nm];
                char *acc = cat(markName[nm], "(", "");
                free(markName[nm]);
                for (int i = base; i < sn; i++) {
                    char *x = cat(acc, st[i], i + 1 < sn ? "," : "");
                    free(acc); free(st[i]); acc = x;
                }
                sn = base;
                t = cat(acc, ")", "");
                free(acc);
            }
            break;
        case 'R': case 's': case 'i': {
            char *a = sn ? st[--sn] : strdup("?");
            const char *pre = op == 'R' ? "-(" : op == 's' ? "str(" : "int(";
            t = cat(pre, a, ")");
            free(a);
            break;
        }
        default: {
            static const char *ops = "+-*/%=!<>SZ&|AO^";
            if (strchr(ops, op)) {
                char *b = sn ? st[--sn] : strdup("?"), *a = sn ? st[--sn] : strdup("?");
                const char *sym = op == 'S' ? "<=" : op == 'Z' ? ">=" : op == '=' ? "==" : op == '!' ? "!=" :
                                  op == '&' ? "&&" : op == '|' ? "||" : op == 'A' ? "&" : op == 'O' ? "|" : NULL;
                char opbuf[4] = { (char)op, 0 };
                char *l = cat("(", a, " "), *r = cat(" ", b, ")");
                char *m = cat(l, sym ? sym : opbuf, r);
                free(l); free(r); free(a); free(b);
                t = m;
            } else {
                snprintf(buf, sizeof buf, "<%02x:%u>", op, len);
                t = strdup(buf);
            }
        }
        }
        if (t && sn < 256) st[sn++] = t;
        else free(t);
    }
    while (nm) free(markName[--nm]);
    char *r = strdup("");
    for (int i = 0; i < sn; i++) {
        char *x = cat(r, i ? " ; " : "", st[i]);
        free(r); free(st[i]); r = x;
    }
    return r;
}

static void dump_script(YR *y, u32 si, u32 from, u32 to) {
    YScript *s = yr_script(y, (u16)si);
    if (!s) return;
    char *nm = yr_to_utf8(y->enc, s->name);
    printf("== script %u %s (%u instructions)\n", si, nm, s->count);
    free(nm);
    for (u32 i = from; i < s->count && i <= to; i++) {
        for (u32 l = 0; l < y->labelCount; l++)
            if (y->labels[l].script == si && y->labels[l].ip == i) {
                char *u = yr_to_utf8(y->enc, y->labels[l].name);
                printf("#%s:\n", u);
                free(u);
            }
        YIns *in = &s->ins[i];
        printf("%5u L%-5u %s[", i, in->line, y->cmds[in->cmd].name);
        for (int k = 0; k < in->argc; k++) {
            YArg *a = &s->args[in->arg + k];
            if (!(a->type & 0xff) && !getenv("YRDIS_RAW")) continue;
            const char *key = yr_arg_key(y, in, a);
            char *e;
            if (y->cmds[in->cmd].id == C_WORD || y->cmds[in->cmd].id == C_TEXTOUT) {
                char *raw = yr_strndup((const char *)a->data, a->size);
                e = yr_to_utf8(y->enc, raw);
                free(raw);
            } else e = expr(y, a->data, a->size);
            const char *opn[] = { "=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=" };
            int op = a->type >> 8;
            if (getenv("YRDIS_RAW")) printf("%s{k%u t%04x n%u o%ld}", k ? " " : "", a->key, a->type, a->size, (long)(a->data - (s->args[0].data)));
            printf("%s%s%s%s", k ? " " : "", key, op < 9 ? opn[op] : "?=", e);
            free(e);
        }
        printf("]\n");
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: yrdis <gameDir> [script [from to]] | --labels | --vars | --file NAME OUT\n"); return 2; }
    VNYConfig cfg = { 0 };
    cfg.gameDir = argv[1];
    char err[512];
    YR *y = yr_alloc(sizeof(YR));
    y->gameDir = yr_strdup(cfg.gameDir);
    y->saveDir = yr_strdup(cfg.gameDir);
    if (!yr_fs_open(y) || !yr_script_open(y)) { fprintf(stderr, "open failed: %s\n", y->problem); return 1; }
    (void)err;
    if (argc >= 3 && !strcmp(argv[2], "--labels")) {
        for (u32 l = 0; l < y->labelCount; l++) {
            char *u = yr_to_utf8(y->enc, y->labels[l].name);
            printf("%s %u %u\n", u, y->labels[l].script, y->labels[l].ip);
            free(u);
        }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[2], "--list")) {
        for (u32 i = 0; i < y->scriptCount; i++) {
            char *u = yr_to_utf8(y->enc, y->scripts[i].name);
            printf("%u %s\n", i, u);
            free(u);
        }
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[2], "--cmds")) {
        for (u32 i = 0; i < y->cmdCount; i++) {
            printf("%u %s:", i, y->cmds[i].name);
            for (int k = 0; k < y->cmds[i].keyCount; k++) printf(" %s", y->cmds[i].keys[k]);
            printf("\n");
        }
        return 0;
    }
    if (argc >= 5 && !strcmp(argv[2], "--file")) {
        YFile f;
        if (!yr_fs_read(y, argv[3], NULL, &f)) { fprintf(stderr, "missing %s\n", argv[3]); return 1; }
        yr_write_file(argv[4], f.data, f.size);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[2], "--vars")) {
        for (u32 i = 0; i < y->varCount; i++) {
            YVar *v = &y->vars[i];
            if (!v->init.t && !v->t) continue;
            printf("%u scope=%u script=%u t=%u nd=%u", i, v->scope, v->script, v->t, v->initNdim);
            for (int d = 0; d < v->initNdim; d++) printf(" %u", v->initDims[d]);
            if (v->initExpr) { char *e = expr(y, v->initExpr, v->initExprSize); printf(" init=%s", e); free(e); }
            else if (v->init.t == YT_INT) printf(" init=%lld", (long long)v->init.i);
            else if (v->init.t == YT_FLT) printf(" init=%g", v->init.f);
            printf("\n");
        }
        return 0;
    }
    if (argc >= 3) {
        u32 si = (u32)atoi(argv[2]);
        u32 from = argc >= 4 ? (u32)atoi(argv[3]) : 0, to = argc >= 5 ? (u32)atoi(argv[4]) : 0xffffffffu;
        dump_script(y, si, from, to);
        return 0;
    }
    for (u32 i = 0; i < y->scriptCount; i++) dump_script(y, i, 0, 0xffffffffu);
    return 0;
}
