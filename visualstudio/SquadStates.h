#pragma once
#include "SquadStateAbstract.h"
#include "Squad.h"

class AttackingState : public SquadStateAbstract {
public:
	void Enter(Squad* squad);
	void Update(Squad* squad);
	void Exit(Squad* squad);
	static SquadStateAbstract& getInstance();
private:
	AttackingState() {};
	AttackingState(const AttackingState&);
	AttackingState& operator=(const AttackingState&) {};
};