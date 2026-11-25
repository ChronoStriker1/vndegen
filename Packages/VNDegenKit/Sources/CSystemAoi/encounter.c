#include "sa_private.h"

enum {
    ENCOUNTER_QUIET = 10,
    ENCOUNTER_ODDS = 20,
};

bool sa_encounter_due(int sinceBattle, bool alarm, u32 roll) {
    if (sinceBattle <= ENCOUNTER_QUIET) return false;
    return roll % (u32)(alarm ? ENCOUNTER_ODDS / 2 : ENCOUNTER_ODDS) == 0;
}
