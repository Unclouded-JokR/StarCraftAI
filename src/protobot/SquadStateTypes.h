#pragma once
#include "SquadState.h"
#include "Squad.h"
#include "CombatManager.h"
#include "VectorPos.h"
#include "StrategyManager.h"
#include <bwem.h>

//#define DEBUG_STATES
#define MAX_REINFORCE_DIST 1500
#define KITING_FRAME_DELAY 8

using namespace std;

namespace {
	auto& bwemMap = BWEM::Map::Instance();
}
/// \brief The AttackingState has all units in the attacking squad move towards the assigned attack position
/// 
/// <summary>
/// In the AttackingState, squads will attack the position. During this movement, three scenarios can occur
/// \n - If no enemy is near squad: Continue moving towards attack position
/// \n - If an enemy is near squad: Attack enemy and move away towards base while weapons are on cooldown (Kiting)
/// \n - If squad is near chokepoint: Ignore kiting behavior and move towards attack position even if weapons are on cooldown (This behavior stops squads from cluttering too much at chokepoints and causing traffic jams)
/// </summary>
class AttackingState : public SquadState {
public:
	/// <summary>
	/// Squad is added to AttackingSquads vector in CombatManager
	/// </summary>
	/// <param name="squad"></param>
	void Enter(Squad* squad);
	/// <summary>
	/// Every frame, loops through all units in the squad.
	/// \n For each unit, finds closest chokepoint position (for avoiding traffic jams).
	/// 
	/// \n\n If unit is melee, applies kitingMelee() method from KitingBehaviors class. 
	/// \n If unit is ranged, applies kitingRanged() method from KitingBehaviors class.
	/// 
	/// </summary>
	/// <param name="squad"></param>
	void Update(Squad* squad);
	/// <summary>
	/// Squad is removed from AttackingSquads vector in CombatManager
	/// </summary>
	/// <param name="squad"></param>
	void Exit(Squad* squad);
	static SquadState& getInstance();
	set<BWAPI::Position> chokepointPositions;
private:
	AttackingState() {};
	AttackingState(const AttackingState&);
	AttackingState& operator=(const AttackingState&) {};
};

/// \brief All units move towards assigned chokepoints and defends against any enemy attacks
/// 
/// <summary>
/// Squads are assigned a defensive position at a chokepoint from the StrategyManager.
/// \n Squads move towards the defensive position while attacking any enemies along the way. Once the squad arrives, stays at position and defends.
/// </summary>
class DefendingState : public SquadState {
public:
	/// <summary>
	/// Adds squad to DefendingSquads vector in CombatManager
	/// </summary>
	/// <param name="squad"></param>
	void Enter(Squad* squad);
	/// <summary>
	/// Every frame, loops through all units in the squad and "attack-move"s towards defensive position, attacking nearby enemies
	/// </summary>
	/// <param name="squad"></param>
	void Update(Squad* squad);
	/// <summary>
	/// Removes squad from DefendingSquads vector in CombatManager and resets currentDefensivePosition to invalid
	/// </summary>
	/// <param name="squad"></param>
	void Exit(Squad* squad);
	static SquadState& getInstance();
private:
	DefendingState() {};
	DefendingState(const DefendingState&);
	DefendingState& operator=(const DefendingState&) {};
};

/// \brief Squad moves towards nearby friendly squads being attacked. Once no nearby squads are being attacked or the squad moves too far away from its defensive position (MAX_REINFORCE_DIST defined in SquadStateTypes.h), the squad returns to its defensive state and position.
/// 
/// <summary>
/// Checks if nearby squads are being attacked with a distance < MAX_REINFORCE_DIST. 
/// \n If so, the squad moves towards the nearby squad while attacking any enemies along the way. If no nearby squads are being attacked or the squad moves too far away from its defensive position, the squad returns to its defensive state and position.
/// </summary>
class ReinforcingState : public SquadState {
public:
	/// <summary>
	/// Adds squad to ReinforcingSquads vector in CombatManager
	/// </summary>
	/// <param name="squad"></param>
	void Enter(Squad* squad);
	/// <summary>
	/// Every frame, checks if the reinforce position sent to it by StrategyManager is within range (distance < MAX_REINFORCE_DIST)
	/// \n If enemies in range, moves towards reinforce position while attacking nearby enemies.
	/// \n If no longer close enough or no enemies remain, returns to DefendingState.
	/// 
	/// \n\n While in ReinforcingState, the squad handles combat similar to AttackingState.
	/// \n Units use the kiting methods kitingMelee() and kitingRanged() from KitingBehaviors class to attack enemies while moving back towards the base or forward towards the reinforce position.
	/// </summary>
	/// <param name="squad"></param>
	void Update(Squad* squad);
	/// <summary>
	/// Removes squad from ReinforcingSquads vector in CombatManager and resets currentReinforcePosition to invalid
	/// </summary>
	/// <param name="squad"></param>
	void Exit(Squad* squad);
	static SquadState& getInstance();
private:
	ReinforcingState() {};
	ReinforcingState(const ReinforcingState&);
	ReinforcingState& operator=(const ReinforcingState&) {};
};

/// <summary>
/// When any squad is created, it is assigned this state by default. This state does nothing.
/// </summary>
class IdleState : public SquadState {
public:
	/// <summary>
	/// Squad is added to IdleSquads vector in CombatManager
	/// </summary>
	/// <param name="squad"></param>
	void Enter(Squad* squad);
	/// <summary>
	/// Base state for all squads when first created.
	/// \n These squads will be later reassigned after actions are received from StrategyManager.
	/// \n Idle squads will not do anything until they are assigned a new state.
	/// </summary>
	/// <param name="squad"></param>
	void Update(Squad* squad);
	/// <summary>
	/// Squad is removed from IdleSquads vector in CombatManager and resets all command positions to invalid
	/// </summary>
	/// <param name="squad"></param>
	void Exit(Squad* squad);
	static SquadState& getInstance();
private:
	IdleState() {};
	IdleState(const IdleState&);
	IdleState& operator=(const IdleState&) {};
};

class KitingBehaviors {
public:
	static void kitingMelee(BWAPI::Unit unit, BWAPI::Position targetPos);
	static void kitingRanged(BWAPI::Unit unit, BWAPI::Position targetPos, BWAPI::Position closestCPPosition=BWAPI::Positions::Invalid);
};
