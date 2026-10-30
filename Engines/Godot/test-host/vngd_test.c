// Runs Godot games through vngd.h on Linux, one after another in one process, to test that the engine starts again
// after vngd_stop.
//   vngd_test <libgodot.so> <data dir> <frames> <pack> <exe> [<pack> <exe> ...]
// Built with -DVNGD_STATIC and linked with the engine, the first argument is ignored.
// VNGD_TEST_STEAM=1 and VNGD_TEST_EOS=1 ask for those stand-ins, and VNGD_TEST_KEY gives an encrypted pack's key in hex.
#include <vngd.h>

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef VNGD_STATIC
#define LOAD(name) typeof(&name) p_##name = &name;
#else
#define LOAD(name) typeof(&name) p_##name = dlsym(lib, #name); if (!p_##name) { fprintf(stderr, "missing %s\n", #name); return 2; }
#endif

int main(int argc, char **argv) {
    if (argc < 6) { fprintf(stderr, "usage: %s lib datadir frames pack exe...\n", argv[0]); return 2; }
#ifndef VNGD_STATIC
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 2; }
#endif
    LOAD(vngd_start) LOAD(vngd_iterate) LOAD(vngd_stop) LOAD(vngd_problem) LOAD(vngd_key) LOAD(vngd_request_quit)
    int frames = atoi(argv[3]);
    uint8_t key[32];
    const char *hex = getenv("VNGD_TEST_KEY");
    for (int i = 0; hex && i < 32; i++) sscanf(hex + 2 * i, "%2hhx", &key[i]);
    const char *steam = getenv("VNGD_TEST_STEAM"), *eos = getenv("VNGD_TEST_EOS");
    for (int i = 4; i + 1 < argc; i += 2) {
        VNGDLaunch launch = { .pack = argv[i], .executable = argv[i + 1], .dataDir = argv[2], .renderingMethod = "gl_compatibility",
                              .steam = steam && *steam == '1', .epicOnlineServices = eos && *eos == '1', .encryptionKey = hex ? key : NULL };
        fprintf(stderr, "=== start %s\n", argv[i]);
        if (p_vngd_start(&launch) != 0) {
            fprintf(stderr, "=== start failed: %s\n", p_vngd_problem() ? p_vngd_problem() : "?");
            p_vngd_stop();
            continue;
        }
        int frame = 0, quit = 0;
        for (; frame < frames && !quit; frame++) {
            quit = p_vngd_iterate();
            usleep(16000);
        }
        // Then ask it to close and give it two seconds.
        p_vngd_request_quit();
        int extra = 0;
        for (; !quit && extra < 120; extra++) {
            quit = p_vngd_iterate();
            usleep(16000);
        }
        fprintf(stderr, "=== ran %d frames, quit %s after %d more\n", frame, quit ? "yes" : "no", extra);
        // With VNGD_SNAPSHOT set, the engine writes its frames there. Each run's last frame is kept beside it.
        const char *snapshot = getenv("VNGD_SNAPSHOT");
        if (snapshot) {
            char kept[4096];
            snprintf(kept, sizeof kept, "%s.%d.png", snapshot, i / 2 - 1);
            rename(snapshot, kept);
        }
        p_vngd_stop();
        fprintf(stderr, "=== stopped\n");
    }
    return 0;
}
