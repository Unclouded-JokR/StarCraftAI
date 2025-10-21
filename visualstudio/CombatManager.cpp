#include "CombatManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"

CombatManager::CombatManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{

}

void CombatManager::onFrame() {
	Update();
}

void CombatManager::Update() {
	// Send out all units to attack nearest enemy. For testing.
	AttackClosest();
}

void CombatManager::AttackClosest() {
	commanderReference->testPrint("CombatManager");
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

void CombatManager::assignUnit(BWAPI::Unit unit)
{

}
