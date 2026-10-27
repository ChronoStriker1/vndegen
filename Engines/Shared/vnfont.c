#include "vnfont.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// Normalized names are at most a few times the length of the table's longest name.
enum { key_max = 256 };

struct line {
    char key[key_max];
    vnfont_entry entry;
};

static struct line *lines;
static int line_count;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

// Decodes one UTF-8 sequence at `s`, setting `*length`. Malformed bytes decode as themselves, one at a time, so a
// name in another encoding still compares byte for byte.
static unsigned decode(const unsigned char *s, int *length) {
    unsigned c = s[0];
    int extra = c >= 0xF0 && c < 0xF8 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if ((c >= 0x80 && c < 0xC0) || c >= 0xF8) extra = 0;
    unsigned value = extra == 3 ? c & 0x07 : extra == 2 ? c & 0x0F : extra == 1 ? c & 0x1F : c;
    for (int i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *length = 1;
            return c;
        }
        value = value << 6 | (s[i] & 0x3F);
    }
    *length = extra + 1;
    return value;
}

int vnfont_normalize(const char *name, char *out, size_t size) {
    const unsigned char *s = (const unsigned char *)name;
    size_t n = 0;
    if (*s == '@') s++;
    while (*s) {
        int length;
        unsigned c = decode(s, &length);
        if (c >= 0xFF01 && c <= 0xFF5E) c -= 0xFEE0;
        if (c == ' ' || c == '\t' || c == 0x3000) {
            s += length;
            continue;
        }
        if (c < 0x80) {
            if (n + 1 >= size) return 0;
            out[n++] = (char)(c >= 'A' && c <= 'Z' ? c + 32 : c);
        } else {
            if (n + (size_t)length >= size) return 0;
            memcpy(out + n, s, (size_t)length);
            n += (size_t)length;
        }
        s += length;
    }
    out[n] = 0;
    return 1;
}

static int is_file(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

// Parses one table line into `line`, finding its file in `dirs`. Returns 0 for comments, blank or malformed lines, and
// lines whose file is in no folder.
static int parse(char *text, const char *const *dirs, int dir_count, struct line *line) {
    text[strcspn(text, "\r\n")] = 0;
    if (text[0] == '#' || text[0] == 0) return 0;
    char *fields[4];
    for (int i = 0; i < 4; i++) {
        fields[i] = text;
        char *tab = strchr(text, '\t');
        if (i < 3) {
            if (!tab) return 0;
            *tab = 0;
            text = tab + 1;
        } else if (tab) {
            *tab = 0;
        }
    }
    const char *name = fields[0], *style = fields[1], *file = fields[2];
    if (strlen(name) >= sizeof line->entry.name || strchr(file, '/') || !vnfont_normalize(name, line->key, sizeof line->key)) return 0;
    memset(&line->entry, 0, sizeof line->entry);
    strcpy(line->entry.name, name);
    vnfont_match *match = &line->entry.match;
    if (!strcmp(style, "bold")) {
        match->bold = 1;
    } else if (!strcmp(style, "italic")) {
        match->italic = 1;
    } else if (!strcmp(style, "bold-italic")) {
        match->bold = match->italic = 1;
    } else if (strcmp(style, "regular")) {
        return 0;
    }
    match->face = atoi(fields[3]);
    for (int i = 0; i < dir_count; i++) {
        int written = snprintf(match->path, sizeof match->path, "%s/%s", dirs[i], file);
        if (written > 0 && (size_t)written < sizeof match->path && is_file(match->path)) return 1;
    }
    return 0;
}

int vnfont_init(const char *table_path, const char *const *font_dirs, int dir_count) {
    struct line *loaded = NULL;
    int count = 0, capacity = 0, status = -1;
    FILE *table = fopen(table_path, "r");
    if (table) {
        char text[2048];
        struct line line;
        status = 0;
        while (fgets(text, sizeof text, table)) {
            if (!parse(text, font_dirs, dir_count, &line)) continue;
            if (count == capacity) {
                capacity = capacity ? capacity * 2 : 64;
                struct line *grown = realloc(loaded, (size_t)capacity * sizeof *loaded);
                if (!grown) {
                    status = -1;
                    break;
                }
                loaded = grown;
            }
            loaded[count++] = line;
        }
        fclose(table);
    }
    if (status != 0) {
        free(loaded);
        loaded = NULL;
        count = 0;
    }
    pthread_mutex_lock(&lock);
    struct line *old = lines;
    lines = loaded;
    line_count = count;
    pthread_mutex_unlock(&lock);
    free(old);
    return status;
}

int vnfont_resolve(const char *name, int bold, int italic, vnfont_match *match) {
    char key[key_max];
    if (!name || !vnfont_normalize(name, key, sizeof key)) return 0;
    bold = !!bold;
    italic = !!italic;
    pthread_mutex_lock(&lock);
    // The best line has the most of the asked-for styles and none that weren't asked for, bold counting more than
    // italic, since a synthesized bold changes widths and a slant doesn't.
    const struct line *best = NULL;
    int best_score = -1;
    for (int i = 0; i < line_count; i++) {
        const vnfont_match *m = &lines[i].entry.match;
        if (strcmp(lines[i].key, key) || m->bold > bold || m->italic > italic) continue;
        int score = m->bold * 2 + m->italic;
        if (score > best_score) {
            best = &lines[i];
            best_score = score;
        }
    }
    if (best) {
        *match = best->entry.match;
        match->embolden = bold && !match->bold;
        match->slant = italic && !match->italic;
    }
    pthread_mutex_unlock(&lock);
    return best != NULL;
}

int vnfont_count(void) {
    pthread_mutex_lock(&lock);
    int count = line_count;
    pthread_mutex_unlock(&lock);
    return count;
}

int vnfont_entry_at(int index, vnfont_entry *entry) {
    pthread_mutex_lock(&lock);
    int found = index >= 0 && index < line_count;
    if (found) *entry = lines[index].entry;
    pthread_mutex_unlock(&lock);
    return found;
}
