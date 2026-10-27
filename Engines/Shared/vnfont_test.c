// Tests vnfont against the real table in fonts/, with a second font folder standing in for downloaded fonts. On Linux:
//   cc -o /tmp/vnfont_test vnfont.c vnfont_test.c -lpthread && /tmp/vnfont_test fonts "$(mktemp -d)"
#include "vnfont.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void expect(const char *name, int bold, int italic, const char *file, int embolden, int slant) {
    vnfont_match match;
    int found = vnfont_resolve(name, bold, italic, &match);
    const char *base = found ? strrchr(match.path, '/') + 1 : NULL;
    int ok = file ? found && !strcmp(base, file) && match.embolden == embolden && match.slant == slant : !found;
    if (!ok) {
        failures++;
        printf("FAIL %s bold=%d italic=%d: got %s embolden=%d slant=%d, want %s embolden=%d slant=%d\n", name, bold, italic,
               base ? base : "nothing", found ? match.embolden : 0, found ? match.slant : 0, file ? file : "nothing",
               embolden, slant);
    }
}

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: vnfont_test <fonts folder> <empty folder>\n");
        return 2;
    }
    char table[1024], downloaded[1024];
    snprintf(table, sizeof table, "%s/fonts.tsv", argv[1]);
    const char *dirs[] = {argv[1], argv[2]};
    if (vnfont_init(table, dirs, 2) != 0) {
        printf("FAIL cannot read %s\n", table);
        return 1;
    }
    int bundled = vnfont_count();

    expect("Arial", 0, 0, "LiberationSans-Regular.ttf", 0, 0);
    expect("arial", 1, 0, "LiberationSans-Bold.ttf", 0, 0);
    expect("Arial", 1, 1, "LiberationSans-Bold.ttf", 0, 1);
    expect("Arial", 0, 1, "LiberationSans-Regular.ttf", 0, 1);
    expect("MS PGothic", 0, 0, "ume-pgo4.ttf", 0, 0);
    expect("ＭＳ Ｐゴシック", 0, 0, "ume-pgo4.ttf", 0, 0);
    expect("MS Pゴシック", 1, 0, "ume-pgo4.ttf", 1, 0);
    expect("@ＭＳ ゴシック", 0, 0, "ume-tgo4.ttf", 0, 0);
    expect("msgothic", 0, 0, "ume-tgo4.ttf", 0, 0);
    expect("ＭＳ 明朝", 0, 0, "ume-tmo3.ttf", 0, 0);
    expect("MS PMincho", 0, 0, "ume-pmo3.ttf", 0, 0);
    expect("Tahoma", 1, 0, "DejaVuSansCondensed-Bold.ttf", 0, 0);
    expect("Courier New", 0, 0, "LiberationMono-Regular.ttf", 0, 0);
    expect("Comic Sans MS", 0, 0, NULL, 0, 0);
    expect("", 0, 0, NULL, 0, 0);
    // Not downloaded yet.
    expect("Microsoft YaHei", 0, 0, NULL, 0, 0);
    expect("굴림", 0, 0, NULL, 0, 0);

    // Downloaded: the Chinese files appear in the second folder.
    snprintf(downloaded, sizeof downloaded, "%s/SourceHanSansCN-Regular.otf", argv[2]);
    FILE *f = fopen(downloaded, "w");
    if (!f || fclose(f)) return 2;
    vnfont_init(table, dirs, 2);
    expect("微软雅黑", 0, 0, "SourceHanSansCN-Regular.otf", 0, 0);
    expect("SimHei", 1, 0, "SourceHanSansCN-Regular.otf", 1, 0);
    if (vnfont_count() <= bundled) {
        failures++;
        printf("FAIL count %d after the download, %d before\n", vnfont_count(), bundled);
    }
    vnfont_entry entry;
    if (!vnfont_entry_at(0, &entry) || strcmp(entry.name, "Arial") || vnfont_entry_at(vnfont_count(), &entry)) {
        failures++;
        printf("FAIL entries\n");
    }

    char key[64];
    if (!vnfont_normalize("@ＭＳ　Ｐゴシック Bold", key, sizeof key) || strcmp(key, "mspゴシックbold")
        || vnfont_normalize("Arial", key, 5)) {
        failures++;
        printf("FAIL normalize\n");
    }

    // A missing table leaves nothing.
    if (vnfont_init("/nonexistent/fonts.tsv", dirs, 2) != -1 || vnfont_count() != 0) {
        failures++;
        printf("FAIL missing table\n");
    }
    expect("Arial", 0, 0, NULL, 0, 0);

    printf("%s: %d failures, %d bundled lines\n", failures ? "FAILED" : "passed", failures, bundled);
    return failures != 0;
}
