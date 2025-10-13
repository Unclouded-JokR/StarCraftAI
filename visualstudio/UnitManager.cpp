#include "UnitManager.h"
#include "ProtoBotCommander.cpp"


void UnitManager::addUnit(BWAPI::Unit unit)
{
	idleUnits.push_back(unit);
}

void UnitManager::removeUnit(int index)
{
	//idleUnits.erase(index);
}

void UnitManager::assignUnitJobs()
{
	//Auto add units to their repsective module as quick as possible
	//Is each modules job to get units and assign them effectivly

	for (BWAPI::Unit unit : idleUnits)
	{

		if (unit->getType().isWorker())
		{
			//commander.economyManager.assignWorker(unit);
		}
		else if(unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
		{
			//commander.scoutingManager.assignScout(unit);
		}
		else
		{
			//commander.combatManager.assignCombat(unit);
		}
	}

	idleUnits = {};
}

BWAPI::Unit UnitManager::getUnit(BWAPI::UnitType unitType)
{
	for (int i = 0; i < idleUnits.size(); i++)
	{
		BWAPI::Unit unit = idleUnits[i];

		if (unit->getType() == unitType)
		{
			removeUnit(i);
			return unit;
		}
	}

	return nullptr;
}

//BWAPI::Unitset& UnitManager::getUnits(BWAPI::UnitType unitType, int numUnits)
//{
//	BWAPI::Unitset units = BWAPI::Unitset.UnitSet();
//
//	for (int i = 0; i < idleUnits.size(); i++)
//	{
//		BWAPI::Unit unit = idleUnits[i];
//
//		if (unit->getType() == unitType)
//		{
//			units.addUnit
//		}
//	}
//
//	return units;
//}