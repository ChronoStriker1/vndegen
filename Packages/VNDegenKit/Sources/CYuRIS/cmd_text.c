// Text output and the TEXT command.
//
// ERIS runs the engine in manual output mode (TEXT[OUTPUTMODE=0]) and draws text itself. Under the original engine
// (probe scripts run under Wine): WORD and _ append their characters to $_TEXT and a flag per character to
// $_TEXT_FLAG, '1' half-width and '2' full-width, without resting the task; RETURNCODE, at the end of a line of text,
// adds 'E' to the flags and rests the task; when that task runs again, both are emptied.
#include "text.h"

int yr_cmd_font(YCall *c, int id);

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
