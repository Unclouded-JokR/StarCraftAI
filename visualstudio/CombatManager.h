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
    std::vector<Squad> Squads;

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);
    void attack(BWAPI::Position position);
    Squad& addSquad();
    void removeSquad(int squadId);
    void move(BWAPI::Position position);
    bool assignUnit(BWAPI::Unit unit);
    void drawDebugInfo();
    BWAPI::Unit getAvailableUnit();
    BWAPI::Unit getAvailableUnit(std::function<bool(BWAPI::Unit)> filter);
    bool isAssigned(BWAPI::Unit unit);
};