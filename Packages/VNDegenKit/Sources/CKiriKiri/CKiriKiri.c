// SwiftPM builds a C target only when it has a source file. EngineKiriKiri imports this target for VNKKLaunch and
// resolves the functions at run time, so nothing here links against the engine.
#include "vnkk.h"
