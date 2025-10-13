#include "CombatManager.h"

#include "../src/starterbot/Tools.h"

void CombatManager::Update() {
	// Send out all units to attack nearest enemy. For testing.
	//AttackClosest();
}

void CombatManager::AttackClosest() {
	BWAPI::Unitset enemyUnits = BWAPI::Broodwar->enemy()->getUnits();
	BWAPI::Unitset myUnits = BWAPI::Broodwar->self()->getUnits();
	for (auto& unit : myUnits)
	{
		if (unit->getType().isWorker() == false && unit->getType().isBuilding() == false && unit->isIdle()) {

			BWAPI::Unit closestEnemy = Tools::GetClosestUnitTo(unit, enemyUnits);
			unit->attack(closestEnemy);
			BWAPI::Broodwar->sendText("Debug:Attacking");
		}
	}
}
