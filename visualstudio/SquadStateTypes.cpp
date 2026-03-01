#include "SquadStateTypes.h"

#define DEBUG_STATES

// Initializing extern variables from "CombatManager.h"
vector<Squad*> AttackingSquads;
vector<Squad*> DefendingSquads;
vector<Squad*> ReinforcingSquads;
vector<Squad*> IdleSquads;

void AttackingState::Enter(Squad* squad) {
#ifdef DEBUG_STATES
	AttackingSquads.push_back(squad);
#endif
}
void AttackingState::Update(Squad* squad) {
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
	AttackingSquads.erase(remove(AttackingSquads.begin(), AttackingSquads.end(), squad), AttackingSquads.end());
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
#ifdef DEBUG_STATES
	DefendingSquads.push_back(squad);
#endif
}

void DefendingState::Update(Squad* squad) {
	squad->leader->attack(squad->prevDefendPos);
	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate == squad->leader) continue;

		if (squadMate->isIdle() && !(squadMate->getDistance(squad->leader) < 200))
		{
			squadMate->attack(squad->prevDefendPos);
		}
	}
}

void DefendingState::Exit(Squad* squad) {
	DefendingSquads.erase(remove(DefendingSquads.begin(), DefendingSquads.end(), squad), DefendingSquads.end());
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

void ReinforcingState::Enter(Squad* squad) {
#ifdef DEBUG_STATES
	ReinforcingSquads.push_back(squad);
	squad->leader->attack(squad->commandPos);
#endif
}

void ReinforcingState::Update(Squad* squad) {
	int searchRadius = 100;
	BWAPI::Unitset enemies = BWAPI::Broodwar->getUnitsInRadius(squad->commandPos, searchRadius, BWAPI::Filter::IsEnemy);

	// If theres no enemies around where the squad needs to reinforce, retreat back to the defensive position
	if (enemies.empty() || squad->leader->getPosition().getApproxDistance(squad->commandPos) < MAX_REINFORCE_DIST) {
		cout << "Returning to defend chokepoint: " << squad->prevDefendPos.x << "," << squad->prevDefendPos.y << endl;
		squad->setState(DefendingState::getInstance());
	}

	squad->leader->attack(squad->commandPos);
	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate == squad->leader) continue;

		if (squadMate->isIdle() && !(squadMate->getDistance(squad->leader) < 200))
		{
			squadMate->attack(squad->commandPos);
		}
	}
}

void ReinforcingState::Exit(Squad* squad) {
	ReinforcingSquads.erase(remove(ReinforcingSquads.begin(), ReinforcingSquads.end(), squad), ReinforcingSquads.end());
}

SquadState& ReinforcingState::getInstance()
{
	static ReinforcingState singleton;
	return singleton;
}

void IdleState::Enter(Squad* squad) {
#ifdef DEBUG_STATES
	IdleSquads.push_back(squad);
#endif
}
void IdleState::Update(Squad* squad) {
}
void IdleState::Exit(Squad* squad) {
	IdleSquads.erase(remove(IdleSquads.begin(), IdleSquads.end(), squad), IdleSquads.end());
	squad->kitePos = BWAPI::Positions::Invalid;
	squad->currentPath = Path();
	squad->currentPathIdx = 0;
}

SquadState& IdleState::getInstance()
{
	static IdleState singleton;
	return singleton;
}
