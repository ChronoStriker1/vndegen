// Saving and loading: the save commands and the game state in Save/SaveDataNN.sav, as the PC engine writes them.
#include "game.h"
#include "game_internal.h"

namespace wolf {

void Game::saveCommand(Thread &t, const Command &c) {
    if (c.code == 221) writeVar(c.arg(0), 0, t);
    unsupported(c);
}

}  // namespace wolf
