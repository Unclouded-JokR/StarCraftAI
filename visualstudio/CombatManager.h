#pragma once
#include <BWAPI.h>
#include "Squad.h"
#include "A-StarPathfinding.h"

class ProtoBotCommander;

class CombatManager{
public:
    ProtoBotCommander* commanderReference;
    CombatManager(ProtoBotCommander* commanderReference);
    BWAPI::Unitset combatUnits;
	std::map<int, int> unitSquadIdMap;
    std::map<BWAPI::Unit, Path> unitPathMap;
    std::vector<Squad> Squads;

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);
    void attack(BWAPI::Position position);
    Squad& addSquad();
    void removeSquad(int squadId);
    void move(BWAPI::Position position);
    Squad assignUnit(BWAPI::Unit unit);
    void drawDebugInfo();
    BWAPI::Unit getAvailableUnit();
    bool isAssigned(BWAPI::Unit unit);
};