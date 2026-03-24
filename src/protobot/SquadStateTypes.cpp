#include "SquadStateTypes.h"

// Initializing extern variables from "CombatManager.h"
vector<Squad*> CombatManager::AttackingSquads;
vector<Squad*> CombatManager::DefendingSquads;
vector<Squad*> CombatManager::ReinforcingSquads;
vector<Squad*> CombatManager::IdleSquads;

void AttackingState::Enter(Squad* squad) {
	CombatManager::AttackingSquads.push_back(squad);
#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Entered ATTACKING state" << endl;
#endif
}

void AttackingState::Update(Squad* squad) {

	if (squad->currentAttackPosition.isValid() && squad->currentAttackPosition == squad->commandPos) {
		return;
	}

	if (!squad->commandPos.isValid()) {
		return;
	}

	squad->currentAttackPosition = squad->commandPos;

	for (const auto& squadMate : squad->units)
	{
		squadMate->attack(squad->currentAttackPosition);
	}
}

void AttackingState::Exit(Squad* squad) {
	CombatManager::AttackingSquads.erase(remove(CombatManager::AttackingSquads.begin(), CombatManager::AttackingSquads.end(), squad), CombatManager::AttackingSquads.end());

#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Exited ATTACKING state" << endl;
#endif
}

SquadState& AttackingState::getInstance()
{
	static AttackingState singleton;
	return singleton;
}

void DefendingState::Enter(Squad* squad) {
	CombatManager::DefendingSquads.push_back(squad);

#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Entered DEFENDING state" << endl;
#endif

	// On enter, move towards defending state
	// [Was a bug where spamming move to defend was making units walk past enemies w/o attacking]
	squad->leader->attack(squad->currentDefensivePosition);
	for (BWAPI::Unit& squadMate : squad->units)
	{
		squadMate->attack(squad->currentDefensivePosition);
	}
}

void DefendingState::Update(Squad* squad) {
	// TODO: Integrate logic to interchange between kiting and defending
}

void DefendingState::Exit(Squad* squad) {
	CombatManager::DefendingSquads.erase(remove(CombatManager::DefendingSquads.begin(), CombatManager::DefendingSquads.end(), squad), CombatManager::DefendingSquads.end());

#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Exited DEFENDING state" << endl;
#endif
}

// TODO: Singleton pattern for getInstance
SquadState& DefendingState::getInstance()
{
	static DefendingState singleton;
	return singleton;
}

void ReinforcingState::Enter(Squad* squad) {
	CombatManager::ReinforcingSquads.push_back(squad);

#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Entered REINFORCING state" << endl;
#endif
}

void ReinforcingState::Update(Squad* squad) {
	// Every frame, check if squad is out of range
	if (squad->currentDefensivePosition != BWAPI::Positions::Invalid 
		&& squad->currentDefensivePosition.getApproxDistance(squad->commandPos) > MAX_REINFORCE_DIST) {
		squad->setState(DefendingState::getInstance());
		return;
	}

	// If no enemies are left but still in range, return to defending state
	int searchRadius = 100;
	BWAPI::Unitset enemies = BWAPI::Broodwar->getUnitsInRadius(squad->commandPos, searchRadius, BWAPI::Filter::IsEnemy);

#ifdef DEBUG_STATES
	BWAPI::Broodwar->drawCircleMap(squad->commandPos, searchRadius, BWAPI::Colors::Yellow);
#endif

	if (enemies.empty()) {
		squad->setState(DefendingState::getInstance());
		return;
	}

	if (squad->currentReinforcePosition == BWAPI::Positions::Invalid) {
		squad->currentReinforcePosition = squad->commandPos;
		for (BWAPI::Unit& squadMate : squad->units)
		{
			squadMate->attack(squad->currentReinforcePosition);
		}
	}
	else if (squad->currentReinforcePosition != squad->commandPos) {
		if (squad->currentReinforcePosition.getApproxDistance(squad->commandPos) > 100) {
			squad->currentReinforcePosition = squad->commandPos;
			for (BWAPI::Unit& squadMate : squad->units)
			{
				squadMate->attack(squad->currentReinforcePosition);
			}
		}
	}

	// TODO: Integrate logic to kite while reinforcing
}

void ReinforcingState::Exit(Squad* squad) {
	squad->currentReinforcePosition = BWAPI::Positions::Invalid;
	CombatManager::ReinforcingSquads.erase(remove(CombatManager::ReinforcingSquads.begin(), CombatManager::ReinforcingSquads.end(), squad), CombatManager::ReinforcingSquads.end());

#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Exited REINFORCING state" << endl;
#endif
}

SquadState& ReinforcingState::getInstance()
{
	static ReinforcingState singleton;
	return singleton;
}

void IdleState::Enter(Squad* squad) {
	CombatManager::IdleSquads.push_back(squad);

#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Entered IDLE state" << endl;
#endif
}
void IdleState::Update(Squad* squad) {
}
void IdleState::Exit(Squad* squad) {
	CombatManager::IdleSquads.erase(remove(CombatManager::IdleSquads.begin(), CombatManager::IdleSquads.end(), squad), CombatManager::IdleSquads.end());
	squad->kitePos = BWAPI::Positions::Invalid;
	squad->currentPath = Path();
	squad->currentPathIdx = 0;

#ifdef DEBUG_STATES
	cout << "(" << squad->squadId << ")" << "Exited IDLE state" << endl;
#endif
}

SquadState& IdleState::getInstance()
{
	static IdleState singleton;
	return singleton;
}
