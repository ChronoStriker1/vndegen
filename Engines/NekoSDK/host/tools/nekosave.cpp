// Reads saves the PC game wrote and writes them back, to test the save reader and writer against the real thing.
// usage: nekosave <user data folder, such as waffle\alchemy> <save file>...
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "archive.h"
#include "save.h"

using namespace neko;

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    const SaveLayout *layout = saveLayout(argv[1]);
    if (!layout) {
        std::printf("no layout for %s\n", argv[1]);
        return 2;
    }
    int failures = 0;
    for (int a = 2; a < argc; a++) {
        std::ifstream f(argv[a], std::ios::binary);
        std::vector<uint8_t> packed((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        auto plain = unpackEntry(packed.data(), packed.size());
        SaveData data;
        if (plain.empty() || !parseSave(plain, *layout, data)) {
            std::printf("%s: not read\n", argv[a]);
            failures++;
            continue;
        }
        auto again = buildSave(data, *layout);
        size_t differ = 0, first = size_t(-1);
        for (size_t i = 0; i < std::min(plain.size(), again.size()); i++)
            if (plain[i] != again[i]) {
                if (first == size_t(-1)) first = i;
                differ++;
            }
        std::printf("%s: flow %s #%d, scene %s #%d, %d sprites, %d log lines, %d sounds; %zu bytes, written %zu, %zu differ, first at 0x%zx\n", argv[a], data.flow.c_str(),
                    data.flowNode, data.scene.c_str(), data.node, int(data.sprites.size()), int(data.backlog.size()), int(data.sounds.size()), plain.size(), again.size(), differ, first);
        // With NEKOSAVE_SPRITES set, each sprite's place, which is how to learn what a load command's position modes mean.
        if (std::getenv("NEKOSAVE_SPRITES"))
            for (const SavedSprite &sprite : data.sprites)
                std::printf("  slot %d flip %d %s at %.1f, %.1f scale %.3f, %.3f anchor %d, %d alpha %d wave %d change %d frames [%s] end %.1f, %.1f scale %.3f\n", sprite.slot, sprite.flipFrame, sprite.path.c_str(), sprite.x, sprite.y, sprite.scaleX, sprite.scaleY, sprite.anchorX, sprite.anchorY, sprite.alpha, sprite.waveOn, sprite.animated, sprite.animationFrames, sprite.animation.c_str(), sprite.endX, sprite.endY, sprite.endScaleX);
        for (size_t i = first, shown = 0; first != size_t(-1) && i < std::min(plain.size(), again.size()) && shown < 12; i++)
            if (plain[i] != again[i]) {
                std::printf("  0x%zx: PC %02x, written %02x\n", i, plain[i], again[i]);
                shown++;
            }
    }
    return failures;
}
