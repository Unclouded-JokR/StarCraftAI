#include "SquadStateTypes.h"

// Initializing extern variables from "CombatManager.h"
vector<Squad*> CombatManager::AttackingSquads;
vector<Squad*> CombatManager::DefendingSquads;
vector<Squad*> CombatManager::ReinforcingSquads;
vector<Squad*> CombatManager::IdleSquads;

void AttackingState::Enter(Squad* squad) {
#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Entered ATTACKING state" << endl;
#endif
	CombatManager::AttackingSquads.push_back(squad);
}
void AttackingState::Update(Squad* squad) {
	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate->isIdle()) {
			squadMate->attack(squad->commandPos);
		}
	}
}
void AttackingState::Exit(Squad* squad) {
#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Exited ATTACKING state" << endl;
#endif
	CombatManager::AttackingSquads.erase(remove(CombatManager::AttackingSquads.begin(), CombatManager::AttackingSquads.end(), squad), CombatManager::AttackingSquads.end());
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
	cout << "(" << squad->squadId << ")" << "Entered DEFENDING state" << endl;
#endif
	CombatManager::DefendingSquads.push_back(squad);
}

void DefendingState::Update(Squad* squad) {
	squad->leader->attack(squad->currentDefensivePosition);
	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate == squad->leader) continue;

		if (squadMate->getDistance(squad->leader) > 200)
		{
			squadMate->attack(squad->currentDefensivePosition);
		}
	}
}

void DefendingState::Exit(Squad* squad) {
#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Exited DEFENDING state" << endl;
#endif
	CombatManager::DefendingSquads.erase(remove(CombatManager::DefendingSquads.begin(), CombatManager::DefendingSquads.end(), squad), CombatManager::DefendingSquads.end());
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
	cout << "(" << squad->squadId << ")" << "Entered REINFORCING state" << endl;
#endif

	CombatManager::ReinforcingSquads.push_back(squad);
}

void ReinforcingState::Update(Squad* squad) {
	int searchRadius = 200;
	BWAPI::Unitset enemies = BWAPI::Broodwar->getUnitsInRadius(squad->commandPos, searchRadius, BWAPI::Filter::IsEnemy);

	// If theres no enemies around where the squad needs to reinforce, retreat back to the defensive position
	if (squad->leader->getPosition().getApproxDistance(squad->currentDefensivePosition) > MAX_REINFORCE_DIST || enemies.empty()) {
#ifdef DEBUG_STATES
		cout << "Returning to defend chokepoint: " << squad->currentDefensivePosition.x << "," << squad->currentDefensivePosition.y << endl;
#endif
		squad->setState(DefendingState::getInstance());
		return;
	}

	squad->leader->attack(squad->commandPos);
	for (BWAPI::Unit& squadMate : squad->units)
	{
		if (squadMate->isIdle()) {
			squadMate->attack(squad->commandPos);
		}
	}
}

void ReinforcingState::Exit(Squad* squad) {
#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Exited REINFORCING state" << endl;
#endif
	CombatManager::ReinforcingSquads.erase(remove(CombatManager::ReinforcingSquads.begin(), CombatManager::ReinforcingSquads.end(), squad), CombatManager::ReinforcingSquads.end());
}

SquadState& ReinforcingState::getInstance()
{
	static ReinforcingState singleton;
	return singleton;
}

void IdleState::Enter(Squad* squad) {
#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Entered IDLE state" << endl;
#endif
	CombatManager::IdleSquads.push_back(squad);
}
void IdleState::Update(Squad* squad) {
}
void IdleState::Exit(Squad* squad) {
#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Exited IDLE state" << endl;
#endif
	CombatManager::IdleSquads.erase(remove(CombatManager::IdleSquads.begin(), CombatManager::IdleSquads.end(), squad), CombatManager::IdleSquads.end());
	squad->kitePos = BWAPI::Positions::Invalid;
	squad->currentPath = Path();
	squad->currentPathIdx = 0;
}

SquadState& IdleState::getInstance()
{
	static IdleState singleton;
	return singleton;
}
