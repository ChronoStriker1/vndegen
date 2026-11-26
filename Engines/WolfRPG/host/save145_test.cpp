#include "reader.h"
#include "savefile.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>

static std::vector<uint8_t> load(const std::string &dir, const char *name) {
    std::ifstream f(dir + "/" + name, std::ios::binary);
    assert(f);
    return {std::istreambuf_iterator<char>(f), {}};
}

static void sameAfterWriting(const wolf::SaveFile &pc) {
    auto written = wolf::serializeSave(pc);
    assert(written[8] == 2);
    wolf::SaveFile again = wolf::parseSave(written);
    again.header = pc.header;
    wolf::SaveFile a = pc;
    a.header[8] = again.header[8] = 0;
    assert(wolf::serializeSave(a) == wolf::serializeSave(again));
}

int main() {
    const char *dir = std::getenv("WOLF_SAVE145_DIR");
    if (!dir) {
        std::puts("save 145: set WOLF_SAVE145_DIR to a folder of False Myth 2 saves");
        return 77;
    }

    wolf::SaveFile menu = wolf::parseSave(load(dir, "play1-SaveData02.sav"));
    assert(menu.header[8] == 2 && menu.utf8());
    assert(menu.version == 145);
    assert(menu.title.bytes == "FalseMyth2");
    assert(menu.map.mapId == 27);
    assert(menu.party.hero.x == 44 && menu.party.hero.y == 40);
    assert(menu.system.b[0] == 277);
    assert(menu.pictures.count == 109 && menu.pictures.pictures.size() == 109);
    const wolf::SavePicture &light = menu.pictures.pictures[0];
    assert(light.number == 1 && light.file.bytes == "Back/light_l.png");
    assert(light.a[0] == 388 && light.a[1] == 436 && light.a[9] == 200 && light.a[10] == 200);
    assert((light.v73b == std::array<int32_t, 6>{1, 123, 195, -1, -1, -1}));
    for (auto &p : menu.pictures.pictures) assert((p.v9fC == std::array<int32_t, 2>{0, 1}));
    sameAfterWriting(menu);

    wolf::SaveFile boot = wolf::parseSave(load(dir, "boot1-101.sav"));
    assert(boot.version == 145 && boot.map.mapId == 110);
    assert(boot.map.width == 25 && boot.map.height == 19 && boot.map.layers == 3);
    assert(boot.party.hero.x == 26 && boot.party.hero.y == 12);
    assert(boot.system.frameCount == 1 && boot.pictures.count == 0);
    sameAfterWriting(boot);

    wolf::SaveFile afterVn = wolf::parseSave(load(dir, "pc-after-vn04-SaveData05.sav"));
    assert(afterVn.version == 145 && afterVn.system.b[0] == 7314);

    for (uint16_t unseen : {143, 144, 146, 158}) {
        wolf::SaveFile other = boot;
        other.version = unseen;
        bool refused = false;
        try {
            wolf::parseSave(wolf::serializeSave(other));
        } catch (const wolf::FormatError &) {
            refused = true;
        }
        assert(refused);
    }
    std::puts("save 145 ok");
}
