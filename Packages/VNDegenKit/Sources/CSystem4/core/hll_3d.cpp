// SealEngine and MarmotModelEngine: the 3D libraries of later games (Test Game 56's battles and maps). VNDegen
// doesn't draw 3D. The calls the 2D parts of the game make each frame do nothing.
#include "game.h"

namespace s4 {

S4_HLL(SealEngine, SetMagSpeed) {}
// The game makes its 3D view as a parts layer first; that is where its 3D scenes begin.
S4_HLL(PartsEngine, Parts_CreateParts3DLayerPluginID) { game().stopAt3D(); }
S4_HLL(MarmotModelEngine, Update) {}

}  // namespace s4
