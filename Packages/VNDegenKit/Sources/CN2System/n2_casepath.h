// Engines/Shared/casepath.h under names of this runtime's own, so its copy of casepath.c doesn't clash with the other
// engines' copies in the same app.
#pragma once

#define casepath_resolve n2_casepath_resolve
#define casepath_changed n2_casepath_changed
#define casepath_same_file n2_casepath_same_file
#include "../../../../Engines/Shared/casepath.h"
