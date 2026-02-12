#include "SquadStateTypes.h"


// TODO: State transitions for the attacking state
void AttackingState::Enter(Squad* squad) {
	//cout << "Entered Attack State" << endl;
	squad->leader->attack(squad->commandPos);
}
void AttackingState::Update(Squad* squad) {
	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate == squad->leader) continue;

		if (squadMate->isIdle() && !(squadMate->getDistance(squad->leader) < 500))
		{
			squadMate->attack(squad->leader->getPosition());
		}
	}
}
void AttackingState::Exit(Squad* squad) {
	//cout << "Exited Attack State" << endl;
	squad->kitePos = BWAPI::Positions::Invalid;
	squad->currentPath = Path();
	squad->currentPathIdx = 0;
}

// TODO: Singleton pattern for getInstance
SquadState& AttackingState::getInstance()
{
	static AttackingState singleton;
	return singleton;
}

// TODO: State transitions for the defending state
void DefendingState::Enter(Squad* squad) {
	//cout << "Entered Defend State" << endl;
	squad->leader->attack(squad->commandPos);
}
void DefendingState::Update(Squad* squad) {
	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate == squad->leader) continue;

		if (squadMate->isIdle() && !(squadMate->getDistance(squad->leader) < 500))
		{
			squadMate->attack(squad->leader->getPosition());
		}
	}
}
void DefendingState::Exit(Squad* squad) {
	//cout << "Exited Defend State" << endl;
	squad->kitePos = BWAPI::Positions::Invalid;
	squad->currentPath = Path();
	squad->currentPathIdx = 0;
}

// TODO: Singleton pattern for getInstance
SquadState& DefendingState::getInstance()
{
	static DefendingState singleton;
	return singleton;
}

// TODO: State transitions for the idle state
void IdleState::Enter(Squad* squad) {
	//cout << "Entered Idle State" << endl;
}
void IdleState::Update(Squad* squad) {
}
void IdleState::Exit(Squad* squad) {
	//cout << "Exited Idle State" << endl;
	squad->kitePos = BWAPI::Positions::Invalid;
	squad->currentPath = Path();
	squad->currentPathIdx = 0;
}

// TODO: Singleton pattern for getInstance
SquadState& IdleState::getInstance()
{
	static IdleState singleton;
	return singleton;
}
