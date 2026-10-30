// Engines/Shared/casepath.h under this runtime's own names. Other engines compile their own copies of casepath.c into
// the same app, so this copy's functions are renamed to keep them apart.
#pragma once

#define casepath_resolve rgss_casepath_resolve
#define casepath_changed rgss_casepath_changed
#define casepath_same_file rgss_casepath_same_file
#include "../../../../../Engines/Shared/casepath.h"
