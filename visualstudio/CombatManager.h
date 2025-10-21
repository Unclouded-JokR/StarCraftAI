#pragma once
#include <BWAPI.h>

class ProtoBotCommander;

class CombatManager{
public:
    ProtoBotCommander* commanderReference;
    CombatManager(ProtoBotCommander* commanderReference);
    void onFrame();
    void Update();
    void AttackClosest();
    void assignUnit(BWAPI::Unit unit);
};