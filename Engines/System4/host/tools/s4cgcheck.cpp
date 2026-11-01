// Decodes every picture in an ALD or AFA archive and reports the ones that fail. Each decoded picture is also encoded
// as a QNT (as save thumbnails are) and decoded again, which must give back the same colours.
// Usage: s4cgcheck <archive.ald|archive.afa>
#include <cstdio>
#include <string>

#include "archive.h"
#include "cg.h"

namespace {

int ok = 0, bad = 0, badRoundTrip = 0;

void check(s4::Archive &a, const s4::ArchiveEntry &e, const std::string &label) {
    std::vector<uint8_t> d;
    s4::Image img;
    if (!a.read(e, d) || !s4::decodeImage(d.data(), d.size(), img)) {
        if (++bad < 20) std::printf("failed %s\n", label.c_str());
        return;
    }
    ok++;
    std::vector<uint8_t> q = s4::encodeQNT(img.rgba.data(), img.w, img.h);
    s4::Image back;
    bool same = s4::decodeQNT(q.data(), q.size(), back) && back.w == img.w && back.h == img.h;
    for (size_t i = 0; same && i < img.rgba.size(); i += 4)
        same = back.rgba[i] == img.rgba[i] && back.rgba[i + 1] == img.rgba[i + 1] && back.rgba[i + 2] == img.rgba[i + 2];
    if (!same && ++badRoundTrip < 20) std::printf("QNT round trip differs %s\n", label.c_str());
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 2) return 2;
    std::string path = argv[1];
    if (path.size() > 4 && path.compare(path.size() - 4, 4, ".afa") == 0) {
        auto a = s4::AfaArchive::open(path);
        if (!a) return 2;
        for (auto &e : a->entries())
            if (e.name.size() > 4 && e.name.compare(e.name.size() - 4, 4, ".qnt") == 0) check(*a, e, e.name);
    } else {
        auto a = s4::AldArchive::open(path);
        if (!a) return 2;
        for (auto &[n, e] : a->entries()) check(*a, e, std::to_string(n) + " " + e.name);
    }
    std::printf("%d decoded, %d failed, %d QNT round trips differ\n", ok, bad, badRoundTrip);
    return bad || badRoundTrip ? 1 : 0;
}
