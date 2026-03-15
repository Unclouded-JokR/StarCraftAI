#pragma once
#include <BWAPI.h>
#include "Squad.h"
#include "A-StarPathfinding.h"

#define DEBUG_CM
//#define ASTAR_COMMANDING
#define FRAMES_BETWEEN_CACHING 5
using namespace std;

class ProtoBotCommander;

class CombatManager{
public:
    ProtoBotCommander* commanderReference;
    CombatManager(ProtoBotCommander* commanderReference);
    BWAPI::Unitset allUnits;
	map<BWAPI::Unit, Squad*> unitSquadMap;
    vector<Squad*> Squads = vector<Squad*>();
    static vector<Squad*> AttackingSquads;
    static vector<Squad*> DefendingSquads;
    static vector<Squad*> ReinforcingSquads;
    static vector<Squad*> IdleSquads;

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);
    void attack(BWAPI::Position position);
    void defend(BWAPI::Position position);
    void reinforce(BWAPI::Position position);
    bool reinforceOutOfRange(Squad* squad, BWAPI::Position reinforcePos);
    Squad* addSquad(BWAPI::Unit leaderUnit);
    void removeSquad(Squad* squad);
    bool assignUnit(BWAPI::Unit unit);
    bool isAssigned(BWAPI::Unit unit);
    void drawDebugInfo();
    BWAPI::Unit getAvailableUnit();
    BWAPI::Unit getAvailableUnit(function<bool(BWAPI::Unit)> filter);

    void handleTextCommand(std::string text);

    // Adding in this to strip scouts from squad if they somehow make it in -Marshall
    bool detachUnit(BWAPI::Unit unit);
};