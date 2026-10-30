// SwiftPM builds a C target only when it has a source file. EngineGodot imports this target for VNGDLaunch and resolves
// the functions at run time from the Godot framework a game needs, so nothing here links against an engine.
#include "vngd.h"
