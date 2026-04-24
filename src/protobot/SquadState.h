#pragma once
#include "Squad.h"

class Squad;

/// <summary>
/// Base class for squad states. Each state has its own Enter(), Update(), and Exit() implementation.
/// \n These states and their implementations can be found in SquadStateTypes.h
/// 
/// \n\n States: IdleState, AttackingState, DefendingState, ReinforcingState
/// </summary>
class SquadState {
public:
	virtual void Enter(Squad* squad) = 0;
	virtual void Update(Squad* squad) = 0;
	virtual void Exit(Squad* squad) = 0;
	virtual ~SquadState() {};
};