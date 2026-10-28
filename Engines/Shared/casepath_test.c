// Tests casepath in an empty folder on a case-sensitive file system, which it takes as its argument. `scripts/build.sh
// test` runs it on a case-sensitive disk image. On Linux:
//   cc -o /tmp/casepath_test casepath.c casepath_test.c -lpthread && /tmp/casepath_test "$(mktemp -d)"
#include "casepath.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
static char root[1024];

static void touch(const char *relative) {
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", root, relative);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || close(fd) != 0) {
        perror(path);
        exit(2);
    }
}

static void folder(const char *relative) {
    char path[2048];
    snprintf(path, sizeof path, "%s/%s", root, relative);
    if (mkdir(path, 0755) != 0) {
        perror(path);
        exit(2);
    }
}

// Resolves root/asked and checks the result against root/expected and the return value.
static void expect(const char *asked, const char *expected, int found) {
    char path[2048], want[2048], out[2048];
    snprintf(path, sizeof path, "%s/%s", root, asked);
    snprintf(want, sizeof want, "%s/%s", root, expected);
    int r = casepath_resolve(path, out, sizeof out);
    if (r != found || strcmp(out, want) != 0) {
        printf("FAIL %s: got %d %s, want %d %s\n", asked, r, out, found, want);
        failures++;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <empty folder on a case-sensitive file system>\n", argv[0]);
        return 2;
    }
    snprintf(root, sizeof root, "%s", argv[1]);
    folder("Img");
    folder("Img/Pictures");
    touch("Img/Pictures/Title.png");
    touch("Img/Pictures/title.PNG");
    folder("audio");
    touch("audio/battle1.OGG");
    folder("save");

    expect("Img/Pictures/Title.png", "Img/Pictures/Title.png", 1);
    // An exact name wins over another that differs in case only.
    expect("Img/Pictures/title.PNG", "Img/Pictures/title.PNG", 1);
    // Otherwise the first in sorted order: "Title.png" sorts before "title.PNG".
    expect("img/pictures/TITLE.png", "Img/Pictures/Title.png", 1);
    expect("IMG/PICTURES", "Img/Pictures", 1);
    expect("Audio/Battle1.ogg", "audio/battle1.OGG", 1);
    expect("AUDIO/./battle1.ogg", "audio/./battle1.OGG", 1);
    expect("audio/../IMG/Pictures/TITLE.PNG", "audio/../Img/Pictures/Title.png", 1);
    // A missing name keeps the game's spelling, below folders in their real case.
    expect("SAVE/Save01.rvdata2", "save/Save01.rvdata2", 0);
    expect("IMG/missing/Title.png", "Img/missing/Title.png", 0);
    // A file can't hold anything.
    expect("audio/BATTLE1.ogg/x", "audio/battle1.OGG/x", 0);

    // A file made after the folder was listed is found, in any case, through the folder's time or casepath_changed.
    char made[2048];
    snprintf(made, sizeof made, "%s/save/Save01.rvdata2", root);
    sleep(1);
    touch("save/Save01.rvdata2");
    casepath_changed(made);
    expect("SAVE/save01.RVDATA2", "save/Save01.rvdata2", 1);
    touch("save/Save02.rvdata2");
    expect("save/SAVE02.rvdata2", "save/Save02.rvdata2", 1);

    char small[8];
    if (casepath_resolve(made, small, sizeof small) != -1) {
        printf("FAIL: a short buffer should return -1\n");
        failures++;
    }
    if (failures == 0) printf("casepath: all passed\n");
    return failures != 0;
}
