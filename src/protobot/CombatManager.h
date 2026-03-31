#pragma once
#include <BWAPI.h>
#include "Squad.h"
#include "A-StarPathfinding.h"
#include "BOIDS.h"
#include "StrategyManager.h"

//#define DEBUG_CM
//#define ASTAR_COMMANDING
#define FRAMES_BETWEEN_CACHING 11
using namespace std;

class ProtoBotCommander;
class SharedSquad;

struct unitCMHash {
    std::size_t operator()(const BWAPI::Unit& unit) const {
        return std::hash<int>{}(unit->getID());
    }
};

class CombatManager{
public:
    ProtoBotCommander* commanderReference;
    CombatManager(ProtoBotCommander* commanderReference);
    BWAPI::Unitset allUnits;
    vector<Squad*> Squads = vector<Squad*>();
    BWAPI::Position globalAttackPosition = BWAPI::Positions::Invalid;
	static unordered_map<BWAPI::Unit, Squad*, unitCMHash> unitSquadMap;
    static map<SquadState*, BWAPI::Color> stateColorMap;
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