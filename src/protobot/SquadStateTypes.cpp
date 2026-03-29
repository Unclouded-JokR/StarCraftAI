#include "SquadStateTypes.h"

// Initializing extern variables from "CombatManager.h"
vector<SharedSquad*> CombatManager::SharedSquads;
vector<Squad*> CombatManager::AttackingSquads;
vector<Squad*> CombatManager::DefendingSquads;
vector<Squad*> CombatManager::ReinforcingSquads;
vector<Squad*> CombatManager::IdleSquads;

void AttackingState::Enter(Squad* squad) {
	CombatManager::AttackingSquads.push_back(squad);
#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Entered ATTACKING state" << endl;
#endif
}

void AttackingState::Update(Squad* squad) {

	if (squad->info.currentAttackPosition.isValid() && squad->info.currentAttackPosition == squad->info.commandPos) {
		return;
	}

	if (!squad->info.commandPos.isValid()) {
		return;
	}

	squad->info.currentAttackPosition = squad->info.commandPos;

	for (const auto& squadMate : squad->info.units)
	{
		squadMate->attack(squad->info.currentAttackPosition);
	}
}

void AttackingState::Exit(Squad* squad) {
	CombatManager::AttackingSquads.erase(remove(CombatManager::AttackingSquads.begin(), CombatManager::AttackingSquads.end(), squad), CombatManager::AttackingSquads.end());

#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Exited ATTACKING state" << endl;
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
	cout << "(" << squad->info.squadId << ")" << "Entered DEFENDING state" << endl;
#endif

	// On enter, move towards defending state
	// [Was a bug where spamming move to defend was making units walk past enemies w/o attacking]
	squad->leader->attack(squad->info.currentDefensivePosition);
	for (BWAPI::Unit& squadMate : squad->info.units)
	{
		squadMate->attack(squad->info.currentDefensivePosition);
	}
}

void DefendingState::Update(Squad* squad) {
	// TODO: Integrate logic to interchange between kiting and defending
}

void DefendingState::Exit(Squad* squad) {
	CombatManager::DefendingSquads.erase(remove(CombatManager::DefendingSquads.begin(), CombatManager::DefendingSquads.end(), squad), CombatManager::DefendingSquads.end());

#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Exited DEFENDING state" << endl;
#endif
};

// TODO: Singleton pattern for getInstance
SquadState& DefendingState::getInstance()
{
	static DefendingState singleton;
	return singleton;
}

void ReinforcingState::Enter(Squad* squad) {
	CombatManager::ReinforcingSquads.push_back(squad);

#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Entered REINFORCING state" << endl;
#endif
}

void ReinforcingState::Update(Squad* squad) {
	// Every frame, check if squad is out of range
	if (squad->info.currentDefensivePosition != BWAPI::Positions::Invalid
		&& squad->info.currentDefensivePosition.getApproxDistance(squad->info.commandPos) > MAX_REINFORCE_DIST) {
		squad->setState(DefendingState::getInstance());
		return;
	}

	// If near a a shared squad, join them in combat
	const BWAPI::Position leaderPos = squad->leader->getPosition();
	for (auto& sharedSquad : CombatManager::SharedSquads) {
		if (leaderPos.getApproxDistance(sharedSquad->getPosition()) < COMBINE_DISTANCE) {
			sharedSquad->Squads.push_back(squad);
			sharedSquad->savedSquadInfoMap[squad] = squad->info; // Save current info for when squads will split
			squad->setState(NullState::getInstance());
		}
	}

	// If no enemies are left but still in range, return to defending state
	int searchRadius = 100;
	BWAPI::Unitset enemies = BWAPI::Broodwar->getUnitsInRadius(squad->info.commandPos, searchRadius, BWAPI::Filter::IsEnemy);

#ifdef DEBUG_STATES
	BWAPI::Broodwar->drawCircleMap(squad->info.commandPos, searchRadius, BWAPI::Colors::Yellow);
#endif

	if (enemies.empty()) {
		squad->setState(DefendingState::getInstance());
		return;
	}

	if (squad->info.currentReinforcePosition == BWAPI::Positions::Invalid) {
		squad->info.currentReinforcePosition = squad->info.commandPos;
		for (BWAPI::Unit& squadMate : squad->info.units)
		{
			squadMate->attack(squad->info.currentReinforcePosition);
		}
	}
	else if (squad->info.currentReinforcePosition != squad->info.commandPos) {
		if (squad->info.currentReinforcePosition.getApproxDistance(squad->info.commandPos) > 100) {
			squad->info.currentReinforcePosition = squad->info.commandPos;
			for (BWAPI::Unit& squadMate : squad->info.units)
			{
				squadMate->attack(squad->info.currentReinforcePosition);
			}
		}
	}

	// TODO: Integrate logic to kite while reinforcing
}

void ReinforcingState::Exit(Squad* squad) {
	squad->info.currentReinforcePosition = BWAPI::Positions::Invalid;
	CombatManager::ReinforcingSquads.erase(remove(CombatManager::ReinforcingSquads.begin(), CombatManager::ReinforcingSquads.end(), squad), CombatManager::ReinforcingSquads.end());

#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Exited REINFORCING state" << endl;
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
	cout << "(" << squad->info.squadId << ")" << "Entered IDLE state" << endl;
#endif
}
void IdleState::Update(Squad* squad) {
}
void IdleState::Exit(Squad* squad) {
	CombatManager::IdleSquads.erase(remove(CombatManager::IdleSquads.begin(), CombatManager::IdleSquads.end(), squad), CombatManager::IdleSquads.end());
	squad->info.kitePos = BWAPI::Positions::Invalid;
	squad->info.currentPath = Path();
	squad->info.currentPathIdx = 0;

#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Exited IDLE state" << endl;
#endif
}

SquadState& IdleState::getInstance()
{
	static IdleState singleton;
	return singleton;
}

void NullState::Enter(Squad* squad) {
#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Entered NULL state" << endl;
#endif
}
void NullState::Update(Squad* squad) {
}
void NullState::Exit(Squad* squad) {
#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Exited NULL state" << endl;
#endif
}

SquadState& NullState::getInstance()
{
	static NullState singleton;
	return singleton;
}
