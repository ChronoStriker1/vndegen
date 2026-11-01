// One running System 4 game: its files, its VM on a thread of its own, the screen, sound and input. The host
// drives it through the C interface in vns4.h; the HLL modules reach it through game().
#pragma once
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
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
// The file in dir whose name matches name in any case; empty when there is none.
std::string findCaseless(const std::string &dir, const std::string &name);
// What the game page says about how the game runs, such as the parts that don't yet; empty when there is nothing.
std::string compatibilityNote(const Ain &ain);

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
    // Cheats (cheats.cpp), on the game's globals while the VM waits for the next step. The state is CheatState's JSON,
    // empty while the VM is running. cheatAddGold returns false when the game has no money global or the VM is running.
    std::string cheatState();
    int cheatSetGlobal(int index, int32_t value);  // a VNS4_CHEAT_ result
    bool cheatAddGold(int amount);
    bool hasGold() const { return vm && goldGlobal() >= 0; }  // the code file never changes while the game runs
    // The game's clock runs this many times as fast.
    void setSpeed(int speed) { speed_ = std::clamp(speed, 1, 8); }

    // Game side, on the VM thread.
    GameConfig cfg;
    std::unique_ptr<VM> vm;
    int screenW = 800, screenH = 600;
    std::map<std::string, std::string> ini;  // AliceStart.ini or System40.ini, keys and raw values
    std::unique_ptr<AldArchive> cgArchive, waveArchive, bgmArchive;
    // Later games (Test Game 56) keep pictures by name in <stem>CG.afa, and load more archives through AFAFactory.
    std::unique_ptr<AfaArchive> cgAfa;
    // Their music and sounds are numbered entries ("00001.ogg") of <stem>BA.afa and <stem>WA.afa.
    std::unique_ptr<AfaArchive> bgmAfa, waveAfa;
    std::vector<std::unique_ptr<AfaArchive>> extraAfa;
    // A picture by its CP932 name, without extension: the CG archive's "<name>.qnt" and the like.
    const ArchiveEntry *cgFind(const std::string &name, AfaArchive **from = nullptr) const;
    bool cgLoadNamed(const std::string &name, Image &out);
    std::shared_ptr<const Image> cgCachedNamed(const std::string &name);
    void cgForgetNamed(const std::string &name);  // after the game rewrites a picture file, such as a thumbnail
    Sprites sprites;
    Audio audio;
    void log(const std::string &s);
    void problem(const std::string &s);
    // Where a game starts its 3D scenes (SealEngine, Test Game 56's battles and maps): shows a notice over the
    // dimmed screen and holds the game there until the player quits.
    [[noreturn]] void stopAt3D();
    uint32_t now() const { return clockMs_.load(); }
    // Time stepped without the Cheats page's game speed: a movie keeps to its sound, which plays at normal speed.
    uint32_t movieNow() const { return movieClockMs_.load(); }
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
    // Typed text, CP932, from the host while the game's text entry is open (InputString.Begin to End).
    std::atomic<bool> textEntry{false};
    void typed(const std::string &cp932) {
        if (!textEntry) return;
        std::lock_guard<std::mutex> lock(typedMu_);
        typed_ += cp932;
    }
    std::string takeTyped() {
        std::lock_guard<std::mutex> lock(typedMu_);
        return std::exchange(typed_, {});
    }
    // The last problem() text; the VM thread sets it while the host may read it.
    std::string lastProblem() {
        std::lock_guard<std::mutex> lock(problemMu_);
        return lastProblem_;
    }
    void dumpSpritesOnNextFrame() { dumpSprites_ = true; }  // debugging: logs every sprite
    void debugCall(const std::string &sjisName) { std::lock_guard<std::mutex> l(mu_); debugCall_ = sjisName; }
    std::atomic<bool> ended{false};
    // Times the VM has finished a frame and handed control back: each one has had its chance to read the input.
    std::atomic<uint32_t> yields{0};

private:
    void vmMain();
    bool cgLoad(int number, Image &out, int depth);
    std::thread thread_;
    std::mutex mu_;
    std::condition_variable cv_;
    bool vmTurn_ = false;            // the VM thread runs while true; the host waits
    bool stopping_ = false;
    std::atomic<bool> paused_{false};
    std::atomic<int> speed_{1};
    std::string codeStem_;            // the code file's name without .ain, such as Rance02
    bool cheatReady() const;          // with mu_ held
    int goldGlobal() const;           // the global the party's money is in, or -1
    std::atomic<uint32_t> clockMs_{0};
    std::atomic<uint32_t> movieClockMs_{0};
    uint32_t budgetMs_ = 0;          // game time the current step may still use
    int hitches_ = 0, lostMs_ = 0;   // steps the clock capped and the time they dropped, since the last timing log
    uint32_t timingLoggedAt_ = 0;
    bool inputLog_ = false;          // S4_INPUTLOG: every key and button change the host sends
    std::atomic<bool> keys_[256] = {};
    std::atomic<int> mouseX_{0}, mouseY_{0}, wheel_{0};
    std::mutex frameMu_;
    std::vector<uint8_t> frame_;
    FILE *logFile_ = nullptr;
    std::mutex problemMu_, typedMu_;
    bool stoppedAt3D_ = false;  // the VM thread's
    std::string typed_;
    std::string lastProblem_;
    std::atomic<bool> dumpSprites_{false};
    std::string debugCall_;
    std::mutex cgMu_;
    std::map<int, std::weak_ptr<const Image>> cgCache_;
    std::map<std::string, std::weak_ptr<const Image>> cgNamedCache_;
    std::vector<std::shared_ptr<const Image>> cgRecent_;  // keeps recently used pictures alive
};

Game &game();  // the running game; the HLL modules call it

}  // namespace s4
