// One running System 4 game: its files, its VM on a thread of its own, the screen, sound and input. The host
// drives it through the C interface in vns4.h; the HLL modules reach it through game().
#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "archive.h"
#include "audio.h"
#include "cg.h"
#include "gfx.h"
#include "vm.h"

namespace s4 {

struct GameConfig {
    std::string root;        // folder holding the .ain
    std::string saveDir;     // the game's SaveData folder
    std::string logPath;
    std::string fontsDir;    // VNDegen's Windows font pack (fonts.tsv), may be empty
};

std::map<std::string, std::string> readIni(const std::string &root);
std::string codeFile(const std::string &root, const std::map<std::string, std::string> &ini);
// A sentence for the player when the game needs libraries this runtime doesn't have; empty when it can run.
std::string unsupportedReason(const Ain &ain);

class Game {
public:
    explicit Game(GameConfig cfg);
    ~Game();
    bool start(std::string &error);  // loads the game and starts its VM thread
    bool noThread = false;           // tools: load the game without running it

    // Host side, on the host's thread.
    bool step(int ms);               // runs the game for ms milliseconds of game time; false once it has ended
    void stop();
    void setPaused(bool p) { paused_ = p; }
    void key(int vk, bool down);
    void mouseMove(int x, int y);
    void wheel(int delta);
    // The last presented frame, RGBA. Copies it out under the lock.
    bool takeFrame(std::vector<uint8_t> &out, int &w, int &h);

    // Game side, on the VM thread.
    GameConfig cfg;
    std::unique_ptr<VM> vm;
    int screenW = 800, screenH = 600;
    std::map<std::string, std::string> ini;  // AliceStart.ini or System40.ini, keys and raw values
    std::unique_ptr<AldArchive> cgArchive, waveArchive, bgmArchive;
    Sprites sprites;
    Audio audio;
    void log(const std::string &s);
    void problem(const std::string &s);
    uint32_t now() const { return clockMs_.load(); }
    // Called where the game shows a frame or polls: hands control to the host until its next step.
    void yieldFrame();
    void present();                  // composites the sprites into the frame
    void showFrame(std::vector<uint8_t> f);  // publishes a frame drawn elsewhere, such as a transition
    bool cgLoad(int number, Image &out);  // a picture drawn over another (Image::overBase) comes out whole
    std::shared_ptr<const Image> cgCached(int number);
    bool cgExists(int number) const;

    // Input state the game reads.
    bool keyDown(int vk) const { return vk >= 0 && vk < 256 && keys_[vk].load(); }
    int mouseX() const { return mouseX_.load(); }
    int mouseY() const { return mouseY_.load(); }
    int takeWheel() { return wheel_.exchange(0); }
    std::string lastProblem;
    void dumpSpritesOnNextFrame() { dumpSprites_ = true; }  // debugging: logs every sprite
    void debugCall(const std::string &sjisName) { std::lock_guard<std::mutex> l(mu_); debugCall_ = sjisName; }
    std::atomic<bool> ended{false};

private:
    void vmMain();
    bool cgLoad(int number, Image &out, int depth);
    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool vmTurn_ = false;            // the VM thread runs while true; the host waits
    bool stopping_ = false;
    std::atomic<bool> paused_{false};
    std::atomic<uint32_t> clockMs_{0};
    uint32_t budgetMs_ = 0;          // game time the current step may still use
    std::atomic<bool> keys_[256] = {};
    std::atomic<int> mouseX_{0}, mouseY_{0}, wheel_{0};
    std::mutex frameMu_;
    std::vector<uint8_t> frame_;
    FILE *logFile_ = nullptr;
    std::atomic<bool> dumpSprites_{false};
    std::string debugCall_;
    std::mutex cgMu_;
    std::map<int, std::weak_ptr<const Image>> cgCache_;
    std::vector<std::shared_ptr<const Image>> cgRecent_;  // keeps recently used pictures alive
};

Game &game();  // the running game; the HLL modules call it

}  // namespace s4
