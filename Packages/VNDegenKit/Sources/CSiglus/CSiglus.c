// SwiftPM builds a C target only when it has a source file. EngineSiglus imports this target for vnsiglus.h and finds
// the table at run time in the VNSiglus framework the app embeds, so nothing here links against the engine.
#include "vnsiglus.h"
