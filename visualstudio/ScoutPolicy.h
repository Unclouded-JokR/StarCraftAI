#pragma once
#include <BWAPI.h>

// Temporary switch: only Zealots for now.
// Later, just add Dragoon/Observer here or remove the #ifdef.
inline bool IsAllowedScoutType(BWAPI::UnitType t) {
#ifdef SCOUTS_ZEALOT_ONLY
    return t == BWAPI::UnitTypes::Protoss_Zealot;
#else
    return t == BWAPI::UnitTypes::Protoss_Zealot
        || t == BWAPI::UnitTypes::Protoss_Dragoon
        || t == BWAPI::UnitTypes::Protoss_Observer;
#endif
}