#include "BuildManager.h"
#include "ProtoBotCommander.h"
#include "../src/starterbot/Tools.h"
#include "../src/starterbot/MapTools.h"

// constructor for BuildManager
BuildManager::BuildManager(ProtoBotCommander* commanderReference) : commanderReference(commanderReference)
{
    
}

void BuildManager::onStart()
{

}

void BuildManager::onFrame()
{
    const BWAPI::UnitType workerType = BWAPI::Broodwar->self()->getRace().getWorker();
    int currentSupply = BWAPI::Broodwar->self()->supplyUsed();
    int currentMineral = BWAPI::Broodwar->self()->minerals();
    if (currentMineral > 500) {} 
    BWAPI::Unitset myUnits = BWAPI::Broodwar->self()->getUnits();
    //Continually train zealots with excess minerals
    if (currentMineral > 500) {
        for (auto& unit : myUnits)
        {
	        if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway) {
		    unit->train(BWAPI::UnitTypes::Protoss_Zealot);
	        }
        }
    }
    switch (currentSupply)
    //currentSupply value is doubled of what shown ingame
    //Todo: need workers for vespene
    {
        case 20:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Gateway);
    break;
        case 24:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Assimilator);
    break;
        case 28:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Cybernetics_Core);
    break;
        case 34:
    for (auto& unit : myUnits)
    {
	if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway) {
		unit->train(BWAPI::UnitTypes::Protoss_Dragoon);
	}
    }
    break;
         case 50:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Robotics_Facility);
    break;
        case 58:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Gateway);
    break;
        case 76:
    Tools::BuildBuilding(BWAPI::UnitTypes::Protoss_Observatory);
    break;
    }
}

void BuildManager::assignBuilding(BWAPI::Unit unit)
{

}

bool BuildManager::isBuildOrderCompleted()
{
    //Return true if BO is completed
    return false;
}

void BuildManager::buildBuilding(BWAPI::Unit unit, BWAPI::UnitType building)
{

}
