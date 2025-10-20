#include "ProtoBotCommander.h"

void ProtoBotCommander::onStart()
{
	ProtoBotCommander::strategyManager.onStart();
}

void ProtoBotCommander::onFrame()
{
	//Might want to pass unit set to all managers in order to not have to loop through units again and again.
	ProtoBotCommander::strategyManager.onFrame();
}

void ProtoBotCommander::onEnd(bool isWinner)
{
	std::cout << "We " << (isWinner ? "won!" : "lost!") << "\n";
}

void ProtoBotCommander::onUnitDestroy(BWAPI::Unit unit)
{
	ProtoBotCommander::strategyManager.onUnitDestroy(unit);
}

void ProtoBotCommander::onUnitCreate(const BWAPI::Unit unit)
{
	if (unit == nullptr && (BWAPI::Broodwar->self() == unit->getPlayer()))
		return;

	//If the unit is not null and is our own unit, assign them to a module.
	if (unit->getType().isWorker())
	{
		//commander.economyManager.assign(unit);
	}
	else if (unit->getType().isBuilding())
	{
		//commander.buildingManager.assign(unit);
	}
	else
	{
		//commander.combatManager.assign(unit);
	}

}

void ProtoBotCommander::getUnitToScout()
{
	//BWAPI::Unit unit = commander.economyManager.getUnit();

	/*if (unit == nullptr)
		return;*/

	//commander.scoutingManager.assign(unit);
}

BWAPI::Unit ProtoBotCommander::getUnitToBuild()
{
	/*BWAPI::Unit unit = commander.economyManager.getUnit();

	if (unit == nullptr)
		return nullptr;

	return unit*/
	return nullptr;
}

void ProtoBotCommander::onUnitComplete(BWAPI::Unit unit)
{
	ProtoBotCommander::unitManager.addUnit(unit);
}

void ProtoBotCommander::onUnitShow(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitHide(BWAPI::Unit unit)
{

}

void ProtoBotCommander::onUnitRenegade(BWAPI::Unit unit)
{

}