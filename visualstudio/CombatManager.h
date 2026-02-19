#pragma once
#include <BWAPI.h>
#include "Squad.h"
#include "A-StarPathfinding.h"

#define FRAMES_BETWEEN_CACHING 10
using namespace std;

class ProtoBotCommander;

class CombatManager{
public:
    ProtoBotCommander* commanderReference;
    CombatManager(ProtoBotCommander* commanderReference);
    BWAPI::Unitset totalCombatUnits;
	map<BWAPI::Unit, int> unitSquadIdMap;
    vector<Squad*> Squads = vector<Squad*>();
    vector<Squad*> DefendingSquads = vector<Squad*>();
    vector<Squad*> AttackingSquads = vector<Squad*>();
    vector<Squad*> IdleSquads = vector<Squad*>();

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);
    void attack(BWAPI::Position position);
    void defend(BWAPI::Position position);
    Squad* addSquad(BWAPI::Unit leaderUnit);
    void removeSquad(Squad* squad);
    bool assignUnit(BWAPI::Unit unit);
    int isAssigned(BWAPI::Unit unit);
    void drawDebugInfo();
    BWAPI::Unit getAvailableUnit();
    BWAPI::Unit getAvailableUnit(function<bool(BWAPI::Unit)> filter);

    void handleTextCommand(std::string text);
};