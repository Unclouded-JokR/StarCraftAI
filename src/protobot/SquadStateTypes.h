#pragma once
#include "SquadState.h"
#include "Squad.h"
#include "CombatManager.h"

#define DEBUG_STATES
#define MAX_REINFORCE_DIST 1500
#define COMBINE_DISTANCE 200

// AttackingState occurs when the squad is specifically told to attack a location (i.e. base, building, unit, etc.)
class AttackingState : public SquadState {
public:
	void Enter(Squad* squad);
	void Update(Squad* squad);
	void Exit(Squad* squad);
	static SquadState& getInstance();
private:
	AttackingState() {};
	AttackingState(const AttackingState&);
	AttackingState& operator=(const AttackingState&) {};
};

// DefendingState occurs when the squad is defending a position (i.e. chokepoint)
class DefendingState : public SquadState {
public:
	void Enter(Squad* squad);
	void Update(Squad* squad);
	void Exit(Squad* squad);
	static SquadState& getInstance();
private:
	DefendingState() {};
	DefendingState(const DefendingState&);
	DefendingState& operator=(const DefendingState&) {};
};

// ReinforcingState occurs when the squad leaves its assigned defensive position (i.e. chokepoint) to help a squad that is being attacked
class ReinforcingState : public SquadState {
public:
	void Enter(Squad* squad);
	void Update(Squad* squad);
	void Exit(Squad* squad);
	static SquadState& getInstance();
private:
	ReinforcingState() {};
	ReinforcingState(const ReinforcingState&);
	ReinforcingState& operator=(const ReinforcingState&) {};
};

// IdleState occurs when a squad has not been told to do anything (i.e. new squad created without instructions)
class IdleState : public SquadState {
public:
	void Enter(Squad* squad);
	void Update(Squad* squad);
	void Exit(Squad* squad);
	static SquadState& getInstance();
private:
	IdleState() {};
	IdleState(const IdleState&);
	IdleState& operator=(const IdleState&) {};
};

// NullState occurs when a squad joins another squad in combat into a SharedSquad. 
// During this, the FSM should not trigger since SharedSquads have their own, separate behavior
class NullState : public SquadState {
public:
	void Enter(Squad* squad);
	void Update(Squad* squad);
	void Exit(Squad* squad);
	static SquadState& getInstance();
private:
	NullState() {};
	NullState(const NullState&);
	NullState& operator=(const NullState&) {};
};