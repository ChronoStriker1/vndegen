// Windows font substitutes, the way Proton provides them. Games ask for fonts by their Windows names, such as
// "MS PGothic" or "Arial". VNDegen can't ship Microsoft's fonts, so it bundles freely licensed fonts with the same
// metrics and maps each Windows name to one through the table in fonts/fonts.tsv. docs/fonts.md lists the fonts.
//
// Every engine reads the same table: native engines through this interface, compiled into each engine from this
// folder, and the app and the web engines through the FontPack Swift module, which wraps it. This interface is
// stable. New functions may be added; existing ones keep their meaning.
#ifndef VNDEGEN_VNFONT_H
#define VNDEGEN_VNFONT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { vnfont_path_max = 1024 };

// A substitute font file. `bold` and `italic` say what the file is. When the caller asked for bold or italic and the
// table has no such file, `embolden` or `slant` is 1, and the caller draws the regular face bolder or slanted, as
// Windows does for a font that has no bold or italic face.
typedef struct vnfont_match {
    char path[vnfont_path_max];
    // The face's index in a font collection (.ttc), 0 for a single font.
    int face;
    int bold;
    int italic;
    int embolden;
    int slant;
} vnfont_match;

// Reads the table at `table_path` and finds each file it names in `font_dirs`, the first folder that has it winning.
// Lines whose file is in no folder are left out, so a font the player hasn't downloaded resolves to nothing. Replaces
// any table read before. Returns 0, or -1 when the table can't be read, which leaves an empty table.
//
// Engines call it once at startup with the folders the app passes them. It is safe to call while other threads
// resolve.
int vnfont_init(const char *table_path, const char *const *font_dirs, int dir_count);

// Finds the substitute for the Windows font `name`, such as "MS PGothic", "ＭＳ Ｐゴシック" or "@MS Gothic". The match
// ignores ASCII case, spaces, full-width forms of ASCII characters, and a leading @, which Windows uses for vertical
// text. `bold` and `italic` are 0 or 1. Returns 1 and fills `match`, or 0 when the table has no substitute for the name.
int vnfont_resolve(const char *name, int bold, int italic, vnfont_match *match);

// Writes the form of `name` that lookups compare into `out`: no leading @, no spaces, full-width ASCII as ASCII, and
// ASCII in lower case. It is never longer than `name`. Returns 1, or 0 when it doesn't fit in `size` bytes. Callers
// that look for font names in a game's files normalize both sides with it.
int vnfont_normalize(const char *name, char *out, size_t size);

// The table's lines whose files are installed, in table order, for callers that list every substitute, such as the
// web engines' @font-face rules. `name` is the Windows name as the table spells it.
typedef struct vnfont_entry {
    char name[128];
    vnfont_match match;
} vnfont_entry;

int vnfont_count(void);
// Fills `entry` with line `index` of the installed lines. Returns 1, or 0 when `index` is out of range.
int vnfont_entry_at(int index, vnfont_entry *entry);

#ifdef __cplusplus
}
#endif

#endif
