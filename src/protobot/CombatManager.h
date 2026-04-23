#pragma once
#include <BWAPI.h>
#include "Squad.h"
#include "A-StarPathfinding.h"
#include "BOIDS.h"
#include "StrategyManager.h"

//#define DEBUG_CM
#define FRAMES_BETWEEN_CACHING 11
using namespace std;

class ProtoBotCommander;
class SharedSquad;

struct unitCMHash {
    std::size_t operator()(const BWAPI::Unit& unit) const {
        return std::hash<int>{}(unit->getID());
    }
};

/// \brief Combat module of ProtoBot
/// 
/// <summary>
/// CombatManager handles the combat functionality of ProtoBot. It handles unit assignment, squad creation and destruction, as well as strategic actions sent by StrategyManager (attack, defend, reinforce)
/// </summary>
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
    bool combat_debug_on = false;

    void onStart();
    void onFrame();
    void onUnitDestroy(BWAPI::Unit unit);
    bool assignUnit(BWAPI::Unit unit);

    /// \brief All squads move towards position to attack
    /// 
    /// <summary>
    /// Loops through all existing squads and updates their command positions.
    /// \nAlso, sets their state to AttackingState where they will move towards the position while attacking nearby enemies.
    /// </summary>
    /// <param name="position"></param>
    void attack(BWAPI::Position position);

    /// \brief Idle squads are given a position to defend. If any squads are currently attacking, this will override the state
    /// 
    /// <summary>
    /// Loops through IdleSquads vector. If any squads in IdleSquads, sets their state machine to the state DefendingState
    /// \n If any attacking squads, overrides their state machine to DefendingState.
    /// </summary>
    /// <param name="position"></param>
    void defend(BWAPI::Position position);

	/// \brief Moves to assist squads that are being attacked nearby. If any squads are currently attacking, this will override the state
    /// <summary>
    /// Affects squads in the DefendingState. If the squad is too far, less than MAX_REINFORCE_DIST (listed in SquadStateTypes.h), the squad will stay in DefendingState
    /// Also sends reinforce position to the current reinforcing squads to update their reinforce positions
    /// </summary>
    /// <param name="position"></param>
    void reinforce(BWAPI::Position position);

    Squad* addSquad(BWAPI::Unit leaderUnit);
    void removeSquad(Squad* squad);
    BWAPI::Unit getAvailableUnit();
    BWAPI::Unit getAvailableUnit(function<bool(BWAPI::Unit)> filter);

    /// \brief Detaches scout unit from squad when ScoutingManager grabs unit
    /// 
    /// <summary>
    /// ScoutingManager grabs combat scouts from CombatManager. This function is a fallback to ensure that scouts do not remain inside the squad.
    /// </summary>
    /// <param name="unit"></param>
    /// <returns>Boolean \n True = unit was deatched \n False = unit not found </returns>
    bool detachUnit(BWAPI::Unit unit);


private:
    /// \brief Queue of observers from destroyed squads
    /// <summary>
    /// Observers are not part of squad unit vectors. If a squad is destroyed and the observer is alive, this structure stores that observer.
    /// \n If a squad needs an observer, it will check detachedObservers for any available observers before requesting one.
    /// </summary>
    queue<BWAPI::Unit> detachedObservers;
    bool isAssigned(BWAPI::Unit unit);
    void drawDebugInfo();
};