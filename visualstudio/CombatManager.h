#pragma once
#include <BWAPI.h>
#include "Squad.h"

class ProtoBotCommander;

class CombatManager{
public:
    ProtoBotCommander* commanderReference;
    CombatManager(ProtoBotCommander* commanderReference);
    BWAPI::Unitset combatUnits;
	std::map<int, int> unitSquadMap;
    std::vector<Squad> Squads;
    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);
    void update();
    void attack();
    void addSquad();
    void removeSquad(int squadId);
    void redistributeUnits();
    void assignUnit(BWAPI::Unit unit);
    void drawDebugInfo();
    bool isAssigned(BWAPI::Unit unit);
};