// Engines/Shared/casepath.h under names of this runtime's own. NScripter compiles its own copy of casepath.c into
// the same app, so this copy's functions are renamed to keep the two apart.
#pragma once

#define casepath_resolve wolf_casepath_resolve
#define casepath_changed wolf_casepath_changed
#define casepath_same_file wolf_casepath_same_file
#include "../../../../../Engines/Shared/casepath.h"
