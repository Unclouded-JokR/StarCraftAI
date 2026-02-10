#include "SquadStateTypes.h"


// TODO: State transitions for the attacking state
void AttackingState::Enter(Squad* squad) {
	
}
void AttackingState::Update(Squad* squad) {

}
void AttackingState::Exit(Squad* squad) {

}

// TODO: Singleton pattern for getInstance
SquadState& AttackingState::getInstance()
{
	static AttackingState singleton;
	return singleton;
}

// TODO: State transitions for the defending state
void DefendingState::Enter(Squad* squad) {

}
void DefendingState::Update(Squad* squad) {

}
void DefendingState::Exit(Squad* squad) {

}

// TODO: Singleton pattern for getInstance
SquadState& DefendingState::getInstance()
{
	static DefendingState singleton;
	return singleton;
}

// TODO: State transitions for the idle state
void IdleState::Enter(Squad* squad) {

}
void IdleState::Update(Squad* squad) {

}
void IdleState::Exit(Squad* squad) {

}

// TODO: Singleton pattern for getInstance
SquadState& IdleState::getInstance()
{
	static IdleState singleton;
	return singleton;
}
