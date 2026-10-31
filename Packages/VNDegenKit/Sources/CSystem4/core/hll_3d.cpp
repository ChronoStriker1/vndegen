// SealEngine and MarmotModelEngine: the 3D libraries of later games (Test Game 56's battles and maps). VNDegen
// doesn't draw 3D. The calls the 2D parts of the game make each frame do nothing.
#include "game.h"

namespace s4 {

S4_HLL(SealEngine, SetMagSpeed) {}
S4_HLL(MarmotModelEngine, Update) {}

}  // namespace s4
