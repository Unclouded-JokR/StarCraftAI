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
    //Make false at the start of a game.
    buildOrderCompleted = false;
}

void BuildManager::onFrame()
{
    const BWAPI::UnitType workerType = BWAPI::Broodwar->self()->getRace().getWorker();
    int currentSupply = BWAPI::Broodwar->self()->supplyUsed() / 2;
    int currentMineral = BWAPI::Broodwar->self()->minerals();

    BWAPI::Unitset myUnits = BWAPI::Broodwar->self()->getUnits();

    //Continually train zealots with excess minerals
    if (currentMineral > 500) {
        for (auto& unit : myUnits)
        {
	        if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway) 
            {
		        unit->train(BWAPI::UnitTypes::Protoss_Zealot);
	        }
        }
    }

    //if the supply does not follow the build order supply numbers we will skip pylons
    switch (currentSupply)
    {
        case 8:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Pylon);
            break;
        case 10:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Gateway);
            break;
        case 12:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Assimilator);
            break;
        case 14:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Cybernetics_Core);
            break;
        case 15:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Pylon);
            break;
        case 17:
            for (auto& unit : myUnits)
            {
	            if (unit->getType() == BWAPI::UnitTypes::Protoss_Gateway) 
                {
		            unit->train(BWAPI::UnitTypes::Protoss_Dragoon);
	            }
            }
            break;
         case 25:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Robotics_Facility);
            break;
        case 29:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Gateway);
            break;
        case 31:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Pylon);
            break;
        case 38:
            Tools::BuildBuilding(commanderReference->getUnitToBuild(), BWAPI::UnitTypes::Protoss_Observatory);
            buildOrderCompleted = true;
            break;
        default:
            break;
    }
}

void BuildManager::assignBuilding(BWAPI::Unit unit)
{

}

bool BuildManager::isBuildOrderCompleted()
{
    return buildOrderCompleted;
}

void BuildManager::buildBuilding(BWAPI::Unit unit, BWAPI::UnitType building)
{
    Tools::BuildBuilding(unit, building);
}
