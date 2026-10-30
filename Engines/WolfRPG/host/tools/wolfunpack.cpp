// Lists or extracts a Wolf RPG Editor archive.
#include "dxa.h"

#include <cstdio>
#include <filesystem>
#include <fstream>

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: wolfunpack <archive.wolf> [output folder]\n");
        return 2;
    }
    std::string error;
    auto a = wolf::Archive::open(argv[1], &error);
    if (!a) {
        std::fprintf(stderr, "%s: %s\n", argv[1], error.c_str());
        return 1;
    }
    auto paths = a->paths();
    std::printf("%s: DxLib archive version %d, %zu files\n", argv[1], a->version(), paths.size());
    int failed = 0;
    for (auto &p : paths) {
        auto data = a->read(p);
        if (!data) {
            std::printf("  FAILED %s\n", p.c_str());
            failed++;
            continue;
        }
        if (argc > 2) {
            std::filesystem::path out = std::filesystem::path(argv[2]) / p;
            std::filesystem::create_directories(out.parent_path());
            std::ofstream(out, std::ios::binary).write(reinterpret_cast<const char *>(data->data()), data->size());
        }
    }
    std::printf("%zu read, %d failed\n", paths.size() - failed, failed);
    return failed ? 1 : 0;
}
