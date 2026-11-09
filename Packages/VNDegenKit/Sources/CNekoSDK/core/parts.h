// Game parts: code for one game's own commands, screens and drawing, kept in files of its own. A part's file defines a
// maker, the list in parts.cpp names it, a game's table turns it on with `parts NAME...` under `screen defaults`, and
// the engine calls it at the points below. What a part may use of the engine is PartHost; ask for more there rather
// than reaching in.
#pragma once
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "canvas.h"
#include "image.h"
#include "script.h"

namespace neko {

class Engine;
struct SaveData;

// Where a part's drawing goes, in the order the frame is drawn.
enum class Layer {
    Slot,         // between the scene's picture slots: draw what sits from `fromSlot` up to before `toSlot`
    UnderWindow,  // over the scene, under the message window
    Window,       // inside the message window, over its plate and under its text
    OverWindow,   // over the message window and its buttons
    Menu,         // a table screen naming the part with `part NAME`: over its pictures, under its buttons
    Screen,       // the part's own screen, while it's open, over everything, or over a table screen naming the part
};

// What a part may use of the engine.
class PartHost {
public:
    explicit PartHost(Engine &engine) : e_(engine) {}
    ImageRef image(const std::string &path);
    std::shared_ptr<const std::vector<uint8_t>> read(const std::string &path);
    bool exists(const std::string &path);
    // The game's table: the lines of `key` on `screen`, each as its fields, in order.
    std::vector<std::vector<std::string>> setting(const std::string &screen, const std::string &key);
    // Variables by the number saves and cfg.sav keep them: scope 1 for the story's own, 0 for globals.
    int variable(int scope, int number);
    void setVariable(int scope, int number, int value);
    // The story's clock, which stops while a menu is open, and the menus' clock, which runs while one is.
    int64_t nowMs();
    int64_t screenMs();
    int width();
    int height();
    // The open table screen's page, from 0, as its `page` buttons set it.
    int page();
    void redraw();
    // From command(): the scene waits for a click, or for a time, before its next command.
    void waitClick();
    void waitMs(int ms);
    // From an open screen: it closes, and the story goes on with this result.
    void finishScreen(int result);
    // A menu table button's action from the part's own screen, such as nav_save, nav_load, nav_config or nav_title: the
    // menu opens over the part's screen as from the story, and its Back returns there, the part waiting as it was.
    void openMenu(const std::string &action);
    // A sound on one of the engine's channels: kind 0 music, 1 effects, 2 voice.
    // Kind 0 music, 1 effects, 2 voices, each in its own channels.
    void playSound(int kind, int channel, const std::string &path, int volume = 100, bool loop = false);
    // Whether that channel's sound is still playing.
    bool soundPlaying(int kind, int channel);
    // A picture into a slot as command 15 loads one, shown by the next screen update and gone with 17 or 22, saved as any
    // picture: its top left at x, y, its light added to the scene when `additive`, and, with a scroll, tiled along that
    // axis and moving that many pixels a second along it. False when the picture is missing.
    bool loadImage(int slot, const std::string &path, int x, int y, bool additive = false, float scrollX = 0, float scrollY = 0);
    void log(const std::string &line);

private:
    Engine &e_;
};

class GamePart {
public:
    virtual ~GamePart() = default;
    // A scene command the engine doesn't know: true when this part ran it.
    virtual bool command(PartHost &, const Node &) { return false; }
    // Each frame; true while the part needs the frame drawn again.
    virtual bool step(PartHost &) { return false; }
    // Drawing at a layer. For Layer::Slot, what sits in slots from `fromSlot` to before `toSlot`.
    virtual void draw(PartHost &, Canvas &, Layer, int fromSlot, int toSlot) {}
    // The story table's `open NAME ARGS...`: true when this part opened that screen. It gets the input until it calls
    // finishScreen. A table screen with `part NAME` calls it too as it opens, with the screen's name and no ARGS; while
    // that screen is open the part draws at Layer::Menu and Layer::Screen and is offered each press first.
    virtual bool openScreen(PartHost &, const std::string &name, const std::vector<int> &args) { return false; }
    // A press at x, y, button 0 left and 1 right: true when this part took it.
    virtual bool press(PartHost &, int x, int y, int button) { return false; }
    virtual void pointer(PartHost &, int x, int y) {}
    // Controller and keyboard focus while this part's screen is open: the middles of its controls, which a direction
    // moves the pointer between and Confirm presses as a click there. True when only these count, as for a viewer open
    // over a table screen's own controls.
    virtual bool focusSpots(PartHost &, std::vector<std::pair<int, int>> &spots) { return false; }
    // With the table's `partplate`, whether the message window's plate shows over this part's screen now.
    virtual bool showsPlate() const { return true; }
    // The story table's `chapter N`: the scene scripts to play, in order, or none when this part doesn't know it. A scene
    // may end in ` @N`, the PC's number for it, which a save in it holds and a PC save with it resumes at.
    virtual std::vector<std::string> chapter(PartHost &, int number) { return {}; }
    // State kept with a save: lines in VNDegen's own sidecar, and the PC's save.
    virtual void save(std::vector<std::string> &lines) const {}
    virtual void load(const std::vector<std::string> &lines) {}
    virtual void savePC(SaveData &) const {}
    virtual void loadPC(const SaveData &) {}
    // A new story, a load, or back to the title.
    virtual void reset() {}
};

// A new part by the name parts.cpp's list gives it, or none.
std::unique_ptr<GamePart> makePart(const std::string &name);

}  // namespace neko
