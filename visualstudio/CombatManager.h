#pragma once
#include <BWAPI.h>

class ProtoBotCommander;

class CombatManager{
public:
    ProtoBotCommander* commanderReference;
    CombatManager(ProtoBotCommander* commanderReference);
    static void Update();
    void AttackClosest();
    void assignUnit(BWAPI::Unit unit);
};