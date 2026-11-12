// Text output and the TEXT command.
//
// ERIS runs the engine in manual output mode (TEXT[OUTPUTMODE=0]) and draws text itself. Under the original engine
// (probe scripts run under Wine): WORD and _ append their characters to $_TEXT and a flag per character to
// $_TEXT_FLAG, '1' half-width and '2' full-width, without resting the task; RETURNCODE, at the end of a line of text,
// adds 'E' to the flags and rests the task; when that task runs again, both are emptied.
#include "text.h"

int yr_cmd_font(YCall *c, int id);

static bool valid_utf8(const u8 *p, size_t n) {
    for (size_t i = 0; i < n;) {
        u8 b = p[i];
        size_t k = b < 0x80 ? 1 : (b & 0xe0) == 0xc0 ? 2 : (b & 0xf0) == 0xe0 ? 3 : (b & 0xf8) == 0xf0 ? 4 : 0;
        if (!k || i + k > n) return false;
        for (size_t j = 1; j < k; j++) if ((p[i + j] & 0xc0) != 0x80) return false;
        i += k;
    }
    return true;
}

static void load_lines(YCall *c, const char *name) {
    YR *y = c->y;
    YRef out;
    if (!yc_ref(c, "LET", &out) || !out.var || out.var->t != YT_STR) return;
    YVar *v = out.var;
    long at = out.index < 0 ? 0 : out.index;
    YFile f;
    if (yr_fs_read(y, name, NULL, &f)) {
        const u8 *p = f.data, *end = f.data + f.size;
        if (f.size >= 3 && p[0] == 0xef && p[1] == 0xbb && p[2] == 0xbf) p += 3;
        bool utf8 = y->enc != YR_UTF8 && valid_utf8(p, (size_t)(end - p));
        while (p < end && at < (long)v->count) {
            const u8 *nl = memchr(p, '\n', (size_t)(end - p));
            const u8 *stop = nl ? nl : end;
            char *line = yr_strndup((const char *)p, (size_t)(stop - p));
            free(v->strs[at]);
            v->strs[at++] = utf8 ? yr_from_utf8(y->enc, line) : line;
            if (utf8) free(line);
            p = nl ? nl + 1 : end;
        }
        yr_file_free(&f);
    } else {
        yr_log(y, "TEXT: file not found: %s\n", name);
    }
    for (long i = at; i < (long)v->count; i++) {
        free(v->strs[i]);
        v->strs[i] = NULL;
    }
}

static void append(YR *y, const char *s) {
    size_t n = strlen(s), tn = y->text ? strlen(y->text) : 0, fn = y->textFlag ? strlen(y->textFlag) : 0;
    y->text = yr_realloc(y->text, tn + n + 1);
    memcpy(y->text + tn, s, n + 1);
    size_t chars = 0;
    for (const u8 *p = (const u8 *)s; *p; p += yr_char_len(y->enc, p)) chars++;
    y->textFlag = yr_realloc(y->textFlag, fn + chars + 1);
    char *f = y->textFlag + fn;
    for (const u8 *p = (const u8 *)s; *p;) {
        u32 cp;
        int len = yr_char_decode(y->enc, p, &cp);
        bool full = y->enc == YR_SJIS ? len == 2 : (cp >= 0x1100 && !(cp >= 0xff61 && cp <= 0xff9f));
        *f++ = full ? '2' : '1';
        p += len;
    }
    *f = 0;
}

int yr_cmd_text(YCall *c, int id) {
    YR *y = c->y;
    switch (id) {
    case C_WORD:
        if (c->in->argc) {
            const YArg *a = &c->s->args[c->in->arg];
            char *s = yr_strndup((const char *)a->data, a->size);
            append(y, s);
            free(s);
        }
        return YR_NEXT;
    case C_TEXTOUT:
        if (c->in->argc) {
            const YArg *a = &c->s->args[c->in->arg];
            YVal v = yr_eval(y, c->t, a->data, a->size);
            char *s = yv_as_str(&v);
            append(y, s);
            free(s);
            yv_free(&v);
        }
        return YR_NEXT;
    case C_RETURNCODE:
        append(y, "");
        {
            size_t fn = y->textFlag ? strlen(y->textFlag) : 0;
            y->textFlag = yr_realloc(y->textFlag, fn + 2);
            y->textFlag[fn] = 'E';
            y->textFlag[fn + 1] = 0;
        }
        c->t->clearText = true;
        return YR_YIELD;
    case C_TEXT: {
        char *file = yc_str(c, "FILE");
        if (file) {
            load_lines(c, file);
            free(file);
            return YR_NEXT;
        }
        i64 v;
        if (yc_int(c, "OUTPUTMODE", &v)) y->txt->outputManual = v == 0;
        if (yc_int_or(c, "CLEAR", 0)) {
            free(y->text);
            free(y->textFlag);
            y->text = y->textFlag = NULL;
        }
        if (yc_int(c, "WAIT", &v)) y->txt->wait = (int)v;
        return YR_NEXT;
    }
    case C_TEXTINFO: {
        YRef out;
        if (!yc_ref(c, "LET", &out)) return YR_NEXT;
        YVal r = yv_int(0);
        if (yc_int_or(c, "OUTPUTMODE", 0)) r = yv_int(!y->txt->outputManual);
        else if (!yc_int_or(c, "CLICK", 0)) yc_unknown(c, "TEXTINFO form");
        yr_ref_set(y, c->t, &out, &r);
        return YR_NEXT;
    }
    case C_FONT: case C_FONTINFO:
        return yr_cmd_font(c, id);
    }
    return YR_NEXT;
}
