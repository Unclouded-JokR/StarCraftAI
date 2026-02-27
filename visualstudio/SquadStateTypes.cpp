#include "SquadStateTypes.h"


void AttackingState::Enter(Squad* squad) {
	//cout << "Entered Attack State" << endl;
	squad->leader->attack(squad->commandPos);
}
void AttackingState::Update(Squad* squad) {
	BWAPI::Unitset enemies = BWAPI::Broodwar->getUnitsInRadius(squad->commandPos, 100, BWAPI::Filter::IsEnemy);
	BWAPI::Broodwar->drawCircleMap(squad->commandPos, 100, BWAPI::Colors::Red);

	// If theres no enemies around the squad's chosen attack position, prioritize defending the chokepoint
	if (enemies.empty()) {
		cout << "Returning to defend chokepoint: " << squad->prevDefendPos.x << "," << squad->prevDefendPos.y << endl;
		squad->commandPos = squad->prevDefendPos;
		squad->setState(DefendingState::getInstance());
	}

	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate == squad->leader) continue;

		if (squadMate->isIdle() && !(squadMate->getDistance(squad->leader) < 200))
		{
			squadMate->attack(squad->commandPos);
		}
	}
}
void AttackingState::Exit(Squad* squad) {
	//cout << "Exited Attack State" << endl;
	squad->kitePos = BWAPI::Positions::Invalid;
	squad->currentPath = Path();
	squad->currentPathIdx = 0;
}

SquadState& AttackingState::getInstance()
{
	static AttackingState singleton;
	return singleton;
}

void DefendingState::Enter(Squad* squad) {
	//cout << "Entered Defend State" << endl;
	squad->leader->attack(squad->commandPos);
}

void DefendingState::Update(Squad* squad) {
	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate == squad->leader) continue;

		if (squadMate->isIdle() && !(squadMate->getDistance(squad->leader) < 200))
		{
			squadMate->attack(squad->commandPos);
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

SquadState& IdleState::getInstance()
{
	static IdleState singleton;
	return singleton;
}
