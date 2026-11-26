#include "sa_private.h"

enum {
    ENCOUNTER_QUIET = 10,
    ENCOUNTER_LATE = 90,
    ENCOUNTER_PERMILLE = 32,
    ENCOUNTER_LATE_PERMILLE = 5,
};

bool sa_encounter_due(int sinceBattle, int areaSteps, bool alarm, int percent, u32 roll) {
    if (sinceBattle <= ENCOUNTER_QUIET || areaSteps <= ENCOUNTER_QUIET) return false;
    int permille = sinceBattle > ENCOUNTER_LATE ? ENCOUNTER_LATE_PERMILLE : ENCOUNTER_PERMILLE;
    if (alarm) permille *= 2;
    return (int)(roll % 1000) < permille * percent / 100;
}
