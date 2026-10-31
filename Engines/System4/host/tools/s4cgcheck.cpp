// Decodes every picture in an ALD archive and reports the ones that fail. Usage: s4cgcheck <archive.ald>
#include <cstdio>
#include "archive.h"
#include "cg.h"
int main(int argc, char **argv) {
    if (argc < 2) return 2;
    auto a = s4::AldArchive::open(argv[1]);
    int ok = 0, bad = 0;
    for (auto &[n, e] : a->entries()) {
        std::vector<uint8_t> d;
        s4::Image img;
        if (a->read(e, d) && s4::decodeImage(d.data(), d.size(), img)) ok++;
        else { bad++; if (bad < 20) std::printf("failed %d %s\n", n, e.name.c_str()); }
    }
    std::printf("%d decoded, %d failed\n", ok, bad);
    return bad ? 1 : 0;
}
