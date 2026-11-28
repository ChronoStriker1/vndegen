#include "game.h"
#include "render.h"
#include "savefile.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

namespace fs = std::filesystem;

namespace {

std::vector<uint8_t> readFile(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    assert(f);
    return {std::istreambuf_iterator<char>(f), {}};
}

void writeFile(const fs::path &p, const std::vector<uint8_t> &bytes) {
    std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
}

struct Player {
    wolf::Game game;
    std::unique_ptr<wolf::Renderer> renderer;
    wolf::Bitmap frame;
    explicit Player(const fs::path &root) : game(root) {
        game.log = [](const std::string &) {};
        std::string error;
        assert(game.load(&error));
        renderer = std::make_unique<wolf::Renderer>(game);
    }
    void run(int frames, uint32_t held = 0) {
        for (int i = 0; i < frames; i++) {
            wolf::Input in;
            in.held = held;
            in.pressed = i == 0 ? held : 0;
            game.update(in);
            renderer->draw(frame);
        }
    }
    void loadSlot(int slot) {
        run(5);
        std::string error;
        assert(game.loadSlot(slot, &error));
        run(121);
        run(2, wolf::KeyDown);
        run(12);
    }
    void tap(uint32_t key) {
        run(2, key);
        run(20);
    }
    void walk(uint32_t key, int frames) {
        run(frames, key);
        run(10);
    }
    std::array<int, 2> at() const { return game.heroHalfTile(); }
};

}

int main() {
    const char *gameDir = std::getenv("WOLF_FM2_DIR");
    const char *saveDir = std::getenv("WOLF_SAVE145_DIR");
    if (!gameDir || !saveDir) {
        std::puts("half steps: set WOLF_FM2_DIR to False Myth 2's game folder and WOLF_SAVE145_DIR to its saves");
        return 77;
    }
    fs::path root = fs::absolute(gameDir).parent_path() / "wolf-halfstep-test";
    fs::remove_all(root);
    fs::create_directories(root / "Save");
    fs::copy(fs::path(gameDir) / "Data", root / "Data", fs::copy_options::recursive | fs::copy_options::create_hard_links);
    auto pcSave = readFile(fs::path(saveDir) / "play1-SaveData02.sav");
    writeFile(root / "Save" / "SaveData02.sav", pcSave);

    {
        Player p(root);
        p.loadSlot(2);
        assert((p.at() == std::array<int, 2>{44, 40}));
        p.walk(wolf::KeyRight, 60);
        assert((p.at() == std::array<int, 2>{45, 40}));
    }
    {
        Player p(root);
        p.loadSlot(2);
        p.tap(wolf::KeyLeft);
        p.tap(wolf::KeyLeft);
        assert((p.at() == std::array<int, 2>{42, 40}));
        p.walk(wolf::KeyUp, 120);
        assert((p.at() == std::array<int, 2>{42, 36}));
        p.walk(wolf::KeyDown, 200);
        assert((p.at() == std::array<int, 2>{42, 50}));
        for (int i = 0; i < 9; i++) p.tap(wolf::KeyUp);
        for (int i = 0; i < 5; i++) p.tap(wolf::KeyRight);
        assert((p.at() == std::array<int, 2>{47, 41}));
        p.walk(wolf::KeyDown, 80);
        assert((p.at() == std::array<int, 2>{47, 47}));

        std::string error;
        assert(p.game.saveSlot(3, &error));
        wolf::SaveFile written = wolf::parseSave(readFile(root / "Save" / "SaveData03.sav"));
        assert(written.party.hero.x == 47 && written.party.hero.y == 47);
        assert(written.party.hero.v78 == 1);
    }
    {
        Player p(root);
        p.loadSlot(2);
        p.walk(wolf::KeyUp, 60);
        p.run(60);
        for (int i = 0; i < 4; i++) {
            p.run(2, wolf::KeyConfirm);
            p.run(30);
        }
        p.run(60);
        assert((p.at() == std::array<int, 2>{44, 35}));
        assert(p.game.heroTile()[2] == 2);
    }
    {
        Player p(root);
        p.loadSlot(2);
        p.walk(wolf::KeyDown, 150);
        assert((p.at() == std::array<int, 2>{44, 52}));
        p.tap(wolf::KeyConfirm);
        p.run(100);
        assert(p.game.choice().active && p.game.choice().items.size() == 2);
    }
    {
        wolf::SaveFile whole = wolf::parseSave(pcSave);
        whole.party.hero.v78 = 2;
        writeFile(root / "Save" / "SaveData04.sav", wolf::serializeSave(whole));
        Player p(root);
        p.loadSlot(4);
        p.tap(wolf::KeyUp);
        assert((p.at() == std::array<int, 2>{44, 38}));
        p.tap(wolf::KeyLeft);
        assert((p.at() == std::array<int, 2>{42, 38}));
    }
    fs::remove_all(root);
    std::puts("half steps: ok");
    return 0;
}
