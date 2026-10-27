// Text through stb_truetype, the same on every platform. Fonts come from the folders registered for the game (its
// Fonts/ folder and the RTP's), then from VNDegen's shared fonts, found by family name the way Windows finds them.
#include "rgss.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "../third_party/stb_truetype.h"

typedef struct {
    char *path;
    uint8_t *data;    // loaded on first use
    size_t size;
    int index;        // face in a collection
    char **names;     // family and full names, folded to lower case ASCII where ASCII
    int nameCount;
    bool bold, italic;
    bool loaded, broken;
    stbtt_fontinfo info;
} Face;

static Face *faces;
static int faceCount, faceCapacity;
// Faces from folders registered for the current game, which go away with it. Shared faces stay.
static int sharedFaceCount = -1;

// MARK: Names

static void fold_name(char *s) {
    for (; *s; s++) if (*s >= 'A' && *s <= 'Z') *s = (char)(*s + 32);
}

// UTF-16BE from the name table to UTF-8.
static char *utf16be_to_utf8(const uint8_t *in, int bytes) {
    char *out = malloc((size_t)bytes * 2 + 1), *o = out;
    if (!out) return NULL;
    for (int i = 0; i + 1 < bytes; i += 2) {
        uint32_t c = (uint32_t)in[i] << 8 | in[i + 1];
        if (c >= 0xD800 && c < 0xDC00 && i + 3 < bytes) {
            uint32_t low = (uint32_t)in[i + 2] << 8 | in[i + 3];
            c = 0x10000 + ((c - 0xD800) << 10) + (low - 0xDC00);
            i += 2;
        }
        if (c < 0x80) *o++ = (char)c;
        else if (c < 0x800) { *o++ = (char)(0xC0 | c >> 6); *o++ = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { *o++ = (char)(0xE0 | c >> 12); *o++ = (char)(0x80 | (c >> 6 & 63)); *o++ = (char)(0x80 | (c & 63)); }
        else { *o++ = (char)(0xF0 | c >> 18); *o++ = (char)(0x80 | (c >> 12 & 63)); *o++ = (char)(0x80 | (c >> 6 & 63)); *o++ = (char)(0x80 | (c & 63)); }
    }
    *o = 0;
    return out;
}

static void add_name(Face *face, char *name) {
    if (!name || !*name) {
        free(name);
        return;
    }
    fold_name(name);
    for (int i = 0; i < face->nameCount; i++) {
        if (!strcmp(face->names[i], name)) {
            free(name);
            return;
        }
    }
    char **grown = realloc(face->names, sizeof(char *) * (size_t)(face->nameCount + 1));
    if (!grown) {
        free(name);
        return;
    }
    face->names = grown;
    face->names[face->nameCount++] = name;
}

// Reads every family (1, 16) and full name (4) the face lists, in every language, Windows and Mac platforms.
static void read_names(Face *face, const stbtt_fontinfo *info) {
    uint8_t *data = info->data;
    uint32_t nameTable = stbtt__find_table(data, (uint32_t)info->fontstart, "name");
    if (!nameTable) return;
    int count = ttUSHORT(data + nameTable + 2);
    int stringOffset = nameTable + ttUSHORT(data + nameTable + 4);
    for (int i = 0; i < count; i++) {
        uint8_t *record = data + nameTable + 6 + 12 * i;
        int platform = ttUSHORT(record), nameID = ttUSHORT(record + 6);
        int length = ttUSHORT(record + 8), offset = ttUSHORT(record + 10);
        if (nameID != 1 && nameID != 4 && nameID != 16) continue;
        uint8_t *string = data + stringOffset + offset;
        if (platform == 3 || platform == 0) add_name(face, utf16be_to_utf8(string, length));
        else if (platform == 1) {
            char *copy = malloc((size_t)length + 1);
            if (!copy) continue;
            memcpy(copy, string, (size_t)length);
            copy[length] = 0;
            add_name(face, copy);
        }
    }
    // The style from the OS/2 table's selection flags, else from the head table's mac style.
    uint32_t os2 = stbtt__find_table(data, (uint32_t)info->fontstart, "OS/2");
    if (os2) {
        int selection = ttUSHORT(data + os2 + 62);
        face->bold = selection & 32;
        face->italic = selection & 1;
    }
}

static bool load_file(const char *path, uint8_t **data, size_t *size) {
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    *data = malloc((size_t)length);
    bool ok = *data && fread(*data, 1, (size_t)length, file) == (size_t)length;
    fclose(file);
    if (!ok) {
        free(*data);
        return false;
    }
    *size = (size_t)length;
    return true;
}

static void add_font_file(const char *path) {
    uint8_t *data;
    size_t size;
    if (!load_file(path, &data, &size)) return;
    int count = stbtt_GetNumberOfFonts(data);
    for (int i = 0; i < count; i++) {
        stbtt_fontinfo info;
        int offset = stbtt_GetFontOffsetForIndex(data, i);
        if (offset < 0 || !stbtt_InitFont(&info, data, offset)) continue;
        if (faceCount == faceCapacity) {
            faceCapacity = faceCapacity ? faceCapacity * 2 : 32;
            Face *grown = realloc(faces, sizeof(Face) * (size_t)faceCapacity);
            if (!grown) break;
            faces = grown;
        }
        Face *face = &faces[faceCount++];
        memset(face, 0, sizeof *face);
        face->path = strdup(path);
        face->index = i;
        read_names(face, &info);
    }
    // Names are read; the data loads again on first use, so idle fonts take no memory.
    free(data);
}

static bool has_font_extension(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return false;
    char ext[8];
    snprintf(ext, sizeof ext, "%s", dot);
    fold_name(ext);
    return !strcmp(ext, ".ttf") || !strcmp(ext, ".ttc") || !strcmp(ext, ".otf");
}

void text_add_font_folder(const char *path) {
    DIR *dir = opendir(path);
    if (!dir) return;
    for (struct dirent *entry; (entry = readdir(dir));) {
        if (entry->d_name[0] == '.' || !has_font_extension(entry->d_name)) continue;
        char file[2048];
        snprintf(file, sizeof file, "%s/%s", path, entry->d_name);
        add_font_file(file);
    }
    closedir(dir);
}

static void free_face(Face *face) {
    free(face->path);
    free(face->data);
    for (int i = 0; i < face->nameCount; i++) free(face->names[i]);
    free(face->names);
}

// The fonts that go with every game: VNDegen's shared fonts, and on the Linux test host the system's.
static void add_shared_fonts(void) {
    const char *shared = getenv("VNRGSS_FONT_DIR");
    if (shared) text_add_font_folder(shared);
#ifdef __linux__
    const char *system[] = {"/usr/share/fonts/TTF", "/usr/share/fonts/OTF", "/usr/share/fonts/noto-cjk",
                            "/usr/share/fonts/noto", "/usr/share/fonts/liberation", "/usr/share/fonts/vlgothic",
                            "/usr/share/fonts/adobe-source-han-sans", NULL};
    for (int i = 0; system[i]; i++) text_add_font_folder(system[i]);
#endif
}

void text_reset(void) {
    if (sharedFaceCount < 0) {
        add_shared_fonts();
        sharedFaceCount = faceCount;
        return;
    }
    for (int i = sharedFaceCount; i < faceCount; i++) free_face(&faces[i]);
    faceCount = sharedFaceCount;
}

// MARK: Lookup

// Windows font names the shared fonts stand in for, until VNDegen's shared substitution table takes over.
static const char *const substitutes[][2] = {
    {"ms gothic", "ume gothic"}, {"ms pgothic", "ume pgothic"}, {"ms ui gothic", "ume ui gothic"},
    {"ｍｓ ゴシック", "ume gothic"}, {"ｍｓ ｐゴシック", "ume pgothic"}, {"ms mincho", "ume mincho"},
    {"ms pmincho", "ume pmincho"}, {"ｍｓ 明朝", "ume mincho"}, {"ｍｓ ｐ明朝", "ume pmincho"},
    {"arial", "liberation sans"}, {"times new roman", "liberation serif"}, {"courier new", "liberation mono"},
    {"verdana", "dejavu sans"}, {"tahoma", "dejavu sans"}, {"meiryo", "noto sans cjk jp"},
    {"メイリオ", "noto sans cjk jp"}, {"ms gothic", "noto sans cjk jp"}, {"ms pgothic", "noto sans cjk jp"},
    {"ｍｓ ゴシック", "noto sans cjk jp"}, {"ｍｓ ｐゴシック", "noto sans cjk jp"},
};

// The fallback for text no face the game named can draw.
static const char *const fallbacks[] = {"vl gothic", "ume pgothic", "noto sans cjk jp", "source han sans jp",
                                        "liberation sans", "dejavu sans", NULL};

static Face *face_named(const char *name, bool bold, bool italic) {
    char folded[256];
    snprintf(folded, sizeof folded, "%s", name);
    fold_name(folded);
    Face *best = NULL;
    int bestScore = -1;
    for (int i = 0; i < faceCount; i++) {
        Face *face = &faces[i];
        if (face->broken) continue;
        for (int j = 0; j < face->nameCount; j++) {
            if (strcmp(face->names[j], folded)) continue;
            int score = (face->bold == bold) + (face->italic == italic) + (i >= sharedFaceCount ? 4 : 0);
            if (score > bestScore) {
                best = face;
                bestScore = score;
            }
            break;
        }
    }
    return best;
}

static Face *lookup(const char *name, bool bold, bool italic) {
    Face *face = face_named(name, bold, italic);
    if (face) return face;
    char folded[256];
    snprintf(folded, sizeof folded, "%s", name);
    fold_name(folded);
    for (size_t i = 0; i < sizeof substitutes / sizeof substitutes[0]; i++) {
        if (!strcmp(substitutes[i][0], folded) && (face = face_named(substitutes[i][1], bold, italic))) return face;
    }
    return NULL;
}

bool text_font_exists(const char *name) { return lookup(name, false, false) != NULL; }

static bool load_face(Face *face) {
    if (face->loaded) return true;
    if (face->broken) return false;
    if (!load_file(face->path, &face->data, &face->size)) {
        face->broken = true;
        return false;
    }
    int offset = stbtt_GetFontOffsetForIndex(face->data, face->index);
    if (offset < 0 || !stbtt_InitFont(&face->info, face->data, offset)) {
        face->broken = true;
        return false;
    }
    face->loaded = true;
    return true;
}

static Face *primary_face(const FontRequest *font) {
    for (int i = 0; i < font->nameCount; i++) {
        Face *face = lookup(font->names[i], font->bold, font->italic);
        if (face && load_face(face)) return face;
    }
    for (int i = 0; fallbacks[i]; i++) {
        Face *face = lookup(fallbacks[i], font->bold, font->italic);
        if (face && load_face(face)) return face;
    }
    for (int i = 0; i < faceCount; i++) if (load_face(&faces[i])) return &faces[i];
    return NULL;
}

// The face that draws a character: the primary one, else the first fallback that has it.
static Face *face_for(Face *primary, uint32_t c) {
    if (!primary || stbtt_FindGlyphIndex(&primary->info, (int)c)) return primary;
    for (int i = 0; fallbacks[i]; i++) {
        Face *face = lookup(fallbacks[i], false, false);
        if (face && load_face(face) && stbtt_FindGlyphIndex(&face->info, (int)c)) return face;
    }
    return primary;
}

static uint32_t next_char(const char **s) {
    const uint8_t *p = (const uint8_t *)*s;
    uint32_t c = *p++;
    if (c >= 0xF0 && p[0] && p[1] && p[2]) { c = (c & 7) << 18 | (p[0] & 63u) << 12 | (p[1] & 63u) << 6 | (p[2] & 63u); p += 3; }
    else if (c >= 0xE0 && p[0] && p[1]) { c = (c & 15) << 12 | (p[0] & 63u) << 6 | (p[1] & 63u); p += 2; }
    else if (c >= 0xC0 && p[0]) { c = (c & 31) << 6 | (p[0] & 63u); p += 1; }
    *s = (const char *)p;
    return c;
}

// MARK: Layout

typedef struct {
    float scale;
    int ascent, descent, lineHeight;
} Metrics;

// RGSS's font size is the character height in pixels, ascent plus descent, as Windows' CreateFont takes a positive
// height. docs/rgss-cleanroom.md records the check against RGSS under Wine.
static Metrics metrics(Face *face, int size) {
    Metrics m;
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &gap);
    int winAscent = 0, winDescent = 0;
    uint32_t os2 = stbtt__find_table(face->info.data, (uint32_t)face->info.fontstart, "OS/2");
    if (os2) {
        winAscent = ttUSHORT(face->info.data + os2 + 74);
        winDescent = ttUSHORT(face->info.data + os2 + 76);
    }
    if (winAscent + winDescent > 0) {
        ascent = winAscent;
        descent = -winDescent;
    }
    m.scale = (float)size / (float)(ascent - descent);
    m.ascent = (int)lroundf(ascent * m.scale);
    m.descent = (int)lroundf(-descent * m.scale);
    m.lineHeight = m.ascent + m.descent;
    return m;
}

static int advance_of(Face *face, uint32_t c, float scale) {
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&face->info, (int)c, &advance, &bearing);
    return (int)lroundf(advance * scale);
}

void text_size(const FontRequest *font, const char *utf8, int *w, int *h) {
    Face *primary = primary_face(font);
    if (!primary) {
        *w = 0;
        *h = font->size;
        return;
    }
    Metrics m = metrics(primary, font->size);
    int width = 0;
    for (const char *s = utf8; *s;) {
        uint32_t c = next_char(&s);
        Face *face = face_for(primary, c);
        float scale = face == primary ? m.scale : metrics(face, font->size).scale;
        width += advance_of(face, c, scale);
    }
    if (font->bold && !primary->bold) width += 1;
    *w = width;
    *h = m.lineHeight;
}

bool text_render(const FontRequest *font, const char *utf8, uint8_t **mask, int *w, int *h, int *ascent) {
    Face *primary = primary_face(font);
    if (!primary) return false;
    Metrics m = metrics(primary, font->size);
    int width, height;
    text_size(font, utf8, &width, &height);
    bool fakeBold = font->bold && !primary->bold;
    bool fakeItalic = font->italic && !primary->italic;
    int slant = fakeItalic ? height / 4 : 0;
    int maskW = width + slant + 2, maskH = height;
    uint8_t *out = calloc((size_t)maskW * (size_t)(maskH > 0 ? maskH : 1), 1);
    if (!out) return false;
    int pen = 0;
    for (const char *s = utf8; *s;) {
        uint32_t c = next_char(&s);
        Face *face = face_for(primary, c);
        Metrics fm = face == primary ? m : metrics(face, font->size);
        int x0, y0, x1, y1;
        stbtt_GetCodepointBitmapBox(&face->info, (int)c, fm.scale, fm.scale, &x0, &y0, &x1, &y1);
        int gw = x1 - x0, gh = y1 - y0;
        if (gw > 0 && gh > 0) {
            uint8_t *glyph = malloc((size_t)gw * gh);
            if (glyph) {
                stbtt_MakeCodepointBitmap(&face->info, glyph, gw, gh, gw, fm.scale, fm.scale, (int)c);
                int left, bearing;
                stbtt_GetCodepointHMetrics(&face->info, (int)c, &left, &bearing);
                for (int y = 0; y < gh; y++) {
                    int ty = m.ascent + y0 + y;
                    if (ty < 0 || ty >= maskH) continue;
                    int shear = slant ? slant * (maskH - ty) / maskH : 0;
                    for (int x = 0; x < gw; x++) {
                        int tx = pen + x0 + x + shear;
                        if (tx < 0 || tx >= maskW) continue;
                        uint8_t v = glyph[y * gw + x];
                        uint8_t *dst = &out[ty * maskW + tx];
                        if (v > *dst) *dst = v;
                        if (fakeBold && tx + 1 < maskW && v > dst[1]) dst[1] = v;
                    }
                }
                free(glyph);
            }
        }
        pen += advance_of(face, c, fm.scale);
    }
    *mask = out;
    *w = maskW;
    *h = maskH;
    *ascent = m.ascent;
    return true;
}
