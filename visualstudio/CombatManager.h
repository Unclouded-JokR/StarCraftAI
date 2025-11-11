#pragma once
#include <BWAPI.h>
#include "Squad.h"

class ProtoBotCommander;

class CombatManager{
public:
    ProtoBotCommander* commanderReference;
    CombatManager(ProtoBotCommander* commanderReference);
    BWAPI::Unitset combatUnits;
	std::map<int, int> unitSquadIdMap;
    std::vector<Squad> Squads;

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);
    void attack();
    Squad& addSquad();
    void removeSquad(int squadId);
    void allSquadsMove(BWAPI::Position position);
    Squad assignUnit(BWAPI::Unit unit);
    void drawDebugInfo();
    bool isAssigned(BWAPI::Unit unit);
};