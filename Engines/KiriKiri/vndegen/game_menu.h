// The game's window menu bar, which the engine never draws: what a game that asks is told about it, and its commands
// for the host to list and choose in a menu of its own. Everything runs on the main thread.
#ifndef VNKK_GAME_MENU_H
#define VNKK_GAME_MENU_H

#include <string>

// Takes one of the VNKK_MENU_BAR_ values for the next game and forgets what the last game was told.
void VNKKSetMenuBarMode(int mode);
// Installs the engine's hook. Call as the game starts.
void VNKKInstallMenuBarHook();
// -1 until the game asked, then 1 when it was told the menu bar shows and 0 when it was told it is hidden.
int VNKKMenuBarShown();

// The main window's menu as JSON, as vnkk_menu describes it. Empty when the game has no main window.
std::string VNKKMenuJSON();
// Queues a click on the item with the given id, which the game handles in its next frame. False when no item has it.
bool VNKKMenuClick(const char *id);

#endif
