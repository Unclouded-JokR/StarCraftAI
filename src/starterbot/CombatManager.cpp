#include <CombatManager.h>

void Update(){
	// Send out all units to attack nearest enemy. For testing.
	AttackClosest();
}

void CombatManager::AttackClosest(){
	BWAPI::Unitset enemyUnits = BWAPI::BroodWar->enemy()->getUnits();

	for (auto unit : BWAPI::BroodWar->self()->getUnits())
	{
		if (unit.isAttacking()){ continue; }

		BWAPI::Unit closestEnemy = Tools::getClosestUnitTo(myUnit, enemyUnits);
		myUnit->attack(closestEnemy);
	}
}
