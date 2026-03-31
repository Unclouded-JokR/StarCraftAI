#include "SquadStateTypes.h"

void AttackingState::Enter(Squad* squad) {
	CombatManager::AttackingSquads.push_back(squad);
#ifdef DEBUG_STATES
	cout << "(" << squad->info.squadId << ")" << "Entered ATTACKING state" << endl;
#endif
}

void AttackingState::Update(Squad* squad) {

	if (!squad->info.commandPos.isValid()) {
		return;
	}

	if (squad->info.currentAttackPosition != squad->info.commandPos) {
		squad->info.currentAttackPosition = squad->info.commandPos;
	}

	if (BWAPI::Broodwar->getFrameCount() % KITING_FRAME_DELAY != 0) {
		return;
	}

	// Find closest chokepoint for use in ranged kiting
	BWAPI::Position closestCPPos = BWAPI::Positions::Invalid;
	int closestDist = 0;
	const BWAPI::Position leaderPos = squad->leader->getPosition();
	for (const BWEM::Area& area : bwemMap.Areas()) {
		for (const BWEM::ChokePoint* cp : area.ChokePoints()) {
			const BWAPI::Position cpCenter = BWAPI::Position(cp->Center());
			const int dist = cpCenter.getApproxDistance(leaderPos);
			if (dist < closestDist) {
				closestDist = dist;
				closestCPPos = cpCenter;
			}
		}
	}

	for (BWAPI::Unit& unit : squad->info.units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Zealot) {
			KitingBehaviors::kitingMelee(unit, squad->info.currentAttackPosition);
		}
		else if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon) {
			KitingBehaviors::kitingRanged(unit, squad->info.currentAttackPosition, closestCPPos);
		}
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
	// State for keeping squads at chokepoint. Doesn't do anything per frame.
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
	
	if (squad->info.currentReinforcePosition != squad->info.commandPos) {
		if (squad->info.currentReinforcePosition.getApproxDistance(squad->info.commandPos) > 50) {
			squad->info.currentReinforcePosition = squad->info.commandPos;
		}
	}

	for (BWAPI::Unit& unit : squad->info.units)
	{
		if (unit->getType() == BWAPI::UnitTypes::Protoss_Zealot) {
			KitingBehaviors::kitingMelee(unit, squad->info.currentReinforcePosition);
		}
		else if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon) {
			KitingBehaviors::kitingRanged(unit, squad->info.currentReinforcePosition);
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

// Despite the name, this method is not about attack-moving
// Acts as the move command when attacking/kiting
void KitingBehaviors::kitingMelee(BWAPI::Unit unit, BWAPI::Position targetPos) {
	if (!unit) {
		return;
	}

	if (BWAPI::Broodwar->getFrameCount() % KITING_FRAME_DELAY != 0) {
		return;
	}

	// If unit already had a command assigned to it this frame, ignore
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame()) {
		return;
	}

	if (unit->getGroundWeaponCooldown() == 0) {
		unit->attack(targetPos);
	}
	else {
		unit->move(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));
	}
}

void KitingBehaviors::kitingRanged(BWAPI::Unit unit, BWAPI::Position targetPos, BWAPI::Position closestCPPos) {
	// If unit or target is invalid, return
	if (!unit) {
		return;
	}

	if (BWAPI::Broodwar->getFrameCount() % KITING_FRAME_DELAY != 0) {
		return;
	}

	// If unit already had a command assigned to it this frame, ignore
	if (unit->getLastCommandFrame() >= BWAPI::Broodwar->getFrameCount() || unit->isAttackFrame()) {
		return;
	}

	// If unit is already attacking, ignore
	if (unit->isStartingAttack() || unit->isAttackFrame()) {
		return;
	}

	if (unit->getGroundWeaponCooldown() == 0) {
		unit->attack(targetPos);
	}
	else {
		// Special case for attack phase
		// If unit near chokepoint, move towards current target between attacks
		// Removes chokepoint clumping during attack phase
		if (StrategyManager::isAttackPhase && closestCPPos.isValid()) {
			BWAPI::Broodwar->drawCircleMap(closestCPPos, unit->getType().groundWeapon().maxRange(), BWAPI::Colors::Yellow);
			if (closestCPPos.getApproxDistance(unit->getPosition()) < unit->getType().groundWeapon().maxRange()) {
				if (unit->isAttacking()) {
					unit->move(unit->getTargetPosition());
				}
				else {
					unit->attack(targetPos);
				}
			}
		}
		else if (unit->isUnderAttack() || !BWAPI::Broodwar->getUnitsInRadius(unit->getPosition(), unit->getType().groundWeapon().maxRange(), BWAPI::Filter::IsEnemy).empty()){
			unit->move(BWAPI::Position(BWAPI::Broodwar->self()->getStartLocation()));
		}
		else {
			unit->holdPosition();
		}
	}
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
