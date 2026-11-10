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
    ButtonRow,    // just under the message window's lower row of buttons, while that row shows
    OverWindow,   // over the message window and its buttons
    // A table screen naming the part with `part NAME`: over its `pic` lines and under its cards, as the strip along the
    // bottom of Test Game 90's Save and Load runs behind the lowest row of cards (set 1, under Wine).
    MenuUnder,
    Menu,         // the same screen: over its pictures and cards, under its buttons
    Screen,       // the part's own screen, while it's open, over everything, or over a table screen naming the part
};

// What a part may use of the engine.
class PartHost {
public:
    explicit PartHost(Engine &engine) : e_(engine) {}
    ImageRef image(const std::string &path);
    std::string shownPath(const std::string &path);
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
    // That channel's sound stops now.
    void stopSound(int kind, int channel);
    // A picture into a slot as command 15 loads one, shown by the next screen update and gone with 17 or 22, saved as any
    // picture: its top left at x, y, its light added to the scene when `additive`, and, with a scroll, tiled along that
    // axis and moving that many pixels a second along it. False when the picture is missing.
    // A picture into a scene slot, shown by the next screen update as any picture is. `kind` is the number the PC's save
    // holds for such a record in the older layout (SavedSprite::kind), 0 for a plain picture.
    bool loadImage(int slot, const std::string &path, int x, int y, bool additive = false, float scrollX = 0, float scrollY = 0, int kind = 0);
    // A scene slot of the part's own with no picture, such as a particle emitter's: the next screen update shows it,
    // the scene's commands take it away and fade it as they do a picture, and while it's there the part's drawSlot
    // draws it in the slot's order. In the older save layout a save holds a record for it, of this kind and with no
    // picture, which the part's savePC fills (SavedSprite::names and fixedBytes); a load brings the slot back from such a
    // record, which the part's loadPC reads.
    void holdSlot(int slot, int kind);
    // Lets a held slot go: taken away at the next screen update, as command 17 takes a picture, and out of saves from
    // then on. Test Game 86's command 96 ends a particle emitter so (set 3).
    void releaseSlot(int slot);
    // Whether that slot is held, shown or waiting for the next screen update.
    bool slotHeld(int slot);
    // Config's Effects setting, which the PC's effects such as Test Game 83's particles follow.
    bool effectsOn();
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
    // A slot this part holds (PartHost::holdSlot), drawn where its number puts it among the scene's pictures, at the
    // slot's alpha. Every part is offered every held slot.
    virtual void drawSlot(PartHost &, Canvas &, int slot, float alpha) {}
    // The story table's `open NAME ARGS...`: true when this part opened that screen. It gets the input until it calls
    // finishScreen. A table screen with `part NAME` calls it too as it opens, with the screen's name and no ARGS; while
    // that screen is open the part draws at Layer::MenuUnder, Layer::Menu and Layer::Screen and is offered each press first.
    virtual bool openScreen(PartHost &, const std::string &name, const std::vector<int> &args) { return false; }
    // A press at x, y, button 0 left and 1 right: true when this part took it.
    virtual bool press(PartHost &, int x, int y, int button) { return false; }
    virtual void pointer(PartHost &, int x, int y) {}
    // Controller and keyboard focus while this part's screen is open: the middles of its controls, which a direction
    // moves the pointer between and Confirm presses as a click there. True when only these count, as for a viewer open
    // over a table screen's own controls.
    virtual bool focusSpots(PartHost &, std::vector<std::pair<int, int>> &spots) { return false; }
    // A button's action the engine doesn't know, such as a message window button of the game's own: true when this
    // part took it. And how such a button of this part's is drawn: 0 plain, 1 lit, 2 disabled, or -1 when the action
    // isn't this part's.
    virtual bool action(PartHost &, const std::string &name) { return false; }
    virtual int actionLit(const std::string &name) const { return -1; }
    // With the table's `partplate`, whether the message window's plate shows over this part's screen now.
    virtual bool showsPlate() const { return true; }
    virtual bool coversFrame() const { return true; }
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
    // For tests and headless tools: a few words on the part's open screen, added to vnneko_debug_status's line, such as
    // where its moving controls are now.
    virtual std::string debugStatus() const { return {}; }
};

// A new part by the name parts.cpp's list gives it, or none.
std::unique_ptr<GamePart> makePart(const std::string &name);

}  // namespace neko
