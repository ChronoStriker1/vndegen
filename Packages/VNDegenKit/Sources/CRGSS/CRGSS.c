// SwiftPM builds a C target only when it has a source file. EngineRGSS imports this target for VNRGSSLaunch and
// resolves the functions at run time from the RGSS framework, so nothing here links against the engine.
#include "vnrgss.h"
