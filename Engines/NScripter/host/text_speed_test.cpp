#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "engine.h"

// One round-trip test through a game's define block, menu selection and normal quit.
int main() {
    namespace fs = std::filesystem;
    char temp[] = "/tmp/vnns-text-speed-XXXXXX";
    const char *root = mkdtemp(temp);
    assert(root);
    fs::path dir(root);
    fs::create_directory(dir / "Saves");
    std::ofstream(dir / "0.txt") << "*define\nsavedir \"saves\"\ndefaultspeed 73,31,7\n"
                                   "insertmenu \"Slow\",TEXTSLOW\ninsertmenu \"Middle\",TEXTMIDDLE\n"
                                   "insertmenu \"Fast\",TEXTFAST\ngame\n*start\nclick\nend\n";
    std::ofstream(dir / "envdata") << "untouched PC environment";
    const int speeds[] = {73, 31, 7};
    for (int choice = 0; choice < 3; choice++) {
        {
            vnns::Engine engine({root, root});
            std::string error;
            assert(engine.start(&error));
            assert(engine.step(16));
            assert(engine.chooseWindowMenuItem(size_t(choice)));
            assert(engine.textSpeed_ == speeds[choice]);
            // Written on selection, before quitting. Resolve existing file and directory case too.
            auto path = dir / "Saves" / (choice == 0 ? "vndegen-text-speed.dat" : "VNDEGEN-TEXT-SPEED.DAT");
            assert(fs::file_size(path) == 1);
            assert(std::ifstream(path, std::ios::binary).get() == choice);
            engine.requestQuit();
            assert(!engine.step(16));
        }
        if (choice == 0)
            fs::rename(dir / "Saves" / "vndegen-text-speed.dat", dir / "Saves" / "VNDEGEN-TEXT-SPEED.DAT");
        {
            vnns::Engine engine({root, root});
            std::string error;
            assert(engine.start(&error));
            assert(engine.step(16));
            assert(engine.speedChoice_ == choice);
            assert(engine.textSpeed_ == speeds[choice]);
            assert(engine.windowMenuJson().find("\"id\":\"" + std::to_string(choice) +
                                               "\",\"title\":\"" + engine.windowMenu_[size_t(choice)].text +
                                               "\",\"checked\":true") != std::string::npos);
        }
    }
    std::string env;
    std::getline(std::ifstream(dir / "envdata"), env);
    assert(env == "untouched PC environment");
    fs::remove_all(dir);
}
