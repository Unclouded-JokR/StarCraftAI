#pragma once
#include "SquadState.h"
#include "Squad.h"

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